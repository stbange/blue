#ifndef BLUE_CONTROL_H
#define BLUE_CONTROL_H
#include "blue.h"
typedef struct {
  char folder[1024], name[128];
} BluePreset;
int blue_catalog(const char *root, BluePreset *items, int capacity);
int blue_load(BluePatch *out, const char *folder, const char *preset,
              char *error, size_t size);
int blue_write_wav(const char *path, const float *stereo, size_t frames,
                   int rate);
int blue_read_sample(const char *path, BluePatch *patch);
#endif
