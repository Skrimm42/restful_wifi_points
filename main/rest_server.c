/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "wifi.h"
#include "freertos/semphr.h"

static const char *REST_TAG = "esp-rest";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_credentials_strSIZE (10240)

typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_credentials_strSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
    {
        type = "image/png";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
    {
        type = "image/x-icon";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
    {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/')
    {
        strlcat(filepath, "/index.html", sizeof(filepath));
    }
    else
    {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1)
    {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do
    {
        /* Read file in chunks into the scratch credentials_strfer */
        read_bytes = read(fd, chunk, SCRATCH_credentials_strSIZE);
        if (read_bytes == -1)
        {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        }
        else if (read_bytes > 0)
        {
            /* Send the credentials_strfer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
            {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Simple handler for light brightness control */
static esp_err_t pass_update_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *credentials_string = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_credentials_strSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, credentials_string + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    credentials_string[total_len] = '\0';

    cJSON *root = cJSON_Parse(credentials_string);
    ESP_LOGI(REST_TAG, "received json  %s", credentials_string);
    // id = cJSON_GetObjectItem(root, "id")->valueint;
    // password = cJSON_GetObjectItem(root, "password")->valuestring;

    // ESP_LOGI(REST_TAG, "SSID selected: %s \nPassword %s", ap_info[id].ssid, password);
    // cJSON_Delete(root);
    cJSON *id_json = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsNumber(id_json))
    {
        ESP_LOGE(REST_TAG, "Received JSON isn't valid. ID field error.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to validate input JSON");
        return ESP_FAIL;
    }
    id = id_json->valueint;

    cJSON *pass_json = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(pass_json) && (pass_json->valuestring == NULL)){
        ESP_LOGE(REST_TAG, "Received JSON isn't valid. PASSWORD field error.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to validate input JSON");
        return ESP_FAIL;
    }
    password = pass_json->valuestring;
    
    //write to file
    cJSON *credentials = cJSON_CreateObject();
    cJSON_AddStringToObject(credentials, "ssid", (const char *)ap_info[id].ssid);
    cJSON_AddStringToObject(credentials, "password", password);
    const char *credentials_str = cJSON_Print(credentials);
    FILE *fd = fopen("/www/credentials.txt", "w");
    if (!fd)
    {
        ESP_LOGE(REST_TAG, "Failed to open file credentials.txt");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to validate input JSON");
        return ESP_FAIL;
    }
    ESP_LOGI(REST_TAG, "Write json string to SD Card %s, size %d", credentials_str, strlen(credentials_str));
        fwrite(credentials_str, strlen(credentials_str), 1, fd);
    fclose(fd);
    free((void *)credentials_str);
    cJSON_Delete(credentials);
    cJSON_Delete(root);

    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(root, "version", IDF_VER);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const char *ap_list_info = cJSON_Print(root);
    httpd_resp_sendstr(req, ap_list_info);
    free((void *)ap_list_info);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Simple handler for getting temperature data */
static esp_err_t temperature_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "raw", esp_random() % 20);
    const char *ap_list_info = cJSON_Print(root);
    httpd_resp_sendstr(req, ap_list_info);
    free((void *)ap_list_info);
    cJSON_Delete(root);
    return ESP_OK;
}

//GET data
static esp_err_t listWiFi_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();

    cJSON *wifi_list = cJSON_AddArrayToObject(root, "aps"); //access points list
    //Add items to array
    //for(...)
    xSemaphoreTake(s_semph_get_ap_list, portMAX_DELAY);
    for (uint8_t i = 0; i < ap_count; i++)
    {
        ESP_LOGI(REST_TAG, "Enter cycle to make JSON array");
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", i);
        cJSON_AddStringToObject(item, "ssid", (const char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
        cJSON_AddItemToArray(wifi_list, item);
    }
    xSemaphoreGive(s_semph_get_ap_list);

    const char *ap_list_info = cJSON_Print(root);
    httpd_resp_sendstr(req, ap_list_info);
    free((void *)ap_list_info);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /* URI handler for fetching temperature data */
    httpd_uri_t wifi_list_get_uri = {
        .uri = "/aps",
        .method = HTTP_GET,
        .handler = listWiFi_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &wifi_list_get_uri);

    /* URI handler for light brightness control */
    httpd_uri_t pass_update_post_uri = {
        .uri = "/updpassword",
        .method = HTTP_POST,
        .handler = pass_update_post_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &pass_update_post_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
