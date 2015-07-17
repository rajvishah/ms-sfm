#!/bin/bash
# Run this script from the directory where you want the job and slurm output to be created
# This script creates scripts to run on abacus nodes for global matching
# binPath : path to multiscale matching binary
# keyList : path to a list of key files
# numCores : Usually 212 (9 * 24) if using all 9 nodes of cvit cluster
# outputDir : Path to store the results (e.g. /lustre/cvit/rajvi/<dataset>/global_matching/)


binPath=$1
numCores=$3
outputDir=$4

keyList=$outputDir/list_keys.txt

mkdir $outputDir'/pairs'
mkdir $outputDir'/matches'
mkdir $outputDir'/ranks'
mkdir $outputDir'/logs'

numKeys=`wc -l $keyList | cut -d' ' -f1`

filecounter=-1
counter=0
counter1=0

$binPath/CreateGlobalMatchPairs $keyList $outputDir/pairs/ $numCores 

jobFile=global_matching_job.sh
echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=global_match" >> $jobFile
echo "#SBATCH --output=log/global_match_%A_%a.out" >> $jobFile
echo "#SBATCH --error=log/global_match_%A_%a.err" >> $jobFile
arrayStr="#SBATCH --array=0-"$numCores
echo $arrayStr  >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH --ntasks-per-core=2" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

echo time $binPath/multiscale_mg --pairs_list=$outputDir/pairs/initial-pairs-\$SLURM_ARRAY_TASK_ID.txt --ranks_list=$outputDir/pairs/initial-matches-\$SLURM_ARRAY_TASK_ID.txt --matches_file=$outputDir/pairs/initial-matches-\$SLURM_ARRAY_TASK_ID.txt --keyfile_list=$keyList >> jobFile


