/*
 * Auracle ESP32 Connection — JSON Lines v1 over physical UART
 *
 * Device (server) side of the JSON Lines v1 protocol.
 * The ESP32 bridge sends requests; we respond.
 *
 * Protocol rules:
 *   Request:  {"id":N,"cmd":"<name>"}
 *   Response: {"id":N,"ok":true,...} or {"id":N,"ok":false,"error":"<msg>"}
 *   Event:    {"event":"<name>"[,"<key>":"<val>",...]}
 *
 * All messages are terminated with \n. Incoming command lines are accepted up
 * to 1024 bytes; outgoing event lines use the same transport limit.
 *
 * Implementation:
 *   - UART2 uses Zephyr's async UART API, which maps cleanly to nRF UARTE.
 *   - RX runs continuously with double-buffered DMA and assembles complete
 *     lines into a message queue for a dedicated protocol thread.
 *   - TX uses uart_tx() with completion wait, serialized by a mutex so
 *     responses and events cannot interleave on the wire.
 */

#include "esp_conn.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(esp_conn, LOG_LEVEL_INF);

static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart2));

#define RX_DMA_BUF_SIZE        64
#define RX_LINE_BUF_SIZE       1024
#define RX_LINE_QUEUE_DEPTH    4
#define RX_TIMEOUT_US          1000
#define TX_DONE_TIMEOUT_MS     250
#define ESP_CONN_RX_STACK_SIZE 2048
#define ESP_CONN_RX_PRIORITY   7
#define ESP_CONN_TX_LINE_MAX   1024
#define ADV_DATA_HEX_MAX       512

static K_MUTEX_DEFINE(tx_mutex);
static K_SEM_DEFINE(tx_done_sem, 0, 1);
K_MSGQ_DEFINE(rx_line_msgq, RX_LINE_BUF_SIZE, RX_LINE_QUEUE_DEPTH, 4);

static K_THREAD_STACK_DEFINE(esp_conn_rx_stack, ESP_CONN_RX_STACK_SIZE);
static struct k_thread esp_conn_rx_thread_data;

static uint8_t rx_dma_buf_a[RX_DMA_BUF_SIZE];
static uint8_t rx_dma_buf_b[RX_DMA_BUF_SIZE];
static bool    rx_buf_a_in_use;
static bool    rx_buf_b_in_use;
static char    rx_line_buf[RX_LINE_BUF_SIZE];
static size_t  rx_line_pos;
static bool    rx_discard;

static volatile int tx_result;

static void rx_reset_line_state(void)
{
	rx_line_pos = 0;
	rx_discard = false;
}

static void tx_drain_done_sem(void)
{
	while (k_sem_take(&tx_done_sem, K_NO_WAIT) == 0) {
	}
}

static int esp_write(const char *buf, size_t len)
{
	int err;

	if (!buf || len == 0) {
		return 0;
	}

	k_mutex_lock(&tx_mutex, K_FOREVER);
	tx_drain_done_sem();
	tx_result = 0;

	err = uart_tx(uart_dev, (const uint8_t *)buf, len, SYS_FOREVER_MS);
	if (err) {
		LOG_ERR("uart_tx failed (%d)", err);
		k_mutex_unlock(&tx_mutex);
		return err;
	}

	err = k_sem_take(&tx_done_sem, K_MSEC(TX_DONE_TIMEOUT_MS));
	if (err) {
		LOG_ERR("uart_tx completion timeout");
		(void)uart_tx_abort(uart_dev);
		k_mutex_unlock(&tx_mutex);
		return -ETIMEDOUT;
	}

	k_mutex_unlock(&tx_mutex);
	return tx_result;
}

static void rx_enqueue_line(void)
{
	rx_line_buf[rx_line_pos] = '\0';
	if (k_msgq_put(&rx_line_msgq, rx_line_buf, K_NO_WAIT) != 0) {
		LOG_WRN("RX queue full, dropping line: %.60s", rx_line_buf);
	}
	rx_line_pos = 0;
}

static void rx_process_byte(uint8_t c)
{
	if (c == '\r') {
		return;
	}

	if (rx_discard) {
		if (c == '\n') {
			rx_reset_line_state();
			LOG_WRN("RX: recovered from line overflow");
		}
		return;
	}

	if (c == '\n') {
		if (rx_line_pos > 0) {
			rx_enqueue_line();
		}
		return;
	}

	if (rx_line_pos >= RX_LINE_BUF_SIZE - 1) {
		rx_discard = true;
		rx_line_pos = 0;
		LOG_WRN("RX line too long, discarding");
		return;
	}

	rx_line_buf[rx_line_pos++] = (char)c;
}

static void rx_process_chunk(const uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		rx_process_byte(buf[i]);
	}
}

