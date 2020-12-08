#include <stdio.h>
#include <stdlib.h>
#include "grpwk20.h"

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

  unsigned char c, res;
  while((c = getc(sfp)) != '\n'){
    switch(c){
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
    fputc((res>>1)+'0', dfp);
    fputc((res&0x1)+'0', dfp);    
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
