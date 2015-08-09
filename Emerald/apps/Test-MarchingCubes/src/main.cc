/**
 *
 * Marching cubes test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include <algorithm>
#include "main.h"

PRIVATE const unsigned int _blob_size[] = {25, 25, 25};

PRIVATE GLuint              _blob_scalar_field_bo_id                           = 0;
PRIVATE unsigned int        _blob_scalar_field_bo_size                         = 0;
PRIVATE unsigned int        _blob_scalar_field_bo_start_offset                 = 0;
PRIVATE mesh                _blob_mesh                                         = NULL;
PRIVATE ogl_context         _context                                           = NULL;
PRIVATE ogl_flyby           _context_flyby                                     = NULL;
PRIVATE ogl_program         _po_scalar_field_generator                         = NULL;
PRIVATE ogl_program_ub      _po_scalar_field_generator_data_ub                 = NULL;
PRIVATE ogl_program         _po_scalar_field_renderer                          = NULL;
PRIVATE ogl_program_ub      _po_scalar_field_renderer_data_ub                  = NULL;
PRIVATE GLuint              _po_scalar_field_renderer_data_ub_bo_id            = 0;
PRIVATE GLuint              _po_scalar_field_renderer_data_ub_bo_start_offset  = -1;
PRIVATE GLuint              _po_scalar_field_renderer_data_ub_model_offset     = -1;
PRIVATE GLuint              _po_scalar_field_renderer_data_ub_vp_offset        = -1;
PRIVATE system_matrix4x4    _projection_matrix                                 = NULL;
PRIVATE scene               _scene                                             = NULL;
PRIVATE scene_graph_node    _scene_blob_node                                   = NULL;
PRIVATE scene_camera        _scene_camera                                      = NULL;
PRIVATE scene_graph_node    _scene_camera_node                                 = NULL;
PRIVATE scene_graph         _scene_graph                                       = NULL; /* do not release */
PRIVATE ogl_scene_renderer  _scene_renderer                                    = NULL;
PRIVATE system_event        _window_closed_event                               = system_event_create(true); /* manual_reset */
PRIVATE int                 _window_size[2]                                    = {1280, 720};
PRIVATE system_matrix4x4    _view_matrix                                       = NULL;


/* Forward declarations */
PRIVATE void _get_blob_bounding_box_aabb     (void*                             user_arg,
                                              float**                           out_aabb_model_vec4_min,
                                              float**                           out_aabb_model_vec4_max);
PRIVATE void _init_scalar_field              (const ogl_context_gl_entrypoints* entry_points,
                                              const ogl_context_gl_limits*      limits);
PRIVATE void _init_scalar_field_renderer     (const ogl_context_gl_entrypoints *entry_points);
PRIVATE void _init_scene                     ();
PRIVATE void _render_blob                    (ogl_context                       context,
                                              const void*                       user_arg,
                                              const system_matrix4x4            model_matrix,
                                              const system_matrix4x4            vp_matrix,
                                              const system_matrix4x4            normal_matrix,
                                              bool                              is_depth_prepass);
PRIVATE void _rendering_handler              (ogl_context                       context,
                                              uint32_t                          n_frames_rendered,
                                              system_time                       frame_time,
                                              const int*                        rendering_area_px_topdown,
                                              void*                             renderer);
PRIVATE bool _rendering_rbm_callback_handler (system_window                     window,
                                              unsigned short                    x,
                                              unsigned short                    y,
                                              system_window_vk_status           new_status,
                                              void*);
PRIVATE void _window_closed_callback_handler (system_window                     window);
PRIVATE void _window_closing_callback_handler(system_window                     window);


/** TODO */
PRIVATE void _get_blob_bounding_box_aabb(const void* user_arg,
                                         float*      out_aabb_model_vec3_min,
                                         float*      out_aabb_model_vec3_max)
{
    out_aabb_model_vec3_max[0] = 1.0f;
    out_aabb_model_vec3_max[1] = 1.0f;
    out_aabb_model_vec3_max[2] = 1.0f;
    out_aabb_model_vec3_min[0] = 0.0f;
    out_aabb_model_vec3_min[1] = 0.0f;
    out_aabb_model_vec3_min[2] = 0.0f;

}

