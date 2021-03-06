/* HTTP Restful API Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_vfs_semihost.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "protocol_examples_common.h"
#include "wifi.h"
#include "cJSON.h"
#if CONFIG_EXAMPLE_WEB_DEPLOY_SD
#include "driver/sdmmc_host.h"
#endif

#define MDNS_INSTANCE "esp home web server"
static const char *TAG = "example";

esp_err_t start_rest_server(const char *base_path);

static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(CONFIG_EXAMPLE_MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}};

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

#if CONFIG_EXAMPLE_WEB_DEPLOY_SD
esp_err_t init_fs(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY); // CMD
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);  // D0
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);  // D1
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY); // D2
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY); // D3

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024};

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(CONFIG_EXAMPLE_WEB_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }
    /* print card info if mount successfully */
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}
#endif

#if CONFIG_EXAMPLE_WEB_DEPLOY_SF
esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}
#endif

void scan_and_start_softAP(void)
{
    wifi_station_deinit();
    wifi_scan();
    wifi_station_deinit();
    wifi_init_softap();
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(CONFIG_EXAMPLE_MDNS_HOST_NAME);
    ESP_ERROR_CHECK(init_fs());

    //read name of wifi and password from credentials.txt and try to connect to wifi AP (router)
    FILE *fd = NULL;
    esp_err_t result = ESP_OK;
    char file_buf[1024];
    size_t chunksize = 0;
    fd = fopen("/www/credentials.txt", "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to open credentials.txt");
        result = ESP_FAIL;
    }
    else
    {
        chunksize = fread(file_buf, 1, 1024, fd);
        if (chunksize == 0)
        {
            ESP_LOGE(TAG, "Failed to read credentials.txt");
            result = ESP_FAIL;
        }
    }
    ESP_ERROR_CHECK(result);
    ESP_LOGI(TAG, "Read from SD (credentials.txt) %s, size %d", file_buf, chunksize);
    cJSON *root = cJSON_Parse(file_buf);
    cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *pass_json = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(ssid_json) && (ssid_json->valuestring == NULL))
    {
        ESP_LOGE(TAG, "SD Card JSON isn't valid. ssid field error.");
        result = ESP_FAIL;
    }
    else if (!cJSON_IsString(pass_json) && (pass_json->valuestring == NULL))
    {
        ESP_LOGE(TAG, "SD Card JSON isn't valid. password field error.");
        result = ESP_FAIL;
    }
    ESP_ERROR_CHECK(result);
    ssid = ssid_json->valuestring;
    password = pass_json->valuestring;

    if (wifi_init_sta(ssid, password) == ESP_OK)
    {
        ESP_LOGI(TAG, "Connected to WiFi in Station mode");
        ESP_ERROR_CHECK(start_rest_server("/www/prod"));
    }
    else
    {
        ESP_LOGE(TAG, "Attempt to connect WiFi in Station mode FAILED, setup SoftAP mode");
        scan_and_start_softAP();
        ESP_ERROR_CHECK(start_rest_server("/www/softap"));
    }
    cJSON_Delete(root);
}
