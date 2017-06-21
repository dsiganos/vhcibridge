hciconfig
– Gives info about the bluetooth hci on your pc
– Ensure the device is up and running and has required scan modes
– hcitool dev should also give some of this info

hcitool inq and hcitool scan
– Gives info about or rather identifies nearby bluetooth devices

hcitool info <BTAddr>
– Get info about remote bluetooth device



hciconfig hci0 sspmode
I have used the above once but I am not sure if it did something significant.

This one as well:
sdptool add SP





HOW TO CONNECT FROM THE LINUX LAPTOP TO THE BT 900

Connect the BT 900 to the pc on a usb port
Findout which port that is and open a Uwterminal
Execute the program $autorun$.SPP.UART.bridge.incoming.sb on the BT900.
#### hcitool scan
Scanning ...
	24:0A:C4:00:3A:DE	n/a
	00:16:A4:70:67:E8	Laird BT900

From the above we can see that the BT900 which we are trying to connect to has a
MAC address of 00:16:A4:70:67:E8. We will use that to connect.


#### sudo rfcomm connect 3 00:16:A4:70:67:E8 1

This is what the command will output in case of success:
Connected /dev/rfcomm3 to 00:16:A4:70:67:E8 on channel 1


This is what will be printed on the BT900 side.

Pair Req: B00594F7A349
Type 'y' to pair, 'n' to decline or 'cancel' to cancel - and press Enter

y


Pairing...
>
 --- Pair: (00000000) B00594F7A349
OK
>
 --- SPP Connect: (00000000)

We need to open a minicom terminal using as a serial port the port that just was
returned:

sudo minicom -D /dev/rfcomm3

The minicom command needs to be issued as root.


HOW TO CONNECT FROM THE MOBILE PHONE APPLICATION TO THE BT900

On the phone open up an application (Blueterm) and choose to connect with the BT900
Once connected, you can transfer data


#### rfcomm -a
Will show a lot of information including which channels are open and which virtual serial ports
are being used 
