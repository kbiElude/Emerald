/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_frustum_preview.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include "varia/varia_text_renderer.h"
#include <string>
#include <sstream>

/*
 * Data BO structure:
 *
 *
 *  FTL--------FTR            FTL: far  top-left     corner (index: 6)
 *  |\         /|             FTR: far  top-right    corner (index: 7)
 *  | \       / |             FBL: far  bottom-left  corner (index: 5)
 *  |  NTL--NTR |             FBR: far  bottom-right corner (index: 8)
 *  FBL-|\--/|-FBR            NBL: near bottom-left  corner (index: 1)
 *   \  | OO | /              NBR: near bottom-right corner (index: 4)
 *    \ |/  \|/               NTL: near top-left     corner (index: 2)
 *     NBL--NBR               NTR: near top-right    corner (index: 3)
 *                            OO:  origin                   (index: 0)
 *
 * n UBYTE indices describing index data for the frustum.
 * n_cameras * 9 vertices (4 comps each)
 * n_lights  * 9 vertices (4 comps each)
 *
 * This implementation does not use varia_primitive_renderer because
 * it does not support multi draw base vertex calls (not at the moment, at least!),
 * which we can leverage for rendering of multiple frustums.
 */
#define PRIMITIVE_RESTART_TERMINATOR_INDEX (255)

PRIVATE const unsigned char index_data_array[] =
{
    /* side planes */
    0, 1, 5, 6, 2,
    0, 3, 7, 8, 4, 0, PRIMITIVE_RESTART_TERMINATOR_INDEX,

    1, 2, 3, 4, 1,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* near plane */
    5, 6, 7, 8, 5,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* far  plane */

    4, 1, 5, 8, 4,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* bottom plane */
    2, 6, 7, 3, 2,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* top    plane */
};

#define BO_DATA_INDEX_FTL    (6)
#define BO_DATA_INDEX_FTR    (7)
#define BO_DATA_INDEX_FBL    (5)
#define BO_DATA_INDEX_FBR    (8)
#define BO_DATA_INDEX_NBL    (1)
#define BO_DATA_INDEX_NBR    (4)
#define BO_DATA_INDEX_NTL    (2)
#define BO_DATA_INDEX_NTR    (3)
#define BO_DATA_INDEX_ORIGIN (0)
#define BO_DATA_INDEX_MAX    (8)

#define BO_DATA_INDEX_DATA_OFFSET          (0)
#define BO_DATA_VERTEX_DATA_OFFSET         (sizeof(index_data_array) )
#define N_BYTES_PER_BO_DATA_ENTRY          (9 * sizeof(float) * 4)

/** TODO */
typedef struct _scene_renderer_frustum_preview_camera
{
    scene_camera                       camera;
    varia_text_renderer_text_string_id camera_text_id;
    varia_text_renderer                text_renderer;

    explicit _scene_renderer_frustum_preview_camera(scene_camera                       in_camera,
                                                    varia_text_renderer                in_text_renderer,
                                                    varia_text_renderer_text_string_id in_camera_text_id)
    {
        camera         = in_camera;
        camera_text_id = in_camera_text_id;
        text_renderer  = in_text_renderer;

        scene_camera_retain(camera);
    }

    ~_scene_renderer_frustum_preview_camera()
    {
        if (camera != nullptr)
        {
            varia_text_renderer_delete_string(text_renderer,
                                              camera_text_id);

            scene_camera_release(camera);

            camera = nullptr;
        }
    }
} _scene_renderer_frustum_preview_camera;

