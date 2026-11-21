#ifndef BLUE_H
#define BLUE_H
#include <stdint.h>
#include <stddef.h>
#define BLUE_LANES 3
#define BLUE_POINTS 128
#define BLUE_SAMPLE 65536
#define BLUE_VOICES 24
enum BlueSource { SINE, SAW, TABLE, FM, RING, SAMPLE, GRAIN };
enum BlueInsert { BYPASS, LOWPASS, HIGHPASS, BANDPASS, SHAPE, COMB };
enum BlueCommand { NOTE_ON, NOTE_OFF, PEDAL, BEND, PRESSURE, WHEEL, MACRO, PANIC, POSITION, TEMPO };
typedef struct {
    int source, insert[2], root;
    float gain, pan, ratio, depth, cutoff, send, space, morph, density, size, position, scatter;
    float harmonic[2][16];
} BlueLane;
typedef struct { float time, value, curve; } BluePoint;
typedef struct {
    BlueLane lane[BLUE_LANES];
    float gain, attack, release, delay, feedback, reverb, echo_pitch, bpm;
    float macro[8], routes[8][4], lfo[3];
    int points, loop, sync;
    BluePoint envelope[BLUE_POINTS];
    unsigned seed;
    int sample_count, sample_rate, loop_start, loop_end;
    float sample[BLUE_SAMPLE];
} BluePatch;
typedef struct BlueEngine BlueEngine;
typedef struct { uint64_t frame; int command, id; float value; } BlueEvent;
void blue_defaults(BluePatch *p);
BlueEngine *blue_create(int rate, int voices);
void blue_destroy(BlueEngine *e);
BluePatch *blue_stage(BlueEngine *e);
int blue_commit(BlueEngine *e);
void blue_event(BlueEngine *e, int command, int id, float value);
void blue_render(BlueEngine *e, float *left, float *right, int frames);
void blue_timeline(BlueEngine *e,float *left,float *right,int frames,const BlueEvent *events,size_t count);
uint64_t blue_clock(const BlueEngine *e);
unsigned blue_late(const BlueEngine *e);
#endif
