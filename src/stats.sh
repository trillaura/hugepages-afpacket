
for b in 8 16 32 64
do
	for d in 750 1000 1250 1500
	do
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

