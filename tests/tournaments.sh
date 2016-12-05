#!/bin/bash -e

i=1
while [ 0 ]; do

    echo "*** $i `date`"
    mv tournament-2 tournament-2-last
    ./tournament_test --log_level=message 2> tournament-2
    echo
    if [ "$1" = "-c" ]; then
	sleep 2
    else
        break
    fi
    i=$[i + 1]

done