#!/bin/bash

# This script takes the matches files and creates long valid tracks from it
# This is different from merging, in a way that this program only connects directly connected matches a-->b, a-->c, a-->d would make a-->b-->c-->d. 
#This has to be followed up by merging 

BASE_PATH=$1
RES_PATH=$2
NUM_IMAGES=$3

echo USAGE BASE_PATH RES_PATH NUM_IMAGES

counter=0
#cat $RES_PATH/matches_list-*.txt > $RES_PATH/all_tracks.txt
#outFile=$RES_PATH"all_tracks"
#sortedOutFile1=$RES_PATH"sorted_all_tracks1"
#sortedOutFile2=$RES_PATH"sorted_all_tracks2"


#echo "Sorting Files"
#sort -nk1 $outFile > $sortedOutFile1
#sort -nk5 $outFile > $sortedOutFile2

#echo "Doing 1st column print"
#awk -v result_dir="$RES_PATH" '{ print >> result_dir"/tracks-"$1".txt"}' $sortedOutFile1
#echo "Doing 5th column print"
#awk -v result_dir="$RES_PATH"'{ print $5" "$6" "$7" "$8" "$1" "$2" "$3" "$4 >> result_dir"/tracks-"$5".txt"}' $sortedOutFile2

rm $RES_PATH/*tracks*
rm $RES_PATH/tracks*
#ls $RES_PATH/*-*.txt > list
#while read line
#do
#    file2=`basename $line | cut -d'.' -f1 | tr '-' ' ' | awk '{print $2" "$1}' | tr ' ' '-' | sed 's/$/.txt/'`
#    awk '{ print $5" "$6" "$7" "$8" "$1" "$2" "$3" "$4}' $line > $RES_PATH/$file2
#done < list
counter=0

while [ $counter -le $NUM_IMAGES ]
do
    cat $RES_PATH/$counter-*.txt > $RES_PATH/tracks-$counter.txt
    if [ -s $RES_PATH/tracks-$counter.txt ]; then 
        echo $counter
        cat $RES_PATH/tracks-$counter*.txt | sort -nk2 > $RES_PATH/sorted-tracks-$counter.txt
        ../src/app/make_tracks --tracks_dir=$RES_PATH --image_idx=$counter
       ../src/app/triangulate_tracks --bundler_path=$BASE_PATH --tracks_dir=$RES_PATH --image_idx=$counter
    fi
    counter=`expr $counter + 1`
done

