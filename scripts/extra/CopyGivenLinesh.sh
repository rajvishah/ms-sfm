#!/bin/bash

#This script takes as an input a file with list of line numbers and outputs those lines from another file

lineNumFile=$1
listFile=$2

allLines=`cat $lineNumFile`
lineNums=($allLines)
numQueries=${#lineNums[@]}

counter1=0
counter2=0

constantOne=1
while read line
do
    if [ $counter2 -lt $numQueries ]; then
        b=${lineNums[$counter2]}
        if [ $counter1 == $b ];
        then
            echo $b >> ids.txt
            echo $line
            counter2=`expr $counter2 + $constantOne`
        fi
        counter1=`expr $counter1 + $constantOne`
    fi
#    echo $counter1 $counter2 $b $numQueries
done < $listFile