typedef struct _scene_renderer_frustum_preview
{
    /* DO NOT retain/release ogl_context copy, as this object is managed
     * by ogl_context and retaining it will cause the rendering context
     * to never release itself.
     *
     */
    system_resizable_vector assigned_cameras; /* holds _scene_renderer_frustum_preview_camera instances */

    ral_command_buffer       command_buffer;
    ral_context              context;
    unsigned char*           data_bo_buffer;
    system_time              data_bo_buffer_last_update_time;
    unsigned int             data_bo_buffer_size;
    ral_buffer               data_bo;
    unsigned int             data_bo_size;
    ral_gfx_state            gfx_state;
    scene                    owned_scene;
    ral_program              po;
    ral_program_block_buffer po_ub;
    uint32_t                 po_vp_ub_offset;
    varia_text_renderer      text_renderer;  /* TODO: use a global text renderer */

    /* The following arrays are used for the multi draw-call. The number of elements
     * is equal to the total number of frustrums to be rendered. */
    unsigned int mdebv_array_size;
    uint32_t*    mdebv_basevertex_array;

    /* Render / CPU callback present task data cache */
    ral_texture_view render_color_rt;
    ral_texture_view render_depth_rt;
    system_time      render_time;
    system_matrix4x4 render_vp;

    _scene_renderer_frustum_preview()
    {
        assigned_cameras                = system_resizable_vector_create(4 /* capacity */);
        command_buffer                  = nullptr;
        context                         = nullptr;
        data_bo_buffer                  = nullptr;
        data_bo_buffer_size             = 0;
        data_bo_buffer_last_update_time = -1;
        data_bo                         = nullptr;
        data_bo_size                    = 0;
        gfx_state                       = nullptr;
        mdebv_array_size                = 0;
        mdebv_basevertex_array          = nullptr;
        owned_scene                     = nullptr;
        po                              = nullptr;
        po_vp_ub_offset                 = -1;
        render_color_rt                 = nullptr;
        render_depth_rt                 = nullptr;
        render_time                     = -1;
        render_vp                       = system_matrix4x4_create();
        text_renderer                   = nullptr;
    }

    ~_scene_renderer_frustum_preview()
    {
        ASSERT_DEBUG_SYNC(data_bo == nullptr,
                          "Data BO not released at the time scene_renderer_frustum_preview destructor was called.");

        if (assigned_cameras != nullptr)
        {
            _scene_renderer_frustum_preview_camera* current_camera_ptr = nullptr;

            while (system_resizable_vector_pop(assigned_cameras,
                                              &current_camera_ptr) )
            {
                delete current_camera_ptr;

                current_camera_ptr = nullptr;
            }

            system_resizable_vector_release(assigned_cameras);
            assigned_cameras = nullptr;
        }

        if (data_bo_buffer != nullptr)
        {
            delete [] data_bo_buffer;

            data_bo_buffer = nullptr;
        }

        if (gfx_state != nullptr)
        {
            ral_gfx_state_release(gfx_state);

            gfx_state = nullptr;
        }

        if (mdebv_basevertex_array != nullptr)
        {
            delete [] mdebv_basevertex_array;

            mdebv_basevertex_array = nullptr;
        }

        if (po != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&po) );

            po = nullptr;
        }

        if (render_vp != nullptr)
        {
            system_matrix4x4_release(render_vp);

            render_vp = nullptr;
        }

        if (text_renderer != nullptr)
        {
            varia_text_renderer_release(text_renderer);

            text_renderer = nullptr;
        }
    }
} _scene_renderer_frustum_preview;


PRIVATE const char* po_fs =
    "#version 430 core\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "}\n";
PRIVATE const char* po_vs =
    "#version 430 core\n"
    "\n"
    "in vec4 position;\n"
    "\n"
    "uniform dataVS\n"
    "{\n"
    "    mat4 vp;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vp * position;\n"
    "}\n";


/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _scene_renderer_frustum_preview_verify_context_type(ogl_context);
#else
    #define _scene_renderer_frustum_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _scene_renderer_frustum_preview_bake_command_buffer(_scene_renderer_frustum_preview* preview_ptr)
{
    if (preview_ptr->command_buffer == nullptr)
    {
        ral_command_buffer_create_info cmd_buffer_create_info;

        cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        cmd_buffer_create_info.is_executable                           = true;
        cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
        cmd_buffer_create_info.is_resettable                           = true;
        cmd_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(preview_ptr->context,
                                           1, /* n_command_buffers */
                                           &cmd_buffer_create_info,
                                           &preview_ptr->command_buffer);
    }

    ral_command_buffer_start_recording(preview_ptr->command_buffer);
    {
        ral_command_buffer_set_color_rendertarget_command_info color_rt      = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        ral_buffer                                             po_ub_bo_ral  = nullptr;
        uint32_t                                               po_ub_bo_size = 0;
        ral_command_buffer_set_binding_command_info            ub_binding;
        ral_command_buffer_set_vertex_buffer_command_info      vertex_buffer;

        ral_program_block_buffer_get_property(preview_ptr->po_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &po_ub_bo_ral);
        ral_buffer_get_property              (po_ub_bo_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &po_ub_bo_size);

        color_rt.rendertarget_index = 0;
        color_rt.texture_view       = preview_ptr->render_color_rt;

        ub_binding.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub_binding.name                          = system_hashed_ansi_string_create("dataVS");
        ub_binding.uniform_buffer_binding.buffer = po_ub_bo_ral;
        ub_binding.uniform_buffer_binding.offset = 0;
        ub_binding.uniform_buffer_binding.size   = po_ub_bo_size;

        vertex_buffer.buffer       = preview_ptr->data_bo;
        vertex_buffer.name         = system_hashed_ansi_string_create("position");
        vertex_buffer.start_offset = BO_DATA_VERTEX_DATA_OFFSET;


        ral_command_buffer_record_set_bindings           (preview_ptr->command_buffer,
                                                          1, /* n_bindings */
                                                         &ub_binding);
        ral_command_buffer_record_set_color_rendertargets(preview_ptr->command_buffer,
                                                          1, /* n_rendertargets */
                                                         &color_rt);
        ral_command_buffer_record_set_gfx_state          (preview_ptr->command_buffer,
                                                          preview_ptr->gfx_state);
        ral_command_buffer_record_set_program            (preview_ptr->command_buffer,
                                                          preview_ptr->po);
        ral_command_buffer_record_set_vertex_buffers     (preview_ptr->command_buffer,
                                                          1, /* n_vertex_buffers */
                                                         &vertex_buffer);

        if (preview_ptr->render_depth_rt != nullptr)
        {
            ral_command_buffer_record_set_depth_rendertarget(preview_ptr->command_buffer,
                                                             preview_ptr->render_depth_rt);
        }

        /* TODO: This should be a multi-draw call */
        for (uint32_t n_draw_call = 0;
                      n_draw_call < preview_ptr->mdebv_array_size;
                    ++n_draw_call)
        {
            ral_command_buffer_draw_call_indexed_command_info draw_call_info;

            draw_call_info.base_instance = 0;
            draw_call_info.base_vertex   = preview_ptr->mdebv_basevertex_array[n_draw_call];
            draw_call_info.first_index   = 0;
            draw_call_info.index_buffer  = preview_ptr->data_bo;
            draw_call_info.index_type    = RAL_INDEX_TYPE_8BIT;
            draw_call_info.n_indices     = sizeof(index_data_array) / sizeof(index_data_array[0]);
            draw_call_info.n_instances   = 1;

            ral_command_buffer_record_draw_call_indexed(preview_ptr->command_buffer,
                                                        1, /* n_draw_calls */
                                                       &draw_call_info);
        }
    }
    ral_command_buffer_stop_recording(preview_ptr->command_buffer);
}

