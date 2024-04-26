clang -emit-llvm -S benchmark/mat-mul/mat-mul.c  -Xclang -disable-O0-optnone -o benchmark/mat-mul/mat-mul.ll
opt -disable-output -load-pass-plugin=./build/LoopPerforationPass/LoopPerforationPass.so -passes="count-freq","loop-perf" benchmark/mat-mul/mat-mul.ll
clang benchmark/mat-mul/mat-mul.ll -o benchmark/mat-mul/exec_name
cd benchmark/mat-mul/
./exec_name 
cd ../..
python3 accuracy_driver.py mat-mul benchmark/mat-mul/C.txt benchmark/mat-mul/C1.txt