#ifndef DIRENUM_H
#define DIRENUM_H

#include <stdint.h>
#include <dirent.h>

uint8_t openDir(char *srcPath);
void closeDir();
char *getSrcSuffix();
struct dirent *getNextEntry(uint8_t type);

#endif
