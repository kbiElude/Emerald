/**
 *
 * Particles test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "stage_particle.h"

/* Particle data layout:
 *
 * float (mass)     x particles 1
 * vec4  (position) x particles 5
 * vec4  (velocity) x particles 9
 * vec4  (force)    x particles 13
 * vec4  (color)    x particles 17
 */
#define N_PARTICLE_PROPERTY_FLOATS (17)
#define N_PARTICLES                (10000)
#define N_PARTICLES_ALIGNED_4      (N_PARTICLES + (4 - (N_PARTICLES % 4)))

/* These are directly associated with _particle_generate1_vertex_shader_body */
#define N_GENERATE1_OUT_MASS_TF_INDEX     (0)
#define N_GENERATE1_OUT_VELOCITY_TF_INDEX (1)
#define N_GENERATE1_OUT_FORCE_TF_INDEX    (2)
#define N_GENERATE1_OUT_COLOR_TF_INDEX    (3)

/* These are directly associated with _particle_generate2_vertex_shader_body */
#define N_GENERATE2_OUT_POSITION_TF_INDEX (0)

/* These are directly associated with _particle_update_vertex_shader_body */
#define N_UPDATE_OUT_POSITION_TF_INDEX (0)
#define N_UPDATE_OUT_VELOCITY_TF_INDEX (1)
#define N_UPDATE_OUT_FORCE_TF_INDEX    (2)

const char* _particle_draw_fragment_shader_body = "#version 330\n"
                                                  "\n"
                                                  "in  vec4 fp_color;\n"
                                                  "out vec4 result;\n"
                                                  "\n"
                                                  "void main()\n"
                                                  "{\n"
                                                  "    float alpha = 1.4 * clamp(0.4 - distance(vec2(0.5), gl_PointCoord.xy), 0, 1);\n"
                                                  "    result = vec4(fp_color.xy, alpha, alpha * fp_color.w);\n"
                                                  "}\n";

const char* _particle_draw_vertex_shader_body = "#version 330\n"
                                                "\n"
                                                "uniform samplerBuffer data;\n"
                                                "uniform int           n_total_particles;\n"
                                                "uniform mat4          projection_view;\n"
                                                "out     vec4          fp_color;\n"
                                                "\n"
                                                "void main()\n"
                                                "{\n"
                                                "    vec4 position = texelFetch(data, n_total_particles / 4 + gl_VertexID);\n"
                                                "    vec4 velocity = texelFetch(data, n_total_particles / 4 + n_total_particles + gl_VertexID);\n"
                                                "\n"
                                                "    fp_color      = vec4(texelFetch(data, n_total_particles / 4 + n_total_particles * 3 + gl_VertexID).xyz, length(velocity) );\n"
                                                "    gl_Position   = projection_view * position;\n"
                                                "    gl_PointSize  = 10;\n"
                                                "}\n";

/* Generation is split into two passes, because nvidia driver supports up to 4 maximum separate attrib streams */
const char* _particle_generate1_vertex_shader_body = "#version 330\n"
                                                     "\n"
                                                     "uniform float     max_mass_delta;\n"
                                                     "uniform float     min_mass;\n"
                                                     "uniform int       n_total_particles;\n"
                                                     "uniform float     spread;\n"
                                                     "\n"
                                                     "out float out_mass;\n"
                                                     "out vec4  out_velocity;\n"
                                                     "out vec4  out_force;\n"
                                                     "out vec4  out_color;\n"
                                                     "\n"
                                                     "void main()\n"
                                                     "{\n"
                                                     "   float rand_value     = fract(456.789 * sin(789.123 * gl_VertexID) );\n"
                                                     "   int   rand_value_int = int(rand_value * float(n_total_particles));\n"
                                                     "   float angle          = float(rand_value_int % 157) / 100.0f;\n"
                                                     "\n"
                                                     "   out_mass     = rand_value * max_mass_delta + min_mass;\n"
                                                     "   out_velocity = vec4(0);\n"
                                                     "   out_force.x  = cos(angle) * spread * rand_value;\n"
                                                     "   out_force.y  = rand_value * spread * rand_value;\n"
                                                     "   out_force.z  = sin(angle) * spread * rand_value;\n"
                                                     "   out_force.w  = 0;\n"
                                                     "\n"
                                                     "   if ((rand_value_int % 2) == 0) out_force.x = -out_force.x;\n"
                                                     "   if ((rand_value_int % 3) == 0) out_force.z = -out_force.z;\n"
                                                     "\n"
                                                     "   out_color.x = float(rand_value_int % 16)  / 16.0f;\n"
                                                     "   out_color.y = float(rand_value_int % 63)  / 63.0f;\n"
                                                     "   out_color.z = float(rand_value_int % 237) / 237.0f;\n"
                                                     "}\n";