/** TODO */
PRIVATE void _init_scalar_field(const ogl_context_gl_entrypoints* entrypoints_ptr,
                                const ogl_context_gl_limits*      limits_ptr)
{
    ogl_buffers buffers          = NULL;
    ogl_shader  cs               = NULL;
    const char* cs_body_template = "#version 430 core\n"
                                   "\n"
                                   "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = LOCAL_WG_SIZE_Y, local_size_z = LOCAL_WG_SIZE_Z) in;\n"
                                   "\n"
                                   "layout(std430) writeonly buffer data\n"
                                   "{\n"
                                   /* TODO: SIMDify me */
                                   "    restrict float result[];\n"
                                   "};\n"
                                   "\n"
                                   /* TODO: Uniformify me */
                                   "const uvec3 blob_size             = uvec3(BLOB_SIZE_X,      BLOB_SIZE_Y,      BLOB_SIZE_Z);\n"
                                   "const uvec3 global_workgroup_size = uvec3(GLOBAL_WG_SIZE_X, GLOBAL_WG_SIZE_Y, GLOBAL_WG_SIZE_Z);\n"
                                   "\n"
                                   "const struct\n"
                                   "{\n"
                                   "    vec3  xyz3;\n"
                                   "    float radius;\n"
                                   "} spheres[3] =\n"
                                   "{\n"
                                   "    {vec3(0.0,  0.0,  0.0), 0.25},\n"
                                   "    {vec3(0.25, 0.25, 0.5), 0.5 },\n"
                                   "    {vec3(0.75, 0.5,  0.3), 0.6 } };\n"
                                   "const uint n_spheres = 3;\n"
                                   "\n"
                                   "\n"
                                   "void main()\n"
                                   "{\n"
                                   "    const uint global_invocation_id_flat = gl_GlobalInvocationID.z * (global_workgroup_size.x * global_workgroup_size.y) +\n"
                                   "                                           gl_GlobalInvocationID.y * (global_workgroup_size.x)                           +\n"
                                   "                                           gl_GlobalInvocationID.x                                                       + \n"
                                   "                                           gl_LocalInvocationIndex;\n"
                                   "    const uint blob_x =  global_invocation_id_flat                                % blob_size.x;\n"
                                   "    const uint blob_y = (global_invocation_id_flat /  blob_size.x)                % blob_size.y;\n"
                                   "    const uint blob_z = (global_invocation_id_flat / (blob_size.x * blob_size.y));\n"
                                   "\n"
                                   "    if (blob_z >= blob_size.z)\n"
                                   "    {\n"
                                   "        return;\n"
                                   "    }\n"
                                   "\n"
                                   "    const float blob_x_normalized = float(blob_x) / float(blob_size.x - 1);\n"
                                   "    const float blob_y_normalized = float(blob_y) / float(blob_size.y - 1);\n"
                                   "    const float blob_z_normalized = float(blob_z) / float(blob_size.z - 1);\n"
                                   "\n"
                                   "    float max_power = 0.0;\n"
                                   "\n"
                                   "    for (unsigned int n_sphere = 0;\n"
                                   "                      n_sphere < n_spheres;\n"
                                   "                    ++n_sphere)\n"
                                   "    {\n"
                                   "        float delta_x = blob_x_normalized - spheres[n_sphere].xyz3[0];\n"
                                   "        float delta_y = blob_y_normalized - spheres[n_sphere].xyz3[1];\n"
                                   "        float delta_z = blob_z_normalized - spheres[n_sphere].xyz3[2];\n"
                                   "\n"
                                   "        float power = sqrt(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);\n"
                                   "\n"
                                   "        max_power = max(max_power, power);\n"
                                   "    }\n"
                                   "\n"
                                   "    result[global_invocation_id_flat] = max_power;\n"
                                   "}";

    system_hashed_ansi_string token_key_array[] =
    {
        system_hashed_ansi_string_create("LOCAL_WG_SIZE_X"),
        system_hashed_ansi_string_create("LOCAL_WG_SIZE_Y"),
        system_hashed_ansi_string_create("LOCAL_WG_SIZE_Z"),
        system_hashed_ansi_string_create("BLOB_SIZE_X"),
        system_hashed_ansi_string_create("BLOB_SIZE_Y"),
        system_hashed_ansi_string_create("BLOB_SIZE_Z"),
        system_hashed_ansi_string_create("GLOBAL_WG_SIZE_X"),
        system_hashed_ansi_string_create("GLOBAL_WG_SIZE_Y"),
        system_hashed_ansi_string_create("GLOBAL_WG_SIZE_Z"),
    };
    system_hashed_ansi_string token_value_array[] =
    {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    };
    const uint32_t n_token_key_values = 9;

    /* Compute global work-group size */
    const uint32_t n_total_scalars  = _blob_size[0] * _blob_size[1] * _blob_size[2];
    const uint32_t wg_local_size_x  = limits_ptr->max_compute_work_group_size[0]; /* TODO: smarterize me */
    const uint32_t wg_local_size_y  = 1;
    const uint32_t wg_local_size_z  = 1;
    const uint32_t wg_global_size_x = n_total_scalars / wg_local_size_x;
    const uint32_t wg_global_size_y = 1;
    const uint32_t wg_global_size_z = 1;

    ASSERT_DEBUG_SYNC(wg_local_size_x * wg_local_size_y * wg_local_size_z <= limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(wg_global_size_x < limits_ptr->max_compute_work_group_count[0] &&
                      wg_global_size_y < limits_ptr->max_compute_work_group_count[1] &&
                      wg_global_size_z < limits_ptr->max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    /* Fill the token value array */
    char temp_buffer[64];

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_x);
    token_value_array[0] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_y);
    token_value_array[1] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_z);
    token_value_array[2] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[0]);
    token_value_array[3] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[1]);
    token_value_array[4] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[2]);
    token_value_array[5] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_global_size_x);
    token_value_array[6] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_global_size_y);
    token_value_array[7] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_global_size_z);
    token_value_array[8] = system_hashed_ansi_string_create(temp_buffer);

    /* Create program & shader objects */
    cs = ogl_shader_create(_context,
                           SHADER_TYPE_COMPUTE,
                           system_hashed_ansi_string_create("Scalar field generator") );

    _po_scalar_field_generator = ogl_program_create(_context,
                                                    system_hashed_ansi_string_create("Scalar field generator"),
                                                    OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE);

    /* Configure the shader object */
    ogl_shader_set_body_with_token_replacement(cs,
                                               cs_body_template,
                                               n_token_key_values,
                                               token_key_array,
                                               token_value_array);

    /* Configure & link the program object */
    ogl_program_attach_shader(_po_scalar_field_generator,
                              cs);

    ogl_program_link(_po_scalar_field_generator);

    /* Prepare a BO which is going to hold the scalar field data */
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);

    _blob_scalar_field_bo_size = sizeof(float) * _blob_size[0] * _blob_size[1] * _blob_size[2];

    ogl_buffers_allocate_buffer_memory(buffers,
                                       _blob_scalar_field_bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                       0, /* flags */
                                      &_blob_scalar_field_bo_id,
                                      &_blob_scalar_field_bo_start_offset);

    /* Run the CS and generate the scalar field data */
    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_po_scalar_field_generator) );
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        _blob_scalar_field_bo_id,
                                        _blob_scalar_field_bo_start_offset,
                                        _blob_scalar_field_bo_size);

    entrypoints_ptr->pGLDispatchCompute(wg_global_size_x,
                                        wg_global_size_y,
                                        wg_global_size_z);

    /* All done! */
    ogl_shader_release(cs);
}

