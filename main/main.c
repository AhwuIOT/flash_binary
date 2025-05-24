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

// 固定參數
#define DOWNLOAD_URL "http://yourbinary.url/binary.bin" 
#define BUFFER_SIZE 4096
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;

/**
 * HTTP事件處理器（目前未實作細節）
 */
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

/**
 * Wi-Fi事件處理器，處理啟動與獲取IP事件
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * 初始化 Wi-Fi STA 模式，並等待連線完成
 */
static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 註冊 Wi-Fi 和 IP 事件處理器
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

/**
 * 主程式入口點
 */
void app_main(void) {
    ESP_LOGI(TAG, "🔧 Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 尋找名為 "binary" 的 partition
    ESP_LOGI(TAG, "🔍 Locating 'binary' partition...");
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "binary");
    if (partition == NULL) {
        ESP_LOGE(TAG, "❌ Partition 'binary' not found");
        return;
    }

    ESP_LOGI(TAG, "✅ Found partition at 0x%X, size: 0x%X",
             (unsigned int)partition->address, (unsigned int)partition->size);

    // 建立 Wi-Fi 連線
    wifi_init_sta();

    // 設定 HTTP 下載
    ESP_LOGI(TAG, "🌐 Connecting to: %s", DOWNLOAD_URL);
    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .event_handler = _http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    // 開始讀取資料
    esp_http_client_fetch_headers(client);
    uint8_t *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "❌ Memory allocation failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return;
    }

    int offset = 0;
    int total_bytes = 0;
    ESP_LOGI(TAG, "🚀 Start downloading...");
    while (1) {
        int read_bytes = esp_http_client_read(client, (char *)buffer, BUFFER_SIZE);
        if (read_bytes < 0) {
            ESP_LOGE(TAG, "❌ HTTP read error");
            break;
        } else if (read_bytes == 0) {
            ESP_LOGI(TAG, "✅ HTTP download complete");
            break;
        }

        if (offset + read_bytes > partition->size) {
            ESP_LOGE(TAG, "❌ Binary too large for partition");
            break;
        }

        esp_err_t res = esp_partition_write(partition, offset, buffer, read_bytes);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "❌ Write error at offset %d", offset);
            break;
        }

        offset += read_bytes;
        total_bytes += read_bytes;
        ESP_LOGI(TAG, "📥 Written %d bytes...", total_bytes);
    }

    // 清理資源
    free(buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "✅ Done. Total downloaded: %d bytes", total_bytes);

    // 讀取並顯示前64位元組以驗證寫入成功
    uint8_t read_buf[64] = {0};
    esp_err_t read_res = esp_partition_read(partition, 0, read_buf, sizeof(read_buf));
    if (read_res == ESP_OK) {
        ESP_LOGI(TAG, "🔍 Verify: Read back first 64 bytes:");
        ESP_LOG_BUFFER_HEXDUMP(TAG, read_buf, sizeof(read_buf), ESP_LOG_INFO);
    } else {
        ESP_LOGE(TAG, "❌ Failed to read partition for verification");
    }

    // 等待循環，觀察系統狀態
    while (1) {
        ESP_LOGI(TAG, "🟢 Waiting...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
