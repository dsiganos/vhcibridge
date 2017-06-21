import os
import serial
import time
import subprocess
import bluetooth




bd_addr = "00:16:a4:70:67:e8"

port = 1

# os.system("hcitool scan")
# subprocess.Popen("sudo rfcomm connect 24 00:16:A4:70:67:E8 1", shell=True, stdout=subprocess.PIPE)

#time.sleep(7)
#dev = "/dev/rfcomm24"
#ser = serial.Serial(dev, 115200)
#time.sleep(1)
#ser.flush()
#ser.write("hello my name is george and this is some garbage\n")
#time.sleep(2)
#s = ser.read(10000)
#print " I got a response from the serial I am going to print in the next line"
#print s
#time.sleep(2)
#ser.close()


number_of_bytes = 10000
sock=bluetooth.BluetoothSocket( bluetooth.RFCOMM )
sock.connect((bd_addr, port))

sock.send('0x%08X' %number_of_bytes)


n = 0

total_len = 0
while n < 1000:
    data =  sock.recv(1000)
    total_len = total_len + len(data)
    print data, total_len
    if total_len == number_of_bytes:
        break
    time.sleep(1)
    n += 1
sock.close()
