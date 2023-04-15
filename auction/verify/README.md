## Costs of verifying an auction

### How to build
```
cmake . -B build
cd build
make -j
```

### Prerequisite before running
+ Move the [generators.txt](../files/generators.txt) into the `build` directory.
+ Follow the instructions in [README.md](../tools/README.md) in `tools`
  directory regarding how to generate materials for verification.
  And move the outputs of these materials to the `build` directory.

### How to run
+ Run `./verify -a AD-NUM -d DIR-OF-SHARES/ -s FILE-SHARE1 -S FILE-SHARE2 -c
   FILE-COMMITMENT -D DIR-OF-COMMITMENTS -b BUCKET-NUM -t NUMBER-OF-RUN -i
   (if interactive)`.
   + `verify` program outputs the costs of each phase for the auditor's
     verification.
   + When `-i` is not set and `BUCKET_NUM` is set to `10000`, the program is running the non-interactive 
   variant protocol. And the outputs include all the costs in verification.
   + When `-i` is set, `BUCKET_NUM` is set to `100` and `10` respectively, 
   the outputs include the costs of verifying outcome of one round in the
   protocol. For `r`-round variant, in total the verifier will verify the
   outcome of `2r` rounds (the costs should be multiplied by `2r`).
      + For extra safeguard in the interactive protocol, the verifier needs 
      to verify the winner and sale price bidder's full AFE vectors are
      consistent with the commitment. This cost is the same as verifying 
      two sum vectors in the non-interactive variant.
      + The extra safeguard for interactive variant also includes verifying 
      the zkp of the sale price bidder is correct. To produce the cost, we 
      can run `./verify-zkp -b ENTRY_TO_VERIFY`.
         + `ENTRY_TO_VERIFY` should be set to `400` and `80` respectively to
           account for the maximum number of entries in `2-round` and `4-round` variants respectively.

### Note
If not specified, the unit of outputs related to `time` is `second` and the 
unit for size of materials is `byte`.