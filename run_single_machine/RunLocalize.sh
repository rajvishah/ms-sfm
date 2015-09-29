#!/bin/bash
locDir=$1
scriptPath=$2
binPath=$3
prevDir=$4  # Prev Recons
baseDir=$5  # Coarse recons

rm -rf $locDir; mkdir -p $locDir/log
cd $locDir;

$scriptPath/CreateLocalizationJobs.sh $prevDir/bundle.out $baseDir/list_keys.txt $binPath $locDir
$scriptPath/CreateCameraCorrectionJobs.sh $binPath $prevDir $baseDir $locDir

sleep 1

echo $binPath/merge_cameras $prevDir/bundle.out $locDir/CamParFiles/ $locDir/ >> $locDir/run_localization.sh

numQueries=`cat $locDir/queries.txt | wc -l`
if [ $numQueries -eq 0 ]; then 
    echo "No queries to localize"
    exit
fi

sh run_localization.sh >> $locDir/localization_out.log

