#!/bin/bash
dataset=$1
bundleFile=$dataset/coarse_model/bundle.out.rad
#listFile=$dataset/coarse_model/list.txt
listFile=/data/rajvi/JournalExperiments/GroundTruthBdl/$dataset"_list.txt"
binPath=/home/rajvi/PhD/Projects/ThirdPartyCodes/TheiaNew/TheiaSfM/build/bin
statDir=$1_stats
mkdir $statDir


outputFile=$statDir/$1_theia.bin
infoFile=$statDir/info_coarse_rad.txt

$binPath/convert_bundle_file --lists_file=$listFile --bundle_file=$bundleFile --output_reconstruction_file=$outputFile

$binPath/compute_reconstruction_statistics --reconstruction=$outputFile --log_dir=$statDir

num3DPoints=`cat $statDir/compute_reconstruction_statistics.INFO | grep "Num 3D points:" | cut -d' ' -f4`
num2Points=`cat $statDir/compute_reconstruction_statistics.INFO | grep "\[2 - 3" | cut -d' ' -f5`

num3pluspoints=`expr $num3DPoints - $num2Points`

echo "Num 3+ points " $num3pluspoints >> $statDir/compute_reconstruction_statistics.INFO

echo "Num connected pairs " `awk 'BEGIN { s = 0 } {for (i = 1; i <= NF; i++) { if ($i > 0) {s = s + 1;} }} END {print s}' $dataset/dens1/view_inters.txt` >> $statDir/compute_reconstruction_statistics.INFO

recon1="/data/rajvi/JournalExperiments/GroundTruthBdl/"$dataset"_theia.bin"
$binPath/compare_reconstructions --reconstruction1=$recon1 --reconstruction2=$outputFile --log_dir=$statDir

echo $bundleFile > $infoFile
cat $statDir/compute_reconstruction_statistics.INFO >> $infoFile
cat $statDir/compare_reconstructions.INFO >> $infoFile

rm $statDir/*.INFO*
rm $statDir/*FATAL*
rm $statDir/*ERROR*
rm $statDir/*WARNING*
