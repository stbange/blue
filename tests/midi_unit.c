#include "portmidi.h"
#include "blue.h"
#include <string.h>
static int mode,used,commands[16];static float last;static PmEvent messages[16];
static PmError init(void){return mode==1?pmHostError:pmNoError;}
static PmDeviceID dev(void){return mode==2?pmNoDevice:1;}
static PmError open_input(PortMidiStream **s,PmDeviceID id,void *driver,int32_t n,PmTimeProcPtr time,void *info){(void)id;(void)driver;(void)n;(void)time;(void)info;*s=(void*)messages;return pmNoError;}
static int read_input(PortMidiStream *s,PmEvent *out,int32_t capacity){(void)s;(void)capacity;if(mode==3)return -1;memcpy(out,messages,used*sizeof *out);return used;}
static PmError close_input(PortMidiStream *s){(void)s;return pmNoError;}
static PmError terminate(void){return pmNoError;}
#define Pm_Initialize init
#define Pm_GetDefaultInputDeviceID dev
#define Pm_OpenInput open_input
#define Pm_Read read_input
#define Pm_Close close_input
#define Pm_Terminate terminate
#include "../hosts/midi.c"
static void collect(void *ignored,int command,int id,float val){(void)ignored;(void)id;commands[command]++;last=val;}
int main(void){
    blue_midi_read(collect,0);
    for(mode=1;mode<=2;mode++)if(blue_midi_open())return 1;
    mode=0;if(!blue_midi_open())return 1;
    unsigned status[]={0x80,0x90,0x90,0xa0,0xb0,0xb0,0xb0,0xd0,0xe0};
    unsigned key[]={69,69,69,69,64,1,123,64,0},value[]={0,64,0,32,127,100,0,0,96};
    for(used=0;used<9;used++)messages[used].message=Pm_Message(status[used],key[used],value[used]);
    blue_midi_read(collect,0);
    if(commands[NOTE_ON]!=1 || commands[NOTE_OFF]!=2 || commands[PANIC]!=1 || commands[PRESSURE]!=2 || last!=1)return 1;
    mode=3;blue_midi_read(collect,0);blue_midi_close();return commands[PANIC]!=2;
}
