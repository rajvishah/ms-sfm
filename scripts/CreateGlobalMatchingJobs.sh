#!/bin/bash

binPath=$1
keyList=$2
numCores=$3
outputDir=$4


mkdir $outputDir'/pairs'
mkdir $outputDir'/matches'
mkdir $outputDir'/ranks'
mkdir $outputDir'/logs'

numKeys=`wc -l $keyList | cut -d' ' -f1`

filecounter=-1
counter=0
counter1=0

$binPath/create_pairs_list $keyList $outputDir/pairs/ $numCores 



while [ $counter -lt $numCores ] 
do
    pairsFile=$( printf "%s/pairs/initial-pairs-%04d.txt" $outputDir $counter )
    matchesFile=$( printf "%s/matches/initial-matches-%04d.txt" $outputDir $counter )
    ranksFile=$( printf "%s/ranks/initial-ranks-%04d.txt" $outputDir $counter )

    if [ `expr $counter % 24` -eq 0 ];
    then
        echo "wait" >> $filename
        counter1=0
        filecounter=`expr $filecounter + 1`
        filename="job_"$filecounter".sh"
        echo $filename
        touch $filename
        echo "#!/bin/bash" > $filename
        echo "#SBATCH -A cvit" >> $filename
        echo "#SBATCH -p cvit" >> $filename
        echo "#SBATCH -n 24" >> $filename
        echo "#SBATCH --mem-per-cpu=3945" >> $filename
        echo "#SBATCH -t 24:00:00" >> $filename
    fi 
    
    echo time $binPath/multiscale_mg --pairs_list=$pairsFile --ranks_list=$ranksFile --matches_file=$matchesFile --keyfile_list=$keyList '> '$outputDir'/logs/'$counter'.log 2>&1 &' >> $filename
    counter=`expr $counter + 1`
done