/** TODO */
PRIVATE void _init_scalar_field_renderer(const ogl_context_gl_entrypoints* entry_points)
{
    const char* fs_body = "#version 430 core\n"
                          "\n"
                          "out vec4 result;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    result = vec4(1.0);\n"
                          "}\n";
    const char* gs_body = "#version 430 core\n"                     /* TODO: Debug version for now */
                          "\n"
                          "layout(points)                   in;\n"
                          "layout(points, max_vertices = 1) out;\n"
                          "\n"
                          "in vec4  cube_aabb_max[];\n"
                          "in vec4  cube_aabb_min[];\n"
                          "in float scalar_value [];\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = cube_aabb_min[0];\n"
                          "    EmitVertex();\n"
                          "}\n";
    const char* vs_body = "#version 430 core\n"
                          "\n"
                          "layout(std430) readonly buffer data\n"
                          "{\n"
                          "    restrict vec4 scalar_field[];\n"
                          "};\n"
                          "\n"
                          "out vec4  cube_aabb_max;\n"
                          "out vec4  cube_aabb_min;\n"
                          "out float scalar_value;\n"
                          "\n"
                          "uniform dataUB\n"
                          "{\n"
                          "    mat4 model;\n"
                          "    mat4 vp;\n"
                          "};\n"
                          "\n"
                          "const uvec3 blob_size = uvec3(BLOB_SIZE_X, BLOB_SIZE_Y, BLOB_SIZE_Z);\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    const uint blob_x =  gl_VertexID                                % blob_size.x;\n"
                          "    const uint blob_y = (gl_VertexID /  blob_size.x)                % blob_size.y;\n"
                          "    const uint blob_z = (gl_VertexID / (blob_size.x * blob_size.y));\n"
                          "\n"
                          "    const vec3 cube_center = vec3((float(blob_x) + 0.5) / float(blob_size.x),\n"
                          "                                  (float(blob_y) + 0.5) / float(blob_size.y),\n"
                          "                                  (float(blob_z) + 0.5) / float(blob_size.z) );\n"
                          "    const vec3 cube_size   = vec3(1.0) / vec3(blob_size);\n"
                          "\n"
                          "    cube_aabb_max = vp * model * vec4(cube_center + cube_size / vec3(2.0), 1.0);\n"
                          "    cube_aabb_min = vp * model * vec4(cube_center - cube_size / vec3(2.0), 1.0);\n"
                          "    scalar_value  = scalar_field[gl_VertexID / 4][gl_VertexID % 4];\n"
                          "}\n";

    /* Set up vertex body token key/value arrays */
    char                            temp_buffer[256];
    const system_hashed_ansi_string vs_body_token_keys[] =
    {
        system_hashed_ansi_string_create("BLOB_SIZE_X"),
        system_hashed_ansi_string_create("BLOB_SIZE_Y"),
        system_hashed_ansi_string_create("BLOB_SIZE_Z")
    };
    system_hashed_ansi_string       vs_body_token_values[] =
    {
        NULL,
        NULL,
        NULL
    };
    const uint32_t n_vs_body_tokens = sizeof(vs_body_token_keys) / sizeof(vs_body_token_keys[0]);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[0]);
    vs_body_token_values[0] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[1]);
    vs_body_token_values[1] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[2]);
    vs_body_token_values[2] = system_hashed_ansi_string_create(temp_buffer);

    /* Initialize the shaders */
    ogl_shader fs = ogl_shader_create(_context,
                                      SHADER_TYPE_FRAGMENT,
                                      system_hashed_ansi_string_create("Scalar field renderer FS"));
    ogl_shader gs = ogl_shader_create(_context,
                                      SHADER_TYPE_GEOMETRY,
                                      system_hashed_ansi_string_create("Scalar field renderer GS"));
    ogl_shader vs = ogl_shader_create(_context,
                                      SHADER_TYPE_VERTEX,
                                      system_hashed_ansi_string_create("Scalar field renderer VS"));

    ogl_shader_set_body                       (fs,
                                               system_hashed_ansi_string_create(fs_body) );
    ogl_shader_set_body                       (gs,
                                               system_hashed_ansi_string_create(gs_body) );
    ogl_shader_set_body_with_token_replacement(vs,
                                               vs_body,
                                               n_vs_body_tokens,
                                               vs_body_token_keys,
                                               vs_body_token_values);

    /* Prepare & link the program object */
    _po_scalar_field_renderer = ogl_program_create(_context,
                                                   system_hashed_ansi_string_create("Scalar field renderer"),
                                                   OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_po_scalar_field_renderer,
                              fs);
    ogl_program_attach_shader(_po_scalar_field_renderer,
                              gs);
    ogl_program_attach_shader(_po_scalar_field_renderer,
                              vs);

    ogl_program_link(_po_scalar_field_renderer);

    /* Retrieve data UB properties */
    const ogl_program_variable* uniform_model;
    const ogl_program_variable* uniform_vp;

    ogl_program_get_uniform_block_by_name(_po_scalar_field_renderer,
                                          system_hashed_ansi_string_create("dataUB"),
                                         &_po_scalar_field_renderer_data_ub);

    ogl_program_ub_get_property(_po_scalar_field_renderer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &_po_scalar_field_renderer_data_ub_bo_id);
    ogl_program_ub_get_property(_po_scalar_field_renderer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &_po_scalar_field_renderer_data_ub_bo_start_offset);

    ogl_program_get_uniform_by_name(_po_scalar_field_renderer,
                                    system_hashed_ansi_string_create("model"),
                                   &uniform_model);
    ogl_program_get_uniform_by_name(_po_scalar_field_renderer,
                                    system_hashed_ansi_string_create("vp"),
                                   &uniform_vp);

    _po_scalar_field_renderer_data_ub_model_offset = uniform_model->block_offset;
    _po_scalar_field_renderer_data_ub_vp_offset    = uniform_vp->block_offset;

    /* Release stuff */
    ogl_shader_release(fs);
    ogl_shader_release(gs);
    ogl_shader_release(vs);
}