const char* _particle_generate2_vertex_shader_body = "#version 330\n"
                                                     "\n"
                                                     "out vec4 out_position;\n"
                                                     "\n"
                                                     "void main()\n"
                                                     "{\n"
                                                     "   out_position = vec4(0, 10, 0, 1);\n"
                                                     "}\n";

const char* _particle_update_vertex_shader_body = "#version 330\n"
                                                  "\n"
                                                  "uniform samplerBuffer data;\n"
                                                  "uniform float         decay;\n"
                                                  "uniform float         dt;\n"
                                                  "uniform float         gravity;\n"
                                                  "uniform int           n_total_particles;\n"
                                                  "out     vec4          out_position;\n"
                                                  "out     vec4          out_velocity;\n"
                                                  "out     vec4          out_force;\n"
                                                  "\n"
                                                  "bool intersect_plane_ray(in vec3 plane_normal, in vec3 plane_pos, in vec3 ray_origin, in vec3 ray_direction, out float result)\n"
                                                  "{\n"
                                                  "    float plane_ray_dot = dot(ray_direction, plane_normal);\n"
                                                  "\n"
                                                  "    result = 0;\n"
                                                  "\n"
                                                  "    if (abs(plane_ray_dot) < 0.0000001) return false;\n"
                                                  "\n"
                                                  "   result = dot(plane_pos - ray_origin, plane_normal) / plane_ray_dot;\n"
                                                  "\n"
                                                  "   return (result >= 0);\n"
                                                  "}\n"
                                                  "\n"
                                                  "bool intersect_sphere_ray(in float sphere_radius, in vec3 sphere_pos, in vec3 ray_origin, in vec3 ray_direction, out float result)\n"
                                                  "{\n"
                                                  "    vec3  sphere_to_origin           = sphere_pos - ray_origin;\n"
                                                  "    float sphere_to_origin_cos_angle = dot(sphere_to_origin, ray_direction);\n"
                                                  "\n"
                                                  "    if (sphere_to_origin_cos_angle < 0) return false;\n"
                                                  "\n"
                                                  "    float sphere_to_origin_length = length(sphere_to_origin);\n"
                                                  "    float temp                    = sphere_radius * sphere_radius - sphere_to_origin_length + sphere_to_origin_cos_angle * sphere_to_origin_cos_angle;\n"
                                                  "\n"
                                                  "    if (temp < 0) return false;\n"
                                                  "\n"
                                                  "    result = sphere_to_origin_cos_angle - sqrt(temp);\n"
                                                  "    return true;\n"
                                                  "}\n"
                                                  "\n"
                                                  "void main()\n"
                                                  "{\n"
                                                  "   int        vertex_id_div_4 = gl_VertexID / 4;\n"
                                                  "   int        vertex_id_mod_4 = gl_VertexID % 4;\n"
                                                  "   float      mass;\n"
                                                  "   vec3  prev_position        = texelFetch(data, n_total_particles / 4 +                         gl_VertexID).xyz;\n"
                                                  "   vec3  prev_velocity        = texelFetch(data, n_total_particles / 4 + n_total_particles * 1 + gl_VertexID).xyz;\n"
                                                  "   vec3  prev_force           = texelFetch(data, n_total_particles / 4 + n_total_particles * 2 + gl_VertexID).xyz;\n"
                                                  "   vec3       color           = texelFetch(data, n_total_particles / 4 + n_total_particles * 3 + gl_VertexID).xyz;\n"
                                                  "\n"
                                                  "   if (vertex_id_mod_4 == 0) mass = texelFetch(data, vertex_id_div_4).x;else\n"
                                                  "   if (vertex_id_mod_4 == 1) mass = texelFetch(data, vertex_id_div_4).y;else\n"
                                                  "   if (vertex_id_mod_4 == 2) mass = texelFetch(data, vertex_id_div_4).z;else\n"
                                                  "                             mass = texelFetch(data, vertex_id_div_4).w;\n"
                                                  "\n"
                                                  /* Calculate new values */
                                                  "   vec3 new_velocity = decay * prev_velocity + (prev_force + vec3(0, -gravity, 0) / mass) * dt;\n"
                                                  "\n"
                                                  /* Fight infinite bouncing */
                                                  "   if (length(new_velocity) < 0.01) new_velocity = vec3(0);\n"
                                                  "\n"
                                                  "   vec3 new_position = prev_position + new_velocity;\n"
                                                  "   vec3 new_force    = vec3(0.0);\n" /* reset any initial force */
                                                  "\n"
                                                  /* Collision - example sphere */
                                                  "   vec3  particle_dir = normalize(new_velocity);\n"
                                                  "   float delta        = length(new_position - prev_position);\n"
                                                  "   float col_t        = 0;\n"
                                                  "\n"
                                                  "   if (intersect_sphere_ray(3.0, vec3(0), prev_position, particle_dir, col_t) && col_t <= delta)\n"
                                                  "   {\n"
                                                  "       vec3 col_position = prev_position + particle_dir * col_t;\n"
                                                  "       vec3 normal       = normalize(-col_position);\n"
                                                  "\n"
                                                  "       new_velocity = length(new_velocity) * reflect(normalize(new_velocity), normal);\n"
                                                  "       new_position = col_position + new_velocity * (delta - col_t);\n"
                                                  "   }\n"
                                                  /* Collision - example plane */
                                                  "   else\n"
                                                  "   if (intersect_plane_ray( vec3(0, 1, 0), vec3(0, -1, 0), prev_position, particle_dir, col_t) && col_t <= delta)\n"
                                                  "   {\n"
                                                  "       vec3 col_position = prev_position + particle_dir * col_t;\n"
                                                  "\n"
                                                  "       new_velocity = length(new_velocity) * reflect(normalize(new_velocity), vec3(0, 1, 0) );\n"
                                                  "       new_position = col_position + new_velocity * (delta - col_t);\n"
                                                  "   }\n"
                                                  "\n"
                                                  /* Store results */
                                                  "   out_position = vec4(new_position, 1);\n"
                                                  "   out_velocity = vec4(new_velocity, 0);\n"
                                                  "   out_force    = vec4(new_force,    0);\n"
                                                  "}\n";

