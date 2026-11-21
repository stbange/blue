#include "blue.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const BluePatch *author_load(const char *,const char *);
const char *author_error(void),*author_label(int);
int author_size(void),audio_size(void),audio_init(int),audio_commit(void);
void audio_event(int,int,float),audio_render(int);
float *audio_left(void),*audio_right(void);
BluePatch *audio_stage(void);
int main(int argc,char **argv){
    if(argc!=2 || audio_init(200) || !audio_init(48000))return 1;
    if(author_load(argv[1],"missing") || !strstr(author_error(),"missing"))return 1;
    const BluePatch *p=author_load(argv[1],"Calibration");
    if(!p || strcmp(author_label(0),"Motion") || author_label(-1)[0] || audio_size()!=author_size())return 1;
    memcpy(audio_stage(),p,author_size());if(!audio_commit())return 1;
    audio_event(NOTE_ON,69,1);audio_render(128);
    double energy=0;for(int i=0;i<128;i++){if(audio_left()[i]!=audio_right()[i])return 1;energy+=fabs(audio_left()[i]);}
    return energy<1;
}
