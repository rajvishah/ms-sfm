#!/bin/bash
CALL_SBATCH() {
    batchStr=`sbatch $1`
    batchId=`echo $batchStr | cut -d' ' -f4`
    echo $batchStr $batchId
    while [ 1 ]; 
    do
        activeJobs=`squeue | grep "rajvi" | grep $batchId | wc -l`
        echo "Active jobs " $activeJobs
        if [ $activeJobs -gt 0 ]; then
            sleep 1
        else
            break;
        fi
    done
    echo "Job Done"
}
