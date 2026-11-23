#include "blue.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const BluePatch *author_load(const char *, const char *);
const char *author_error(void), *author_label(int);
int author_size(void), audio_size(void), audio_init(int), audio_commit(void);
void audio_event(int, int, float), audio_event_at(int, int, float, int),
    audio_render(int);
int audio_prepare(int, int);
float *audio_left(void), *audio_right(void);
BluePatch *audio_stage(void);
int main(int argc, char **argv) {
  if (argc != 2 || audio_init(200) || !audio_init(48000))
    return 1;
  if (author_load(argv[1], "missing") || !strstr(author_error(), "missing"))
    return 1;
  const BluePatch *p = author_load(argv[1], "Calibration");
  if (!p || strcmp(author_label(0), "Motion") || author_label(-1)[0] ||
      audio_size() != author_size())
    return 1;
  memcpy(audio_stage(), p, author_size());
  if (!audio_commit())
    return 1;
  audio_event(NOTE_ON, 69, 1);
  audio_render(128);
  double energy = 0;
  for (int i = 0; i < 128; i++) {
    if (audio_left()[i] != audio_right()[i])
      return 1;
    energy += fabs(audio_left()[i]);
  }
  if (energy < 1 || !audio_prepare(48000, 8))
    return 1;
  BluePatch *bank = audio_stage();
  memcpy(bank, p, sizeof *p);
  bank->attack = bank->release = 0;
  if (!audio_commit())
    return 1;
  audio_event_at(NOTE_OFF, 69, 0, 150);
  audio_event_at(NOTE_ON, 69, 1, 140);
  audio_render(128);
  for (int i = 0; i < 128; i++)
    if (audio_left()[i] != 0)
      return 1;
  audio_render(64);
  for (int i = 0; i < 64; i++)
    if ((audio_left()[i] != 0) != (i > 12 && i < 22))
      return 1;
  for (int i = 0; i < 300; i++)
    audio_event_at(NOTE_OFF, 69, 0, 0);
  audio_render(32);
  for (int i = 0; i < 32; i++)
    if (audio_left()[i] != 0)
      return 1;
  return 0;
}
