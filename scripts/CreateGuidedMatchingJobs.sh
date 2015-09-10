#!/bin/bash

# Run this script from the directory where you wish to create slurm jobs
# Run this script to create various files necessary to run guided matching
# BUNDLE_DIR (previous reconstruction output dir)
# BASE_LIST_PATH (directory where all lists exist, coarse reconstruction in most cases)
# BASE_DIR (directory where you want to store the matching outcome)
# IMAGE_DIR (where images & keys are stored)
# NUM_NEAR (number of neighbouring images to match)
# NUM_LISTS ( number of parallel lists (usually 212) )

BUNDLE_DIR=$1
BASE_LIST_PATH=$2
BASE_DIR=$3
IMAGE_DIR=$4
NUM_NEAR=$5
NUM_LISTS=$6
BIN_PATH=$7
DENS_NEW=$8
if [ $# -ne 8 ]; then
    echo "Incorrect number of input arguments"
    echo "USAGE : BUNDLE_FILE LISTS_DIR DENS_DIR IMAGE_DIR NUM_NEAR NUM_LISTS BIN_PATH"
#    echo $1 $2 $3 $4 $5 $6 $7
fi

LIST_DIR=$BASE_DIR/pairs/
MATCHES_DIR=$BASE_DIR/matches/

rm -rf $LIST_DIR
rm -rf $MATCHES_DIR
mkdir $LIST_DIR
mkdir $MATCHES_DIR

# For the first iteration, do not pass --query_list 
# For later iterations, pass the list of newly localized images as --query_list

if [ $DENS_NEW -eq 1 ]; then
    echo "Creating limited guided match lists for localized imgs"
    $BIN_PATH/CreateGuidedMatchPairs --bundle_dir=$BUNDLE_DIR --base_dir=$BASE_LIST_PATH --result_dir=$LIST_DIR --nn_images=$NUM_NEAR --num_lists=$NUM_LISTS --query_list=$BUNDLE_DIR/localized_queries.txt
else
    $BIN_PATH/CreateGuidedMatchPairs --bundle_dir=$BUNDLE_DIR --base_dir=$BASE_LIST_PATH --result_dir=$LIST_DIR --nn_images=$NUM_NEAR --num_lists=$NUM_LISTS
fi

sleep 3

pairListFiles=($LIST_DIR/pairs-*.txt)
numPairLists=`expr ${#pairListFiles[@]} - 1`

jobFile=guidedmatch_array_jobs.sh

echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=guidedmatch" >> $jobFile
echo "#SBATCH --output=log/guidedmatch_%A_%a.out" >> $jobFile
echo "#SBATCH --error=log/guidedmatch_%A_%a.err" >> $jobFile
arrayStr="#SBATCH --array=0-"$numPairLists
echo $arrayStr  >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH --ntasks-per-core=2" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

echo $BIN_PATH/GuidedMatchLists --bundle_dir=$BUNDLE_DIR --base_dir=$BASE_LIST_PATH --result_dir=$MATCHES_DIR --list_file=$LIST_DIR/pairs-\$SLURM_ARRAY_TASK_ID.txt >> $jobFile


: '
filecounter=-1
counter=0
numCores=24
filename=""
for file in ${pairListFiles[@]}
do
    if [ `expr $counter % $numCores` -eq 0 ];
    then
        echo "wait" >> $filename
        counter=0
        filecounter=`expr $filecounter + 1`
        filename="job_"$filecounter".sh"
        echo $filename
        touch $filename
        echo "#!/bin/bash" > $filename
        echo "#SBATCH -A cvit" >> $filename
        echo "#SBATCH -p long" >> $filename
        echo "#SBATCH -n 24" >> $filename
        echo "#SBATCH --mem-per-cpu=3945" >> $filename
        echo "#SBATCH -t 24:00:00" >> $filename
        echo "sbatch " $filename >> run_all.sh
    fi 
    echo "time "$BIN_PATH/GuidedMatchLists" --base_dir="$BASE_DIR" --result_dir="$MATCHES_DIR" --list_file="$file" 2>&1 &" >> $filename
#    echo "time "$BIN_PATH/GuidedMatchLists" --base_dir="$BASE_DIR" --result_dir=/scratch/rajvi/pantheon_interior/ --list_file="$file" 2>&1 &" >> $filename
    counter=`expr $counter + 1`
    echo $counter
done
'
