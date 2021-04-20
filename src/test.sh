#!/bin/bash

CPU=$(lscpu | grep "CPU(s):" | awk '{ print $2 }')
((CPU--))

if [[ $# -eq 3 ]]
then
	echo "Disabling offloading features..."
	ethtool -K $1 lro off gro off tso off tx off rx off gso off sg off ufo off
	ethtool -A $1 rx off
	ethtool -A $1 tx off
	ethtool -A $1 autoneg off
	sleep 1

	echo "Configuring CPU frequency scaling to performance..."
	for c in $(seq 0 $CPU)
	do
		sh -c "echo performance > /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor"
	done
	
	echo "Configuring number of preallocated hugepages..."
	HUGEPGS=128
	sh -c "echo ${HUGEPGS} > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages"
	sleep 1

	for d in 750 1000 1250 1500
	do
		echo "Set delay factor to $d"
		for b in 8 16 32 64
		do
			for i in $( seq 1 $3 )
			do
				echo "Run test $i..."
				./run.sh $1 $2 $d $b
			done

			echo "Computing statistics on hash computation with delay factor $d..."
			for j in 5 25 26
			do
				RES=/dev/shm/res*_synt_${b}_${d}_${j}
				FILES=$(ls $RES 2> /dev/null)
				NUM_FILES=$(ls -l $RES | grep ^- | wc -l)
				if [[ $NUM_FILES -eq 0 ]]
				then
					echo "No files found for option $j"
				else
					tmp_list=$(mktemp) 
					for f in $FILES
					do
						echo $f >> $tmp_list
					done	
					./stats/stats $tmp_list $NUM_FILES
					rm $tmp_list
				fi
			done
		done
	done
else
	echo "Usage: <in interface> <out interface> <#runs>"
fi
