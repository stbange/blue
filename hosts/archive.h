#ifndef BLUE_ARCHIVE_H
#define BLUE_ARCHIVE_H
#include "control.h"
int blue_export_pack(const BluePreset *preset, const float *macros,
                     const char *destination);
int blue_import_pack(const char *archive, const char *destination);
#endif