/** TODO */
PRIVATE void _scene_renderer_frustum_preview_bake_gfx_state(_scene_renderer_frustum_preview* preview_ptr,
                                                            ral_texture_view                 color_rt,
                                                            ral_texture_view                 opt_depth_rt)
{
    ral_gfx_state_vertex_attribute                  data_va;
    ral_gfx_state_create_info                       gfx_state_create_info;
    uint32_t                                        rt_size[2];
    ral_command_buffer_set_scissor_box_command_info scissor_box;
    ral_command_buffer_set_viewport_command_info    viewport;

    ral_texture_view_get_mipmap_property(color_rt,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                         rt_size + 0);
    ral_texture_view_get_mipmap_property(color_rt,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                         rt_size + 1);

    if (preview_ptr->gfx_state != nullptr)
    {
        ral_gfx_state_release(preview_ptr->gfx_state);

        preview_ptr->gfx_state = nullptr;
    }

    data_va.format = RAL_FORMAT_RGBA32_FLOAT;
    data_va.input_rate = RAL_VERTEX_INPUT_RATE_PER_VERTEX;
    data_va.name       = system_hashed_ansi_string_create("position");
    data_va.offset     = BO_DATA_VERTEX_DATA_OFFSET;
    data_va.stride     = sizeof(float) * 4;

    scissor_box.index   = 0;
    scissor_box.size[0] = rt_size[0];
    scissor_box.size[1] = rt_size[1];
    scissor_box.xy  [0] = 0;
    scissor_box.xy  [1] = 0;

    viewport.depth_range[0] = 0.0f;
    viewport.depth_range[1] = 1.0f;
    viewport.index          = 0;
    viewport.size[0]        = static_cast<float>(rt_size[0]);
    viewport.size[1]        = static_cast<float>(rt_size[1]);
    viewport.xy  [0]        = 0;
    viewport.xy  [1]        = 0;

    gfx_state_create_info.depth_test                           = (opt_depth_rt != nullptr);
    gfx_state_create_info.depth_test_compare_op                = RAL_COMPARE_OP_LESS;
    gfx_state_create_info.line_width                           = 2.0f;
    gfx_state_create_info.n_vertex_attributes                  = 1;
    gfx_state_create_info.primitive_restart                    = true;
    gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_LINE_STRIP;
    gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
    gfx_state_create_info.static_scissor_boxes                 = &scissor_box;
    gfx_state_create_info.static_scissor_boxes_enabled         = true;
    gfx_state_create_info.static_viewports                     = &viewport;
    gfx_state_create_info.static_viewports_enabled             = true;
    gfx_state_create_info.vertex_attribute_ptrs                = &data_va;

    preview_ptr->gfx_state = ral_gfx_state_create(preview_ptr->context,
                                                 &gfx_state_create_info);
}

