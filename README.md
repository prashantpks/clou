# Clou
Clou is a static analysis tool for detecting and mitigating Spectre vulnerabilities in programs.
Clou is implemented as a custom IR pass in LLVM.
It takes a C source file as input, compiles it to LLVM IR using Clang 12, and analyzes each defined function one-by-one.
Eventually, Clou outputs a list of transmitters and a set of consistent candidate executions that give witness to detected Spectre vulnerabilities.
Clou is optiimzes to detect universal data transmitters, but it can identify other kinds of transmitters as well.

If you're interested in the theoretical foundation -- leakage containment models -- and implementation details of Clou, see [ISCA'22 paper](https://doi.org/10.1145/3470496.3527412).

# Installation

Note: in the following instructions, replace `$CLOU_REPO` with the absolute path to this repository or set the environment variable to be the absolute path to this repository.

## Docker
1. cd into root directory of this repo: `cd $CLOU_REPO`
2. Build the docker image using our wrapper script: `./docker-build.sh clou`. This will probably take 5-10 minutes.
3. Run an instance of the built image with: `./docker-run.sh clou`

In the current /build directory:
Compile Clou: 
```sh
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j$(nproc)
```

# Reproducing Results from the Paper

This section describes how to reproduce the results presented in Table 2 of the paper.
All commands should be run _within_ the Clou docker container.

For each benchmark (i.e., multi-row in the paper), we run Clou multiple times, with different combinations of Spectre detector (Clou-PHT for Spectre v1 or Clou-STL for Spectre v4) and transmitter class (dt, ct, udt, uct).
The following is a breakdown of the various benchmarks, including the different parameter combinations in cartesian-product notation.

We evaluate 4 litmus test suites in the paper:
- pht (Spectre v1 / bounds check bypass) - {v1} x {dt,ct,udt,uct}
- stl (Spectre v4 / speculative store forwarding) - {v4} x {dt,ct,udt,uct}
- fwd (Spectre v1.1 / bounds check bypass write) - {v1,v4} x {dt,ct,udt,uct}
- new (new variant of Spectre v1.1) - {v1,v4} x {dt,ct,udt,uct}

and 4 crypto benchmarks in the paper:
- [tea](https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm) - {v1,v4} x {udt,uct}
- [donna](http://code.google.com/p/curve25519-donna/) - {v1,v4} x {udt,uct}
- [secretbox](https://github.com/jedisct1/libsodium/tree/master/src/libsodium/crypto_secretbox) - {v1,v4} x {udt,uct}
- ssl3-digest - {v1,v4} x {udt,uct}
- mee-cbc - {v1,v4} x {udt,uct}

and 2 crypto libraries in the paper:
- [libsodium](https://libsodium.org) - {v1,v4} x {udt,uct}
- [OpenSSL](https://openssl.org) - {v1,v4} x {udt}

## Running the Tests

To run a single test for a single benchmark, use the script `/clou/scripts/run-bench.sh`.
The general usage to run benchmark `BENCH` with detector `TYPE` and transmitter class `XMIT` is:
```bash
$ /clou/scripts/run-bench.sh -t TYPE -x XMIT BENCH
```
The output directory containing all analysis results is by default stored in a directory named `$BENCH-$TYPE-$XMIT`.

For full usage of the `run-bench.sh` script, run the help command: 
```bash
$ /clou/scripts/run-bench.sh -h
```

For example, to run a single test for benchmark `pht` looking for Spectre v1 leakage and universal data transmitters:
```bash
$ /clou/scripts/run-bench.sh -t v1 -x udt pht
```

This will run the `./run-pht.sh` script with required parameters which will run the test suite of pht whose functions are present in `/test` folder as pht1.c....pht15.c.
The src folder has all the required functionalities of clou such as creating cfg, then aeg and passing it to z3 solver and returning the transmitters and fenced llvm IR. This whole functionality of clou has been linked while generating llvm IR for let say pht1.c using the **libclou.so** linker.

## Analysing output
* By running the above command, `/pht-udt-out` folder will be created within the scripts folder.
* This folder has several subfolders having results of litmus test-suite **pht**.
* Open /pht1 folder, it has several files, leakages.txt, transmitters.txt and many others.
* To get to know more about the result of test functions, which is orginally present in `clou/test` folder as pht1.c and many other test functions.
* The instructions and the functions from the llvm IR are shown in `clou/scripts/pht-udt-out/pht1/lkg/victim_function_v01/8-10-15.txt` for the test function pht1.c file.

## Remarks
As told by the instructor, we have changed the **line number 312** of `clou/src/leakge/leakage.cc` file so that the changes made must be reflected to the output.
After running the above example cmd, the change will be reflected in the  `clou/scripts/pht-udt-out/pht1/lkg/victim_function_v01/8-10-15.txt` file at the last line.



