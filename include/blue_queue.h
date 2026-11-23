#ifndef BLUE_QUEUE_H
#define BLUE_QUEUE_H
#include "blue.h"
#define BLUE_QUEUE_CAPACITY 256
/* The host serializes push and drain. Both have fixed upper bounds. */
typedef struct {
  BlueEvent events[BLUE_QUEUE_CAPACITY];
  unsigned used, coalesced, dropped;
  int emergency;
} BlueQueue;
void blue_queue_at(BlueQueue *queue, uint64_t frame, int command, int id,
                   float value);
void blue_queue_render(BlueQueue *queue, BlueEngine *engine, float *left,
                       float *right, int frames);
void blue_queue_push(BlueQueue *queue, int command, int id, float value);
void blue_queue_drain(BlueQueue *queue, BlueEngine *engine);
#endif
