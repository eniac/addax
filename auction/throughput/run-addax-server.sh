t=$1
dir=$2
share_dir=$3
idx_dir=$4
round=$5
first=$6
port=6667
cmd=""

mkdir $dir
echo $t
for i in $(seq 1 $t)
do
    cmd1="./addax-server -t 1 -a 96 -s $idx_dir -S $idx_dir -d $share_dir -r $round -i 0.0.0.0:$port $first > $dir/$port.txt & "
    cmd="$cmd$cmd1"
    let port++
done

echo $cmd
eval $cmd