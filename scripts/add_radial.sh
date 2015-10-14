sourceBundle=$1
targetBundle=$2

nImgs=`awk 'NR == 2' $targetBundle | cut -d' ' -f1`
nCamLines=`expr $nImgs \* 5 + 2`

sed -n 3,${nCamLines}p $sourceBundle > camlines1
awk 'NR%5 == 1' camlines1 > rad_lines1

sed -n 3,${nCamLines}p $targetBundle > camlines2
awk 'NR%5 == 1' camlines2 > rad_lines2

cat rad_lines2 | cut -d' ' -f1 > a 
cat rad_lines1 | cut -d' ' -f2,3 > b

paste a b | sed -e "s/[[:space:]]\+/ /g" > c

count=0

head -2 $targetBundle > $targetBundle".rad"
awk '{ if(NR%5==1) { getline line < "c"; print line; } else { print $0; } }' camlines2 >> $targetBundle".rad"

nPoints=`expr $nCamLines + 1`
nTotal=`wc -l $targetBundle | cut -d' ' -f1`
sed -n ${nPoints},${nTotal}p $targetBundle >> $targetBundle".rad"

rm a b c rad_lines1 rad_lines2 camlines1 camlines2 