/** TODO */
PRIVATE void _scene_renderer_frustum_preview_init(_scene_renderer_frustum_preview* preview_ptr,
                                                  const uint32_t*                  viewport_size_xy_px)
{
    static const char*        limiter             = "/";
    system_hashed_ansi_string final_renderer_name = nullptr;

    /* Create text renderer instance name */
    system_hashed_ansi_string scene_name = nullptr;

    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    
    const char* final_renderer_name_parts[] =
    {
        system_hashed_ansi_string_get_buffer(scene_name),
        limiter,
        "frustum preview text renderer"
    };
    const uint32_t n_final_renderer_name_parts = sizeof(final_renderer_name_parts) /
                                                 sizeof(final_renderer_name_parts[0]);


    final_renderer_name = system_hashed_ansi_string_create_by_merging_strings(n_final_renderer_name_parts,
                                                                              final_renderer_name_parts);

    /* Initialize the text renderer */
    const float text_color[4] = {1, 0, 0, 1};
    const float text_scale    = 0.5f;

    preview_ptr->text_renderer = varia_text_renderer_create(final_renderer_name,
                                                            preview_ptr->context,
                                                            system_resources_get_meiryo_font_table(),
                                                            viewport_size_xy_px[0],
                                                            viewport_size_xy_px[1]);

    varia_text_renderer_set_text_string_property(preview_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                 text_color);
    varia_text_renderer_set_text_string_property(preview_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                &text_scale);

    /* Is the PO already registered? */
    static const char* po_name = "Frustum preview renderer program";

    preview_ptr->po = ral_context_get_program_by_name(preview_ptr->context,
                                                      system_hashed_ansi_string_create(po_name));

    if (preview_ptr->po != nullptr)
    {
        ral_context_retain_object(preview_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  preview_ptr->po);
    }
    else
    {
        /* Set up the PO */
        ral_shader                      fs      = nullptr;
        const system_hashed_ansi_string fs_body = system_hashed_ansi_string_create(po_fs);
        ral_shader                      vs      = nullptr;
        const system_hashed_ansi_string vs_body = system_hashed_ansi_string_create(po_vs);

        const ral_shader_create_info fs_create_info =
        {
            system_hashed_ansi_string_create("Frustum preview renderer FS"),
            RAL_SHADER_TYPE_FRAGMENT
        };
        const ral_shader_create_info vs_create_info =
        {
            system_hashed_ansi_string_create("Frustum preview renderer VS"),
            RAL_SHADER_TYPE_VERTEX
        };

        const ral_program_create_info program_create_info =
        {
            RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
            system_hashed_ansi_string_create(po_name)
        };

        const ral_shader_create_info shader_create_info_items[] =
        {
            fs_create_info,
            vs_create_info
        };
        const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);

        ral_shader result_shaders[n_shader_create_info_items];


        if (!ral_context_create_shaders(preview_ptr->context,
                                        n_shader_create_info_items,
                                        shader_create_info_items,
                                        result_shaders) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL shader creation failed");
        }

        fs = result_shaders[0];
        vs = result_shaders[1];

        if (!ral_context_create_programs(preview_ptr->context,
                                         1, /* n_create_info_items */
                                        &program_create_info,
                                        &preview_ptr->po) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL program creation failed");
        }


        ral_shader_set_property(fs,
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &fs_body);
        ral_shader_set_property(vs,
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &vs_body);

        if (!ral_program_attach_shader(preview_ptr->po,
                                       fs,
                                       true /* async */) ||
            !ral_program_attach_shader(preview_ptr->po,
                                       vs,
                                       true /* async */) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL program configuration failed.");
        }


        ral_shader shaders_to_release[] =
        {
            fs,
            vs
        };
        const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   n_shaders_to_release,
                                   reinterpret_cast<void* const*>(shaders_to_release) );
    }

    /* Retrieve PO UB details */
    preview_ptr->po_ub = ral_program_block_buffer_create(preview_ptr->context,
                                                         preview_ptr->po,
                                                         system_hashed_ansi_string_create("dataVS") );

    /* Retrieve PO uniform locations */
    const ral_program_variable* po_vp_ral_ptr = nullptr;

    if (!ral_program_get_block_variable_by_name(preview_ptr->po,
                                                system_hashed_ansi_string_create("dataVS"),
                                                system_hashed_ansi_string_create("vp"),
                                               &po_vp_ral_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_program_get_uniform_by_name() failed.");
    }
    else
    {
        preview_ptr->po_vp_ub_offset = po_vp_ral_ptr->block_offset;

        ASSERT_DEBUG_SYNC(preview_ptr->po_vp_ub_offset != -1,
                          "VP UB offset is -1");
    }
}

