cmd=""
adv_num=$[$1-1]
port=$2
bucket=$3
pub_ip=$4
idx=$5
for i in $(seq 0 $adv_num)
do
    cmd1="./bidder-interactive -r $6 -i $idx -c 0.0.0.0:$port -b $3 -p $pub_ip & "
    cmd="$cmd$cmd1"
    let port++
    let idx++
done

echo $cmd
eval $cmd