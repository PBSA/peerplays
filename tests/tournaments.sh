#!/bin/bash -e

i=1
file='tournament-2'
file2=$file
while [ 0 ]; do

    echo "*** $i `date`"
    if [ "$1" = "-c" ]; then
	file2=$file-`date +%Y-%m-%d:%H:%M:%S`

    elif [ -f $file2 ]; then
        mv $file2 $file2-last
    fi
    ./tournament_test --log_level=message 2> $file2
    echo
    if [ "$1" = "-c" ]; then
	sleep 2
    else
        break
    fi
    i=$[i + 1]

done