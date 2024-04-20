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
```
clang test.ll -o exec_name
./exec_name
```

# Time profiling
```
time ./exec_name
```


# Test Accuracy
The first argument is the benchmark name, and the second + third arguments are the output files of the executable. This python script would read from the file and compare with output shown. So please have the files containing the outoput of the executable before running this.

```
python3 accuracy_driver.py sum_to_n result_sum_to_n_1 result_sum_to_n_2
```


