#include <stdio.h>
#include <stdlib.h>
#include "grpwk20.h"
#define N 20

int dec(){
  FILE *sfp;
  if((sfp = fopen(SEQDATA, "r")) ==NULL){
    fprintf(stderr, "cannot open %s\n", SEQDATA);
    exit(1);
  }

  FILE *dfp;
  if((dfp = fopen(DECDATA, "w")) ==NULL){
    fprintf(stderr, "cannot open %s\n", DECDATA);
    exit(1);
  }

  char c1, c2;
  unsigned char res;
  int count;
  c1 = getc(sfp);
  while (1) {
    count = 1;
    while(1) {
      c2 = getc(sfp);
      if(c2 == '\n') c2 = getc(sfp);
      if(c2 == c1) count++;
      else break;
    }
    switch(c1){
      case BASE_A:
      res = 0;
      break;
      case BASE_C:
      res = 1;
      break;
      case BASE_G:
      res = 2;
      break;
      case BASE_T:
      res = 3;
      break;
      default:
      res = 0;
      break;
    }
    for (size_t i = 0; i < (int)((double)count/N+0.5); i++) {
      fputc((res>>1)+'0', dfp);
      fputc((res&0x1)+'0', dfp);
    }
    if(c2 == EOF) break;
    c1 = c2;
  }
  res = '\n';
  fputc(res, dfp);

  fclose(sfp);
  fclose(dfp);
  return(0);
}

int main(){
  dec();
  return(0);
}