/** TODO */
PRIVATE void _scene_renderer_frustum_update_data_bo_cpu_task_callback(void* preview_raw_ptr)
{
    ral_buffer_client_sourced_update_info                                data_bo_update_info;
    std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > data_bo_update_info_ptrs;
    uint32_t                                                             data_dst_offset          = 0;
    uint32_t                                                             data_src_offset          = 0;
    uint32_t                                                             data_size                = 0;
    _scene_renderer_frustum_preview*                                     preview_ptr              = reinterpret_cast<_scene_renderer_frustum_preview*>(preview_raw_ptr);

    ASSERT_DEBUG_SYNC(preview_ptr->data_bo_buffer != nullptr,
                      "Data BO buffer is nullptr");

    if (preview_ptr->data_bo_buffer_last_update_time == preview_ptr->render_time)
    {
        /* Nop */
        goto end;
    }

    /* Update index data */
    memcpy(preview_ptr->data_bo_buffer,
           index_data_array,
           sizeof(index_data_array) );

    /* Reallocate MDEBV argument space if necessary */
    uint32_t n_cameras      = 0;
    bool     realloc_needed = false;

    system_resizable_vector_get_property(preview_ptr->assigned_cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_cameras);

    if (preview_ptr->mdebv_array_size < n_cameras)
    {
        if (preview_ptr->mdebv_basevertex_array != nullptr)
        {
            delete [] preview_ptr->mdebv_basevertex_array;

            preview_ptr->mdebv_basevertex_array = nullptr;
        }

        preview_ptr->mdebv_basevertex_array = new (std::nothrow) uint32_t[n_cameras];

        ASSERT_ALWAYS_SYNC(preview_ptr->mdebv_basevertex_array != nullptr,
                           "Out of memory");

        preview_ptr->mdebv_array_size = n_cameras;
        realloc_needed                = true;
    }

    /* Iterate over all assigned cameras */
    for (uint32_t n_camera = 0;
                  n_camera < n_cameras;
                ++n_camera)
    {
        /* Retrieve transformation matrix for current camera */
        _scene_renderer_frustum_preview_camera* camera_ptr                           = nullptr;
        scene_graph_node                        current_camera_owner_graph_node      = nullptr;
        system_matrix4x4                        current_camera_transformation_matrix = nullptr;

        if (!system_resizable_vector_get_element_at(preview_ptr->assigned_cameras,
                                                    n_camera,
                                                   &camera_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve scene_camera instance at index [%d]",
                              n_camera);

            continue;
        }

        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                  0, /* time is irrelevant for the query */
                                 &current_camera_owner_graph_node);

        ASSERT_DEBUG_SYNC(current_camera_owner_graph_node != nullptr,
                          "No owner graph node assigned to the camera");

        scene_graph_node_get_property(current_camera_owner_graph_node,
                                      SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                     &current_camera_transformation_matrix);

        ASSERT_DEBUG_SYNC(current_camera_transformation_matrix != nullptr,
                          "No transformation matrix available for the camera");

        /* Compute MVP-space frustum vertex positions */
        unsigned int bo_vertex_offset  = BO_DATA_VERTEX_DATA_OFFSET + n_camera * N_BYTES_PER_BO_DATA_ENTRY;
        float*       bo_vertex_data    = reinterpret_cast<float*>(preview_ptr->data_bo_buffer + bo_vertex_offset);
        float        frustum_origin[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_fbl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_fbr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ftl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ftr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_nbl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_nbr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ntl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ntr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};

        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT,
                                  preview_ptr->render_time,
                                  frustum_fbl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT,
                                  preview_ptr->render_time,
                                  frustum_fbr);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT,
                                  preview_ptr->render_time,
                                  frustum_ftl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT,
                                  preview_ptr->render_time,
                                  frustum_ftr);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT,
                                  preview_ptr->render_time,
                                  frustum_nbl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT,
                                  preview_ptr->render_time,
                                  frustum_nbr);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT,
                                  preview_ptr->render_time,
                                  frustum_ntl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT,
                                  preview_ptr->render_time,
                                  frustum_ntr);

        system_matrix4x4_multiply_by_vector4(current_camera_transformation_matrix,
                                             frustum_origin,
                                             bo_vertex_data + 4 * BO_DATA_INDEX_ORIGIN);

        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FBL,
               frustum_fbl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FBR,
               frustum_fbr,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FTL,
               frustum_ftl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FTR,
               frustum_ftr,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NBL,
               frustum_nbl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NBR,
               frustum_nbr,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NTL,
               frustum_ntl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NTR,
               frustum_ntr,
               sizeof(float) * 4);

        /* Position the label at camera origin. Now, the origin we have calculated
         * is in world space, so we need to move it to window space before repositioning
         * the text string.
         */
        const float* origin_model_space = bo_vertex_data + 4 * BO_DATA_INDEX_ORIGIN;
              float  origin_mvp_space   [4];

        system_matrix4x4_multiply_by_vector4(preview_ptr->render_vp,
                                             origin_model_space,
                                             origin_mvp_space);

        system_math_vector_mul3_float(origin_mvp_space,
                                      1.0f / origin_mvp_space[3],
                                      origin_mvp_space);

        /* Squeeze MVP to window coords and use this as the label's position.
         * If the camera is invisible, make sure it stays hidden in window space.
         */
        if (origin_mvp_space[3] >= 0.0f)
        {
            static const bool is_visible = true;
            float             origin_window_space[2];

            origin_window_space[0] =         origin_mvp_space[0] * 0.5f + 0.5f;
            origin_window_space[1] = 1.0f - (origin_mvp_space[1] * 0.5f + 0.5f);

            varia_text_renderer_set_text_string_property(camera_ptr->text_renderer,
                                                         camera_ptr->camera_text_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                        &is_visible);
            varia_text_renderer_set_text_string_property(camera_ptr->text_renderer,
                                                         camera_ptr->camera_text_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS,
                                                         origin_window_space);
        }
        else
        {
            static const bool is_visible = false;

            varia_text_renderer_set_text_string_property(camera_ptr->text_renderer,
                                                         camera_ptr->camera_text_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                        &is_visible);
        }

        /* Update MDEBV draw call arguments */
        preview_ptr->mdebv_basevertex_array[n_camera] = (BO_DATA_INDEX_MAX + 1) * n_camera;
    }

    /* Set up the data BO */
    if (preview_ptr->data_bo != nullptr && realloc_needed ||
        preview_ptr->data_bo == nullptr)
    {
        data_size = preview_ptr->data_bo_buffer_size;
    }
    else
    {
        data_dst_offset = BO_DATA_VERTEX_DATA_OFFSET;
        data_src_offset = BO_DATA_VERTEX_DATA_OFFSET;
        data_size       = preview_ptr->data_bo_buffer_size - BO_DATA_VERTEX_DATA_OFFSET;
    }

    data_bo_update_info.data         = preview_ptr->data_bo_buffer + data_src_offset;
    data_bo_update_info.data_size    = data_size;
    data_bo_update_info.start_offset = data_dst_offset;

    data_bo_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&data_bo_update_info,
                                                                                              NullDeleter<ral_buffer_client_sourced_update_info>() ));

    ral_buffer_set_data_from_client_memory(preview_ptr->data_bo,
                                           data_bo_update_info_ptrs,
                                           false, /* async               */
                                           false  /* sync_other_contexts */);

    preview_ptr->data_bo_buffer_last_update_time = preview_ptr->render_time;

    /* Update the UB */
    ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->po_ub,
                                                           preview_ptr->po_vp_ub_offset,
                                                           system_matrix4x4_get_column_major_data(preview_ptr->render_vp),
                                                           sizeof(float) * 16);
    ral_program_block_buffer_sync_immediately             (preview_ptr->po_ub);

