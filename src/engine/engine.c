#include "blue.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define TAU 6.2831853071795864769
#define ECHO 192000
#define GRAINS 8
typedef struct {
  double pos, step;
  int age, length;
} Grain;
typedef struct {
  double phase, grain_clock, cursor;
  float low[2], band[2], comb[2][1024];
  Grain grain[GRAINS];
  unsigned rng;
  double node_state[8];
  const BlueNodeType *type;
} LaneState;
typedef struct {
  int key, active, released, pedal, generation;
  uint64_t start;
  float velocity, amp, held_env;
  LaneState lane[3];
} Voice;
struct BlueEngine {
  BluePatch patches[2];
  int current, staged, rate, voices, sustain, pos;
  unsigned late;
  uint64_t clock;
  float bend, pressure, wheel, macro[8], target[8], spatial[3], distance[3];
  float echo[2][ECHO], room[2][8192];
  Voice voice[BLUE_VOICES];
};
static float bound(float x, float lo, float hi) {
  return fminf(hi, fmaxf(lo, x));
}
void blue_defaults(BluePatch *p) {
  memset(p, 0, sizeof *p);
  p->gain = .22f;
  p->attack = .012f;
  p->release = .25f;
  p->delay = .5f;
  p->bpm = 120;
  p->feedback = .32f;
  p->reverb = .5f;
  p->seed = 37;
  p->loop = -1;
  for (int i = 0; i < 3; i++) {
    BlueLane *l = &p->lane[i];
    l->gain = i == 0;
    l->root = 60;
    l->ratio = 2;
    l->depth = 2;
    l->cutoff = 5000;
    l->size = .06f;
    l->density = 18;
    l->harmonic[0][0] = l->harmonic[1][0] = 1;
    l->harmonic[1][2] = .4f;
  }
}
BlueEngine *blue_create(int rate, int voices) {
  if (rate < 8000 || rate > 192000 || voices < 1 || voices > BLUE_VOICES)
    return NULL;
  BlueEngine *e = calloc(1, sizeof *e);
  if (e) {
    e->rate = rate;
    e->voices = voices;
    for (int i = 0; i < 3; i++)
      e->distance[i] = 1;
    blue_defaults(&e->patches[0]);
  }
  return e;
}
void blue_destroy(BlueEngine *e) { free(e); }
BluePatch *blue_stage(BlueEngine *e) {
  int next = 1 - e->current;
  for (int i = 0; i < e->voices; i++)
    if (e->voice[i].active && e->voice[i].generation == next)
      return NULL;
  e->staged = 1;
  return &e->patches[next];
}
int blue_commit(BlueEngine *e) {
  if (!e->staged)
    return 0;
  e->staged = 0;
  e->current = 1 - e->current;
  memcpy(e->target, e->patches[e->current].macro, sizeof e->target);
  return 1;
}
void blue_event(BlueEngine *e, int cmd, int id, float val) {
  if (!isfinite(val))
    return;
  if (cmd == NOTE_ON && val <= 0)
    cmd = NOTE_OFF;
  if (cmd == NOTE_ON && id >= 0 && id <= 127) {
    Voice *v = e->voice;
    for (int i = 0; i < e->voices; i++) {
      Voice *t = &e->voice[i];
      if (!t->active) {
        v = t;
        break;
      }
      if (t->start < v->start)
        v = t;
    }
    memset(v, 0, sizeof *v);
    v->active = 1;
    v->key = id;
    v->generation = e->current;
    v->velocity = bound(val, 0, 1);
    v->start = e->clock;
    for (int i = 0; i < 3; i++){
      v->lane[i].rng = e->patches[e->current].seed + (unsigned)id + i;
      v->lane[i].type=blue_node(e->patches[e->current].lane[i].type_key);
      if(v->lane[i].type && v->lane[i].type->reset)v->lane[i].type->reset(v->lane[i].node_state);
    }
  } else if (cmd == NOTE_OFF) {
    Voice *oldest = NULL;
    for (int i = 0; i < e->voices; i++) {
      Voice *v = &e->voice[i];
      if (v->active && !v->released && !v->pedal && v->key == id &&
          (!oldest || v->start < oldest->start))
        oldest = v;
    }
    if (oldest) {
      oldest->pedal = e->sustain;
      oldest->released = !e->sustain;
    }
  } else if (cmd == PANIC || cmd == PEDAL) {
    if (cmd == PEDAL)
      e->sustain = val >= .5f;
    for (int i = 0; i < e->voices; i++) {
      Voice *v = &e->voice[i];
      if (cmd == PANIC || (cmd == PEDAL && !e->sustain && v->pedal))
        v->released = 1;
    }
  } else if (cmd == MACRO && id >= 0 && id < 8)
    e->target[id] = bound(val, 0, 1);
  else if (cmd == POSITION && id >= 0 && id < 3)
    e->spatial[id] = bound(val, -1, 1);
  else if (cmd == DISTANCE && id >= 0 && id < 3)
    e->distance[id] = 1.f / fmaxf(1, bound(val, 0, 100));
  else if (cmd == BEND)
    e->bend = bound(val, -2, 2);
  else if (cmd == PRESSURE)
    e->pressure = bound(val, 0, 1);
  else if (cmd == WHEEL)
    e->wheel = bound(val, 0, 1);
  else if (cmd == TEMPO)
    e->patches[e->current].bpm = bound(val, 20, 300);
}
static float sample_at(const BluePatch *p, double pos) {
  int n = p->loop_end > p->loop_start ? p->loop_end : p->sample_count;
  if (n < 1)
    return 0;
  int start = p->loop_start;
  if (pos >= n || pos < 0) {
    pos = fmod(pos - start, n - start);
    if (pos < 0)
      pos += n - start;
    pos += start;
  }
  int a = (int)pos, b = a + 1 < n ? a + 1 : start;
  return p->sample[a] + (float)(pos - a) * (p->sample[b] - p->sample[a]);
}
static float noise(unsigned *state) {
  *state = *state * 1664525u + 1013904223u;
  return (float)(*state >> 8) / 16777216.f - .5f;
}
static float source(LaneState *s, const BlueLane *l, const BluePatch *p,
                    double freq, int rate, float morph, float density,
                    double age) {
  double ph = s->phase;
  s->phase += freq / rate;
  s->phase -= floor(s->phase);
  double step =
      freq / (440 * exp2((l->root - 69) / 12.)) * p->sample_rate / rate;
  switch (l->source) {
  case CUSTOM: return s->type?s->type->process(s->node_state,ph,freq,rate,l->parameter):0;
  case SAW: {
    double dt = fmin(freq / rate, .499), x = 2 * ph - 1;
    if (ph < dt) {
      double t = ph / dt;
      x -= 2 * t - t * t - 1;
    } else if (ph > 1 - dt) {
      double t = (ph - 1) / dt;
      x -= t * t + 2 * t + 1;
    }
    return (float)x;
  }
  case TABLE: {
    float sum = 0;
    for (int j = 0; j < 16 && freq * (j + 1) < rate * .5; j++) {
      float h =
          l->harmonic[0][j] + morph * (l->harmonic[1][j] - l->harmonic[0][j]);
      if (h != 0)
        sum += h * (float)sin(TAU * ph * (j + 1));
    }
    return sum;
  }
  case FM:
  case RING: {
    double m = sin(TAU * s->cursor);
    s->cursor += freq * l->ratio / rate;
    s->cursor -= floor(s->cursor);
    return l->source == FM
               ? (float)sin(TAU * ph +
                            l->depth * m *
                                (l->decay > 0 ? exp(-age / l->decay) : 1))
               : (float)(sin(TAU * ph) * m);
  }
  case SAMPLE: {
    float x = sample_at(p, s->cursor);
    s->cursor += step;
    return x;
  }
  case GRAIN: {
    float x = 0;
    s->grain_clock -= density / rate;
    if (s->grain_clock <= 0) {
      s->grain_clock += 1;
      for (int k = 0; k < GRAINS; k++)
        if (!s->grain[k].length) {
          s->grain[k] = (Grain){(l->position + noise(&s->rng) * l->scatter) *
                                    p->sample_count,
                                step, 0, (int)fmax(2, l->size * rate)};
          break;
        }
    }
    for (int k = 0; k < GRAINS; k++) {
      Grain *g = &s->grain[k];
      if (!g->length)
        continue;
      x += sample_at(p, g->pos) *
           (float)(.5 - .5 * cos(TAU * g->age / g->length));
      g->pos += g->step;
      if (++g->age == g->length)
        g->length = 0;
    }
    return x * .5f;
  }
  default:
    return (float)sin(TAU * ph);
  }
}
static float insert(LaneState *s, int slot, int type, float x, float cutoff,
                    double freq, int rate, uint64_t frame) {
  if (type == BYPASS)
    return x;
  if (type == SHAPE)
    return tanhf(2 * x);
  if (type == COMB) {
    int delay = (int)bound((float)(rate / freq), 1, 1023);
    float y = x + .7f * s->comb[slot][(frame - delay) & 1023];
    s->comb[slot][frame & 1023] = y;
    return y;
  }
  float f = 2 * sinf((float)(TAU * .5) * bound(cutoff / rate, .0001f, .14f));
  s->low[slot] += f * s->band[slot];
  float high = x - s->low[slot] - .9f * s->band[slot];
  s->band[slot] += f * high;
  return type == LOWPASS    ? s->low[slot]
         : type == BANDPASS ? s->band[slot]
                            : high;
}
static float envelope(const BluePatch *p, double t) {
  if (!p->points)
    return 1;
  if (p->sync)
    t *= p->bpm / 60.;
  double end = p->envelope[p->points - 1].time;
  if (p->loop >= 0 && t >= end) {
    double start = p->envelope[p->loop].time;
    if (end > start)
      t = start + fmod(t - end, end - start);
  }
  for (int j = 1; j < p->points; j++)
    if (t < p->envelope[j].time) {
      BluePoint a = p->envelope[j - 1], b = p->envelope[j];
      double f = (t - a.time) / (b.time - a.time);
      return a.value + (b.value - a.value) * (float)pow(f, b.curve);
    }
  return p->envelope[p->points - 1].value;
}
void blue_render(BlueEngine *e, float *left, float *right, int frames) {
  for (int n = 0; n < frames; n++, e->clock++) {
    float dry[2] = {0}, send[2] = {0}, room[2] = {0};
    for (int k = 0; k < 8; k++)
      e->macro[k] += (e->target[k] - e->macro[k]) * .01f;
    for (int j = 0; j < e->voices; j++) {
      Voice *v = &e->voice[j];
      if (!v->active)
        continue;
      BluePatch *p = &e->patches[v->generation];
      if (v->released)
        v->amp = fmaxf(0, v->amp - 1.f / fmaxf(1, p->release * e->rate));
      else
        v->amp = fminf(1, v->amp + 1.f / fmaxf(1, p->attack * e->rate));
      if (v->released && !v->amp) {
        v->active = 0;
        continue;
      }
      double age = (double)(e->clock - v->start) / e->rate;
      float mod[4] = {0};
      for (int k = 0; k < 8; k++)
        for (int d = 0; d < 4; d++)
          mod[d] += e->macro[k] * p->routes[k][d];
      double freq = 440 * exp2((v->key - 69 + e->bend) / 12.);
      if (!v->released)
        v->held_env = envelope(p, age);
      float amp = p->gain * v->amp * v->velocity * v->held_env *
                  bound(1 + mod[0], 0, 3);
      for (int i = 0; i < 3; i++) {
        const BlueLane *l = &p->lane[i];
        if (!l->gain)
          continue;
        float motion =
            p->lfo[i] > 0 ? (float)sin(TAU * age * p->lfo[i]) * .25f : 0;
        float x = source(&v->lane[i], l, p, freq, e->rate,
                         bound(l->morph + mod[1] + motion, 0, 1),
                         bound(l->density * (1 + mod[3]), 1, 256), age);
        for (int k = 0; k < 2; k++)
          x = insert(&v->lane[i], k, l->insert[k], x,
                     l->cutoff * (1 + mod[2] + e->wheel + e->pressure + motion +
                                  l->env_cutoff * v->held_env),
                     freq, e->rate, e->clock);
        x *= l->gain * amp * e->distance[i];
        float pan = bound(l->pan + e->spatial[i], -1, 1);
        for (int ch = 0; ch < 2; ch++) {
          float y = x * sqrtf(.5f * (1 + (ch ? pan : -pan)));
          dry[ch] += y;
          send[ch] += y * l->send;
          room[ch] += y * l->space;
        }
      }
    }
    BluePatch *p = &e->patches[e->current];
    int d = (int)bound((float)(p->delay * 60 / p->bpm * e->rate), 1, ECHO - 1);
    for (int ch = 0; ch < 2; ch++) {
      int rd = (e->pos + ECHO - d) % ECHO;
      float echo = e->echo[ch][rd];
      if (p->echo_pitch != 0) {
        double u = fmod(e->clock * (exp2(p->echo_pitch / 12.) - 1) / 1024., 1.);
        if (u < 0)
          u++;
        double b = u < .5 ? u + .5 : u - .5;
        float w = (float)(.5 - .5 * cos(TAU * u));
        echo = w * e->echo[ch][(rd + (int)(u * 1024)) % ECHO] +
               (1 - w) * e->echo[ch][(rd + (int)(b * 1024)) % ECHO];
      }
      e->echo[ch][e->pos] = send[ch] + echo * p->feedback;
      int tap = ch ? 4093 : 3253;
      float tail = e->room[1 - ch][(e->clock - tap) & 8191];
      e->room[ch][e->clock & 8191] = room[ch] + tail * p->reverb;
      (ch ? right : left)[n] = bound(dry[ch] + echo + tail, -1, 1);
    }
    if (++e->pos == ECHO)
      e->pos = 0;
  }
}
uint64_t blue_clock(const BlueEngine *e) { return e->clock; }
unsigned blue_late(const BlueEngine *e) { return e->late; }
void blue_timeline(BlueEngine *e, float *l, float *r, int frames,
                   const BlueEvent *ev, size_t count) {
  int done = 0;
  uint64_t end = e->clock + (unsigned)frames;
  for (size_t i = 0; i < count && ev[i].frame < end; i++) {
    if (ev[i].frame < e->clock)
      e->late++;
    else {
      int length = (int)(ev[i].frame - e->clock);
      blue_render(e, l + done, r + done, length);
      done += length;
    }
    blue_event(e, ev[i].command, ev[i].id, ev[i].value);
  }
  blue_render(e, l + done, r + done, frames - done);
}
