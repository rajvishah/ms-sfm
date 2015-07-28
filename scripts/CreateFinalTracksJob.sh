#!/bin/bash

binPath=$1
tracksPath=$2
bundlerPath=$3
listsPath=$4
jobFile=finaltracks_job.sh


echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=finaltracks" >> $jobFile
echo "#SBATCH --output=log/finaltracks.out" >> $jobFile
echo "#SBATCH --error=log/finaltracks.err" >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH -n 24" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

echo $binPath/triangulate_tracks --tracks_dir=$tracksPath --image_idx=0 --bundler_path=$bundlerPath --lists_path=$listsPath --mode=merged >> $jobFile 



