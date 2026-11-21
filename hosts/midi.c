#include "midi.h"
#include "blue.h"
#include "portmidi.h"
static PortMidiStream *input;
int blue_midi_open(void) {
  if (Pm_Initialize() != pmNoError)
    return 0;
  PmDeviceID id = Pm_GetDefaultInputDeviceID();
  return id != pmNoDevice &&
         Pm_OpenInput(&input, id, NULL, 256, NULL, NULL) == pmNoError;
}
void blue_midi_read(BlueMidiSink sink, void *user) {
  if (!input)
    return;
  PmEvent list[64];
  int count = Pm_Read(input, list, 64);
  if (count < 0) {
    sink(user, PANIC, 0, 0);
    return;
  }
  for (int i = 0; i < count; i++) {
    PmMessage m = list[i].message;
    int a = Pm_MessageData1(m), b = Pm_MessageData2(m);
    switch (Pm_MessageStatus(m) >> 4) {
    case 8:
      sink(user, NOTE_OFF, a, 0);
      break;
    case 9:
      sink(user, b ? NOTE_ON : NOTE_OFF, a, b / 127.f);
      break;
    case 10:
      sink(user, PRESSURE, a, b / 127.f);
      break;
    case 11:
      if (a == 64)
        sink(user, PEDAL, 0, b / 127.f);
      else if (a == 1)
        sink(user, WHEEL, 0, b / 127.f);
      else if (a == 120 || a == 123)
        sink(user, PANIC, 0, 0);
      break;
    case 13:
      sink(user, PRESSURE, 0, a / 127.f);
      break;
    case 14:
      sink(user, BEND, 0, ((b * 128 + a) - 8192) / 4096.f);
      break;
    }
  }
}
void blue_midi_close(void) {
  if (input)
    Pm_Close(input);
  input = NULL;
  Pm_Terminate();
}
