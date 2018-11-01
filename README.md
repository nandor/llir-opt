# GenM Optimiser





## Configuration

Debug builds are configured as follows:
```
mkdir build
cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DLLVM_DIR=<path>
```

Since the instruction selectors require target-specific information, the headers
of the available targets must be included in the custom LLVM install location.
