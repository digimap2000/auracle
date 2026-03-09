#include "uart_proto.h"
#include "device_proto.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "uart_proto";

/* ── Constants ────────────────────────────────────────────────── */

#define UART_NUM            CONFIG_AURACLE_UART_PORT_NUM
#define TX_PIN              CONFIG_AURACLE_UART_TX_PIN
#define RX_PIN              CONFIG_AURACLE_UART_RX_PIN
#define BAUD_RATE           CONFIG_AURACLE_UART_BAUD_RATE
#define RX_BUF_SIZE         CONFIG_AURACLE_UART_RX_BUF_SIZE

/* Maximum line length including the terminating '\n', per spec. */
#define LINE_MAX_LEN        256

/*
 * Depth of the FreeRTOS queue between the UART RX task and
 * uart_proto_poll_rx(). At 256 bytes per item this is 2 KB.
 */
#define LINE_QUEUE_DEPTH    8

#define UART_EV_QUEUE_DEPTH 16
#define RX_CHUNK_SIZE       64
#define RX_TASK_STACK       3072
#define RX_TASK_PRIORITY    10  /* High priority — prevents UART FIFO overrun */

/* ── State ────────────────────────────────────────────────────── */

static QueueHandle_t s_uart_ev_queue;
static QueueHandle_t s_line_queue;

typedef enum { RX_NORMAL, RX_DISCARD } rx_state_t;

/* ── Byte accumulator ─────────────────────────────────────────── */

/*
 * Process one byte from the UART into the line buffer.
 *
 * Normal mode: accumulate bytes until '\n'. Strip a preceding '\r'.
 * Enqueue complete lines. Ignore blank lines.
 *
 * Discard mode: entered when an incoming line would exceed LINE_MAX_LEN.
 * Bytes are thrown away until the next '\n', then normal mode resumes.
 *
 * Line length accounting (LINE_MAX_LEN = 256):
 *   - Content bytes occupy buf[0..254], null terminator at buf[255].
 *   - When line_pos reaches LINE_MAX_LEN-1 (255), the next content byte
 *     would overflow the null terminator slot → enter discard mode.
 *   - A '\n' at line_pos == 255 is processed normally (strip optional '\r'
 *     at pos 254, null-terminate at pos 255), so 255-byte lines are valid.
 */
static void process_byte(uint8_t ch, char *buf, int *pos, rx_state_t *state)
{
    if (*state == RX_DISCARD) {
        if (ch == '\n') {
            *state = RX_NORMAL;
            *pos   = 0;
            ESP_LOGW(TAG, "RX: recovered from line overflow, resuming");
        }
        /* Discard regardless — fall through to return */
        return;
    }

    /* Normal mode */

    if (ch == '\n') {
        /* Strip trailing CR (CRLF line endings from some hosts) */
        if (*pos > 0 && buf[*pos - 1] == '\r') {
            (*pos)--;
        }
        if (*pos == 0) {
            return; /* Blank line — ignore */
        }
        buf[*pos] = '\0';
        if (xQueueSend(s_line_queue, buf, 0) != pdTRUE) {
            ESP_LOGW(TAG, "RX: line queue full, dropping: %.60s", buf);
        }
        *pos = 0;
        return;
    }

    if (*pos >= LINE_MAX_LEN - 1) {
        /* Next byte would overwrite the null-terminator slot — discard line */
        *state = RX_DISCARD;
        *pos   = 0;
        ESP_LOGW(TAG, "RX: line exceeds %d bytes, discarding until next \\n", LINE_MAX_LEN);
        return;
    }

    buf[(*pos)++] = (char)ch;
}

/* ── UART RX task ─────────────────────────────────────────────── */

static void uart_rx_task(void *arg)
{
    uart_event_t ev;
    char         line_buf[LINE_MAX_LEN];
    int          line_pos = 0;
    rx_state_t   rx_state = RX_NORMAL;
    uint8_t      chunk[RX_CHUNK_SIZE];

    ESP_LOGI(TAG, "RX task started (UART%d  TX=%d  RX=%d  %d baud)",
             UART_NUM, TX_PIN, RX_PIN, BAUD_RATE);

    while (xQueueReceive(s_uart_ev_queue, &ev, portMAX_DELAY)) {
        switch (ev.type) {

        case UART_DATA: {
            /*
             * Read available bytes in chunks. ev.size is advisory; the driver
             * may have accumulated more by the time we read. Keep reading
             * until uart_read_bytes returns 0.
             */
            int remaining = (int)ev.size;
            while (remaining > 0) {
                int to_read = remaining < RX_CHUNK_SIZE ? remaining : RX_CHUNK_SIZE;
                int n = uart_read_bytes(UART_NUM, chunk, (uint32_t)to_read,
                                        pdMS_TO_TICKS(10));
                if (n <= 0) {
                    break;
                }
                remaining -= n;
                for (int i = 0; i < n; i++) {
                    process_byte(chunk[i], line_buf, &line_pos, &rx_state);
                }
            }
            break;
        }

        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            /*
             * The UART hardware or driver ring buffer overflowed. Flush and
             * reset the line accumulator. Lines in progress are lost.
             */
            ESP_LOGW(TAG, "RX: UART %s — flushing buffers",
                     ev.type == UART_FIFO_OVF ? "FIFO overflow" : "ring buffer full");
            uart_flush_input(UART_NUM);
            xQueueReset(s_uart_ev_queue);
            line_pos = 0;
            rx_state = RX_NORMAL;
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

/* ── Public API ───────────────────────────────────────────────── */

esp_err_t uart_proto_init(void)
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

    s_line_queue = xQueueCreate(LINE_QUEUE_DEPTH, LINE_MAX_LEN);
    if (!s_line_queue) {
        ESP_LOGE(TAG, "Failed to create line queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(uart_rx_task, "uart_proto_rx", RX_TASK_STACK,
                    NULL, RX_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Initialised: UART%d  TX=GPIO%d  RX=GPIO%d  %d baud",
             UART_NUM, TX_PIN, RX_PIN, BAUD_RATE);
    return ESP_OK;
}

esp_err_t uart_proto_send_json_line(const char *json)
{
    if (!json) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(json);
    ESP_LOGD(TAG, "TX: %s", json);

    int written = uart_write_bytes(UART_NUM, json, len);
    if (written < 0) {
        ESP_LOGE(TAG, "TX: uart_write_bytes failed");
        return ESP_FAIL;
    }

    written = uart_write_bytes(UART_NUM, "\n", 1);
    if (written < 0) {
        ESP_LOGE(TAG, "TX: failed to write newline");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void uart_proto_poll_rx(void)
{
    char line[LINE_MAX_LEN];
    while (xQueueReceive(s_line_queue, line, 0) == pdTRUE) {
        ESP_LOGD(TAG, "RX: %s", line);
        protocol_handle_line(line);
    }
}
