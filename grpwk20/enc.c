#include <stdio.h>
#include <stdlib.h>
#include "grpwk20.h"
#define N 20

int enc(){
  FILE *ofp;
  if((ofp = fopen(ORGDATA, "r")) ==NULL){
    fprintf(stderr, "cannot open %s\n", ORGDATA);
    exit(1);
  }

  FILE *efp;
  if((efp = fopen(ENCDATA, "w")) ==NULL){
    fprintf(stderr, "cannot open %s\n", ENCDATA);
    exit(1);
  }

  unsigned char c1, c2, res;
  for(int i=0; i<ORGDATA_LEN; i+=2){
    c1 = getc(ofp);
    c2 = getc(ofp);

    switch( ( (c1 & 0x1) << 7) >> 6 | ( c2 & 0x1) ){
    case 0:
      res = BASE_A;
      break;
    case 1:
      res = BASE_C;
      break;
    case 2:
      res = BASE_G;
      break;
    case 3:
      res = BASE_T;
      break;
    }
    for (size_t j = 0; j < N; j++) fputc(res, efp);
  }
  res = '\n';
  fputc(res, efp);


  fclose(ofp);
  fclose(efp);
  return(0);
}

int main(){
  enc();
  return(0);
}
