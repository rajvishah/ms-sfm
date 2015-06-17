#!/bin/bash
bundleFile=$1
listFile=$2
infoLine=`sed -n -e 2p $bundleFile`
infoArray=($infoLine)
numCameras=${infoArray[0]}
numLinesPerCam=5
numCamLines=`expr $numCameras \* $numLinesPerCam`
numCamLines=`expr $numCamLines + 2`
allLines=`sed -n -e 3,${numCamLines}p $bundleFile | grep -n -e "0 0 0" | cut -d':' -f1 | sed -n '1~5p' | awk '{print ($1 - 1)/5 }'`
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



