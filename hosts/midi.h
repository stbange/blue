#ifndef BLUE_MIDI_H
#define BLUE_MIDI_H
typedef void (*BlueMidiSink)(void *, int, int, float);
int blue_midi_open(void);
void blue_midi_read(BlueMidiSink sink, void *user);
void blue_midi_close(void);
#endif
