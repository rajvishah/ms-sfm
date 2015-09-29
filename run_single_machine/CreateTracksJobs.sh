#!/bin/bash

binPath=$1
matchesFile=$2
tracksPath=$3
numImages=$4
bundlerPath=$5
listsPath=$6

echo $listsPath
echo $matchesFile

    jobFile=maketracks_array_jobs.sh

numImgs=`expr $numImages - 1`

echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=maketracks" >> $jobFile
echo "#SBATCH --output=log/maketracks_%A_%a.out" >> $jobFile
echo "#SBATCH --error=log/maketracks_%A_%a.err" >> $jobFile
arrayStr="#SBATCH --array=0-"$numImgs
echo $arrayStr  >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH --ntasks-per-core=2" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

    echo $binPath/make_tracks --tracks_dir=$tracksPath --image_idx=\$SLURM_ARRAY_TASK_ID --matches_file=$matchesFile >> $jobFile
    echo $binPath/triangulate_tracks --tracks_dir=$tracksPath --image_idx=\$SLURM_ARRAY_TASK_ID --bundler_path=$bundlerPath --lists_path=$listsPath >> $jobFile 


