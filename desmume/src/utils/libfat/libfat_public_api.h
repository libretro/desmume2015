#ifndef _LIBFAT_PUBLIC_API_H_
#define _LIBFAT_PUBLIC_API_H_

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

void LIBFAT_Init(void* buffer, int size_bytes);
void LIBFAT_Shutdown(void);
bool LIBFAT_MkDir(const char *path);
bool LIBFAT_WriteFile(const char *path, const void* data, int len);

#ifdef __cplusplus
}
#endif

#endif //_LIBFAT_PUBLIC_API_H_
