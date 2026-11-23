#ifndef BLUE_TUNING_H
#define BLUE_TUNING_H
#include "blue.h"
#define BLUE_TUNING_STEPS 24
#define BLUE_TUNINGS 12
typedef struct {
  char name[40];
  int root, count;
  float cents[BLUE_TUNING_STEPS];
} BlueTuning;
extern const char blue_keys[BLUE_KEYS + 1];
float blue_tuned_pitch(const BlueTuning *tuning, int key);
int blue_read_tunings(const char *path, BlueTuning *tunings);
int blue_parse_tuning(const char *root, const char *steps, BlueTuning *tuning);
void blue_pitch_name(float pitch, char *output, size_t size);
#endif
