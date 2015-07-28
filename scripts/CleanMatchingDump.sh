#!/bin/bash
CLEAN_DUMP() {

    baseDir=$1
    prefix=$2

    rm -f time_acc
    rm -f pair_acc

    statFile=$baseDir"/"$prefix"_stats.txt"
    logFileRegex=$baseDir"/log/"$prefix"_*.out"
    for file in $logFileRegex;
    do
        timePerScr=`grep -Po '(?<=Matching took).*(?=s)' $file | paste -sd+ - | bc`
        pairsPerScr=`grep "Writing" $file | wc -l`

        echo $timePerScr >> time_acc
        echo $pairsPerScr >> pair_acc

        echo "Script " $file " took " $timePerScr " secs; found matches for " $pairsPerScr " pairs" >> $statFile
    done

    numFiles=`ls $logFileRegex | wc -l`
    totalTime=`cat time_acc | paste -sd+ - | bc`
    averageTime=$(echo "scale=2; $totalTime / $numFiles" | bc)
    maxTime=`awk  'BEGIN{max=0}{if(($1)>max)  max=($1)}END {print max}' time_acc`

    echo "---------------------------" >> $statFile
    echo "Total time to match " $totalTime " secs" >> $statFile
    echo "Average time per script " $averageTime "secs" >> $statFile
    echo "Max time per script " $maxTime " secs" >> $statFile
}
