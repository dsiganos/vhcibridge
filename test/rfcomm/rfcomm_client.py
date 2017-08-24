#!/usr/bin/python

import os
import serial
import time
import subprocess
import bluetooth

bd_addr = "00:16:a4:70:67:e8"

port = 1

number_of_bytes = 1000000
number_of_bytes = 1000
sock=bluetooth.BluetoothSocket(bluetooth.RFCOMM)
#sock.bind(('24:0A:C4:00:3A:DE', port))
#sock.bind(('2C:D0:5A:86:D4:63', port))
sock.bind(('61:E0:9F:7E:DD:EE', port))
sock.connect((bd_addr, port))

print 'connected'
sock.send('0x%08X' %number_of_bytes)

n = 0

total_len = 0
while n < 1000:
    print 'receiving'
    data =  sock.recv(1000)
    total_len = total_len + len(data)
    print data, total_len
    if total_len == number_of_bytes:
        break
    time.sleep(1)
    n += 1
sock.close()
