#include "blue_queue.h"
#include <string.h>
void blue_queue_at(BlueQueue *q, uint64_t frame, int command, int id,
                   float value) {
  int release = command == PANIC || command == NOTE_OFF ||
                (command == NOTE_ON && value == 0) ||
                (command == PEDAL && value < .5f);
  if (!release && q->used >= BLUE_QUEUE_CAPACITY - 16) {
    if (command == MACRO || command == WHEEL || command == PRESSURE ||
        command == BEND || command == POSITION || command == DISTANCE) {
      for (unsigned k = q->used; k > 0; k--) {
        BlueEvent *old = &q->events[k - 1];
        if (old->command == command && old->id == id) {
          old->value = value;
          q->coalesced++;
          return;
        }
      }
    }
    q->dropped++;
    return;
  }
  if (q->used == BLUE_QUEUE_CAPACITY) {
    q->emergency = 1;
    q->dropped++;
    return;
  }
  unsigned i = q->used++;
  while (i && q->events[i - 1].frame > frame) {
    q->events[i] = q->events[i - 1];
    i--;
  }
  q->events[i] = (BlueEvent){frame, command, id, value};
}
void blue_queue_push(BlueQueue *q, int command, int id, float value) {
  blue_queue_at(q, 0, command, id, value);
}
void blue_queue_render(BlueQueue *q, BlueEngine *e, float *l, float *r, int n) {
  if (q->emergency) {
    q->used = 0;
    q->emergency = 0;
    blue_event(e, PANIC, 0, 0);
  }
  uint64_t end = blue_clock(e) + (unsigned)n;
  unsigned due = 0;
  while (due < q->used && q->events[due].frame < end)
    due++;
  blue_timeline(e, l, r, n, q->events, due);
  if (due) {
    q->used -= due;
    memmove(q->events, q->events + due, q->used * sizeof *q->events);
  }
}
void blue_queue_drain(BlueQueue *q, BlueEngine *e) {
  for (unsigned i = 0; i < q->used; i++) {
    BlueEvent *event = &q->events[i];
    blue_event(e, event->command, event->id, event->value);
  }
  q->used = 0;
  if (q->emergency) {
    blue_event(e, PANIC, 0, 0);
    q->emergency = 0;
  }
}
