#!/usr/bin/python

import os, sys, binascii, time, hexdump, threading, serial

STARTFRAMEBT = '\r\nSTARTFRAMEBT,'
STARTFRAMEWF = '\r\nSTARTFRAMEWF,'
STARTFRAMELEN = len(STARTFRAMEBT)
assert(STARTFRAMELEN == len(STARTFRAMEWF))

def write_pkt_to_controller(ser, pkttuple):
    pkttype, pkt = pkttuple
    hexpkt = binascii.hexlify(pkt)
    if pkttype == PktType.BT:
        frame = '%s%04X,%s' % (STARTFRAMEBT, len(pkt), hexpkt)
    else:
        frame = '%s%04X,%s' % (STARTFRAMEWF, len(pkt), hexpkt)
    hexdump.hexdump(pkt)
    print 'TX: %s' % frame[2:]
    ser.write(frame)

class PktType:
    BT, WIFI = range(2)

def read_pkt_from_controller(ser):
    class State:
        IDLE, STARTED, READPKT = range(3)

    state = State.IDLE
    while True:
        if state == State.IDLE:
            ch = ser.read(1)
            sys.stdout.write(ch)
            if ch == '\r':
                data = '\r' + ser.read(STARTFRAMELEN-1)
                if data == STARTFRAMEBT:
                    state = State.STARTED
                    pkttype = PktType.BT
                if data == STARTFRAMEWF:
                    state = State.STARTED
                    pkttype = PktType.WIFI
        
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
            return (pkttype, pkt)

class HostToEsp32Thread(threading.Thread):
    def __init__(self, src, dst):
        super(HostToEsp32Thread, self).__init__()
        self.running = True
        self.src = src
        self.dst = dst

    def run(self):
        while self.running:
            write_pkt_to_controller(self.dst, self.src.read())
        print 'exited host to controller thread'

class Esp32ToHostThread(threading.Thread):
    def __init__(self, vhci, tap, ser):
        super(Esp32ToHostThread, self).__init__()
        self.running = True
        self.vhci = vhci
        self.tap = tap
        self.ser = ser

    def run(self):
        while self.running:
            (pkttype, pkt) = read_pkt_from_controller(self.ser)
            try:
                if pkttype == PktType.BT:
                    self.vhci.write(pkt)
                if pkttype == PktType.WIFI:
                    self.tap.write(pkt)
            except Exception, e:
                print e
        print 'exited controller to host thread'

print 'ensure kernel module is loaded'
os.system('sudo modprobe hci_vhci; sleep 1; sudo chmod a+rw /dev/vhci')

# create the virtual HCI controller
class VhciNode(object):
    def __init__(self):
        self.vhci_fd = os.open("/dev/vhci", os.O_RDWR)
    def write(self, pkt):
        os.write(self.vhci_fd, pkt)
        print 'Wrote %d VHCI bytes to host' % len(pkt)
        hexdump.hexdump(pkt)
    def read(self):
        data = os.read(self.vhci_fd, 2000)
        print 'Read %d bytes from VHCI host' % len(data)
        return (PktType.BT, data)
vhci_node = VhciNode()

# create the tap device
from pytun import TunTapDevice, IFF_TAP
class TapNode(object):
    def __init__(self):
        os.system('sudo ip tuntap add dev esp32 mode tap user ds')
        self.tap = TunTapDevice(name='esp32', flags=IFF_TAP)
    def write(self, pkt):
        self.tap.write('\x00\x00\x00\x00' + pkt)
        print 'Wrote %d WIFI bytes to host' % len(pkt)
        hexdump.hexdump(pkt)
    def read(self):
        data = self.tap.read(2000)[4:]
        print 'Read %d bytes from TAP interface' % len(data)
        return (PktType.WIFI, data)
tap_node = TapNode()

# open the serial port that connects to the controller
# serdev = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
serdev = '/dev/serial/by-id/usb-FTDI_TTL232R-3V3_FT912Q4M-if00-port0'
ctrl_fd = serial.serial_for_url('spy://%s?file=serial.log' % serdev, timeout=1, baudrate=115200, rtscts=False)
ctrl_fd.flushInput()

btHostToControllerThread = HostToEsp32Thread(vhci_node, ctrl_fd)
btHostToControllerThread.start()

wfHostToEsp32Thread = HostToEsp32Thread(tap_node, ctrl_fd)
wfHostToEsp32Thread.start()

esp32ToHostThread = Esp32ToHostThread(vhci_node, tap_node, ctrl_fd)
esp32ToHostThread.start()

btHostToControllerThread.join()
wfHostToEsp32Thread.join()
esp32ToHostThread.join()

# os.system('hciconfig hci1 reset &')

