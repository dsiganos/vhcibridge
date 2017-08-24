#include <stdio.h>
#include <string.h>
//#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "bt.h"
//#include "esp_log.h"
//#include "wifi.h"
//#include "lwip/tcpip.h"
#include "hex.h"
#include "uart.h"

#define uart_num (UART_NUM_2)

uint8_t *STARTFRAMEBT = (uint8_t *) "\r\nSTARTFRAMEBT,";
uint8_t *STARTFRAMEWF = (uint8_t *) "\r\nSTARTFRAMEWF,";
unsigned STARTFRAMELEN = 15;

SemaphoreHandle_t mutex;
QueueHandle_t qh;

#define STATE_IDLE    0
#define STATE_STARTED 1
#define STATE_READPKT 2

int pkt_dump(const char *msg, const uint8_t *data, uint16_t len);

void write_frame_to_uart(int is_bt, uint8_t *data, uint16_t len)
{
    char *startframe = is_bt ? ((char *) STARTFRAMEBT) : ((char *) STARTFRAMEWF);
    char numbuf[8];
    int ready;

    pkt_dump("host RX: ", data, len);
    xSemaphoreTake(mutex, 10000);

    uart_write_bytes(uart_num, startframe, STARTFRAMELEN);

    ready = !!esp_vhci_host_check_send_available();
    sprintf(numbuf, "%0d,%04X,", ready, len);
    uart_write_bytes(uart_num, numbuf, 7);

#if 0
    for (i=0; i < len; i++) {
        char minibuf[3];
        sprintf(minibuf, "%02X", data[i]);
        uart_write_bytes(uart_num, minibuf, 2);
    }
#else
    uart_write_bytes(uart_num, (char *) data, len);
#endif

    xSemaphoreGive(mutex);
}

void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };

    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, 17, 16, 5, 18);
    uart_driver_install(uart_num, 2*4096, 8*4096, 0, NULL, 0);
}

uint8_t read_byte_from_uart()
{
    uint8_t ch;
    int len = 0;

    while (len == 0)
        len = uart_read_bytes(uart_num, &ch, 1, 10000 / portTICK_RATE_MS);
    
    return ch;
}

void read_bytes_from_uart(uint8_t *pbuf, unsigned sz)
{
    int i, len;

    for (i=0; i < sz; i += len) {
        len = uart_read_bytes(uart_num, pbuf + i, sz - i, 50 / portTICK_RATE_MS);
        if (len < 0) {
            printf("uart_read_bytes returned error: %d\n", len);
            vTaskDelay(5000 / portTICK_RATE_MS);
            len = 0;
        }
    }
}

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

void to_uart(void *pvParameters)
{
    struct bridge_pkt *p = 0;
    printf("Starting to uart task\n");

    for (;;) {
        if (!xQueueReceive(qh, &p, 1000000)) {
            printf("xQueueReceive timeout\n");
            continue;
        }

        write_frame_to_uart(p->is_bt, p->payload, p->len);
        free(p);
    }
}

int enqueue_to_uart(struct bridge_pkt *p)
{
    if (xQueueSend(qh, &p, 0) != pdTRUE) {
        printf("Failed to send item to queue\n");
        return 1;
    }
    return 0;
}

