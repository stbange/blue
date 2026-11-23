#include "control.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned u16(const unsigned char *x) { return x[0] + 256u * x[1]; }
static unsigned u32(const unsigned char *x) {
  return u16(x) + (u16(x + 2) << 16);
}
static void put(FILE *f, unsigned v, int bytes) {
  while (bytes--) {
    fputc(v & 255, f);
    v >>= 8;
  }
}
int blue_write_wav(const char *path, const float *s, size_t frames, int rate) {
  if (frames > (UINT32_MAX - 36) / 4)
    return 0;
  FILE *f = fopen(path, "wb");
  if (!f)
    return 0;
  fwrite("RIFF", 1, 4, f);
  put(f, 36 + (unsigned)frames * 4, 4);
  fwrite("WAVEfmt ", 1, 8, f);
  put(f, 16, 4);
  put(f, 1, 2);
  put(f, 2, 2);
  put(f, rate, 4);
  put(f, rate * 4, 4);
  put(f, 4, 2);
  put(f, 16, 2);
  fwrite("data", 1, 4, f);
  put(f, (unsigned)frames * 4, 4);
  for (size_t i = 0; i < frames * 2; i++)
    put(f, (unsigned)(int)lrintf(fminf(1, fmaxf(-1, s[i])) * 32767), 2);
  int ok = !ferror(f);
  return fclose(f) == 0 && ok;
}
int blue_read_sample(const char *path, BluePatch *p) {
  unsigned char h[16];
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  int ok = 0, channels = 0, rate = 0;
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) ||
      memcmp(h + 8, "WAVE", 4))
    goto end;
  while (fread(h, 1, 8, f) == 8) {
    unsigned len = u32(h + 4);
    if (len > 16 * 1024 * 1024)
      break;
    if (!memcmp(h, "fmt ", 4)) {
      if (len < 16 || fread(h, 1, 16, f) != 16 || u16(h) != 1 ||
          u16(h + 14) != 16)
        break;
      channels = (int)u16(h + 2);
      rate = (int)u32(h + 4);
      if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000)
        break;
      len -= 16;
    } else if (!memcmp(h, "data", 4)) {
      if (!channels || !len || len % (2 * channels) ||
          len / (2 * channels) > BLUE_SAMPLE)
        break;
      int frames = (int)len / (2 * channels);
      for (int i = 0; i < frames; i++) {
        if (fread(h, 2, channels, f) != (size_t)channels)
          goto end;
        int sum = (int16_t)u16(h);
        if (channels == 2)
          sum += (int16_t)u16(h + 2);
        p->sample[i] = sum / (32768.f * channels);
      }
      p->sample_count = frames;
      p->sample_rate = rate;
      ok = 1;
      break;
    }
    if (fseek(f, (long)(len + (len & 1)), SEEK_CUR))
      break;
  }
end:
  fclose(f);
  return ok;
}