GLuint           _particle_data_bo_id                                   = 0;
bool             _particle_data_initialized                             = false;
ogl_texture      _particle_data_tbo                                     = NULL; /* GL_RGBA32F */
float            _particle_decay                                        = 0.99995f;
float            _particle_dt                                           = 1.0f / 60.0f;
ogl_shader       _particle_draw_fshader                                 = NULL;
ogl_program      _particle_draw_program                                 = NULL;
GLint            _particle_draw_program_data_location                   = -1;
GLint            _particle_draw_program_n_total_particles_location      = -1;
GLint            _particle_draw_program_projection_view_location        = -1;
ogl_shader       _particle_draw_vshader                                 = NULL;
bool             _particle_flyby_initialized                            = false;
GLint            _particle_generate1_program_max_mass_delta_location    = -1;
GLint            _particle_generate1_program_min_mass_location          = -1;
GLint            _particle_generate1_program_n_total_particles_location = -1;
GLint            _particle_generate1_program_spread_location            = -1;
ogl_program      _particle_generate1_program                            = NULL;
GLuint           _particle_generate1_tfo_id                             = 0;
ogl_shader       _particle_generate1_vshader                            = NULL;
ogl_program      _particle_generate2_program                            = NULL;
GLuint           _particle_generate2_tfo_id                             = 0;
ogl_shader       _particle_generate2_vshader                            = NULL;
float            _particle_gravity                                      = 0.896f;
float            _particle_max_mass_delta                               = 6.0f;
float            _particle_min_mass                                     = 1.0f;
float            _particle_spread                                       = 70.0f;
uint32_t         _particle_stage_id                                     = -1;
ogl_program      _particle_update_program                               = NULL;
GLuint           _particle_update_tfo_id                                = 0;
GLint            _particle_update_program_decay_uniform_location        = -1;
GLint            _particle_update_program_dt_uniform_location           = -1;
GLint            _particle_update_program_gravity_uniform_location      = -1;
GLint            _particle_update_program_n_total_particles_location    = -1;
ogl_shader       _particle_update_vshader                               = NULL;
system_matrix4x4 _matrix_projection                                     = NULL;
system_matrix4x4 _matrix_view                                           = NULL;
GLuint           _vao_id                                                = -1;

