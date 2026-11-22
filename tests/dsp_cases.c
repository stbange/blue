/* Inspect per-grain and envelope state without exposing diagnostics in the
 * audio API. */
#include "../src/engine/engine.c"
#include <stdio.h>

#define VERIFY(x)                                                              \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "Line %d: %s\n", __LINE__, #x);                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static void check_waves(BluePatch *p) {
  BlueLane *lane = &p->lane[0];
  lane->source = TABLE;
  for (int i = 0; i < 16; i++) {
    lane->harmonic[0][i] = 1.f / (i + 1);
    lane->harmonic[1][i] = i ? .1f / (i + 1) : 1;
  }
  blue_prepare_waves(p);
  /* Bin-centred tones cross four mip levels. Compare the complete time-domain
     period with the analytic band-limited wave, including a midway morph. */
  for (int bin = 151; bin < 1900; bin += 107) {
    int partials = 16;
    while (partials * bin >= 2048)
      partials /= 2;
    for (int mix = 0; mix <= 2; mix++) {
      double morph = mix * .5, error = 0, power = 0;
      LaneState state = {0};
      for (int n = 0; n < 4096; n++) {
        double ideal = 0;
        for (int h = 1; h <= partials; h++) {
          double gain = (1 - morph) * lane->harmonic[0][h - 1] +
                        morph * lane->harmonic[1][h - 1];
          ideal += gain * sin(TAU * bin * h * n / 4096.);
        }
        double actual = source(&state, lane, p, 48000. * bin / 4096, 48000,
                               (float)morph, 0, 0);
        error += (actual - ideal) * (actual - ideal);
        power += ideal * ideal;
      }
      VERIFY(error < power * 1.e-7);
    }
  }
}

static int grains(BluePatch *p, int density, float size, int *peak) {
  BlueLane *lane = &p->lane[0];
  lane->source = GRAIN;
  lane->size = size;
  lane->position = .25f;
  lane->scatter = 0;
  p->sample_count = 1024;
  p->sample_rate = 48000;
  LaneState state = {0};
  int starts = 0;
  *peak = 0;
  for (int i = 0; i < 48000; i++) {
    source(&state, lane, p, 440, 48000, 0, (float)density, i / 48000.);
    int live = 0;
    for (int k = 0; k < GRAINS; k++) {
      Grain *g = &state.grain[k];
      if (g->length)
        live++;
      if (g->age == 1) {
        starts++;
        VERIFY(fabs(g->pos - g->step - (256 + lane->scan * i)) < .02);
      }
    }
    if (live > *peak)
      *peak = live;
  }
  return starts;
}
int main(void) {
  BluePatch *p = malloc(sizeof *p);
  VERIFY(p);
  blue_defaults(p);
  check_waves(p);
  int peak;
  int low = grains(p, 20, .01f, &peak), high = grains(p, 40, .01f, &peak);
  VERIFY(abs(high - 2 * low) <= 1);
  p->lane[0].scan = .25f;
  int crowded = grains(p, 240, .4f, &peak);
  VERIFY(peak == GRAINS && crowded <= 3 * GRAINS);
  p->points = 1;
  p->envelope[0] = (BluePoint){0, .42f, 1};
  VERIFY(envelope(p, 12) == .42f);
  p->points = 128;
  p->loop = 126;
  p->sync = 1;
  p->bpm = 120;
  for (int i = 0; i < 128; i++)
    p->envelope[i] = (BluePoint){i * .5f, i == 127, 2};
  VERIFY(fabs(envelope(p, 31.625) - .25) < 1e-6);
  VERIFY(fabs(envelope(p, 31.875) - .25) < 1e-6);
  p->bpm = 60;
  VERIFY(fabs(envelope(p, 63.25) - .25) < 1e-6);
  p->sample_count = 4;
  p->loop_start = 1;
  p->loop_end = 4;
  for (int i = 0; i < 4; i++)
    p->sample[i] = (float)i;
  VERIFY(sample_at(p, 3.5) == 2 && sample_at(p, 4) == 1 &&
         sample_at(p, -1) == 2);
  LaneState f = {0};
  VERIFY(insert(&f, 0, COMB, 1, 2000, 440, 48000, 0) == 1);
  for (int i = 1; i < 109; i++)
    VERIFY(insert(&f, 0, COMB, 0, 2000, 440, 48000, i) == 0);
  VERIFY(fabs(insert(&f, 0, COMB, 0, 2000, 440, 48000, 109) - .7) < 1e-6);
  BlueEngine *e = blue_create(48000, 2);
  blue_event(e, NOTE_ON, 60, .7);
  blue_event(e, NOTE_ON, 60, .5);
  blue_event(e, PEDAL, 0, 1);
  blue_event(e, NOTE_OFF, 60, 0);
  VERIFY(e->voice[0].pedal && !e->voice[1].pedal);
  blue_event(e, PEDAL, 0, 0);
  VERIFY(e->voice[0].released && !e->voice[1].released);
  float l[64], r[64];
  blue_render(e, l, r, 64);
  blue_event(e, NOTE_ON, 62, .8);
  blue_render(e, l, r, 64);
  blue_event(e, NOTE_ON, 64, 1);
  VERIFY(e->voice[0].key == 62 && e->voice[1].key == 64);
  BluePatch *bank = blue_stage(e);
  blue_defaults(bank);
  bank->loop = 0;
  bank->sync = 1;
  bank->points = 2;
  bank->envelope[0] = (BluePoint){0, .1f, 1};
  bank->envelope[1] = (BluePoint){.01f, 1, 1};
  blue_commit(e);
  blue_event(e, NOTE_ON, 69, 1);
  BlueEvent off = {blue_clock(e) + 31, NOTE_OFF, 69, 0};
  blue_timeline(e, l, r, 64, &off, 1);
  Voice *released = e->voice[0].key == 69 ? &e->voice[0] : &e->voice[1];
  VERIFY(released->released && released->held_env > .2f &&
         released->held_env < .23f);
  blue_destroy(e);
  free(p);
  return 0;
}
