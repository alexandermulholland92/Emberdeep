/* Minimal SceIo stat surface, host type-check only.
   Mirrors <psp2/io/stat.h> in the VitaSDK, which is where sceIoMkdir
   lives - declaring it anywhere else lets `make check` pass while the
   real build fails. */
#ifndef STUB_PSP2_IO_STAT_H
#define STUB_PSP2_IO_STAT_H
#include <psp2/io/fcntl.h>
int sceIoMkdir(const char *dir, SceMode mode);
int sceIoRmdir(const char *dir);
#endif
