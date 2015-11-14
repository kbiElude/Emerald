/**
 *
 * DOF test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_background.h"
#include "stage_step_julia.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "procedural/procedural_mesh_sphere.h"
#include "raGL/raGL_buffer.h"
#include "system/system_matrix4x4.h"

GLuint                 _julia_data_ub_offset                      = -1;
GLuint                 _julia_dof_cutoff_ub_offset                = -1;
GLuint                 _julia_dof_far_plane_depth_ub_offset       = -1;
GLuint                 _julia_dof_focal_plane_depth_ub_offset     = -1;
GLuint                 _julia_dof_near_plane_depth_ub_offset      = -1;
GLuint                 _julia_epsilon_ub_offset                   = -1;
GLuint                 _julia_escape_ub_offset                    = -1;
GLuint                 _julia_eye_ub_offset                       = -1;
GLuint                 _julia_fbo_id                              = 0;
ogl_texture            _julia_fbo_color_to                        = 0;
ogl_texture            _julia_fbo_depth_to                        = 0;
GLuint                 _julia_fresnel_reflectance_ub_offset       = -1;
GLuint                 _julia_light_color_ub_offset               = -1;
GLuint                 _julia_light_position_ub_offset            = -1;
GLuint                 _julia_max_iterations_ub_offset            = -1;
GLuint                 _julia_mv_ub_offset                        = -1;
GLuint                 _julia_mvp_ub_offset                       = -1;
ogl_program            _julia_program                             = NULL;
ogl_program_ub         _julia_program_ub                          = NULL;
raGL_buffer            _julia_program_ub_bo                       = NULL;
GLuint                 _julia_program_ub_bo_size                  = 0;
GLuint                 _julia_raycast_radius_multiplier_ub_offset = -1;
GLuint                 _julia_reflectivity_ub_offset              = -1;
GLuint                 _julia_sph_texture_uniform_location        = -1;
GLuint                 _julia_shadows_ub_offset                   = -1;
GLuint                 _julia_specularity_ub_offset               = -1;
procedural_mesh_sphere _julia_sphere                              = NULL;
GLuint                 _julia_vao_id                              = -1;
GLuint                 _julia_vertex_attribute_location           = -1;
system_matrix4x4       _julia_view_matrix                         = NULL;

/* Shaders.
 *
 * Built upon https://developer.apple.com/library/mac/samplecode/OpenCL_RayTraced_Quaternion_Julia-Set_Example/Listings/qjulia_kernel_cl.html#//apple_ref/doc/uid/DTS40008190-qjulia_kernel_cl-DontLinkElementID_5 
 *
 * Stores depth in alpha channel for Scheuermann DOF
 **/
