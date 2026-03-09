#include "wire_uart.h"
#include "wire_proto.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "wire_uart";

#define UART_NUM        CONFIG_AURACLE_UART_PORT_NUM
#define TX_PIN          CONFIG_AURACLE_UART_TX_PIN
#define RX_PIN          CONFIG_AURACLE_UART_RX_PIN
#define BAUD_RATE       CONFIG_AURACLE_UART_BAUD_RATE
#define RX_BUF_SIZE     CONFIG_AURACLE_UART_RX_BUF_SIZE

#define LINE_BUF_SIZE   1024
#define UART_EVENT_QUEUE_SIZE 20
#define RX_TASK_STACK   4096
#define RX_TASK_PRIORITY 10

static QueueHandle_t s_uart_event_queue;

static void uart_rx_task(void *arg)
{
    uart_event_t event;
    char line_buf[LINE_BUF_SIZE];
    size_t line_pos = 0;
    uint8_t byte_buf[128];

    ESP_LOGI(TAG, "UART RX task started on UART%d (TX=%d, RX=%d, %d baud)",
             UART_NUM, TX_PIN, RX_PIN, BAUD_RATE);

    while (true) {
        if (!xQueueReceive(s_uart_event_queue, &event, portMAX_DELAY)) {
            continue;
        }

        switch (event.type) {
        case UART_DATA: {
            /* Read available bytes in chunks */
            int remaining = event.size;
            while (remaining > 0) {
                int to_read = remaining;
                if (to_read > (int)sizeof(byte_buf)) {
                    to_read = (int)sizeof(byte_buf);
                }

                int len = uart_read_bytes(UART_NUM, byte_buf, to_read, pdMS_TO_TICKS(10));
                if (len <= 0) {
                    break;
                }
                remaining -= len;

                for (int i = 0; i < len; i++) {
                    char ch = (char)byte_buf[i];

                    if (ch == '\n' || ch == '\r') {
                        if (line_pos > 0) {
                            line_buf[line_pos] = '\0';
                            wire_proto_feed_line(line_buf);
                            line_pos = 0;
                        }
                        continue;
                    }

                    if (line_pos < LINE_BUF_SIZE - 1) {
                        line_buf[line_pos++] = ch;
                    }
                    /* else: line too long, discard excess bytes until newline */
                }
            }
            break;
        }

        case UART_FIFO_OVF:
            ESP_LOGW(TAG, "UART FIFO overflow — flushing");
            uart_flush_input(UART_NUM);
            xQueueReset(s_uart_event_queue);
            line_pos = 0;
            break;

        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "UART ring buffer full — flushing");
            uart_flush_input(UART_NUM);
            xQueueReset(s_uart_event_queue);
            line_pos = 0;
            break;

        case UART_PARITY_ERR:
            ESP_LOGW(TAG, "UART parity error");
            break;

        case UART_FRAME_ERR:
            ESP_LOGW(TAG, "UART frame error");
            break;

        default:
            break;
        }
    }
}

esp_err_t wire_uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate  = BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err;

    err = uart_driver_install(UART_NUM, RX_BUF_SIZE, 0, UART_EVENT_QUEUE_SIZE,
                              &s_uart_event_queue, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(UART_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t created = xTaskCreate(uart_rx_task, "uart_rx", RX_TASK_STACK,
                                     NULL, RX_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART RX task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UART%d initialized", UART_NUM);
    return ESP_OK;
}

esp_err_t wire_uart_send(const char *data, size_t len)
{
    int written = uart_write_bytes(UART_NUM, data, len);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
