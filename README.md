<meta name="robots" content="noindex">

# LSM-OPD

An open-source implementation for LSM-OPD: Boosting Scans in LSM-Trees by Enabling Direct Computing on Compressed Data for HTAP workloads.

G++12 and [Folly](https://github.com/facebook/folly) needed.

## Building
```bash
mkdir build && cd build
cmake ..
make -j 8
```

## Structure
- `include/LSM-OPD` the header file of LSM-OPD.
- `include/dynamic_bitset` [dynamic_bitset](https://github.com/pinam45/dynamic_bitset) library
- `include/folly` [Folly](https://github.com/facebook/folly) library
- `src` the source code of LSM-OPD.

## Benchmark

We used the following branches of ycsb for system testing which can be found in the following repository:



## Usage in your own work

```c++
#include "include/LSM-OPD/LSM-OPD.h"
```

Then add the linking options during building:
```bash
-llsm-opd -L/path/to/LSM-OPD/build 
```
