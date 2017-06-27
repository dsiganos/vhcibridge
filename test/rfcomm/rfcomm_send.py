#!/usr/bin/python
import os
import time
import bluetooth

ss = bluetooth.BluetoothSocket(bluetooth.RFCOMM)

port = 1
ss.bind(("", port))
ss.listen(1)

cs, addr = ss.accept()
print "Accepted connection from ", addr

cs.send('x' * 20000)

cs.close()
ss.close()
