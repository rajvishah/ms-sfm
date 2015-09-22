#!/bin/bash

bundlePath=$1
listPath=$2
binPath=$3
outPath=$4
rankDir=$5

queryFile=$outPath/queries.txt
touch $queryFile

bundleFile=$bundlePath/bundle.out
listFile=$listPath/list_keys.txt

infoLine=`sed -n -e 2p $bundleFile`
infoArray=($infoLine)
numCameras=${infoArray[0]}
numLinesPerCam=5
numCamLines=`expr $numCameras \* $numLinesPerCam`
numCamLines=`expr $numCamLines + 2`
allLines=`sed -n -e 3,${numCamLines}p $bundleFile | grep -n -e "0 0 0" | cut -d':' -f1 | sed -n '1~5p' | awk '{print ($1 - 1)/5 }'`
lineNums=($allLines)
numQueries=${#lineNums[@]}

echo $numQueries

counter1=0
counter2=0

constantOne=1
while read line
do
    if [ $counter2 -lt $numQueries ]; then
        b=${lineNums[$counter2]}
        if [ $counter1 == $b ];
        then
#            echo $b 
            echo $b" "$line >> $queryFile
            counter2=`expr $counter2 + $constantOne`
        fi
        counter1=`expr $counter1 + $constantOne`
    fi
#    echo $counter1 $counter2 $b $numQueries
done < $listFile

mkdir $outPath/CamParFiles
mkdir $outPath/correspondences/

numImgToLoc=`cat $queryFile | wc -l`
numImgToLoc=`expr $numImgToLoc - 1`
jobFile=match_3d_2d_array_jobs.sh

echo "#!/bin/bash" > $jobFile
echo "#SBATCH --job-name=match_3d_2d" >> $jobFile
echo "#SBATCH --output=log/match_3d_2d_%A_%a.out" >> $jobFile
echo "#SBATCH --error=log/match_3d_2d_%A_%a.err" >> $jobFile
arrayStr="#SBATCH --array=0-"$numImgToLoc
echo $arrayStr  >> $jobFile
echo "#SBATCH --partition=long" >> $jobFile
echo "#SBATCH --account=cvit" >> $jobFile
echo "#SBATCH --mem-per-cpu=3945" >> $jobFile
echo "#SBATCH --ntasks-per-core=2" >> $jobFile
echo "#SBATCH -t 24:00:00" >> $jobFile

echo $binPath/matcher3d_2d $bundlePath $listPath $outPath/queries.txt $rankDir/all_ranks_sorted.txt $outPath/correspondences \$SLURM_ARRAY_TASK_ID 50 >> $jobFile 

: '
'