/** TODO */
static void _stage_particle_step_draw(ogl_context          context,
                                      system_timeline_time time,
                                      void*                not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Set up */
    const GLuint draw_po_id = ogl_program_get_id(_particle_draw_program);

    entrypoints->pGLBindVertexArray(_vao_id);
    entrypoints->pGLUseProgram     (draw_po_id);

    /* Update the uniforms */
    system_matrix4x4 vp = NULL;

    ogl_flyby_get_property(context,
                           OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                          &_matrix_view);

    vp = system_matrix4x4_create_by_mul(_matrix_projection,
                                        _matrix_view);
    {
        entrypoints->pGLProgramUniformMatrix4fv(draw_po_id,
                                                _particle_draw_program_projection_view_location,
                                                1, /* count */
                                                GL_TRUE,
                                                system_matrix4x4_get_row_major_data(vp) );
    }
    system_matrix4x4_release(vp);

    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                            GL_TEXTURE_BUFFER,
                                            _particle_data_tbo);

    entrypoints->pGLClearColor(0,     /* red */
                               0.25f, /* green */
                               0,     /* blue */
                               0);    /* alpha */
    entrypoints->pGLClear     (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    entrypoints->pGLEnable   (GL_BLEND);
    entrypoints->pGLBlendFunc(GL_SRC_ALPHA,
                              GL_ONE);
    {
        entrypoints->pGLDrawArrays(GL_POINTS,
                                   0, /* first */
                                   N_PARTICLES_ALIGNED_4);
    }
    entrypoints->pGLDisable(GL_BLEND);
}

/** TODO */
static void _stage_particle_step_init(ogl_context context, system_timeline_time time, void* not_used)
{
    if (!_particle_data_initialized)
    {
        const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
        const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &dsa_entrypoints);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints);

        /* General */
        entrypoints->pGLBindVertexArray(_vao_id);
        entrypoints->pGLEnable         (GL_RASTERIZER_DISCARD);

        /* Stage 1 */
        const GLuint stage1_po_id = ogl_program_get_id(_particle_generate1_program);

        entrypoints->pGLProgramUniform1f(stage1_po_id,
                                         _particle_generate1_program_max_mass_delta_location,
                                         _particle_max_mass_delta);
        entrypoints->pGLProgramUniform1f(stage1_po_id,
                                         _particle_generate1_program_min_mass_location,
                                         _particle_min_mass);
        entrypoints->pGLProgramUniform1f(stage1_po_id,
                                         _particle_generate1_program_spread_location,
                                         _particle_spread);

        entrypoints->pGLUseProgram           (stage1_po_id);
        entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                              _particle_generate1_tfo_id);

        entrypoints->pGLBeginTransformFeedback(GL_POINTS);
        {
            entrypoints->pGLDrawArrays(GL_POINTS,
                                       0, /* first */
                                       N_PARTICLES_ALIGNED_4);
        }
        entrypoints->pGLEndTransformFeedback();

        /* Stage 2 */
        const GLuint stage2_po_id = ogl_program_get_id(_particle_generate2_program);

        entrypoints->pGLUseProgram           (stage2_po_id);
        entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                              _particle_generate2_tfo_id);
        {
            entrypoints->pGLBeginTransformFeedback(GL_POINTS);
            {
                entrypoints->pGLDrawArrays(GL_POINTS,
                                           0, /* first */
                                           N_PARTICLES_ALIGNED_4);
            }
            entrypoints->pGLEndTransformFeedback();
        }
        entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                              0);

        /* Flyby */
        if (!_particle_flyby_initialized)
        {
            const float camera_movement_delta = 0.025f;
            const float camera_pitch          = -0.2120f;
            const float camera_position[]     = {32.8824f, 12.5255f, -30.4577f};
            const float camera_yaw            = -0.6440f;

            ogl_flyby_activate(context,
                               camera_position);

            ogl_flyby_set_property(context,
                                   OGL_FLYBY_PROPERTY_MOVEMENT_DELTA,
                                  &camera_movement_delta);
            ogl_flyby_set_property(context,
                                   OGL_FLYBY_PROPERTY_PITCH,
                                  &camera_pitch);
            ogl_flyby_set_property(context,
                                   OGL_FLYBY_PROPERTY_YAW,
                                  &camera_yaw);

            _particle_flyby_initialized = true;
        }

        /* Matrices */
        if (_matrix_view == NULL)
        {
            _matrix_view = system_matrix4x4_create();
        }

        /* Done */
        entrypoints->pGLDisable(GL_RASTERIZER_DISCARD);

        _particle_data_initialized = true;
    }
}

