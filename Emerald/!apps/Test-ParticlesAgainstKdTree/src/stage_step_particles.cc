/**
 *
 * Particles test app (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "curve/curve_container.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kdtree.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_variant.h"
#include "stage_step_particles.h"
#include "../include/main.h"

/** NOTE: This is more or less copy of the TestParticles project shaders */
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

/** NOTE: This is more or less copy of the TestParticles project shaders */
#define N_GENERATE1_OUT_MASS_TF_INDEX            (0)
#define N_GENERATE1_OUT_VELOCITY_TF_INDEX        (1)
#define N_GENERATE1_OUT_FORCE_TF_INDEX           (2)
#define N_GENERATE1_OUT_COLOR_TF_INDEX           (3)
#define N_GENERATE2_OUT_POSITION_TF_INDEX        (0)
#define N_UPDATE_OUT_POSITION_TF_INDEX           (0)
#define N_UPDATE_OUT_VELOCITY_TF_INDEX           (1)
#define N_UPDATE_OUT_FORCE_TF_INDEX              (2)
#define N_UPDATE_OUT_COLLISION_LOCATION_TF_INDEX (3)

GLuint                 _stage_step_particles_collide_collision_data_bo                                    = 0;
cl_mem                 _stage_step_particles_collide_collision_data_mo                                    = NULL;
ogl_texture            _stage_step_particles_collide_collision_data_to                                    = 0; /* GL_RGBA32F */
uint32_t               _stage_step_particles_collide_step_id                                              = -1;
ogl_context            _stage_step_particles_context                                                      = NULL;
GLuint                 _stage_step_particles_data_bo_id                                                   = 0;
cl_mem                 _stage_step_particles_data_bo_mo                                                   = NULL;
bool                   _stage_step_particles_data_generated                                               = false;
ogl_texture            _stage_step_particles_data_tbo                                                     = 0; /* GL_RGBA32F */
ogl_program            _stage_step_particles_debug_program                                                = NULL;
GLint                  _stage_step_particles_debug_program_collision_data_location                        = -1;
GLint                  _stage_step_particles_debug_program_data_location                                  = -1;
GLint                  _stage_step_particles_debug_program_mvp_location                                   = -1;
GLint                  _stage_step_particles_debug_program_n_total_particles_location                     = -1;
GLint                  _stage_step_particles_debug_step_id                                                = -1;
float                  _stage_step_particles_decay                                                        = 0.99995f;
ogl_program            _stage_step_particles_draw_program                                                 = NULL;
GLint                  _stage_step_particles_draw_program_data_location                                   = -1;
GLint                  _stage_step_particles_draw_program_n_total_particles_location                      = -1;
GLint                  _stage_step_particles_draw_program_projection_view_location                        = -1;
GLint                  _stage_step_particles_draw_step_id                                                 = -1;
float                  _stage_step_particles_dt                                                           = 1.0f / 120.0f;
uint32_t               _stage_step_particles_generate_step_id                                             = -1;
float                  _stage_step_particles_gravity                                                      = 0.496f;
ocl_kdtree_executor_id _stage_step_particles_kdtree_float2_executor_id                                    = -1;
float                  _stage_step_particles_max_mass_delta                                               = 6.0f;
float                  _stage_step_particles_min_mass                                                     = 30.0f;
int                    _stage_step_particles_n_frames                                                     = 0;
GLint                  _stage_step_particles_generate1_program_max_mass_delta_location                    = -1;
GLint                  _stage_step_particles_generate1_program_min_mass_location                          = -1;
GLint                  _stage_step_particles_generate1_program_n_total_particles_location                 = -1;
GLint                  _stage_step_particles_generate1_program_spread_location                            = -1;
ogl_program            _stage_step_particles_generate1_program                                            = NULL;
ogl_program            _stage_step_particles_generate2_program                                            = NULL;
GLuint                 _stage_step_particles_generate1_tfo_id                                             = 0;
GLuint                 _stage_step_particles_generate2_tfo_id                                             = 0;
GLuint                 _stage_step_particles_prev_data_bo_id[2]                                           = {0};
ogl_texture            _stage_step_particles_prev_data_tbo[2]                                             = {0};
float                  _stage_step_particles_spread                                                       = 1.0f;
ogl_program            _stage_step_particles_update_program                                               = NULL;
GLint                  _stage_step_particles_update_program_barycentric_data_uniform_location             = -1;
GLint                  _stage_step_particles_update_program_decay_uniform_location                        = -1;
GLint                  _stage_step_particles_update_program_dt_uniform_location                           = -1;
GLint                  _stage_step_particles_update_program_is_prev_pass_data_available_uniform_location  = -1;
GLint                  _stage_step_particles_update_program_gravity_uniform_location                      = -1;
GLint                  _stage_step_particles_update_program_n_total_particles_location                    = -1;
GLint                  _stage_step_particles_update_program_mesh_vertex_data_uniform_location             = -1;
GLint                  _stage_step_particles_update_program_prev_pass_collide_stage_data_uniform_location = -1;
GLint                  _stage_step_particles_update_step_id                                               = -1;
GLuint                 _stage_step_particles_update_tfo_id                                                = 0;
uint32_t               _stage_step_particles_vao_id                                                       = 0;
system_matrix4x4       _stage_step_particles_view_matrix                                                  = NULL;

bool _stage_step_particles_collide_allowed = true;
bool _stage_step_particles_iterate_status  = false;
bool _stage_step_particles_iterate_frame   = _stage_step_particles_iterate_status;
bool _stage_step_particles_update_allowed  = true;

const char* _particle_debug_vertex_shader_body = "#version 330\n"
                                                 "\n"
                                                 "        uniform samplerBuffer collision_data;\n"
                                                 "        uniform samplerBuffer data;\n"
                                                 "        uniform mat4          mvp;\n"
                                                 "        uniform int           n_total_particles;\n"
                                                 "   flat out     int           instance_id;\n"
                                                 "        out     float         vertex_id_mod_2;\n"
                                                 "\n"
                                                 "void main()\n"
                                                 "{\n"
                                                 "    int   particle_vertex_id = gl_VertexID / 2;\n"
                                                 "    int   vertex_id_div_4    = particle_vertex_id / 4;\n"
                                                 "    int   vertex_id_mod_4    = particle_vertex_id % 4;\n"
                                                 "    vec3  position           = texelFetch(data, n_total_particles / 4 + particle_vertex_id).xyz;\n"
                                                 "    vec3  velocity           = texelFetch(data, n_total_particles / 4 + n_total_particles + particle_vertex_id).xyz;\n"
                                                 "    float collision_t        = texelFetch(collision_data, gl_VertexID).x;\n"
                                                  "   vec3  collision_location = position + collision_t * normalize(velocity);\n"
                                                 "    bool  is_end_vertex      = ((gl_VertexID % 2) == 1);\n"
                                                 "\n"
                                                 "    if (gl_InstanceID == 0)\n"
                                                 "    {\n"
                                                 "        vec3 result_pos = position.xyz;\n"
                                                 "\n"
                                                 "        if (is_end_vertex) result_pos += velocity.xyz;\n"
                                                 "\n"
                                                 "        gl_Position = mvp * vec4(result_pos, 1.0);\n"
                                                 "    }\n"
                                                 "    else\n"
                                                 "    {\n"
                                                 "       vec3 result_pos = position.xyz;\n"
                                                 "\n"
                                                 "       if (collision_t < -665) collision_location = result_pos;\n"
                                                 "       if (is_end_vertex) result_pos = collision_location;\n"
                                                 "\n"
                                                 "       gl_Position     = mvp * vec4(result_pos, 1.0);\n"
                                                 "       vertex_id_mod_2 = float(gl_VertexID % 2);\n"
                                                 "    }\n"
                                                 "\n"
                                                 "    instance_id = gl_InstanceID;\n"
                                                 "}\n";
