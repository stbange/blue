#ifndef BLUE_EDITOR_H
#define BLUE_EDITOR_H
#include "control.h"
#include <SDL.h>
typedef struct {
  char *text;
  size_t cursor, length, capacity;
  int script;
  char path[2048];
} BlueEditor;
int editor_open(BlueEditor *ed, const BluePreset *preset, int script);
void editor_close(BlueEditor *ed);
void editor_input(BlueEditor *ed, const char *text);
void editor_key(BlueEditor *ed, SDL_Keycode key, SDL_Keymod mod);
int editor_save(BlueEditor *ed, const BluePreset *preset, BluePatch *patch,
                char *error, size_t size);
#endif
