/* Portable single-instrument ZIP exchange. Import only creates a fresh tree. */
#include "archive.h"
#include "cJSON.h"
#include "control.h"
#include "miniz.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
#else
#include <unistd.h>
#define make_dir(p) mkdir(p, 0700)
#define remove_dir(p) rmdir(p)
#endif

enum {
  MAX_FILE = 2 * 1024 * 1024,
  MAX_TOTAL = 32 * 1024 * 1024,
  MAX_ENTRIES = 512
};
static int safe_name(const char *name) {
  if (!name || !*name || strlen(name) >= 480)
    return 0;
  for (const char *p = name; *p;) {
    const char *start = p;
    char stem[8] = {0};
    size_t letters = 0;
    while (*p && *p != '/') {
      unsigned char c = (unsigned char)*p++;
      if (!(isalnum(c) || c == '_' || c == '-' || c == '.'))
        return 0;
    }
    if (p == start || p[-1] == '.')
      return 0;
    while (start + letters < p && start[letters] != '.' &&
           letters < sizeof stem - 1) {
      stem[letters] = (char)toupper((unsigned char)start[letters]);
      letters++;
    }
    if (!strcmp(stem, "CON") || !strcmp(stem, "PRN") || !strcmp(stem, "NUL") ||
        !strcmp(stem, "AUX") ||
        ((!strncmp(stem, "COM", 3) || !strncmp(stem, "LPT", 3)) &&
         strlen(stem) == 4 && isdigit((unsigned char)stem[3])))
      return 0;
    if (*p && !*++p)
      return 0;
  }
  return 1;
}

static int join(char *path, size_t size, const char *dir, const char *rel) {
  int n = snprintf(path, size, "%s/%s", dir, rel);
  return n >= 0 && (size_t)n < size;
}

static char *read_file(const char *folder, const char *rel, size_t *size) {
  char path[2048];
  if (!safe_name(rel) || !join(path, sizeof path, folder, rel))
    return NULL;
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  char *bytes = NULL;
  if (!fseek(f, 0, SEEK_END)) {
    long n = ftell(f);
    if (n >= 0 && n <= MAX_FILE && !fseek(f, 0, SEEK_SET)) {
      bytes = malloc((size_t)n + 1);
      if (bytes && fread(bytes, 1, (size_t)n, f) == (size_t)n) {
        bytes[n] = 0;
        *size = (size_t)n;
      } else {
        free(bytes);
        bytes = NULL;
      }
    }
  }
  fclose(f);
  return bytes;
}

static int add(mz_zip_archive *zip, const char *name, const void *data,
               size_t size) {
  return mz_zip_writer_add_mem(zip, name, data, size, MZ_DEFAULT_COMPRESSION) !=
         0;
}
static int add_resource(mz_zip_archive *zip, const char *folder,
                        const char *relative) {
  size_t size = 0;
  char name[512];
  char *bytes = read_file(folder, relative, &size);
  int ok = bytes && join(name, sizeof name, "packs/instrument", relative) &&
           add(zip, name, bytes, size);
  free(bytes);
  return ok;
}
static const char *value(cJSON *obj, const char *key) {
  return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
}

