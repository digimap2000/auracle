#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct cJSON cJSON;

#define DISCOVERY_ADDR_MAX      18
#define DISCOVERY_ADDR_TYPE_MAX 12
#define DISCOVERY_NAME_MAX      64
#define DISCOVERY_HEX_MAX       512

typedef struct {
    uint64_t seq;
    int64_t  received_us;
    uint32_t source_ts_ms;

    char addr[DISCOVERY_ADDR_MAX];
    char addr_type[DISCOVERY_ADDR_TYPE_MAX];

    int sid;
    int rssi;
    int tx_power;
    int adv_type;
    int adv_props;
    int interval;
    int primary_phy;
    int secondary_phy;

    int  data_len;
    char data_hex[DISCOVERY_HEX_MAX + 1];

    bool has_flags;
    uint8_t flags;

    bool local_name_complete;
    char local_name[DISCOVERY_NAME_MAX];

    bool has_manufacturer_id;
    uint16_t manufacturer_id;

    bool has_service_data_uuid16;
    uint16_t service_data_uuid16;

    bool has_broadcast_audio_announcement;
    bool payload_parse_error;
} discovery_report_t;

esp_err_t discovery_init(void);
esp_err_t discovery_ingest_adv_json(const cJSON *root);

cJSON *discovery_json_create_summary(void);
cJSON *discovery_json_create_reports(size_t limit, bool include_ad_structures);
cJSON *discovery_json_create_devices(size_t limit);

