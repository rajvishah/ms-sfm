#!/bin/bash
locDir=$1
scriptPath=$2
binPath=$3
prevDir=$4  # Prev Recons
baseDir=$5  # Coarse recons
source $scriptPath/SbatchCaller.sh

rm -rf $locDir; mkdir -p $locDir/log
cd $locDir;

$scriptPath/CreateLocalizationJobs.sh $prevDir/bundle.out $baseDir/list_keys.txt $binPath $locDir

$scriptPath/CreateCameraCorrectionJobs.sh $binPath $prevDir $baseDir $locDir

sleep 1

cat $scriptPath/run_merge_camera_preemble.txt > $locDir/run_merge_camera.sh

echo $binPath/merge_cameras $prevDir/bundle.out $locDir/CamParFiles/ $locDir/ >> run_merge_camera.sh

CALL_SBATCH prepare_localization.sh 

CALL_SBATCH localization_array_jobs.sh 

CALL_SBATCH camcorrect_array_jobs.sh 

CALL_SBATCH run_merge_camera.sh
