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
sleep 1
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


###################################################################################

densDir=$runDir/dens0
rm -rf $densDir; mkdir -p $densDir/log
cd $densDir


nImgs=`wc -l $cgmDir/list.txt | cut -d' ' -f1`
nNear=`expr $nImgs / 10`
if [ $nNear -le 20 ]; then
    nNear=20
fi
$scriptPath/CreateGuidedMatchingJobs.sh $locDir $cgmDir $densDir $imageDir $nNear 200 $binPath 
sleep 5

CALL_SBATCH $densDir/guidedmatch_array_jobs.sh

cat $densDir/matches_files/matches*.txt > matches.txt

'
: '
cd $densDir
rm log/*
rm -rf tracks

nImgs=`wc -l $cgmDir/list.txt | cut -d' ' -f1`
mkdir $densDir/tracks
$scriptPath/CreateTracksJobs.sh $binPath $densDir/matches.txt $densDir/tracks/ $nImgs $locDir/ $cgmDir

CALL_SBATCH $densDir/maketracks_array_jobs.sh
cd $densDir
cat $scriptPath/run_merge_tracks_preemble.txt > $densDir/run_merge_tracks.sh
echo $binPath/merge_tracks $locDir $densDir/tracks/ $densDir >> $densDir/run_merge_tracks.sh

CALL_SBATCH $densDir/run_merge_tracks.sh
$scriptPath/CreateFinalTracksJob.sh $binPath $densDir $locDir $cgmDir
CALL_SBATCH $densDir/finaltracks_job.sh

nImgs=`wc -l $cgmDir/list.txt | cut -d' ' -f1`
nPtLines=`wc -l $densDir/triangulated-tracks-final.txt | cut -d' ' -f1`
nPts=$nPtLines
echo $nImgs $nPts 
head -1 $locDir/bundle.out > $densDir/bundle.out
echo $nImgs $nPts >> $densDir/bundle.out
nCamLines=`expr $nImgs \* 5`
nCamLineLast=`expr $nCamLines + 2`
echo $nCamLines $nCamLineLast
sed -n -e 3,${nCamLineLast}p $locDir/bundle.out >> $densDir/bundle.out
cat $densDir/bundle-triangulated_tracks-final.txt >> $densDir/bundle.out

'
: '
###################################################################################
'

rm -rf $loc1Dir; mkdir $loc1Dir
$scriptPath/RunLocalize.sh $loc1Dir $scriptPath $binPath $densDir $cgmDir

echo Done
