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
  char prev='X';
  for(int i=0; i<ORGDATA_LEN; i++){
    c1 = getc(ofp);
    if(c1=='0')rep(j,5)fputc('A',efp);
    else rep(j,5)fputc('T',efp);
    fputc('G',efp);
    fputc('C',efp);
    fputc('G',efp);
  }
  fputc('\n',efp);
  fclose(ofp);
  fclose(efp);
  return(0);
}

int main(){
  enc();
  return(0);
}
