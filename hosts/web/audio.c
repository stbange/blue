#include "blue.h"
#include "blue_queue.h"
static BlueQueue queue;
static BlueEngine *engine;
static float left[2048], right[2048];
int audio_init(int rate) {
  engine = blue_create(rate, 16);
  return engine != 0;
}
void audio_event(int c, int i, float v) { blue_queue_push(&queue, c, i, v); }
void audio_render(int n) {
  blue_queue_drain(&queue, engine);
  blue_render(engine, left, right, n);
}
float *audio_left(void) { return left; }
float *audio_right(void) { return right; }
BluePatch *audio_stage(void) { return blue_stage(engine); }
int audio_commit(void) { return blue_commit(engine); }
int audio_size(void) { return sizeof(BluePatch); }
