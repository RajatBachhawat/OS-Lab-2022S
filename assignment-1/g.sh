> $1
var="NO"
for (( i=1;i<=1500;i++ )); do
    printf $RANDOM"," >> $1
done
sed -i 's/,/\n/10;P;D' $1
cut -d',' -f$2 $1 | grep -qEwe $3
if(($?==0)); then
    var="YES"
fi
echo $var