/** TODO */
static void _stage_particle_step_update(ogl_context          context,
                                        system_timeline_time time,
                                        void*                not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;
    const GLuint                                              update_po_gl_id = ogl_program_get_id(_particle_update_program);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLBindVertexArray(_vao_id);

    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                            GL_TEXTURE_BUFFER,
                                            _particle_data_tbo);
    entrypoints->pGLProgramUniform1f       (update_po_gl_id,
                                            _particle_update_program_dt_uniform_location,
                                            _particle_dt);
    entrypoints->pGLProgramUniform1f       (update_po_gl_id,
                                            _particle_update_program_gravity_uniform_location,
                                            _particle_gravity);
    entrypoints->pGLProgramUniform1f       (update_po_gl_id,
                                            _particle_update_program_decay_uniform_location,
                                            _particle_decay);

    entrypoints->pGLUseProgram           (update_po_gl_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          _particle_update_tfo_id);

    entrypoints->pGLEnable                (GL_RASTERIZER_DISCARD);
    entrypoints->pGLBeginTransformFeedback(GL_POINTS);
    {
        entrypoints->pGLDrawArrays(GL_POINTS,
                                   0, /* first */
                                   N_PARTICLES_ALIGNED_4);
    }
    entrypoints->pGLEndTransformFeedback();
    entrypoints->pGLDisable             (GL_RASTERIZER_DISCARD);
}


