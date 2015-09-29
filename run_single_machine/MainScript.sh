#!/bin/bash

MSSFM_PATH=/home/rajvi/MSSFM_FRESH/

imageDir=$1
runDir=`pwd`
binPath=$MSSFM_PATH/ms-sfm/bin/
scriptPath=$MSSFM_PATH/ms-sfm/run_single_machine/

vsfmBinPath=

globalMatchDir=$runDir/initial_matching
cgmDir=$runDir/coarse_model

locDir=$runDir/loc0
locOwnDir=$runDir/loc0own
densDir=$runDir/dens0
loc1Dir=$runDir/loc1
loc1OwnDir=$runDir/loc1own
dens1Dir=$runDir/dens1
loc2Dir=$runDir/loc2
loc2OwnDir=$runDir/loc2own
dens2Dir=$runDir/dens2

#################################################################################
rm -rf $globalMatchDir; mkdir $globalMatchDir
#rm -rf $cgmDir; mkdir -p $cgmDir/log
ls $imageDir/*.key > $cgmDir/list_keys.txt
ls $imageDir/*.jpg > $cgmDir/list_images.txt
identify $imageDir/*.jpg | cut -d' ' -f3 | sed 's/x/ /g'> $cgmDir/image_dims.txt

### -----------------------------------------------

# Write VisualSFM Call Commands/ Matching Commands
# Process Outputs / Parse logs and output stats


$scriptPath/NVM2Bdl.sh $cgmDir/result.nvm $cgmDir/list_images.txt $cgmDir/bdlconvert
$binPath/nvm2bdl $cgmDir/bundle.buggy.out $cgmDir/bdlconvert.map1 $cgmDir/bdlconvert.map2 $cgmDir/bundle.out

###------------------------------------------------
:'
###################################################################################
$scriptPath/RunLocalize.sh $locDir $scriptPath $binPath $cgmDir $cgmDir
$scriptPath/RunLocalizeOwn.sh $locOwnDir $scriptPath $binPath $locDir $cgmDir $globalMatchDir

###################################################################################

$scriptPath/RunDensify.sh $densDir $cgmDir $locOwnDir $imageDir $scriptPath $binPath 0

###################################################################################

$scriptPath/RunLocalize.sh $loc1Dir $scriptPath $binPath $densDir $cgmDir
$scriptPath/RunLocalizeOwn.sh $loc1OwnDir $scriptPath $binPath $loc1Dir $cgmDir $globalMatchDir

###################################################################################

$scriptPath/RunDensify.sh $dens1Dir $cgmDir $loc1OwnDir $imageDir $scriptPath $binPath 1

###################################################################################

$scriptPath/RunLocalize.sh $loc2Dir $scriptPath $binPath $dens1Dir $cgmDir

###################################################################################
'
