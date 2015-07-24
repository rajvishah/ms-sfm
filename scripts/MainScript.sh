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

source $scriptPath/SbatchCaller.sh

#################################################################################
: '
rm -rf $globalMatchDir; mkdir $globalMatchDir
ls $imageDir/*.key > $globalMatchDir/list_keys.txt

numJobs=200
cd $globalMatchDir
$scriptPath/CreateGlobalMatchingJobs.sh $binPath $numJobs $globalMatchDir

CALL_SBATCH global_matching_job.sh 

cd ..

'
cgmDir=$runDir/coarse_model

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

rm -rf $locDir; mkdir -p $locDir/log
cd $locDir; 

$scriptPath/CreateLocalizationJobs.sh $cgmDir/bundle/bundle.out $cgmDir/list_keys.txt $binPath $locDir
$scriptPath/CreateCameraCorrectionJobs.sh $binPath $cgmDir $cgmDir $locDir
cat $scriptPath/run_merge_camera_preemble.txt > run_merge_camera.sh
echo $binPath/merge_cameras $cgmDir/bundle.out $locDir/CamParFiles/ $locDir/bundle.out >> run_merge_camera.sh 

#================================================================================

#CALL_SBATCH prepare_localization.sh 

#================================================================================

#CALL_SBATCH localization_array_jobs.sh 

#================================================================================

#CALL_SBATCH cam_correction_jobs.sh 

#================================================================================

CALL_SBATCH run_merge_camera.sh

#================================================================================

'


###################################################################################

densDir=$runDir/dens0
rm -rf $densDir; mkdir -p $densDir/log

cd $densDir
$scriptPath/CreateGuidedMatchingJobs.sh
###################################################################################

: '
'
echo Done
