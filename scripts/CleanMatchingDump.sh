#!/bin/bash
CLEAN_DUMP() {

    baseDir=$1
    prefix=$2

    retVal=1

    rm -f time_acc
    rm -f pair_acc

    statFile=$baseDir"/"$prefix"_stats.txt"
    logFileRegex=$baseDir"/log/"$prefix"_*.out"
    logErrFileRegex=$baseDir"/log/"$prefix"_*.err"

    globalErrFileTest=$baseDir/"log/test_"$prefix".err"
    globalErrFile=$baseDir/"log/combined_"$prefix".err"
    awk '!/real/ && !/sys/ && !/user/ && /./' $logErrFileRegex > $globalErrFileTest
    if [ -s $globalErrFileTest ]; then
        echo "Error files non-empty"
        retVal=0
        return $retVal
    fi
    rm $globalErrFileTest
    globalOutFile=$baseDir/"log/combined_"$prefix".out"
    cat $logFileRegex > $globalOutFile
    cat $logErrFileRegex > $globalErrFile

    for file in $logFileRegex;
    do
        timePerScr=`grep -Po '(?<=Matching took).*(?=s )' $file | paste -sd+ - | bc`
        pairsPerScr=`grep "Writing" $file | wc -l`

        echo $timePerScr >> time_acc
        echo $pairsPerScr >> pair_acc

        echo "Script " $file " took " $timePerScr " secs; found matches for " $pairsPerScr " pairs" >> $statFile
    done

    numFiles=`ls $logFileRegex | wc -l`
    totalPairs=`cat pair_acc | paste -sd+ - | bc`
    totalTime=`cat time_acc | paste -sd+ - | bc`
    averageTime=$(echo "scale=2; $totalTime / $numFiles" | bc)
    maxTime=`awk  'BEGIN{max=0}{if(($1)>max)  max=($1)}END {print max}' time_acc`

    echo "---------------------------" >> $statFile
    echo "Total time to match " $totalTime " secs" >> $statFile
    echo "Average time per script " $averageTime "secs" >> $statFile
    echo "Max time per script " $maxTime " secs" >> $statFile
    echo "Total pairs matched " $totalPairs >> $statFile 
    
    rm -f time_acc
    rm -f pair_acc

    #rm $logFileRegex
    #rm $logErrFileRegex

    currDir=`pwd`
    if [ -d $baseDir/matches ]; then
        cd $baseDir/matches/
        echo "Writing Matches"
        cat *matches*.txt > $baseDir/matches.txt
        cd ..
        #rm -r matches
    fi

    cd $currDir
    if [ -d $baseDir/pairs ]; then
        cd $baseDir/pairs/
        cat *pairs*.txt > $baseDir/all_pairs.txt
        cd ..
        #rm -r pairs
    fi
    
    cd $currDir
    if [ -d $baseDir/ranks ]; then
        cd $baseDir/ranks/
        cat *ranks*.txt > $baseDir/all_ranks.txt
        cd ..
        #rm -r ranks
    fi
   
    echo "Cleaned " $prefix " dump"
    return $retVal
}
