#include <math.h>
#include <inttypes.h>
#include "../maths.h"

typedef struct {
    int width, height;
    vec4_t *buffer;
} texture_t;

vec4_t texture_repeat_sample(texture_t *texture, vec2_t texcoord) {
    float u = texcoord.x - floorf(texcoord.x);
    float v = texcoord.y - floorf(texcoord.y);
    int c = (int)((texture->width - 1) * u);
    int r = (int)((texture->height - 1) * v);
    int index = r * texture->width + c;
    return texture->buffer[index];
}

vec4_t texture_sample(texture_t *texture, vec2_t texcoord) {
    return texture_repeat_sample(texture, texcoord);
}

// End texture

typedef struct {
    vec3_t diffuse;
    vec3_t specular;
    float alpha;
    float shininess;
    vec3_t normal;
    vec3_t emission;
} material_t;

typedef struct {
    vec3_t position;
    vec2_t texcoord;
    vec3_t normal;
    vec4_t joint;
    vec4_t weight;
} blinn_attribs_t;

typedef struct {
    vec3_t world_position;
    vec3_t depth_position;
    vec2_t texcoord;
    vec3_t normal;
} blinn_varyings_t;

typedef struct {
    vec3_t light_dir;
    vec3_t camera_pos;
    mat4_t model_matrix;
    mat3_t normal_matrix;
    mat4_t light_vp_matrix;
    mat4_t camera_vp_matrix;
    mat4_t *joint_matrices;
    mat3_t *joint_n_matrices;
    float ambient_intensity;
    float punctual_intensity;
    texture_t *shadow_map;
    /* surface parameters */
    vec4_t basecolor;
    float shininess;
    texture_t *diffuse_map;
    texture_t *specular_map;
    texture_t *emission_map;
    /* render controls */
    float alpha_cutoff;
    int shadow_pass;
} blinn_uniforms_t;

static int is_zero_vector(vec3_t v) {
    return v.x == 0 && v.y == 0 && v.z == 0;
}

static int is_in_shadow(blinn_varyings_t *varyings,
                        blinn_uniforms_t *uniforms,
                        float n_dot_l) {
    if (uniforms->shadow_map) {
        float u = (varyings->depth_position.x + 1) * 0.5f;
        float v = (varyings->depth_position.y + 1) * 0.5f;
        float d = (varyings->depth_position.z + 1) * 0.5f;

        float depth_bias = float_max(0.05f * (1 - n_dot_l), 0.005f);
        float current_depth = d - depth_bias;
        vec2_t texcoord = vec2_new(u, v);
        float closest_depth = texture_sample(uniforms->shadow_map, texcoord).x;

        return current_depth > closest_depth;
    } else {
        return 0;
    }
}

static vec3_t get_view_dir(blinn_varyings_t *varyings,
                           blinn_uniforms_t *uniforms) {
    vec3_t camera_pos = uniforms->camera_pos;
    vec3_t world_pos = varyings->world_position;
    return vec3_normalize(vec3_sub(camera_pos, world_pos));
}

static vec3_t get_specular(vec3_t light_dir, vec3_t view_dir,
                           material_t material) {
    if (!is_zero_vector(material.specular)) {
        vec3_t half_dir = vec3_normalize(vec3_add(light_dir, view_dir));
        float n_dot_h = vec3_dot(material.normal, half_dir);
        if (n_dot_h > 0) {
            float strength = powf(n_dot_h, material.shininess);
            return vec3_mul(material.specular, strength);
        }
    }
    return vec3_new(0, 0, 0);
}

static material_t get_material(blinn_varyings_t *varyings,
                               blinn_uniforms_t *uniforms,
                               int backface) {
    vec2_t texcoord = varyings->texcoord;
    vec3_t diffuse, specular, normal, emission;
    float alpha, shininess;
    material_t material;

    diffuse = vec3_from_vec4(uniforms->basecolor);
    alpha = uniforms->basecolor.w;
    if (uniforms->diffuse_map) {
        vec4_t sample = texture_sample(uniforms->diffuse_map, texcoord);
        diffuse = vec3_modulate(diffuse, vec3_from_vec4(sample));
        alpha *= sample.w;
    }

    specular = vec3_new(0, 0, 0);
    if (uniforms->specular_map) {
        vec4_t sample = texture_sample(uniforms->specular_map, texcoord);
        specular = vec3_from_vec4(sample);
    }
    shininess = uniforms->shininess;

    normal = vec3_normalize(varyings->normal);
    if (backface) {
        normal = vec3_negate(normal);
    }

    emission = vec3_new(0, 0, 0);
    if (uniforms->emission_map) {
        vec4_t sample = texture_sample(uniforms->emission_map, texcoord);
        emission = vec3_from_vec4(sample);
    }

    material.diffuse = diffuse;
    material.specular = specular;
    material.alpha = alpha;
    material.shininess = shininess;
    material.normal = normal;
    material.emission = emission;
    return material;
}

static vec4_t shadow_fragment_shader(blinn_varyings_t *varyings,
                                     blinn_uniforms_t *uniforms,
                                     int *discard) {
    if (uniforms->alpha_cutoff > 0) {
        float alpha = uniforms->basecolor.w;
        if (uniforms->diffuse_map) {
            vec2_t texcoord = varyings->texcoord;
            alpha *= texture_sample(uniforms->diffuse_map, texcoord).w;
        }
        if (alpha < uniforms->alpha_cutoff) {
            *discard = 1;
        }
    }
    return vec4_new(0, 0, 0, 0);
}

static vec4_t common_fragment_shader(blinn_varyings_t *varyings,
                                     blinn_uniforms_t *uniforms,
                                     int *discard,
                                     int backface) {
    material_t material = get_material(varyings, uniforms, backface);
    if (uniforms->alpha_cutoff > 0 && material.alpha < uniforms->alpha_cutoff) {
        *discard = 1;
        return vec4_new(0, 0, 0, 0);
    } else {
        vec3_t color = material.emission;

        if (uniforms->ambient_intensity > 0) {
            vec3_t ambient = material.diffuse;
            float intensity = uniforms->ambient_intensity;
            color = vec3_add(color, vec3_mul(ambient, intensity));
        }

        if (uniforms->punctual_intensity > 0) {
            vec3_t light_dir = vec3_negate(uniforms->light_dir);
            float n_dot_l = vec3_dot(material.normal, light_dir);
            if (n_dot_l > 0 && !is_in_shadow(varyings, uniforms, n_dot_l)) {
                vec3_t view_dir = get_view_dir(varyings, uniforms);
                vec3_t specular = get_specular(light_dir, view_dir, material);
                vec3_t diffuse = vec3_mul(material.diffuse, n_dot_l);
                vec3_t punctual = vec3_add(diffuse, specular);
                float intensity = uniforms->punctual_intensity;
                color = vec3_add(color, vec3_mul(punctual, intensity));
            }
        }

        return vec4_from_vec3(color, material.alpha);
    }
}

vec4_t fragA(void *varyings_, void *uniforms_,
             int *discard, int backface) {
    blinn_varyings_t *varyings = (blinn_varyings_t*)varyings_;
    blinn_uniforms_t *uniforms = (blinn_uniforms_t*)uniforms_;

    if (uniforms->shadow_pass) {
        return shadow_fragment_shader(varyings, uniforms, discard);
    } else {
        return common_fragment_shader(varyings, uniforms, discard, backface);
    }
}
