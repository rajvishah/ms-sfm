#!/bin/bash
imageDir=$1
runDir=`pwd`
binPath=/home/cvit/rajvi/ms-sfm/ms-sfm/bin/
scriptPath=/home/cvit/rajvi/ms-sfm/ms-sfm/scripts/

globalMatchDir=$runDir/initial_matching
cgmDir=$runDir/coarse_model
locDir=$runDir/loc0
densDir=$runDir/dens0
loc1Dir=$runDir/loc1
dens1Dir=$runDir/dens1
loc2Dir=$runDir/loc2
dens2Dir=$runDir/dens2
loc3Dir=$runDir/loc3
dens3Dir=$runDir/dens3

source $scriptPath/SbatchCaller.sh

#################################################################################
: '
rm -rf $globalMatchDir; mkdir $globalMatchDir
ls $imageDir/*.key > $globalMatchDir/list_keys.txt

numJobs=200
cd $globalMatchDir
$scriptPath/CreateGlobalMatchingJobs.sh $binPath $numJobs $globalMatchDir

CALL_SBATCH global_matching_job.sh 

touch global_match_stats.txt
averageTime=0
maxTime=0
numLists=`ls log/global_match_*.out | wc -l`
for file in log/global_match_*.out;
do
    timePerScript=`grep -Po '(?<=Matching took).*(=?s)' $file | paste -sd+ - | bc`
    echo "Time taken by script $file is " $timePerScript " seconds" >> global_match_stats.txt
    averageTime=`expr $timePerScript + $averageTime`
    if [ $timePerScript -ge $maxTime ]; then
        maxTime=$timePerScript
    fi
done
averageTime=`expr $averageTime / $numLists`
echo "-----------------" >> global_match_stats.txt 
echo "Average time by a script is " $averageTime >> global_match_stats.txt 
echo "Max time by a script is " $averageTime >> global_match_stats.txt 



cat log/global_match_*.err > log/combined_global_match.err
cat log/global_match_*.out > log/combined_global_match.out
rm log/global_match_*.err
rm log/global_match_*.out
rm ranks/*
rm pairs/*

grep 
numPairsMatched=`grep "Writing" log/combined_global_match.out | wc -l`
echo "Total connected pairs " $numPairsMatched

cd ..

'

: '
#================================================================================
cat $globalMatchDir/matches/initial-matches*.txt > $globalMatchDir/matches.txt

#################################################################################
rm -rf $cgmDir; mkdir -p $cgmDir/log
cp $globalMatchDir/matches.txt $cgmDir/matches.init.txt   

cd $cgmDir

cat $scriptPath/run_bundler_preemble.txt > run_bundler_job.sh
echo $scriptPath/RunBundler.sh $imageDir >> run_bundler_job.sh  

#=================================================================================

CALL_SBATCH run_bundler_job.sh

cd ..

#==================================================================================
cp $cgmDir/list_tmp.txt $cgmDir/list_images.txt 
ls $imageDir/*.key > $cgmDir/list_keys.txt

###################################################################################

$scriptPath/RunLocalize.sh $locDir $scriptPath $binPath $cgmDir $cgmDir

###################################################################################

$scriptPath/RunDensify.sh $densDir $cgmDir $locDir $imageDir $scriptPath

###################################################################################

$scriptPath/RunLocalize.sh $loc1Dir $scriptPath $binPath $densDir $cgmDir

###################################################################################

$scriptPath/RunDensify.sh $dens1Dir $cgmDir $loc1Dir $imageDir $scriptPath $binPath

###################################################################################

$scriptPath/RunLocalize.sh $loc2Dir $scriptPath $binPath $dens1Dir $cgmDir

###################################################################################

$scriptPath/RunDensify.sh $dens2Dir $cgmDir $loc2Dir $imageDir $scriptPath $binPath

###################################################################################

'
###################################################################################

$scriptPath/RunLocalize.sh $loc3Dir $scriptPath $binPath $dens2Dir $cgmDir

###################################################################################

$scriptPath/RunDensify.sh $dens3Dir $cgmDir $loc3Dir $imageDir $scriptPath $binPath

###################################################################################
echo Done
