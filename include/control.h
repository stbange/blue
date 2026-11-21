#ifndef BLUE_CONTROL_H
#define BLUE_CONTROL_H
#include "blue.h"
int blue_load(BluePatch *out,const char *folder,const char *preset,char *error,size_t size);
int blue_write_wav(const char *path,const float *stereo,size_t frames,int rate);
int blue_read_sample(const char *path,BluePatch *patch);
#endif
