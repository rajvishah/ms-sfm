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
nImgs=`wc -l $baseDir/list.txt | cut -d' ' -f1`
nNear=`expr $nImgs / 10`
if [ $nNear -le 20 ]; then
    nNear=20
fi

# Create jobs for guided matching and run
$scriptPath/CreateGuidedMatchingJobs.sh $prevDir $baseDir $densDir $imageDir $nNear 200 $binPath $densifyNew

CALL_SBATCH $densDir/guidedmatch_array_jobs.sh
cat $densDir/matches/matches*.txt > matches.txt

# Add verification and timing print here
#rm $densDir/matches_files/matches*.txt


# From matches, create long tracks 
mkdir $densDir/tracks
$scriptPath/CreateTracksJobs.sh $binPath $densDir/matches.txt $densDir/tracks/ $nImgs $prevDir/ $baseDir
CALL_SBATCH $densDir/maketracks_array_jobs.sh

# Merge old and new tracks using connected components
cat $scriptPath/run_merge_tracks_preemble.txt > $densDir/run_merge_tracks.sh
echo $binPath/merge_tracks $prevDir $densDir/tracks/ $densDir >> $densDir/run_merge_tracks.sh
CALL_SBATCH $densDir/run_merge_tracks.sh

# Triangulate final tracks 
$scriptPath/CreateFinalTracksJob.sh $binPath $densDir $prevDir $baseDir
CALL_SBATCH $densDir/finaltracks_job.sh

# Merge old bundle file cameras and new tracks 
nPts=`wc -l $densDir/triangulated-tracks-final.txt | cut -d' ' -f1`
nCamLines=`expr $nImgs \* 5`
nCamLineLast=`expr $nCamLines + 2`

head -1 $prevDir/bundle.out > $densDir/bundle.out   # copy header line
echo $nImgs $nPts >> $densDir/bundle.out            # write numImg numPts
sed -n -e 3,${nCamLineLast}p $prevDir/bundle.out >> $densDir/bundle.out      # write all cameras
cat $densDir/bundle-triangulated_tracks-final.txt >> $densDir/bundle.out    # write all tracks
