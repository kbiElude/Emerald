/**
 *
 * Julia 4D test app (kbi/elude @2012-2015)
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_julia.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "procedural/procedural_mesh_sphere.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "system/system_matrix4x4.h"

GLuint                   _julia_data_ub_offset                      = -1;
GLuint                   _julia_epsilon_ub_offset                   = -1;
GLuint                   _julia_escape_ub_offset                    = -1;
GLuint                   _julia_eye_ub_offset                       = -1;
GLuint                   _julia_light_color_ub_offset               = -1;
GLuint                   _julia_light_position_ub_offset            = -1;
GLuint                   _julia_max_iterations_ub_offset            = -1;
GLuint                   _julia_mvp_ub_offset                       = -1;
ral_program              _julia_program                             = 0;
ral_program_block_buffer _julia_program_ub                          = NULL;
GLuint                   _julia_raycast_radius_multiplier_ub_offset = -1;
GLuint                   _julia_shadows_ub_offset                   = -1;
GLuint                   _julia_specularity_ub_offset               = -1;
procedural_mesh_sphere   _julia_sphere                              = NULL;
GLuint                   _julia_vao_id                              = -1;
GLuint                   _julia_vertex_attribute_location           = -1;
system_matrix4x4         _julia_view_matrix                         = NULL;

/* Shaders.
 *
 * Built upon https://developer.apple.com/library/mac/samplecode/OpenCL_RayTraced_Quaternion_Julia-Set_Example/Listings/qjulia_kernel_cl.html#//apple_ref/doc/uid/DTS40008190-qjulia_kernel_cl-DontLinkElementID_5 
 **/
const char* julia_fragment_shader_code = "#version 430 core\n"
                                         "\n"
                                         "in vec3 fp_vertex;\n"
                                         "\n"
                                         "uniform dataUB\n"
                                         "{\n"
                                         "    vec4  data;\n"
                                         "    float epsilon;\n"
                                         "    float escape;\n"
                                         "    vec3  eye;\n"
                                         "    vec3  light_color;\n"
                                         "    vec3  light_position;\n"
                                         "    int   max_iterations;\n"
                                         "    mat4  mvp;\n"
                                         "    float raycast_radius_multiplier;\n"
                                         "    bool  shadows;\n"
                                         "    float specularity;\n"
                                         "};\n"
                                         "\n"
                                         "out vec4 result;\n"
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
                                         "\n"
                                         "    vec3 diffuse  = light_color * max(n_dot_l, 0);\n"
                                         "    vec3 specular = vec3(pow(max(dot(eye_direction, reflected_direction), 0.0), specularity) );\n"
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
                                         // gl_FragDepth should be used with NDC coordinates.
                                         //"            gl_FragDepth = gl_FragCoord.z + eye_fractal_intersection.z;\n"
                                         "            result       = vec4(diffuse, 1);\n"
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
                                       "uniform dataUB\n"
                                       "{\n"
                                       "    vec4  data;\n"
                                       "    float epsilon;\n"
                                       "    float escape;\n"
                                       "    vec3  eye;\n"
                                       "    vec3  light_color;\n"
                                       "    vec3  light_position;\n"
                                       "    int   max_iterations;\n"
                                       "    mat4  mvp;\n"
                                       "    float raycast_radius_multiplier;\n"
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
    ogl_context                       context_gl       = ral_context_get_gl_context(context);
    const ogl_context_gl_entrypoints* entrypoints_ptr  = NULL;
    const raGL_program                julia_po_raGL    = ral_context_get_program_gl(context,
                                                                                    _julia_program);
    GLuint                            julia_po_raGL_id = 0;

    raGL_program_get_property(julia_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &julia_po_raGL_id);
    ogl_context_get_property (context_gl,
                              OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                             &entrypoints_ptr);

    entrypoints_ptr->pGLUseProgram     (julia_po_raGL_id);
    entrypoints_ptr->pGLBindVertexArray(_julia_vao_id);

    /* Calculate MVP matrix */
          float      camera_location[3];
    const float*     data                      = main_get_data_vector();
    const float      epsilon                   = main_get_epsilon();
    const float      escape                    = main_get_escape_threshold();
    const float*     light_color               = main_get_light_color();
    const float*     light_position            = main_get_light_position();
    const int        max_iterations            = main_get_max_iterations();
    const float      raycast_radius_multiplier = main_get_raycast_radius_multiplier();
    const bool       shadows                   = main_get_shadows_status();
    const float      specularity               = main_get_specularity();
    system_matrix4x4 mvp                       = NULL;

    if (_julia_view_matrix == NULL)
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
                                                           _julia_epsilon_ub_offset,
                                                          &epsilon,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_julia_program_ub,
                                                           _julia_escape_ub_offset,
                                                          &escape,
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
                                                           _julia_mvp_ub_offset,
                                                           system_matrix4x4_get_column_major_data(mvp),
                                                           sizeof(float) * 16);

    ral_program_block_buffer_sync(_julia_program_ub);

    system_matrix4x4_release(mvp);

    /* Draw the fractal */
    raGL_buffer julia_program_ub_bo_raGL              = NULL;
    GLuint      julia_program_ub_bo_raGL_id           = 0;
    uint32_t    julia_program_ub_bo_raGL_start_offset = -1;
    ral_buffer  julia_program_ub_bo_ral               = NULL;
    uint32_t    julia_program_ub_bo_ral_size          = 0;
    uint32_t    julia_program_ub_bo_ral_start_offset  = 0;

    ral_program_block_buffer_get_property(_julia_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &julia_program_ub_bo_ral);

    julia_program_ub_bo_raGL = ral_context_get_buffer_gl(context,
                                                         julia_program_ub_bo_ral);

    ral_buffer_get_property (julia_program_ub_bo_ral,
                             RAL_BUFFER_PROPERTY_SIZE,
                             &julia_program_ub_bo_ral_size);
    ral_buffer_get_property (julia_program_ub_bo_ral,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                             &julia_program_ub_bo_ral_start_offset);
    raGL_buffer_get_property(julia_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &julia_program_ub_bo_raGL_id);
    raGL_buffer_get_property(julia_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &julia_program_ub_bo_raGL_start_offset);

    entrypoints_ptr->pGLClearColor     (0,
                                        0.3f,
                                        0.7f,
                                        1.0f);
    entrypoints_ptr->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entrypoints_ptr->pGLEnable         (GL_DEPTH_TEST);
    entrypoints_ptr->pGLDepthFunc      (GL_LESS);
    entrypoints_ptr->pGLEnable         (GL_CULL_FACE);
    entrypoints_ptr->pGLFrontFace      (GL_CW);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        julia_program_ub_bo_raGL_id,
                                        julia_program_ub_bo_raGL_start_offset + julia_program_ub_bo_ral_start_offset,
                                        julia_program_ub_bo_ral_size);
    {
        unsigned int n_triangles = 0;

        procedural_mesh_sphere_get_property(_julia_sphere,
                                            PROCEDURAL_MESH_SPHERE_PROPERTY_N_TRIANGLES,
                                           &n_triangles);

        entrypoints_ptr->pGLDrawArrays(GL_TRIANGLES,
                                       0, /* first */
                                       n_triangles * 3);
    }
    entrypoints_ptr->pGLDisable(GL_CULL_FACE);
}