const char* julia_fragment_shader_code = "#version 430 core\n"
                                         "\n"
                                         "in vec3 fp_vertex;\n"
                                         "\n"
                                         "uniform dataUB\n"
                                         "{\n"
                                         "    vec4  data;\n"
                                         "    float dof_cutoff;\n"
                                         "    float dof_far_plane_depth;\n"
                                         "    float dof_focal_plane_depth;\n"
                                         "    float dof_near_plane_depth;\n"
                                         "    float epsilon;\n"
                                         "    float escape;\n"
                                         "    vec3  eye;\n"
                                         "    float fresnel_reflectance;\n"
                                         "    vec3  light_color;\n"
                                         "    vec3  light_position;\n"
                                         "    int   max_iterations;\n"
                                         "    mat4  mvp;\n"
                                         "    mat4  mv;\n"
                                         "    float raycast_radius_multiplier;\n"
                                         "    float reflectivity;\n"
                                         "    bool  shadows;\n"
                                         "    float specularity;\n"
                                         "};\n"
                                         "\n"
                                         "out     vec4      result;\n"
                                         "uniform sampler2D sph_texture;\n" /* texture unit 0 */
                                         "\n"
                                         "\n"
                                         "float scheuermann_get_blurriness(float depth)\n"
                                         "{\n"
                                         "    float tmp = 0.0f;\n"
                                         "\n"
                                         "    if (depth < dof_focal_plane_depth)\n"
                                         "    {\n"
                                         "       tmp = (depth - dof_focal_plane_depth) / (dof_focal_plane_depth - dof_near_plane_depth);\n"
                                         "       tmp = clamp(tmp, -1, 0);\n"
                                         "    }\n"
                                         "    else\n"
                                         "    {\n"
                                         "       tmp = (depth - dof_focal_plane_depth) / (dof_far_plane_depth - dof_focal_plane_depth);\n"
                                         "       tmp = clamp(tmp, 0, dof_cutoff);\n"
                                         "    }\n"
                                         "\n"
                                         "    return tmp * 0.5 + 0.5;\n"
                                         "}\n"
                                         "\n"
                                         "vec4 quaternion_mul(in vec4 q1, in vec4 q2)\n"
                                         "{\n"
                                         "    vec4 r;\n"
                                         "    vec3 t;\n"
                                         "\n"
                                         "    vec3 q1yzw = vec3(q1.y, q1.z, q1.w);\n"
                                         "    vec3 q2yzw = vec3(q2.y, q2.z, q2.w);\n"
                                         "    vec3 c     = cross(q1yzw, q2yzw);\n"
                                         "\n"
                                         "    t     = q2yzw * q1.x + q1yzw * q2.x + c;\n"
                                         "    r.x   = q1.x * q2.x - dot( q1yzw, q2yzw );\n"
                                         "    r.yzw = t.xyz;\n"
                                         "\n"
                                         "    return r;\n"
                                         "}\n"
                                         "\n"
                                         "vec4 quaternion_sqrt(in vec4 q)\n"
                                         "{\n"
                                         "    vec4 r;\n"
                                         "    vec3 t;\n"
                                         "\n"
                                         "    vec3 qyzw = vec3(q.y, q.z, q.w);\n"
                                         "\n"
                                         "    t     = 2.0f * q.x * qyzw;\n"
                                         "    r.x   = q.x * q.x - dot(qyzw, qyzw);\n"
                                         "    r.yzw = t.xyz;\n"
                                         "\n"
                                         "    return r;\n"
                                         "}\n"
                                         "\n"
                                         "float intersect_ray_sphere(in vec3 ray_origin, in vec3 ray_direction, float radius)\n"
                                         "{\n"
                                         "    float fB  = 2.0 * dot(ray_origin, ray_direction);\n"
                                         "    float fB2 = fB * fB;\n"
                                         "    float fC  = dot(ray_origin, ray_origin) - radius;\n"
                                         "    float fT  = (fB2 - 4.0 * fC);\n"
                                         "\n"
                                         "    if (fT <= 0.0) return 0.0;\n"
                                         "\n"
                                         "    float fD  = sqrt(fT);\n"
                                         "    float fT0 = (-fB + fD) * 0.5;\n"
                                         "    float fT1 = (-fB - fD) * 0.5;\n"
                                         "\n"
                                         "    fT = min(fT0, fT1);\n"
                                         "\n"
                                         "    return fT;\n"
                                         "}\n"
                                         "\n"
                                         "vec4 intersect_ray_quaternion_julia(in vec3 ray_origin, in vec3 ray_direction, in vec4 coeffs, in float epsilon, in float threshold)\n"
                                         "{\n"
                                         "    int   cnt  = 0;\n"
                                         "    float rd   = 0.0;\n"
                                         "    float dist = epsilon;\n"
                                         "\n"
                                         "    while (dist >= epsilon && rd < threshold)\n"
                                         "    {\n"
                                         "        vec4  z   = vec4(ray_origin, 0);\n"
                                         "        vec4  zp  = vec4(1, 0, 0, 0);\n"
                                         "        float zd  = 0;\n"
                                         "\n"
                                         "        cnt = 0;\n"
                                         "\n"
                                         "        while (zd < threshold && cnt < max_iterations)\n"
                                         "        {\n"
                                         "            zp = 2.0 * quaternion_mul(z, zp);\n"
                                         "            z  = quaternion_sqrt(z) + data;\n"
                                         "            zd = dot(z, z);\n"
                                         "\n"
                                         "            cnt++;\n"
                                         "        }\n"
                                         "\n"
                                         "        float z_length = length(z);\n"
                                         "\n"
                                         "        dist        = 0.5 * z_length * log(z_length) / length(zp);\n"
                                         "        ray_origin += ray_direction * dist;\n"
                                         "        rd          = dot(ray_origin, ray_origin);\n"
                                         "    }\n"
                                         "\n"
                                         "    return vec4(ray_origin.xyz, dist);\n"
                                         "}\n"
                                         "\n"
                                         "vec3 normal_quaternion_julia(in vec3 p, in vec4 c)\n"
                                         "{\n"
                                         "    vec4 qp  =      vec4(p.x,     p.y,     p.z,     0.0);\n"
                                         "    vec4 gx1 = qp - vec4(epsilon, 0.0,     0.0,     0.0);\n"
                                         "    vec4 gx2 = qp + vec4(epsilon, 0.0,     0.0,     0.0);\n"
                                         "    vec4 gy1 = qp - vec4(0.0,     epsilon, 0.0,     0.0);\n"
                                         "    vec4 gy2 = qp + vec4(0.0,     epsilon, 0.0,     0.0);\n"
                                         "    vec4 gz1 = qp - vec4(0.0,     0.0,     epsilon, 0.0);\n"
                                         "    vec4 gz2 = qp + vec4(0.0,     0.0,     epsilon, 0.0);\n"
                                         " \n"
                                         "    for (int i = 0; i < max_iterations; i++)\n"
                                         "    {\n"
                                         "        gx1 = quaternion_sqrt(gx1) + c;\n"
                                         "        gx2 = quaternion_sqrt(gx2) + c;\n"
                                         "        gy1 = quaternion_sqrt(gy1) + c;\n"
                                         "        gy2 = quaternion_sqrt(gy2) + c;\n"
                                         "        gz1 = quaternion_sqrt(gz1) + c;\n"
                                         "        gz2 = quaternion_sqrt(gz2) + c;\n"
                                         "    }\n"
                                         "\n"
                                         "    float nx = length(gx2) - length(gx1);\n"
                                         "    float ny = length(gy2) - length(gy1);\n"
                                         "    float nz = length(gz2) - length(gz1);\n"
                                         "\n"
                                         "    return normalize(vec3(nx, ny, nz));\n"
                                         "}\n"
                                         "\n"
                                         "vec3 shade_phong(in vec3 ray_direction, in vec3 ray_origin, in vec3 normal)\n"
                                         "{\n"
                                         "    vec3  light_direction     = normalize(light_position - ray_origin);\n"
                                         "    vec3  eye_direction       = normalize(eye            - ray_origin);\n"
                                         "    float n_dot_l             = dot(normal, light_direction);\n"
                                         "    vec3  reflected_direction = light_direction - 2.0 * n_dot_l * normal;\n"
                                         "    vec3  half_vector         = normalize(light_direction + ray_direction);\n"
                                         "\n"
                                         /* Fresnel (from https://developer.nvidia.com/content/gpu-gems-3-chapter-14-advanced-techniques-realistic-real-time-skin-rendering )*/
                                         "    float base        = 1.0 - dot(ray_direction, half_vector);\n"
                                         "    float exponential = pow(base, 5.0);\n"
                                         "    float fresnel     = exponential + fresnel_reflectance * (1.0 - exponential);\n"
                                         "\n"
                                         /* Specular etc */
                                         "    vec3 diffuse  = light_color * max(n_dot_l, 0);\n"
                                         "    vec3 specular = fresnel * vec3(pow(max(dot(eye_direction, reflected_direction), 0.0), specularity) );\n"
                                         "\n"
                                         "    return diffuse + specular;\n"
                                         "}\n"
                                         "\n"
                                         "void main()\n"
                                         "{\n"
                                         "    vec3 bounding_sphere_vertex = fp_vertex;\n"
                                         "    vec3 ray_direction          = normalize(bounding_sphere_vertex - eye);\n"
                                         "\n"
                                         "    float eye_sphere_intersection = intersect_ray_sphere(eye, ray_direction, raycast_radius_multiplier);\n"
                                         "\n"
                                         "    if (eye_sphere_intersection > 0.0)\n"
                                         "    {\n"
                                         "        vec3 ray_origin               = eye + eye_sphere_intersection * ray_direction;\n"
                                         "        vec4 eye_fractal_intersection = intersect_ray_quaternion_julia(ray_origin, ray_direction, data, epsilon, escape * raycast_radius_multiplier);\n"
                                         "\n"
                                         "        if (eye_fractal_intersection.w < epsilon)\n"
                                         "        {\n"
                                         "            vec3 normal  = normal_quaternion_julia(eye_fractal_intersection.xyz, data);\n"
                                         "            vec3 diffuse = shade_phong(ray_direction, ray_origin, normal);\n"
                                         "\n"
                                         /* Environment mapping */
                                         "            vec3 reflected_vector = reflect(-ray_direction, normal);\n"
                                         "\n"
                                         "            diffuse = mix(diffuse, texture(sph_texture, asin(reflected_vector.xy) / vec2(3.14152965) + vec2(.5)).xyz, vec3(reflectivity));\n"
                                         "\n"
                                         /* Carry on.. */
                                         "            vec4 eyespace_pos = -mv * vec4(eye_fractal_intersection.xyz, 1);\n"
                                         "            result            = vec4(diffuse, scheuermann_get_blurriness(-eyespace_pos.z / eyespace_pos.w) );\n"
                                         /* Shadows */
                                         "            if (shadows)\n"
                                         "            {\n"
                                         "                vec3 light_direction = normalize(light_position - eye_fractal_intersection.xyz);\n"
                                         "\n"
                                         "                eye_fractal_intersection = intersect_ray_quaternion_julia(eye_fractal_intersection.xyz + normal * epsilon * 2.0, light_direction, data, epsilon, escape * raycast_radius_multiplier);\n"
                                         "\n"
                                         "                if (eye_fractal_intersection.w < epsilon) result.xyz *= 0.4;\n"
                                         "            }\n"
                                         "        }\n"
                                         "        else\n"
                                         "            discard;\n"
                                         "    }\n"
                                         "    else\n"
                                         "        discard;\n"
                                         "}\n";