/** TODO */
PRIVATE void _init_scene()
{
    scene_mesh blob_scene_mesh = NULL;

    /* Set up the test mesh instance */
    _blob_mesh = mesh_create_custom_mesh(&_render_blob,
                                          NULL, /* render_custom_mesh_proc_user_arg */
                                         &_get_blob_bounding_box_aabb,
                                          NULL, /* get_custom_mesh_aabb_proc_user_arg */
                                          system_hashed_ansi_string_create("Test blob") );

    /* Set up the scene */
    system_hashed_ansi_string blob_has       = system_hashed_ansi_string_create("Test blob");
    system_hashed_ansi_string camera_has     = system_hashed_ansi_string_create("Test camera");
    system_hashed_ansi_string test_scene_has = system_hashed_ansi_string_create("Test scene");

    _scene        = scene_create       (_context,
                                        test_scene_has);
    _scene_camera = scene_camera_create(camera_has,
                                        test_scene_has);

    scene_get_property(_scene,
                       SCENE_PROPERTY_GRAPH,
                      &_scene_graph);

    scene_add_mesh_instance(_scene,
                            _blob_mesh,
                            blob_has,
                           &blob_scene_mesh);

    /* Set up the scene graph */
    system_matrix4x4 identity_matrix = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(identity_matrix);

    _scene_blob_node   = scene_graph_create_general_node                        (_scene_graph);
    _scene_camera_node = scene_graph_create_static_matrix4x4_transformation_node(_scene_graph,
                                                                                 identity_matrix,
                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

    system_matrix4x4_release(identity_matrix);
    identity_matrix = NULL;

    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_blob_node,
                                      SCENE_OBJECT_TYPE_MESH,
                                      blob_scene_mesh);
    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_camera_node,
                                      SCENE_OBJECT_TYPE_CAMERA,
                                      _scene_camera);

    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_camera_node);
    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_blob_node);

    /* Set up the scene renderer */
    _scene_renderer = ogl_scene_renderer_create(_context,
                                                _scene);
}