const char* _particle_debug_fragment_shader_body = "#version 330\n"
                                                   "\n"
                                                   "flat in  int   instance_id;\n"
                                                   "     in  float vertex_id_mod_2;\n"
                                                   "     out vec4  color;\n"
                                                   "\n"
                                                   "void main()\n"
                                                   "{\n"
                                                   "    if (instance_id == 0)\n"
                                                   "    {\n"
                                                   "        color = vec4(0, 0, 1, 1);\n"
                                                   "    }\n"
                                                   "    else\n"
                                                   "    {\n"
                                                   "        color = vec4(0.2, 0.2, 0.2, 1) + vec4(vertex_id_mod_2) * vec4(1, 1, 1, 1);\n"
                                                   "    }\n"
                                                   "}\n";

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
                                                "    fp_color      = vec4(texelFetch(data, n_total_particles / 4 + n_total_particles * 3 + gl_VertexID).xyz, 1.0f);\n"
                                                "    gl_Position   = projection_view * position;\n"
                                                "    gl_PointSize  = 10;\n"
                                                "}\n";

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
                                                     "   float angle          = float(gl_VertexID) / float(n_total_particles) * 3.14152965;\n"
                                                     "\n"
                                                     "   out_mass     = rand_value * max_mass_delta + min_mass;\n"
                                                     "   out_velocity = vec4(0);\n"
                                                     "   out_force.x  = max(0.00001f, cos(angle) * spread * rand_value);\n"
                                                     "   out_force.y  = max(0.00001f, 1.0        * spread * rand_value);\n"
                                                     "   out_force.z  = max(0.00001f, sin(angle) * spread * rand_value);\n"
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
                                                     "   out_position = vec4(0, 4, 0, 0);\n"
                                                     "}\n";

const char* _particle_update_vertex_shader_body = "#version 330\n"
                                                  "\n"
                                                  "uniform samplerBuffer  collide_stage_barycentric_data;\n"  /* vec4 floats (t1 t2 t3 t4) */
                                                  "uniform samplerBuffer  prev_pass_data;\n"
                                                  "uniform samplerBuffer  prev_pass_collide_stage_data;\n"  /* vec4 floats (xyz_) */
                                                  "uniform float          decay;\n"
                                                  "uniform float          dt;\n"
                                                  "uniform bool           is_prev_pass_data_available;\n"
                                                  "uniform float          gravity;\n"
                                                  "uniform int            n_total_particles;\n"
                                                  "out     vec4           out_position;\n"
                                                  "out     vec4           out_velocity;\n"
                                                  "out     vec4           out_force;\n"
                                                  "out     vec3           out_collision_location;\n"
                                                  "\n"
                                                  "void main()\n"
                                                  "{\n"
                                                  "   int        vertex_id_div_4 = gl_VertexID / 4;\n"
                                                  "   int        vertex_id_mod_4 = gl_VertexID % 4;\n"
                                                  "   float      mass;\n"
                                                  "   vec3  prev_position        = texelFetch(prev_pass_data, n_total_particles / 4 +                         gl_VertexID).xyz;\n"
                                                  "   vec3  prev_velocity        = texelFetch(prev_pass_data, n_total_particles / 4 + n_total_particles * 1 + gl_VertexID).xyz;\n"
                                                  "   vec3  prev_force           = texelFetch(prev_pass_data, n_total_particles / 4 + n_total_particles * 2 + gl_VertexID).xyz;\n"
                                                  "   vec3       color           = texelFetch(prev_pass_data, n_total_particles / 4 + n_total_particles * 3 + gl_VertexID).xyz;\n"
                                                  "\n"
                                                  "   mass = texelFetch(prev_pass_data, vertex_id_div_4)[vertex_id_mod_4];\n"
                                                  /* Calculate new values */
                                                  "   vec3 new_velocity = decay * prev_velocity + (prev_force + vec3(0, -gravity, 0) / mass) * dt;\n"
                                                  "   vec3 new_position = prev_position + new_velocity;\n"
                                                  "   vec3 new_force    = vec3(0.0);\n" /* reset any initial force */
                                                  "\n"
                                                  "   if (all(equal(prev_velocity.xyz, vec3(0))) && all(equal(prev_force.xyz, vec3(0))) )\n"
                                                  "   {\n"
                                                  "       new_velocity = prev_velocity;\n"
                                                  "       new_position = prev_position;\n"
                                                  "   }\n"
                                                  "   else\n"
                                                  /* Collision */
                                                  "   {\n"
                                                  "       vec4  collision_data     = texelFetch(collide_stage_barycentric_data, gl_VertexID);\n"
                                                  "       float collision_t        = collision_data.x;\n"
                                                  "       vec3  collision_normal   = normalize(collision_data.yzw);\n"
                                                  "       vec3  collision_location = prev_position + collision_t * normalize(prev_velocity);\n"
                                                  "       float new_velocity_len   = length(new_velocity);\n"
                                                  "       float prev_velocity_len  = length(prev_velocity);\n"
                                                  "\n"
                                                  "       if (collision_t >= 0 && collision_t <= prev_velocity_len)\n"
                                                  "       {\n"
                                                  "           new_velocity = new_velocity_len * reflect(normalize(new_velocity), collision_normal);\n"
                                                  "           new_position = collision_location + new_velocity * (prev_velocity_len - collision_t);\n"
                                                  "       }\n"
                                                  "       else\n"
                                                  "       {\n"
                                                  "           if (is_prev_pass_data_available)\n"
                                                  "           {\n"
                                                  "               vec3 prev_collision_location = texelFetch(prev_pass_collide_stage_data, gl_VertexID).xyz;\n"
                                                  "\n"
                                                  "               if (length(new_position - prev_position) >= length(new_position - prev_collision_location) )"
                                                  "               {\n"
                                                  "                  new_velocity = new_velocity_len * reflect(normalize(new_velocity), collision_normal);\n"
                                                  "                  new_position = collision_location + prev_velocity * (prev_velocity_len - collision_t);\n"
                                                  "               }\n"
                                                  "           }\n"
                                                  "\n"
                                                  "           out_collision_location = collision_location;\n"
                                                  "       }\n"
                                                  "   }\n"
                                                  
                                                  /* Store results */
                                                  "   out_position = vec4(new_position, 1);\n"
                                                  "   out_velocity = vec4(new_velocity, 0);\n"
                                                  "   out_force    = vec4(new_force,    0);\n"
                                                  "}\n";

