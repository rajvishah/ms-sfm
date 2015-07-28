#!/bin/bash
imageDir=$1
runDir=`pwd`
binPath=/home/cvit/rajvi/ms-sfm/ms-sfm/bin/
scriptPath=/home/cvit/rajvi/ms-sfm/ms-sfm/scripts/

#################################################################################
globalMatchDir=$runDir/initial_matching
: '
rm -r $globalMatchDir
mkdir $globalMatchDir
ls $imageDir/*.key > $globalMatchDir/list_keys.txt

numJobs=200
cd $globalMatchDir
$scriptPath/CreateGlobalMatchingJobs.sh $binPath $numJobs $globalMatchDir

sbatch global_matching_job.sh 

while [ 1 ];
do
    numLogFiles=`ls $globalMatchDir/log/*.out | wc -l`
    echo "Written " $numLogFiles
    if [ $numLogFiles -eq $numJobs ];
    then
        break
    fi
    sleep 1
done

while [ 1 ];
do
    numDone=`cat $globalMatchDir/log/*.out | grep "Done matching " | wc -l`
    echo "Done matching " $numDone
    if [ $numDone -eq $numJobs ];then
        break
    fi
    sleep 5
done
'

cgmDir=$runDir/coarse_model

: '
#================================================================================
cat $globalMatchDir/matches/initial-matches*.txt > $globalMatchDir/matches.txt

#################################################################################
rm -r $cgmDir
mkdir $cgmDir

cp $globalMatchDir/matches.txt $cgmDir/matches.init.txt   
cd $cgmDir

mkdir log
cat $scriptPath/run_bundler_preemble.txt > run_bundler_job.sh
echo $scriptPath/RunBundler.sh $imageDir >> run_bundler_job.sh  

sbatch run_bundler_job.sh

while [ ! -e log/run_bundler.out];
do
    sleep 1
done
ls log/*.out
sleep 1

while [ 1 ]
do
    bundlerDone=`cat log/run_bundler.out | grep "Done Bundler" | wc -l`
    echo "Bundler Done " $bundlerDone 
    if [ $bundlerDone -eq 1 ];
    then
        break``
    fi
    sleep 1
done
'
#==================================================================================
cp $cgmDir/list_tmp.txt $cgmDir/list_images.txt 
ls $imageDir/*.key > $cgmDir/list_keys.txt

###################################################################################

locDir=$runDir/loc0
if [ -d $locDir ]; then
#    rm -r $locDir
    sleep 1
fi
mkdir $locDir

cd $locDir
#$scriptPath/CreateLocalizationJobs.sh $cgmDir/bundle/bundle.out $cgmDir/list_keys.txt $binPath $locDir


mkdir log

#sbatch prepare_localization.sh 

while [ ! -f log/prep_localization.out ];
do
    sleep 1
done
ls log/prep_localization.out

while [ 1 ];
do
    infoDone=`cat log/prep_localization.out | grep "done writing info file" | wc -l`
    echo "Info Done " $infoDone 
    if [ $infoDone -eq 1 ];
    then
        cdaDone=`cat log/prep_localization.out | grep "Done computing descriptor" | wc -l`
        echo "CDA Done " $cdaDone 
        if [ $cdaDone -eq 1 ]; then
            break
        fi
    fi
    sleep 1
done


#================================================================================
sbatch localization_array_jobs.sh 
numQueries=`cat queries.txt | wc -l`
echo $numQueries
while [ 1 ];
do
    numLocFiles=`ls log/localization*.out | wc -l`
    echo "Written " $numLocFiles
    if [ $numLocFiles -eq $numQueries ]; then
        break
    fi
    sleep 1
done

while [ 1 ];
do
    numDone=`cat log/localization*.out | grep "Localization took " | wc -l`
    echo "Localization took " $numDone
    if [ $numDone -eq $numQueries ]; then
        break
    fi
    sleep 1
done

: '
#================================================================================

sbatch cam_correction_jobs.sh 

#================================================================================

sbatch merge_cameras_job.sh

###################################################################################

densDir=$runDir/dens0
mkdir $densDir

cd $densDir
$scriptPath/CreateGuidedMatchingJobs.sh
###################################################################################
'
echo Done
