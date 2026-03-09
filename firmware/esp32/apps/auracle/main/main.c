#include "status.h"
#include "wire_uart.h"
#include "wire_proto.h"
#include "wifi_manager.h"
#include "http_server.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"

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
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    mdns_hostname_set(CONFIG_AURACLE_MDNS_HOSTNAME);
    mdns_instance_name_set("Auracle Bridge");
    mdns_service_add(NULL, "_http", "_tcp", CONFIG_AURACLE_HTTP_PORT, NULL, 0);

    ESP_LOGI(TAG, "mDNS: %s.local", CONFIG_AURACLE_MDNS_HOSTNAME);
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

    /* 3. UART to nRF5340 */
    ESP_ERROR_CHECK(wire_uart_init());

    /* 4. Wire protocol parser + ring buffer */
    ESP_ERROR_CHECK(wire_proto_init());

    /* 5. WiFi — blocks until STA connected (runs provisioning if needed) */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 6. mDNS — discoverable as auracle.local */
    init_mdns();  /* Non-fatal if this fails */

    /* 7. HTTP API server */
    ESP_ERROR_CHECK(http_server_start());

    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "  Ready — http://%s.local/api/status", CONFIG_AURACLE_MDNS_HOSTNAME);
    ESP_LOGI(TAG, "═══════════════════════════════════════");
}
