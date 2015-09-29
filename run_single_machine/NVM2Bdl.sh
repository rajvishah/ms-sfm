#!/bin/bash
inputFile=$1 #NVM File
listFile=$2  #list_images.txt
outputFile=$3 #File to store id mappings

# Find the number of cameras in 1st model
numCams=`awk 'NR==3' $inputFile`
numCamLines=`expr $numCams + 3`
# Extract filename part of information from nvm cameras output
sed -n -e 4,${numCamLines}p $inputFile | sed -e "s/[[:space:]]\+/ /g" | cut -d' ' -f1 > tmpnvmlist

# Extract filename part of information from full list (list_images.txt)
while read line
do
  basename $line >> tmpfulllist
done < $listFile

rm $outputFile.map1 $outputFile.map2
#Find and assign corresponding ID for NVM sequential IDs in full list file
count=0
while read line
do
 mapidstr=`grep -n -e $line tmpfulllist | sed 's/:/ /'`
 mapid=`echo $mapidstr | cut -d' ' -f1`
 mapid=`expr $mapid - 1`
 mapstr=`echo $mapidstr | cut -d' ' -f2`
 echo $count $mapid $mapstr >> $outputFile.map1
 count=`expr $count + 1`
done < tmpnvmlist

#Inverse map of the above from list_images --> nvm id
count=0
while read line
do
  outputLine=`grep -n -e $line tmpnvmlist`
  if [ $? -eq 0 ]; then 
    mapid=`echo $outputLine | cut -d':' -f1`
    mapid=`expr $mapid - 1`
    mapstr=`echo $outputLine | cut -d':' -f2`
    echo $mapid $mapstr
    echo $count $mapid $mapstr >> $outputFile.map2
  else
    echo 0
    echo $count "-1" "notfound" >> $outputFile.map2
  fi
 count=`expr $count + 1`
done < tmpfulllist


# Remove the tmp files 
rm tmpfulllist tmpnvmlist
