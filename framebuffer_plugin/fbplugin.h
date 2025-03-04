#ifndef _FBPLUGIN_H
#define _FBPLUGIN_H

#ifdef __cplusplus
#include <cstdlib>
extern "C" {
#else
#include <stdlib.h>
#endif

/* Allocate memory in the MMIO plugin memory space. */
void* plugin_malloc(size_t size);
void plugin_free(void* ptr);

/* Signal to spike that it should terminate, so that the plugin can be deallocated. */
void plugin_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* _FBPLUGIN_H */
