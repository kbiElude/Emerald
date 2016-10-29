/**
 *
 * DOF test app (kbi/elude @2012-2016)
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_julia.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "procedural/procedural_mesh_sphere.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture_view.h"
#include "system/system_matrix4x4.h"

ral_texture              _julia_color_rt                            = nullptr;
ral_texture_view         _julia_color_rt_view                       = nullptr;
ral_texture              _julia_depth_rt                            = nullptr;
ral_texture_view         _julia_depth_rt_view                       = nullptr;
uint32_t                 _julia_dof_cutoff_ub_offset                = -1;
uint32_t                 _julia_dof_far_plane_depth_ub_offset       = -1;
uint32_t                 _julia_dof_focal_plane_depth_ub_offset     = -1;
uint32_t                 _julia_dof_near_plane_depth_ub_offset      = -1; 
uint32_t                 _julia_data_ub_offset                      = -1;
uint32_t                 _julia_epsilon_ub_offset                   = -1;
uint32_t                 _julia_escape_ub_offset                    = -1;
uint32_t                 _julia_eye_ub_offset                       = -1;
uint32_t                 _julia_fresnel_reflectance_ub_offset       = -1;
ral_gfx_state            _julia_gfx_state                           = nullptr;
uint32_t                 _julia_light_color_ub_offset               = -1;
uint32_t                 _julia_light_position_ub_offset            = -1;
uint32_t                 _julia_max_iterations_ub_offset            = -1;
uint32_t                 _julia_mv_ub_offset                        = -1;
uint32_t                 _julia_mvp_ub_offset                       = -1;
ral_present_task         _julia_present_task                        = nullptr;
ral_program              _julia_program                             = 0;
ral_program_block_buffer _julia_program_ub                          = nullptr;
uint32_t                 _julia_raycast_radius_multiplier_ub_offset = -1;
uint32_t                 _julia_reflectivity_ub_offset              = -1;
uint32_t                 _julia_shadows_ub_offset                   = -1;
uint32_t                 _julia_specularity_ub_offset               = -1;
procedural_mesh_sphere   _julia_sphere                              = nullptr;
system_matrix4x4         _julia_view_matrix                         = nullptr;

/* Shaders.
 *
 * Built upon https://developer.apple.com/library/mac/samplecode/OpenCL_RayTraced_Quaternion_Julia-Set_Example/Listings/qjulia_kernel_cl.html#//apple_ref/doc/uid/DTS40008190-qjulia_kernel_cl-DontLinkElementID_5
 *
 * Stores depth in alpha channel for Scheuermann DOF
 **/
const char* julia_fragment_shader_code =
    "#version 430 core\n"
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

