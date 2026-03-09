/*
 * Auracle Wire Protocol — NDJSON over USB CDC ACM
 *
 * Upstream (device → desktop): one JSON object per \n, tagged by type.
 * Downstream (desktop → device): JSON commands, one per line.
 * All taps off by default — device is silent until told to report.
 *
 * This module owns the USB device stack — no other subsystem (console,
 * logging) should be configured to use the CDC ACM UART.
 */

#include "wire.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/addr.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wire, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * USB device stack — we own the USBD context
 * ---------------------------------------------------------------------------*/
#define AURACLE_USB_VID 0x2fe3   /* Zephyr project VID — replace for production */
#define AURACLE_USB_PID 0x00A0   /* Auracle test harness */

USBD_DEVICE_DEFINE(wire_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   AURACLE_USB_VID, AURACLE_USB_PID);

USBD_DESC_LANG_DEFINE(wire_lang);
USBD_DESC_MANUFACTURER_DEFINE(wire_mfr, "Focal Naim");
USBD_DESC_PRODUCT_DEFINE(wire_product, "Auracle nRF5340");

USBD_DESC_CONFIG_DEFINE(wire_fs_cfg_desc, "FS Configuration");

USBD_CONFIGURATION_DEFINE(wire_fs_config,
			  USB_SCD_SELF_POWERED,
			  125, /* bMaxPower in 2 mA units = 250 mA */
			  &wire_fs_cfg_desc);

