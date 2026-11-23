#ifndef BLUE_PANEL_H
#define BLUE_PANEL_H
#include "cJSON.h"
#include "editor.h"
enum { PANEL_ROWS = 14 };
typedef struct {
  cJSON *doc;
  int row, lane, frame, partial, point;
} BluePanel;
int panel_open(BluePanel *p, const char *text);
void panel_close(BluePanel *p);
void panel_value(const BluePanel *p, int row, char *out, size_t size);
void panel_change(BluePanel *p, int delta);
int panel_copy(const BluePanel *p, BlueEditor *ed);
extern const char *const panel_fields[PANEL_ROWS];
#endif