const char* julia_vertex_shader_code = 
    "#version 430 core\n"
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
PRIVATE void _stage_step_julia_update_buffers(void* unused)
{
    /* Calculate MVP matrix */
    float            camera_location[3];
    const float*     data                      = main_get_data_vector();
    const float      dof_cutoff                = main_get_dof_cutoff();
    const float      dof_far_plane_depth       = main_get_dof_far_plane_depth();
    const float      dof_focal_plane_depth     = main_get_dof_focal_plane_depth();
    const float      dof_near_plane_depth      = main_get_dof_near_plane_depth();
    const float      epsilon                   = main_get_epsilon();
    const float      escape                    = main_get_escape_threshold();
    const float      fresnel_reflectance       = main_get_fresnel_reflectance();
    const float      reflectivity              = main_get_reflectivity();
    const float*     light_color               = main_get_light_color();
    const float*     light_position            = main_get_light_position();
    const int        max_iterations            = main_get_max_iterations();
    const float      raycast_radius_multiplier = main_get_raycast_radius_multiplier();
    const bool       shadows                   = main_get_shadows_status();
    const float      specularity               = main_get_specularity();
    system_matrix4x4 mvp                       = nullptr; 

    if (_julia_view_matrix == nullptr)
    {
        _julia_view_matrix = system_matrix4x4_create();
    }

    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_CAMERA_LOCATION,
                            camera_location);
    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
                           &_julia_view_matrix);

    mvp = system_matrix4x4_create_by_mul(main_get_projection_matrix(),
                                         _julia_view_matrix);

    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_data_ub_offset,
                                                           data,
                                                           sizeof(float) * 4);
     ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_dof_cutoff_ub_offset,
                                                          &dof_cutoff,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_dof_far_plane_depth_ub_offset,
                                                          &dof_far_plane_depth,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_dof_focal_plane_depth_ub_offset,
                                                          &dof_focal_plane_depth,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_dof_near_plane_depth_ub_offset,
                                                          &dof_near_plane_depth,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_epsilon_ub_offset,
                                                          &epsilon,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_escape_ub_offset,
                                                          &escape,
                                                           sizeof(float) );
     ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_fresnel_reflectance_ub_offset,
                                                          &fresnel_reflectance,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_reflectivity_ub_offset,
                                                          &reflectivity,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub, 
                                                           _julia_light_position_ub_offset,
                                                           light_position,
                                                           sizeof(float) * 3);
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_light_color_ub_offset,
                                                           light_color,
                                                           sizeof(float) * 3);
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_max_iterations_ub_offset,
                                                          &max_iterations,
                                                           sizeof(int) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_raycast_radius_multiplier_ub_offset,
                                                          &raycast_radius_multiplier,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_shadows_ub_offset,
                                                          &shadows,
                                                           sizeof(bool) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_specularity_ub_offset,
                                                          &specularity,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_eye_ub_offset,
                                                           camera_location,
                                                           sizeof(float) * 3);
     ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_mv_ub_offset,
                                                           system_matrix4x4_get_column_major_data(_julia_view_matrix),
                                                           sizeof(float) * 16); 
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_mvp_ub_offset,
                                                           system_matrix4x4_get_column_major_data(mvp),
                                                           sizeof(float) * 16);

    ral_program_block_buffer_sync_immediately(_julia_program_ub);

    system_matrix4x4_release(mvp);
}

/* Please see header for specification */
PUBLIC void stage_step_julia_deinit(ral_context context)
{
    const ral_texture textures[] = 
    {
        _julia_color_rt,
        _julia_depth_rt
    };
    const ral_texture_view texture_views[] = 
    {
        _julia_color_rt_view,
        _julia_depth_rt_view
    };

    const uint32_t n_textures      = sizeof(textures)      / sizeof(textures     [0]);
    const uint32_t n_texture_views = sizeof(texture_views) / sizeof(texture_views[0]);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_julia_gfx_state) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_julia_program) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures,
                               reinterpret_cast<void* const*>(textures) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               n_texture_views,
                               reinterpret_cast<void* const*>(texture_views) );

    procedural_mesh_sphere_release(_julia_sphere);
    system_matrix4x4_release      (_julia_view_matrix);

    if (_julia_present_task != nullptr)
    {
        ral_present_task_release(_julia_present_task);

        _julia_present_task = nullptr;
    }

    if (_julia_program_ub != nullptr)
    {
        ral_program_block_buffer_release(_julia_program_ub);

        _julia_program_ub = nullptr;
    }
}

/** TODO */
PUBLIC ral_texture stage_step_julia_get_color_texture()
{
    return _julia_color_rt;
}

/** TODO */
PUBLIC ral_texture_view stage_step_julia_get_color_texture_view()
{
    return _julia_color_rt_view;
}

/** TODO */
PUBLIC ral_texture_view stage_step_julia_get_depth_texture_view()
{
    return _julia_depth_rt_view;
}

/* Please see header for specification */
PUBLIC ral_present_task stage_step_julia_get_present_task()
{
    return _julia_present_task;
}

