t=$1
dir=$2
port=6667
cmd=""

mkdir $dir
echo $t
for i in $(seq 1 $t)
do
    cmd1="./plain-server -t 1 -i 0.0.0.0:$port > $dir/$port.txt & "
    cmd="$cmd$cmd1"
    let port++
done

echo $cmd
eval $cmd