# bash adv-gen-sc.sh $NUM_ADV $BUCKET $SHARE_DIR $COMMIT_DIR

cmd=""
adv_num=$[$1-1]
bucket=$2
sdir=$3
cdir=$4

mkdir $sdir
mkdir $cdir

for i in $(seq 0 $adv_num)
do
    cmd1="./share-commit-gen -b $2 -s $sdir/adv$i -c $cdir/adv$i"
    echo $cmd1
    eval $cmd1
done

ls $sdir | grep s1 > $1-$2-s1-idx
ls $sdir | grep s2 > $1-$2-s2-idx
ls $cdir > $1-$2-commit-idx