#ifndef _SHADERS_H
#define _SHADERS_H

#ifdef __cplusplus
#include <cstdlib>
#include <cstdint>
#include <cstddef>
extern "C" {
#else
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#endif
#include "../renderer/renderer/core/graphics.h"

/* Run fragment shader on spike. */
vec4_t plugin_fragment_shader(program_t *program, int *discard, int backface);

/* Run vertex shader on spike. */
vec4_t plugin_vertex_shader(program_t *program, int i);

/* Update the framebuffer address in spike. */
void plugin_update_fbaddr(unsigned char* addr);
/* Draw the given pixel in the specified offset from the framebuffer. */
void plugin_draw(uint32_t pixel, ptrdiff_t offset);

/* Set the fragment shader.
 * file_name : File name of shader.
 * sdr_type : Fragment shader if 0, else vertex shader.
 * Returns the entry point to the shader in shared memory or NULL if
 * loading failed. */
void* plugin_set_shader(const char* file_name, char sdr_type);

#ifdef __cplusplus
}
#endif

#endif /* _SHADERS_H */
