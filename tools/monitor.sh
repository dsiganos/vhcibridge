#!/bin/bash
set -x

if [ $# -ne 2 ]; then
	echo "$0 <wifi-interface> <channel>"
	exit 1;
fi

if=$1
ch=$2

sudo iwconfig $if txpower on
sudo ip link set $if down
sudo iwconfig $if mode Monitor
sudo ip link set $if up
sudo iwconfig $if channel $ch
sudo iwconfig $if
