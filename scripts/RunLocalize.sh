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

numQueries=`cat $locDir/queries.txt | wc -l`
if [ $numQueries -eq 0 ]; then 
    echo "No queries to localize"
    cp $prevDir/bundle.out $locDir/bundle.out
    exit
fi

CALL_SBATCH prepare_localization.sh 

CALL_SBATCH localization_array_jobs.sh 

CALL_SBATCH camcorrect_array_jobs.sh 

CALL_SBATCH run_merge_camera.sh

cat $locDir/log/camcorrect_*.err > $locDir/camcorrect_log.err
cat $locDir/log/localization_*.err > $locDir/localizer_log.err
cat $locDir/log/camcorrect_*.out > $locDir/camcorrect_log.out
cat $locDir/log/localization_*.out > $locDir/localizer_log.out

rm $locDir/log/camcorrect*.err
rm $locDir/log/camcorrect*.out
rm $locDir/log/localization*.err
rm $locDir/log/localization*.out
