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
    mkdir $prevDir/coverset
    echo $binPath/kcover $prevDir/bundle.out $prevDir/coverset/bundle.out 200 > $locDir/run_own_localization.sh
    bundleDir=$prevDir/coverset
fi

$scriptPath/Create3D_2D_MatchingJobs.sh $bundleDir $baseDir $binPath $locDir $rankDir               

echo $binPath/acg_localizer_with_own_matches $outPath >> $locDir/run_own_localization.sh 
 
$scriptPath/CreateCameraCorrectionJobs.sh $binPath $bundleDir $baseDir $locDir                       
sleep 1
 
echo $binPath/merge_cameras $prevDir/bundle.out $locDir/CamParFiles/ $locDir/ >> $locDir/run_own_localization.sh
 
numQueries=`cat $locDir/queries.txt | wc -l`
if [ $numQueries -eq 0 ]; then
    echo "No queries to localize"
    exit
fi

sh run_own_localization.sh >> $locDir/localization_own_out.log
cat $prevDir/localized_queries.txt $locDir/localized_queries.txt | sort -nk1 > $locDir/localized_queries_final.txt



#cat matcher_3d_2d_log.* | grep "took" | grep -v "Reading nearby keys" | awk '{print $(NF)}' | cut -d's' -f1 | paste -sd+ - | bc
#cat localizer_log.out | grep "Localization took" | awk '{print $(NF)}' | cut -d's' -f1 | paste -sd+ - | bc
#cat camcorrect_log.out | grep "Registration took" | awk '{print $(NF)}' | cut -d's' -f1 | paste -sd+ - | bc
