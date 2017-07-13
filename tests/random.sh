#!/bin/bash -e

i=1
while [ 0 ]; do

    echo "*** $i `date`"
    if [ -f random-2 ]; then
        mv random-2 random-2-last
    fi
    ./random_test --log_level=message 2> random-2
    echo "*** $i `date`"
    echo
    if [ "$1" = "-c" ]; then
	sleep 2
    else
        break
    fi
    i=$[i + 1]

done