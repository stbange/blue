#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
void editor_close(BlueEditor *e) {
  free(e->text);
  memset(e, 0, sizeof *e);
  SDL_StopTextInput();
}
int editor_open(BlueEditor *e, const BluePreset *preset, int script) {
  char path[2048];
  if (!blue_source_path(preset->folder, preset->name, script, path,
                        sizeof path))
    return 0;
  size_t n = 0;
  char *data = SDL_LoadFile(path, &n);
  if (!data)
    return 0;
  if (n > 1048576) {
    SDL_free(data);
    return 0;
  }
  char *buf = malloc(n + 1024);
  if (!buf) {
    SDL_free(data);
    return 0;
  }
  memcpy(buf, data, n);
  buf[n] = 0;
  SDL_free(data);
  free(e->text);
  *e = (BlueEditor){
      .text = buf, .length = n, .capacity = n + 1024, .script = script};
  snprintf(e->path, sizeof e->path, "%s", path);
  SDL_StartTextInput();
  return 1;
}
void editor_input(BlueEditor *e, const char *s) {
  size_t n = strlen(s), need = e->length + n + 1;
  if (need > 1048577)
    return;
  if (need > e->capacity) {
    char *p = realloc(e->text, need + 1024);
    if (!p)
      return;
    e->text = p;
    e->capacity = need + 1024;
  }
  memmove(e->text + e->cursor + n, e->text + e->cursor,
          e->length - e->cursor + 1);
  memcpy(e->text + e->cursor, s, n);
  e->cursor += n;
  e->length += n;
}
void editor_key(BlueEditor *e, SDL_Keycode key, SDL_Keymod mod) {
  if (mod & (KMOD_CTRL | KMOD_GUI)) {
    if (key == SDLK_c)
      SDL_SetClipboardText(e->text);
    if (key == SDLK_v) {
      char *p = SDL_GetClipboardText();
      if (p) {
        editor_input(e, p);
        SDL_free(p);
      }
    }
    return;
  }
  size_t c = e->cursor;
  if (key == SDLK_HOME) {
    while (c && e->text[c - 1] != '\n')
      c--;
  } else if (key == SDLK_END) {
    while (c < e->length && e->text[c] != '\n')
      c++;
  } else if (key == SDLK_LEFT && c)
    c--;
  else if (key == SDLK_RIGHT && c < e->length)
    c++;
  else if (key == SDLK_UP || key == SDLK_DOWN) {
    size_t start = c;
    while (start && e->text[start - 1] != '\n')
      start--;
    size_t column = c - start;
    if (key == SDLK_UP) {
      if (!start)
        return;
      c = start - 1;
      while (c && e->text[c - 1] != '\n')
        c--;
    } else {
      while (c < e->length && e->text[c] != '\n')
        c++;
      if (c < e->length)
        c++;
    }
    while (column-- && c < e->length && e->text[c] != '\n')
      c++;
  } else if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
    if (key == SDLK_BACKSPACE) {
      if (!c)
        return;
      c--;
    }
    if (c >= e->length)
      return;
    memmove(e->text + c, e->text + c + 1, e->length - c);
    e->length--;
  } else if (key == SDLK_RETURN) {
    editor_input(e, "\n");
    return;
  }
  e->cursor = c;
}
int editor_save(BlueEditor *e, const BluePreset *preset, BluePatch *p,
                char *err, size_t size) {
  if (!blue_compile(p, preset->folder, preset->name, e->script ? NULL : e->text,
                    e->script ? e->text : NULL, err, size))
    return 0;
  char temp[2200];
  snprintf(temp, sizeof temp, "%s.%llu.tmp", e->path,
           (unsigned long long)SDL_GetPerformanceCounter());
  FILE *f = fopen(temp, "wbx");
  if (f) {
    int ok = fwrite(e->text, 1, e->length, f) == e->length;
    if (fclose(f))
      ok = 0;
#ifdef _WIN32
    if (ok && MoveFileExA(temp, e->path,
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return 1;
#else
    if (ok && !rename(temp, e->path))
      return 1;
#endif
    remove(temp);
  }
  snprintf(err, size, "Could not save %s", e->path);
  return 0;
}
