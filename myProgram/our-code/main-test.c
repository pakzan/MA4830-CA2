#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define NO_POINT  100

void f_WaveGen(int * sine){

  int i;
  float dummy;

  //Sine wave array
  float delta = (2.0*3.142)/NO_POINT;
  for(i=0; i<NO_POINT-1; i++){
    dummy = (sinf((float)(i*delta))+1)*0xf000;
    sine[i] = (unsigned) dummy;
  }

  //Square wave array
  //Triangular wave array
  //Saw-tooth wave array
}

int main(){
  int i;
  static unsigned int sine_wave[NO_POINT];

  f_WaveGen(sine_wave);

  for(i=0;i<NO_POINT-1;i++){
    printf("%d\n",sine_wave[i]);
  }

  printf("Successful");

  return(0);

}