const char* julia_vertex_shader_code = "#version 430 core\n"
                                       "\n"
                                       "in      vec3  vertex;\n"
                                       "out     vec3  fp_vertex;\n"
                                       "\n"
                                       "uniform dataUB\n"
                                       "{\n"
                                       "    vec4  data;\n"
                                       "    float dof_cutoff;\n"
                                       "    float dof_far_plane_depth;\n"
                                       "    float dof_focal_plane_depth;\n"
                                       "    float dof_near_plane_depth;\n"
                                       "    float epsilon;\n"
                                       "    float escape;\n"
                                       "    vec3  eye;\n"
                                       "    float fresnel_reflectance;\n"
                                       "    vec3  light_color;\n"
                                       "    vec3  light_position;\n"
                                       "    int   max_iterations;\n"
                                       "    mat4  mvp;\n"
                                       "    mat4  mv;\n"
                                       "    float raycast_radius_multiplier;\n"
                                       "    float reflectivity;\n"
                                       "    bool  shadows;\n"
                                       "    float specularity;\n"
                                       "};\n"
                                       "\n"
                                       "void main()\n"
                                       "{\n"
                                       "    fp_vertex   = raycast_radius_multiplier * vertex.xyz;\n"
                                       "    gl_Position = mvp * vec4(fp_vertex, 1.0);\n"
                                       "}\n";

