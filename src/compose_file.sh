#!/bin/bash
# 1: delay 2: tmp file 3: blocknum 4: dest file
echo $1 >> $4
grep "page-faults" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "dTLB-loads" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "dTLB-load-misses" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "dTLB-stores" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "dTLB-store-misses" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "cycles" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "instructions" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "cache-misses" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "cache-references" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "elapsed time" "$2" | awk ' {print $1} ' | sed "s/,//g" >> $4
grep "upackets" "$2" | awk ' {print $1} ' >> $4
grep "kpackets" "$2" | awk ' {print $1} ' >> $4
grep "bytes" "$2" | awk ' {print $1} ' >> $4
grep "dropped" "$2" | awk ' {print $1} ' >> $4
grep "maxcpucycles" "$2" | awk ' {print $1} ' >> $4
grep "avgcpucycles" "$2" | awk ' {print $1} ' >> $4
echo $3 >> $4