/** TODO */
static void _stage_step_particles_collide(ogl_context context, system_timeline_time time, void* not_used)
{
    ocl_context                           context_cl      = main_get_ocl_context();
    cl_command_queue                      command_queue   = ocl_context_get_command_queue(context_cl, 0);
    const ocl_khr_gl_sharing_entrypoints* ocl_entrypoints = ocl_get_khr_gl_sharing_entrypoints();
    const ogl_context_gl_entrypoints*     ogl_entrypoints = NULL;
    bool                                  result          = false;
    ocl_kdtree                            tree            = main_get_scene_kdtree();

    const cl_mem                          acquirable_mos[] = {_stage_step_particles_data_bo_mo,
                                                              _stage_step_particles_collide_collision_data_mo};
    const uint32_t                        n_acquirable_mos = sizeof(acquirable_mos) / sizeof(acquirable_mos[0]);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &ogl_entrypoints);

    if (!_stage_step_particles_collide_allowed)
    {
        return;
    }

    ogl_entrypoints->pGLFinish();
    if (ocl_entrypoints->pCLEnqueueAcquireGLObjects(command_queue, n_acquirable_mos, acquirable_mos, 0, 0, NULL) != CL_SUCCESS)
    {
        ASSERT_DEBUG_SYNC(false, "Could not acquire GL buffer objects");
    }

    result = ocl_kdtree_intersect_rays(tree,
                                       _stage_step_particles_kdtree_float2_executor_id,
                                       1,                                                              /* a single ray per particle location */
                                       N_PARTICLES_ALIGNED_4,                                          /* many particle locations */
                                       NULL,                                                           /* we're using a buffer object-backed memory object instead of client-side pointer */
                                       _stage_step_particles_data_bo_mo,
                                       N_PARTICLES_ALIGNED_4, /* position data */
                                       _stage_step_particles_data_bo_mo,
                                       N_PARTICLES_ALIGNED_4 + N_PARTICLES_ALIGNED_4 * 4,
                                       _stage_step_particles_collide_collision_data_mo,
                                       NULL,
                                       false,                                            /* don't need barycentric data */
                                       true,                                             /* find closest intersection */
                                       NULL,                                             /* we don't need callbacks */
                                       NULL);                                            /* we don't need callbacks */
    ASSERT_DEBUG_SYNC(result, "Kd Tree intersection failed");

    if (ocl_entrypoints->pCLEnqueueReleaseGLObjects(command_queue, n_acquirable_mos, acquirable_mos, 0, 0, NULL) != CL_SUCCESS)
    {
        ASSERT_DEBUG_SYNC(false, "Could not release GL buffer object");
    }

    _stage_step_particles_collide_allowed = false;
}

/** TODO */
static void _stage_step_particles_debug(ogl_context context, system_timeline_time time, void* not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    if (_stage_step_particles_iterate_status)
    {
        /* Set up */
        const GLuint debug_po_id = ogl_program_get_id(_stage_step_particles_debug_program);

        entrypoints->pGLBindVertexArray(_stage_step_particles_vao_id);
        entrypoints->pGLUseProgram     (debug_po_id);

        /* Update the uniforms */
        system_matrix4x4 vp = NULL;

             ogl_flyby_get_view_matrix     (context,                      _stage_step_particles_view_matrix);
        vp = system_matrix4x4_create_by_mul(main_get_projection_matrix(), _stage_step_particles_view_matrix);
        {
            entrypoints->pGLProgramUniformMatrix4fv(debug_po_id, _stage_step_particles_debug_program_mvp_location, 1, GL_TRUE, system_matrix4x4_get_row_major_data(vp) );
        }
        system_matrix4x4_release(vp);

        dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, _stage_step_particles_data_tbo);
        dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE1, GL_TEXTURE_BUFFER, _stage_step_particles_collide_collision_data_to);

        entrypoints->pGLEnable(GL_BLEND);
        entrypoints->pGLEnable(GL_DEPTH_TEST);
        entrypoints->pGLBlendFunc(GL_SRC_ALPHA, GL_ONE);
        {
            entrypoints->pGLDrawArraysInstanced(GL_LINES, 0, 2*N_PARTICLES_ALIGNED_4, 2 /* instances */);
        }
        entrypoints->pGLDisable(GL_BLEND);
        entrypoints->pGLDisable(GL_DEPTH_TEST);
    }
}

/** TODO */
static void _stage_step_particles_draw(ogl_context context, system_timeline_time time, void* not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entry_points);

    /* Set up */
    const GLuint draw_po_id = ogl_program_get_id(_stage_step_particles_draw_program);

    entrypoints->pGLBindVertexArray(_stage_step_particles_vao_id);
    entrypoints->pGLUseProgram     (draw_po_id);

    /* Update the uniforms */
    system_matrix4x4 vp = NULL;

         ogl_flyby_get_view_matrix     (context,                      _stage_step_particles_view_matrix);
    vp = system_matrix4x4_create_by_mul(main_get_projection_matrix(), _stage_step_particles_view_matrix);
    {
        entrypoints->pGLProgramUniformMatrix4fv(draw_po_id, _stage_step_particles_draw_program_projection_view_location, 1, GL_TRUE, system_matrix4x4_get_row_major_data(vp) );
    }
    system_matrix4x4_release(vp);

    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, _stage_step_particles_data_tbo);

    entrypoints->pGLEnable(GL_BLEND);
    entrypoints->pGLEnable(GL_DEPTH_TEST);
    entrypoints->pGLDepthMask(GL_FALSE);
    entrypoints->pGLBlendFunc(GL_SRC_ALPHA, GL_ONE);
    {
        entrypoints->pGLDrawArrays(GL_POINTS, 0, N_PARTICLES_ALIGNED_4);
    }
    entrypoints->pGLDepthMask(GL_TRUE);
    entrypoints->pGLDisable(GL_BLEND);
    entrypoints->pGLEnable(GL_DEPTH_TEST);
}

