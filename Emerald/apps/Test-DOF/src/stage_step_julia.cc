/**
 *
 * DOF test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_background.h"
#include "stage_step_julia.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program_ub.h"
#include "procedural/procedural_mesh_sphere.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "system/system_matrix4x4.h"

GLuint                 _julia_data_ub_offset                      = -1;
GLuint                 _julia_dof_cutoff_ub_offset                = -1;
GLuint                 _julia_dof_far_plane_depth_ub_offset       = -1;
GLuint                 _julia_dof_focal_plane_depth_ub_offset     = -1;
GLuint                 _julia_dof_near_plane_depth_ub_offset      = -1;
GLuint                 _julia_epsilon_ub_offset                   = -1;
GLuint                 _julia_escape_ub_offset                    = -1;
GLuint                 _julia_eye_ub_offset                       = -1;
ral_framebuffer        _julia_fbo                                 = NULL;
ral_texture            _julia_fbo_color_to                        = 0;
ral_texture            _julia_fbo_depth_to                        = 0;
GLuint                 _julia_fresnel_reflectance_ub_offset       = -1;
GLuint                 _julia_light_color_ub_offset               = -1;
GLuint                 _julia_light_position_ub_offset            = -1;
GLuint                 _julia_max_iterations_ub_offset            = -1;
GLuint                 _julia_mv_ub_offset                        = -1;
GLuint                 _julia_mvp_ub_offset                       = -1;
ral_program            _julia_program                             = NULL;
ogl_program_ub         _julia_program_ub                          = NULL;
ral_buffer             _julia_program_ub_bo                       = NULL;
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
static void _stage_step_julia_execute(ral_context context,
                                      uint32_t    frame_index,
                                      system_time time,
                                      const int*  rendering_area_px_topdown,
                                      void*       not_used)
{
    ogl_context                                               context_gl       = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints  = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints      = NULL;
    const raGL_program                                        julia_po_raGL    = ral_context_get_program_gl(context,
                                                                                                            _julia_program);
    GLuint                                                    julia_po_raGL_id = 0;

    raGL_program_get_property(julia_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &julia_po_raGL_id);

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLUseProgram     (julia_po_raGL_id);
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
    const float             fresnel_reflectance       = main_get_fresnel_reflectance();
    const float             reflectivity              = main_get_reflectivity();
    const float*            light_color               = main_get_light_color();
    const float*            light_position            = main_get_light_position();
    const int               max_iterations            = main_get_max_iterations();
    const float             raycast_radius_multiplier = main_get_raycast_radius_multiplier();
    const bool              shadows                   = main_get_shadows_status();
    const float             specularity               = main_get_specularity();
           system_matrix4x4 mvp                       = NULL;

    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_CAMERA_LOCATION,
                            camera_location);
    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
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


    ral_texture  background_texture         = stage_step_background_get_background_texture();
    raGL_texture background_texture_raGL    = NULL;
    GLuint       background_texture_raGL_id = 0;

    background_texture_raGL = ral_context_get_texture_gl(context,
                                                         background_texture);

    raGL_texture_get_property(background_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &background_texture_raGL_id);


    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                            GL_TEXTURE_2D,
                                            background_texture_raGL_id);

    /* Draw the fractal */
    raGL_framebuffer julia_fbo_raGL                        = NULL;
    GLuint           julia_fbo_raGL_id                     = 0;
    raGL_buffer      julia_program_ub_bo_raGL              = NULL;
    GLuint           julia_program_ub_bo_raGL_id           = 0;
    uint32_t         julia_program_ub_bo_raGL_start_offset = -1;
    uint32_t         julia_program_ub_bo_ral_size          = 0;
    uint32_t         julia_program_ub_bo_ral_start_offset  = 0;

    julia_fbo_raGL = ral_context_get_framebuffer_gl(context,
                                                    _julia_fbo);

    raGL_framebuffer_get_property(julia_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &julia_fbo_raGL_id);


    julia_program_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                         _julia_program_ub_bo);

    raGL_buffer_get_property(julia_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &julia_program_ub_bo_raGL_id);
    raGL_buffer_get_property(julia_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &julia_program_ub_bo_raGL_id);
    raGL_buffer_get_property(julia_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &julia_program_ub_bo_raGL_start_offset);
    ral_buffer_get_property (_julia_program_ub_bo,
                             RAL_BUFFER_PROPERTY_SIZE,
                            &julia_program_ub_bo_ral_size);
    ral_buffer_get_property (_julia_program_ub_bo,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &julia_program_ub_bo_ral_start_offset);

    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    0, /* index */
                                    julia_program_ub_bo_raGL_id,
                                    julia_program_ub_bo_raGL_start_offset + julia_program_ub_bo_ral_start_offset,
                                    julia_program_ub_bo_ral_size);

    entrypoints->pGLClearColor     (0.0f,  /* red */
                                    0.0f,  /* green */
                                    0.0f,  /* blue */
                                    0.0f); /* alpha */
    entrypoints->pGLClearDepth     (1.0f);
    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    julia_fbo_raGL_id);
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
PUBLIC void stage_step_julia_deinit(ral_context context)
{
    ral_texture textures_to_release[] =
    {
        _julia_fbo_color_to,
        _julia_fbo_depth_to
    };
    const uint32_t n_textures_to_release = sizeof(textures_to_release) / sizeof(textures_to_release[0]);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                               1, /* n_objects */
                               (const void**) &_julia_fbo);
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &_julia_program);
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures_to_release,
                               (const void**) textures_to_release);

    procedural_mesh_sphere_release(_julia_sphere);
    system_matrix4x4_release      (_julia_view_matrix);
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_julia_get_color_texture()
{
    return _julia_fbo_color_to;
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_julia_get_depth_texture()
{
    return _julia_fbo_depth_to;
}

/* Please see header for specification */
PUBLIC ral_framebuffer stage_step_julia_get_fbo()
{
    return _julia_fbo;
}

/* Please see header for specification */
PUBLIC void stage_step_julia_init(ral_context  context,
                                  ogl_pipeline pipeline,
                                  uint32_t     stage_id)
{
    ogl_context context_gl = NULL;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);

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
    const system_hashed_ansi_string julia_fs_body = system_hashed_ansi_string_create(julia_fragment_shader_code);
    const system_hashed_ansi_string julia_vs_body = system_hashed_ansi_string_create(julia_vertex_shader_code);

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("julia program")
    };
    const ral_shader_create_info shader_create_info_items[] =
    {
        {
            system_hashed_ansi_string_create("julia fragment"),
            RAL_SHADER_TYPE_FRAGMENT
        },
        {
            system_hashed_ansi_string_create("julia vertex"),
            RAL_SHADER_TYPE_VERTEX
        }
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader     result_shaders[n_shader_create_info_items];

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &program_create_info,
                               &_julia_program);
    ral_context_create_shaders (context,
                                n_shader_create_info_items,
                                shader_create_info_items,
                                result_shaders);

    ral_shader_set_property(result_shaders[0],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &julia_fs_body);
    ral_shader_set_property(result_shaders[1],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &julia_vs_body);

    ral_program_attach_shader(_julia_program,
                              result_shaders[0]);
    ral_program_attach_shader(_julia_program,
                              result_shaders[1]);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shader_create_info_items,
                               (const void**) result_shaders);

    /* Retrieve data uniform block properties */
    const raGL_program julia_program_raGL    = ral_context_get_program_gl(context,
                                                                          _julia_program);
    GLuint             julia_program_raGL_id = 0;

    raGL_program_get_property             (julia_program_raGL,
                                           RAGL_PROGRAM_PROPERTY_ID,
                                          &julia_program_raGL_id);
    raGL_program_get_uniform_block_by_name(julia_program_raGL,
                                           system_hashed_ansi_string_create("dataUB"),
                                          &_julia_program_ub);

    ogl_program_ub_get_property(_julia_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &_julia_program_ub_bo);

    /* Retrieve attribute/uniform locations */
    const ral_program_variable*    data_uniform_ral_ptr                      = NULL;
    const ral_program_variable*    dof_cutoff_uniform_ral_ptr                = NULL;
    const ral_program_variable*    dof_far_plane_depth_uniform_ral_ptr       = NULL;
    const ral_program_variable*    dof_focal_plane_depth_uniform_ral_ptr     = NULL;
    const ral_program_variable*    dof_near_plane_depth_uniform_ral_ptr      = NULL;
    const ral_program_variable*    epsilon_uniform_ral_ptr                   = NULL;
    const ral_program_variable*    escape_uniform_ral_ptr                    = NULL;
    const ral_program_variable*    eye_uniform_ral_ptr                       = NULL;
    const ral_program_variable*    fresnel_reflectance_uniform_ral_ptr       = NULL;
    const ral_program_variable*    light_color_uniform_ral_ptr               = NULL;
    const ral_program_variable*    light_position_uniform_ral_ptr            = NULL;
    const ral_program_variable*    max_iterations_uniform_ral_ptr            = NULL;
    const ral_program_variable*    mv_uniform_ral_ptr                        = NULL;
    const ral_program_variable*    mvp_uniform_ral_ptr                       = NULL;
    const ral_program_variable*    raycast_radius_multiplier_uniform_ral_ptr = NULL;
    const ral_program_variable*    reflectivity_uniform_ral_ptr              = NULL;
    const ral_program_variable*    shadows_uniform_ral_ptr                   = NULL;
    const ral_program_variable*    specularity_uniform_ral_ptr               = NULL;
    const _raGL_program_variable*  sph_texture_uniform_raGL_ptr              = NULL;
    const _raGL_program_attribute* vertex_attribute_raGL_ptr                 = NULL;

    raGL_program_get_vertex_attribute_by_name(julia_program_raGL,
                                              system_hashed_ansi_string_create("vertex"),
                                             &vertex_attribute_raGL_ptr);
    raGL_program_get_uniform_by_name         (julia_program_raGL,
                                             system_hashed_ansi_string_create("sph_texture"),
                                            &sph_texture_uniform_raGL_ptr);

    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("data"),
                                          &data_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("dof_cutoff"),
                                          &dof_cutoff_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("dof_far_plane_depth"),
                                          &dof_far_plane_depth_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("dof_focal_plane_depth"),
                                          &dof_focal_plane_depth_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("dof_near_plane_depth"),
                                          &dof_near_plane_depth_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("epsilon"),
                                          &epsilon_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("escape"),
                                          &escape_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("eye"),
                                          &eye_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                          system_hashed_ansi_string_create("fresnel_reflectance"),
                                         &fresnel_reflectance_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("light_color"),
                                          &light_color_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("light_position"),
                                          &light_position_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("max_iterations"),
                                          &max_iterations_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("mv"),
                                          &mv_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("mvp"),
                                          &mvp_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("raycast_radius_multiplier"),
                                          &raycast_radius_multiplier_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("reflectivity"),
                                          &reflectivity_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("shadows"),
                                          &shadows_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("specularity"),
                                          &specularity_uniform_ral_ptr);

    _julia_data_ub_offset                      = (data_uniform_ral_ptr                      != NULL) ? data_uniform_ral_ptr->block_offset                      : -1;
    _julia_dof_cutoff_ub_offset                = (dof_cutoff_uniform_ral_ptr                != NULL) ? dof_cutoff_uniform_ral_ptr->block_offset                : -1;
    _julia_dof_far_plane_depth_ub_offset       = (dof_far_plane_depth_uniform_ral_ptr       != NULL) ? dof_far_plane_depth_uniform_ral_ptr->block_offset       : -1;
    _julia_dof_focal_plane_depth_ub_offset     = (dof_focal_plane_depth_uniform_ral_ptr     != NULL) ? dof_focal_plane_depth_uniform_ral_ptr->block_offset     : -1;
    _julia_dof_near_plane_depth_ub_offset      = (dof_near_plane_depth_uniform_ral_ptr      != NULL) ? dof_near_plane_depth_uniform_ral_ptr->block_offset      : -1;
    _julia_epsilon_ub_offset                   = (epsilon_uniform_ral_ptr                   != NULL) ? epsilon_uniform_ral_ptr->block_offset                   : -1;
    _julia_escape_ub_offset                    = (escape_uniform_ral_ptr                    != NULL) ? escape_uniform_ral_ptr->block_offset                    : -1;
    _julia_eye_ub_offset                       = (eye_uniform_ral_ptr                       != NULL) ? eye_uniform_ral_ptr->block_offset                       : -1;
    _julia_fresnel_reflectance_ub_offset       = (fresnel_reflectance_uniform_ral_ptr       != NULL) ? fresnel_reflectance_uniform_ral_ptr->block_offset       : -1;
    _julia_light_color_ub_offset               = (light_color_uniform_ral_ptr               != NULL) ? light_color_uniform_ral_ptr->block_offset               : -1;
    _julia_light_position_ub_offset            = (light_position_uniform_ral_ptr            != NULL) ? light_position_uniform_ral_ptr->block_offset            : -1;
    _julia_max_iterations_ub_offset            = (max_iterations_uniform_ral_ptr            != NULL) ? max_iterations_uniform_ral_ptr->block_offset            : -1;
    _julia_mv_ub_offset                        = (mv_uniform_ral_ptr                        != NULL) ? mv_uniform_ral_ptr->block_offset                        : -1;
    _julia_mvp_ub_offset                       = (mvp_uniform_ral_ptr                       != NULL) ? mvp_uniform_ral_ptr->block_offset                       : -1;
    _julia_raycast_radius_multiplier_ub_offset = (raycast_radius_multiplier_uniform_ral_ptr != NULL) ? raycast_radius_multiplier_uniform_ral_ptr->block_offset : -1;
    _julia_reflectivity_ub_offset              = (reflectivity_uniform_ral_ptr              != NULL) ? reflectivity_uniform_ral_ptr->block_offset              : -1;
    _julia_shadows_ub_offset                   = (shadows_uniform_ral_ptr                   != NULL) ? shadows_uniform_ral_ptr->block_offset                   : -1;
    _julia_specularity_ub_offset               = (specularity_uniform_ral_ptr               != NULL) ? specularity_uniform_ral_ptr->block_offset               : -1;
    _julia_sph_texture_uniform_location        = (sph_texture_uniform_raGL_ptr              != NULL) ? sph_texture_uniform_raGL_ptr->location                  : -1;
    _julia_vertex_attribute_location           = (vertex_attribute_raGL_ptr                 != NULL) ? vertex_attribute_raGL_ptr->location                     : -1;

    /* Generate & set VAO up */
    const ogl_context_gl_entrypoints* entrypoints       = NULL;
    ral_buffer                        sphere_bo         = NULL;
    raGL_buffer                       sphere_bo_raGL    = NULL;
    GLuint                            sphere_bo_raGL_id = 0;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_RAL,
                                       &sphere_bo);

    sphere_bo_raGL = ral_context_get_buffer_gl(context,
                                               sphere_bo);

    raGL_buffer_get_property(sphere_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &sphere_bo_raGL_id);

    entrypoints->pGLGenVertexArrays(1, /* n */
                                   &_julia_vao_id);
    entrypoints->pGLBindVertexArray(_julia_vao_id);

    entrypoints->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                            sphere_bo_raGL_id);
    entrypoints->pGLVertexAttribPointer    (_julia_vertex_attribute_location,
                                            3, /* size */
                                            GL_FLOAT,
                                            GL_FALSE,
                                            0, /* stride */
                                            (void*) (intptr_t) vertex_data_offset);
    entrypoints->pGLEnableVertexAttribArray(_julia_vertex_attribute_location);

    entrypoints->pGLProgramUniform1i(julia_program_raGL_id,
                                     _julia_sph_texture_uniform_location,
                                     0);

    /* Generate & set FBO up */
    ral_texture_create_info color_to_create_info;
    ral_texture_create_info depth_to_create_info;

    ral_context_create_framebuffers(context,
                                    1, /* n_framebuffers */
                                    &_julia_fbo);

    color_to_create_info.base_mipmap_depth      = 1;
    color_to_create_info.base_mipmap_height     = main_get_window_height();
    color_to_create_info.base_mipmap_width      = main_get_window_width();
    color_to_create_info.fixed_sample_locations = true;
    color_to_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA32_FLOAT;
    color_to_create_info.name                   = system_hashed_ansi_string_create("Julia FBO color texture");
    color_to_create_info.n_layers               = 1;
    color_to_create_info.n_samples              = 1;
    color_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_to_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  RAL_TEXTURE_USAGE_SAMPLED_BIT;

    depth_to_create_info.base_mipmap_depth      = 1;
    depth_to_create_info.base_mipmap_height     = main_get_window_height();
    depth_to_create_info.base_mipmap_width      = main_get_window_width ();
    depth_to_create_info.fixed_sample_locations = true;
    depth_to_create_info.format                 = RAL_TEXTURE_FORMAT_DEPTH32_FLOAT;
    depth_to_create_info.name                   = system_hashed_ansi_string_create("Julia FBO depth texture");
    depth_to_create_info.n_layers               = 1;
    depth_to_create_info.n_samples              = 1;
    depth_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    depth_to_create_info.usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                  RAL_TEXTURE_USAGE_SAMPLED_BIT;

#pragma pack(push)
#pragma pack(1)
    const ral_texture_create_info texture_create_info_items[] =
    {
        color_to_create_info,
        depth_to_create_info
    };
#pragma pack(pop)
    const uint32_t n_texture_create_info_items = sizeof(texture_create_info_items) / sizeof(texture_create_info_items[0]);
    ral_texture    result_textures[n_texture_create_info_items];

    ral_context_create_textures(context,
                                n_texture_create_info_items,
                                texture_create_info_items,
                                result_textures);

    _julia_fbo_color_to = result_textures[0];
    _julia_fbo_depth_to = result_textures[1];

    ral_framebuffer_set_attachment_2D(_julia_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      _julia_fbo_color_to,
                                      0 /* n_mipmap */);
    ral_framebuffer_set_attachment_2D(_julia_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL,
                                      0, /* index */
                                      _julia_fbo_depth_to,
                                      0 /* n_mipmap */);

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
