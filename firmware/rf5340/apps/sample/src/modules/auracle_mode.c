/*
 * Auracle runtime mode control for the nRF5340 side.
 */

#include "auracle_mode.h"

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "broadcast_sink.h"
#include "bt_mgmt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(auracle_mode, LOG_LEVEL_INF);

static atomic_t current_mode = ATOMIC_INIT(AURACLE_MODE_MONITOR);
static K_MUTEX_DEFINE(mode_mutex);

const char *auracle_mode_name(enum auracle_mode mode)
{
	switch (mode) {
	case AURACLE_MODE_MONITOR:
		return "monitor";
	case AURACLE_MODE_SINK:
		return "sink";
	default:
		return "unknown";
	}
}

enum auracle_mode auracle_mode_get(void)
{
	return (enum auracle_mode)atomic_get(&current_mode);
}

bool auracle_mode_allows_broadcast_auto_sync(void)
{
	return auracle_mode_get() == AURACLE_MODE_SINK;
}

int auracle_mode_from_str(const char *name, enum auracle_mode *mode)
{
	if (!name || !mode) {
		return -EINVAL;
	}

	if (strcmp(name, "monitor") == 0) {
		*mode = AURACLE_MODE_MONITOR;
		return 0;
	}

	if (strcmp(name, "sink") == 0) {
		*mode = AURACLE_MODE_SINK;
		return 0;
	}

	return -EINVAL;
}

static int auracle_mode_apply_locked(enum auracle_mode mode)
{
	int ret;

	ret = bt_le_scan_stop();
	if (ret && ret != -EALREADY) {
		LOG_WRN("Stop scan failed while switching mode: %d", ret);
	}

	ret = broadcast_sink_disable();
	if (ret && ret != -EALREADY) {
		LOG_WRN("Broadcast sink disable failed while switching mode: %d", ret);
	}

	switch (mode) {
	case AURACLE_MODE_MONITOR:
		ret = bt_mgmt_scan_start(0, 0, BT_MGMT_SCAN_TYPE_BROADCAST, NULL,
					 BRDCAST_ID_NOT_USED);
		break;

	case AURACLE_MODE_SINK:
		ret = bt_mgmt_scan_start(0, 0, BT_MGMT_SCAN_TYPE_BROADCAST,
					 CONFIG_BT_AUDIO_BROADCAST_NAME,
					 CONFIG_BT_AUDIO_BROADCAST_ID);
		break;

	default:
		return -EINVAL;
	}

	if (ret && ret != -EALREADY) {
		LOG_ERR("Failed to apply mode %s: %d", auracle_mode_name(mode), ret);
		return ret;
	}

	atomic_set(&current_mode, mode);
	LOG_INF("Auracle mode active: %s", auracle_mode_name(mode));
	return 0;
}

int auracle_mode_apply(void)
{
	int ret;

	k_mutex_lock(&mode_mutex, K_FOREVER);
	ret = auracle_mode_apply_locked(auracle_mode_get());
	k_mutex_unlock(&mode_mutex);

	return ret;
}

int auracle_mode_set(enum auracle_mode mode)
{
	int ret;

	k_mutex_lock(&mode_mutex, K_FOREVER);
	ret = auracle_mode_apply_locked(mode);
	k_mutex_unlock(&mode_mutex);

	return ret;
}
