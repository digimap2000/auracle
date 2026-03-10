#include "status.h"
#include "uart_bridge.h"
#include "bridge_server.h"
#include "wifi_manager.h"

#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrupt, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t init_mdns(void)
{
    esp_err_t err = mdns_init();
    char port_str[8];
    mdns_txt_item_t txt[] = {
        { .key = "name", .value = CONFIG_AURACLE_MDNS_INSTANCE_NAME },
        { .key = "role", .value = "uart_wifi_bridge" },
        { .key = "transport", .value = "tcp" },
        { .key = "protocol", .value = "jsonl" },
        { .key = "port", .value = port_str },
    };

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    snprintf(port_str, sizeof(port_str), "%d", CONFIG_AURACLE_BRIDGE_TCP_PORT);

    err = mdns_hostname_set(CONFIG_AURACLE_MDNS_HOSTNAME);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_instance_name_set(CONFIG_AURACLE_MDNS_INSTANCE_NAME);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS instance name set failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_service_add(CONFIG_AURACLE_MDNS_INSTANCE_NAME, "_auracle", "_tcp",
                           CONFIG_AURACLE_BRIDGE_TCP_PORT, txt, sizeof(txt) / sizeof(txt[0]));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS service add failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS: %s.local  service=%s._auracle._tcp  port=%d",
             CONFIG_AURACLE_MDNS_HOSTNAME,
             CONFIG_AURACLE_MDNS_INSTANCE_NAME,
             CONFIG_AURACLE_BRIDGE_TCP_PORT);
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "  Auracle Bridge — ESP32-S3");
    ESP_LOGI(TAG, "  Focal & Naim");
    ESP_LOGI(TAG, "═══════════════════════════════════════");

    /* 1. NVS (required for WiFi credential storage) */
    ESP_ERROR_CHECK(init_nvs());

    /* 2. Shared status */
    ESP_ERROR_CHECK(status_init());

    /* 3. Raw UART transport to nRF5340 */
    ESP_ERROR_CHECK(uart_bridge_init());

    /* 4. WiFi — blocks until STA connected (runs provisioning if needed) */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 5. mDNS — discoverable as auracle.local */
    init_mdns();  /* Non-fatal if this fails */

    /* 6. Raw TCP bridge server */
    ESP_ERROR_CHECK(bridge_server_start());

    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "  Ready — tcp://%s.local:%d", CONFIG_AURACLE_MDNS_HOSTNAME,
             CONFIG_AURACLE_BRIDGE_TCP_PORT);
    ESP_LOGI(TAG, "═══════════════════════════════════════");
}
