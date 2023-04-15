## Tools for creating some shares and commitments for computation

### How to build
```
cmake . -B build
cd build
make -j
```

### Prerequisite before running
+ Move the [generators.txt](../files/generators.txt) into the `build` directory.
+ Move [adv-gen-sc.sh](adv-gen-sc.sh) and
  [adv-gen-sc-interactive.sh](adv-gen-sc-interactive.sh) into the `build`
  directory.

### Generate local verification materials for verification
+ Run `bash adv-gen-sc.sh BIDDER-NUM BUCKET-NUM DIR-TO-STORE-SHARES
   DIR-TO-STORE-COMMITMENTS` from `build` directory. 
   + The script also generates three files, each containing all
   file names of `share1`, `share2` and `commitments`, with a prefix
   `$ADV_NUM-$BUCKET_NUM-`.
   + `DIR-TO-STORE-SHARES` is the directory where the generated shares are
     stored and `DIR-TO-STORE-COMMITMENTS` is the directory where the 
     generated commitments are stored.
   + To generate materials of `non-interactive`, `2-round-interactive`,
     `4-round-interactive` variants, set `BUCKET-NUM` to `10000`,
     `100`, and `10` respectively.

### Generate materials for throughput evaluation
+ Run `bash adv-gen-sc-interactive.sh $BIDDER-NUM $BUCKET $SHARE_DIR 
  $COMMIT_DIR $round` to generate shares and commitments locally.
  + `BUCKET-NUM` should be set to `100` and `10` for interactive
    protocol of 2-round and 4-round respectively.
    `BUCKET-NUM` here refers to the number of buckets the servers use to 
    compute in each round of interactive protocol.
  + `SHARE_DIR` is the directory where the generated shares are
    stored and `COMMIT_DIR` is the directory where the 
    generated commitments are stored.
  + The script creates a directory `${ROUND}-interactive-id`, where there 
    are `3*ROUND` files, each containing all
    file names of `share1`, `share2` and `commitments` materials for `r` 
    round, `$r-round-s1-idx`, `$r-round-s2-idx`, and 
    `$r-round-commit-idx`.

### Generate materials for local computation costs of interactive variants
Follow the instruction of `Generate materials for throughput evaluation`.

### Generate materials for local computation costs of non-interactive protocol
Follow the instruction of `Generate local verification materials for
verification`, but only need to set `BUCKET-NUM` to `10000`.
