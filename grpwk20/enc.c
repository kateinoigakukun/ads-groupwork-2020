#include <stdio.h>
#include <stdlib.h>
#include "grpwk20.h"
#define rep(i,n) for(int (i)=0;(i)<(n);(i)++)
#define repp(i,m,n) for(int (i)=(m);(i)<(n);(i)++)
#define repm(i,n) for(int (i)=(n-1);(i)>=0;(i)--)

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
  int j=0;
  int dig[7];
  for(int i=0; i<ORGDATA_LEN; i+=2){
    if(i%22==0){
      int k=j;
      rep(i,7){
        dig[i]=k%4;
        k/=4;
      }
      j++;
      repm(i,7){
        if(dig[i]==0){
          fputc('A',efp);
          fputc('A',efp);
        }else if(dig[i]==1){
          fputc('G',efp);
          fputc('G',efp);
        }else if(dig[i]==2){
          fputc('C',efp);
          fputc('C',efp);
        }else{
          fputc('T',efp);
          fputc('T',efp);
        }
      }
    }
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
    fputc(res, efp);
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
