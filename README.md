# Build

```
mkdir build
cd build
cmake ..
make
```

# Run the optimization pass
Go to the project directory.
```
clang -emit-llvm -S test.c -Xclang -disable-O0-optnone -o test.ll
opt -disable-output -load-pass-plugin=./build/LoopPerforationPass/LoopPerforationPass.so -passes="count-freq","loop-perf" test.ll
```

# Compile and run the executable 
clang test.ll -o exec_name
./exec_name





