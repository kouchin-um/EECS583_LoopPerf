# Build

```
mkdir build
cd build
cmake ..
make
```

# Test with the Wrapped Running Script
```
cd benchmark
sh run.sh sum-to-n/sum-to-n
sh run.sh mat-mul/mat-mul
```

# Visualize the result
Please comment out the rm in run.sh first.
```
sh viz.sh sum-to-n/sum-to-n.fplicm
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


# Example of testing mat-mul:
After building, run this
```
clang -emit-llvm -S benchmark/mat-mul/mat-mul.c  -Xclang -disable-O0-optnone -o benchmark/mat-mul/mat-mul.ll
opt -disable-output -load-pass-plugin=./build/LoopPerforationPass/LoopPerforationPass.so -passes="count-freq","loop-perf" benchmark/mat-mul/mat-mul.ll
clang benchmark/mat-mul/mat-mul.ll -o benchmark/mat-mul/exec_name
cd benchmark/mat-mul/
./exec_name 
cd ../..
python3 accuracy_driver.py mat-mul benchmark/mat-mul/C.txt benchmark/mat-mul/C1.txt
``` 
(C.txt is the ground truth result and C1 is the perforated result)