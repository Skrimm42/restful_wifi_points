// wifi.h
#define DEFAULT_SCAN_LIST_SIZE 10

extern xSemaphoreHandle s_semph_get_ap_list;
extern wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
extern uint16_t ap_count;
extern uint16_t id;
extern const char* password;

esp_err_t wifi_init_sta(const char* ap_name, const char* ap_password);
void wifi_init_softap(void);
void wifi_scan(void);
void wifi_station_deinit(void);