/** TODO */
PRIVATE void _render_blob(ogl_context             context,
                          const void*             user_arg,
                          const system_matrix4x4  model_matrix,
                          const system_matrix4x4  vp_matrix,
                          const system_matrix4x4  normal_matrix,
                          bool                    is_depth_prepass)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    /* Set up the unifrom block contents */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    ogl_program_ub_set_nonarrayed_uniform_value(_po_scalar_field_renderer_data_ub,
                                                _po_scalar_field_renderer_data_ub_model_offset,
                                                system_matrix4x4_get_column_major_data(model_matrix),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_set_nonarrayed_uniform_value(_po_scalar_field_renderer_data_ub,
                                                _po_scalar_field_renderer_data_ub_vp_offset,
                                                system_matrix4x4_get_column_major_data(vp_matrix),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_sync                        (_po_scalar_field_renderer_data_ub);

    /* Set up the SSBO & UBO bindings */
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        _blob_scalar_field_bo_id,
                                        _blob_scalar_field_bo_start_offset,
                                        _blob_scalar_field_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        _po_scalar_field_renderer_data_ub_bo_id,
                                        _po_scalar_field_renderer_data_ub_bo_start_offset,
                                        sizeof(float) * 32);

    /* Draw stuff */
    GLuint zero_vaas_vao_id = 0;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &zero_vaas_vao_id);

    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_po_scalar_field_renderer) );
    entrypoints_ptr->pGLBindVertexArray(zero_vaas_vao_id);
    entrypoints_ptr->pGLDrawArrays     (GL_POINTS,
                                        0, /* first */
                                        _blob_size[0] * _blob_size[1] * _blob_size[2]);
}

