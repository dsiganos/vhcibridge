#!/usr/bin/python

import os, sys, binascii, time, hexdump, threading, serial

STARTFRAMEBT = '\r\nSTARTFRAMEBT,'
STARTFRAMEWF = '\r\nSTARTFRAMEWF,'
STARTFRAMELEN = len(STARTFRAMEBT)
assert(STARTFRAMELEN == len(STARTFRAMEWF))

class PktType(object):
    BT, WIFI = range(2)

class HostToEsp32Thread(threading.Thread):
    def __init__(self, src, dst):
        super(HostToEsp32Thread, self).__init__()
        self.running = True
        self.src = src
        self.dst = dst

    def run(self):
        # send an empty packet to get the status of the controller
        print 'sending empty packet'
        self.dst.write_pkt_to_controller((PktType.BT, ''))
        print 'sent empty packet'
        while self.running:
            self.dst.write_pkt_to_controller(self.src.read())
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
            (pkttype, pkt) = bt_ctrl.read_pkt_from_controller()
            if len(pkt) > 0:
                try:
                    if pkttype == PktType.BT:
                        self.vhci.write(pkt)
                    if pkttype == PktType.WIFI:
                        self.tap.write(pkt)
                except Exception, e:
                    print e
        print 'exited controller to host thread'

class BtController(object):
    def __init__(self, ser):
        self.ser = ser
        self.ctrl_ready = False

    def read_pkt_from_controller(self):
        class State:
            IDLE, STARTED, READPKT = range(3)

        ser = self.ser
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
                data = ser.read(7)
                if len(data) != 7 or data[6] != ',' or data[1] != ',':
                    print 'failed to parse pkt len'
                    state == State.IDLE
                    continue
                pktlen = int(data[2:-1], 16)
                self.ctrl_ready = bool(int(data[0]))
                if self.ctrl_ready:
                    print 'controller is ready'
                else:
                    print 'controller is not ready'
                state = State.READPKT

            if state == State.READPKT:
                #data = ser.read(pktlen * 2)
                #pkt = binascii.unhexlify(data)
                pkt = ser.read(pktlen)
                print 'Read pkt from controller, pkttype=%s' % pkttype
                #hexdump.hexdump(pkt)
                return (pkttype, pkt)

    def write_pkt_to_controller(self, pkttuple):
        while True:
            if not self.ctrl_ready:
                time.sleep(0.1)
                continue
            pkttype, pkt = pkttuple
            hexpkt = binascii.hexlify(pkt)
            if pkttype == PktType.BT:
                frame = '%s%04X,%s' % (STARTFRAMEBT, len(pkt), hexpkt)
            else:
                frame = '%s%04X,%s' % (STARTFRAMEWF, len(pkt), hexpkt)
            #hexdump.hexdump(pkt)
            #print 'TX: %s' % frame[2:]
            self.ser.write(frame)
            return

print 'ensure kernel module is loaded'
os.system('sudo modprobe hci_vhci; sleep 1; sudo chmod a+rw /dev/vhci')

# create the virtual HCI controller
class VhciNode(object):
    def __init__(self):
        self.vhci_fd = os.open("/dev/vhci", os.O_RDWR)
    def write(self, pkt):
        os.write(self.vhci_fd, pkt)
        print 'Wrote %d VHCI bytes to host' % len(pkt)
        #hexdump.hexdump(pkt)
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
        time.sleep(1)
        self.tap = TunTapDevice(name='esp32', flags=IFF_TAP)
    def write(self, pkt):
        self.tap.write('\x00\x00\x00\x00' + pkt)
        print 'Wrote %d WIFI bytes to host' % len(pkt)
        #hexdump.hexdump(pkt)
    def read(self):
        data = self.tap.read(2000)[4:]
        print 'Read %d bytes from TAP interface' % len(data)
        return (PktType.WIFI, data)
tap_node = TapNode()

# open the serial port that connects to the controller
serdev = '/dev/serial/by-id/usb-FTDI_UMFT234XD_FTWJO0MJ-if00-port0'
serdev = 'spy://%s?file=serial.log' % serdev
ctrl_fd = serial.serial_for_url(serdev, timeout=1, baudrate=115200, rtscts=True)
ctrl_fd.flushInput()

bt_ctrl = BtController(ctrl_fd)

print 'Creating host to controller thread for bluetooth packets'
btHostToControllerThread = HostToEsp32Thread(vhci_node, bt_ctrl)
btHostToControllerThread.start()

print 'Creating host to controller thread for Wifi packets'
wfHostToEsp32Thread = HostToEsp32Thread(tap_node, bt_ctrl)
wfHostToEsp32Thread.start()

print 'Creating controller to host thread'
esp32ToHostThread = Esp32ToHostThread(vhci_node, tap_node, ctrl_fd)
esp32ToHostThread.start()

btHostToControllerThread.join()
wfHostToEsp32Thread.join()
esp32ToHostThread.join()
