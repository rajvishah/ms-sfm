#!/bin/bash

# Run this script from the directory where you wish to create slurm jobs
# Run this script to create various files necessary to run guided matching
# BUNDLER_DIR (previous reconstruction directory)
# BASE_DIR (directory where you want to store the matching outcome)
# IMAGE_DIR (where images & keys are stored)
# NUM_NEAR (number of neighbouring images to match)
# NUM_LISTS ( number of parallel lists (usually 212) )

BUNDLER_DIR=$1
BASE_DIR=$2
IMAGE_DIR=$3
NUM_NEAR=$4
NUM_LISTS=$5
BIN_PATH=$6

echo "USAGE : BUNDLER_DIR BASE_DIR IMAGE_DIR NUM_NEAR NUM_LISTS BIN_PATH"

RESULT_DIR=$BASE_DIR/guided_match/
LIST_DIR=$RESULT_DIR"/matching_lists/"
MATCHES_DIR=$RESULT_DIR"/matches_files/"

rm -r $LIST_DIR
rm -r $MATCHES_DIR
mkdir $RESULT_DIR
mkdir $LIST_DIR
mkdir $MATCHES_DIR

cp $BUNDLER_DIR/bundle.out $BASE_DIR/bundle.out
cp $BUNDLER_DIR/list_images.txt $BASE_DIR/list_images.txt
cp $BUNDLER_DIR/list_keys.txt $BASE_DIR/list_keys.txt
identify $IMAGE_DIR/*.jpg | cut -d' ' -f3 | sed 's/x/ /g'> $BASE_DIR/image_dims.txt

# For the first iteration, do not pass --query_list
# For later iterations, pass the list of newly localized images as --query_list

#../bin/CreateGuidedMatchPairs --base_dir=$BASE_DIR --result_dir=$RESULT_DIR --nn_images=$NUM_NEAR --num_lists=$NUM_LISTS --query_list=$1/query_ids.txt

$BIN_PATH/CreateGuidedMatchPairs --base_dir=$BUNDLER_DIR --result_dir=$LIST_DIR --nn_images=$NUM_NEAR --num_lists=$NUM_LISTS
pairListFiles=($LIST_DIR/pairs-*.txt)
numPairLists=${#pairListFiles[@]}

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

echo $BIN_PATH/GuidedMatchLists --base_dir="$BASE_DIR" --result_dir="$MATCHES_DIR" --list_file="$file 2>&1 &" >> $filename


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
