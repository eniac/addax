# note: this is only a simulation of generating encodings of interactive
# protocol, those generated encodings may be invalid
# bash adv-gen-sc.sh $NUM_ADV $BUCKET $SHARE_DIR $COMMIT_DIR $round

cmd=""
adv_num=$[$1-1]
bucket=$2
sdir=$3
cdir=$4
round=$[$5*2]

mkdir $sdir
mkdir $cdir

for r in $(seq 1 $round)
do
    mkdir $sdir/$r
    mkdir $cdir/$r
done

for i in $(seq 0 $adv_num)
    do
    for r in $(seq 1 $round)
        do
            cmd1="./share-commit-gen -b $2 -s $sdir/$r/adv$i -c $cdir/$r/adv$i"
            echo $cmd1
            eval $cmd1
        done
    done

mkdir $5-interactive-idx
for r in $(seq 1 $round)
do
    ls $sdir/$r | grep s1 > $5-interactive-idx/$r-round-s1-idx
    ls $sdir/$r | grep s2 > $5-interactive-idx/$r-round-s2-idx
    ls $cdir/$r > $5-interactive-idx/$r-round-commit-idx
done