#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include "maths.h"

typedef struct {
    int width, height;
    unsigned char *color_buffer;
    float *depth_buffer;
} framebuffer_t;

typedef struct program program_t;
typedef vec4_t vertex_shader_t(void *attribs, void *varyings, void *uniforms);
typedef vec4_t fragment_shader_t(void *varyings, void *uniforms,
                                 int *discard, int backface);

#define MAX_VARYINGS 10

struct program {
    vertex_shader_t *vertex_shader;
    fragment_shader_t *fragment_shader;
    int sizeof_attribs;
    int sizeof_varyings;
    int sizeof_uniforms;
    int double_sided;
    int enable_blend;
    /* for shaders */
    void *shader_attribs[3];
    void *shader_varyings;
    void *shader_uniforms;
    /* for clipping */
    vec4_t in_coords[MAX_VARYINGS];
    vec4_t out_coords[MAX_VARYINGS];
    void *in_varyings[MAX_VARYINGS];
    void *out_varyings[MAX_VARYINGS];
};

#endif /* _GRAPHICS_H */