end:
    ;
}


/** Please see header for spec */
PUBLIC void scene_renderer_frustum_preview_assign_cameras(scene_renderer_frustum_preview preview,
                                                          unsigned int                   n_cameras,
                                                          scene_camera*                  cameras)
{
    _scene_renderer_frustum_preview* preview_ptr = reinterpret_cast<_scene_renderer_frustum_preview*>(preview);

    /* Release all cameras that may have been assigned in the past */
    {
        _scene_renderer_frustum_preview_camera* current_camera_ptr = nullptr;

        while (system_resizable_vector_pop(preview_ptr->assigned_cameras,
                                          &current_camera_ptr) )
        {
            delete current_camera_ptr;

            current_camera_ptr = nullptr;
        }
    }

    /* Add the new cameras to the vector */
    {
        for (uint32_t n_camera = 0;
                      n_camera < n_cameras;
                    ++n_camera)
        {
            system_hashed_ansi_string          camera_name   = nullptr;
            varia_text_renderer_text_string_id label_text_id = varia_text_renderer_add_string(preview_ptr->text_renderer);

            scene_camera_get_property(cameras[n_camera],
                                      SCENE_CAMERA_PROPERTY_NAME,
                                      0, /* time - irrelevant */
                                     &camera_name);

            varia_text_renderer_set(preview_ptr->text_renderer,
                                    label_text_id,
                                    system_hashed_ansi_string_get_buffer(camera_name) );

            _scene_renderer_frustum_preview_camera* current_camera_ptr = new _scene_renderer_frustum_preview_camera(cameras[n_camera],
                                                                                                                    preview_ptr->text_renderer,
                                                                                                                    label_text_id);

            system_resizable_vector_push(preview_ptr->assigned_cameras,
                                         current_camera_ptr);
        }
    }

    /* Re-allocate the data buffer we will use to hold data before updating the BO */
    const unsigned int n_bytes_needed = BO_DATA_VERTEX_DATA_OFFSET            +
                                        N_BYTES_PER_BO_DATA_ENTRY * n_cameras;

    if (preview_ptr->data_bo_buffer_size < n_bytes_needed)
    {
        if (preview_ptr->data_bo_buffer != nullptr)
        {
            delete [] preview_ptr->data_bo_buffer;

            preview_ptr->data_bo_buffer      = nullptr;
            preview_ptr->data_bo_buffer_size = 0;
        }

        preview_ptr->data_bo_buffer = new (std::nothrow) unsigned char[n_bytes_needed];

        ASSERT_ALWAYS_SYNC(preview_ptr->data_bo_buffer != nullptr,
                           "Out of memory");

        if (preview_ptr->data_bo_buffer != nullptr)
        {
            preview_ptr->data_bo_buffer_size = n_bytes_needed;
        }
    }

    /* The cameras assigned have likely changed. Reset the "last update time" state
     * so that the data BO contents are refreshed during next redraw. */
    preview_ptr->data_bo_buffer_last_update_time = -1;
}

