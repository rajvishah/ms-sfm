dir=$1
rm $dir/file-*.txt
rm $dir/*.srt
cat $dir/initial-ranks-* | sort -nk1 > $dir/all_ranks
awk -v dir1=$dir '{print >> dir1"/file-"$1".txt"}' $dir/all_ranks
FILES=$dir'/file-*.txt'
for file in $FILES;
do
#    echo $file
newFile=$file'.srt'
    sort -nk3 -r $file > $newFile
   # rm $file
done
