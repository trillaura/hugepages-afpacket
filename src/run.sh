#!/bin/bash
CPU=87 # NOTE: same CPU where PERF counters are enabled
CMD="taskset -c ${CPU}"
FILE="$(date '+%Y%m%d_%H%M%S')"

if [[ $# -eq 4 ]]
then
	echo "Running test for all options on interface $1..."
	for i in 5 25 26
	do
		echo "Running test for option $i"
		if [[ $i == 26 ]]
		then 
			sh -c "echo always > /sys/kernel/mm/transparent_hugepage/enabled"
			sleep 1
		else
			sh -c "echo never > /sys/kernel/mm/transparent_hugepage/enabled"
			sleep 1
		fi		

		tmp="$(mktemp)"
		$CMD $(pwd -P)/capture/capture $1 $i $3 $4 2> "${tmp}" &
		PID_s=$!
		sleep 1

		ssh delta00 "$(pwd -P)/replay.sh $2;" 
		sleep 1

		kill -2 $PID_s
		
		# Waiting for file to be completed
		while [[ $(grep "dropped" ${tmp}) == '' ]]
		do
		    	sleep 1
			PID_perf=$(ps -ef | grep -v grep | grep "capture" | awk '{ print $2 }')
			if [ ! -z "${PID_perf}" ] && kill -0 "${PID_perf}" &> /dev/null ; then
				echo "Exit status capture not 0"
				break
			fi
		done
		
		bash compose_file.sh $3 $tmp $4 "/dev/shm/res${FILE}_synt_$4_$3_$i"
		
		rm $tmp
	done
else
	echo "Usage <in interface> <out interface> <delay> <#blocks>"
fi
