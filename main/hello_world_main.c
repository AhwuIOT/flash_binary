#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_err.h"

#define DOWNLOAD_URL "https://5txv1hps-5000.asse.devtunnels.ms/firmware/test_firmware.bin"
#define BUFFER_SIZE 4096

static const char *TAG = "bin_downloader";

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

void app_main(void) {
    // 初始化 NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // 尋找名為 "binary" 的自定 partition
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, "binary");

    if (partition == NULL) {
        ESP_LOGE(TAG, "Failed to find 'binary' partition");
        return;
    }

    ESP_LOGI(TAG, "Found partition at 0x%X, size: 0x%X",
             (unsigned int)partition->address,
             (unsigned int)partition->size);

    // 開啟 HTTP
    esp_http_client_config_t config = {
        .url = DOWNLOAD_URL,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return;
    }

    uint8_t *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return;
    }

    int offset = 0;
    int total_bytes = 0;

    ESP_LOGI(TAG, "Start downloading...");
    while (1) {
        int read_bytes = esp_http_client_read(client, (char *)buffer, BUFFER_SIZE);
        if (read_bytes <= 0) break;

        if (offset + read_bytes > partition->size) {
            ESP_LOGE(TAG, "File too large for partition");
            break;
        }

        esp_err_t res = esp_partition_write(partition, offset, buffer, read_bytes);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Write failed at offset %d", offset);
            break;
        }

        offset += read_bytes;
        total_bytes += read_bytes;
        ESP_LOGI(TAG, "Written %d bytes so far...", total_bytes);
    }

    free(buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "Download complete: %d bytes", total_bytes);
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
