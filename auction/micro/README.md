## Micro benchmark for bidder's cost
### How to build
```
cmake . -B build
cd build
make -j
```

### How to run
+ Move the [generators.txt](../files/generators.txt) into the `build` directory.
+ Run `./adv-cost`.

### Note
The outputs of the program include the time of different operations of 
bidders and the size of generated materials.
If not specified, the unit of outputs related to `time` is `second` and the 
unit for size of materials is `byte`.
