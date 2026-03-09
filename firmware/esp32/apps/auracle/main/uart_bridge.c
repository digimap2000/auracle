#include "uart_bridge.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "uart_bridge";

#define UART_NUM            CONFIG_AURACLE_UART_PORT_NUM
#define TX_PIN              CONFIG_AURACLE_UART_TX_PIN
#define RX_PIN              CONFIG_AURACLE_UART_RX_PIN
#define BAUD_RATE           CONFIG_AURACLE_UART_BAUD_RATE
#define RX_BUF_SIZE         CONFIG_AURACLE_UART_RX_BUF_SIZE

#define UART_EV_QUEUE_DEPTH 16
#define RX_QUEUE_DEPTH      16
#define RX_TASK_STACK       3072
#define RX_TASK_PRIORITY    10

typedef struct {
    size_t  len;
    uint8_t data[UART_BRIDGE_RX_CHUNK_SIZE];
} uart_rx_chunk_t;

static QueueHandle_t s_uart_ev_queue;
static QueueHandle_t s_rx_queue;

static void uart_rx_task(void *arg)
{
    uart_event_t ev;
    uint8_t chunk_buf[UART_BRIDGE_RX_CHUNK_SIZE];

    (void)arg;

    ESP_LOGI(TAG, "RX task started (UART%d  TX=%d  RX=%d  %d baud)",
             UART_NUM, TX_PIN, RX_PIN, BAUD_RATE);

    while (xQueueReceive(s_uart_ev_queue, &ev, portMAX_DELAY)) {
        switch (ev.type) {
        case UART_DATA: {
            int remaining = (int)ev.size;

            while (remaining > 0) {
                int to_read = remaining < (int)sizeof(chunk_buf)
                            ? remaining : (int)sizeof(chunk_buf);
                int n = uart_read_bytes(UART_NUM, chunk_buf, (uint32_t)to_read,
                                        pdMS_TO_TICKS(10));
                if (n <= 0) {
                    break;
                }

                uart_rx_chunk_t item = {
                    .len = (size_t)n,
                };
                memcpy(item.data, chunk_buf, (size_t)n);

                if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "RX queue full, dropping %d bytes", n);
                }

                remaining -= n;
            }
            break;
        }

        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "RX: UART %s — flushing buffers",
                     ev.type == UART_FIFO_OVF ? "FIFO overflow" : "ring buffer full");
            uart_flush_input(UART_NUM);
            xQueueReset(s_uart_ev_queue);
            xQueueReset(s_rx_queue);
            break;

        case UART_FRAME_ERR:
            ESP_LOGW(TAG, "RX: frame error (baud rate mismatch?)");
            break;

        case UART_PARITY_ERR:
            ESP_LOGW(TAG, "RX: parity error");
            break;

        default:
            break;
        }
    }
}

esp_err_t uart_bridge_init(void)
{
    const uart_config_t uart_cfg = {
        .baud_rate  = BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err;

    err = uart_driver_install(UART_NUM, RX_BUF_SIZE, 0,
                              UART_EV_QUEUE_DEPTH, &s_uart_ev_queue, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(UART_NUM, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(UART_NUM, TX_PIN, RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin: %s", esp_err_to_name(err));
        return err;
    }

    s_rx_queue = xQueueCreate(RX_QUEUE_DEPTH, sizeof(uart_rx_chunk_t));
    if (!s_rx_queue) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(uart_rx_task, "uart_bridge_rx", RX_TASK_STACK,
                    NULL, RX_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Initialised: UART%d  TX=GPIO%d  RX=GPIO%d  %d baud",
             UART_NUM, TX_PIN, RX_PIN, BAUD_RATE);
    return ESP_OK;
}

esp_err_t uart_bridge_write(const uint8_t *data, size_t len)
{
    if (!data && len > 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }

    int written = uart_write_bytes(UART_NUM, data, len);
    if (written < 0 || (size_t)written != len) {
        ESP_LOGE(TAG, "TX: uart_write_bytes failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

int uart_bridge_read(uint8_t *buf, size_t buf_size, TickType_t wait_ticks)
{
    uart_rx_chunk_t item;

    if (!buf || buf_size < UART_BRIDGE_RX_CHUNK_SIZE) {
        return -1;
    }

    if (xQueueReceive(s_rx_queue, &item, wait_ticks) != pdTRUE) {
        return 0;
    }

    memcpy(buf, item.data, item.len);
    return (int)item.len;
}

void uart_bridge_reset_rx(void)
{
    if (s_rx_queue) {
        xQueueReset(s_rx_queue);
    }
}

