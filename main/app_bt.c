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

#define uart_num (UART_NUM_2)

uint8_t *STARTFRAMEBT = (uint8_t *) "\r\nSTARTFRAMEBT,";
uint8_t *STARTFRAMEWF = (uint8_t *) "\r\nSTARTFRAMEWF,";
unsigned STARTFRAMELEN = 15;

static SemaphoreHandle_t mutex;

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void controller_rcv_pkt_ready(void)
{
    //printf("controller rcv pkt ready\n");
}

int pkt_dump(const char *msg, const uint8_t *data, uint16_t len)
{
    printf("%s", msg);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
    return 0;
}

void write_frame_to_uart(int is_bt, uint8_t *data, uint16_t len)
{
    char *startframe = is_bt ? ((char *) STARTFRAMEBT) : ((char *) STARTFRAMEWF);
    char numbuf[6];
    int i;

    xSemaphoreTake(mutex, 10000);

    //pkt_dump("host RX: ", data, len);
    uart_write_bytes(uart_num, startframe, STARTFRAMELEN);

    sprintf(numbuf, "%04X,", len);
    uart_write_bytes(uart_num, numbuf, 5);
    
    for (i=0; i < len; i++) {
        char minibuf[3];
        sprintf(minibuf, "%02X", data[i]);
        uart_write_bytes(uart_num, minibuf, 2);
    }

    xSemaphoreGive(mutex);
}

/*
 * @brief: BT controller callback function, to transfer data packet to upper
 *         controller is ready to receive command
 */
static int vhci_recv_pkt_cb(uint8_t *data, uint16_t len)
{
    write_frame_to_uart(1, data, len);
    return 0;
}

int esp_wifi_internal_tx(int wifi_if, void *buffer, u16_t len);
static void wifi_inject_packet_cb(void *ctx)
{
    struct pbuf *p = (struct pbuf *) ctx;
    pkt_dump("wifi inject cb: ", p->payload, p->len);

    switch(esp_wifi_internal_tx(0, p->payload, p->len))
    {
        case ERR_OK:
            printf("Packet in the air!\n");
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
    struct pbuf *p = malloc(sizeof(struct pbuf));
    if (p == NULL) return;

    printf("sizeof(struct pbuf) = %d\n", sizeof(struct pbuf));
    pkt_dump("wifi inject TX: ", data, len);

    p->payload = data;
    p->len = len;
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

int hexdigit_to_num(unsigned char c)
{
    if (c >= '0' && c <= '9' )
        return c - '0';
    else if( c >= 'a' && c <= 'f' )
        return c + 10 - 'a';
    else if( c >= 'A' && c <= 'F' )
        return c + 10 - 'A';
    else
        return -1;
}

/*
 * Convert a hex string to bytes.
 * Return 0 on success, -1 on error.
 */
int unhexify(unsigned char *buf)
{
    unsigned len = strlen((char *) buf);
    int i;

    if (len % 2 != 0) return -1;

    for (i=0; i < len; i += 2 )
    {
        unsigned char c;
        int num;

        num = hexdigit_to_num(buf[i]);
        if (num < 0) return -1;
        c = num << 4;

        num = hexdigit_to_num(buf[i+1]);
        if (num < 0) return -1;
        c |= num;

        buf[i/2] = c;
    }

    return( 0 );
}

void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
//        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uart_num, 2048, 0, 0, NULL, 0);
}

uint8_t read_byte_from_uart()
{
    uint8_t ch;
    int len = 0;

    while (len == 0)
        len = uart_read_bytes(uart_num, &ch, 1, 20 / portTICK_RATE_MS);
    
    return ch;
}

void read_bytes_from_uart(uint8_t *pbuf, unsigned sz)
{
    int i;
    for (i=0; i < sz; i++)
        pbuf[i] = read_byte_from_uart();
}

#define STATE_IDLE    0
#define STATE_STARTED 1
#define STATE_READPKT 2
#define PKT_TYPE_BT   1
#define PKT_TYPE_WIFI 2

unsigned read_pkt_from_uart(uint8_t *pktbuf, int *p_pkttype)
{
    int state = STATE_IDLE;
    int pktlen = 0;
    int pkttype = 0;

    while (1) {
        if (state == STATE_IDLE) {
            pktbuf[0] = read_byte_from_uart();
            printf("%c", pktbuf[0]);
            if (pktbuf[0] == '\r') {
                read_bytes_from_uart(&pktbuf[1], STARTFRAMELEN-1);
                if (memcmp(pktbuf, STARTFRAMEBT, STARTFRAMELEN) == 0) {
                    state = STATE_STARTED;
                    pkttype = PKT_TYPE_BT;
                }
                if (memcmp(pktbuf, STARTFRAMEWF, STARTFRAMELEN) == 0) {
                    state = STATE_STARTED;
                    pkttype = PKT_TYPE_WIFI;
                }
            }
        }

        if (state == STATE_STARTED) {
            read_bytes_from_uart(pktbuf, 5);
            if (pktbuf[4] != ',') {
                state = STATE_IDLE;
                continue;
            }
            pktbuf[4] = 0;
            pktlen = strtol((char *) pktbuf, NULL, 16);
            printf("pktlen=%d\n", pktlen);
            state = STATE_READPKT;
        }

        if (state == STATE_READPKT) {
            read_bytes_from_uart(pktbuf, pktlen*2);
            pktbuf[pktlen*2] = 0;
            printf("PKT: %s\n", pktbuf);
            unhexify(pktbuf);
            *p_pkttype = pkttype;
            return pktlen;
        }
    }
}

void wait_until_ready()
{
    while (!esp_vhci_host_check_send_available()) {
        printf("HCI controller not ready\n");
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

uint8_t g_pktbuf[2000];

void vhcibridge(void *pvParameters)
{
    int len;

    printf("Starting vhci bridge\n");

    mutex = xSemaphoreCreateMutex();
    esp_vhci_host_register_callback(&vhci_host_cb);

    uart_init();

    wifi_start();
    
    while (1) {
        int pkttype = 0;
        wait_until_ready();

        len = read_pkt_from_uart(g_pktbuf, &pkttype);

        if (len == 4 && memcmp("\xFF\x00\x01\x00", g_pktbuf, 4) == 0) {
            printf("skipping pkt FF 00 01 00\n");
            continue;
        }

        switch (pkttype) {
        case PKT_TYPE_BT:
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
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        printf("Bluetooth controller initialize failed");
        return;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
        printf("Bluetooth controller enable failed");
        return;
    }

    xTaskCreatePinnedToCore(&vhcibridge, "vhcibridge", 4096, NULL, 5, NULL, 0);
}

