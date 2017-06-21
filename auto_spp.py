import os
import serial
import time
import subprocess




# os.system("hcitool scan")

subprocess.Popen("sudo rfcomm connect 24 00:16:A4:70:67:E8 1", shell=True, stdout=subprocess.PIPE)
time.sleep(7)
dev = "/dev/rfcomm24"
ser = serial.Serial(dev, 115200)
time.sleep(1)
ser.flush()
ser.write("hello my name is george and this is some garbage\n")
time.sleep(2)
s = ser.read(10000)
print " I got a response from the serial I am going to print in the next line"
print s
time.sleep(2)
ser.close()
