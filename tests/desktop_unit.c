/* Unit driver for the native host. Device callbacks, the event source and clock
   are supplied by the test. Production DSP and physical I/O get separate tests. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static SDL_AudioSpec requested;
static int mx,my,scenario,position,between,saw_audio,errors;
static const char *settings;
static SDL_AudioDeviceID device_open(const char *name,int capture,const SDL_AudioSpec *in,SDL_AudioSpec *out,int flags){
    (void)name;(void)capture;(void)flags;requested=*in;*out=*in;
    if(scenario==1)return 0;if(scenario==2)out->freq=0;return 3;
}
static void clock_tick(unsigned ms){
    (void)ms;float audio[256*2];if(requested.callback){
        requested.callback(requested.userdata,(Uint8 *)audio,sizeof audio);
        for(unsigned k=0;k<512;k++)if(fabsf(audio[k])>.0001f)saw_audio=1;
    }
}
static Uint32 mouse_position(int *x,int *y){*x=mx;*y=my;return SDL_BUTTON_LMASK;}
static void device_noop(SDL_AudioDeviceID id){(void)id;}
static void pause_noop(SDL_AudioDeviceID id,int paused){(void)id;(void)paused;}
static int virtual_devices(int capture){return capture?0:1;}
static const char *device_name(int i,int capture){(void)i;(void)capture;return "unit output";}
static char *settings_path(const char *org,const char *app){(void)org;(void)app;return SDL_strdup(settings);}
static int next_event(SDL_Event *ev){
    if(between){between=0;return 0;}between=1;
    memset(ev,0,sizeof *ev);
    static const SDL_Keycode actions[]={SDLK_LEFT,SDLK_r,SDLK_h,SDLK_h,SDLK_r,SDLK_F5,SDLK_RIGHT,SDLK_F9,SDLK_z,SDLK_F2,SDLK_F3,SDLK_F4,SDLK_END,SDLK_HOME,SDLK_DOWN,SDLK_UP,SDLK_RIGHT,SDLK_LEFT,SDLK_s,SDLK_ESCAPE,SDLK_F6,SDLK_RETURN,SDLK_BACKSPACE,SDLK_DELETE,SDLK_ESCAPE,SDLK_RIGHTBRACKET,SDLK_LEFTBRACKET};
    if(position<(int)(sizeof actions/sizeof *actions)){
        ev->type=position==3?SDL_KEYUP:SDL_KEYDOWN;ev->key.keysym.sym=actions[position];
        if(ev->key.keysym.sym==SDLK_s)ev->key.keysym.mod=KMOD_CTRL;
    }else switch(position-(int)(sizeof actions/sizeof *actions)){
        case 0:ev->type=SDL_MOUSEBUTTONDOWN;mx=400;my=200;break;
        case 1:ev->type=SDL_MOUSEMOTION;ev->motion.state=SDL_BUTTON_LMASK;mx=500;my=240;break;
        case 2:ev->type=SDL_MOUSEBUTTONDOWN;mx=150;my=320;break;
        case 3:ev->type=SDL_MOUSEBUTTONUP;break;
        case 4:ev->type=SDL_WINDOWEVENT;ev->window.event=SDL_WINDOWEVENT_FOCUS_LOST;break;
        case 5:ev->type=SDL_AUDIODEVICEREMOVED;ev->adevice.which=3;break;
        default:ev->type=SDL_QUIT;break;
    }
    position++;return 1;
}
static SDL_Window *test_window(const char *label,int x,int y,int w,int h,Uint32 flags){
  return scenario==3?NULL:SDL_CreateWindow(label,x,y,w,h,flags);
}
static char *test_base(void){return scenario==4?SDL_strdup("absent-dir/"):SDL_GetBasePath();}
static int test_init(Uint32 subsystems){return scenario==5?-1:SDL_Init(subsystems);}
#define SDL_CreateWindow test_window
#define SDL_GetBasePath test_base
#define SDL_Init test_init
#define SDL_PollEvent next_event
#define SDL_GetMouseState mouse_position
#define SDL_OpenAudioDevice device_open
#define SDL_CloseAudioDevice device_noop
#define SDL_LockAudioDevice device_noop
#define SDL_UnlockAudioDevice device_noop
#define SDL_PauseAudioDevice pause_noop
#define SDL_GetNumAudioDevices virtual_devices
#define SDL_GetAudioDeviceName device_name
#define SDL_GetPrefPath settings_path
#define SDL_Delay clock_tick
#define main desktop_main
#include "../hosts/desktop.c"
#undef main
void blue_midi_close(void){}
int blue_midi_open(void){return 1;}
void blue_midi_read(BlueMidiSink sink,void *ctx){if(position==0)sink(ctx,WHEEL,0,.25f);}
int main(int argc,char **argv){
    if(argc!=4)return 2;
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);settings=argv[3];
    char *args[]={argv[0],"--content-root",argv[1],"--record-to",argv[2]};
    if(desktop_main(5,args)!=0 || !saw_audio)return 1;
    FILE *wav=fopen(argv[2],"rb");if(!wav)return 1;char id[4];int valid=fread(id,1,4,wav)==4 && !memcmp(id,"RIFF",4);fclose(wav);
    if(!valid)return 1;
    SDL_Init(SDL_INIT_VIDEO);
    BluePreset preset={0};snprintf(preset.folder,sizeof preset.folder,"%s/packs/blue.factory",argv[1]);strcpy(preset.name,"Calibration");
    BlueEditor ed={0};if(!editor_open(&ed,&preset,1))return 1;
    editor_key(&ed,SDLK_UP,0);editor_key(&ed,SDLK_BACKSPACE,0);
    editor_key(&ed,SDLK_c,KMOD_CTRL);editor_key(&ed,SDLK_v,KMOD_CTRL);
    ed.cursor=ed.length;editor_key(&ed,SDLK_DELETE,0);editor_key(&ed,SDLK_UP,0);editor_key(&ed,SDLK_DOWN,0);
    char large[2000];memset(large,' ',sizeof large-1);large[sizeof large-1]=0;editor_input(&ed,large);
    size_t before=ed.length;char *huge=malloc(1048578);memset(huge,'x',1048577);huge[1048577]=0;editor_input(&ed,huge);free(huge);if(ed.length!=before)return 1;
    BluePatch *patch=malloc(sizeof *patch);char why[2048];
    editor_input(&ed,"=broken);");if(editor_save(&ed,&preset,patch,why,sizeof why))return 1;
    if(!editor_open(&ed,&preset,1))return 1;strcpy(ed.path,"missing-parent/path.lua");
    if(editor_save(&ed,&preset,patch,why,sizeof why))return 1;
    strcpy(preset.name,"missing");if(editor_open(&ed,&preset,0))return 1;
    editor_close(&ed);free(patch);SDL_Quit();
    App a={0};a.device=3;a.rate=48000;a.engine=blue_create(48000,2);a.count=1;a.catalog[0]=preset;
    load(&a,0);if(!strstr(a.message,"missing"))return 1;
    strcpy(a.catalog[0].name,"Calibration");load(&a,0);blue_event(a.engine,NOTE_ON,69,.8);
    load(&a,0);load(&a,0);if(!a.pending)return 1;
    blue_event(a.engine,PANIC,0,0);float left[2048],right[2048];for(int i=0;i<16;i++)blue_render(a.engine,left,right,2048);
    apply_pending(&a);if(a.pending)return 1;blue_destroy(a.engine);
    for(scenario=1;scenario<=5;scenario++){if(!desktop_main(5,args))return 1;SDL_Quit();}
    args[2]="absent-pack";if(!desktop_main(5,args))return 1;SDL_Quit();
    return errors;
}
