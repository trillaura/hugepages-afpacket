#!/bin/bash
if [[ $# -eq 1 ]]
then
	echo "Adding device $1@0..."
	echo rem_device_all > /proc/net/pktgen/kpktgend_0
	echo add_device $1@0 > /proc/net/pktgen/kpktgend_0
	echo "min_pkt_size 700" > /proc/net/pktgen/"$1"@0
	echo "max_pkt_size 1500" > /proc/net/pktgen/"$1"@0
	echo "src_min 172.17.0.1" > /proc/net/pktgen/"$1"@0
	echo "src_max 172.17.255.255" > /proc/net/pktgen/"$1"@0
	echo "dst_mac 24:6e:96:7e:b4:0c" > /proc/net/pktgen/"$1"@0
	echo "src_mac 24:6e:96:02:94:92" > /proc/net/pktgen/"$1"@0
	echo "count 1000000000" > /proc/net/pktgen/"$1"@0

	echo "flag IPSRC_RND" > /proc/net/pktgen/"$1"@0
	echo "flag IPDST_RND" > /proc/net/pktgen/"$1"@0
	echo "flag UDPSRC_RND" > /proc/net/pktgen/"$1"@0
	echo "flag UDPDST_RND" > /proc/net/pktgen/"$1"@0
	echo "flag MACSRC_RND" > /proc/net/pktgen/"$1"@0
	echo "flag MACDST_RND" > /proc/net/pktgen/"$1"@0

	echo "Starting $1@0..."
	echo "start" > /proc/net/pktgen/pgctrl
else
	echo "Usage <interface>"
fi