static uint8_t *rx_acquire_spare_buffer(void)
{
	if (!rx_buf_a_in_use) {
		rx_buf_a_in_use = true;
		return rx_dma_buf_a;
	}

	if (!rx_buf_b_in_use) {
		rx_buf_b_in_use = true;
		return rx_dma_buf_b;
	}

	return NULL;
}

static void rx_release_buffer(const uint8_t *buf)
{
	if (buf == rx_dma_buf_a) {
		rx_buf_a_in_use = false;
	} else if (buf == rx_dma_buf_b) {
		rx_buf_b_in_use = false;
	}
}

static void uart_event_handler(const struct device *dev,
			       struct uart_event *evt,
			       void *user_data)
{
	int err;

	ARG_UNUSED(user_data);

	switch (evt->type) {
	case UART_TX_DONE:
		tx_result = 0;
		k_sem_give(&tx_done_sem);
		break;

	case UART_TX_ABORTED:
		tx_result = -EIO;
		LOG_ERR("UART TX aborted");
		k_sem_give(&tx_done_sem);
		break;

	case UART_RX_RDY:
		rx_process_chunk(&evt->data.rx.buf[evt->data.rx.offset],
				 evt->data.rx.len);
		break;

	case UART_RX_BUF_REQUEST: {
		uint8_t *spare = rx_acquire_spare_buffer();

		if (!spare) {
			LOG_WRN("No spare RX buffer available");
			break;
		}

		err = uart_rx_buf_rsp(dev, spare, RX_DMA_BUF_SIZE);
		if (err) {
			rx_release_buffer(spare);
			LOG_ERR("uart_rx_buf_rsp failed (%d)", err);
		}
		break;
	}

	case UART_RX_BUF_RELEASED:
		rx_release_buffer(evt->data.rx_buf.buf);
		break;

	case UART_RX_DISABLED:
		rx_buf_a_in_use = true;
		rx_buf_b_in_use = false;
		err = uart_rx_enable(dev, rx_dma_buf_a, RX_DMA_BUF_SIZE, RX_TIMEOUT_US);
		if (err) {
			LOG_ERR("uart_rx_enable restart failed (%d)", err);
		}
		break;

	case UART_RX_STOPPED:
		LOG_ERR("UART RX stopped (reason=%d)", evt->data.rx_stop.reason);
		err = uart_rx_disable(dev);
		if (err && err != -EFAULT) {
			LOG_ERR("uart_rx_disable failed (%d)", err);
		}
		break;

	default:
		break;
	}
}

/* ── Command helpers ──────────────────────────────────────────── */

static int extract_id(const char *line)
{
	const char *p = strstr(line, "\"id\":");

	if (!p) {
		return -1;
	}
	p += 5;
	return (int)strtol(p, NULL, 10);
}

static bool cmd_is(const char *line, const char *cmd)
{
	char needle[48];

	snprintf(needle, sizeof(needle), "\"cmd\":\"%s\"", cmd);
	return strstr(line, needle) != NULL;
}

/* ── Request handlers ─────────────────────────────────────────── */

static void handle_get_info(int id)
{
	char resp[256];
	int  len = snprintf(resp, sizeof(resp),
		"{\"id\":%d,\"ok\":true,"
		"\"name\":\"nrf5430\","
		"\"version\":\"1.0.0\"}\n",
		id);

	if (len > 0 && (size_t)len < sizeof(resp) && esp_write(resp, len) == 0) {
		LOG_INF("TX: get_info response (id=%d)", id);
	}
}

static void handle_get_capabilities(int id)
{
	char resp[256];
	int  len = snprintf(resp, sizeof(resp),
		"{\"id\":%d,\"ok\":true,"
		"\"capabilities\":[\"auracast_sink\"]}\n",
		id);

	if (len > 0 && (size_t)len < sizeof(resp) && esp_write(resp, len) == 0) {
		LOG_INF("TX: get_capabilities response (id=%d)", id);
	}
}

static void handle_unknown_cmd(int id, const char *line)
{
	char resp[96];
	int  len = snprintf(resp, sizeof(resp),
		"{\"id\":%d,\"ok\":false,\"error\":\"unknown_cmd\"}\n", id);

	if (len > 0 && (size_t)len < sizeof(resp) && esp_write(resp, len) == 0) {
		LOG_WRN("TX: unknown_cmd (id=%d  line=%.60s)", id, line);
	}
}

static void handle_line(const char *line)
{
	int id;

	LOG_DBG("RX: %s", line);

	id = extract_id(line);
	if (id < 0) {
		LOG_WRN("RX: no id field — ignoring: %.60s", line);
		return;
	}

	if (cmd_is(line, "get_info")) {
		handle_get_info(id);
	} else if (cmd_is(line, "get_capabilities")) {
		handle_get_capabilities(id);
	} else {
		handle_unknown_cmd(id, line);
	}
}

