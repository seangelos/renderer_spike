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

static mat4_t get_model_matrix(blinn_attribs_t *attribs,
                               blinn_uniforms_t *uniforms) {
    if (uniforms->joint_matrices) {
        mat4_t joint_matrices[4];
        mat4_t skin_matrix;

        joint_matrices[0] = uniforms->joint_matrices[(int)attribs->joint.x];
        joint_matrices[1] = uniforms->joint_matrices[(int)attribs->joint.y];
        joint_matrices[2] = uniforms->joint_matrices[(int)attribs->joint.z];
        joint_matrices[3] = uniforms->joint_matrices[(int)attribs->joint.w];

        skin_matrix = mat4_combine(joint_matrices, attribs->weight);
        return mat4_mul_mat4(uniforms->model_matrix, skin_matrix);
    } else {
        return uniforms->model_matrix;
    }
}

static mat3_t get_normal_matrix(blinn_attribs_t *attribs,
                                blinn_uniforms_t *uniforms) {
    if (uniforms->joint_n_matrices) {
        mat3_t joint_n_matrices[4];
        mat3_t skin_n_matrix;

        joint_n_matrices[0] = uniforms->joint_n_matrices[(int)attribs->joint.x];
        joint_n_matrices[1] = uniforms->joint_n_matrices[(int)attribs->joint.y];
        joint_n_matrices[2] = uniforms->joint_n_matrices[(int)attribs->joint.z];
        joint_n_matrices[3] = uniforms->joint_n_matrices[(int)attribs->joint.w];

        skin_n_matrix = mat3_combine(joint_n_matrices, attribs->weight);
        return mat3_mul_mat3(uniforms->normal_matrix, skin_n_matrix);
    } else {
        return uniforms->normal_matrix;
    }
}

static vec4_t shadow_vertex_shader(blinn_attribs_t *attribs,
                                   blinn_varyings_t *varyings,
                                   blinn_uniforms_t *uniforms) {
    mat4_t model_matrix = get_model_matrix(attribs, uniforms);
    mat4_t light_vp_matrix = uniforms->light_vp_matrix;

    vec4_t input_position = vec4_from_vec3(attribs->position, 1);
    vec4_t world_position = mat4_mul_vec4(model_matrix, input_position);
    vec4_t depth_position = mat4_mul_vec4(light_vp_matrix, world_position);

    varyings->texcoord = attribs->texcoord;
    return depth_position;
}

static vec4_t common_vertex_shader(blinn_attribs_t *attribs,
                                   blinn_varyings_t *varyings,
                                   blinn_uniforms_t *uniforms) {
    mat4_t model_matrix = get_model_matrix(attribs, uniforms);
    mat3_t normal_matrix = get_normal_matrix(attribs, uniforms);
    mat4_t camera_vp_matrix = uniforms->camera_vp_matrix;
    mat4_t light_vp_matrix = uniforms->light_vp_matrix;

    vec4_t input_position = vec4_from_vec3(attribs->position, 1);
    vec4_t world_position = mat4_mul_vec4(model_matrix, input_position);
    vec4_t clip_position = mat4_mul_vec4(camera_vp_matrix, world_position);
    vec4_t depth_position = mat4_mul_vec4(light_vp_matrix, world_position);

    vec3_t input_normal = attribs->normal;
    vec3_t world_normal = mat3_mul_vec3(normal_matrix, input_normal);

    varyings->world_position = vec3_from_vec4(world_position);
    varyings->depth_position = vec3_from_vec4(depth_position);
    varyings->texcoord = attribs->texcoord;
    varyings->normal = vec3_normalize(world_normal);
    return clip_position;
}

vec4_t vertA(void *attribs_, void *varyings_, void *uniforms_) {
    blinn_attribs_t *attribs = (blinn_attribs_t*)attribs_;
    blinn_varyings_t *varyings = (blinn_varyings_t*)varyings_;
    blinn_uniforms_t *uniforms = (blinn_uniforms_t*)uniforms_;

    if (uniforms->shadow_pass) {
        return shadow_vertex_shader(attribs, varyings, uniforms);
    } else {
        return common_vertex_shader(attribs, varyings, uniforms);
    }
}
