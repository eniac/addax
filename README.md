# Addax: A fast, private, and accountable ad exchange infrastructure

This repository contains the codes of the paper "Addax: A fast, private, and
accountable ad exchange infrastructure" in NSDI 2023.

More features are being organized and will be released later.

## Setup
Please refer to [install.md](./install.md).

## Code organization
+ Private and verifiable auction
    + [auction/addax-lib](./auction/addax-lib/):
      library codes of Addax.
    + [auction/tools](./auction/tools/):
    a tool used for evaluation.
    + [auction/micro](./auction/micro/):
    microbenchmark for bidder's costs.
    + [auction/auction-local-computation](./auction/auction-local-computation/):
      evaluation of local computation costs.
    + [auction/end-to-end-latency](./auction/end-to-end-latency/):
    evaluation of end-to-end latency.
    + [auction/throughput](./auction/throughput/):
    evaluation of throughput.
    + [auction/verify](./auction/verify/):
    evaluation of verification costs.

## Instructions for running
Please refer to the `README.md` files under each directory.