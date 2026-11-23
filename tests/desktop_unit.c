/* Unit driver for the native host. Device callbacks, the event source and clock
   are supplied by the test. Production DSP and physical I/O get separate tests.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SDL_AudioSpec requested;
static int mx, my, scenario, position, between, saw_audio, errors;
static const char *settings;
static SDL_AudioDeviceID device_open(const char *name, int capture,
                                     const SDL_AudioSpec *in,
                                     SDL_AudioSpec *out, int flags) {
  (void)name;
  (void)capture;
  (void)flags;
  requested = *in;
  *out = *in;
  if (scenario == 1)
    return 0;
  if (scenario == 2)
    out->freq = 0;
  return 3;
}
static void clock_tick(unsigned ms) {
  (void)ms;
  float audio[256 * 2];
  if (requested.callback) {
    requested.callback(requested.userdata, (Uint8 *)audio, sizeof audio);
    for (unsigned k = 0; k < 512; k++)
      if (fabsf(audio[k]) > .0001f)
        saw_audio = 1;
  }
}
static Uint32 mouse_position(int *x, int *y) {
  *x = mx;
  *y = my;
  return SDL_BUTTON_LMASK;
}
static void device_noop(SDL_AudioDeviceID id) { (void)id; }
static void pause_noop(SDL_AudioDeviceID id, int paused) {
  (void)id;
  (void)paused;
}
static int virtual_devices(int capture) { return capture ? 0 : 1; }
static const char *device_name(int i, int capture) {
  (void)i;
  (void)capture;
  return "unit output";
}
static char *settings_path(const char *org, const char *app) {
  (void)org;
  (void)app;
  return SDL_strdup(settings);
}
static int next_event(SDL_Event *ev) {
  if (between) {
    between = 0;
    return 0;
  }
  between = 1;
  memset(ev, 0, sizeof *ev);
  static const SDL_Keycode actions[] = {SDLK_LEFT,        SDLK_F10,
                                        SDLK_r,           SDLK_r,
                                        SDLK_F10,         SDLK_F5,
                                        SDLK_RIGHT,       SDLK_F9,
                                        SDLK_F11,         SDLK_F2,
                                        SDLK_F3,          SDLK_F4,
                                        SDLK_END,         SDLK_HOME,
                                        SDLK_DOWN,        SDLK_UP,
                                        SDLK_RIGHT,       SDLK_LEFT,
                                        SDLK_s,           SDLK_ESCAPE,
                                        SDLK_F6,          SDLK_RETURN,
                                        SDLK_BACKSPACE,   SDLK_DELETE,
                                        SDLK_ESCAPE,      SDLK_RIGHTBRACKET,
                                        SDLK_LEFTBRACKET, SDLK_F7,
                                        SDLK_F8,          SDLK_DOWN,
                                        SDLK_RIGHT,       SDLK_s,
                                        SDLK_ESCAPE,      SDLK_F12,
                                        SDLK_DOWN,        SDLK_RETURN,
                                        SDLK_F12,         SDLK_UP,
                                        SDLK_UP,          SDLK_RETURN,
                                        SDLK_END,         SDLK_TAB,
                                        SDLK_RETURN,      SDLK_F12,
                                        SDLK_ESCAPE,      SDLK_0};
  if (position < (int)(sizeof actions / sizeof *actions)) {
    ev->type = position == 3 ? SDL_KEYUP : SDL_KEYDOWN;
    ev->key.keysym.sym = actions[position];
    if (ev->key.keysym.sym == SDLK_s)
      ev->key.keysym.mod = KMOD_CTRL;
  } else
    switch (position - (int)(sizeof actions / sizeof *actions)) {
    case 0:
      ev->type = SDL_MOUSEBUTTONDOWN;
      mx = 400;
      my = 200;
      break;
    case 1:
      ev->type = SDL_MOUSEMOTION;
      ev->motion.state = SDL_BUTTON_LMASK;
      mx = 500;
      my = 240;
      break;
    case 2:
      ev->type = SDL_MOUSEBUTTONDOWN;
      mx = 150;
      my = 320;
      break;
    case 3:
      ev->type = SDL_MOUSEBUTTONUP;
      break;
    case 4:
      ev->type = SDL_WINDOWEVENT;
      ev->window.event = SDL_WINDOWEVENT_FOCUS_LOST;
      break;
    case 5:
      ev->type = SDL_MOUSEBUTTONDOWN;
      mx = 353;
      my = 268;
      break;
    case 6:
      ev->type = SDL_TEXTINPUT;
      strcpy(ev->text.text, "127");
      break;
    case 7:
      ev->type = SDL_KEYDOWN;
      ev->key.keysym.sym = SDLK_RETURN;
      break;
    case 8:
      ev->type = SDL_KEYDOWN;
      ev->key.keysym.sym = SDLK_ESCAPE;
      break;
    case 9:
      ev->type = SDL_AUDIODEVICEREMOVED;
      ev->adevice.which = 3;
      break;
    default:
      ev->type = SDL_QUIT;
      break;
    }
  position++;
  return 1;
}
static SDL_Window *test_window(const char *label, int x, int y, int w, int h,
                               Uint32 flags) {
  return scenario == 3 ? NULL : SDL_CreateWindow(label, x, y, w, h, flags);
}
static char *test_base(void) {
  return scenario == 4 ? SDL_strdup("absent-dir/") : SDL_GetBasePath();
}
static int test_init(Uint32 subsystems) {
  return scenario == 5 ? -1 : SDL_Init(subsystems);
}
#define SDL_CreateWindow test_window
#define SDL_GetBasePath test_base
#define SDL_Init test_init
#define SDL_PollEvent next_event
#define SDL_GetMouseState mouse_position
#define SDL_OpenAudioDevice device_open
#define SDL_CloseAudioDevice device_noop
#define SDL_LockAudioDevice device_noop
#define SDL_UnlockAudioDevice device_noop
#define SDL_PauseAudioDevice pause_noop
#define SDL_GetNumAudioDevices virtual_devices
#define SDL_GetAudioDeviceName device_name
#define SDL_GetPrefPath settings_path
#define SDL_Delay clock_tick
#define main desktop_main
#include "../hosts/desktop.c"
#undef main
void blue_midi_close(void) {}
int blue_midi_open(void) { return 1; }
void blue_midi_read(BlueMidiSink sink, void *ctx) {
  if (position == 0)
    sink(ctx, WHEEL, 0, .25f);
}
int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
  settings = argv[3];
  char zip[2048];
  snprintf(zip, sizeof zip, "%s/unit-export.zip", argv[3]);
  remove(zip);
  char *args[] = {argv[0], "--content-root", argv[1],      "--record-to",
                  argv[2], "--export-to",    zip,          "--user-dir",
                  argv[3], "--preset",       "Calibration"};
  if (desktop_main(11, args) != 0 || !saw_audio)
    return 1;
  FILE *wav = fopen(argv[2], "rb");
  if (!wav)
    return 1;
  char id[4];
  int valid = fread(id, 1, 4, wav) == 4 && !memcmp(id, "RIFF", 4);
  fclose(wav);
  if (!valid)
    return 1;
  SDL_Init(SDL_INIT_VIDEO);
  App tuning_app = {.tuning_count = 1, .user_dir = argv[3]};
  custom_tuning(&tuning_app, 0);
  if (tuning_app.tunings[1].root != 40 || tuning_app.tunings[1].cents[2] != 300)
    return 1;
  BlueTuning catalog[BLUE_TUNINGS], custom;
  char tunings_path[2048];
  snprintf(tunings_path, sizeof tunings_path, "%s/tunings.json", argv[1]);
  if (blue_read_tunings(tunings_path, catalog) != 6 ||
      blue_tuned_pitch(&catalog[0], 35) != 96 ||
      blue_tuned_pitch(&catalog[1], 2) != 43 ||
      blue_tuned_pitch(&catalog[3], 2) != 39.5f ||
      blue_read_tunings("missing-tuning", catalog) != 1)
    return 1;
  if (!blue_parse_tuning(" 24 ", "0, 200 350, 700", &custom) ||
      blue_tuned_pitch(&custom, 2) != 27.5f ||
      blue_tuned_pitch(&custom, 35) != 127)
    return 1;
  static const char *invalid_steps[] = {
      "20 200 350 700", "0 200 199 700", "0 200 350 1200", "0 200 350 nan",
      "0 200 350 700,", "0 x 350 700",   "0 200"};
  for (unsigned i = 0; i < sizeof invalid_steps / sizeof *invalid_steps; i++)
    if (blue_parse_tuning("24", invalid_steps[i], &custom))
      return 1;
  if (blue_parse_tuning("71", "0 200 350 700", &custom) ||
      blue_parse_tuning("2.5", "0 200 350 700", &custom))
    return 1;
  char micro[16];
  blue_pitch_name(39.5f, micro, sizeof micro);
  if (strcmp(micro, "D#2+50"))
    return 1;
  BluePreset preset = {0};
  snprintf(preset.folder, sizeof preset.folder, "%s/packs/blue.factory",
           argv[1]);
  strcpy(preset.name, "Calibration");
  BlueEditor ed = {0};
  if (!editor_open(&ed, &preset, 1))
    return 1;
  editor_key(&ed, SDLK_UP, 0);
  editor_key(&ed, SDLK_BACKSPACE, 0);
  editor_key(&ed, SDLK_c, KMOD_CTRL);
  editor_key(&ed, SDLK_v, KMOD_CTRL);
  ed.cursor = ed.length;
  editor_key(&ed, SDLK_DELETE, 0);
  editor_key(&ed, SDLK_UP, 0);
  editor_key(&ed, SDLK_DOWN, 0);
  char large[2000];
  memset(large, ' ', sizeof large - 1);
  large[sizeof large - 1] = 0;
  editor_input(&ed, large);
  size_t before = ed.length;
  char *huge = malloc(1048578);
  memset(huge, 'x', 1048577);
  huge[1048577] = 0;
  editor_input(&ed, huge);
  free(huge);
  if (ed.length != before)
    return 1;
  BluePatch *patch = malloc(sizeof *patch);
  char why[2048];
  editor_input(&ed, "=broken);");
  if (editor_save(&ed, &preset, patch, why, sizeof why))
    return 1;
  if (!editor_open(&ed, &preset, 1))
    return 1;
  strcpy(ed.path, "missing-parent/path.lua");
  if (editor_save(&ed, &preset, patch, why, sizeof why))
    return 1;
  strcpy(preset.name, "missing");
  if (editor_open(&ed, &preset, 0))
    return 1;
  editor_close(&ed);
  free(patch);
  SDL_Quit();
  App a = {0};
  a.device = 3;
  a.rate = 48000;
  a.engine = blue_create(48000, 2);
  a.count = 1;
  a.catalog[0] = preset;
  load(&a, 0);
  if (!strstr(a.message, "missing"))
    return 1;
  strcpy(a.catalog[0].name, "Calibration");
  load(&a, 0);
  blue_event(a.engine, NOTE_ON, 69, .8);
  load(&a, 0);
  load(&a, 0);
  if (!a.pending)
    return 1;
  blue_event(a.engine, PANIC, 0, 0);
  float left[2048], right[2048];
  for (int i = 0; i < 16; i++)
    blue_render(a.engine, left, right, 2048);
  apply_pending(&a);
  if (a.pending)
    return 1;
  blue_destroy(a.engine);
  for (scenario = 1; scenario <= 5; scenario++) {
    if (!desktop_main(11, args))
      return 1;
    SDL_Quit();
  }
  args[2] = "absent-pack";
  if (!desktop_main(11, args))
    return 1;
  SDL_Quit();
  BluePanel panel = {0};
  if (panel_open(&panel, "{}") ||
      !panel_open(&panel, "{\"lanes\":[{\"source\":\"sine\"}]}"))
    return 1;
  panel.row = 1;
  panel_change(&panel, 1);
  panel.row = 4;
  panel_change(&panel, 1);
  char fixture_path[2048];
  snprintf(fixture_path, sizeof fixture_path, "%s/editor-gesture.json",
           argv[3]);
  char *expected = SDL_LoadFile(fixture_path, NULL);
  cJSON *golden = cJSON_Parse(expected);
  SDL_free(expected);
  int same = cJSON_Compare(panel.doc, golden, 1);
  cJSON_Delete(golden);
  if (!same)
    return 1;
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < PANEL_ROWS; i++) {
      panel.row = i;
      char value[64];
      panel_value(&panel, i, value, sizeof value);
      panel_change(&panel, 1);
      panel_value(&panel, i, value, sizeof value);
      if (!*value)
        return 1;
    }
  BlueEditor draft = {0};
  if (!panel_copy(&panel, &draft) || !strstr(draft.text, "harmonics"))
    return 1;
  free(draft.text);
  panel_close(&panel);
  return errors;
}
