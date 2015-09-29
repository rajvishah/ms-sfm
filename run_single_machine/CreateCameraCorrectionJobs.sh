#!/bin/bash

# This script takes as an input bundle path, localization path and creates script for writing cameras in bundler format

listPath=$3
bundlePath=$2
locPath=$4
binPath=$1

queryFile=$locPath/queries.txt

echo $binPath/localizer $bundlePath $listPath/list.txt $locPath/queries.txt $locPath/CamParFiles/ >> $locPath/run_localization.sh 
