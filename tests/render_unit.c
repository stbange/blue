#include "control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failure;
static void *alloc_buffer(size_t n,size_t b){return failure==1?NULL:calloc(n,b);}
static BlueEngine *create_engine(int rate,int voices){return failure==2?NULL:blue_create(rate,voices);}
#define main render_main
#define calloc alloc_buffer
#define blue_create create_engine
#include "../hosts/render.c"
#undef blue_create
#undef calloc
#undef main
int main(int argc,char **argv){
    if(argc!=4)return 2;
    char *a[]={argv[0],argv[1],"Calibration",argv[2],argv[3]};
    if(render_main(1,a)!=2 || render_main(4,a))return 1;
    FILE *f=fopen(argv[3],"w");if(!f)return 1;
    fputs("48000 319\n17 0 69 0.9\n279 1 69 0\n",f);fclose(f);
    if(render_main(5,a))return 1;
    f=fopen(argv[3],"w");if(!f)return 1;fputs("48000 250\n3 6 1 0.4\n2 0 60 1\n",f);fclose(f);
    if(render_main(5,a)!=2)return 1;remove(argv[3]);if(render_main(5,a)!=2)return 1;
    failure=1;if(render_main(4,a)!=1)return 1;failure=2;if(render_main(4,a)!=1)return 1;
    failure=0;a[2]="unknown";return render_main(4,a)!=1;
}
