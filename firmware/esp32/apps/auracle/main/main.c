#include "status.h"
#include "discovery.h"
#include "uart_proto.h"
#include "device_proto.h"
#include "wifi_manager.h"
#include "http_server.h"

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

/* ── Device task ──────────────────────────────────────────────── */

/*
 * Runs startup discovery then polls the UART for incoming lines and
 * checks for request timeouts. Runs forever at a 10ms tick.
 */
static void device_task(void *arg)
{
    (void)arg;
    attached_device_start_discovery();

    uint32_t last_retry_ms = 0;

    while (1) {
        attached_device_process();

        /*
         * Belt-and-suspenders retry: if startup discovery failed and the
         * device hasn't sent a "ready" event to trigger automatic restart,
         * re-attempt every 10 seconds. This recovers from cases where the
         * ready event was lost (e.g., received before our UART task started).
         */
        if (attached_device_startup_failed()) {
            uint32_t now_ms = (uint32_t)((uint64_t)xTaskGetTickCount()
                                         * portTICK_PERIOD_MS);
            if (now_ms - last_retry_ms >= 10000u) {
                ESP_LOGI(TAG, "Retrying startup discovery...");
                attached_device_start_discovery();
                last_retry_ms = now_ms;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
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

    /* 3. Discovery cache / normalization layer */
    ESP_ERROR_CHECK(discovery_init());

    /* 4. UART transport to nRF5340 */
    ESP_ERROR_CHECK(uart_proto_init());

    /* 5. Device protocol task — discovery + polling */
    xTaskCreate(device_task, "device", 4096, NULL, 5, NULL);

    /* 6. WiFi — blocks until STA connected (runs provisioning if needed) */
    ESP_ERROR_CHECK(wifi_manager_init());

    /* 7. mDNS — discoverable as auracle.local */
    init_mdns();  /* Non-fatal if this fails */

    /* 8. HTTP API server */
    ESP_ERROR_CHECK(http_server_start());

    ESP_LOGI(TAG, "═══════════════════════════════════════");
    ESP_LOGI(TAG, "  Ready — http://%s.local/api/status", CONFIG_AURACLE_MDNS_HOSTNAME);
    ESP_LOGI(TAG, "═══════════════════════════════════════");
}