static int wire_usbd_init(void)
{
	int err;

	err = usbd_add_descriptor(&wire_usbd, &wire_lang);
	if (err) {
		LOG_ERR("Wire USB: add lang descriptor failed (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&wire_usbd, &wire_mfr);
	if (err) {
		LOG_ERR("Wire USB: add mfr descriptor failed (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&wire_usbd, &wire_product);
	if (err) {
		LOG_ERR("Wire USB: add product descriptor failed (%d)", err);
		return err;
	}

	err = usbd_add_configuration(&wire_usbd, USBD_SPEED_FS, &wire_fs_config);
	if (err) {
		LOG_ERR("Wire USB: add FS config failed (%d)", err);
		return err;
	}

	err = usbd_register_all_classes(&wire_usbd, USBD_SPEED_FS, 1, NULL);
	if (err) {
		LOG_ERR("Wire USB: register classes failed (%d)", err);
		return err;
	}

	/* CDC ACM uses Interface Association Descriptor — needs this triple */
	usbd_device_set_code_triple(&wire_usbd, USBD_SPEED_FS,
				    USB_BCC_MISCELLANEOUS, 0x02, 0x01);

	err = usbd_init(&wire_usbd);
	if (err) {
		LOG_ERR("Wire USB: init failed (%d)", err);
		return err;
	}

	err = usbd_enable(&wire_usbd);
	if (err) {
		LOG_ERR("Wire USB: enable failed (%d)", err);
		return err;
	}

	LOG_INF("Wire USB: device enabled");
	return 0;
}

/* ---------------------------------------------------------------------------
 * USB CDC ACM device (UART API)
 * ---------------------------------------------------------------------------*/
static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm0));

/* ---------------------------------------------------------------------------
 * Tap state — all off by default
 * ---------------------------------------------------------------------------*/
enum tap_id {
	TAP_ADV = 0,
	TAP_SYNC,
	TAP_BIS,
	TAP_LOG,
	TAP_COUNT
};

static const char *const tap_names[TAP_COUNT] = {"adv", "sync", "bis", "log"};
static bool taps[TAP_COUNT];

/* ---------------------------------------------------------------------------
 * TX — mutex prevents interleaved lines from concurrent threads
 * ---------------------------------------------------------------------------*/
static K_MUTEX_DEFINE(tx_mutex);

static void wire_write(const char *buf, size_t len)
{
	k_mutex_lock(&tx_mutex, K_FOREVER);
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
	k_mutex_unlock(&tx_mutex);
}

/* ---------------------------------------------------------------------------
 * TX: hello
 * ---------------------------------------------------------------------------*/
static void wire_send_hello(void)
{
	char buf[128];
	const char *role = "unknown";

#if defined(CONFIG_AUDIO_DEV)
#if CONFIG_AUDIO_DEV == 1
	role = "sink";
#elif CONFIG_AUDIO_DEV == 2
	role = "source";
#endif
#endif

	int len = snprintf(buf, sizeof(buf),
		"{\"type\":\"hello\",\"firmware\":\"auracle-nrf5340\","
		"\"version\":\"0.1.0\",\"role\":\"%s\"}\n",
		role);

	if (len > 0 && (size_t)len < sizeof(buf)) {
		wire_write(buf, len);
	}
}

/* ---------------------------------------------------------------------------
 * TX: adv — raw BLE advertisement
 * ---------------------------------------------------------------------------*/
void wire_send_adv(const bt_addr_le_t *addr, int8_t rssi,
		   const uint8_t *data, uint16_t data_len)
{
	if (!taps[TAP_ADV] || !device_is_ready(uart_dev)) {
		return;
	}

	char buf[1024];
	char addr_str[BT_ADDR_STR_LEN];
	char hex_buf[512]; /* 255 bytes max → 510 hex chars + nul */

	bt_addr_to_str(&addr->a, addr_str, sizeof(addr_str));
	bin2hex(data, data_len, hex_buf, sizeof(hex_buf));

	const char *addr_type = (addr->type == BT_ADDR_LE_PUBLIC) ? "public" : "random";

	int len = snprintf(buf, sizeof(buf),
		"{\"type\":\"adv\",\"ts\":%u,\"addr\":\"%s\","
		"\"addr_type\":\"%s\",\"rssi\":%d,\"data\":\"%s\"}\n",
		k_uptime_get_32(), addr_str, addr_type, rssi, hex_buf);

	if (len > 0 && (size_t)len < sizeof(buf)) {
		wire_write(buf, len);
	}
}

/* ---------------------------------------------------------------------------
 * TX: sync — PA sync state change
 * ---------------------------------------------------------------------------*/
void wire_send_sync(const char *state, uint32_t broadcast_id,
		    const bt_addr_le_t *addr)
{
	if (!taps[TAP_SYNC] || !device_is_ready(uart_dev)) {
		return;
	}

	char buf[256];
	char addr_str[BT_ADDR_STR_LEN];

	bt_addr_to_str(&addr->a, addr_str, sizeof(addr_str));

	int len = snprintf(buf, sizeof(buf),
		"{\"type\":\"sync\",\"ts\":%u,\"state\":\"%s\","
		"\"broadcast_id\":\"0x%06x\",\"addr\":\"%s\"}\n",
		k_uptime_get_32(), state, broadcast_id, addr_str);

	if (len > 0 && (size_t)len < sizeof(buf)) {
		wire_write(buf, len);
	}
}

/* ---------------------------------------------------------------------------
 * TX: bis — BIS stream state change
 * ---------------------------------------------------------------------------*/
void wire_send_bis(const char *state, uint32_t broadcast_id,
		   uint8_t bis_index)
{
	if (!taps[TAP_BIS] || !device_is_ready(uart_dev)) {
		return;
	}

	char buf[256];

	int len = snprintf(buf, sizeof(buf),
		"{\"type\":\"bis\",\"ts\":%u,\"state\":\"%s\","
		"\"broadcast_id\":\"0x%06x\",\"bis_index\":%d}\n",
		k_uptime_get_32(), state, broadcast_id, bis_index);

	if (len > 0 && (size_t)len < sizeof(buf)) {
		wire_write(buf, len);
	}
}

/* ---------------------------------------------------------------------------
 * RX: command parsing (strstr — good enough for a tiny command set)
 * ---------------------------------------------------------------------------*/
static int tap_id_from_str(const char *cmd)
{
	for (int i = 0; i < TAP_COUNT; i++) {
		if (strstr(cmd, tap_names[i])) {
			return i;
		}
	}
	return -1;
}

static void wire_process_cmd(char *cmd)
{
	LOG_DBG("Wire RX: %s", cmd);

	if (strstr(cmd, "\"tap_enable\"")) {
		int id = tap_id_from_str(cmd);

		if (id >= 0) {
			taps[id] = true;
			LOG_INF("Tap enabled: %s", tap_names[id]);
		}
	} else if (strstr(cmd, "\"tap_disable\"")) {
		int id = tap_id_from_str(cmd);

		if (id >= 0) {
			taps[id] = false;
			LOG_INF("Tap disabled: %s", tap_names[id]);
		}
	} else {
		LOG_WRN("Unknown wire command");
	}
}

/* ---------------------------------------------------------------------------
 * RX: UART IRQ callback + processing thread
 * ---------------------------------------------------------------------------*/
#define RX_BUF_SIZE 256

static char rx_buf[RX_BUF_SIZE];
static uint16_t rx_pos;
static char rx_ready[RX_BUF_SIZE];
static K_SEM_DEFINE(rx_line_sem, 0, 1);

static void uart_irq_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!uart_irq_rx_ready(dev)) {
			continue;
		}

		uint8_t c;

		while (uart_fifo_read(dev, &c, 1) == 1) {
			if (c == '\n' || c == '\r') {
				if (rx_pos > 0) {
					rx_buf[rx_pos] = '\0';
					memcpy(rx_ready, rx_buf, rx_pos + 1);
					rx_pos = 0;
					k_sem_give(&rx_line_sem);
				}
			} else if (rx_pos < RX_BUF_SIZE - 1) {
				rx_buf[rx_pos++] = c;
			}
		}
	}
}

