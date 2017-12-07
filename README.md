# vhcibridge
VHCI bridge between linux and Espressif ESP32

## To build the project
```
. ../setenv
make clean
make -j
```
## Example setenv file:
```
PATH=$HOME/esp32/toolchain/default/bin:$PATH
export IDF_PATH=$HOME/esp32/esp-idf
export ESPPORT=/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

## To flash the board
```
make flash
```

## To start the bridge
```
# On a terminal
./vhcibridge

# On another terminal
make monitor
```
To exit monitor mode, type Ctrl-].
Remember to change the MAC address of the laptop to match the MAC address of the Wifi module.

## esp-idf change
For the Wifi bridge to work, the following change must be made to esp-idf.
```
diff --git a/components/tcpip_adapter/tcpip_adapter_lwip.c b/components/tcpip_adapter/tcpip_adapter_lwip.c
index b1bb85b..b688d2e 100644
--- a/components/tcpip_adapter/tcpip_adapter_lwip.c
+++ b/components/tcpip_adapter/tcpip_adapter_lwip.c
@@ -943,7 +943,14 @@ esp_err_t tcpip_adapter_eth_input(void *buffer, uint16_t len, void *eb)
 
 esp_err_t tcpip_adapter_sta_input(void *buffer, uint16_t len, void *eb)
 {
+#if 1
+    int pkt_dump(const char *msg, const uint8_t *data, uint16_t len);
+    void write_frame_to_uart(int is_bt, uint8_t *data, uint16_t len);
+    pkt_dump("RX: ", buffer, len);
+    write_frame_to_uart(0, buffer, len);
+//#else
     wlanif_input(esp_netif[TCPIP_ADAPTER_IF_STA], buffer, len, eb);
+#endif
     return ESP_OK;
 }
```

## MAC address spoofing
For the esp32 to operate as a bridge, the MAC address of the wifi module must be spoofed.
```
sudo ip link set esp32 address 24:0a:c4:00:3a:dc
```

## DHCP client
a dhcp client can be easily started like this:
```
sudo dhclient -d esp32
```
