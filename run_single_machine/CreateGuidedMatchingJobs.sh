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
    $BIN_PATH/CreateGuidedMatchPairs --bundle_dir=$BUNDLE_DIR --base_dir=$BASE_LIST_PATH --result_dir=$LIST_DIR --nn_images=$NUM_NEAR --num_lists=$NUM_LISTS --query_list=$BUNDLE_DIR/localized_queries_final.txt
else
    $BIN_PATH/CreateGuidedMatchPairs --bundle_dir=$BUNDLE_DIR --base_dir=$BASE_LIST_PATH --result_dir=$LIST_DIR --nn_images=$NUM_NEAR --num_lists=$NUM_LISTS
fi

sleep 3

echo $BIN_PATH/match_pairs $BASE_LIST_PATH/list_images.txt $BASE_LIST_PATH/list_keys.txt $BASE_DIR/pairs/pairs-0.txt $MATCHES_DIR/matches.txt > $BASE_DIR/run_guidedmatch.sh 

