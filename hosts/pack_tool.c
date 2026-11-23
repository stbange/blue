#include "archive.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
  int ok = 0;
  if (argc == 4 && !strcmp(argv[1], "import"))
    ok = blue_import_pack(argv[2], argv[3]);
  else if (argc == 5 && !strcmp(argv[1], "export")) {
    BluePreset preset = {0};
    if (strlen(argv[2]) < sizeof preset.folder &&
        strlen(argv[3]) < sizeof preset.name) {
      strcpy(preset.folder, argv[2]);
      strcpy(preset.name, argv[3]);
      ok = blue_export_pack(&preset, NULL, argv[4]);
    }
  } else {
    fprintf(stderr, "blue-pack export PACK_DIR PRESET NEW.zip\nblue-pack "
                    "import ARCHIVE.zip NEW_DIR\n");
    return 2;
  }
  if (!ok)
    fprintf(stderr, "Pack exchange failed. Check the source and choose a new "
                    "destination.\n");
  return !ok;
}
