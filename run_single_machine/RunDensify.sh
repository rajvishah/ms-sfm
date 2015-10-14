#!/bin/bash
densDir=$1
baseDir=$2
prevDir=$3
imageDir=$4
scriptPath=$5
binPath=$6
densifyNew=$7


source $scriptPath/SbatchCaller.sh

# Remove old directories and create fresh dir
rm -rf $densDir; mkdir -p $densDir/log
cd $densDir

# If num nearby images is too few restrict to 20
nImgs=`wc -l $baseDir/list_keys.txt | cut -d' ' -f1`
nNear=`expr $nImgs / 10`
if [ $nNear -le 20 ]; then
    nNear=20
fi

# Create jobs for guided matching and run
$scriptPath/CreateGuidedMatchingJobs.sh $prevDir $baseDir $densDir $imageDir $nNear 1 $binPath $densifyNew
sh $densDir/run_guidedmatch.sh

#$densDir/run_guidedmatch.sh
mv $densDir/matches/matches.txt $densDir/matches.txt

# Add verification and timing print here
#rm $densDir/matches_files/matches*.txt


# From matches, create long tracks 
mkdir $densDir/tracks
echo $binPath/find_and_triangulate_tracks --input_path=$densDir/ --output_path=$densDir/tracks/ --bundler_path=$prevDir --lists_path=$baseDir --num_images=$nImgs > $densDir/run_trackjob.sh
echo $binPath/merge_tracks $prevDir $densDir/tracks/ $densDir >> $densDir/run_trackjob.sh
echo $binPath/triangulate_tracks --tracks_dir=$densDir/tracks/ --image_idx=0 --bundler_path=$prevDir --lists_path=$baseDir --mode=merged >> $densDir/run_trackjob.sh

sh $densDir/run_trackjob.sh

# Merge old bundle file cameras and new tracks 
nPts=`wc -l $densDir/triangulated-tracks-final.txt | cut -d' ' -f1`
nCamLines=`expr $nImgs \* 5`
nCamLineLast=`expr $nCamLines + 2`

head -1 $prevDir/bundle.out > $densDir/bundle.out   # copy header line
echo $nImgs $nPts >> $densDir/bundle.out            # write numImg numPts
sed -n -e 3,${nCamLineLast}p $prevDir/bundle.out >> $densDir/bundle.out      # write all cameras
cat $densDir/bundle-triangulated_tracks-final.txt >> $densDir/bundle.out    # write all tracks
