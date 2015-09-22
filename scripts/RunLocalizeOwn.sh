#!/bin/bash
locDir=$1
scriptPath=$2
binPath=$3
prevDir=$4  # Prev Recons
baseDir=$5  # Coarse recons
rankDir=$6 #initial match
source $scriptPath/SbatchCaller.sh                                                                 
 
rm -rf $locDir; mkdir -p $locDir/log                                                               
cd $locDir;

$scriptPath/Create3D_2D_MatchingJobs.sh $prevDir/ $baseDir $binPath $locDir $rankDir               
 
$scriptPath/CreatePoseEstimationJobs.sh $prevDir/bundle.out $baseDir/list_keys.txt $binPath $locDir
 
$scriptPath/CreateCameraCorrectionJobs.sh $binPath $prevDir $baseDir $locDir                       
 
sleep 1
 
cat $scriptPath/run_merge_camera_preemble.txt > $locDir/run_merge_camera.sh
 
echo $binPath/merge_cameras $prevDir/bundle.out $locDir/CamParFiles/ $locDir/ >> run_merge_camera.sh
 
CALL_SBATCH match_3d_2d_array_jobs.sh
 
CALL_SBATCH localization_array_jobs.sh 
 
CALL_SBATCH camcorrect_array_jobs.sh
 
CALL_SBATCH run_merge_camera.sh
 
cat $locDir/log/match_3d_*.err > $locDir/matcher_3d_2d_err.err
cat $locDir/log/camcorrect_*.err > $locDir/camcorrect_.err
cat $locDir/log/localization_*.err > $locDir/localizer_*.err
cat $locDir/log/match_3d_*.out > $locDir/matcher_3d_2d_err.out
cat $locDir/log/camcorrect_*.out > $locDir/camcorrect_*.out
cat $locDir/log/localization_*.out > $locDir/localizer_*.out
#rm -r $locDir/log

