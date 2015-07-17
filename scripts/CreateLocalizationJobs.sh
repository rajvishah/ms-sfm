#!/bin/bash

# This script takes as an input a bundle file and outputs a list of unlocalized images and their ids
# It also creates jobs to run sattlers localization code on these query images and bundle file

bundleFile=$1
listFile=$2
acgBinPath=$3
outPath=$4

rm $queryFile

infoLine=`sed -n -e 2p $bundleFile`
infoArray=($infoLine)
numCameras=${infoArray[0]}
numLinesPerCam=5
numCamLines=`expr $numCameras \* $numLinesPerCam`
numCamLines=`expr $numCamLines + 2`
allLines=`sed -n -e 3,${numCamLines}p $bundleFile | grep -n -e "0 0 0" | cut -d':' -f1 | sed -n '1~5p' | awk '{print ($1 - 1)/5 }'`
lineNums=($allLines)
numQueries=${#lineNums[@]}

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
            echo $b" "$line >> $outPath/$queryFile
            counter2=`expr $counter2 + $constantOne`
        fi
        counter1=`expr $counter1 + $constantOne`
    fi
#    echo $counter1 $counter2 $b $numQueries
done < $listFile

suffix=`echo $bundleFile | cut -d'.' -f2`
infoFileName=""
descFileName=""
echo $suffix
if [ $suffix==".out" ]; then
    infoFileName="bundle.info"
    descFileName="bundle_desc_voc.bin"
else
    infoFileName="bundle."$suffix".info"
    descFileName="bundle_desc_voc"$suffix".bin"
fi

$acgBinPath/Bundle2Info $bundleFile $listFile $outPath/$infoFileName
args=$outPath/$infoFileName" 1 100000 "$acgBinPath"/clusters.txt "$outPath/$descFileName" 6 1 0"
echo $args
$acgBinPath/compute_desc_assignments $args

mkdir $outPath/CamParFiles

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

echo $acgBinPath/acg_localizer_active_search $queryFile $bundleFile 100000 $acgBinPath/clusters.txt $outPath/$descFileName 0 $outPath/CamParFiles/ 200 1 1 1 10 \$SLURM_ARRAY_TASK_ID 10 >> $jobFile 