int blue_export_pack(const BluePreset *preset, const float *macros,
                     const char *destination) {
  size_t n = 0;
  char *meta = read_file(preset->folder, "pack.json", &n);
  cJSON *manifest = cJSON_Parse(meta);
  free(meta);
  cJSON *list = cJSON_GetObjectItemCaseSensitive(manifest, "presets");
  const char *rel = value(list, preset->name);
  char docname[512];
  char *raw = read_file(preset->folder, rel, &n);
  cJSON *doc = cJSON_Parse(raw);
  free(raw);
  int ok = 0;
  if (!doc || !join(docname, sizeof docname, "packs/instrument", rel))
    goto finish;
  cJSON *current = cJSON_DetachItemFromObjectCaseSensitive(list, preset->name);
  cJSON *one = cJSON_CreateObject();
  cJSON_AddItemToObject(one, preset->name, current);
  cJSON_ReplaceItemInObjectCaseSensitive(manifest, "presets", one);
  if (macros) {
    cJSON_DeleteItemFromObjectCaseSensitive(doc, "macros");
    cJSON_AddItemToObject(doc, "macros", cJSON_CreateFloatArray(macros, 8));
  }
  char *metadata = cJSON_PrintUnformatted(manifest),
       *patch = cJSON_PrintUnformatted(doc);
  mz_zip_archive zip = {0};
  FILE *out = fopen(destination, "wbx");
  if (out) {
    if (mz_zip_writer_init_cfile(&zip, out, 0)) {
      const char index[] = "{\"packs\":[\"packs/instrument\"]}";
      ok =
          metadata && patch &&
          add(&zip, "index.json", index, sizeof index - 1) &&
          add(&zip, "packs/instrument/pack.json", metadata, strlen(metadata)) &&
          add(&zip, docname, patch, strlen(patch)) &&
          add_resource(&zip, preset->folder, value(manifest, "script"));
      cJSON *entry;
      cJSON_ArrayForEach(
          entry, cJSON_GetObjectItemCaseSensitive(manifest, "assets")) if (ok)
          ok = add_resource(&zip, preset->folder, cJSON_GetStringValue(entry));
      ok = ok && mz_zip_writer_finalize_archive(&zip);
      mz_zip_writer_end(&zip);
    }
    if (fclose(out))
      ok = 0;
    if (!ok)
      remove(destination);
  }
  cJSON_free(metadata);
  cJSON_free(patch);
finish:
  cJSON_Delete(doc);
  cJSON_Delete(manifest);
  return ok;
}

static void parents(char *path, size_t base, int create) {
  size_t n = strlen(path);
  for (size_t i = base + 1; i < n; i++) {
    size_t j = create ? i : n + base - i;
    if (path[j] == '/') {
      path[j] = 0;
      if (create)
        make_dir(path);
      else
        remove_dir(path);
      path[j] = '/';
    }
  }
}
int blue_import_pack(const char *archive, const char *destination) {
  mz_zip_archive zip = {0};
  mz_zip_archive_file_stat entry;
  if (!mz_zip_reader_init_file(&zip, archive, 0))
    return 0;
  int ok = 0;
  unsigned count = mz_zip_reader_get_num_files(&zip);
  size_t total = 0, base = strlen(destination);
  char path[2048];
  if (!count || count > MAX_ENTRIES || base + 482 >= sizeof path)
    goto close;
  for (unsigned i = 0; i < count; i++) {
    if (!mz_zip_reader_file_stat(&zip, i, &entry) || entry.m_is_encrypted ||
        entry.m_uncomp_size > MAX_FILE || !safe_name(entry.m_filename) ||
        mz_zip_reader_get_filename(&zip, i, NULL, 0) !=
            strlen(entry.m_filename) + 1)
      goto close;
    total += (size_t)entry.m_uncomp_size;
    if (total > MAX_TOTAL)
      goto close;
  }
  if (make_dir(destination))
    goto close;
  unsigned created = 0;
  for (; created < count; created++) {
    mz_zip_reader_file_stat(&zip, created, &entry);
    join(path, sizeof path, destination, entry.m_filename);
    parents(path, base, 1);
    FILE *out = fopen(path, "wbx");
    if (!out)
      break;
    int good = mz_zip_reader_extract_to_cfile(&zip, created, out, 0);
    if (fclose(out) || !good) {
      remove(path);
      break;
    }
  }
  if (created == count) {
    BluePreset candidate;
    BluePatch *p = malloc(sizeof *p);
    char error[1024];
    ok = p && blue_catalog(destination, &candidate, 1) == 1 &&
         blue_load(p, candidate.folder, candidate.name, error, sizeof error);
    free(p);
  }
  if (!ok) {
    for (unsigned i = 0; i < created; i++) {
      mz_zip_reader_file_stat(&zip, i, &entry);
      join(path, sizeof path, destination, entry.m_filename);
      remove(path);
    }
    for (unsigned i = 0; i < count; i++) {
      mz_zip_reader_file_stat(&zip, i, &entry);
      join(path, sizeof path, destination, entry.m_filename);
      parents(path, base, 0);
    }
    remove_dir(destination);
  }
close:
  mz_zip_reader_end(&zip);
  return ok;
}
