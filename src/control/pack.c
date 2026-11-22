#include "cJSON.h"
#include "control.h"
#include "lauxlib.h"
#include "lua.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  size_t used;
  BluePatch *patch;
} Script;
static void *limited(void *ud, void *ptr, size_t old, size_t n) {
  Script *s = ud;
  if (!ptr)
    old = 0;
  if (!n) {
    free(ptr);
    s->used -= old;
    return NULL;
  }
  if (n > 524288 || s->used - old > 524288 - n)
    return NULL;
  void *p = realloc(ptr, n);
  if (p)
    s->used = s->used - old + n;
  return p;
}
static void budget(lua_State *L, lua_Debug *ar) {
  (void)ar;
  luaL_error(L, "instruction budget exceeded");
}
static int route(lua_State *L) {
  static const char *const dest[] = {"gain", "morph", "cutoff", "density",
                                     NULL};
  Script *s;
  lua_getallocf(L, (void **)&s);
  lua_Integer index = luaL_checkinteger(L, 1);
  int k = luaL_checkoption(L, 2, NULL, dest);
  double val = luaL_checknumber(L, 3);
  if (index < 1 || index > 8 || !isfinite(val) || fabs(val) > 16)
    return luaL_error(L, "invalid macro route");
  s->patch->routes[index - 1][k] = (float)val;
  return 0;
}
static int modulation(lua_State *L) {
  static const char *const sources[] = {
      "macro1",   "macro2",   "macro3", "macro4", "macro5", "macro6",
      "macro7",   "macro8",   "lfo1",   "lfo2",   "lfo3",   "envelope",
      "velocity", "pressure", "wheel",  NULL};
  static const char *const targets[] = {"level",  "pan",   "pitch",
                                        "cutoff", "morph", "density",
                                        "send",   "space", NULL};
  Script *script;
  lua_getallocf(L, (void **)&script);
  BluePatch *patch = script->patch;
  int source = luaL_checkoption(L, 1, NULL, sources),
      lane = (int)luaL_checkinteger(L, 2);
  int destination = luaL_checkoption(L, 3, NULL, targets);
  double amount = luaL_checknumber(L, 4);
  if (lane < 1 || lane > 3 || !isfinite(amount) || fabs(amount) > 48 ||
      patch->route_count == 32)
    return luaL_error(L,
                      "invalid modulation route or 32-route budget exceeded");
  patch->matrix[patch->route_count++] =
      (BlueRoute){(unsigned char)source, (unsigned char)(lane - 1),
                  (unsigned char)destination, (float)amount};
  return 0;
}
static int scheduled(lua_State *L) {
  Script *s;
  lua_getallocf(L, (void **)&s);
  BluePatch *p = s->patch;
  double sec = luaL_checknumber(L, 1), v = luaL_checknumber(L, 3);
  lua_Integer i = luaL_checkinteger(L, 2);
  if (p->step_count == 64 || !isfinite(sec) || sec < 0 || sec > 3600 || i < 1 ||
      i > 8 || !(v >= 0 && v <= 1) ||
      (p->step_count && sec < p->steps[p->step_count - 1].seconds))
    return luaL_error(L, "invalid scheduled macro");
  int n = p->step_count++;
  p->steps[n].seconds = (float)sec;
  p->steps[n].value = (float)v;
  p->steps[n].macro = (int)i - 1;
  return 0;
}
static int panel(lua_State *L) {
  Script *s;
  lua_getallocf(L, (void **)&s);
  lua_Integer id = luaL_checkinteger(L, 1);
  size_t n;
  const char *text = luaL_checklstring(L, 2, &n);
  if (id < 1 || id > 8 || !n || n >= 32)
    return luaL_error(L, "invalid control");
  memcpy(s->patch->labels[id - 1], text, n + 1);
  return 0;
}
static const char *const modes[] = {"sine", "saw",    "table", "fm",
                                    "ring", "sample", "grain", NULL};
static const char *const inserts[] = {
    "bypass", "lowpass", "highpass", "bandpass", "shape", "comb", NULL};