/* Please see header for specification */
PUBLIC void stage_step_julia_deinit(ral_context context)
{
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &_julia_program);

    procedural_mesh_sphere_release(_julia_sphere);
    system_matrix4x4_release      (_julia_view_matrix);

    if (_julia_program_ub != NULL)
    {
        ral_program_block_buffer_release(_julia_program_ub);

        _julia_program_ub = NULL;
    }
}

/* Please see header for specification */
PUBLIC void stage_step_julia_init(ral_context  context,
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

    const ral_program_create_info po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("julia program")
    };

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &po_create_info,
                               &_julia_program);
    ral_context_create_shaders (context,
                                n_shader_create_info_items,
                                shader_create_info_items,
                                result_shaders);

    system_hashed_ansi_string julia_fs_code_body = system_hashed_ansi_string_create(julia_fragment_shader_code);
    system_hashed_ansi_string julia_vs_code_body = system_hashed_ansi_string_create(julia_vertex_shader_code);

    ral_shader_set_property(result_shaders[0],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &julia_fs_code_body);
    ral_shader_set_property(result_shaders[1],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &julia_vs_code_body);

    ral_program_attach_shader(_julia_program,
                              result_shaders[0],
                              true /* async */);
    ral_program_attach_shader(_julia_program,
                              result_shaders[1],
                              true /* async */);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shader_create_info_items,
                               (const void**) result_shaders);

    /* Generate & set VAO up */
    ogl_context                       context_gl      = NULL;
    ral_buffer                        data_bo         = NULL;
    raGL_buffer                       data_bo_raGL    = NULL;
    GLuint                            data_bo_raGL_id = 0;
    const ogl_context_gl_entrypoints* entrypoints     = NULL;

    context_gl = ral_context_get_gl_context(context);

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    procedural_mesh_sphere_get_property(_julia_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_RAL,
                                       &data_bo);

    data_bo_raGL = ral_context_get_buffer_gl(context,
                                             data_bo);

    raGL_buffer_get_property(data_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &data_bo_raGL_id);

    entrypoints->pGLGenVertexArrays(1,
                                   &_julia_vao_id);
    entrypoints->pGLBindVertexArray(_julia_vao_id);

    /* Retrieve attribute/uniform locations */
    const ral_program_variable*    data_uniform_ptr                      = NULL;
    const ral_program_variable*    epsilon_uniform_ptr                   = NULL;
    const ral_program_variable*    escape_uniform_ptr                    = NULL;
    const ral_program_variable*    eye_uniform_ptr                       = NULL;
    const ral_program_variable*    light_color_uniform_ptr               = NULL;
    const ral_program_variable*    light_position_uniform_ptr            = NULL;
    const ral_program_variable*    max_iterations_uniform_ptr            = NULL;
    const ral_program_variable*    mvp_uniform_ptr                       = NULL;
    const ral_program_variable*    raycast_radius_multiplier_uniform_ptr = NULL;
    const ral_program_variable*    shadows_uniform_ptr                   = NULL;
    const ral_program_variable*    specularity_uniform_ptr               = NULL;
    const _raGL_program_attribute* vertex_attribute_ptr                  = NULL;

    const raGL_program julia_po_raGL = ral_context_get_program_gl(context,
                                                                  _julia_program);

    raGL_program_get_vertex_attribute_by_name(julia_po_raGL,
                                              system_hashed_ansi_string_create("vertex"),
                                             &vertex_attribute_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("data"),
                                             &data_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("epsilon"),
                                             &epsilon_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("escape"),
                                             &escape_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("eye"),
                                             &eye_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("light_color"),
                                             &light_color_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("light_position"),
                                             &light_position_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("max_iterations"),
                                             &max_iterations_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("mvp"),
                                             &mvp_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("raycast_radius_multiplier"),
                                             &raycast_radius_multiplier_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("shadows"),
                                             &shadows_uniform_ptr);
    ral_program_get_block_variable_by_name   (_julia_program,
                                              system_hashed_ansi_string_create("dataUB"),
                                              system_hashed_ansi_string_create("specularity"),
                                             &specularity_uniform_ptr);

    _julia_data_ub_offset                      = (data_uniform_ptr                      != NULL) ? data_uniform_ptr->block_offset                      : -1;
    _julia_epsilon_ub_offset                   = (epsilon_uniform_ptr                   != NULL) ? epsilon_uniform_ptr->block_offset                   : -1;
    _julia_escape_ub_offset                    = (escape_uniform_ptr                    != NULL) ? escape_uniform_ptr->block_offset                    : -1;
    _julia_eye_ub_offset                       = (eye_uniform_ptr                       != NULL) ? eye_uniform_ptr->block_offset                       : -1;
    _julia_light_color_ub_offset               = (light_color_uniform_ptr               != NULL) ? light_color_uniform_ptr->block_offset               : -1;
    _julia_light_position_ub_offset            = (light_position_uniform_ptr            != NULL) ? light_position_uniform_ptr->block_offset            : -1;
    _julia_max_iterations_ub_offset            = (max_iterations_uniform_ptr            != NULL) ? max_iterations_uniform_ptr->block_offset            : -1;
    _julia_mvp_ub_offset                       = (mvp_uniform_ptr                       != NULL) ? mvp_uniform_ptr->block_offset                       : -1;
    _julia_raycast_radius_multiplier_ub_offset = (raycast_radius_multiplier_uniform_ptr != NULL) ? raycast_radius_multiplier_uniform_ptr->block_offset : -1;
    _julia_shadows_ub_offset                   = (shadows_uniform_ptr                   != NULL) ? shadows_uniform_ptr->block_offset                   : -1;
    _julia_specularity_ub_offset               = (specularity_uniform_ptr               != NULL) ? specularity_uniform_ptr->block_offset               : -1;
    _julia_vertex_attribute_location           = (vertex_attribute_ptr                  != NULL) ? vertex_attribute_ptr->location                      : -1;

    /* Retrieve uniform block data */
    _julia_program_ub = ral_program_block_buffer_create(context,
                                                        _julia_program,
                                                        system_hashed_ansi_string_create("dataUB") );

    /* Set up the VAO bindings */
    entrypoints->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                            data_bo_raGL_id);
    entrypoints->pGLVertexAttribPointer    (_julia_vertex_attribute_location,
                                            3,        /* size */
                                            GL_FLOAT,
                                            GL_FALSE, /* normalized */
                                            0,        /* stride */
                                            (void*) vertex_data_offset);
    entrypoints->pGLEnableVertexAttribArray(_julia_vertex_attribute_location);

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
