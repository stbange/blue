#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "control.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define RATE 48000
#define RECORD_FRAMES (60*RATE)
typedef struct {
    BlueEngine *engine;SDL_AudioDeviceID device;float *recording;
    int capturing,recorded,rate,selected;float macros[8];
    const char *folder,*wav;char message[2048];
} App;
static const char *presets[]={"Glass Tide","Struck Wire","Paper Cloud","Step Current","Calibration"};
static const char keys[]="awsedftgyhujk";
static void callback(void *ud,Uint8 *stream,int bytes){
    App *a=ud;float *out=(float *)stream;int frames=bytes/(2*sizeof(float));
    for(int pos=0;pos<frames;){
        int n=frames-pos>256?256:frames-pos;float l[256],r[256];blue_render(a->engine,l,r,n);
        for(int i=0;i<n;i++){out[(pos+i)*2]=l[i];out[(pos+i)*2+1]=r[i];}
        if(a->capturing && a->recorded+n<=RECORD_FRAMES){
            memcpy(a->recording+2*a->recorded,out+pos*2,n*2*sizeof(float));a->recorded+=n;
        }
        pos+=n;
    }
}
static void event(App *a,int c,int key,float v){SDL_LockAudioDevice(a->device);blue_event(a->engine,c,key,v);SDL_UnlockAudioDevice(a->device);}
static void load(App *a,int which){
    BluePatch *p=malloc(sizeof *p);if(!p)return;
    if(blue_load(p,a->folder,presets[which],a->message,sizeof a->message)){
        SDL_LockAudioDevice(a->device);BluePatch *slot=blue_stage(a->engine);SDL_UnlockAudioDevice(a->device);
        if(slot){
            *slot=*p;SDL_LockAudioDevice(a->device);blue_commit(a->engine);SDL_UnlockAudioDevice(a->device);
            a->selected=which;snprintf(a->message,sizeof a->message,"Loaded %s",presets[which]);
            for(int k=0;k<8;k++)event(a,MACRO,k,a->macros[k]);
        }else snprintf(a->message,sizeof a->message,"Release held notes and reload.");
    }
    free(p);
}
static void capture(App *a){
    SDL_LockAudioDevice(a->device);int save=a->capturing;a->capturing=!save;if(!save)a->recorded=0;SDL_UnlockAudioDevice(a->device);
    if(save)snprintf(a->message,sizeof a->message,"%s",blue_write_wav(a->wav,a->recording,a->recorded,a->rate)?"Recording saved":"Recording could not be written");
}
static void rect(SDL_Renderer *r,int x,int y,int w,int h,int red,int green,int blue){
    SDL_Rect b={x,y,w,h};SDL_SetRenderDrawColor(r,red,green,blue,255);SDL_RenderFillRect(r,&b);
}
static void label(SDL_Renderer *r,SDL_Texture *font,int x,int y,const char *str,int scale){
    for(int i=0;str[i] && i<100;i++){int c=(unsigned char)str[i];if(c<32||c>126)c='?';
        SDL_Rect src={(c-32)*8,0,8,14},dst={x+i*8*scale,y,8*scale,14*scale};SDL_RenderCopy(r,font,&src,&dst);}
}
static void draw(App *a,SDL_Renderer *r,SDL_Texture *font){
    rect(r,0,0,920,480,13,22,30);label(r,font,28,18,"BLUE",3);
    label(r,font,28,75,"HYBRID SYNTHESIZER",1);rect(r,27,108,866,50,36,65,74);
    label(r,font,46,122,presets[a->selected],2);label(r,font,610,126,"LEFT / RIGHT   Change preset",1);
    for(int k=0;k<2;k++){
        int y=185+k*47;label(r,font,30,y,k?"Level trim":"Motion",2);
        rect(r,270,y,610,23,42,62,74);rect(r,270,y,(int)(610*a->macros[k]),23,92,201,192);
    }
    for(int k=0;k<13;k++){
        int dark=strchr("wetyu",keys[k])!=NULL;
        rect(r,28+66*k,295,62,78,dark?53:160,dark?85:190,dark?94:192);
        char s[2]={keys[k],0};label(r,font,48+66*k,324,s,2);
    }
    label(r,font,28,390,"Z  Reload pack      R  Record / save      F5  Save controls      F9  Recall",1);
    label(r,font,28,425,a->capturing?"Recording master ...":a->message,1);SDL_RenderPresent(r);
}
static void state(App *a,int save){
    char *dir=SDL_GetPrefPath("Blue","Instrument");if(!dir)return;
    char path[2048];snprintf(path,sizeof path,"%scontrols.txt",dir);SDL_free(dir);
    FILE *f=fopen(path,save?"w":"r");if(!f)return;
    if(save)fprintf(f,"%d %.9g %.9g\n",a->selected,a->macros[0],a->macros[1]);
    else {int s;float m,n;if(fscanf(f,"%d %f %f",&s,&m,&n)==3 && s>=0 && s<5 && m>=0 && m<=1 && n>=0 && n<=1){a->macros[0]=m;a->macros[1]=n;load(a,s);}}
    fclose(f);
}
int main(int argc,char **argv){
    SDL_SetMainReady();if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO))return 1;
    char *base=SDL_GetBasePath();char root[2048],pack[2048],atlas[2048];
    snprintf(root,sizeof root,"%scontent",base?base:"");SDL_free(base);
    App a={0};a.wav="Blue-recording.wav";
    for(int i=1;i+1<argc;i+=2){
        if(!strcmp(argv[i],"--content-root"))snprintf(root,sizeof root,"%s",argv[i+1]);
        else if(!strcmp(argv[i],"--record-to"))a.wav=argv[i+1];
    }
    snprintf(pack,sizeof pack,"%s/packs/blue.factory",root);a.folder=pack;
    SDL_Window *win=SDL_CreateWindow("Blue | Hybrid synthesizer",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,920,480,SDL_WINDOW_SHOWN);
    SDL_Renderer *r=win?SDL_CreateRenderer(win,-1,0):NULL;if(!r){fprintf(stderr,"%s\n",SDL_GetError());return 1;}
    snprintf(atlas,sizeof atlas,"%s/graphics/font.bmp",root);SDL_Surface *bmp=SDL_LoadBMP(atlas);if(!bmp){fprintf(stderr,"%s\n",atlas);return 1;}
    SDL_SetColorKey(bmp,SDL_TRUE,SDL_MapRGB(bmp->format,0,0,0));SDL_Texture *font=SDL_CreateTextureFromSurface(r,bmp);SDL_FreeSurface(bmp);
    SDL_AudioSpec want={0},have;want.freq=RATE;want.format=AUDIO_F32SYS;want.channels=2;want.samples=256;want.callback=callback;want.userdata=&a;
    a.device=SDL_OpenAudioDevice(NULL,0,&want,&have,SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if(!a.device){fprintf(stderr,"%s\n",SDL_GetError());return 1;}a.rate=have.freq;a.engine=blue_create(a.rate,16);
    a.recording=calloc(RECORD_FRAMES*2,sizeof(float));if(!a.recording || !a.engine)return 1;
    load(&a,0);SDL_PauseAudioDevice(a.device,0);
    int quit=0,mouse=-1;
    while(!quit){
        SDL_Event ev;while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT)quit=1;
            if(ev.type==SDL_WINDOWEVENT && ev.window.event==SDL_WINDOWEVENT_FOCUS_LOST)event(&a,PANIC,0,0);
            if((ev.type==SDL_KEYDOWN||ev.type==SDL_KEYUP)&&!ev.key.repeat){
                int down=ev.type==SDL_KEYDOWN;SDL_Keycode k=ev.key.keysym.sym;
                const char *p=k>0 && k<128?strchr(keys,(int)k):NULL;
                if(p)event(&a,down?NOTE_ON:NOTE_OFF,60+(int)(p-keys),.85f);
                else if(down){if(k==SDLK_ESCAPE)quit=1;else if(k==SDLK_LEFT||k==SDLK_RIGHT)load(&a,(a.selected+(k==SDLK_RIGHT?1:4))%5);
                    else if(k==SDLK_z)load(&a,a.selected);else if(k==SDLK_r)capture(&a);else if(k==SDLK_F5||k==SDLK_F9)state(&a,k==SDLK_F5);}
            }
            if(ev.type==SDL_MOUSEBUTTONDOWN || (ev.type==SDL_MOUSEMOTION && ev.motion.state&SDL_BUTTON_LMASK)){
                int x,y;SDL_GetMouseState(&x,&y);
                if(x>=270 && x<=880 && y>=185 && y<255){int k=y>=232;a.macros[k]=(x-270)/610.f;event(&a,MACRO,k,a.macros[k]);}
                else if(ev.type==SDL_MOUSEBUTTONDOWN && y>=295 && y<373 && x>=28 && x<886){mouse=60+(x-28)/66;event(&a,NOTE_ON,mouse,.85f);}
            }
            if(ev.type==SDL_MOUSEBUTTONUP && mouse>=0){event(&a,NOTE_OFF,mouse,0);mouse=-1;}
        }
        draw(&a,r,font);SDL_Delay(16);
    }
    if(a.capturing)capture(&a);SDL_CloseAudioDevice(a.device);blue_destroy(a.engine);free(a.recording);
    SDL_DestroyTexture(font);SDL_DestroyRenderer(r);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
