#ifndef _DLDITOOL_H
#define _DLDITOOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int DLDI_tryPatch(void* data, size_t size, unsigned int device);

#ifdef __cplusplus
}
#endif

#endif