static int choice(const char *const *list, const char *name) {
  if (name)
    for (int i = 0; list[i]; i++)
      if (!strcmp(name, list[i]))
        return i;
  return -1;
}
static int layer(lua_State *L) {
  Script *s;
  lua_getallocf(L, (void **)&s);
  lua_Integer i = luaL_checkinteger(L, 1);
  int mode = luaL_checkoption(L, 2, NULL, modes);
  double gain = luaL_checknumber(L, 3);
  if (i < 1 || i > 3 || !isfinite(gain) || gain < 0 || gain > 2)
    return luaL_error(L, "invalid lane");
  s->patch->lane[i - 1].source = mode;
  s->patch->lane[i - 1].gain = (float)gain;
  return 0;
}
static int safe(const char *s) {
  if (!s || !*s || *s == '/' || strstr(s, "..") || strstr(s, "//"))
    return 0;
  for (; *s; s++)
    if (!isalnum((unsigned char)*s) && !strchr("-_./", *s))
      return 0;
  return 1;
}
static char *read_text(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return NULL;
  }
  long n = ftell(f);
  if (n < 0 || n > 1048576) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char *s = malloc((size_t)n + 1);
  if (s && fread(s, 1, n, f) != (size_t)n) {
    free(s);
    s = NULL;
  }
  if (s)
    s[n] = 0;
  fclose(f);
  return s;
}
static int unique_keys(const cJSON *o) {
  if (cJSON_IsObject(o))
    for (const cJSON *c = o->child; c; c = c->next)
      for (const cJSON *d = c->next; d; d = d->next)
        if (!strcmp(c->string, d->string))
          return 0;
  for (const cJSON *c = o ? o->child : NULL; c; c = c->next)
    if (!unique_keys(c))
      return 0;
  return 1;
}
static cJSON *json(const char *path) {
  char *s = read_text(path);
  cJSON *j = s ? cJSON_ParseWithOpts(s, NULL, 1) : NULL;
  free(s);
  return j;
}
#define FIELD(o, k) cJSON_GetObjectItemCaseSensitive(o, k)
static void number(cJSON *o, const char *k, float *dst, double min, double max,
                   int *ok) {
  cJSON *v = FIELD(o, k);
  if (!v)
    return;
  if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < min ||
      v->valuedouble > max) {
    *ok = 0;
    return;
  }
  *dst = (float)v->valuedouble;
}
static int path_for(char *path, size_t n, const char *root, const char *rel) {
  if (!safe(rel))
    return 0;
  int m = snprintf(path, n, "%s/%s", root, rel);
  return m >= 0 && (size_t)m < n;
}
int blue_catalog(const char *root, BluePreset *items, int capacity) {
  int used = 0;
  char path[2048], folder[1024];
  if (!path_for(path, sizeof path, root, "index.json"))
    return 0;
  cJSON *index = json(path);
  cJSON *entry;
  cJSON_ArrayForEach(entry, FIELD(index, "packs")) {
    if (!path_for(folder, sizeof folder, root, cJSON_GetStringValue(entry)) ||
        !path_for(path, sizeof path, folder, "pack.json"))
      continue;
    cJSON *pack = json(path);
    cJSON *preset;
    cJSON_ArrayForEach(preset, FIELD(pack, "presets")) {
      if (used >= capacity || !preset->string ||
          strlen(preset->string) >= sizeof items[0].name)
        continue;
      strcpy(items[used].folder, folder);
      strcpy(items[used++].name, preset->string);
    }
    cJSON_Delete(pack);
  }
  cJSON_Delete(index);
  return used;
}
int blue_source_path(const char *folder, const char *preset, int script,
                     char *path, size_t size) {
  char manifest[2048];
  if (!path_for(manifest, sizeof manifest, folder, "pack.json"))
    return 0;
  cJSON *pack = json(manifest);
  const char *rel = cJSON_GetStringValue(
      script ? FIELD(pack, "script") : FIELD(FIELD(pack, "presets"), preset));
  int ok = path_for(path, size, folder, rel);
  cJSON_Delete(pack);
  return ok;
}
int blue_load(BluePatch *out, const char *folder, const char *preset, char *err,
              size_t size) {
  return blue_compile(out, folder, preset, NULL, NULL, err, size);
}
int blue_compile(BluePatch *out, const char *folder, const char *preset,
                 const char *patch_text, const char *lua_text, char *err,
                 size_t size) {
  char path[2048] = "pack.json";
  int ok = 0, valid = 1;
  cJSON *pack = NULL, *doc = NULL;
  BluePatch *p = malloc(sizeof *p);
  const char *reason = "invalid or missing document";
  char script_error[180] = "";
  if (!p || !folder || !preset ||
      !path_for(path, sizeof path, folder, "pack.json"))
    goto end;
  blue_defaults(p);
  pack = json(path);
  if (!pack || !unique_keys(pack) ||
      cJSON_GetNumberValue(FIELD(pack, "format")) != 1 ||
      cJSON_GetNumberValue(FIELD(pack, "luaApi")) != 1)
    goto end;
  const char *rel = cJSON_GetStringValue(FIELD(FIELD(pack, "presets"), preset));
  if (!path_for(path, sizeof path, folder, rel))
    goto end;
  doc = patch_text ? cJSON_ParseWithOpts(patch_text, NULL, 1) : json(path);
  double version = cJSON_GetNumberValue(FIELD(doc, "format"));
  if (!doc || !unique_keys(doc) || (version != 0 && version != 1))
    goto end;
  if (version == 0)
    number(doc, "volume", &p->gain, 0, 1, &valid);
  cJSON *lanes = FIELD(doc, "lanes");
  int count = cJSON_GetArraySize(lanes);
  if (count < 1 || count > 3)
    goto end;
#define NUM(obj, field, lo, hi) number(obj, #field, &p->field, lo, hi, &valid)
  NUM(doc, gain, 0, 1);
  NUM(doc, attack, 0, 30);
  NUM(doc, release, 0, 30);
  NUM(doc, bpm, 20, 300);
  NUM(doc, delay, .01, 4);
  NUM(doc, feedback, 0, .98);
  NUM(doc, reverb, 0, .98);
  NUM(doc, echo_pitch, -24, 24);
  for (int i = 0; i < count; i++) {
    cJSON *o = cJSON_GetArrayItem(lanes, i);
    BlueLane *l = &p->lane[i];
    const char *source_id = cJSON_GetStringValue(FIELD(o, "source"));
    l->source = choice(modes, source_id);
    if (l->source < 0) {
      l->type_key = blue_node_key(source_id);
      const BlueNodeType *type = blue_node(l->type_key);
      if (!type || strcmp(type->type_id, source_id)) {
        snprintf(script_error, sizeof script_error, "unknown node %s",
                 source_id ? source_id : "(missing)");
        reason = script_error;
        goto end;
      }
      l->source = CUSTOM;
      l->parameter = type->initial;
      number(o, "parameter", &l->parameter, type->minimum, type->maximum,
             &valid);
    }
#define LNUM(k, lo, hi) number(o, #k, &l->k, lo, hi, &valid)
    LNUM(gain, 0, 2);
    LNUM(pan, -1, 1);
    LNUM(ratio, .01, 32);
    LNUM(depth, 0, 32);
    LNUM(cutoff, 10, 24000);
    LNUM(decay, 0, 60);
    LNUM(env_cutoff, -1, 12);
    float root = 60;
    number(o, "root", &root, 0, 127, &valid);
    l->root = (int)root;
    LNUM(send, 0, 1);
    LNUM(space, 0, 1);
    LNUM(morph, 0, 1);
    LNUM(position, 0, 1);
    LNUM(scan, -4, 4);
    if (cJSON_IsTrue(FIELD(o, "freeze")))
      l->scan = 0;
    LNUM(scatter, 0, 1);
    LNUM(size, .001, 1);
    LNUM(density, 1, 256);
    cJSON *ins = FIELD(o, "inserts");
    if (cJSON_GetArraySize(ins) > 2)
      goto end;
    for (int k = 0; k < cJSON_GetArraySize(ins); k++) {
      l->insert[k] =
          choice(inserts, cJSON_GetStringValue(cJSON_GetArrayItem(ins, k)));
      if (l->insert[k] < 0)
        goto end;
    }
    cJSON *waves = FIELD(o, "harmonics");
    for (int k = 0; k < cJSON_GetArraySize(waves); k++) {
      cJSON *row = cJSON_GetArrayItem(waves, k);
      if (k >= 2 || cJSON_GetArraySize(row) > 16)
        goto end;
      memset(l->harmonic[k], 0, sizeof l->harmonic[k]);
      for (int h = 0; h < cJSON_GetArraySize(row); h++) {
        double x = cJSON_GetNumberValue(cJSON_GetArrayItem(row, h));
        if (!isfinite(x) || fabs(x) > 2)
          goto end;
        l->harmonic[k][h] = (float)x;
      }
    }
  }
  cJSON *env = FIELD(doc, "envelope");
  p->points = cJSON_GetArraySize(env);
  if (p->points > BLUE_POINTS)
    goto end;
  for (int i = 0; i < p->points; i++) {
    cJSON *o = cJSON_GetArrayItem(env, i);
    BluePoint *pt = &p->envelope[i];
    pt->curve = 1;
    number(o, "time", &pt->time, 0, 3600, &valid);
    number(o, "value", &pt->value, 0, 1, &valid);
    number(o, "curve", &pt->curve, .05, 20, &valid);
    if ((i == 0 && pt->time != 0) || (i && pt->time <= p->envelope[i - 1].time))
      goto end;
  }
  if (FIELD(doc, "loop")) {
    double x = cJSON_GetNumberValue(FIELD(doc, "loop"));
    if (!isfinite(x) || x < 0 || x >= p->points || floor(x) != x)
      goto end;
    p->loop = (int)x;
  }
  p->sync = cJSON_IsTrue(FIELD(doc, "sync"));
  cJSON *macros = FIELD(doc, "macros");
  for (int i = 0; i < cJSON_GetArraySize(macros); i++) {
    double v = cJSON_GetNumberValue(cJSON_GetArrayItem(macros, i));
    if (i >= 8 || !isfinite(v) || v < 0 || v > 1)
      goto end;
    p->macro[i] = (float)v;
  }
  float seed = 37;
  number(doc, "seed", &seed, 0, 16777215, &valid);
  p->seed = (unsigned)seed;
  cJSON *speeds = FIELD(doc, "lfo");
  for (int i = 0; i < cJSON_GetArraySize(speeds); i++) {
    double hz = cJSON_GetNumberValue(cJSON_GetArrayItem(speeds, i));
    if (i >= 3 || !isfinite(hz) || hz < 0 || hz > 30)
      goto end;
    p->lfo[i] = (float)hz;
  }
  if (!valid)
    goto end;
  if (FIELD(doc, "sample")) {
    rel = cJSON_GetStringValue(FIELD(
        FIELD(pack, "assets"), cJSON_GetStringValue(FIELD(doc, "sample"))));
    if (!path_for(path, sizeof path, folder, rel) || !blue_read_sample(path, p))
      goto end;
    float start = 0, end = (float)p->sample_count;
    number(doc, "loop_start", &start, 0, p->sample_count - 1, &valid);
    number(doc, "loop_end", &end, 1, p->sample_count, &valid);
    if (!valid || end <= start)
      goto end;
    p->loop_start = (int)start;
    p->loop_end = (int)end;
  }
  rel = cJSON_GetStringValue(FIELD(pack, "script"));
  if (!path_for(path, sizeof path, folder, rel))
    goto end;
  char *src = lua_text ? NULL : read_text(path);
  if (!src && !lua_text)
    goto end;
  Script state = {0, p};
  lua_State *L = lua_newstate(limited, &state);
  if (!L) {
    free(src);
    goto end;
  }
  lua_newtable(L);
  lua_pushcfunction(L, modulation);
  lua_setfield(L, -2, "mod");
  lua_pushcfunction(L, route);
  lua_setfield(L, -2, "route");
  lua_pushcfunction(L, layer);
  lua_setfield(L, -2, "lane");
  lua_pushcfunction(L, panel);
  lua_setfield(L, -2, "control");
  lua_pushcfunction(L, scheduled);
  lua_setfield(L, -2, "at");
  lua_setglobal(L, "blue");
  lua_sethook(L, budget, LUA_MASKCOUNT, 50000);
  int rc = luaL_loadbufferx(L, lua_text ? lua_text : src,
                            strlen(lua_text ? lua_text : src), rel, "t");
  free(src);
  if (!rc)
    rc = lua_pcall(L, 0, 0, 0);
  if (rc) {
    snprintf(script_error, sizeof script_error, "%s", lua_tostring(L, -1));
    reason = script_error;
  }
  lua_close(L);
  if (rc)
    goto end;
  int solo = 0;
  for (int i = 0; i < cJSON_GetArraySize(lanes); i++) {
    cJSON *lane = cJSON_GetArrayItem(lanes, i);
    cJSON *mute = FIELD(lane, "mute"), *selected = FIELD(lane, "solo");
    if ((mute && !cJSON_IsBool(mute)) || (selected && !cJSON_IsBool(selected)))
      goto end;
    solo |= cJSON_IsTrue(selected);
  }
  for (int i = 0; i < BLUE_LANES; i++) {
    cJSON *lane = cJSON_GetArrayItem(lanes, i);
    if (cJSON_IsTrue(FIELD(lane, "mute")) ||
        (solo && !cJSON_IsTrue(FIELD(lane, "solo"))))
      p->lane[i].gain = 0;
  }
  blue_prepare_waves(p);
  *out = *p;
  ok = 1;
end:
  if (!ok && size)
    snprintf(err, size, "%s (%s): %s", path, preset ? preset : "", reason);
  free(p);
  cJSON_Delete(pack);
  cJSON_Delete(doc);
  return ok;
}
