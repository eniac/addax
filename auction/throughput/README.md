## Throughput

### How to build
```
cmake . -B build
cd build
make -j
```

### Prerequisite before running
+ Move `*.pem` and `*.sh` to your `build` directory.
+ Follow the instructions in [README.md](../tools/README.md) in `tools`
  directory regarding how to generate materials of `96` bidders for 
  throughput evaluation.
  And move the outputs of these materials to the `build` directory.

### How to run
#### Non-private baseline throughput
+ On the test machine, run `bash run-plain-server.sh $PARALLEL_NUM
  $DIR_OF_SERVER_OUTPUTS`.
  + `PARALLEL_NUM` is the number of servers you run in parallel, it's
    recommended that you set it to the number of cores in your machine.
  + `DIR_OF_SERVER_OUTPUTS` is the directory where the outputs files of 
  each server will be written. 
+ On the same machine, run `bash run-plain-client.sh $PARALLEL_NUM $LOAD_PER_SEC
  $DIR_OF_CLIENT_OUTPUTS`.
  + `DIR_OF_CLIENT_OUTPUTS` is the directory where the outputs files of 
  each client will be written.
  + `LOAD_PER_SEC` is the number of requests that each client program 
  sends to the server in one second.
+ Run `pkill plain-server` every time you finish running.

#### Addax throughput
+ On one test machine, run `bash run-addax-server.sh $PARALLEL_NUM
  $DIR_OF_SERVER_OUTPUTS $DIR_WITH_SHARES $SHARE_FILES_NAMES_DIR $ROUND`.
  + `PARALLEL_NUM` is the number of servers you run in parallel, it's
    recommended that you set it to the number of cores in your machine.
  + `DIR_WITH_SHARES` is the directory where the shares are stored.
  + `SHARE_FILES_NAMES_DIR` is the directory that contains the file names of the shares.
  + `DIR_OF_SERVER_OUTPUTS` is the directory where the outputs files of 
  each server will be written.
  + If you want to run first-price baseline add `-q` as the last 
  parameter of the command.
+ On the same machine, run `bash run-addax-client.sh $PARALLEL_NUM 
  $LOAD_PER_SEC $DIR_OF_CLIENT_OUTPUTS $ROUND`
  + `DIR_OF_CLIENT_OUTPUTS` is the directory where the outputs files of 
  each client will be written.
  + `LOAD_PER_SEC` is the number of requests that each client program 
  sends to the server in one second.
+ Run `pkill addax-server` every time you finish running.

#### Analyze throughput from the outputs
+ To compute the median and 99% latency of all the requests from the client, run
  `python cal-latency.py DIR_OF_CLIENT_OUTPUTS`.
+ To compute the throughput (i.e., how many tasks are finished within the 
  given time frame) of the server, run
  `python cal-throughput.py DIR_OF_SERVER_OUTPUTS LOAD_PER_SEC`. 