/** TODO */
static void _stage_step_julia_execute(ogl_context context,
                                      uint32_t    frame_index,
                                      system_time time,
                                      const int*  rendering_area_px_topdown,
                                      void*       not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLUseProgram     (ogl_program_get_id(_julia_program) );
    entrypoints->pGLBindVertexArray(_julia_vao_id);

    /* Calculate MVP matrix */
          float             camera_location[3];
    const float*            data                      = main_get_data_vector();
    const float             dof_cutoff                = main_get_dof_cutoff();
    const float             dof_far_plane_depth       = main_get_dof_far_plane_depth();
    const float             dof_focal_plane_depth     = main_get_dof_focal_plane_depth();
    const float             dof_near_plane_depth      = main_get_dof_near_plane_depth();
    const float             epsilon                   = main_get_epsilon();
    const float             escape                    = main_get_escape_threshold();
          ogl_flyby         flyby                     = NULL;
    const float             fresnel_reflectance       = main_get_fresnel_reflectance();
    const float             reflectivity              = main_get_reflectivity();
    const float*            light_color               = main_get_light_color();
    const float*            light_position            = main_get_light_position();
    const int               max_iterations            = main_get_max_iterations();
    const float             raycast_radius_multiplier = main_get_raycast_radius_multiplier();
    const bool              shadows                   = main_get_shadows_status();
    const float             specularity               = main_get_specularity();
           system_matrix4x4 mvp                       = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_FLYBY,
                            &flyby);
    ogl_flyby_get_property  (flyby,
                             OGL_FLYBY_PROPERTY_CAMERA_LOCATION,
                             camera_location);
    ogl_flyby_get_property  (flyby,
                             OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                            &_julia_view_matrix);

    mvp = system_matrix4x4_create_by_mul(main_get_projection_matrix(),
                                         _julia_view_matrix);

    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_data_ub_offset,
                                                data,
                                                0, /* src_data_flags */
                                                sizeof(float) * 4);
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_dof_cutoff_ub_offset,
                                               &dof_cutoff,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_dof_far_plane_depth_ub_offset,
                                               &dof_far_plane_depth,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_dof_focal_plane_depth_ub_offset,
                                               &dof_focal_plane_depth,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_dof_near_plane_depth_ub_offset,
                                               &dof_near_plane_depth,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_epsilon_ub_offset,
                                               &epsilon,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_escape_ub_offset,
                                               &escape,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_fresnel_reflectance_ub_offset,
                                               &fresnel_reflectance,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_reflectivity_ub_offset,
                                               &reflectivity,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_light_position_ub_offset,
                                                light_position,
                                                0, /* src_data_flags */
                                                sizeof(float) * 3);
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_light_color_ub_offset,
                                                light_color,
                                                0, /* src_data_flags */
                                                sizeof(float) * 3);
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_max_iterations_ub_offset,
                                               &max_iterations,
                                                0, /* src_data_flags */
                                                sizeof(int) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_raycast_radius_multiplier_ub_offset,
                                               &raycast_radius_multiplier,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_shadows_ub_offset,
                                               &shadows,
                                                0, /* src_data_flags */
                                                sizeof(bool) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_specularity_ub_offset,
                                               &specularity,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_eye_ub_offset,
                                                camera_location,
                                                0, /* src_data_flags */
                                                sizeof(float) * 3);
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_mv_ub_offset,
                                                system_matrix4x4_get_column_major_data(_julia_view_matrix),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_set_nonarrayed_uniform_value(_julia_program_ub,
                                                _julia_mvp_ub_offset,
                                                system_matrix4x4_get_column_major_data(mvp),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);

    ogl_program_ub_sync(_julia_program_ub);

    system_matrix4x4_release(mvp);

    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                            GL_TEXTURE_2D,
                                            stage_step_background_get_background_texture() );

    /* Draw the fractal */
    GLuint   julia_program_ub_bo_id           = 0;
    uint32_t julia_program_ub_bo_start_offset = -1;

    raGL_buffer_get_property(_julia_program_ub_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &julia_program_ub_bo_id);
    raGL_buffer_get_property(_julia_program_ub_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &julia_program_ub_bo_start_offset);

    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    0, /* index */
                                     julia_program_ub_bo_id,
                                     julia_program_ub_bo_start_offset,
                                    _julia_program_ub_bo_size);

    entrypoints->pGLClearColor     (0.0f,  /* red */
                                    0.0f,  /* green */
                                    0.0f,  /* blue */
                                    0.0f); /* alpha */
    entrypoints->pGLClearDepth     (1.0f);
    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    _julia_fbo_id);
    entrypoints->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entrypoints->pGLEnable         (GL_DEPTH_TEST);
    entrypoints->pGLDepthFunc      (GL_LESS);
    entrypoints->pGLEnable         (GL_CULL_FACE);
    entrypoints->pGLFrontFace      (GL_CW);
    {
        unsigned int n_triangles = 0;

        procedural_mesh_sphere_get_property(_julia_sphere,
                                            PROCEDURAL_MESH_SPHERE_PROPERTY_N_TRIANGLES,
                                           &n_triangles);

        entrypoints->pGLDrawArrays(GL_TRIANGLES,
                                   0, /* first */
                                   n_triangles * 3);
    }
    entrypoints->pGLDisable        (GL_CULL_FACE);
    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    0);
    entrypoints->pGLBindVertexArray(0);
}

