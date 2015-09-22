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

#Check how dense is the bundle file
infoLine=`sed -n -e 2p $prevDir/bundle.out`
infoArray=($infoLine)
numPoints=${infoArray[1]}

bundleDir=$prevDir
if [ $numPoints -gt 100000 ]; then
    cat $scriptPath/run_coverset_preemble.txt > $locDir/run_coverset.sh
    mkdir $prevDir/coverset
    echo $binPath/kcover $prevDir/bundle.out $prevDir/coverset/bundle.out 200 >> $locDir/run_coverset.sh
    bundleDir=$prevDir/coverset
    CALL_SBATCH run_coverset.sh
fi

$scriptPath/Create3D_2D_MatchingJobs.sh $bundleDir $baseDir $binPath $locDir $rankDir               

$scriptPath/CreatePoseEstimationJobs.sh $binPath $locDir
 
$scriptPath/CreateCameraCorrectionJobs.sh $binPath $bundleDir $baseDir $locDir                       
sleep 1
 
cat $scriptPath/run_merge_camera_preemble.txt > $locDir/run_merge_camera.sh
 
echo $binPath/merge_cameras $prevDir/bundle.out $locDir/CamParFiles/ $locDir/ >> run_merge_camera.sh
 
numQueries=`cat $locDir/queries.txt | wc -l`
if [ $numQueries -eq 0 ]; then
    echo "No queries to localize"
    cp $prevDir/bundle.out $locDir/bundle.out
    exit
fi

CALL_SBATCH match_3d_2d_array_jobs.sh
 
CALL_SBATCH localization_array_jobs.sh 
 
CALL_SBATCH camcorrect_array_jobs.sh
 
CALL_SBATCH run_merge_camera.sh

cat $prevDir/localized_queries.txt $locDir/localized_queries.txt | sort -nk1 > $locDir/localized_queries_final.txt


cat $locDir/log/match_3d_*.err > $locDir/matcher_3d_2d_log.err
cat $locDir/log/camcorrect_*.err > $locDir/camcorrect_log.err
cat $locDir/log/localization_*.err > $locDir/localizer_log.err
cat $locDir/log/match_3d_*.out > $locDir/matcher_3d_2d_log.out
cat $locDir/log/camcorrect_*.out > $locDir/camcorrect_log.out
cat $locDir/log/localization_*.out > $locDir/localizer_log.out

rm $locDir/log/camcorrect*.err
rm $locDir/log/camcorrect*.out
rm $locDir/log/localization*.err
rm $locDir/log/localization*.out
rm $locDir/log/match_3d_*.err
rm $locDir/log/match_3d_*.out

#cat matcher_3d_2d_log.* | grep "took" | grep -v "Reading nearby keys" | awk '{print $(NF)}' | cut -d's' -f1 | paste -sd+ - | bc
#cat localizer_log.out | grep "Localization took" | awk '{print $(NF)}' | cut -d's' -f1 | paste -sd+ - | bc
#cat camcorrect_log.out | grep "Registration took" | awk '{print $(NF)}' | cut -d's' -f1 | paste -sd+ - | bc
