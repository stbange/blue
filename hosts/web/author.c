#include "control.h"
static BluePatch patch;
static char error[2048];
const BluePatch *author_load(const char *root, const char *id) {
  return blue_load(&patch, root, id, error, sizeof error) ? &patch : NULL;
}
const char *author_error(void) { return error; }
int author_size(void) { return sizeof patch; }

const char *author_label(int i) {
  return i >= 0 && i < 8 ? patch.labels[i] : "";
}
