/* Minimal SceIo surface used only to type-check save.c on the host.
   Signatures mirror the VitaSDK; this file is NOT shipped to the Vita. */
#ifndef STUB_PSP2_IO_FCNTL_H
#define STUB_PSP2_IO_FCNTL_H
typedef int SceUID;
typedef unsigned int SceMode;
typedef unsigned int SceSize;
#define SCE_O_RDONLY 0x0001
#define SCE_O_WRONLY 0x0002
#define SCE_O_CREAT  0x0200
#define SCE_O_TRUNC  0x0400
SceUID sceIoOpen(const char *file, int flags, SceMode mode);
int    sceIoClose(SceUID fd);
int    sceIoRead(SceUID fd, void *data, SceSize size);
int    sceIoWrite(SceUID fd, const void *data, SceSize size);
int    sceIoRemove(const char *file);
#endif
