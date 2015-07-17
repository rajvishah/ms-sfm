imageDir=$1
runDir=`pwd`
binPath=/home/cvit/rajvi/ms-sfm/ms-sfm/bin/
scriptPath=/home/cvit/rajvi/ms-sfm/ms-sfm/scripts/

#################################################################################
globalMatchDir=$runDir/initial_matching
ls $imageDir/*.key > $globalMatchDir/list_keys.txt

cd $globalMatchDir
$scriptPath/CreateGlobalMatchingJob.sh $binPath 300 $globalMatchDir

sbatch global_matching_job.sh 

#================================================================================
cat $globalMatchDir/matches/initial-matches*.txt > $globalMatchDir/matches.txt

#################################################################################
cgmDir=$runDir/coarse_model
mkdir $cgmDir

cp $globalMatchDir/matches.txt $cgmDir/matches.txt

cd $cgmDir
sbatch run_bundler_job.sh

#==================================================================================
cp $cgmDir/list_tmp.txt $cgmDir/list_images.txt 
cat $cgmDir/list_tmp.txt | tr ".jpg" ".key" > $cgmDir/list_keys.txt

###################################################################################

locDir=$runDir/loc0
mkdir $locDir

cd $locDir
$scriptPath/CreateLocalizationJobs.sh $cgmDir/bundle/bundle.out $cgmDir/list_keys.txt $binPath $locDir 

sbatch localization_array_jobs.sh 

#================================================================================

sbatch cam_correction_jobs.sh 

#================================================================================

sbatch merge_cameras_job.sh

###################################################################################

densDir=$runDir/dens0
mkdir $densDir

cd $densDir
$scriptPath/CreateGuidedMatchingJobs.sh

