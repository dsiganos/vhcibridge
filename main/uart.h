#ifndef __UART_H__
#define __UART_H__

#define PKT_TYPE_BT   1
#define PKT_TYPE_WIFI 2

struct bridge_pkt {
    int       is_bt;
    void     *payload;
    unsigned  len;
};

void uart_init();
uint8_t read_byte_from_uart();
void read_bytes_from_uart(uint8_t *pbuf, unsigned sz);
unsigned read_pkt_from_uart(uint8_t *pktbuf, int *p_pkttype);
void write_frame_to_uart(int is_bt, uint8_t *data, uint16_t len);
int enqueue_to_uart(struct bridge_pkt *p);

#endif