/** TODO */
static void _stage_step_particles_generate(ogl_context context, system_timeline_time time, void* not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    if (_stage_step_particles_iterate_frame || !_stage_step_particles_iterate_status)
    {
        _stage_step_particles_collide_allowed = true;
        _stage_step_particles_update_allowed  = true;
        _stage_step_particles_iterate_frame   = false;
    }

    if (!_stage_step_particles_data_generated)
    {

        /* General */
        entrypoints->pGLBindVertexArray(_stage_step_particles_vao_id);
        entrypoints->pGLEnable(GL_RASTERIZER_DISCARD);

        /* Barycentric data buffer must be filled with negative values for our hacked approach to work correctly */
        {
            const uint32_t n_entries        = N_PARTICLES_ALIGNED_4 * 4;
            float*         barycentric_data = new (std::nothrow) float[n_entries];
        
            for (unsigned int n = 0; n < n_entries; ++n)
            {
                barycentric_data[n] = -666;
            }

            dsa_entrypoints->pGLNamedBufferSubDataEXT(_stage_step_particles_collide_collision_data_bo, 
                                                      0,
                                                      n_entries * sizeof(float),
                                                      barycentric_data);

            delete [] barycentric_data;
        }

        /* Stage 1 */
        const GLuint stage1_po_id = ogl_program_get_id(_stage_step_particles_generate1_program);

        entrypoints->pGLProgramUniform1f(stage1_po_id, _stage_step_particles_generate1_program_max_mass_delta_location, _stage_step_particles_max_mass_delta);
        entrypoints->pGLProgramUniform1f(stage1_po_id, _stage_step_particles_generate1_program_min_mass_location,       _stage_step_particles_min_mass);
        entrypoints->pGLProgramUniform1f(stage1_po_id, _stage_step_particles_generate1_program_spread_location,         _stage_step_particles_spread);

        entrypoints->pGLUseProgram           (stage1_po_id);
        entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _stage_step_particles_generate1_tfo_id);

        entrypoints->pGLBeginTransformFeedback(GL_POINTS);
        {
            entrypoints->pGLDrawArrays(GL_POINTS, 0, N_PARTICLES_ALIGNED_4);
        }
        entrypoints->pGLEndTransformFeedback();

        /* Stage 2 */
        const GLuint stage2_po_id = ogl_program_get_id(_stage_step_particles_generate2_program);

        entrypoints->pGLUseProgram(stage2_po_id);
        entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _stage_step_particles_generate2_tfo_id);
        {
            entrypoints->pGLBeginTransformFeedback(GL_POINTS);
            {
                entrypoints->pGLDrawArrays(GL_POINTS, 0, N_PARTICLES_ALIGNED_4);
            }
            entrypoints->pGLEndTransformFeedback();
        }
        entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

        /* Matrices */
        if (_stage_step_particles_view_matrix == NULL)
        {
            _stage_step_particles_view_matrix = system_matrix4x4_create();
        }

        /* Update collide step's flag so that previous pass data won't be considered - there is no previous pass,
         * now that we've just started a new run! */
        entrypoints->pGLProgramUniform1i(ogl_program_get_id(_stage_step_particles_update_program), _stage_step_particles_update_program_is_prev_pass_data_available_uniform_location, 0);

        /* Fill the 'previous data' buffer with nans */
        {
            uint32_t n_prev_pass_entries = N_PARTICLES_ALIGNED_4 * 3;
            char*    prev_pass_nan_data  = new (std::nothrow) char[sizeof(float) * n_prev_pass_entries];

            memset(prev_pass_nan_data, 0, n_prev_pass_entries * sizeof(float) );

            dsa_entrypoints->pGLNamedBufferSubDataEXT(_stage_step_particles_prev_data_bo_id[0], 0, n_prev_pass_entries * sizeof(float), prev_pass_nan_data);
            dsa_entrypoints->pGLNamedBufferSubDataEXT(_stage_step_particles_prev_data_bo_id[1], 0, n_prev_pass_entries * sizeof(float), prev_pass_nan_data);

            delete [] prev_pass_nan_data;
            prev_pass_nan_data = NULL;
        }

        /* Done */
        entrypoints->pGLDisable(GL_RASTERIZER_DISCARD);

        _stage_step_particles_data_generated = true;
    }
    else
    {
        /* Data is already available */
        entrypoints->pGLProgramUniform1i(ogl_program_get_id(_stage_step_particles_update_program), _stage_step_particles_update_program_is_prev_pass_data_available_uniform_location, 1);
    }
}

/** TODO */
static void _stage_step_particles_update(ogl_context context, system_timeline_time time, void* not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;
    const GLuint                                              update_po_gl_id = ogl_program_get_id(_stage_step_particles_update_program);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    if (!_stage_step_particles_update_allowed)
    {
        return;
    }

    entrypoints->pGLBindVertexArray(_stage_step_particles_vao_id);
    
    entrypoints->pGLProgramUniform1f(update_po_gl_id, _stage_step_particles_update_program_dt_uniform_location,      _stage_step_particles_dt);
    entrypoints->pGLProgramUniform1f(update_po_gl_id, _stage_step_particles_update_program_gravity_uniform_location, _stage_step_particles_gravity);
    entrypoints->pGLProgramUniform1f(update_po_gl_id, _stage_step_particles_update_program_decay_uniform_location,   _stage_step_particles_decay);

    entrypoints->pGLUseProgram           (update_po_gl_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK, _stage_step_particles_update_tfo_id);
    entrypoints->pGLBindBufferBase       (GL_TRANSFORM_FEEDBACK_BUFFER, N_UPDATE_OUT_COLLISION_LOCATION_TF_INDEX, _stage_step_particles_prev_data_bo_id[(_stage_step_particles_n_frames+1) % 2]);

    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, _stage_step_particles_data_tbo);
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE1, GL_TEXTURE_BUFFER, _stage_step_particles_collide_collision_data_to);
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE4, GL_TEXTURE_BUFFER, _stage_step_particles_prev_data_tbo[(_stage_step_particles_n_frames) % 2]);

    entrypoints->pGLEnable                (GL_RASTERIZER_DISCARD);
    entrypoints->pGLBeginTransformFeedback(GL_POINTS);
    {
        entrypoints->pGLDrawArrays(GL_POINTS, 0, N_PARTICLES_ALIGNED_4);
    }
    entrypoints->pGLEndTransformFeedback();
    entrypoints->pGLDisable             (GL_RASTERIZER_DISCARD);

    _stage_step_particles_n_frames++;
    _stage_step_particles_update_allowed = false;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_particles_deinit(__in __notnull ogl_context  context,
                                                               __in __notnull ogl_pipeline pipeline)
{
    const ocl_11_entrypoints*         cl_entrypoints = ocl_get_entrypoints();
    const ogl_context_gl_entrypoints* gl_entrypoints = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &gl_entrypoints);

    /* Programs */
    if (_stage_step_particles_debug_program != NULL)
    {
        ogl_program_release(_stage_step_particles_debug_program);
    }

    if (_stage_step_particles_draw_program != NULL)
    {
        ogl_program_release(_stage_step_particles_draw_program);
    }

    if (_stage_step_particles_generate1_program != NULL)
    {
        ogl_program_release(_stage_step_particles_generate1_program);
    }

    if (_stage_step_particles_generate2_program != NULL)
    {
        ogl_program_release(_stage_step_particles_generate2_program);
    }

    if (_stage_step_particles_update_program != NULL)
    {
        ogl_program_release(_stage_step_particles_update_program);
    }

    /* GL stuff */
    if (_stage_step_particles_context != NULL)
    {
        ogl_context_release(_stage_step_particles_context);
    }

    /* Memory objects*/
    if (_stage_step_particles_data_bo_mo != NULL)
    {
        cl_entrypoints->pCLReleaseMemObject(_stage_step_particles_data_bo_mo);

        _stage_step_particles_data_bo_mo = NULL;
    }
    if (_stage_step_particles_collide_collision_data_mo != NULL)
    {
        cl_entrypoints->pCLReleaseMemObject(_stage_step_particles_collide_collision_data_mo);

        _stage_step_particles_collide_collision_data_mo = NULL;
    }

    /* Buffer objects */
    if (_stage_step_particles_collide_collision_data_bo != 0)
    {
        gl_entrypoints->pGLDeleteBuffers(1, &_stage_step_particles_collide_collision_data_bo);

        _stage_step_particles_collide_collision_data_bo = 0;
    }

    if (_stage_step_particles_data_bo_id != NULL)
    {
        gl_entrypoints->pGLDeleteBuffers(1, &_stage_step_particles_data_bo_id);

        _stage_step_particles_data_bo_id = 0;
    }

    ogl_texture_release(_stage_step_particles_collide_collision_data_to);
    ogl_texture_release(_stage_step_particles_data_tbo);
    ogl_texture_release(_stage_step_particles_prev_data_tbo[0]);
    ogl_texture_release(_stage_step_particles_prev_data_tbo[1]);
}

