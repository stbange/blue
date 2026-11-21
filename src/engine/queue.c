#include "blue_queue.h"
void blue_queue_push(BlueQueue *q, int command, int id, float value) {
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
  q->events[q->used++] = (BlueEvent){0, command, id, value};
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
