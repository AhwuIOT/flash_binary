#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

#define TAG "bin_downloader"

#define DOWNLOAD_URL "http://192.168.1.188:5000/firmware/test_firmware.bin"
#define BUFFER_SIZE 4096

#define WIFI_SSID "ahwufamily"
#define WIFI_PASS "29670221"
#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifi", "📡 Waiting for Wi-Fi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI("wifi", "✅ Wi-Fi connected");
}

void app_main(void)
{
    ESP_LOGI(TAG, "🔧 Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "🔍 Locating 'binary' partition...");
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "binary");
    if (partition == NULL)
    {
        ESP_LOGE(TAG, "❌ Partition 'binary' not found");
        return;
    }

    ESP_LOGI(TAG, "✅ Found partition at 0x%X, size: 0x%X",
             (unsigned int)partition->address, (unsigned int)partition->size);

    wifi_init_sta();

    ESP_LOGI(TAG, "🌐 Connecting to: %s", DOWNLOAD_URL);
    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .event_handler = _http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_TCP, // <--- 正確設定
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }
    esp_http_client_fetch_headers(client);
    uint8_t *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
    {
        ESP_LOGE(TAG, "❌ Memory allocation failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    int offset = 0;
    int total_bytes = 0;
    ESP_LOGI(TAG, "🚀 Start downloading...");
    while (1)
    {
        int read_bytes = esp_http_client_read(client, (char *)buffer, BUFFER_SIZE);
        if (read_bytes < 0)
        {
            ESP_LOGE(TAG, "❌ HTTP read error");
            break;
        }
        else if (read_bytes == 0)
        {
            ESP_LOGI(TAG, "✅ HTTP download complete");
            break;
        }

        if (offset + read_bytes > partition->size)
        {
            ESP_LOGE(TAG, "❌ Binary too large for partition");
            break;
        }

        esp_err_t res = esp_partition_write(partition, offset, buffer, read_bytes);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "❌ Write error at offset %d", offset);
            break;
        }

        offset += read_bytes;
        total_bytes += read_bytes;
        ESP_LOGI(TAG, "📥 Written %d bytes...", total_bytes);
    }

    free(buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "✅ Done. Total downloaded: %d bytes", total_bytes);
    
    while (1)
    {
        ESP_LOGI(TAG, "🟢 Waiting...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// #include <string.h>
// #include <stdio.h>
// #include "esp_log.h"
// #include "esp_system.h"
// #include "nvs_flash.h"
// #include "esp_partition.h"

// static const char *TAG = "partition_test";

// void app_main(void) {
//     ESP_LOGI(TAG, "Initializing NVS");
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ESP_ERROR_CHECK(nvs_flash_init());
//     }

//     // 尋找名為 "binary" 的 partition
//     ESP_LOGI(TAG, "Looking for partition 'binary'");
//     const esp_partition_t *partition = esp_partition_find_first(
//         ESP_PARTITION_TYPE_DATA, 0x40, "binary");

//     if (!partition) {
//         ESP_LOGE(TAG, "Partition 'binary' not found!");
//         return;
//     }

//     ESP_LOGI(TAG, "Found partition at offset 0x%X, size 0x%X",
//              (unsigned int)partition->address, (unsigned int)partition->size);

//     // 準備測試資料
//     uint8_t test_data[16];
//     for (int i = 0; i < sizeof(test_data); i++) {
//         test_data[i] = i;
//     }

//     ESP_LOGI(TAG, "Writing test data to partition...");
//     ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, 0x1000));  // 清除一個 sector
//     ESP_ERROR_CHECK(esp_partition_write(partition, 0, test_data, sizeof(test_data)));

//     // 讀回驗證
//     uint8_t readback[16] = {0};
//     ESP_ERROR_CHECK(esp_partition_read(partition, 0, readback, sizeof(readback)));

//     ESP_LOGI(TAG, "Readback:");
//     for (int i = 0; i < sizeof(readback); i++) {
//         printf("%02X ", readback[i]);
//     }
//     printf("\n");

//     ESP_LOGI(TAG, "Partition test done.");
// }