/** Please see header for spec */
PUBLIC scene_renderer_frustum_preview scene_renderer_frustum_preview_create(ral_context     context,
                                                                            scene           scene,
                                                                            const uint32_t* viewport_size_xy_px)
{
    _scene_renderer_frustum_preview* new_instance_ptr = new (std::nothrow) _scene_renderer_frustum_preview;

    ASSERT_ALWAYS_SYNC(new_instance_ptr != nullptr,
                       "Out of memory");

    new_instance_ptr->context     = context;
    new_instance_ptr->owned_scene = scene;

    _scene_renderer_frustum_preview_init(new_instance_ptr,
                                         viewport_size_xy_px);

    return (scene_renderer_frustum_preview) new_instance_ptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_frustum_preview_release(scene_renderer_frustum_preview preview)
{
    _scene_renderer_frustum_preview* preview_ptr = reinterpret_cast<_scene_renderer_frustum_preview*>(preview);

    /* Release data BO */
    if (preview_ptr->data_bo != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&preview_ptr->data_bo) );

        preview_ptr->data_bo = nullptr;
    }

    /* Release PO */
    if (preview_ptr->po != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&preview_ptr->po) );

        preview_ptr->po = nullptr;
    }

    if (preview_ptr->po_ub != nullptr)
    {
        ral_program_block_buffer_release(preview_ptr->po_ub);

        preview_ptr->po_ub = nullptr;
    }

    delete preview_ptr;
    preview_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC ral_present_task scene_renderer_frustum_preview_render(scene_renderer_frustum_preview preview,
                                                              system_time                    time,
                                                              system_matrix4x4               vp,
                                                              ral_texture_view               color_rt,
                                                              ral_texture_view               opt_depth_rt)
{
    uint32_t                         n_assigned_cameras    = 0;
    _scene_renderer_frustum_preview* preview_ptr           = reinterpret_cast<_scene_renderer_frustum_preview*>(preview);
    bool                             should_bake_cmdbuffer = (preview_ptr->command_buffer == nullptr);

    /* Do we need to re-create the gfx state? */
    if (preview_ptr->gfx_state       == nullptr      ||
        preview_ptr->render_color_rt != color_rt     ||
        preview_ptr->render_depth_rt != opt_depth_rt)
    {
        _scene_renderer_frustum_preview_bake_gfx_state(preview_ptr,
                                                       color_rt,
                                                       opt_depth_rt);
    }

    /* Do we need to re-create the data BO? */
    system_resizable_vector_get_property(preview_ptr->assigned_cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_assigned_cameras);

    if (preview_ptr->mdebv_array_size < n_assigned_cameras)
    {
        ral_buffer_create_info data_bo_create_info;

        if (preview_ptr->data_bo != nullptr)
        {
            ral_context_delete_objects(preview_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&preview_ptr->data_bo) );

            preview_ptr->data_bo = nullptr;
        }

        data_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
        data_bo_create_info.parent_buffer    = 0;
        data_bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
        data_bo_create_info.size             = preview_ptr->data_bo_buffer_size;
        data_bo_create_info.start_offset     = 0;
        data_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        data_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

        ral_context_create_buffers(preview_ptr->context,
                                   1, /* n_buffers */
                                  &data_bo_create_info,
                                  &preview_ptr->data_bo);

        /* Also need to re-bake the command buffer at this point */
        should_bake_cmdbuffer = true;
    }
    else
    if (!system_matrix4x4_is_equal(preview_ptr->render_vp,
                                   vp)                    ||
        preview_ptr->render_color_rt != color_rt          ||
        preview_ptr->render_depth_rt != opt_depth_rt)
    {
        should_bake_cmdbuffer = true;
    }

    if (should_bake_cmdbuffer)
    {
        _scene_renderer_frustum_preview_bake_command_buffer(preview_ptr);
    }

    /* Bake the result present task */
    ral_present_task                    bo_render_gpu_task;
    ral_present_task_gpu_create_info    bo_render_gpu_task_create_info;
    ral_present_task_io                 bo_render_gpu_task_unique_inputs [3];
    ral_present_task_io                 bo_render_gpu_task_unique_outputs[2];
    ral_present_task                    bo_update_cpu_task;
    ral_present_task_cpu_create_info    bo_update_cpu_task_create_info;
    ral_present_task_io                 bo_update_cpu_task_unique_output;
    ral_present_task                    render_text_task;
    ral_present_task                    result_task;
    ral_present_task_group_create_info  result_task_create_info;
    ral_present_task_ingroup_connection result_task_ingroup_connections[2];
    ral_present_task_group_mapping      result_task_input_mappings     [2];
    ral_present_task_group_mapping      result_task_output_mappings    [2];
    ral_present_task                    result_task_present_tasks      [3];

    bo_update_cpu_task_unique_output.buffer      = preview_ptr->data_bo;
    bo_update_cpu_task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    bo_update_cpu_task_create_info.cpu_task_callback_user_arg = preview_ptr;
    bo_update_cpu_task_create_info.n_unique_inputs            = 0;
    bo_update_cpu_task_create_info.n_unique_outputs           = 1;
    bo_update_cpu_task_create_info.pfn_cpu_task_callback_proc = _scene_renderer_frustum_update_data_bo_cpu_task_callback;
    bo_update_cpu_task_create_info.unique_inputs              = nullptr;
    bo_update_cpu_task_create_info.unique_outputs             = &bo_update_cpu_task_unique_output;

    bo_update_cpu_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Frustum preview: BO update"),
                                                    &bo_update_cpu_task_create_info);


    bo_render_gpu_task_unique_inputs[0].buffer      = preview_ptr->data_bo;
    bo_render_gpu_task_unique_inputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    bo_render_gpu_task_unique_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    bo_render_gpu_task_unique_inputs[1].texture_view = color_rt;

    bo_render_gpu_task_unique_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    bo_render_gpu_task_unique_inputs[2].texture_view = opt_depth_rt;

    bo_render_gpu_task_unique_outputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    bo_render_gpu_task_unique_outputs[0].texture_view = color_rt;

    bo_render_gpu_task_unique_outputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    bo_render_gpu_task_unique_outputs[1].texture_view = opt_depth_rt;

    bo_render_gpu_task_create_info.command_buffer   = preview_ptr->command_buffer;
    bo_render_gpu_task_create_info.n_unique_inputs  = (opt_depth_rt != nullptr) ? 3 : 2;
    bo_render_gpu_task_create_info.n_unique_outputs = (opt_depth_rt != nullptr) ? 2 : 1;
    bo_render_gpu_task_create_info.unique_inputs    = bo_render_gpu_task_unique_inputs;
    bo_render_gpu_task_create_info.unique_outputs   = bo_render_gpu_task_unique_outputs;

    bo_render_gpu_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Frustum preview: rasterization"),
                                                    &bo_render_gpu_task_create_info);


    render_text_task = varia_text_renderer_get_present_task(preview_ptr->text_renderer,
                                                            color_rt);


    result_task_present_tasks[0] = bo_update_cpu_task;
    result_task_present_tasks[1] = bo_render_gpu_task;
    result_task_present_tasks[2] = render_text_task;

    result_task_input_mappings[0].group_task_io_index   = 0;
    result_task_input_mappings[0].n_present_task        = 1;
    result_task_input_mappings[0].present_task_io_index = 1; /* color_rt */

    result_task_input_mappings[1].group_task_io_index   = 0;
    result_task_input_mappings[1].n_present_task        = 1;
    result_task_input_mappings[1].present_task_io_index = 2; /* opt_depth_rt */

    result_task_output_mappings[0].group_task_io_index   = 0;
    result_task_output_mappings[0].n_present_task        = 2;
    result_task_output_mappings[0].present_task_io_index = 0; /* color_rt */

    result_task_output_mappings[1].group_task_io_index   = 0;
    result_task_output_mappings[1].n_present_task        = 1;
    result_task_output_mappings[1].present_task_io_index = 1; /* opt_depth_rt */

    result_task_ingroup_connections[0].input_present_task_index     = 1;
    result_task_ingroup_connections[0].input_present_task_io_index  = 0;
    result_task_ingroup_connections[0].output_present_task_index    = 0;
    result_task_ingroup_connections[0].output_present_task_io_index = 0; /* UB */

    result_task_ingroup_connections[1].input_present_task_index     = 2;
    result_task_ingroup_connections[1].input_present_task_io_index  = 0;
    result_task_ingroup_connections[1].output_present_task_index    = 1;
    result_task_ingroup_connections[1].output_present_task_io_index = 0; /* color_rt */

    result_task_create_info.ingroup_connections                      = result_task_ingroup_connections;
    result_task_create_info.n_ingroup_connections                    = 2;
    result_task_create_info.n_present_tasks                          = sizeof(result_task_present_tasks) / sizeof(result_task_present_tasks[0]);
    result_task_create_info.n_total_unique_inputs                    = (opt_depth_rt != nullptr) ? 2 : 1;
    result_task_create_info.n_total_unique_outputs                   = (opt_depth_rt != nullptr) ? 2 : 1;
    result_task_create_info.n_unique_input_to_ingroup_task_mappings  = result_task_create_info.n_total_unique_inputs;
    result_task_create_info.n_unique_output_to_ingroup_task_mappings = result_task_create_info.n_total_unique_outputs;
    result_task_create_info.present_tasks                            = result_task_present_tasks;
    result_task_create_info.unique_input_to_ingroup_task_mapping     = result_task_input_mappings;
    result_task_create_info.unique_output_to_ingroup_task_mapping    = result_task_output_mappings;

    result_task = ral_present_task_create_group(system_hashed_ansi_string_create("Frustum preview: rasterization"),
                                                &result_task_create_info);

    /* Cache important data for subsequent calls & CPU present task's handler */
    preview_ptr->render_color_rt = color_rt;
    preview_ptr->render_depth_rt = opt_depth_rt;
    preview_ptr->render_time     = time;

    system_matrix4x4_set_from_matrix4x4(preview_ptr->render_vp,
                                        vp);

    /* Release the in-group tasks */
    ral_present_task_release(bo_render_gpu_task);
    ral_present_task_release(bo_update_cpu_task);
    ral_present_task_release(render_text_task);

    return result_task;
}



