# Build

```
mkdir build
cd build
cmake ..
make
```

# Run
Go to the project directory.
```
clang -emit-llvm -S test.c -Xclang -disable-O0-optnone -o test.ll
opt -disable-output -load-pass-plugin=./build/LoopPerforationPass/LoopPerforationPass.so -passes="count-freq","loop-perf" test.ll
```
