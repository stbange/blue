#include "blue_queue.h"
#include "control.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x);              \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
static void tuned_notes(void) {
  BlueEngine *engine = blue_create(48000, 4);
  CHECK(engine);
  BluePatch *patch = blue_stage(engine);
  CHECK(patch);
  blue_defaults(patch);
  patch->attack = patch->release = 0;
  CHECK(blue_commit(engine));
  float left[480], right[480], prev = 0;
  blue_event(engine, TUNED_NOTE_ON, BLUE_KEY_ID, 69.5f);
  int cycles = 0;
  for (int chunk = 0; chunk < 100; chunk++) {
    blue_render(engine, left, right, 480);
    for (int frame = 0; frame < 480; frame++) {
      cycles += prev < 0 && left[frame] >= 0;
      prev = left[frame];
    }
  }
  CHECK(abs(cycles - 453) <= 1);
  blue_event(engine, NOTE_OFF, 69, 0);
  blue_render(engine, left, right, 480);
  CHECK(fabsf(left[40]) > .02f);
  blue_event(engine, NOTE_OFF, BLUE_KEY_ID, 0);
  blue_event(engine, TUNED_NOTE_ON, BLUE_KEY_ID - 1, 69.5f);
  blue_event(engine, TUNED_NOTE_ON, BLUE_KEY_ID + BLUE_KEYS, 69.5f);
  blue_event(engine, TUNED_NOTE_ON, BLUE_KEY_ID, 128);
  blue_event(engine, TUNED_NOTE_ON, BLUE_KEY_ID, -1);
  blue_render(engine, left, right, 480);
  for (int frame = 0; frame < 480; frame++)
    CHECK(left[frame] == 0 && right[frame] == 0);
  blue_destroy(engine);
}
static void timing(int rate, int block) {
  BlueEngine *e = blue_create(rate, 2);
  CHECK(e);
  BluePatch *p = blue_stage(e);
  CHECK(p);
  blue_defaults(p);
  p->attack = p->release = 0;
  CHECK(blue_commit(e));
  CHECK(!blue_commit(e));
  float l[511], r[511];
  int crosses = 0;
  double first = 0, final = 0;
  float last = 0;
  for (int pos = 0; pos < rate;) {
    int n = block;
    if (n > rate - pos)
      n = rate - pos;
    BlueEvent ev = {137, NOTE_ON, 69, 1};
    blue_timeline(e, l, r, n, &ev, pos <= 137);
    for (int k = 0; k < n; k++) {
      if (pos + k < 137)
        CHECK(l[k] == 0);
      CHECK(l[k] == r[k]);
      if (last < 0 && l[k] >= 0) {
        final = pos + k - 1 - last / (l[k] - last);
        if (!crosses)
          first = final;
        crosses++;
      }
      last = l[k];
    }
    pos += n;
  }
  CHECK(fabs(1200 * log2((crosses - 1) * rate / (final - first) / 440.0)) < 1);
  CHECK(abs(crosses - (int)(440. * (rate - 138) / rate)) <= 1);
  BlueEvent off = {3, NOTE_OFF, 69, 0};
  blue_timeline(e, l, r, 1, &off, 1);
  CHECK(blue_late(e) == 1 && l[0] == 0 && blue_clock(e) == (unsigned)rate + 1);
  blue_destroy(e);
}
static double render(BlueEngine *e, int n) {
  double power = 0;
  float l[64], r[64];
  while (n) {
    int k = n < 64 ? n : 64;
    blue_render(e, l, r, k);
    for (int i = 0; i < k; i++) {
      CHECK(isfinite(l[i]) && isfinite(r[i]));
      power += l[i] * l[i] + r[i] * r[i];
    }
    n -= k;
  }
  return power;
}
static void node_reset(double state[8]) { state[0] = .5; }
static float node_sample(double state[8], double phase, double hz, int rate,
                         float parameter) {
  (void)phase;
  (void)hz;
  (void)rate;
  return (float)state[0] * parameter;
}
static void registry_test(void) {
  static const BlueNodeType type = {"fixture.node.v1", "amplitude", 0, 1, .5f,
                                    node_reset,        node_sample};
  CHECK(!blue_register_node(NULL));
  CHECK(blue_register_node(&type));
  CHECK(!blue_register_node(&type));
  CHECK(blue_node_key("") == 0);
  BlueEngine *e = blue_create(48000, 1);
  BluePatch *p = blue_stage(e);
  blue_defaults(p);
  p->lane[0].source = CUSTOM;
  p->lane[0].type_key = blue_node_key(type.type_id);
  p->lane[0].parameter = .5;
  blue_commit(e);
  blue_event(e, NOTE_ON, 69, 1);
  CHECK(render(e, 32) > 0);
  blue_destroy(e);
}
int main(int argc, char **argv) {
  registry_test();
  const int partitions[] = {1, 63, 128, 511};
  for (int k = 0; k < 4; k++) {
    BlueEngine *e = blue_create(48000, 2);
    BluePatch *p = blue_stage(e);
    blue_defaults(p);
    p->lane[0].source = CUSTOM;
    p->lane[0].type_key = blue_node_key("fixture.node.v1");
    p->lane[0].parameter = 1;
    p->attack = p->release = 0;
    p->step_count = 1;
    p->steps[0].seconds = .002;
    p->steps[0].macro = 2;
    p->steps[0].value = .5;
    blue_commit(e);
    BlueEvent ev[] = {{137, NOTE_ON, 69, 1}, {279, NOTE_OFF, 69, 0}};
    size_t cursor = 0;
    for (int n = 0; n < 511;) {
      int len = partitions[k];
      if (len > 511 - n)
        len = 511 - n;
      size_t first = cursor;
      while (cursor < 2 && ev[cursor].frame < (unsigned)(n + len))
        cursor++;
      float l[511], r[511];
      blue_timeline(e, l, r, len, ev + first, cursor - first);
      for (int j = 0; j < len; j++)
        CHECK((l[j] != 0) == (n + j >= 137 && n + j < 279));
      n += len;
    }
    blue_destroy(e);
  }
  BlueQueue queue = {0};
  BlueEngine *qe = blue_create(48000, 2);
  for (int k = 0; k < 512; k++)
    blue_queue_push(&queue, MACRO, 1, .6f);
  for (int k = 0; k < 64; k++)
    blue_queue_push(&queue, NOTE_OFF, k, 0);
  blue_queue_push(&queue, NOTE_ON, 69, 1);
  CHECK(queue.coalesced > 0 && queue.emergency && queue.dropped &&
        queue.used == BLUE_QUEUE_CAPACITY);
  blue_queue_drain(&queue, qe);
  CHECK(!queue.emergency && !queue.used && render(qe, 64) == 0);
  blue_destroy(qe);
  CHECK(argc == 2);
  char locations[2048], diagnostic[2048];
  CHECK(blue_source_path(argv[1], "Calibration", 0, locations,
                         sizeof locations) &&
        strstr(locations, "calibration.json"));
  CHECK(blue_source_path(argv[1], "Calibration", 1, locations,
                         sizeof locations) &&
        strstr(locations, "performance.lua"));
  CHECK(!blue_source_path(argv[1], "Calibration", 1, locations, 1));
  BluePatch *custom = malloc(sizeof *custom);
  CHECK(custom);
  CHECK(blue_compile(custom, argv[1], "Calibration",
                     "{\"format\":1,\"lanes\":[{\"source\":\"fixture.node.v1\","
                     "\"parameter\":0.3}],\"macros\":[0.6]}",
                     "blue.control(1,'Timbre')", diagnostic,
                     sizeof diagnostic));
  CHECK(custom->lane[0].source == CUSTOM && fabs(custom->macro[0] - .6) < .001);
  CHECK(!blue_compile(
      custom, argv[1], "Calibration",
      "{\"format\":1,\"lanes\":[{\"source\":\"sine\"}],\"macros\":[-2]}", "",
      diagnostic, sizeof diagnostic));
  CHECK(blue_compile(
      custom, argv[1], "Calibration", NULL,
      "blue.mod('macro1',1,'pitch',12) blue.mod('lfo2',1,'pan',.1,'control') "
      "blue.mod('envelope',1,'cutoff',1) "
      "blue.mod('velocity',1,'density',1) blue.mod('pressure',1,'send',.4) "
      "blue.mod('wheel',1,'space',.1)",
      diagnostic, sizeof diagnostic));
  CHECK(custom->route_count == 6);
  BlueEngine *matrix = blue_create(48000, 2);
  *blue_stage(matrix) = *custom;
  blue_commit(matrix);
  blue_event(matrix, MACRO, 0, 1);
  blue_event(matrix, NOTE_ON, 69, 1);
  float ml[512], mr[512], previous = 0;
  int periods = 0;
  blue_render(matrix, ml, mr, 512);
  for (int k = 0; k < 48; k++) {
    blue_render(matrix, ml, mr, 512);
    for (int j = 0; j < 512; j++) {
      if (previous < 0 && ml[j] >= 0)
        periods++;
      previous = ml[j];
    }
  }
  CHECK(abs(periods - 451) < 3);
  blue_destroy(matrix);
  free(custom);
  tuned_notes();
  CHECK(!blue_create(1, 3));
  CHECK(!blue_create(48000, 100));
  int rates[] = {44100, 48000, 96000}, blocks[] = {1, 63, 128, 511};
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
      timing(rates[i], blocks[j]);
  BlueEngine *e = blue_create(48000, 2);
  char error[2048];
  const char *presets[] = {"Glass Tide", "Struck Wire", "Paper Cloud",
                           "Step Current", "Calibration"};
  for (int i = 0; i < 5; i++) {
    BluePatch *p = blue_stage(e);
    CHECK(p);
    CHECK(blue_load(p, argv[1], presets[i], error, sizeof error));
    CHECK(blue_commit(e));
    blue_event(e, NOTE_ON, 69, .8f);
    CHECK(render(e, 12000) > 0);
    blue_event(e, PANIC, 0, 0);
    render(e, 65000);
  }
  BluePatch *p = blue_stage(e);
  CHECK(!blue_load(p, argv[1], "missing", error, sizeof error));
  blue_defaults(p);
  p->release = 0;
  p->lane[0].source = RING;
  p->lane[0].ratio = .25;
  p->lane[0].insert[0] = HIGHPASS;
  p->lane[0].insert[1] = SHAPE;
  p->lane[0].send = .5;
  p->echo_pitch = 12;
  blue_commit(e);
  blue_event(e, NOTE_ON, 65, 1);
  blue_event(e, NOTE_ON, 66, 1);
  blue_event(e, NOTE_ON, 69, .8);
  blue_event(e, PEDAL, 0, 1);
  blue_event(e, NOTE_OFF, 69, 0);
  blue_event(e, BEND, 0, 1);
  blue_event(e, WHEEL, 0, .5);
  blue_event(e, PRESSURE, 0, .8);
  blue_event(e, MACRO, 0, .4);
  blue_event(e, POSITION, 0, -1);
  blue_event(e, TEMPO, 0, 90);
  CHECK(render(e, 4000) > 0);
  p = blue_stage(e);
  blue_defaults(p);
  p->lane[0].source = SAMPLE;
  p->sample_count = 3;
  p->sample_rate = 24000;
  p->sample[1] = 1;
  p->lane[0].insert[0] = BANDPASS;
  blue_commit(e);
  CHECK(!blue_stage(e));
  blue_event(e, PEDAL, 0, 0);
  blue_event(e, PANIC, 0, 0);
  render(e, 32);
  CHECK(blue_stage(e));
  blue_event(e, NOTE_ON, 66, 1);
  CHECK(render(e, 800) > 0);
  blue_destroy(e);
  puts("unit checks passed");
  return 0;
}
