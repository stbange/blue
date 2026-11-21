#include "control.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: blue-render PACK_FOLDER PRESET OUTPUT.wav\n");
    return 2;
  }
  BlueEngine *e = blue_create(48000, 16);
  if (!e)
    return 1;
  char error[2048];
  if (!blue_load(blue_stage(e), argv[1], argv[2], error, sizeof error)) {
    fprintf(stderr, "%s\n", error);
    blue_destroy(e);
    return 1;
  }
  blue_commit(e);
  int frames = 48000 * 4;
  float *pcm = calloc((size_t)frames * 2, sizeof(float));
  if (!pcm) {
    blue_destroy(e);
    return 1;
  }
  BlueEvent notes[] = {{0, NOTE_ON, 69, .85f}, {96000, NOTE_OFF, 69, 0}};
  for (int i = 0; i < frames; i += 128) {
    float l[128], r[128];
    size_t first = i == 0 ? 0 : 1;
    blue_timeline(e, l, r, 128, notes + first, i > 96000 ? 0 : 2 - first);
    for (int k = 0; k < 128; k++) {
      pcm[2 * (i + k)] = l[k];
      pcm[2 * (i + k) + 1] = r[k];
    }
  }
  int ok = blue_write_wav(argv[3], pcm, frames, 48000);
  free(pcm);
  blue_destroy(e);
  return !ok;
}
