#!/bin/bash
N=3
TRACE_synt="$(pwd -P)/traces/pktgen1B.pcap"
TCPREPLAY=tcpreplay

if [[ $# -eq 1 ]]
then
	for i in $( seq 1 $N )
	do
		echo "Starting tcpreplay $i..."
		$TCPREPLAY -i $1 -t $TRACE_synt &
		PID=$!
	done
	echo "Sleeping on PID $PID..."
	while $(ps -p $PID > /dev/null)
	do
		sleep 1;
	done

else
	echo "Usage: <interface>"
fi

