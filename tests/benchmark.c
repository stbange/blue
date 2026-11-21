#include "control.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
static double seconds(void){struct timespec now;timespec_get(&now,TIME_UTC);return now.tv_sec+now.tv_nsec/1e9;}
static int order(const void *p,const void *q){double a=*(const double*)p,b=*(const double*)q;return (a>b)-(a<b);}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    int duration=atoi(argv[2]);if(duration<1 || duration>600)return 2;
    const int rate=48000,block=128,blocks=duration*rate/block;
    BlueEngine *e=blue_create(rate,16);char error[2048];
    BluePatch *p=blue_stage(e);if(!blue_load(p,argv[1],"Glass Tide",error,sizeof error)){puts(error);return 1;}
    for(int i=0;i<3;i++){p->lane[i].insert[0]=LOWPASS;p->lane[i].send=.1f;p->lane[i].space=.1f;}
    p->lane[2].density=80;p->lane[2].size=.1f;
    blue_commit(e);for(int i=0;i<16;i++)blue_event(e,NOTE_ON,44+i,.65f);
    double *cost=calloc(blocks,sizeof *cost);if(!cost)return 1;
    int silent=0;double peak=0;float l[128],r[128];
    for(int b=0;b<blocks;b++){
        double start=seconds();blue_render(e,l,r,block);cost[b]=seconds()-start;
        double energy=0;for(int i=0;i<128;i++){energy+=l[i]*l[i]+r[i]*r[i];peak=fmax(peak,fabs(l[i]));}
        if(!isfinite(energy)||!energy)silent++;
    }
    qsort(cost,blocks,sizeof *cost,order);double p99=cost[(int)(.99*(blocks-1))];
    printf("rendered_seconds=%d voices=16 sample_rate=48000 block=128 p99_ms=%.4f deadline_ms=%.4f silent_or_invalid_blocks=%d peak=%.4f\n",duration,p99*1000,128000./rate,silent,peak);
    free(cost);blue_destroy(e);return silent || p99> .7*block/rate;
}