/* ── RX thread ────────────────────────────────────────────────── */

static void esp_conn_rx_thread_fn(void *p1, void *p2, void *p3)
{
	char line[RX_LINE_BUF_SIZE];

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_msgq_get(&rx_line_msgq, line, K_FOREVER);
		handle_line(line);
	}
}

/* ── Public API ───────────────────────────────────────────────── */

void esp_conn_send_event(const char *event_name, const char *extra_json)
{
	char buf[ESP_CONN_TX_LINE_MAX];
	int  len;

	if (extra_json && extra_json[0] != '\0') {
		len = snprintf(buf, sizeof(buf),
			"{\"event\":\"%s\",%s}\n", event_name, extra_json);
	} else {
		len = snprintf(buf, sizeof(buf),
			"{\"event\":\"%s\"}\n", event_name);
	}

	if (len > 0 && (size_t)len < sizeof(buf)) {
		(void)esp_write(buf, len);
	}
}

static const char *addr_type_str(const bt_addr_le_t *addr)
{
	if (!addr) {
		return "unknown";
	}

	switch (addr->type) {
	case BT_ADDR_LE_PUBLIC:
		return "public";
	case BT_ADDR_LE_RANDOM:
		return "random";
	case BT_ADDR_LE_PUBLIC_ID:
		return "public_id";
	case BT_ADDR_LE_RANDOM_ID:
		return "random_id";
	default:
		return "unknown";
	}
}

void esp_conn_send_adv_report(const struct bt_le_scan_recv_info *info,
			      const uint8_t *data, size_t data_len)
{
	char addr_str[BT_ADDR_STR_LEN];
	char payload_hex[ADV_DATA_HEX_MAX];
	char line[ESP_CONN_TX_LINE_MAX];
	int len;

	if (!info || !info->addr || (!data && data_len > 0)) {
		return;
	}

	if (bin2hex(data, data_len, payload_hex, sizeof(payload_hex)) == 0U) {
		LOG_WRN("Dropping adv report: hex encode failed (len=%u)",
			(unsigned int)data_len);
		return;
	}

	bt_addr_to_str(&info->addr->a, addr_str, sizeof(addr_str));

	len = snprintf(line, sizeof(line),
		       "{\"event\":\"adv\",\"ts\":%u,\"addr\":\"%s\","
		       "\"addr_type\":\"%s\",\"sid\":%u,\"rssi\":%d,"
		       "\"tx_power\":%d,\"adv_type\":%u,\"adv_props\":%u,"
		       "\"interval\":%u,\"primary_phy\":%u,\"secondary_phy\":%u,"
		       "\"data_len\":%u,\"data_hex\":\"%s\"}\n",
		       k_uptime_get_32(), addr_str, addr_type_str(info->addr),
		       info->sid, info->rssi, info->tx_power, info->adv_type,
		       info->adv_props, info->interval, info->primary_phy,
		       info->secondary_phy, (unsigned int)data_len, payload_hex);
	if (len <= 0 || (size_t)len >= sizeof(line)) {
		LOG_WRN("Dropping adv report: event too large (data_len=%u)",
			(unsigned int)data_len);
		return;
	}

	(void)esp_write(line, len);
}

int esp_conn_init(void)
{
	int err;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART2 device not ready");
		return -ENODEV;
	}

	rx_reset_line_state();
	rx_buf_a_in_use = true;
	rx_buf_b_in_use = false;

	err = uart_callback_set(uart_dev, uart_event_handler, NULL);
	if (err) {
		LOG_ERR("uart_callback_set failed (%d)", err);
		return err;
	}

	err = uart_rx_enable(uart_dev, rx_dma_buf_a, RX_DMA_BUF_SIZE, RX_TIMEOUT_US);
	if (err) {
		LOG_ERR("uart_rx_enable failed (%d)", err);
		return err;
	}

	k_thread_create(&esp_conn_rx_thread_data, esp_conn_rx_stack,
			ESP_CONN_RX_STACK_SIZE,
			esp_conn_rx_thread_fn, NULL, NULL, NULL,
			K_PRIO_PREEMPT(ESP_CONN_RX_PRIORITY), 0, K_NO_WAIT);
	k_thread_name_set(&esp_conn_rx_thread_data, "esp_conn_rx");

	LOG_INF("ESP32 connection initialised (UART2 async)");
	return 0;
}

void esp_conn_announce_ready(void)
{
	esp_conn_send_event("ready", NULL);

	LOG_INF("Ready event sent to ESP32");
}
