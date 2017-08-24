// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "bt.h"
#include "esp_log.h"
#include "wifi.h"
#include "lwip/tcpip.h"
#include "uart.h"
#include "hex.h"

int pkt_dump(const char *msg, const uint8_t *data, uint16_t len)
{
    printf("%s", msg);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
    return 0;
}

static int vhci_recv_pkt_cb(uint8_t *data, uint16_t len)
{
    struct bridge_pkt *p = malloc(sizeof(struct bridge_pkt) + len);
    if (p == NULL) return 0;

    p->payload = p + 1;
    p->len = len;
    memcpy(p->payload, data, len);
    
    if (enqueue_to_uart(p))
        free(p);

    return 0;
}

static void controller_rcv_pkt_ready(void)
{
    vhci_recv_pkt_cb((uint8_t *) "", 0);
}

int esp_wifi_internal_tx(int wifi_if, void *buffer, u16_t len);
static void wifi_inject_packet_cb(void *ctx)
{
    struct pbuf *p = (struct pbuf *) ctx;
    pkt_dump("wifi inject cb: ", p->payload, p->len);

    switch(esp_wifi_internal_tx(0, p->payload, p->len))
    {
        case ERR_OK:
            //printf("Packet in the air!\n");
            break;
        case ERR_IF:
            printf("WiFi driver error\n");
            break;
        default:
            printf("Some other error I don't want to control now\n");
            break;
    }
    free(p);
}

static void wifi_inject_packet(uint8_t *data, uint16_t len)
{
    err_t rc;
    struct pbuf *p = malloc(sizeof(struct pbuf) + len);
    if (p == NULL) {
        printf("%s] Failed to malloc buffer\n", __FUNCTION__);
        return;
    }

    pkt_dump("wifi inject TX: ", data, len);

    p->payload = p + 1;
    p->len = len;
    memcpy(p->payload, data, len);

    rc = tcpip_callback_with_block(wifi_inject_packet_cb, p, 1);
    if (rc) free(p);
}

static int vhci_send_pkt(uint8_t *data, uint16_t len)
{
    //pkt_dump("host TX: ", data, len);
    esp_vhci_host_send_packet(data, len);
    return 0;
}

static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready,
    vhci_recv_pkt_cb
};

void wait_until_ready()
{
    while (!esp_vhci_host_check_send_available()) {
        printf("HCI controller not ready\n");
        vTaskDelay(1 / portTICK_PERIOD_MS + 1);
    }
}

uint8_t g_pktbuf[8000];

void from_uart(void *pvParameters)
{
    int len;

    printf("Starting from uart task\n");

    esp_vhci_host_register_callback(&vhci_host_cb);

    uart_init();

    wifi_start();
    
    wait_until_ready();
    write_frame_to_uart(1, (uint8_t *) "", 0);
    
    while (1) {
        int pkttype = 0;

        len = read_pkt_from_uart(g_pktbuf, &pkttype);

        if (len == 0) {
            // send status of controller
            write_frame_to_uart(1, (uint8_t *) "", 0);
            continue;
        }

        if (len == 4 && memcmp("\xFF\x00\x01\x00", g_pktbuf, 4) == 0) {
            printf("not skipping pkt FF 00 01 00\n");
            //continue;
        }

        switch (pkttype) {
        case PKT_TYPE_BT:
            wait_until_ready();
            vhci_send_pkt(g_pktbuf, len);
            break;
        case PKT_TYPE_WIFI:
            wifi_inject_packet(g_pktbuf, len);
            break;
        default:
            printf("ERROR: unknown packet type!");
        }
    }
}

void app_main()
{
    extern QueueHandle_t qh;
    extern SemaphoreHandle_t mutex;
    void to_uart(void *pvParameters);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    qh = xQueueCreate(200, sizeof(struct bridge_pkt *));
    if (!qh) {
        printf("failed to create queue\n");
        return;
    }

    mutex = xSemaphoreCreateMutex();

    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        printf("Bluetooth controller initialize failed");
        return;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
        printf("Bluetooth controller enable failed");
        return;
    }

    printf("portTICK_RATE_MS=%d\n", portTICK_RATE_MS);

    xTaskCreatePinnedToCore(&from_uart, "from_uart", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(&to_uart,   "to_uart",   4096, NULL, 5, NULL, 0);
}