/** TODO */
PRIVATE void _rendering_handler(ogl_context context,
                                uint32_t    n_frames_rendered,
                                system_time frame_time,
                                const int*  rendering_area_px_topdown,
                                void*       renderer)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    const ogl_context_gl_limits*      limits_ptr      = NULL;
    static bool                       is_initialized  = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* If this is the first frame we're rendering, initialize various stuff. */
    if (!is_initialized)
    {
        /* Initialize scene stuff */
        _init_scalar_field         (entrypoints_ptr,
                                    limits_ptr);
        _init_scalar_field_renderer(entrypoints_ptr);
        _init_scene                ();

        /* Initialize projection & view matrices */
        _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f,  /* fov_y */
                                                                                   float(_window_size[0]) / float(_window_size[1]),
                                                                                   0.01f,  /* z_near */
                                                                                   10.0f); /* z_far  */
        _view_matrix       = system_matrix4x4_create                              ();

        system_matrix4x4_set_to_identity(_view_matrix);

        /* Initialize the flyby */
        static const bool is_flyby_active = true;

        ogl_context_get_property(_context,
                                 OGL_CONTEXT_PROPERTY_FLYBY,
                                &_context_flyby);
        ogl_flyby_set_property  (_context_flyby,
                                 OGL_FLYBY_PROPERTY_IS_ACTIVE,
                                &is_flyby_active);

        /* All done */
        is_initialized = true;
    } /* if (!is_initialized) */

    /* Update the flyby */
    ogl_flyby_update(_context_flyby);

    /* Render the scene */
    ogl_flyby_get_property(_context_flyby,
                           OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                          &_view_matrix);

    ogl_scene_renderer_render_scene_graph(_scene_renderer,
                                          _view_matrix,
                                          _projection_matrix,
                                          _scene_camera,
                                          RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
                                          false, /* apply_shadow_mapping */
                                          HELPER_VISUALIZATION_BOUNDING_BOXES,
                                          frame_time);
}

/** Event callback handlers */
PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window)
{
    if (_blob_mesh != NULL)
    {
        mesh_release(_blob_mesh);

        _blob_mesh = NULL;
    }

    if (_po_scalar_field_generator != NULL)
    {
        ogl_program_release(_po_scalar_field_generator);

        _po_scalar_field_generator = NULL;
    }

    if (_po_scalar_field_renderer != NULL)
    {
        ogl_program_release(_po_scalar_field_renderer);

        _po_scalar_field_renderer = NULL;
    }

    if (_projection_matrix != NULL)
    {
        system_matrix4x4_release(_projection_matrix);

        _projection_matrix = NULL;
    }

    if (_scene != NULL)
    {
        scene_release(_scene);

        _scene = NULL;
    }

    if (_scene_camera != NULL)
    {
        scene_camera_release(_scene_camera);

        _scene_camera = NULL;
    }

    if (_scene_renderer != NULL)
    {
        ogl_scene_renderer_release(_scene_renderer);

        _scene_renderer = NULL;
    }

    if (_view_matrix != NULL)
    {
        system_matrix4x4_release(_view_matrix);

        _view_matrix = NULL;
    }

    /* All done */
    system_event_set(_window_closed_event);
}


/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    system_screen_mode    screen_mode              = NULL;
    system_window         window                   = NULL;
    system_pixel_format   window_pf                = NULL;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                           0); /* stencil_buffer_bits     */

#if 0
    window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                             screen_mode,
                                             true, /* vsync_enabled */
                                             window_pf);
#else
    _window_size[0] /= 2;
    _window_size[1] /= 2;

    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Test window"),
                                                 false, /* scalable */
                                                 true,  /* vsync_enabled */
                                                 true,  /* visible */
                                                 window_pf);
#endif

    /* Set up the rendering contxt */
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,
                                                                            _rendering_handler,
                                                                            NULL); /* user_arg */

    /* Kick off the rendering process */
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    /* Set up mouse click & window tear-down callbacks */
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    /* Launch the rendering process and wait until the window is closed. */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    system_window_close (window);
    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}