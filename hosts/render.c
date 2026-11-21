#include "control.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define MAX_EVENTS 4096
static int read_score(const char *path, int *rate, int *frames,
                      BlueEvent *events, size_t *count) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  int ok = fscanf(f, "%d%d", rate, frames) == 2 && *rate >= 8000 &&
           *rate <= 192000 && *frames >= 1 && *frames <= 192000 * 60;
  size_t n = 0;
  int fields;
  while (ok && n < MAX_EVENTS) {
    BlueEvent *e = &events[n];
    fields = fscanf(f, "%" SCNu64 "%d%d%f", &e->frame, &e->command, &e->id,
                    &e->value);
    if (fields == EOF)
      break;
    ok = fields == 4 && e->frame < (unsigned)*frames &&
         (!n || e->frame >= events[n - 1].frame) && isfinite(e->value);
    if (ok)
      n++;
  }
  *count = n;
  fclose(f);
  return ok && n < MAX_EVENTS;
}
int main(int argc, char **argv) {
  if (argc != 4 && argc != 5) {
    fprintf(stderr,
            "Usage: blue-render PACK_FOLDER PRESET OUTPUT.wav [SCORE]\n");
    return 2;
  }
  int rate = 48000, frames = rate * 4;
  size_t count = 2, next = 0;
  BlueEvent events[MAX_EVENTS] = {{0, NOTE_ON, 69, .85f},
                                  {96000, NOTE_OFF, 69, 0}};
  if (argc == 5 && !read_score(argv[4], &rate, &frames, events, &count)) {
    fprintf(stderr, "Invalid score: %s\n", argv[4]);
    return 2;
  }
  BlueEngine *e = blue_create(rate, 16);
  if (!e)
    return 1;
  char error[2048];
  if (!blue_load(blue_stage(e), argv[1], argv[2], error, sizeof error)) {
    fprintf(stderr, "%s\n", error);
    blue_destroy(e);
    return 1;
  }
  blue_commit(e);
  float *pcm = calloc((size_t)frames * 2, sizeof(float));
  if (!pcm) {
    blue_destroy(e);
    return 1;
  }
  for (int i = 0; i < frames; i += 128) {
    float l[128], r[128];
    int n = frames - i < 128 ? frames - i : 128;
    size_t first = next;
    while (next < count && events[next].frame < (unsigned)(i + n))
      next++;
    blue_timeline(e, l, r, n, events + first, next - first);
    for (int k = 0; k < n; k++) {
      pcm[2 * (i + k)] = l[k];
      pcm[2 * (i + k) + 1] = r[k];
    }
  }
  int ok = blue_write_wav(argv[3], pcm, frames, rate);
  free(pcm);
  blue_destroy(e);
  return !ok;
}
