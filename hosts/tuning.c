#include "tuning.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char blue_keys[BLUE_KEYS + 1] = "zxcvbnmasdfghjklqwertyuiop1234567890";

float blue_tuned_pitch(const BlueTuning *tuning, int key) {
  return tuning->root + 12 * (key / tuning->count) +
         tuning->cents[key % tuning->count] / 100.f;
}
static int valid(const BlueTuning *tuning) {
  if (tuning->root < 0 || tuning->root > 127 || tuning->count < 4 ||
      tuning->count > BLUE_TUNING_STEPS || tuning->cents[0] != 0)
    return 0;
  for (int i = 1; i < tuning->count; i++)
    if (!isfinite(tuning->cents[i]) ||
        tuning->cents[i] <= tuning->cents[i - 1] || tuning->cents[i] >= 1200)
      return 0;
  return blue_tuned_pitch(tuning, BLUE_KEYS - 1) <= 127;
}
int blue_read_tunings(const char *path, BlueTuning *tunings) {
  tunings[0] = (BlueTuning){
      "C major (C Dur)", 36, 7, {0, 200, 400, 500, 700, 900, 1100}};
  FILE *f = fopen(path, "rb");
  if (!f)
    return 1;
  char text[8192];
  size_t length = fread(text, 1, sizeof text - 1, f);
  int complete = feof(f) && !ferror(f);
  fclose(f);
  text[length] = 0;
  cJSON *array = complete ? cJSON_ParseWithOpts(text, NULL, 1) : NULL, *item;
  int count = 0;
  cJSON_ArrayForEach(item, array) {
    BlueTuning t = {0};
    const char *name =
        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item, "name"));
    cJSON *root = cJSON_GetObjectItemCaseSensitive(item, "root");
    cJSON *cents = cJSON_GetObjectItemCaseSensitive(item, "cents");
    t.count = cJSON_GetArraySize(cents);
    if (count == BLUE_TUNINGS || !name || !*name ||
        strlen(name) >= sizeof t.name || !cJSON_IsNumber(root) ||
        root->valuedouble != root->valueint || !cJSON_IsArray(cents) ||
        t.count > BLUE_TUNING_STEPS)
      continue;
    t.root = root->valueint;
    for (int i = 0; i < t.count; i++) {
      cJSON *step = cJSON_GetArrayItem(cents, i);
      t.cents[i] = cJSON_IsNumber(step) ? (float)step->valuedouble : NAN;
    }
    if (valid(&t)) {
      strcpy(t.name, name);
      tunings[count++] = t;
    }
  }
  cJSON_Delete(array);
  return count ? count : 1;
}
int blue_parse_tuning(const char *root, const char *steps, BlueTuning *tuning) {
  BlueTuning t = {.name = "Custom"};
  char *end;
  long base = strtol(root, &end, 10);
  if (end == root || end[strspn(end, " \t\r\n")] || base < 0 || base > 127)
    return 0;
  t.root = (int)base;
  steps += strspn(steps, " \t\r\n");
  while (*steps && t.count < BLUE_TUNING_STEPS) {
    float cents = strtof(steps, &end);
    if (steps == end || (*end && !strchr(", \t\r\n", *end)))
      return 0;
    t.cents[t.count++] = cents;
    steps = end + strspn(end, " \t\r\n");
    if (*steps == ',') {
      steps++;
      steps += strspn(steps, " \t\r\n");
      if (!*steps)
        return 0;
    }
  }
  if (*steps || !valid(&t))
    return 0;
  *tuning = t;
  return 1;
}
void blue_pitch_name(float pitch, char *output, size_t size) {
  static const char *const names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                      "F#", "G",  "G#", "A",  "A#", "B"};
  int tenths = (int)lroundf(pitch * 1000.f);
  int note = tenths / 1000;
  float cents = (tenths % 1000) / 10.f;
  if (cents)
    snprintf(output, size, "%s%d+%g", names[note % 12], note / 12 - 1, cents);
  else
    snprintf(output, size, "%s%d", names[note % 12], note / 12 - 1);
}