/* Please see header for specification */
PUBLIC void stage_step_julia_deinit(ogl_context context)
{
    const ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLDeleteFramebuffers(1, /* n */
                                      &_julia_fbo_id);

    ogl_texture_release(_julia_fbo_color_to);
    ogl_texture_release(_julia_fbo_depth_to);

    ogl_program_release           (_julia_program);
    procedural_mesh_sphere_release(_julia_sphere);
    system_matrix4x4_release      (_julia_view_matrix);
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_julia_get_color_texture()
{
    return _julia_fbo_color_to;
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_julia_get_depth_texture()
{
    return _julia_fbo_depth_to;
}

/* Please see header for specification */
PUBLIC GLuint stage_step_julia_get_fbo_id()
{
    return _julia_fbo_id;
}

/* Please see header for specification */
PUBLIC void stage_step_julia_init(ogl_context  context,
                                  ogl_pipeline pipeline,
                                  uint32_t     stage_id)
{
    /* Instantiate sphere we will use for raytracing */
    uint32_t normals_data_offset  = 0;
    uint32_t vertex_data_offset   = 0;

    _julia_sphere = procedural_mesh_sphere_create(context,
                                                  DATA_BO_ARRAYS,
                                                  7, /* n_latitude_splices */
                                                  7, /* n_longitude_splices */
                                                  system_hashed_ansi_string_create("julia sphere") );

    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_NORMALS_DATA_OFFSET,
                                       &normals_data_offset);
    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET,
                                       &vertex_data_offset);

    /* Link Julia program */
    ogl_shader fragment_shader = NULL;
    ogl_shader vertex_shader   = NULL;

    _julia_program  = ogl_program_create(context,
                                         system_hashed_ansi_string_create("julia program"),
                                         OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);
    fragment_shader = ogl_shader_create (context,
                                         RAL_SHADER_TYPE_FRAGMENT,
                                         system_hashed_ansi_string_create("julia fragment") );
    vertex_shader   = ogl_shader_create (context,
                                         RAL_SHADER_TYPE_VERTEX,
                                         system_hashed_ansi_string_create("julia vertex") );

    ogl_shader_set_body(fragment_shader,
                        system_hashed_ansi_string_create(julia_fragment_shader_code));
    ogl_shader_set_body(vertex_shader,
                        system_hashed_ansi_string_create(julia_vertex_shader_code) );

    ogl_shader_compile (fragment_shader);
    ogl_shader_compile (vertex_shader);

    ogl_program_attach_shader(_julia_program,
                              fragment_shader);
    ogl_program_attach_shader(_julia_program,
                              vertex_shader);

    ogl_program_link(_julia_program);

    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);

    /* Retrieve data uniform block properties */
    ogl_program_get_uniform_block_by_name(_julia_program,
                                          system_hashed_ansi_string_create("dataUB"),
                                         &_julia_program_ub);

    ogl_program_ub_get_property(_julia_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_julia_program_ub_bo_size);
    ogl_program_ub_get_property(_julia_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &_julia_program_ub_bo);

    /* Retrieve attribute/uniform locations */
    const ogl_program_variable*             data_uniform_data                      = NULL;
    const ogl_program_variable*             dof_cutoff_uniform_data                = NULL;
    const ogl_program_variable*             dof_far_plane_depth_uniform_data       = NULL;
    const ogl_program_variable*             dof_focal_plane_depth_uniform_data     = NULL;
    const ogl_program_variable*             dof_near_plane_depth_uniform_data      = NULL;
    const ogl_program_variable*             epsilon_uniform_data                   = NULL;
    const ogl_program_variable*             escape_uniform_data                    = NULL;
    const ogl_program_variable*             eye_uniform_data                       = NULL;
    const ogl_program_variable*             fresnel_reflectance_uniform_data       = NULL;
    const ogl_program_variable*             light_color_uniform_data               = NULL;
    const ogl_program_variable*             light_position_uniform_data            = NULL;
    const ogl_program_variable*             max_iterations_uniform_data            = NULL;
    const ogl_program_variable*             mv_uniform_data                        = NULL;
    const ogl_program_variable*             mvp_uniform_data                       = NULL;
    const ogl_program_variable*             raycast_radius_multiplier_uniform_data = NULL;
    const ogl_program_variable*             reflectivity_uniform_data              = NULL;
    const ogl_program_variable*             shadows_uniform_data                   = NULL;
    const ogl_program_variable*             specularity_uniform_data               = NULL;
    const ogl_program_variable*             sph_texture_uniform_data               = NULL;
    const ogl_program_attribute_descriptor* vertex_attribute_data                  = NULL;

    ogl_program_get_attribute_by_name(_julia_program,
                                      system_hashed_ansi_string_create("vertex"),
                                     &vertex_attribute_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("data"),
                                     &data_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("dof_cutoff"),
                                     &dof_cutoff_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("dof_far_plane_depth"),
                                     &dof_far_plane_depth_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("dof_focal_plane_depth"),
                                     &dof_focal_plane_depth_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("dof_near_plane_depth"),
                                     &dof_near_plane_depth_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("epsilon"),
                                     &epsilon_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("escape"),
                                     &escape_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("eye"),
                                     &eye_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("fresnel_reflectance"),
                                     &fresnel_reflectance_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("light_color"),
                                     &light_color_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("light_position"),
                                     &light_position_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("max_iterations"),
                                     &max_iterations_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("mv"),
                                     &mv_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("mvp"),
                                     &mvp_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("raycast_radius_multiplier"),
                                     &raycast_radius_multiplier_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("reflectivity"),
                                     &reflectivity_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("shadows"),
                                     &shadows_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("specularity"),
                                     &specularity_uniform_data);
    ogl_program_get_uniform_by_name  (_julia_program,
                                      system_hashed_ansi_string_create("sph_texture"),
                                     &sph_texture_uniform_data);

    _julia_data_ub_offset                      = (data_uniform_data                      != NULL) ? data_uniform_data->block_offset                      : -1;
    _julia_dof_cutoff_ub_offset                = (dof_cutoff_uniform_data                != NULL) ? dof_cutoff_uniform_data->block_offset                : -1;
    _julia_dof_far_plane_depth_ub_offset       = (dof_far_plane_depth_uniform_data       != NULL) ? dof_far_plane_depth_uniform_data->block_offset       : -1;
    _julia_dof_focal_plane_depth_ub_offset     = (dof_focal_plane_depth_uniform_data     != NULL) ? dof_focal_plane_depth_uniform_data->block_offset     : -1;
    _julia_dof_near_plane_depth_ub_offset      = (dof_near_plane_depth_uniform_data      != NULL) ? dof_near_plane_depth_uniform_data->block_offset      : -1;
    _julia_epsilon_ub_offset                   = (epsilon_uniform_data                   != NULL) ? epsilon_uniform_data->block_offset                   : -1;
    _julia_escape_ub_offset                    = (escape_uniform_data                    != NULL) ? escape_uniform_data->block_offset                    : -1;
    _julia_eye_ub_offset                       = (eye_uniform_data                       != NULL) ? eye_uniform_data->block_offset                       : -1;
    _julia_fresnel_reflectance_ub_offset       = (fresnel_reflectance_uniform_data       != NULL) ? fresnel_reflectance_uniform_data->block_offset       : -1;
    _julia_light_color_ub_offset               = (light_color_uniform_data               != NULL) ? light_color_uniform_data->block_offset               : -1;
    _julia_light_position_ub_offset            = (light_position_uniform_data            != NULL) ? light_position_uniform_data->block_offset            : -1;
    _julia_max_iterations_ub_offset            = (max_iterations_uniform_data            != NULL) ? max_iterations_uniform_data->block_offset            : -1;
    _julia_mv_ub_offset                        = (mv_uniform_data                        != NULL) ? mv_uniform_data->block_offset                        : -1;
    _julia_mvp_ub_offset                       = (mvp_uniform_data                       != NULL) ? mvp_uniform_data->block_offset                       : -1;
    _julia_raycast_radius_multiplier_ub_offset = (raycast_radius_multiplier_uniform_data != NULL) ? raycast_radius_multiplier_uniform_data->block_offset : -1;
    _julia_reflectivity_ub_offset              = (reflectivity_uniform_data              != NULL) ? reflectivity_uniform_data->block_offset              : -1;
    _julia_shadows_ub_offset                   = (shadows_uniform_data                   != NULL) ? shadows_uniform_data->block_offset                   : -1;
    _julia_specularity_ub_offset               = (specularity_uniform_data               != NULL) ? specularity_uniform_data->block_offset               : -1;
    _julia_sph_texture_uniform_location        = (sph_texture_uniform_data               != NULL) ? sph_texture_uniform_data->location                   : -1;
    _julia_vertex_attribute_location           = (vertex_attribute_data                  != NULL) ? vertex_attribute_data->location                      : -1;

    /* Generate & set VAO up */
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;
    unsigned int                                              sphere_bo_id    = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_ID,
                                       &sphere_bo_id);

    entrypoints->pGLGenVertexArrays(1, /* n */
                                   &_julia_vao_id);
    entrypoints->pGLBindVertexArray(_julia_vao_id);

    entrypoints->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                            sphere_bo_id);
    entrypoints->pGLVertexAttribPointer    (_julia_vertex_attribute_location,
                                            3, /* size */
                                            GL_FLOAT,
                                            GL_FALSE,
                                            0, /* stride */
                                            (void*) (intptr_t) vertex_data_offset);
    entrypoints->pGLEnableVertexAttribArray(_julia_vertex_attribute_location);

    entrypoints->pGLProgramUniform1i(ogl_program_get_id(_julia_program),
                                     _julia_sph_texture_uniform_location,
                                     0);

    /* Generate & set FBO up */
    entrypoints->pGLGenFramebuffers(1, /* n */
                                   &_julia_fbo_id);

    _julia_fbo_color_to = ogl_texture_create_empty(context,
                                                   system_hashed_ansi_string_create("Julia FBO color texture") );
    _julia_fbo_depth_to = ogl_texture_create_empty(context,
                                                   system_hashed_ansi_string_create("Julia FBO depth texture") );

    dsa_entrypoints->pGLTextureStorage2DEXT(_julia_fbo_color_to,
                                            GL_TEXTURE_2D,
                                            1,         /* levels */
                                            GL_RGBA32F,
                                            main_get_window_width(),
                                            main_get_window_height() );
    dsa_entrypoints->pGLTextureStorage2DEXT(_julia_fbo_depth_to,
                                            GL_TEXTURE_2D,
                                            1,                     /* levels */
                                            GL_DEPTH_COMPONENT32F,
                                            main_get_window_width(),
                                            main_get_window_height() );

    dsa_entrypoints->pGLNamedFramebufferTexture2DEXT(_julia_fbo_id,
                                                     GL_COLOR_ATTACHMENT0,
                                                     GL_TEXTURE_2D,
                                                     _julia_fbo_color_to,
                                                     0); /* level */
    dsa_entrypoints->pGLNamedFramebufferTexture2DEXT(_julia_fbo_id,
                                                     GL_DEPTH_ATTACHMENT,
                                                     GL_TEXTURE_2D,
                                                     _julia_fbo_depth_to,
                                                     0); /* level */

    /* Add ourselves to the pipeline */
    ogl_pipeline_stage_step_declaration raytracing_stage_step;

    raytracing_stage_step.name              = system_hashed_ansi_string_create("Raytracing");
    raytracing_stage_step.pfn_callback_proc = _stage_step_julia_execute;
    raytracing_stage_step.user_arg          = NULL;

    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                               &raytracing_stage_step);

    /* Set up matrices */
    _julia_view_matrix = system_matrix4x4_create();
}
