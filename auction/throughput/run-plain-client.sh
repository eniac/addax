t=$1
load=$2
dir=$3
ip=0.0.0.0
port=6667
cmd=""

mkdir $dir
for i in $(seq 1 $t)
do
    cmd1="./plain-client -t 1 -l $load -i $ip:$port > $3/$port.txt & "
    cmd="$cmd$cmd1"
    let port++
done

echo $cmd
eval $cmd