#include <stdio.h>

int foo(int z){
    printf("%d \n", z);
    return 1;
}

int bar(int y){
    printf("%d \n", y);
    return 2;
}

int main()
{

  int in[100]; 
  int i,j;

  foo(24);
  for (i = 0; i < 10; i++)
  {
    in[i] = i;
  }   

  for (j = 10; j < 100; j++)
  {
   in[j] = i;
  }

  bar(2023);
  for (i = 0; i< 100; i++)
    fprintf(stdout,"%d \n", in[i]);
  
  return 1;
}