/** Please see header for specification */
PUBLIC bool stage_step_particles_get_debug()
{
    return _stage_step_particles_iterate_status;
}

/** Please see header for specification */
PUBLIC float stage_step_particles_get_decay()
{
    return _stage_step_particles_decay;
}

/** Please see header for specification */
PUBLIC float stage_step_particles_get_dt()
{
    return _stage_step_particles_dt;
}

/** Please see header for specification */
PUBLIC float stage_step_particles_get_gravity()
{
    return _stage_step_particles_gravity;
}

/** Please see header for specification */
PUBLIC float stage_step_particles_get_maximum_mass_delta()
{
    return _stage_step_particles_max_mass_delta;
}

/** Please see header for specification */
PUBLIC float stage_step_particles_get_minimum_mass()
{
    return _stage_step_particles_min_mass;
}

/** Please see header for specification */
PUBLIC float stage_step_particles_get_spread()
{
    return _stage_step_particles_spread;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_particles_init(__in __notnull ogl_context  context,
                                                             __in __notnull ogl_pipeline pipeline,
                                                             __in           uint32_t     stage_id)
{
    _stage_step_particles_context = context;
    ogl_context_retain(context);

    const ocl_khr_gl_sharing_entrypoints*                     cl_gl_entrypoints = ocl_get_khr_gl_sharing_entrypoints();
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints   = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints       = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    /* General */
    entrypoints->pGLGenVertexArrays(1, &_stage_step_particles_vao_id);

    /* Set up particle data buffer object */
    entrypoints->pGLGenBuffers            (1, &_stage_step_particles_data_bo_id);
    dsa_entrypoints->pGLNamedBufferDataEXT(_stage_step_particles_data_bo_id, sizeof(float) * N_PARTICLES_ALIGNED_4 * 17, NULL, GL_STREAM_DRAW);

    /* Set up prev pass' collision data buffer object */
    entrypoints->pGLGenBuffers (2, _stage_step_particles_prev_data_bo_id);

    _stage_step_particles_prev_data_tbo[0] = ogl_texture_create(context, system_hashed_ansi_string_create("Prev data TBO 1") );
    _stage_step_particles_prev_data_tbo[1] = ogl_texture_create(context, system_hashed_ansi_string_create("Prev data TBO 2") );

    for (uint32_t n = 0; n < 2; ++n)
    {
        dsa_entrypoints->pGLNamedBufferDataEXT(_stage_step_particles_prev_data_bo_id[n], sizeof(float) * N_PARTICLES_ALIGNED_4 * 3, NULL,      GL_DYNAMIC_COPY);
        dsa_entrypoints->pGLTextureBufferEXT  (_stage_step_particles_prev_data_tbo[n],   GL_TEXTURE_BUFFER,                         GL_RGB32F, _stage_step_particles_prev_data_bo_id[n]);
    }

    /* Set up corresponding particle data memory object */
    cl_context cl_context    = ocl_context_get_context(main_get_ocl_context() );
    cl_int     cl_error_code = 0;

    _stage_step_particles_data_bo_mo = cl_gl_entrypoints->pCLCreateFromGLBuffer(cl_context,
                                                                                CL_MEM_READ_ONLY,
                                                                                _stage_step_particles_data_bo_id,
                                                                                &cl_error_code);

    ASSERT_DEBUG_SYNC(cl_error_code == CL_SUCCESS, "Could not create particle data memory object");

    /* Set up a corresponding texture buffer object */
    _stage_step_particles_data_tbo = ogl_texture_create(context, system_hashed_ansi_string_create("Particles data TBO") );

    dsa_entrypoints->pGLTextureBufferEXT(_stage_step_particles_data_tbo, GL_TEXTURE_BUFFER, GL_RGBA32F, _stage_step_particles_data_bo_id);

    /* Add executor we'll be using for chasing particles */
    _ocl_kdtree_executor_configuration executor_config = ocl_kdtree_get_float2_executor_configuration(false);
    
    executor_config.reset_cl_code  = "#define RESET_RESULT  ((__global float*)result)[4*ray_index] = -666;\n";
    executor_config.update_cl_code = "#define UPDATE_RESULT float3 t_normal = u * vload3(0, normal_a) + v * vload3(0, normal_b) + (1.0 - u - v) * vload3(0, normal_c);\\\n"
                                     "                                                                                                  \\\n"
                                     "                      vstore4((float4)(t_hit, t_normal), ray_index, (__global float*) result);\n";

    ocl_kdtree_add_executor(main_get_scene_kdtree(), executor_config, &_stage_step_particles_kdtree_float2_executor_id);

    /******************************************* PARTICLE DEBUG *****************************************************************/
    const ogl_program_uniform_descriptor* debug_collision_data_ptr    = NULL;
    const ogl_program_uniform_descriptor* debug_data_ptr              = NULL;
    const ogl_program_uniform_descriptor* debug_mvp_ptr               = NULL;
    const ogl_program_uniform_descriptor* debug_n_total_particles_ptr = NULL;
    GLuint                                debug_program_gl_id         = 0;
    ogl_shader                            debug_fshader               = NULL;
    ogl_shader                            debug_vshader               = NULL;

    debug_fshader                       = ogl_shader_create (context, SHADER_TYPE_FRAGMENT, system_hashed_ansi_string_create("particle debug fragment") );
    _stage_step_particles_debug_program = ogl_program_create(context,                       system_hashed_ansi_string_create("particle debug") );
    debug_vshader                       = ogl_shader_create (context, SHADER_TYPE_VERTEX,   system_hashed_ansi_string_create("particle debug vertex") );

    ogl_shader_set_body(debug_fshader, system_hashed_ansi_string_create(_particle_debug_fragment_shader_body) );
    ogl_shader_set_body(debug_vshader, system_hashed_ansi_string_create(_particle_debug_vertex_shader_body) );

    ogl_program_attach_shader(_stage_step_particles_debug_program, debug_fshader);
    ogl_program_attach_shader(_stage_step_particles_debug_program, debug_vshader);

    ogl_shader_compile(debug_fshader);
    ogl_shader_compile(debug_vshader);
    ogl_program_link  (_stage_step_particles_debug_program);

    debug_program_gl_id = ogl_program_get_id(_stage_step_particles_debug_program);

    ogl_program_get_uniform_by_name(_stage_step_particles_debug_program, system_hashed_ansi_string_create("collision_data"),    &debug_collision_data_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_debug_program, system_hashed_ansi_string_create("data"),              &debug_data_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_debug_program, system_hashed_ansi_string_create("mvp"),               &debug_mvp_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_debug_program, system_hashed_ansi_string_create("n_total_particles"), &debug_n_total_particles_ptr);

    _stage_step_particles_debug_program_collision_data_location    = (debug_collision_data_ptr    != NULL) ? debug_collision_data_ptr->location    : -1;
    _stage_step_particles_debug_program_data_location              = (debug_data_ptr              != NULL) ? debug_data_ptr->location              : -1;
    _stage_step_particles_debug_program_mvp_location               = (debug_mvp_ptr               != NULL) ? debug_mvp_ptr->location               : -1;
    _stage_step_particles_debug_program_n_total_particles_location = (debug_n_total_particles_ptr != NULL) ? debug_n_total_particles_ptr->location : -1;

    entrypoints->pGLProgramUniform1i(debug_program_gl_id, _stage_step_particles_debug_program_data_location,              0); /* GL_TEXTURE0 */
    entrypoints->pGLProgramUniform1i(debug_program_gl_id, _stage_step_particles_debug_program_collision_data_location,    1); /* GL_TEXTURE1 */
    entrypoints->pGLProgramUniform1i(debug_program_gl_id, _stage_step_particles_debug_program_n_total_particles_location, N_PARTICLES_ALIGNED_4);

    ogl_shader_release(debug_fshader);
    ogl_shader_release(debug_vshader);

    /******************************************* PARTICLE DRAW *****************************************************************/
    const ogl_program_uniform_descriptor* draw_data_particles_ptr    = NULL;
    const ogl_program_uniform_descriptor* draw_n_total_particles_ptr = NULL;
    const ogl_program_uniform_descriptor* draw_projection_view_ptr   = NULL;
    GLuint                                draw_program_gl_id         = 0;
    ogl_shader                            draw_fshader               = NULL;
    ogl_shader                            draw_vshader               = NULL;

    draw_fshader                       = ogl_shader_create (context, SHADER_TYPE_FRAGMENT, system_hashed_ansi_string_create("particle draw fragment") );
    _stage_step_particles_draw_program = ogl_program_create(context,                       system_hashed_ansi_string_create("particle draw") );
    draw_vshader                       = ogl_shader_create (context, SHADER_TYPE_VERTEX,   system_hashed_ansi_string_create("particle draw vertex") );

    ogl_shader_set_body(draw_fshader, system_hashed_ansi_string_create(_particle_draw_fragment_shader_body) );
    ogl_shader_set_body(draw_vshader, system_hashed_ansi_string_create(_particle_draw_vertex_shader_body) );

    ogl_program_attach_shader(_stage_step_particles_draw_program, draw_fshader);
    ogl_program_attach_shader(_stage_step_particles_draw_program, draw_vshader);

    ogl_shader_compile(draw_fshader);
    ogl_shader_compile(draw_vshader);
    ogl_program_link  (_stage_step_particles_draw_program);

    draw_program_gl_id = ogl_program_get_id(_stage_step_particles_draw_program);

    ogl_program_get_uniform_by_name(_stage_step_particles_draw_program, system_hashed_ansi_string_create("data"),              &draw_data_particles_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_draw_program, system_hashed_ansi_string_create("n_total_particles"), &draw_n_total_particles_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_draw_program, system_hashed_ansi_string_create("projection_view"),   &draw_projection_view_ptr);

    _stage_step_particles_draw_program_data_location              = (draw_data_particles_ptr    != NULL) ? draw_data_particles_ptr->location    : -1;
    _stage_step_particles_draw_program_n_total_particles_location = (draw_n_total_particles_ptr != NULL) ? draw_n_total_particles_ptr->location : -1;
    _stage_step_particles_draw_program_projection_view_location   = (draw_projection_view_ptr   != NULL) ? draw_projection_view_ptr->location   : -1;

    entrypoints->pGLProgramUniform1i(draw_program_gl_id, _stage_step_particles_draw_program_data_location,              0); /* GL_TEXTURE0 */
    entrypoints->pGLProgramUniform1i(draw_program_gl_id, _stage_step_particles_draw_program_n_total_particles_location, N_PARTICLES_ALIGNED_4);

    ogl_shader_release(draw_fshader);
    ogl_shader_release(draw_vshader);

    /**************************************** PARTICLE GENERATE 1 **************************************************************/
    const ogl_program_uniform_descriptor* generate1_max_mass_delta_ptr     = NULL;
    const ogl_program_uniform_descriptor* generate1_min_mass_ptr           = NULL;
    const ogl_program_uniform_descriptor* generate1_n_total_particles_ptr  = NULL;
    const ogl_program_uniform_descriptor* generate1_spread_ptr             = NULL;
    ogl_shader                            particle_generate1_vshader       = NULL;
    GLuint                                particle_generate1_program_gl_id = 0;

    const char*    generate1_tf_variables[] = {"out_mass", "out_velocity", "out_force", "out_color"};
    const uint32_t n_generate1_tf_variables = sizeof(generate1_tf_variables) / sizeof(generate1_tf_variables[0]);
    
    _stage_step_particles_generate1_program = ogl_program_create(context,                     system_hashed_ansi_string_create("particle generate") );
    particle_generate1_vshader              = ogl_shader_create (context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create("particle generate") );
    particle_generate1_program_gl_id        = ogl_program_get_id(_stage_step_particles_generate1_program);

    ogl_shader_set_body(particle_generate1_vshader, system_hashed_ansi_string_create(_particle_generate1_vertex_shader_body) );
    ogl_shader_compile (particle_generate1_vshader);

    entrypoints->pGLTransformFeedbackVaryings(particle_generate1_program_gl_id, n_generate1_tf_variables, generate1_tf_variables, GL_SEPARATE_ATTRIBS);

    ogl_program_attach_shader(_stage_step_particles_generate1_program, particle_generate1_vshader);
    ogl_program_link         (_stage_step_particles_generate1_program);

    ogl_program_get_uniform_by_name(_stage_step_particles_generate1_program, system_hashed_ansi_string_create("max_mass_delta"),    &generate1_max_mass_delta_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_generate1_program, system_hashed_ansi_string_create("min_mass"),          &generate1_min_mass_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_generate1_program, system_hashed_ansi_string_create("n_total_particles"), &generate1_n_total_particles_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_generate1_program, system_hashed_ansi_string_create("spread"),            &generate1_spread_ptr);

    _stage_step_particles_generate1_program_max_mass_delta_location    = generate1_max_mass_delta_ptr->location;
    _stage_step_particles_generate1_program_min_mass_location          = generate1_min_mass_ptr->location;
    _stage_step_particles_generate1_program_n_total_particles_location = generate1_n_total_particles_ptr->location;
    _stage_step_particles_generate1_program_spread_location            = (generate1_spread_ptr != NULL ? generate1_spread_ptr->location : -1);

    entrypoints->pGLProgramUniform1i(particle_generate1_program_gl_id, _stage_step_particles_generate1_program_n_total_particles_location, N_PARTICLES_ALIGNED_4);

    ogl_shader_release(particle_generate1_vshader);

    /* Set up TFO */
    entrypoints->pGLGenTransformFeedbacks(1,                           &_stage_step_particles_generate1_tfo_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,        _stage_step_particles_generate1_tfo_id);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_GENERATE1_OUT_MASS_TF_INDEX,     _stage_step_particles_data_bo_id, 0,                                          N_PARTICLES_ALIGNED_4 * sizeof(float) );
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_GENERATE1_OUT_VELOCITY_TF_INDEX, _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float) * 5,  N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_GENERATE1_OUT_FORCE_TF_INDEX,    _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float) * 9,  N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_GENERATE1_OUT_COLOR_TF_INDEX,    _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float) * 13, N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);

    /**************************************** PARTICLE GENERATE 2 **************************************************************/
    const char*    generate2_tf_variables[]          = {"out_position"};
    const uint32_t n_generate2_tf_variables          = sizeof(generate2_tf_variables) / sizeof(generate2_tf_variables[0]);
    GLuint         particle_generate2_program_gl_id  = 0;
    ogl_shader     particle_generate2_vshader        = NULL;

    _stage_step_particles_generate2_program = ogl_program_create(context,                     system_hashed_ansi_string_create("particle generate 2") );
    particle_generate2_vshader              = ogl_shader_create (context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create("particle generate 2") );
    particle_generate2_program_gl_id        = ogl_program_get_id(_stage_step_particles_generate2_program);

    ogl_shader_set_body(particle_generate2_vshader, system_hashed_ansi_string_create(_particle_generate2_vertex_shader_body) );
    ogl_shader_compile (particle_generate2_vshader);

    entrypoints->pGLTransformFeedbackVaryings(particle_generate2_program_gl_id, n_generate2_tf_variables, generate2_tf_variables, GL_SEPARATE_ATTRIBS);

    ogl_program_attach_shader(_stage_step_particles_generate2_program, particle_generate2_vshader);
    ogl_program_link         (_stage_step_particles_generate2_program);

    /* Set up TFO */
    entrypoints->pGLGenTransformFeedbacks(1,                           &_stage_step_particles_generate2_tfo_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,        _stage_step_particles_generate2_tfo_id);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_GENERATE2_OUT_POSITION_TF_INDEX, _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float),  N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);

    ogl_shader_release(particle_generate2_vshader);

    /************************************************ PARTICLE COLLISION DETECTION *****************************/
    entrypoints->pGLGenBuffers            (1, &_stage_step_particles_collide_collision_data_bo);

    _stage_step_particles_collide_collision_data_to = ogl_texture_create(context, system_hashed_ansi_string_create("Collision data TBO") );

    dsa_entrypoints->pGLNamedBufferDataEXT(_stage_step_particles_collide_collision_data_bo, 
                                           N_PARTICLES_ALIGNED_4 * 4 * sizeof(float),
                                           NULL,
                                           GL_DYNAMIC_COPY);
    dsa_entrypoints->pGLTextureBufferEXT  (_stage_step_particles_collide_collision_data_to,
                                           GL_TEXTURE_BUFFER,
                                           GL_RGBA32F,
                                           _stage_step_particles_collide_collision_data_bo);

    _stage_step_particles_collide_collision_data_mo = cl_gl_entrypoints->pCLCreateFromGLBuffer(cl_context,
                                                                                               CL_MEM_WRITE_ONLY,
                                                                                               _stage_step_particles_collide_collision_data_bo,
                                                                                               &cl_error_code);
    ASSERT_DEBUG_SYNC(_stage_step_particles_collide_collision_data_mo != NULL && cl_error_code == CL_SUCCESS,
                      "Could not generate memory object to hold collision data");

    /* Create objects */
    ogl_shader particle_update_vshader = NULL;

    _stage_step_particles_update_program = ogl_program_create(context,                     system_hashed_ansi_string_create("particle update") );
    particle_update_vshader              = ogl_shader_create (context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create("particle update") );

    /* Set up particle update vshader object */
    ogl_shader_set_body(particle_update_vshader, system_hashed_ansi_string_create(_particle_update_vertex_shader_body) );
    ogl_shader_compile (particle_update_vshader);

    /* Set up particle update program */
    const ogl_program_uniform_descriptor* barycentric_data_ptr             = NULL;
    const ogl_program_uniform_descriptor* data_ptr                         = NULL;
    const ogl_program_uniform_descriptor* decay_ptr                        = NULL;
    const ogl_program_uniform_descriptor* dt_ptr                           = NULL;
    const ogl_program_uniform_descriptor* gravity_ptr                      = NULL;
    const ogl_program_uniform_descriptor* is_prev_pass_data_available_ptr  = NULL;
    const ogl_program_uniform_descriptor* mesh_vertex_data_ptr             = NULL;
    const ogl_program_uniform_descriptor* n_total_particles_ptr            = NULL;
    const ogl_program_uniform_descriptor* prev_pass_collide_stage_data_ptr = NULL;
    const char*                           update_tf_variables[]            = {"out_position", "out_velocity", "out_force", "out_collision_location"};
    const uint32_t                        n_update_tf_variables            = sizeof(update_tf_variables) / sizeof(update_tf_variables[0]);
    GLuint                                update_program_gl_id             = ogl_program_get_id(_stage_step_particles_update_program);

    entrypoints->pGLTransformFeedbackVaryings(update_program_gl_id, n_update_tf_variables, update_tf_variables, GL_SEPARATE_ATTRIBS);

    ogl_program_attach_shader(_stage_step_particles_update_program, particle_update_vshader);
    ogl_program_link         (_stage_step_particles_update_program);

    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("collide_stage_barycentric_data"),  &barycentric_data_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("prev_pass_data"),                  &data_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("decay"),                           &decay_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("dt"),                              &dt_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("gravity"),                         &gravity_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("is_prev_pass_data_available"),     &is_prev_pass_data_available_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("mesh_vertex_data"),                &mesh_vertex_data_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("prev_pass_collide_stage_data"),    &prev_pass_collide_stage_data_ptr);
    ogl_program_get_uniform_by_name(_stage_step_particles_update_program, system_hashed_ansi_string_create("n_total_particles"),               &n_total_particles_ptr);

    _stage_step_particles_update_program_barycentric_data_uniform_location             = (barycentric_data_ptr             != NULL ? barycentric_data_ptr->location             : -1);
    _stage_step_particles_update_program_decay_uniform_location                        = (decay_ptr                        != NULL ? decay_ptr->location                        : -1);
    _stage_step_particles_update_program_dt_uniform_location                           = (dt_ptr                           != NULL ? dt_ptr->location                           : -1);
    _stage_step_particles_update_program_is_prev_pass_data_available_uniform_location  = (is_prev_pass_data_available_ptr  != NULL ? is_prev_pass_data_available_ptr->location  : -1);
    _stage_step_particles_update_program_gravity_uniform_location                      = (gravity_ptr                      != NULL ? gravity_ptr->location                      : -1);
    _stage_step_particles_update_program_mesh_vertex_data_uniform_location             = (mesh_vertex_data_ptr             != NULL ? mesh_vertex_data_ptr->location             : -1);
    _stage_step_particles_update_program_n_total_particles_location                    = (n_total_particles_ptr            != NULL ? n_total_particles_ptr->location            : -1);
    _stage_step_particles_update_program_prev_pass_collide_stage_data_uniform_location = (prev_pass_collide_stage_data_ptr != NULL ? prev_pass_collide_stage_data_ptr->location : -1);

    if (data_ptr                         != NULL) entrypoints->pGLProgramUniform1i(update_program_gl_id, data_ptr->location,                         0); /* GL_TEXTURE0 */
    if (barycentric_data_ptr             != NULL) entrypoints->pGLProgramUniform1i(update_program_gl_id, barycentric_data_ptr->location,             1); /* GL_TEXTURE1 */
    if (mesh_vertex_data_ptr             != NULL) entrypoints->pGLProgramUniform1i(update_program_gl_id, mesh_vertex_data_ptr->location,             2); /* GL_TEXTURE2 */
    if (prev_pass_collide_stage_data_ptr != NULL) entrypoints->pGLProgramUniform1i(update_program_gl_id, prev_pass_collide_stage_data_ptr->location, 4); /* GL_TEXTURE4 */

    entrypoints->pGLProgramUniform1i(update_program_gl_id, _stage_step_particles_update_program_n_total_particles_location, N_PARTICLES_ALIGNED_4);

    ogl_shader_release(particle_update_vshader);

    /* Set up TFO */
    entrypoints->pGLGenTransformFeedbacks(1,                            &_stage_step_particles_update_tfo_id);
    entrypoints->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,         _stage_step_particles_update_tfo_id);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_UPDATE_OUT_POSITION_TF_INDEX,           _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float),     N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_UPDATE_OUT_VELOCITY_TF_INDEX,           _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float) * 5, N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);
    entrypoints->pGLBindBufferRange      (GL_TRANSFORM_FEEDBACK_BUFFER, N_UPDATE_OUT_FORCE_TF_INDEX,              _stage_step_particles_data_bo_id, N_PARTICLES_ALIGNED_4 * sizeof(float) * 9, N_PARTICLES_ALIGNED_4 * sizeof(float) * 4);

    /* Add the step */
    _stage_step_particles_generate_step_id = ogl_pipeline_add_stage_step(pipeline, stage_id, system_hashed_ansi_string_create("Particle generation"), _stage_step_particles_generate,  NULL);
    _stage_step_particles_update_step_id   = ogl_pipeline_add_stage_step(pipeline, stage_id, system_hashed_ansi_string_create("Particle update"),     _stage_step_particles_update,    NULL);
    _stage_step_particles_collide_step_id  = ogl_pipeline_add_stage_step(pipeline, stage_id, system_hashed_ansi_string_create("Particle collisions"), _stage_step_particles_collide,   NULL);
    _stage_step_particles_draw_step_id     = ogl_pipeline_add_stage_step(pipeline, stage_id, system_hashed_ansi_string_create("Particle draw"),       _stage_step_particles_draw,      NULL);
    _stage_step_particles_debug_step_id    = ogl_pipeline_add_stage_step(pipeline, stage_id, system_hashed_ansi_string_create("Particle debug"),      _stage_step_particles_debug,     NULL);
}

/** Please see header for specification */
PUBLIC void stage_step_particles_iterate_frame()
{
    _stage_step_particles_iterate_frame = true;
}

/** Please see header for specification */
PUBLIC void stage_step_particles_reset()
{
    _stage_step_particles_data_generated = false;
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_debug(bool new_debug_status)
{
    _stage_step_particles_iterate_status = new_debug_status;
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_decay(__in float new_decay)
{
    _stage_step_particles_decay = new_decay;
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_dt(__in float new_dt)
{
    _stage_step_particles_dt = new_dt;
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_gravity(__in float new_gravity)
{
    _stage_step_particles_gravity = new_gravity;
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_maximum_mass_delta(__in float new_max_mass_delta)
{
    _stage_step_particles_max_mass_delta = new_max_mass_delta;

    stage_step_particles_reset();
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_minimum_mass(__in float new_min_mass)
{
    _stage_step_particles_min_mass = new_min_mass;

    stage_step_particles_reset();
}

/** Please see header for specification */
PUBLIC void stage_step_particles_set_spread(__in float new_spread)
{
    _stage_step_particles_spread = new_spread;

    stage_step_particles_reset();
}