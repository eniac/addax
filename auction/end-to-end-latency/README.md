## End-to-end latency of Addax's interactive auction protocol

### How to build
```
cmake . -B build
cd build
make -j
```

### How to run
1. Start publisher.
   + Run `./publisher-interactive -a AD_NUM -r ROUND`
2. Start bidders.
   + On each machine of the bidders, run `bash simulate-advs-interactive.sh BIDDER_NUM
   STARTING_PORT BUCKET_NUM PUBLISHER_IP:6668 IDX-OF-STARTING-ADVERTISER ROUND`.
      + The script will launch `BIDDER_NUM` bidders on that machine.
      These bidders' ports start from `STARTING_PORT` to
      `STARTING_PORT+BIDDER_NUM-1`, each bidder instance listens on one port.
   + If you want to run the bidder programs on different machines, then you 
   can run the script on each machine individually. 
3. Start another auction server.
   + Create a file (`FILENAME-OF-ADVS-IP` in the parameter when running 
      `server-interactive`) where each row consists of one bidder's
      ip address in the format of `IP:PORT`.
      + An example file is [example-ips.txt](./example-ips.txt).
   + Run `./server-interactive -a AD_NUM -p PUBLISHER-IP -r ROUND -f
   FILENAME-OF-ADVS-IP`.
   + The last output of `server-interactive` is the total end-to-end latency.
4. After each run, you need to manually kill all advertiser progesses, by 
   running `pkill -f -9 advertiser-interactive`.

### Note
+ If not specified, the unit of outputs related to `time` is `second` and 
the unit for size of materials is `byte`.
+ By default the program is running second-price auction. If you want to test
  the end-to-end latency of first-price auction, add `-q` to when running
  `publisher-interactive` and `server-interactive`.