#define WIRE_RX_STACK_SIZE 2048
#define WIRE_RX_PRIORITY   7

static K_THREAD_STACK_DEFINE(wire_rx_stack, WIRE_RX_STACK_SIZE);
static struct k_thread wire_rx_thread_data;

static void wire_rx_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Wait for USB host to open the port (DTR asserted) */
	uint32_t dtr = 0;
	int err;

	LOG_INF("Wire: waiting for USB host (DTR)...");

	while (!dtr) {
		err = uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
		if (err && err != -EAGAIN) {
			LOG_WRN("Wire: DTR query failed (%d), proceeding anyway", err);
			break;
		}
		k_sleep(K_MSEC(100));
	}

	LOG_INF("Wire: USB host connected (DTR=%u)", dtr);
	wire_send_hello();

	/* Enable RX interrupts now that host is connected */
	uart_irq_callback_set(uart_dev, uart_irq_handler);
	uart_irq_rx_enable(uart_dev);

	/* Process incoming command lines */
	while (1) {
		k_sem_take(&rx_line_sem, K_FOREVER);
		wire_process_cmd(rx_ready);
	}
}

/* ---------------------------------------------------------------------------
 * Init
 * ---------------------------------------------------------------------------*/
int wire_init(void)
{
	int err;

	/* Initialise and enable the USB device stack */
	err = wire_usbd_init();
	if (err) {
		LOG_ERR("Wire: USB init failed (%d)", err);
		return err;
	}

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("Wire: CDC ACM device not ready");
		return -ENODEV;
	}

	k_thread_create(&wire_rx_thread_data, wire_rx_stack,
			WIRE_RX_STACK_SIZE,
			wire_rx_thread_fn, NULL, NULL, NULL,
			K_PRIO_PREEMPT(WIRE_RX_PRIORITY), 0, K_NO_WAIT);
	k_thread_name_set(&wire_rx_thread_data, "wire_rx");

	LOG_INF("Wire protocol initialised");

	return 0;
}