/* Please see header for specification */
PUBLIC void stage_step_julia_init(ral_context context)
{
    ral_command_buffer cmd_buffer;

    /* Set up textures */
    ral_texture_create_info color_to_create_info;
    ral_texture_create_info depth_to_create_info;

    color_to_create_info.base_mipmap_depth      = 1;
    color_to_create_info.base_mipmap_height     = main_get_window_height();
    color_to_create_info.base_mipmap_width      = main_get_window_width();
    color_to_create_info.fixed_sample_locations = true;
    color_to_create_info.format                 = RAL_FORMAT_RGBA16_FLOAT;
    color_to_create_info.name                   = system_hashed_ansi_string_create("Julia FBO color texture");
    color_to_create_info.n_layers               = 1;
    color_to_create_info.n_samples              = 1;
    color_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_to_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  RAL_TEXTURE_USAGE_SAMPLED_BIT;
    color_to_create_info.use_full_mipmap_chain  = false;

    depth_to_create_info.base_mipmap_depth      = 1;
    depth_to_create_info.base_mipmap_height     = main_get_window_height();
    depth_to_create_info.base_mipmap_width      = main_get_window_width ();
    depth_to_create_info.fixed_sample_locations = true;
    depth_to_create_info.format                 = RAL_FORMAT_DEPTH16_SNORM;
    depth_to_create_info.name                   = system_hashed_ansi_string_create("Julia FBO depth texture");
    depth_to_create_info.n_layers               = 1;
    depth_to_create_info.n_samples              = 1;
    depth_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    depth_to_create_info.usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                  RAL_TEXTURE_USAGE_SAMPLED_BIT;
    depth_to_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(context,
                                1, /* n_textures */
                               &color_to_create_info,
                               &_julia_color_rt);
    ral_context_create_textures(context,
                                1, /* n_textures */
                               &depth_to_create_info,
                               &_julia_depth_rt);

    /* Set up texture views */
    ral_texture_view_create_info color_texture_view_create_info = ral_texture_view_create_info(_julia_color_rt);
    ral_texture_view_create_info depth_texture_view_create_info = ral_texture_view_create_info(_julia_depth_rt);

    ral_context_create_texture_views(context,
                                     1, /* n_texture_views */
                                    &color_texture_view_create_info,
                                    &_julia_color_rt_view);
    ral_context_create_texture_views(context,
                                     1, /* n_texture_views */
                                    &depth_texture_view_create_info,
                                    &_julia_depth_rt_view);

    /* Instantiate sphere we will use for raytracing */
    uint32_t normals_data_offset = 0;
    uint32_t vertex_data_offset  = 0;

    _julia_sphere = procedural_mesh_sphere_create(context,
                                                  PROCEDURAL_MESH_DATA_TYPE_NONINDEXED_BIT,
                                                  7, /* n_latitude_splices */
                                                  7, /* n_longitude_splices */
                                                  system_hashed_ansi_string_create("julia sphere") );

    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER_NORMAL_DATA_OFFSET,
                                       &normals_data_offset);
    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER_VERTEX_DATA_OFFSET,
                                       &vertex_data_offset);

    /* Set up matrices */
    _julia_view_matrix = system_matrix4x4_create();

    /* Prepare shaders */
    system_hashed_ansi_string    julia_fs_code_body;
    system_hashed_ansi_string    julia_vs_code_body;
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

    ral_context_create_shaders(context,
                               n_shader_create_info_items,
                               shader_create_info_items,
                               result_shaders);

    julia_fs_code_body = system_hashed_ansi_string_create(julia_fragment_shader_code);
    julia_vs_code_body = system_hashed_ansi_string_create(julia_vertex_shader_code);

    ral_shader_set_property(result_shaders[0],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &julia_fs_code_body);
    ral_shader_set_property(result_shaders[1],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &julia_vs_code_body);

    /* Link Julia program */
    const ral_program_create_info po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("julia program")
    };

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &po_create_info,
                               &_julia_program);
    ral_program_attach_shader(_julia_program,
                              result_shaders[0],
                              true /* async */);
    ral_program_attach_shader(_julia_program,
                              result_shaders[1],
                              true /* async */);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shader_create_info_items,
                               reinterpret_cast<void* const*>(result_shaders) );

    /* Generate & set VAO up */
    ral_buffer mesh_data_bo_ral = nullptr;

    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER,
                                       &mesh_data_bo_ral);

    /* Retrieve attribute/uniform locations */
    const ral_program_variable* data_uniform_ptr                      = nullptr;
    const ral_program_variable* dof_cutoff_uniform_ral_ptr            = nullptr;
    const ral_program_variable* dof_far_plane_depth_uniform_ral_ptr   = nullptr;
    const ral_program_variable* dof_focal_plane_depth_uniform_ral_ptr = nullptr;
    const ral_program_variable* dof_near_plane_depth_uniform_ral_ptr  = nullptr; 
    const ral_program_variable* epsilon_uniform_ptr                   = nullptr;
    const ral_program_variable* escape_uniform_ptr                    = nullptr;
    const ral_program_variable* eye_uniform_ptr                       = nullptr;
    const ral_program_variable* fresnel_reflectance_uniform_ral_ptr   = nullptr; 
    const ral_program_variable* light_color_uniform_ptr               = nullptr;
    const ral_program_variable* light_position_uniform_ptr            = nullptr;
    const ral_program_variable* max_iterations_uniform_ptr            = nullptr;
    const ral_program_variable* mv_uniform_ptr                        = nullptr;
    const ral_program_variable* mvp_uniform_ptr                       = nullptr;
    const ral_program_variable* raycast_radius_multiplier_uniform_ptr = nullptr;
    const ral_program_variable* reflectivity_uniform_ral_ptr          = nullptr; 
    const ral_program_variable* shadows_uniform_ptr                   = nullptr;
    const ral_program_variable* specularity_uniform_ptr               = nullptr;

    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("data"),
                                          &data_uniform_ptr);
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
                                          &epsilon_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("escape"),
                                          &escape_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("eye"),
                                          &eye_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("fresnel_reflectance"),
                                          &fresnel_reflectance_uniform_ral_ptr); 
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("light_color"),
                                          &light_color_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("light_position"),
                                          &light_position_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("max_iterations"),
                                          &max_iterations_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("mv"),
                                          &mv_uniform_ptr); 
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("mvp"),
                                          &mvp_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("raycast_radius_multiplier"),
                                          &raycast_radius_multiplier_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("reflectivity"),
                                          &reflectivity_uniform_ral_ptr); 
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("shadows"),
                                          &shadows_uniform_ptr);
    ral_program_get_block_variable_by_name(_julia_program,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("specularity"),
                                          &specularity_uniform_ptr);

    _julia_data_ub_offset                      = (data_uniform_ptr                      != nullptr) ? data_uniform_ptr->block_offset                      : -1;
    _julia_dof_cutoff_ub_offset                = (dof_cutoff_uniform_ral_ptr            != nullptr) ? dof_cutoff_uniform_ral_ptr->block_offset            : -1;
    _julia_dof_far_plane_depth_ub_offset       = (dof_far_plane_depth_uniform_ral_ptr   != nullptr) ? dof_far_plane_depth_uniform_ral_ptr->block_offset   : -1;
    _julia_dof_focal_plane_depth_ub_offset     = (dof_focal_plane_depth_uniform_ral_ptr != nullptr) ? dof_focal_plane_depth_uniform_ral_ptr->block_offset : -1; 
    _julia_dof_near_plane_depth_ub_offset      = (dof_near_plane_depth_uniform_ral_ptr  != nullptr) ? dof_near_plane_depth_uniform_ral_ptr->block_offset  : -1;
    _julia_epsilon_ub_offset                   = (epsilon_uniform_ptr                   != nullptr) ? epsilon_uniform_ptr->block_offset                   : -1;
    _julia_escape_ub_offset                    = (escape_uniform_ptr                    != nullptr) ? escape_uniform_ptr->block_offset                    : -1;
    _julia_eye_ub_offset                       = (eye_uniform_ptr                       != nullptr) ? eye_uniform_ptr->block_offset                       : -1;
    _julia_fresnel_reflectance_ub_offset       = (fresnel_reflectance_uniform_ral_ptr   != nullptr) ? fresnel_reflectance_uniform_ral_ptr->block_offset   : -1; 
    _julia_light_color_ub_offset               = (light_color_uniform_ptr               != nullptr) ? light_color_uniform_ptr->block_offset               : -1;
    _julia_light_position_ub_offset            = (light_position_uniform_ptr            != nullptr) ? light_position_uniform_ptr->block_offset            : -1;
    _julia_max_iterations_ub_offset            = (max_iterations_uniform_ptr            != nullptr) ? max_iterations_uniform_ptr->block_offset            : -1;
    _julia_mv_ub_offset                        = (mv_uniform_ptr                        != nullptr) ? mv_uniform_ptr->block_offset                        : -1;
    _julia_mvp_ub_offset                       = (mvp_uniform_ptr                       != nullptr) ? mvp_uniform_ptr->block_offset                       : -1;
    _julia_raycast_radius_multiplier_ub_offset = (raycast_radius_multiplier_uniform_ptr != nullptr) ? raycast_radius_multiplier_uniform_ptr->block_offset : -1;
    _julia_reflectivity_ub_offset              = (reflectivity_uniform_ral_ptr          != nullptr) ? reflectivity_uniform_ral_ptr->block_offset          : -1; 
    _julia_shadows_ub_offset                   = (shadows_uniform_ptr                   != nullptr) ? shadows_uniform_ptr->block_offset                   : -1;
    _julia_specularity_ub_offset               = (specularity_uniform_ptr               != nullptr) ? specularity_uniform_ptr->block_offset               : -1;

    /* Retrieve uniform block data */
    ral_buffer julia_program_ub_bo_ral = nullptr;

    _julia_program_ub = ral_program_block_buffer_create(context,
                                                        _julia_program,
                                                        system_hashed_ansi_string_create("dataUB") );

    ral_program_block_buffer_get_property(_julia_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &julia_program_ub_bo_ral);

    /* Set up gfx state we will use to render the fractal */
    ral_gfx_state_create_info      gfx_state_create_info;
    ral_gfx_state_vertex_attribute va;

    va.format     = RAL_FORMAT_RGB32_FLOAT;
    va.input_rate = RAL_VERTEX_INPUT_RATE_PER_VERTEX;
    va.name       = system_hashed_ansi_string_create("vertex"); /* is this really needed? */
    va.offset     = vertex_data_offset;
    va.stride     = sizeof(float) * 3;

    ral_command_buffer_set_viewport_command_info viewport_info;

    viewport_info.depth_range[0] = 0.0f;
    viewport_info.depth_range[1] = 1.0f;
    viewport_info.index          = 0;
    viewport_info.size[0]        = static_cast<float>(color_to_create_info.base_mipmap_width);
    viewport_info.size[1]        = static_cast<float>(color_to_create_info.base_mipmap_height);
    viewport_info.xy[0]          = 0;
    viewport_info.xy[1]          = 0;

    gfx_state_create_info.culling                              = true;
    gfx_state_create_info.cull_mode                            = RAL_CULL_MODE_BACK;
    gfx_state_create_info.depth_test                           = true;
    gfx_state_create_info.depth_test_compare_op                = RAL_COMPARE_OP_LESS;
    gfx_state_create_info.depth_writes                         = true;
    gfx_state_create_info.front_face                           = RAL_FRONT_FACE_CCW;
    gfx_state_create_info.n_vertex_attributes                  = 1;
    gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLES;
    gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
    gfx_state_create_info.static_viewports                     = &viewport_info;
    gfx_state_create_info.static_viewports_enabled             = true;
    gfx_state_create_info.vertex_attribute_ptrs                = &va;


    ral_context_create_gfx_states(context,
                                  1, /* n_create_info_items */
                                 &gfx_state_create_info,
                                 &_julia_gfx_state);

    /* Set up a command buffer we will use to render the fractal */
    ral_command_buffer_create_info cmd_buffer_create_info;

    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    cmd_buffer_create_info.is_executable                           = true;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &cmd_buffer);

    ral_command_buffer_start_recording(cmd_buffer);
    {
        ral_command_buffer_clear_rt_binding_command_info       clear_op;
        ral_command_buffer_set_color_rendertarget_command_info color_rt                = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        ral_command_buffer_draw_call_regular_command_info      draw_call;
        ral_buffer                                             julia_program_ub_bo_ral = nullptr;
        unsigned int                                           n_mesh_triangles        = 0;
        ral_command_buffer_set_binding_command_info            ub;
        ral_command_buffer_set_vertex_buffer_command_info      vb;

        procedural_mesh_sphere_get_property(_julia_sphere,
                                            PROCEDURAL_MESH_SPHERE_PROPERTY_N_TRIANGLES,
                                           &n_mesh_triangles);

        ral_program_block_buffer_get_property(_julia_program_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &julia_program_ub_bo_ral);

        clear_op.clear_regions[0].n_base_layer             = 0;
        clear_op.clear_regions[0].n_layers                 = 1;
        clear_op.clear_regions[0].size[0]                  = color_to_create_info.base_mipmap_width;
        clear_op.clear_regions[0].size[1]                  = color_to_create_info.base_mipmap_height;
        clear_op.clear_regions[0].xy  [0]                  = 0;
        clear_op.clear_regions[0].xy  [1]                  = 0;
        clear_op.n_clear_regions                           = 1;
        clear_op.n_rendertargets                           = 2;
        clear_op.rendertargets[0].aspect                   = static_cast<ral_texture_aspect>(RAL_TEXTURE_ASPECT_COLOR_BIT);
        clear_op.rendertargets[0].clear_value.color.f32[0] = 0;
        clear_op.rendertargets[0].clear_value.color.f32[1] = 0;
        clear_op.rendertargets[0].clear_value.color.f32[2] = 0;
        clear_op.rendertargets[0].clear_value.color.f32[3] = 0;
        clear_op.rendertargets[0].rt_index                 = 0;
        clear_op.rendertargets[1].aspect                   = static_cast<ral_texture_aspect>(RAL_TEXTURE_ASPECT_DEPTH_BIT);
        clear_op.rendertargets[1].clear_value.depth        = 1.0f;
        clear_op.rendertargets[1].rt_index                 = -1; /* irrelevant */

        color_rt.rendertarget_index = 0;
        color_rt.texture_view       = _julia_color_rt_view;

        draw_call.base_instance = 0;
        draw_call.base_vertex   = 0;
        draw_call.n_instances   = 1;
        draw_call.n_vertices    = n_mesh_triangles * 3;

        ub.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub.name                          = system_hashed_ansi_string_create("dataUB");
        ub.uniform_buffer_binding.buffer = julia_program_ub_bo_ral;
        ub.uniform_buffer_binding.offset = 0;
        ub.uniform_buffer_binding.size   = 0;

        vb.buffer       = mesh_data_bo_ral;
        vb.name         = system_hashed_ansi_string_create("vertex");
        vb.start_offset = vertex_data_offset;

        ral_command_buffer_record_set_gfx_state(cmd_buffer,
                                                _julia_gfx_state);
        ral_command_buffer_record_set_program  (cmd_buffer,
                                                _julia_program);

        ral_command_buffer_record_set_color_rendertargets   (cmd_buffer,
                                                             1, /* n_rendertargets */
                                                            &color_rt);
        ral_command_buffer_record_set_depth_rendertarget    (cmd_buffer,
                                                             _julia_depth_rt_view);
        ral_command_buffer_record_clear_rendertarget_binding(cmd_buffer,
                                                             1, /* n_clear_ops */
                                                            &clear_op);

        ral_command_buffer_record_set_bindings      (cmd_buffer,
                                                     1, /* n_bindings */
                                                    &ub);
        ral_command_buffer_record_set_vertex_buffers(cmd_buffer,
                                                     1, /* n_vertex_buffers */
                                                    &vb);

        ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call);
    }
    ral_command_buffer_stop_recording(cmd_buffer);

    /* Set up a present task we will use to render the fractal */
    ral_present_task                    cpu_task;
    ral_present_task_cpu_create_info    cpu_task_create_info;
    ral_present_task_io                 cpu_task_output;
    ral_present_task                    gpu_task;
    ral_present_task_gpu_create_info    gpu_task_create_info;
    ral_present_task_io                 gpu_task_inputs [3];
    ral_present_task_io                 gpu_task_outputs[2];
    ral_present_task_ingroup_connection group_task_connection;
    ral_present_task_group_create_info  group_task_create_info;
    ral_present_task_group_mapping      group_task_output_mappings[2];
    ral_present_task                    group_task_present_tasks  [2];

    cpu_task_output.buffer      = julia_program_ub_bo_ral;
    cpu_task_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_task_create_info.cpu_task_callback_user_arg = nullptr;
    cpu_task_create_info.n_unique_inputs            = 0;
    cpu_task_create_info.n_unique_outputs           = 1;
    cpu_task_create_info.pfn_cpu_task_callback_proc = _stage_step_julia_update_buffers;
    cpu_task_create_info.unique_inputs              = 0;
    cpu_task_create_info.unique_outputs             = &cpu_task_output;


    gpu_task_inputs[0]              = cpu_task_output;
    gpu_task_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    gpu_task_inputs[1].texture_view = _julia_color_rt_view;
    gpu_task_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    gpu_task_inputs[2].texture_view = _julia_depth_rt_view;

    gpu_task_outputs[0] = gpu_task_inputs[1];
    gpu_task_outputs[1] = gpu_task_inputs[2];

    gpu_task_create_info.command_buffer   = cmd_buffer;
    gpu_task_create_info.n_unique_inputs  = sizeof(gpu_task_inputs)  / sizeof(gpu_task_inputs [0]);
    gpu_task_create_info.n_unique_outputs = sizeof(gpu_task_outputs) / sizeof(gpu_task_outputs[0]);
    gpu_task_create_info.unique_inputs    = gpu_task_inputs;
    gpu_task_create_info.unique_outputs   = gpu_task_outputs;

    cpu_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Julia rendering: buffer update"),
                                          &cpu_task_create_info);
    gpu_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Julia rendering: rasterization"),
                                          &gpu_task_create_info);


    group_task_connection.input_present_task_index     = 1; /* gpu_present_task        */
    group_task_connection.input_present_task_io_index  = 0; /* julia_program_ub_bo_ral */
    group_task_connection.output_present_task_index    = 0; /* cpu_present_task        */
    group_task_connection.output_present_task_io_index = 0; /* julia_program_ub_bo_ral */

    group_task_output_mappings[0].group_task_io_index   = 0;
    group_task_output_mappings[0].n_present_task        = 1; /* gpu_present_task */
    group_task_output_mappings[0].present_task_io_index = 0; /* color_rt         */
    group_task_output_mappings[1].group_task_io_index   = 1;
    group_task_output_mappings[1].n_present_task        = 1; /* gpu_present_task */
    group_task_output_mappings[1].present_task_io_index = 1; /* depth_rt         */

    group_task_present_tasks[0] = cpu_task;
    group_task_present_tasks[1] = gpu_task;

    group_task_create_info.ingroup_connections                      = &group_task_connection;
    group_task_create_info.n_ingroup_connections                    = 1;
    group_task_create_info.n_present_tasks                          = sizeof(group_task_present_tasks) / sizeof(group_task_present_tasks[0]);
    group_task_create_info.n_total_unique_inputs                    = 0;
    group_task_create_info.n_total_unique_outputs                   = 2;
    group_task_create_info.n_unique_input_to_ingroup_task_mappings  = 0;
    group_task_create_info.n_unique_output_to_ingroup_task_mappings = sizeof(group_task_output_mappings) / sizeof(group_task_output_mappings[0]);
    group_task_create_info.present_tasks                            = group_task_present_tasks;
    group_task_create_info.unique_input_to_ingroup_task_mapping     = nullptr;
    group_task_create_info.unique_output_to_ingroup_task_mapping    = group_task_output_mappings;

    _julia_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("Julia rendering: rendering"),
                                                                                        &group_task_create_info);

    /* Clean up */
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&cmd_buffer) );

    ral_present_task_release  (cpu_task);
    ral_present_task_release  (gpu_task);
}
