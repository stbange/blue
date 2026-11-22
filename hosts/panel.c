#include "panel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const char *const panel_fields[PANEL_ROWS] = {
    "Lane",       "Source",       "Solo",          "Mute",           "Level",
    "Stereo pan", "First insert", "Second insert", "Harmonic frame", "Partial",
    "Amplitude",  "Breakpoint",   "Time",          "Value"};
static const char *const sources[] = {"sine", "saw",    "table", "fm",
                                      "ring", "sample", "grain"};
static const char *const effects[] = {"bypass",   "lowpass", "highpass",
                                      "bandpass", "shape",   "comb"};
static int limit(int n, int max) { return n < 0 ? 0 : n > max ? max : n; }
static double clamp(double v, double min, double max) {
  return fmax(min, fmin(max, v));
}
static cJSON *field(cJSON *o, const char *name) {
  return cJSON_GetObjectItemCaseSensitive(o, name);
}
static cJSON *elem(cJSON *a, int i) { return cJSON_GetArrayItem(a, i); }
static void set(cJSON *o, const char *key, cJSON *v) {
  if (!cJSON_ReplaceItemInObjectCaseSensitive(o, key, v))
    cJSON_AddItemToObject(o, key, v);
}
static cJSON *array(cJSON *o, const char *key) {
  cJSON *a = field(o, key);
  if (!a) {
    a = cJSON_CreateArray();
    set(o, key, a);
  }
  return a;
}
static double number(cJSON *v, double fallback) {
  return cJSON_IsNumber(v) ? v->valuedouble : fallback;
}
static const char *string(cJSON *v, const char *fallback) {
  return cJSON_IsString(v) ? v->valuestring : fallback;
}
static int index_of(const char *const *list, int count, const char *name) {
  for (int i = 0; i < count; i++)
    if (!strcmp(list[i], name))
      return i;
  return 0;
}
int panel_open(BluePanel *p, const char *text) {
  panel_close(p);
  p->doc = cJSON_Parse(text);
  if (!cJSON_IsArray(field(p->doc, "lanes"))) {
    panel_close(p);
    return 0;
  }
  return 1;
}
void panel_close(BluePanel *p) {
  cJSON_Delete(p->doc);
  *p = (BluePanel){0};
}
static cJSON *current_lane(const BluePanel *p) {
  return elem(field(p->doc, "lanes"), p->lane);
}
void panel_value(const BluePanel *p, int row, char *out, size_t size) {
  cJSON *lane = current_lane(p), *env = field(p->doc, "envelope"),
        *point = elem(env, p->point);
  const char *text = NULL;
  double value = 0;
  switch (row) {
  case 0:
    snprintf(out, size, "%c", 'A' + p->lane);
    return;
  case 1:
    text = string(field(lane, "source"), "sine");
    break;
  case 2:
  case 3:
    text = cJSON_IsTrue(field(lane, row == 2 ? "solo" : "mute")) ? "On" : "Off";
    break;
  case 4:
    value = number(field(lane, "gain"), p->lane == 0);
    break;
  case 5:
    value = number(field(lane, "pan"), 0);
    break;
  case 6:
  case 7:
    text = string(elem(field(lane, "inserts"), row - 6), "bypass");
    break;
  case 8:
    value = p->frame + 1;
    break;
  case 9:
    value = p->partial + 1;
    break;
  case 10:
    value = number(elem(elem(field(lane, "harmonics"), p->frame), p->partial),
                   p->partial == 0);
    break;
  case 11:
    value = p->point + 1;
    break;
  case 12:
    value = number(field(point, "time"), p->point);
    break;
  case 13:
    value = number(field(point, "value"), 1);
    break;
  }
  if (text)
    snprintf(out, size, "%s", text);
  else
    snprintf(out, size, "%.3g", value);
}
static void set_index(cJSON *a, int i, cJSON *v) {
  if (i == cJSON_GetArraySize(a))
    cJSON_AddItemToArray(a, v);
  else
    cJSON_ReplaceItemInArray(a, i, v);
}
void panel_change(BluePanel *p, int d) {
  cJSON *lanes = field(p->doc, "lanes");
  if (p->row == 0) {
    p->lane = limit(p->lane + d, 2);
    while (p->lane >= cJSON_GetArraySize(lanes)) {
      cJSON *lane = cJSON_CreateObject();
      set(lane, "source", cJSON_CreateString("sine"));
      set(lane, "gain", cJSON_CreateNumber(.25));
      cJSON_AddItemToArray(lanes, lane);
    }
    return;
  }
  cJSON *lane = current_lane(p);
  if (p->row == 1) {
    int old = index_of(sources, 7, string(field(lane, "source"), "sine"));
    set(lane, "source", cJSON_CreateString(sources[(old + d + 7) % 7]));
  } else if (p->row == 2 || p->row == 3) {
    const char *key = p->row == 2 ? "solo" : "mute";
    set(lane, key, cJSON_CreateBool(!cJSON_IsTrue(field(lane, key))));
  } else if (p->row == 4 || p->row == 5) {
    int gain = p->row == 4;
    const char *key = gain ? "gain" : "pan";
    double v = number(field(lane, key), gain && p->lane == 0);
    set(lane, key,
        cJSON_CreateNumber(clamp(v + .05 * d, gain ? 0 : -1, gain ? 2 : 1)));
  } else if (p->row == 6 || p->row == 7) {
    cJSON *a = array(lane, "inserts");
    int slot = p->row - 6;
    while (cJSON_GetArraySize(a) < slot)
      cJSON_AddItemToArray(a, cJSON_CreateString("bypass"));
    int old = index_of(effects, 6, string(elem(a, slot), "bypass"));
    set_index(a, slot, cJSON_CreateString(effects[(old + d + 6) % 6]));
  } else if (p->row == 8)
    p->frame = limit(p->frame + d, 1);
  else if (p->row == 9)
    p->partial = limit(p->partial + d, 15);
  else if (p->row == 10) {
    cJSON *frames = array(lane, "harmonics");
    while (cJSON_GetArraySize(frames) < 2) {
      cJSON *a = cJSON_CreateArray();
      cJSON_AddItemToArray(a, cJSON_CreateNumber(1));
      cJSON_AddItemToArray(frames, a);
    }
    cJSON *a = elem(frames, p->frame);
    while (cJSON_GetArraySize(a) <= p->partial)
      cJSON_AddItemToArray(a, cJSON_CreateNumber(0));
    set_index(a, p->partial,
              cJSON_CreateNumber(
                  clamp(number(elem(a, p->partial), 0) + .05 * d, -2, 2)));
  } else {
    cJSON *a = array(p->doc, "envelope");
    if (p->row == 11)
      p->point = limit(p->point + d, 127);
    while (cJSON_GetArraySize(a) <= p->point) {
      int n = cJSON_GetArraySize(a);
      double t = n ? number(field(elem(a, n - 1), "time"), 0) + 1 : 0;
      cJSON *pt = cJSON_CreateObject();
      set(pt, "time", cJSON_CreateNumber(t));
      set(pt, "value", cJSON_CreateNumber(1));
      cJSON_AddItemToArray(a, pt);
    }
    cJSON *pt = elem(a, p->point);
    if (p->row == 12 && p->point) {
      double lo = number(field(elem(a, p->point - 1), "time"), 0) + .001;
      double hi = number(field(elem(a, p->point + 1), "time"), 3600.001) - .001;
      set(pt, "time",
          cJSON_CreateNumber(
              clamp(number(field(pt, "time"), lo) + .05 * d, lo, hi)));
    }
    if (p->row == 13)
      set(pt, "value",
          cJSON_CreateNumber(
              clamp(number(field(pt, "value"), 1) + .05 * d, 0, 1)));
  }
}
int panel_copy(const BluePanel *p, BlueEditor *ed) {
  char *text = cJSON_Print(p->doc);
  if (!text)
    return 0;
  size_t n = strlen(text);
  char *copy = realloc(ed->text, n + 1);
  if (copy) {
    memcpy(copy, text, n + 1);
    ed->text = copy;
    ed->length = n;
    ed->capacity = n + 1;
    ed->cursor = 0;
  }
  cJSON_free(text);
  return copy != NULL;
}
