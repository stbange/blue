#include "blue.h"
#include "blue_queue.h"
static BlueQueue queue;
static BlueEngine *engine;
static float left[2048], right[2048];
int audio_prepare(int rate, int voices) {
  BlueEngine *next = blue_create(rate, voices);
  if (!next)
    return 0;
  blue_destroy(engine);
  engine = next;
  queue = (BlueQueue){0};
  return 1;
}
int audio_init(int rate) { return audio_prepare(rate, 16); }
void audio_event_at(int c, int i, float v, int offset) {
  blue_queue_at(&queue,
                blue_clock(engine) + (unsigned)(offset > 0 ? offset : 0), c, i,
                v);
}
void audio_event(int c, int i, float v) { audio_event_at(c, i, v, 0); }
void audio_render(int n) { blue_queue_render(&queue, engine, left, right, n); }
float *audio_left(void) { return left; }
float *audio_right(void) { return right; }
BluePatch *audio_stage(void) { return blue_stage(engine); }
int audio_commit(void) { return blue_commit(engine); }
int audio_size(void) { return sizeof(BluePatch); }