/** Please see header for specification */
PUBLIC uint32_t stage_particle_get_stage_id()
{
    return _particle_stage_id;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_particle_deinit(__in __notnull ogl_context,
                                                         __in __notnull ogl_pipeline)
{
    ogl_texture_release(_particle_data_tbo);

    ogl_shader_release (_particle_draw_fshader);
    ogl_shader_release (_particle_draw_vshader);
    ogl_program_release(_particle_draw_program);

    ogl_shader_release (_particle_generate1_vshader);
    ogl_program_release(_particle_generate1_program);

    ogl_shader_release (_particle_generate2_vshader);
    ogl_program_release(_particle_generate2_program);

    ogl_shader_release (_particle_update_vshader);
    ogl_program_release(_particle_update_program);
}

/** Please see header for specification */
PUBLIC float stage_particle_get_decay()
{
    return _particle_decay;
}

/** Please see header for specification */
PUBLIC float stage_particle_get_dt()
{
    return _particle_dt;
}

/** Please see header for specification */
PUBLIC float stage_particle_get_gravity()
{
    return _particle_gravity;
}

/** Please see header for specification */
PUBLIC float stage_particle_get_minimum_mass()
{
    return _particle_min_mass;
}

/** Please see header for specification */
PUBLIC float stage_particle_get_spread()
{
    return _particle_spread;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_particle_init(__in __notnull ogl_context  context,
                                                       __in __notnull ogl_pipeline pipeline)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* General */
    entrypoints->pGLGenVertexArrays(1, /* n */
                                   &_vao_id);

    /* Set up particle data buffer object */
    entrypoints->pGLGenBuffers            (1,
                                          &_particle_data_bo_id);
    dsa_entrypoints->pGLNamedBufferDataEXT(_particle_data_bo_id,
                                           sizeof(float) * N_PARTICLES_ALIGNED_4 * 17,
                                           NULL, /* data */
                                           GL_STREAM_DRAW);

    /* Set up a corresponding texture buffer object */
    _particle_data_tbo = ogl_texture_create_empty(context,
                                                  system_hashed_ansi_string_create("Particle data") );

    dsa_entrypoints->pGLTextureBufferEXT(_particle_data_tbo,
                                         GL_TEXTURE_BUFFER,
                                         GL_RGBA32F,
                                         _particle_data_bo_id);

    /******************************************* PARTICLE DRAW *****************************************************************/
    const ogl_program_uniform_descriptor* draw_data_particles_ptr    = NULL;
    const ogl_program_uniform_descriptor* draw_n_total_particles_ptr = NULL;
    const ogl_program_uniform_descriptor* draw_projection_view_ptr   = NULL;
    GLuint                                draw_program_gl_id         = 0;

    _particle_draw_fshader = ogl_shader_create (context,
                                                SHADER_TYPE_FRAGMENT,
                                                system_hashed_ansi_string_create("particle draw fragment") );
    _particle_draw_program = ogl_program_create(context,
                                                system_hashed_ansi_string_create("particle draw") );
    _particle_draw_vshader = ogl_shader_create (context,
                                                SHADER_TYPE_VERTEX,
                                                system_hashed_ansi_string_create("particle draw vertex") );

    ogl_shader_set_body(_particle_draw_fshader,
                        system_hashed_ansi_string_create(_particle_draw_fragment_shader_body) );
    ogl_shader_set_body(_particle_draw_vshader,
                        system_hashed_ansi_string_create(_particle_draw_vertex_shader_body) );

    ogl_program_attach_shader(_particle_draw_program,
                              _particle_draw_fshader);
    ogl_program_attach_shader(_particle_draw_program,
                              _particle_draw_vshader);

    ogl_shader_compile(_particle_draw_fshader);
    ogl_shader_compile(_particle_draw_vshader);

    ogl_program_link(_particle_draw_program);

    draw_program_gl_id = ogl_program_get_id(_particle_draw_program);

    ogl_program_get_uniform_by_name(_particle_draw_program,
                                    system_hashed_ansi_string_create("data"),
                                   &draw_data_particles_ptr);
    ogl_program_get_uniform_by_name(_particle_draw_program,
                                    system_hashed_ansi_string_create("n_total_particles"),
                                   &draw_n_total_particles_ptr);
    ogl_program_get_uniform_by_name(_particle_draw_program,
                                    system_hashed_ansi_string_create("projection_view"),
                                   &draw_projection_view_ptr);

    if (draw_data_particles_ptr != NULL)
    {
        _particle_draw_program_data_location = draw_data_particles_ptr->location;
    }
    else
    {
        _particle_draw_program_data_location = -1;
    }

    if (draw_n_total_particles_ptr != NULL)
    {
        _particle_draw_program_n_total_particles_location = draw_n_total_particles_ptr->location;
    }
    else
    {
        _particle_draw_program_n_total_particles_location = -1;
    }

    if (draw_projection_view_ptr != NULL)
    {
        _particle_draw_program_projection_view_location = draw_projection_view_ptr->location;
    }
    else
    {
        _particle_draw_program_projection_view_location = -1;
    }

    entrypoints->pGLProgramUniform1i(draw_program_gl_id,
                                     _particle_draw_program_data_location,
                                     0); /* GL_TEXTURE0 */
    entrypoints->pGLProgramUniform1i(draw_program_gl_id,
                                     _particle_draw_program_n_total_particles_location,
                                     N_PARTICLES_ALIGNED_4);

    /**************************************** PARTICLE GENERATE 1 **************************************************************/
    const ogl_program_uniform_descriptor* generate1_max_mass_delta_ptr    = NULL;
    const ogl_program_uniform_descriptor* generate1_min_mass_ptr          = NULL;
    const ogl_program_uniform_descriptor* generate1_n_total_particles_ptr = NULL;
    const ogl_program_uniform_descriptor* generate1_spread_ptr            = NULL;

    const char* generate1_tf_variables[] =
    {
        "out_mass",
        "out_velocity",
        "out_force",
        "out_color"
    };

    const uint32_t n_generate1_tf_variables = sizeof(generate1_tf_variables) /
                                              sizeof(generate1_tf_variables[0]);
    GLuint         generate1_program_gl_id  = 0;

    _particle_generate1_program = ogl_program_create(context,
                                                     system_hashed_ansi_string_create("particle generate") );
    _particle_generate1_vshader = ogl_shader_create (context,
                                                     SHADER_TYPE_VERTEX,
                                                     system_hashed_ansi_string_create("particle generate") );
    generate1_program_gl_id     = ogl_program_get_id(_particle_generate1_program);

    ogl_shader_set_body(_particle_generate1_vshader,
                        system_hashed_ansi_string_create(_particle_generate1_vertex_shader_body) );
    ogl_shader_compile (_particle_generate1_vshader);

    entrypoints->pGLTransformFeedbackVaryings(generate1_program_gl_id,
                                              n_generate1_tf_variables,
                                              generate1_tf_variables,
                                              GL_SEPARATE_ATTRIBS);

    ogl_program_attach_shader(_particle_generate1_program,
                              _particle_generate1_vshader);
    ogl_program_link         (_particle_generate1_program);

    ogl_program_get_uniform_by_name(_particle_generate1_program,
                                    system_hashed_ansi_string_create("max_mass_delta"),
                                   &generate1_max_mass_delta_ptr);
    ogl_program_get_uniform_by_name(_particle_generate1_program,
                                    system_hashed_ansi_string_create("min_mass"),
                                   &generate1_min_mass_ptr);
    ogl_program_get_uniform_by_name(_particle_generate1_program,
                                    system_hashed_ansi_string_create("n_total_particles"),
                                   &generate1_n_total_particles_ptr);
    ogl_program_get_uniform_by_name(_particle_generate1_program,
                                    system_hashed_ansi_string_create("spread"),
                                   &generate1_spread_ptr);

    _particle_generate1_program_max_mass_delta_location    = generate1_max_mass_delta_ptr->location;
    _particle_generate1_program_min_mass_location          = generate1_min_mass_ptr->location;
    _particle_generate1_program_n_total_particles_location = generate1_n_total_particles_ptr->location;
    _particle_generate1_program_spread_location            = (generate1_spread_ptr != NULL ? generate1_spread_ptr->location : -1);

    entrypoints->pGLProgramUniform1i(generate1_program_gl_id,
                                     _particle_generate1_program_n_total_particles_location,
                                     N_PARTICLES_ALIGNED_4);

    /* Set up TFO */
    entrypoints->pGLGenTransformFeedbacks(1, /* n */
                                         &_particle_generate1_tfo_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          _particle_generate1_tfo_id);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_GENERATE1_OUT_MASS_TF_INDEX,
                                          _particle_data_bo_id,
                                          0,                                        /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) );  /* size */
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_GENERATE1_OUT_VELOCITY_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 5, /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);/* size */
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_GENERATE1_OUT_FORCE_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 9, /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);/* size */
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_GENERATE1_OUT_COLOR_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 13, /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4); /* size */

    /**************************************** PARTICLE GENERATE 2 **************************************************************/
    const char*    generate2_tf_variables[] = {"out_position"};
    const uint32_t n_generate2_tf_variables = sizeof(generate2_tf_variables) /
                                              sizeof(generate2_tf_variables[0]);
    GLuint         generate2_program_gl_id  = 0;

    _particle_generate2_program = ogl_program_create(context,
                                                     system_hashed_ansi_string_create("particle generate 2") );
    _particle_generate2_vshader = ogl_shader_create (context,
                                                     SHADER_TYPE_VERTEX,
                                                     system_hashed_ansi_string_create("particle generate 2") );
    generate2_program_gl_id     = ogl_program_get_id(_particle_generate2_program);

    ogl_shader_set_body(_particle_generate2_vshader,
                        system_hashed_ansi_string_create(_particle_generate2_vertex_shader_body) );

    ogl_shader_compile (_particle_generate2_vshader);

    entrypoints->pGLTransformFeedbackVaryings(generate2_program_gl_id,
                                              n_generate2_tf_variables,
                                              generate2_tf_variables,
                                              GL_SEPARATE_ATTRIBS);

    ogl_program_attach_shader(_particle_generate2_program,
                              _particle_generate2_vshader);
    ogl_program_link         (_particle_generate2_program);

    /* Set up TFO */
    entrypoints->pGLGenTransformFeedbacks(1,
                                         &_particle_generate2_tfo_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          _particle_generate2_tfo_id);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_GENERATE2_OUT_POSITION_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float),      /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4); /* size */

    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          0);
    /**************************************** PARTICLE UPDATE **************************************************************/
    /* Create objects */
    _particle_update_program = ogl_program_create(context,
                                                  system_hashed_ansi_string_create("particle update") );
    _particle_update_vshader = ogl_shader_create (context,
                                                  SHADER_TYPE_VERTEX,
                                                  system_hashed_ansi_string_create("particle update") );

    /* Set up particle update vshader object */
    ogl_shader_set_body(_particle_update_vshader,
                        system_hashed_ansi_string_create(_particle_update_vertex_shader_body) );

    ogl_shader_compile (_particle_update_vshader);

    /* Set up particle update program */
    const ogl_program_uniform_descriptor* data_ptr              = NULL;
    const ogl_program_uniform_descriptor* decay_ptr             = NULL;
    const ogl_program_uniform_descriptor* dt_ptr                = NULL;
    const ogl_program_uniform_descriptor* gravity_ptr           = NULL;
    const ogl_program_uniform_descriptor* n_total_particles_ptr = NULL;

    const char* update_tf_variables[] =
    {
        "out_position",
        "out_velocity",
        "out_force"
    };
    const uint32_t n_update_tf_variables = sizeof(update_tf_variables) /
                                           sizeof(update_tf_variables[0]);
    GLuint         update_program_gl_id  = ogl_program_get_id(_particle_update_program);

    entrypoints->pGLTransformFeedbackVaryings(update_program_gl_id,
                                              n_update_tf_variables,
                                              update_tf_variables,
                                              GL_SEPARATE_ATTRIBS);

    ogl_program_attach_shader(_particle_update_program,
                              _particle_update_vshader);
    ogl_program_link         (_particle_update_program);

    ogl_program_get_uniform_by_name(_particle_update_program,
                                    system_hashed_ansi_string_create("data"),
                                   &data_ptr);
    ogl_program_get_uniform_by_name(_particle_update_program,
                                    system_hashed_ansi_string_create("decay"),
                                   &decay_ptr);
    ogl_program_get_uniform_by_name(_particle_update_program,
                                    system_hashed_ansi_string_create("dt"),
                                   &dt_ptr);
    ogl_program_get_uniform_by_name(_particle_update_program,
                                    system_hashed_ansi_string_create("gravity"),
                                   &gravity_ptr);
    ogl_program_get_uniform_by_name(_particle_update_program,
                                    system_hashed_ansi_string_create("n_total_particles"),
                                   &n_total_particles_ptr);

    _particle_update_program_decay_uniform_location     = decay_ptr->location;
    _particle_update_program_dt_uniform_location        = dt_ptr->location;
    _particle_update_program_gravity_uniform_location   = gravity_ptr->location;
    _particle_update_program_n_total_particles_location = n_total_particles_ptr->location;

    entrypoints->pGLProgramUniform1i(update_program_gl_id,
                                     data_ptr->location,
                                     0); /* GL_TEXTURE0 */
    entrypoints->pGLProgramUniform1i(update_program_gl_id,
                                     _particle_update_program_n_total_particles_location,
                                     N_PARTICLES_ALIGNED_4);

    /* Set up TFO */
    entrypoints->pGLGenTransformFeedbacks(1,
                                         &_particle_update_tfo_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          _particle_update_tfo_id);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_UPDATE_OUT_POSITION_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float),      /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4); /* size */
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_UPDATE_OUT_VELOCITY_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 5,  /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4); /* size */
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER,
                                          N_UPDATE_OUT_FORCE_TF_INDEX,
                                          _particle_data_bo_id,
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 9, /* offset */
                                          N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);/* size */

    /* Set up a new pipeline stage */
    _particle_stage_id = ogl_pipeline_add_stage(pipeline);

    ogl_pipeline_add_stage_step(pipeline,
                                _particle_stage_id,
                                system_hashed_ansi_string_create("data init"),
                                _stage_particle_step_init,
                                NULL); /* step_callback_user_arg */
    ogl_pipeline_add_stage_step(pipeline,
                                _particle_stage_id,
                                system_hashed_ansi_string_create("data update"),
                                _stage_particle_step_update,
                                NULL); /* step_callback_user_arg */
    ogl_pipeline_add_stage_step(pipeline,
                                _particle_stage_id,
                                system_hashed_ansi_string_create("data draw"),
                                _stage_particle_step_draw,
                                NULL); /* step_callback_user_arg */

    /* Matrices */
    _matrix_projection = system_matrix4x4_create_perspective_projection_matrix(45.0f,           /* fov_y */
                                                                               640.0f / 480.0f, /* ar */
                                                                               0.001f,          /* z_near */
                                                                               500.0f);         /* z_far */
}

/** Please see header for specification */
PUBLIC void stage_particle_reset()
{
    _particle_data_initialized = false;
}

/** Please see header for specification */
PUBLIC void stage_particle_set_decay(__in float new_decay)
{
    _particle_decay = new_decay;
}

/** Please see header for specification */
PUBLIC void stage_particle_set_dt(__in float new_dt)
{
    _particle_dt = new_dt;
}

/** Please see header for specification */
PUBLIC void stage_particle_set_gravity(__in float new_gravity)
{
    _particle_gravity = new_gravity;
}

/** Please see header for specification */
PUBLIC void stage_particle_set_minimum_mass(__in float new_min_mass)
{
    _particle_min_mass         = new_min_mass;
    _particle_data_initialized = false;
}

/** Please see header for specification */
PUBLIC void stage_particle_set_spread(__in float new_spread)
{
    _particle_spread           = new_spread;
    _particle_data_initialized = false;
}
