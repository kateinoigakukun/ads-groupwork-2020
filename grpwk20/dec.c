#include <stdio.h>
#include <stdlib.h>
#include "grpwk20.h"
#define rep(i,n) for(int (i)=0;(i)<(n);(i)++)
#define repp(i,m,n) for(int (i)=(m);(i)<(n);(i)++)
#define repm(i,n) for(int (i)=(n-1);(i)>=0;(i)--)



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
  char inp[2000000];
  int num=0;
  char c2=getc(sfp);
  while(c2!='\n'){
    inp[num]=c2;
    num++;
    c2=getc(sfp);
  }
  
  rep(i,num){
    if(i==0){
      if(inp[i]=='A')fputc('0',dfp);
      if(inp[i]=='T')fputc('1',dfp);
    }
    else{
      if(inp[i]=='A'&&inp[i-1]!='A')fputc('0',dfp);
      else if(inp[i]=='T'&&inp[i-1]!='T')fputc('1',dfp);
    }
  }
  fputc('\n',dfp);
  fclose(sfp);
  fclose(dfp);
  return(0);
}

int main(){
  dec();
  return(0);
}
