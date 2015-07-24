#!/bin/bash

# This script takes as an input bundle path, localization path and creates script for writing cameras in bundler format

listPath=$3
bundlePath=$2
locPath=$4
binPath=$1

queryFile=$locPath/queries.txt

numImgToLoc=`cat $queryFile | wc -l`
numImgToLoc=`expr $numImgToLoc - 1`
jobFile=camcorrect_array_jobs.sh

echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=camcorrect" >> $jobFile
echo "#SBATCH --output=log/camcorrect_%A_%a.out" >> $jobFile
echo "#SBATCH --error=log/camcorrect_%A_%a.err" >> $jobFile
arrayStr="#SBATCH --array=0-"$numImgToLoc
echo $arrayStr  >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH --ntasks-per-core=2" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

echo $binPath/localizer $bundlePath $listPath/list.txt $locPath/queries.txt $locPath/CamParFiles/ \$SLURM_ARRAY_TASK_ID >> $jobFile 


: '
'
