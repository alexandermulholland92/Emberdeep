/* Minimal SceIo directory surface, host type-check only. */
#ifndef STUB_PSP2_IO_DIRENT_H
#define STUB_PSP2_IO_DIRENT_H
#include <psp2/io/fcntl.h>
int sceIoMkdir(const char *dir, SceMode mode);
#endif
