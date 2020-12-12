#include <stdio.h>
#include <stdlib.h>
#include "grpwk20.h"
#define rep(i,n) for(int (i)=0;(i)<(n);(i)++)
#define repp(i,m,n) for(int (i)=(m);(i)<(n);(i)++)
#define repm(i,n) for(int (i)=(n-1);(i)>=0;(i)--)

int stonum(char a){
  if(a=='A')return 0;
  if(a=='G')return 1;
  if(a=='C')return 2;
  if(a=='T')return 3;
  else return 2;
}
   char idx[14];
   char real[11];
int num[4][100500];
int pow4[10];
int candi[20000];
int idxcand=0;
int count4=0;
int count=0;
int count2=0;
void dfs(char x[],int j,int num){
  if(j==12){
      candi[idxcand]=num+stonum(x[j])*pow4[6-j/2];
      idxcand++;
      if(x[j]!=x[j+1]){
        candi[idxcand]=num+stonum(x[j+1])*pow4[6-j/2];
        idxcand++;
      }
    return ;
  }
  dfs(x,j+2,num+stonum(x[j])*pow4[6-j/2]);
  if(x[j]!=x[j+1])dfs(x,j+2,num+stonum(x[j+1])*pow4[6-j/2]);
  }


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
    int t=1;
    rep(i,10){
      pow4[i]=t;
      t*=4;
    }
   rep(kai,3){
   rep(i,9091){
     rep(j,14)idx[j]=getc(sfp);
     rep(o,11)real[o]=getc(sfp);
     idxcand=0;
     dfs(idx,0,0);
     if(idxcand==4)count4++;
     if(idxcand==1)count++;
     if(idxcand==2)count2++;
    // if(!i)printf("%d\n",idxcand);
     rep(j,idxcand){
       int g=candi[j];
       //if(!i)printf("%d\n",g);
       if(idxcand>15)break;
       if(g>9091)continue;
       rep(k,11){
         int h=stonum(real[k]);
         if(idxcand==1){
           num[h][11*g+k]+=1000;
         }
         else if(idxcand==2)num[h][11*g+k]+=500;
         else if(idxcand==4)num[h][11*g+k]+=100;
         else num[h][11*g+k]+=50;      
       }
     }

   }
    getc(sfp);
   }
   printf("%d %d %d\n",count,count2,count4);
    rep(i,100000){
        int comp=-1;
        int ans=0;
        rep(j,4){
         // if(i==0)printf("%d\n",num[j][i]);
          if(num[j][i]>comp){
            ans=j;
            comp=num[j][i];
          }
        }
        int ans1=ans%2;
        ans/=2;
        int ans2=ans%2;
        fprintf(dfp,"%d%d",ans2,ans1);
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
