#define SDL_MAIN_HANDLED
#include "archive.h"
#include "blue_queue.h"
#include "control.h"
#include "editor.h"
#include "midi.h"
#include "panel.h"
#include "tuning.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RATE 48000
#define RECORD_FRAMES (60 * RATE)
typedef struct {
  SDL_Window *window;
  BlueEditor editor;
  BluePanel panel;
  BlueTuning tunings[BLUE_TUNINGS + 1];
  BlueEditor builder[2];
  unsigned char held[BLUE_KEYS];
  int tuning_count, tuning_selected, tuning_menu, tuning_cursor, tuning_field;
  BlueEngine *engine;
  BlueQueue queue;
  int macro_page, output_index;
  char labels[8][32];
  SDL_AudioDeviceID device;
  float *recording;
  int capturing, recorded, rate, selected;
  float macros[8];
  BluePatch *pending;
  int pending_index;
  BluePreset catalog[128];
  int count;
  const char *wav, *user_dir, *export_to;
  char message[2048];
} App;

static const int key_starts[] = {26, 16, 7, 0};
enum { KEYBOARD_X = 28, KEYBOARD_Y = 307, KEY_WIDTH = 86, KEY_HEIGHT = 26 };
enum { TUNING_MENU_Y = 282, TUNING_ITEM_HEIGHT = 20 };
static void callback(void *ud, Uint8 *stream, int bytes) {
  App *a = ud;
  blue_queue_drain(&a->queue, a->engine);
  float *out = (float *)stream;
  int frames = bytes / (2 * sizeof(float));
  for (int pos = 0; pos < frames;) {
    int n = frames - pos > 256 ? 256 : frames - pos;
    float l[256], r[256];
    blue_render(a->engine, l, r, n);
    for (int i = 0; i < n; i++) {
      out[(pos + i) * 2] = l[i];
      out[(pos + i) * 2 + 1] = r[i];
    }
    if (a->capturing && a->recorded + n <= RECORD_FRAMES) {
      memcpy(a->recording + 2 * a->recorded, out + pos * 2,
             n * 2 * sizeof(float));
      a->recorded += n;
    }
    pos += n;
  }
}
static void event(App *a, int c, int key, float v) {
  SDL_LockAudioDevice(a->device);
  blue_queue_push(&a->queue, c, key, v);
  SDL_UnlockAudioDevice(a->device);
}
static void keyboard_note(App *a, int step, int source, int down) {
  unsigned char old = a->held[step];
  a->held[step] = down ? old | source : old & ~source;
  if (!!old == !!a->held[step])
    return;
  event(a, down ? TUNED_NOTE_ON : NOTE_OFF, BLUE_KEY_ID + step,
        down ? blue_tuned_pitch(&a->tunings[a->tuning_selected], step) : 0);
}
static void panic(App *a) {
  memset(a->held, 0, sizeof a->held);
  event(a, PANIC, 0, 0);
}
static void midi_sink(void *ud, int c, int key, float v) {
  event((App *)ud, c, key, v);
}
static void apply_pending(App *a) {
  if (!a->pending)
    return;
  SDL_LockAudioDevice(a->device);
  BluePatch *slot = blue_stage(a->engine);
  SDL_UnlockAudioDevice(a->device);
  if (!slot)
    return;
  *slot = *a->pending;
  SDL_LockAudioDevice(a->device);
  blue_commit(a->engine);
  SDL_UnlockAudioDevice(a->device);
  a->selected = a->pending_index;
  memcpy(a->macros, a->pending->macro, sizeof a->macros);
  memcpy(a->labels, a->pending->labels, sizeof a->labels);
  if (a->window) {
    char title[200];
    snprintf(title, sizeof title, "Blue | %s", a->catalog[a->selected].name);
    SDL_SetWindowTitle(a->window, title);
  }
  snprintf(a->message, sizeof a->message, "Loaded %s",
           a->catalog[a->selected].name);
  for (int k = 0; k < 8; k++)
    event(a, MACRO, k, a->macros[k]);
  free(a->pending);
  a->pending = NULL;
}
static void load_controls(App *a, int which, const float *values) {
  BluePatch *p = malloc(sizeof *p);
  if (!p)
    return;
  if (!blue_load(p, a->catalog[which].folder, a->catalog[which].name,
                 a->message, sizeof a->message)) {
    free(p);
    return;
  }
  if (values)
    memcpy(p->macro, values, sizeof p->macro);
  free(a->pending);
  a->pending = p;
  a->pending_index = which;
  apply_pending(a);
  if (a->pending)
    snprintf(a->message, sizeof a->message,
             "Waiting for the previous notes to release");
}
static void load(App *a, int which) { load_controls(a, which, NULL); }
static void capture(App *a) {
  SDL_LockAudioDevice(a->device);
  int save = a->capturing;
  a->capturing = !save;
  if (!save)
    a->recorded = 0;
  SDL_UnlockAudioDevice(a->device);
  if (save)
    snprintf(a->message, sizeof a->message, "%s",
             blue_write_wav(a->wav, a->recording, a->recorded, a->rate)
                 ? "Recording saved"
                 : "Recording could not be written");
}
static void rect(SDL_Renderer *r, int x, int y, int w, int h, int red,
                 int green, int blue) {
  SDL_Rect b = {x, y, w, h};
  SDL_SetRenderDrawColor(r, red, green, blue, 255);
  SDL_RenderFillRect(r, &b);
}
static void label(SDL_Renderer *r, SDL_Texture *font, int x, int y,
                  const char *str, int scale) {
  for (int i = 0; str[i] && i < 100; i++) {
    int c = (unsigned char)str[i];
    if (c < 32 || c > 126)
      c = '?';
    SDL_Rect src = {(c - 32) * 8, 0, 8, 14},
             dst = {x + i * 8 * scale, y, 8 * scale, 14 * scale};
    SDL_RenderCopy(r, font, &src, &dst);
  }
}
static void draw_tuning(App *a, SDL_Renderer *r, SDL_Texture *font) {
  const BlueTuning *t = &a->tunings[a->tuning_selected];
  char menu[70], pitch[16];
  snprintf(menu, sizeof menu, "Tuning: %s  v", t->name);
  rect(r, 28, 262, 302, 24, 36, 65, 74);
  label(r, font, 34, 268, menu, 1);
  rect(r, 339, 262, 140, 24, 36, 65, 74);
  label(r, font, 349, 268, "Build tuning", 1);
  label(r, font, 496, 268, "F12 opens tuning", 1);
  for (int row = 0; row < 4; row++) {
    int count = row < 2 ? 10 : (row == 2 ? 9 : 7);
    for (int col = 0; col < count; col++) {
      int step = key_starts[row] + col;
      int x = KEYBOARD_X + col * KEY_WIDTH, y = KEYBOARD_Y + row * KEY_HEIGHT;
      int root = step % t->count == 0;
      int light = a->held[step] ? 100 : (root ? 72 : 45);
      rect(r, x, y, KEY_WIDTH - 4, KEY_HEIGHT - 3, 32, light, light + 10);
      char key[2] = {(char)SDL_toupper(blue_keys[step]), 0};
      blue_pitch_name(blue_tuned_pitch(t, step), pitch, sizeof pitch);
      label(r, font, x + 5, y + 4, key, 1);
      label(r, font, x + 24, y + 4, pitch, 1);
    }
  }
  if (a->tuning_menu) {
    int top = TUNING_MENU_Y - (a->tuning_count + 2) * TUNING_ITEM_HEIGHT;
    rect(r, 28, top, 302, TUNING_MENU_Y - top, 18, 33, 44);
    for (int i = 0; i <= a->tuning_count + 1; i++) {
      int y = top + i * TUNING_ITEM_HEIGHT;
      if (i == a->tuning_cursor)
        rect(r, 28, y, 302, TUNING_ITEM_HEIGHT, 37, 80, 89);
      const char *name =
          i <= a->tuning_count ? a->tunings[i].name : "Build tuning...";
      label(r, font, 36, y + 3, name, 1);
    }
  }
  if (a->tuning_field) {
    rect(r, 22, 74, 870, 242, 18, 37, 48);
    label(r, font, 40, 89, "Tuning builder", 2);
    for (int i = 0; i < 2; i++) {
      BlueEditor *field = &a->builder[i];
      int y = 134 + i * 54;
      size_t start = field->cursor > 94 ? field->cursor - 94 : 0;
      rect(r, 39, y + 14, 832, 23, 27, a->tuning_field == i + 1 ? 75 : 53, 75);
      label(r, font, 42, y, i ? "Scale steps in cents" : "Root MIDI note", 1);
      label(r, font, 46, y + 18, field->text + start, 1);
      if (a->tuning_field == i + 1)
        rect(r, 46 + (int)(field->cursor - start) * 8, y + 33, 8, 2, 85, 235,
             221);
    }
    label(r, font, 42, 249,
          "C2 = 36. Use 4-24 ascending steps from 0, below 1200 cents.", 1);
    label(r, font, 42, 278,
          "Tab switches fields    Enter saves Custom    Esc cancels", 1);
  }
}
static void draw(App *a, SDL_Renderer *r, SDL_Texture *font) {
  rect(r, 0, 0, 920, 480, 13, 22, 30);
  if (a->panel.doc) {
    label(r, font, 24, 16, "Sound editor", 2);
    for (int i = 0; i < PANEL_ROWS; i++) {
      int y = 54 + i * 25;
      char value[96];
      panel_value(&a->panel, i, value, sizeof value);
      if (a->panel.row == i)
        rect(r, 18, y - 3, 862, 22, 37, 80, 89);
      label(r, font, 30, y, panel_fields[i], 1);
      label(r, font, 320, y, value, 1);
    }
    label(r, font, 24, 418,
          "Up / Down  Select     Left / Right  Change     Ctrl+S  Apply and "
          "save     Esc  Close",
          1);
    label(r, font, 24, 448, a->message, 1);
    SDL_RenderPresent(r);
    return;
  }
  if (a->editor.text) {
    BlueEditor *e = &a->editor;
    label(r, font, 24, 12, e->script ? "Lua mapping" : "Patch document", 2);
    size_t line = 0;
    for (size_t i = 0; i < e->cursor; i++)
      if (e->text[i] == '\n')
        line++;
    size_t first = line > 19 ? line - 19 : 0;
    const char *start = e->text;
    for (size_t i = 0; i < first; i++) {
      start = strchr(start, '\n') + 1;
    }
    for (int row = 0; row < 22 && start; row++) {
      const char *end = strchr(start, '\n');
      size_t len = end ? (size_t)(end - start) : strlen(start);
      char displayed[101];
      size_t width = len < 100 ? len : 100;
      memcpy(displayed, start, width);
      displayed[width] = 0;
      label(r, font, 24, 55 + row * 16, displayed, 1);
      size_t offset = start - e->text;
      if (e->cursor >= offset && e->cursor <= offset + len)
        rect(r, 24 + (int)((e->cursor - offset) % 101) * 8, 68 + row * 16, 8, 2,
             85, 235, 221);
      start = end ? end + 1 : NULL;
    }
    label(r, font, 24, 423,
          "Ctrl+S  Validate and save source     Esc  Close editor", 1);
    label(r, font, 24, 447, a->message, 1);
    SDL_RenderPresent(r);
    return;
  }
  label(r, font, 28, 18, "BLUE", 3);
  label(r, font, 28, 75, "HYBRID SYNTHESIZER", 1);
  rect(r, 27, 108, 866, 50, 36, 65, 74);
  label(r, font, 46, 122, a->catalog[a->selected].name, 2);
  label(r, font, 610, 126, "LEFT / RIGHT   Change preset", 1);
  for (int k = 0; k < 2; k++) {
    int y = 185 + k * 47;
    int idx = 2 * a->macro_page + k;
    char fallback[20];
    snprintf(fallback, sizeof fallback, "Macro %d", idx + 1);
    label(r, font, 30, y, a->labels[idx][0] ? a->labels[idx] : fallback, 2);
    rect(r, 270, y, 610, 23, 42, 62, 74);
    rect(r, 270, y, (int)(610 * a->macros[idx]), 23, 92, 201, 192);
  }
  draw_tuning(a, r, font);
  label(r, font, 28, 418,
        "F2 Macros   F3 Output   F4 Patch JSON   F5 Save   F6 Lua   F7 Export  "
        " F8 Edit   F9 Recall",
        1);
  label(r, font, 28, 438, "F10 Record     F11 Reload files", 1);
  label(r, font, 28, 458, a->capturing ? "Recording master ..." : a->message,
        1);
  SDL_RenderPresent(r);
}
static void state(App *a, int save) {
  char *dir = a->user_dir ? SDL_strdup(a->user_dir)
                          : SDL_GetPrefPath("Blue", "Instrument");
  if (!dir)
    return;
  char path[2048];
  snprintf(path, sizeof path, "%s/controls.txt", dir);
  SDL_free(dir);
  FILE *f = fopen(path, save ? "w" : "r");
  if (!f)
    return;
  if (save) {
    fprintf(f, "%d", a->selected);
    for (int i = 0; i < 8; i++)
      fprintf(f, " %.9g", a->macros[i]);
    fputc('\n', f);
  } else {
    int selected;
    float values[8] = {0};
    if (fscanf(f, "%d", &selected) == 1 && selected >= 0 &&
        selected < a->count) {
      int count = 0;
      while (count < 8 && fscanf(f, "%f", &values[count]) == 1 &&
             values[count] >= 0 && values[count] <= 1)
        count++;
      /* Earlier builds stored the first two macros. */
      if (count == 8 || (count == 2 && feof(f)))
        load_controls(a, selected, values);
    }
  }
  fclose(f);
}
static void format_steps(const BlueTuning *t, char *text) {
  size_t offset = 0;
  for (int i = 0; i < t->count; i++)
    offset += (size_t)snprintf(text + offset, 512 - offset, "%s%.7g",
                               i ? ", " : "", t->cents[i]);
}
static void custom_tuning(App *a, int save) {
  char *dir = a->user_dir ? SDL_strdup(a->user_dir)
                          : SDL_GetPrefPath("Blue", "Instrument");
  if (!dir)
    return;
  char path[2048], root[32], text[512];
  snprintf(path, sizeof path, "%s/tuning.txt", dir);
  SDL_free(dir);
  FILE *file = fopen(path, save ? "w" : "r");
  if (!file)
    return;
  BlueTuning *t = &a->tunings[a->tuning_count];
  if (save) {
    format_steps(t, text);
    fprintf(file, "%d\n%s\n", t->root, text);
  } else if (fgets(root, sizeof root, file) && fgets(text, sizeof text, file))
    blue_parse_tuning(root, text, t);
  fclose(file);
}
static void close_builder(App *a) {
  editor_close(&a->builder[0]);
  editor_close(&a->builder[1]);
  a->tuning_field = 0;
}
static void open_builder(App *a) {
  const BlueTuning *t = &a->tunings[a->tuning_selected];
  for (int i = 0; i < 2; i++) {
    BlueEditor *field = &a->builder[i];
    field->text = malloc(512);
    if (!field->text) {
      close_builder(a);
      return;
    }
    if (i)
      format_steps(t, field->text);
    else
      snprintf(field->text, 512, "%d", t->root);
    field->capacity = 512;
    field->cursor = field->length = strlen(field->text);
  }
  a->tuning_field = 1;
  SDL_StartTextInput();
}
static int tuning_input(App *a, const SDL_Event *ev) {
  int mouse = ev->type == SDL_MOUSEBUTTONDOWN, x = 0, y = 0;
  if (mouse)
    SDL_GetMouseState(&x, &y);
  SDL_Keycode key = ev->type == SDL_KEYDOWN ? ev->key.keysym.sym : SDLK_UNKNOWN;
  if (a->tuning_field) {
    BlueEditor *field = &a->builder[a->tuning_field - 1];
    if (ev->type == SDL_TEXTINPUT)
      editor_input(field, ev->text.text);
    else if (key == SDLK_ESCAPE)
      close_builder(a);
    else if (key == SDLK_TAB || (mouse && y >= 145 && y < 225))
      a->tuning_field = mouse ? (y >= 188 ? 2 : 1) : 3 - a->tuning_field;
    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      if (blue_parse_tuning(a->builder[0].text, a->builder[1].text,
                            &a->tunings[a->tuning_count])) {
        a->tuning_selected = a->tuning_count;
        custom_tuning(a, 1);
        close_builder(a);
        snprintf(a->message, sizeof a->message, "Custom tuning saved");
      } else
        snprintf(a->message, sizeof a->message,
                 "Invalid root or steps. All 36 keys must fit in MIDI 0-127.");
    } else if (key != SDLK_UNKNOWN)
      editor_key(field, key, ev->key.keysym.mod);
    return 1;
  }
  if (a->tuning_menu) {
    int count = a->tuning_count + 2;
    if (key == SDLK_UP || key == SDLK_DOWN)
      a->tuning_cursor =
          (a->tuning_cursor + count + (key == SDLK_UP ? -1 : 1)) % count;
    else if (key == SDLK_RETURN || mouse) {
      int top = TUNING_MENU_Y - count * TUNING_ITEM_HEIGHT;
      int selected = mouse
                         ? (x >= 28 && x < 330 && y >= top && y < TUNING_MENU_Y
                                ? (y - top) / TUNING_ITEM_HEIGHT
                                : -1)
                         : a->tuning_cursor;
      a->tuning_menu = 0;
      if (selected == count - 1)
        open_builder(a);
      else if (selected >= 0)
        a->tuning_selected = selected;
    } else if (key == SDLK_ESCAPE || key == SDLK_F12)
      a->tuning_menu = 0;
    return 1;
  }
  if (key == SDLK_F12 || (mouse && y >= 262 && y < 286 && x >= 28 && x < 330)) {
    a->tuning_cursor = a->tuning_selected;
    a->tuning_menu = 1;
    return 1;
  }
  if (mouse && y >= 262 && y < 286 && x >= 339 && x < 479) {
    open_builder(a);
    return 1;
  }
  return 0;
}
static int open_output(App *a, const char *name) {
  SDL_AudioSpec want = {0}, have;
  want.freq = RATE;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 256;
  want.callback = callback;
  want.userdata = a;
  SDL_AudioDeviceID device = SDL_OpenAudioDevice(
      name, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (!device) {
    snprintf(a->message, sizeof a->message, "%s", SDL_GetError());
    return 0;
  }
  BlueEngine *engine = blue_create(have.freq, 16);
  if (!engine) {
    SDL_CloseAudioDevice(device);
    return 0;
  }
  if (a->capturing)
    capture(a);
  if (a->device)
    SDL_CloseAudioDevice(a->device);
  blue_destroy(a->engine);
  a->engine = engine;
  a->device = device;
  a->rate = have.freq;
  a->queue.used = 0;
  memset(a->held, 0, sizeof a->held);
  load(a, a->selected);
  SDL_PauseAudioDevice(device, 0);
  return 1;
}
int main(int argc, char **argv) {
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    return 1;
  char *base = SDL_GetBasePath();
  char root[2048], atlas[2048], tuning_path[2048];
  snprintf(root, sizeof root, "%scontent", base ? base : "");
  snprintf(atlas, sizeof atlas, "%scontent/graphics/font.bmp",
           base ? base : "");
  snprintf(tuning_path, sizeof tuning_path, "%scontent/tunings.json",
           base ? base : "");
  SDL_free(base);
  App a = {0};
  a.export_to = "Blue-pack.zip";
  a.wav = "Blue-recording.wav";
  const char *initial = NULL;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "--content-root"))
      snprintf(root, sizeof root, "%s", argv[i + 1]);
    else if (!strcmp(argv[i], "--export-to"))
      a.export_to = argv[i + 1];
    else if (!strcmp(argv[i], "--user-dir"))
      a.user_dir = argv[i + 1];
    else if (!strcmp(argv[i], "--preset"))
      initial = argv[i + 1];
    else if (!strcmp(argv[i], "--record-to"))
      a.wav = argv[i + 1];
  }
  a.tuning_count = blue_read_tunings(tuning_path, a.tunings);
  a.tunings[a.tuning_count] = a.tunings[0];
  strcpy(a.tunings[a.tuning_count].name, "Custom");
  custom_tuning(&a, 0);
  a.count = blue_catalog(root, a.catalog, 128);
  if (initial)
    for (int i = 0; i < a.count; i++)
      if (!strcmp(initial, a.catalog[i].name)) {
        a.selected = i;
        break;
      }
  if (!a.count) {
    fprintf(stderr, "No presets in %s\n", root);
    return 1;
  }
  SDL_Window *win =
      SDL_CreateWindow("Blue | Hybrid synthesizer", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 920, 480, SDL_WINDOW_SHOWN);
  a.window = win;
  SDL_Renderer *r = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
  if (!r) {
    fprintf(stderr, "%s\n", SDL_GetError());
    return 1;
  }

  SDL_Surface *bmp = SDL_LoadBMP(atlas);
  if (!bmp) {
    fprintf(stderr, "%s\n", atlas);
    return 1;
  }
  SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, 0, 0, 0));
  SDL_Texture *font = SDL_CreateTextureFromSurface(r, bmp);
  SDL_FreeSurface(bmp);
  if (!open_output(&a, NULL)) {
    fprintf(stderr, "%s\n", a.message);
    return 1;
  }
  a.recording = calloc(RECORD_FRAMES * 2, sizeof(float));
  if (!a.recording || !a.engine)
    return 1;
  blue_midi_open();

  int quit = 0, mouse = -1;
  while (!quit) {
    apply_pending(&a);
    blue_midi_read(midi_sink, &a);
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_AUDIODEVICEREMOVED && ev.adevice.which == a.device)
        open_output(&a, NULL);
      if (ev.type == SDL_QUIT)
        quit = 1;
      if (ev.type == SDL_WINDOWEVENT &&
          ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        panic(&a);
        mouse = -1;
      }
      SDL_Keycode symbol = ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP
                               ? ev.key.keysym.sym
                               : 0;
      const char *key =
          symbol > 0 && symbol < 128 ? strchr(blue_keys, symbol) : NULL;
      if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) &&
          ev.key.keysym.scancode >= SDL_SCANCODE_1 &&
          ev.key.keysym.scancode <= SDL_SCANCODE_0)
        key =
            blue_keys + key_starts[0] + ev.key.keysym.scancode - SDL_SCANCODE_1;
      if (ev.type == SDL_KEYUP && key)
        keyboard_note(&a, (int)(key - blue_keys), 1, 0);
      if (ev.type == SDL_MOUSEBUTTONUP && mouse >= 0) {
        keyboard_note(&a, mouse, 2, 0);
        mouse = -1;
      }
      if (!a.editor.text && tuning_input(&a, &ev))
        continue;
      if (a.panel.doc) {
        if (ev.type == SDL_MOUSEBUTTONDOWN) {
          int x, y;
          SDL_GetMouseState(&x, &y);
          if (y >= 51 && y < 51 + PANEL_ROWS * 25) {
            a.panel.row = (y - 51) / 25;
            panel_change(&a.panel, x < 320 ? -1 : 1);
          }
        }
        if (ev.type != SDL_KEYDOWN)
          continue;
        SDL_Keycode key = ev.key.keysym.sym;
        if (key == SDLK_ESCAPE) {
          panel_close(&a.panel);
          editor_close(&a.editor);
          continue;
        }
        if (key == SDLK_UP || key == SDLK_DOWN)
          a.panel.row =
              (a.panel.row + (key == SDLK_DOWN ? 1 : -1) + PANEL_ROWS) %
              PANEL_ROWS;
        if (key == SDLK_LEFT || key == SDLK_RIGHT)
          panel_change(&a.panel, key == SDLK_RIGHT ? 1 : -1);
        if (key != SDLK_s || !(ev.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) ||
            !panel_copy(&a.panel, &a.editor))
          continue;
      }
      if (a.editor.text) {
        if (ev.type == SDL_TEXTINPUT)
          editor_input(&a.editor, ev.text.text);
        if (ev.type == SDL_KEYDOWN) {
          SDL_Keycode key = ev.key.keysym.sym;
          if (key == SDLK_ESCAPE)
            editor_close(&a.editor);
          else if (key == SDLK_s &&
                   ev.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) {
            BluePatch *p = malloc(sizeof *p);
            if (p && editor_save(&a.editor, &a.catalog[a.selected], p,
                                 a.message, sizeof a.message)) {
              free(a.pending);
              a.pending = p;
              a.pending_index = a.selected;
              apply_pending(&a);
            } else
              free(p);
          } else
            editor_key(&a.editor, key, ev.key.keysym.mod);
        }
        continue;
      }
      if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        SDL_Keycode k = symbol;
        if (key && !(ev.key.keysym.mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT)))
          keyboard_note(&a, (int)(key - blue_keys), 1, 1);
        else {
          if (k == SDLK_ESCAPE)
            quit = 1;
          else if (k == SDLK_LEFT || k == SDLK_RIGHT)
            load(&a,
                 (a.selected + (k == SDLK_RIGHT ? 1 : a.count - 1)) % a.count);
          else if (k == SDLK_F8) {
            panic(&a);
            if (editor_open(&a.editor, &a.catalog[a.selected], 0) &&
                !panel_open(&a.panel, a.editor.text))
              editor_close(&a.editor);
          } else if (k == SDLK_F4 || k == SDLK_F6) {
            panic(&a);
            if (!editor_open(&a.editor, &a.catalog[a.selected], k == SDLK_F6))
              snprintf(a.message, sizeof a.message, "Could not open source");
          } else if (k == SDLK_F2)
            a.macro_page = (a.macro_page + 1) % 4;
          else if (k == SDLK_LEFTBRACKET || k == SDLK_RIGHTBRACKET) {
            int index = 2 * a.macro_page + 1;
            float value =
                a.macros[index] + (k == SDLK_RIGHTBRACKET ? .125f : -.125f);
            a.macros[index] = SDL_clamp(value, 0.f, 1.f);
            event(&a, MACRO, index, a.macros[index]);
          } else if (k == SDLK_F3) {
            int n = SDL_GetNumAudioDevices(0);
            if (n > 0) {
              a.output_index = (a.output_index + 1) % n;
              open_output(&a, SDL_GetAudioDeviceName(a.output_index, 0));
            }
          } else if (k == SDLK_F7) {
            int ok =
                blue_export_pack(&a.catalog[a.selected], a.macros, a.export_to);
            snprintf(a.message, sizeof a.message, "%s",
                     ok ? "Pack exported"
                        : "Export failed. Choose a new ZIP path.");
          } else if (k == SDLK_F11)
            load_controls(&a, a.selected, a.macros);
          else if (k == SDLK_F10)
            capture(&a);
          else if (k == SDLK_F5 || k == SDLK_F9)
            state(&a, k == SDLK_F5);
        }
      }
      if (ev.type == SDL_MOUSEBUTTONDOWN ||
          (ev.type == SDL_MOUSEMOTION && ev.motion.state & SDL_BUTTON_LMASK)) {
        int x, y;
        SDL_GetMouseState(&x, &y);
        if (x >= 270 && x <= 880 && y >= 185 && y < 255) {
          int k = (y >= 232) + 2 * a.macro_page;
          a.macros[k] = (x - 270) / 610.f;
          event(&a, MACRO, k, a.macros[k]);
        } else if (ev.type == SDL_MOUSEBUTTONDOWN && y >= KEYBOARD_Y &&
                   y < KEYBOARD_Y + 4 * KEY_HEIGHT && x >= KEYBOARD_X) {
          int row = (y - KEYBOARD_Y) / KEY_HEIGHT;
          int col = (x - KEYBOARD_X) / KEY_WIDTH;
          int count = row < 2 ? 10 : (row == 2 ? 9 : 7);
          if (col < count) {
            if (mouse >= 0)
              keyboard_note(&a, mouse, 2, 0);
            mouse = key_starts[row] + col;
            keyboard_note(&a, mouse, 2, 1);
          }
        }
      }
    }
    draw(&a, r, font);
    SDL_Delay(16);
  }
  if (a.tuning_field)
    close_builder(&a);
  panel_close(&a.panel);
  editor_close(&a.editor);
  blue_midi_close();
  if (a.capturing)
    capture(&a);
  SDL_CloseAudioDevice(a.device);
  blue_destroy(a.engine);
  free(a.pending);
  free(a.recording);
  SDL_DestroyTexture(font);
  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
