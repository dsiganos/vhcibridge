#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"

static const char * TAG = "EVENT";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    uint8_t primary;
    wifi_second_chan_t second;
    esp_err_t err;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
	ESP_ERROR_CHECK(esp_wifi_connect());
	break;
    case SYSTEM_EVENT_STA_GOT_IP:
    err = esp_wifi_get_channel(&primary, &second);
    printf("channel=%d err=%d\n", primary, err);
	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
	ESP_LOGI(TAG, "got ip:%s\n",
		ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
	ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
	ESP_ERROR_CHECK(esp_wifi_connect());
	break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_start(void)
{
    tcpip_adapter_init();

    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA); // Don't run a DHCP client

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
