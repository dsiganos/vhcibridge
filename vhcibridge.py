#!/usr/bin/python

import os, sys, binascii, time, hexdump, threading, serial

#class State:
#    def __init__(self):
#        self.set(State.IDLE)
#
#    def set(self, st):
#        self.i = 0
#        self.state = st
#
#    def tick(self.ch):
#        if self.state == State.IDLE
#
#    IDLE, STARTED, READPKT = range(3)

def read_pkt_from_host(fd):
    data = os.read(fd, 2000)
    print 'Read %d bytes from host' % len(data)
    return data

def write_pkt_to_host(fd, pkt):
    data = os.write(fd, pkt)
    print 'Wrote %d bytes to host' % len(pkt)
    hexdump.hexdump(pkt)

def write_pkt_to_controller(ser, pkt):
    hexpkt = binascii.hexlify(pkt)
    frame = '\r\nSTARTFRAME,%04X,%s' % (len(pkt), hexpkt)
    hexdump.hexdump(pkt)
    print 'TX: %s' % frame[2:]
    ser.write(frame)

def read_pkt_from_controller(ser):
    class State:
        IDLE, STARTED, READPKT = range(3)

    startframe = '\r\nSTARTFRAME,'
    state = State.IDLE
    pktlen = 0

    while True:
        if state == State.IDLE:
            ch = ser.read(1)
            sys.stdout.write(ch)
            if ch == '\r':
                data = '\r' + ser.read(len(startframe)-1)
                if data == startframe:
                    state = State.STARTED
        
        if state == State.STARTED:
            data = ser.read(5)
            if len(data) != 5 or data[4] != ',':
                print 'failed to parse pkt len'
                state == State.IDLE
                continue
            pktlen = int(data[0:-1], 16)
            state = State.READPKT

        if state == State.READPKT:
            data = ser.read(pktlen * 2)
            pkt = binascii.unhexlify(data)
            print 'Read pkt from controller'
            hexdump.hexdump(pkt)
            return pkt

class HostToControllerThread(threading.Thread):
    def __init__(self, vhci, ser):
        super(HostToControllerThread, self).__init__()
        self.running = True
        self.vhci = vhci
        self.ser = ser

    def run(self):
        while self.running:
            pkt = read_pkt_from_host(self.vhci)
            write_pkt_to_controller(self.ser, pkt)
        print 'exited host to controller thread'

class ControllerToHostThread(threading.Thread):
    def __init__(self, vhci, ser):
        super(ControllerToHostThread, self).__init__()
        self.running = True
        self.vhci = vhci
        self.ser = ser

    def run(self):
        while self.running:
            pkt = read_pkt_from_controller(self.ser)
            try:
                write_pkt_to_host(self.vhci, pkt)
            except Exception, e:
                print e
        print 'exited controller to host thread'

print 'ensure kernel module is loaded'
os.system('sudo modprobe hci_vhci; sleep 1; sudo chmod a+rw /dev/vhci')

# create the virtual HCI controller
vhci_fd = os.open("/dev/vhci", os.O_RDWR)

# open the serial port that connects to the controller
# serdev = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
serdev = '/dev/serial/by-id/usb-FTDI_TTL232R-3V3_FT912Q4M-if00-port0'
ctrl_fd = serial.serial_for_url('spy://%s?file=serial.log' % serdev, timeout=1, baudrate=115200, rtscts=False)

hostToControllerThread = HostToControllerThread(vhci_fd, ctrl_fd)
hostToControllerThread.start()

controllerToHostThread = ControllerToHostThread(vhci_fd, ctrl_fd)
controllerToHostThread.start()

hostToControllerThread.join()
controllerToHostThread.join()

# os.system('hciconfig hci1 reset &')

