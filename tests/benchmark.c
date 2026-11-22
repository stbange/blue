#include "blue_queue.h"
#include "control.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double seconds(void) {
  struct timespec now;
  timespec_get(&now, TIME_UTC);
  return now.tv_sec + now.tv_nsec / 1e9;
}
static int order(const void *p, const void *q) {
  double a = *(const double *)p, b = *(const double *)q;
  return (a > b) - (a < b);
}
int main(int argc, char **argv) {
  if (argc < 3 || argc > 5)
    return 2;
  int duration = atoi(argv[2]);
  if (duration < 1 || duration > 600)
    return 2;
  const int rate = 48000, block = 128, blocks = duration * rate / block;
  BlueEngine *e = blue_create(rate, 16);
  char error[2048];
  BluePatch *p = blue_stage(e);
  if (!blue_load(p, argv[1], "Glass Tide", error, sizeof error)) {
    puts(error);
    return 1;
  }
  for (int i = 0; i < 3; i++) {
    p->lane[i].insert[0] = LOWPASS;
    p->lane[i].send = .1f;
    p->lane[i].space = .1f;
  }
  p->lane[2].density = 80;
  p->lane[2].size = .1f;
  blue_commit(e);
  for (int i = 0; i < 16; i++)
    blue_event(e, NOTE_ON, 44 + i, .65f);
  double *cost = calloc(blocks, sizeof *cost);
  if (!cost)
    return 1;
  FILE *capture = argc >= 4 ? fopen(argv[3], "wb") : NULL;
  if (argc >= 4 && !capture)
    return 2;
  int soak = argc == 5 && !strcmp(argv[4], "soak"), reloads = 0;
  BluePatch *prepared = malloc(sizeof *prepared);
  if (!prepared)
    return 1;
  *prepared = *p;
  BlueQueue queue = {0};
  int silent = 0;
  double peak = 0;
  float l[128], r[128];
  for (int b = 0; b < blocks; b++) {
    if (soak && b && b % 375 == 0) {
      BluePatch *next = blue_stage(e);
      if (next) {
        *next = *prepared;
        blue_commit(e);
        reloads++;
      }
      for (int k = 0; k < 512; k++)
        blue_queue_push(&queue, MACRO, 0, (k % 101) * .01f);
      blue_queue_drain(&queue, e);
      for (int k = 0; k < 16; k++)
        blue_event(e, NOTE_ON, 44 + k, .65f);
    }
    double start = seconds();
    blue_render(e, l, r, block);
    cost[b] = seconds() - start;
    double energy = 0;
    for (int i = 0; i < 128; i++) {
      energy += l[i] * l[i] + r[i] * r[i];
      peak = fmax(peak, fabs(l[i]));
    }
    if (capture) {
      int16_t pcm[256];
      for (int i = 0; i < 128; i++) {
        pcm[i * 2] = (int16_t)lrintf(l[i] * 32767);
        pcm[i * 2 + 1] = (int16_t)lrintf(r[i] * 32767);
      }
      if (fwrite(pcm, sizeof *pcm, 256, capture) != 256)
        return 1;
    }
    if (!isfinite(energy) || !energy)
      silent++;
  }
  qsort(cost, blocks, sizeof *cost, order);
  double p99 = cost[(int)(.99 * (blocks - 1))];
  printf("rendered_seconds=%d voices=16 sample_rate=48000 block=128 "
         "p99_ms=%.4f deadline_ms=%.4f silent_or_invalid_blocks=%d peak=%.4f\n",
         duration, p99 * 1000, 128000. / rate, silent, peak);
  if (capture && fclose(capture))
    return 1;
  if (soak)
    printf("reloads=%d coalesced=%u\n", reloads, queue.coalesced);
  free(prepared);
  free(cost);
  blue_destroy(e);
  return silent || p99 > .7 * block / rate;
}
