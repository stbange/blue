#include "control.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  if (argc < 3)
    return 2;
  if (!strcmp(argv[1], "catalog")) {
    BluePreset items[3];
    return blue_catalog(argv[2], items, 3) != atoi(argv[3]);
  }
  BluePatch *p = calloc(1, sizeof *p);
  if (!p)
    return 2;
  int result = 0;
  if (!strcmp(argv[1], "wave")) {
    float pcm[] = {1, -1, .75f, .25f, 0, 0};
    if (!blue_write_wav(argv[2], pcm, 3, 44100) ||
        !blue_read_sample(argv[2], p) || p->sample_rate != 44100 ||
        p->sample_count != 3 || fabs(p->sample[1] - .5) > .001 ||
        blue_write_wav("", pcm, 3, 44100) ||
        blue_write_wav(argv[2], pcm, UINT32_MAX, 44100))
      result = 1;
  } else {
    char err[2048];
    p->gain = -1;
    int ok = blue_load(p, argv[2], "test", err, sizeof err);
    if (ok != atoi(argv[3]) || (!ok && (p->gain != -1 || !err[0])))
      result = 1;
    if (result)
      fprintf(stderr, "pack result %d: %s\n", ok, ok ? "accepted" : err);
  }
  free(p);
  return result;
}
