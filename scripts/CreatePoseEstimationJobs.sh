#!/bin/bash

binPath=$1
outPath=$2

queryFile=$outPath/queries.txt

numImgToLoc=`cat $queryFile | wc -l`
numImgToLoc=`expr $numImgToLoc - 1`
jobFile=localization_array_jobs.sh

echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=localization" >> $jobFile
echo "#SBATCH --output=log/localization_%A_%a.out" >> $jobFile
echo "#SBATCH --error=log/localization_%A_%a.err" >> $jobFile
arrayStr="#SBATCH --array=0-"$numImgToLoc
echo $arrayStr  >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH --ntasks-per-core=2" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

echo $binPath/acg_localizer_with_own_matches $outPath \$SLURM_ARRAY_TASK_ID >> $jobFile 

: '
'
