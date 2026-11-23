#define SDL_MAIN_HANDLED
#include "archive.h"
#include "blue_queue.h"
#include "control.h"
#include "editor.h"
#include "midi.h"
#include "panel.h"
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

static const char keys[] = "awsedftgyhujk";
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
  for (int k = 0; k < 13; k++) {
    int dark = strchr("wetyu", keys[k]) != NULL;
    rect(r, 28 + 66 * k, 295, 62, 78, dark ? 53 : 160, dark ? 85 : 190,
         dark ? 94 : 192);
    char s[2] = {keys[k], 0};
    label(r, font, 48 + 66 * k, 324, s, 2);
  }
  label(r, font, 28, 390,
        "Z Reload   R Record   F2 Macro pair   F3 Audio output   F5 Save   F9 "
        "Recall",
        1);
  label(r, font, 28, 425, a->capturing ? "Recording master ..." : a->message,
        1);
  label(r, font, 28, 452,
        "F4  Edit patch JSON       F6  Edit Lua mapping       F7  Export pack  "
        "  F8  Sound editor",
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
  load(a, a->selected);
  SDL_PauseAudioDevice(device, 0);
  return 1;
}
int main(int argc, char **argv) {
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO))
    return 1;
  char *base = SDL_GetBasePath();
  char root[2048], atlas[2048];
  snprintf(root, sizeof root, "%scontent", base ? base : "");
  snprintf(atlas, sizeof atlas, "%scontent/graphics/font.bmp",
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
          ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        event(&a, PANIC, 0, 0);
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
      if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) && !ev.key.repeat) {
        int down = ev.type == SDL_KEYDOWN;
        SDL_Keycode k = ev.key.keysym.sym;
        const char *p = k > 0 && k < 128 ? strchr(keys, (int)k) : NULL;
        if (p)
          event(&a, down ? NOTE_ON : NOTE_OFF, 60 + (int)(p - keys), .85f);
        else if (down) {
          if (k == SDLK_ESCAPE)
            quit = 1;
          else if (k == SDLK_LEFT || k == SDLK_RIGHT)
            load(&a,
                 (a.selected + (k == SDLK_RIGHT ? 1 : a.count - 1)) % a.count);
          else if (k == SDLK_F8) {
            event(&a, PANIC, 0, 0);
            if (editor_open(&a.editor, &a.catalog[a.selected], 0) &&
                !panel_open(&a.panel, a.editor.text))
              editor_close(&a.editor);
          } else if (k == SDLK_F4 || k == SDLK_F6) {
            event(&a, PANIC, 0, 0);
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
          } else if (k == SDLK_z)
            load_controls(&a, a.selected, a.macros);
          else if (k == SDLK_r)
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
        } else if (ev.type == SDL_MOUSEBUTTONDOWN && y >= 295 && y < 373 &&
                   x >= 28 && x < 886) {
          mouse = 60 + (x - 28) / 66;
          event(&a, NOTE_ON, mouse, .85f);
        }
      }
      if (ev.type == SDL_MOUSEBUTTONUP && mouse >= 0) {
        event(&a, NOTE_OFF, mouse, 0);
        mouse = -1;
      }
    }
    draw(&a, r, font);
    SDL_Delay(16);
  }
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
