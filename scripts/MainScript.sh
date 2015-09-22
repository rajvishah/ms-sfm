#!/bin/bash
imageDir=$1
runDir=`pwd`
binPath=/home/cvit/rajvi/ms-sfm/ms-sfm/bin/
scriptPath=/home/cvit/rajvi/ms-sfm/ms-sfm/scripts/

globalMatchDir=$runDir/initial_matching
cgmDir=$runDir/coarse_model
locDir=$runDir/loc0
locOwnDir=$runDir/loc0own
densDir=$runDir/dens0
loc1Dir=$runDir/loc1
loc1OwnDir=$runDir/loc1own
dens1Dir=$runDir/dens1
loc2Dir=$runDir/loc2
dens2Dir=$runDir/dens2
loc3Dir=$runDir/loc3
dens3Dir=$runDir/dens3

source $scriptPath/SbatchCaller.sh
source $scriptPath/CleanMatchingDump.sh
#source $scriptPath/CleanTrackDump.sh

#################################################################################
: '
rm -rf $globalMatchDir; mkdir $globalMatchDir
ls $imageDir/*.key > $globalMatchDir/list_keys.txt

numJobs=200
cd $globalMatchDir
$scriptPath/CreateGlobalMatchingJobs.sh $binPath $numJobs $globalMatchDir

CALL_SBATCH global_matching_job.sh 
CLEAN_DUMP $globalMatchDir "global_match"
if [ $? -eq 0 ]; then PS4=':${LINENO}+'; set -x; exit; fi

#################################################################################
rm -rf $cgmDir; mkdir -p $cgmDir/log
cp $globalMatchDir/matches.txt $cgmDir/matches.init.txt   

cd $cgmDir
cat $scriptPath/run_bundler_preemble.txt > run_bundler_job.sh
echo $scriptPath/RunBundler.sh $imageDir >> run_bundler_job.sh  

CALL_SBATCH run_bundler_job.sh
cp $cgmDir/list_tmp.txt $cgmDir/list_images.txt 
ls $imageDir/*.key > $cgmDir/list_keys.txt
cp $cgmDir/bundle/bundle.out $cgmDir/bundle.out

'
###################################################################################

$scriptPath/RunLocalize.sh $locDir $scriptPath $binPath $cgmDir $cgmDir
$scriptPath/RunLocalizeOwn.sh $locOwnDir $scriptPath $binPath $locDir $cgmDir $globalMatchDir

###################################################################################

$scriptPath/RunDensify.sh $densDir $cgmDir $locOwnDir $imageDir $scriptPath $binPath 0
echo "Cleaning match dump"
CLEAN_DUMP $densDir "guidedmatch"
echo "Cleaning track dump"

###################################################################################

$scriptPath/RunLocalize.sh $loc1Dir $scriptPath $binPath $densDir $cgmDir
$scriptPath/RunLocalizeOwn.sh $loc1OwnDir $scriptPath $binPath $loc1Dir $cgmDir $globalMatchDir

###################################################################################

numLoc=`cat $loc1ownDir/localized_queries_final.txt | wc -l`
if [ $numLoc -eq 0 ]; then
    exit
fi
$scriptPath/RunDensify.sh $dens1Dir $cgmDir $loc1OwnDir $imageDir $scriptPath $binPath 1
echo "Cleaning match dump"
CLEAN_DUMP $dens1Dir "guidedmatch"
echo "Cleaning track dump"

:'
###################################################################################

$scriptPath/RunLocalize.sh $loc2Dir $scriptPath $binPath $dens1Dir $cgmDir

###################################################################################

$scriptPath/RunDensify.sh $dens2Dir $cgmDir $loc2Dir $imageDir $scriptPath $binPath

###################################################################################

$scriptPath/RunLocalize.sh $loc3Dir $scriptPath $binPath $dens2Dir $cgmDir

###################################################################################

$scriptPath/RunDensify.sh $dens3Dir $cgmDir $loc3Dir $imageDir $scriptPath $binPath

###################################################################################
'
echo Done
