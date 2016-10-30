/**
 *
 * DOF test app.
 *
 */
#include "shared.h"
#include "include/main.h"
#include "stage_step_background.h"
#include "stage_step_julia.h"
#include "demo/demo_flyby.h"
#include "gfx/gfx_image.h"
#include "gfx/gfx_rgbe.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_matrix4x4.h"
#include "varia/varia_skybox.h"

PRIVATE system_matrix4x4 _inv_projection      = nullptr;
PRIVATE system_matrix4x4 _mv                  = nullptr;
PRIVATE ral_present_task _present_task        = nullptr;
PRIVATE ral_texture      _result_texture      = nullptr;
PRIVATE ral_texture_view _result_texture_view = nullptr;
PRIVATE varia_skybox     _skybox              = nullptr;
PRIVATE gfx_image        _skybox_image        = nullptr;
PRIVATE ral_texture      _skybox_texture      = nullptr;


/** TODO */
PRIVATE void _stage_step_background_update_cpu_data(void* unused)
{
    ral_program_block_buffer block_buffer      = nullptr;
    uint32_t                 inv_proj_offset;
    uint32_t                 mv_offset;
    system_matrix4x4         projection_matrix = main_get_projection_matrix();

    demo_flyby_get_property  (_flyby,
                              DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
                             &_mv);
    varia_skybox_get_property(_skybox,
                              VARIA_SKYBOX_PROPERTY_PROGRAM_BLOCK_BUFFER,
                             &block_buffer);
    varia_skybox_get_property(_skybox,
                              VARIA_SKYBOX_PROPERTY_INV_PROJ_MAT4_OFFSET,
                             &inv_proj_offset);
    varia_skybox_get_property(_skybox,
                              VARIA_SKYBOX_PROPERTY_MV_MAT4_OFFSET,
                             &mv_offset);

    system_matrix4x4_set_from_matrix4x4(_inv_projection,
                                        projection_matrix);
    system_matrix4x4_invert            (_inv_projection);

    ral_program_block_buffer_set_nonarrayed_variable_value(block_buffer,
                                                           inv_proj_offset,
                                                           system_matrix4x4_get_column_major_data(_inv_projection),
                                                           sizeof(float) * 16);
    ral_program_block_buffer_set_nonarrayed_variable_value(block_buffer,
                                                           mv_offset,
                                                           system_matrix4x4_get_column_major_data(_mv),
                                                           sizeof(float) * 16);

    ral_program_block_buffer_sync_immediately(block_buffer);
}

/* Please see header for specification */
PUBLIC void stage_step_background_deinit(ral_context context)
{
    ral_texture textures_to_release[] =
    {
        _result_texture,
        _skybox_texture
    };
    const uint32_t n_textures_to_release = sizeof(textures_to_release) / sizeof(textures_to_release[0]);

    system_matrix4x4_release(_inv_projection);
    system_matrix4x4_release(_mv);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures_to_release,
                               reinterpret_cast<void* const*>(textures_to_release) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_result_texture_view) );

    varia_skybox_release(_skybox);
    gfx_image_release   (_skybox_image);
}

/* Please see header for specification */
PUBLIC ral_texture_view stage_step_background_get_bg_texture_view()
{
    return _result_texture_view;
}

/* Please see header for specification */
PUBLIC ral_present_task stage_step_background_get_present_task()
{
    ral_program_block_buffer            block_buffer;
    ral_buffer                          buffer_ral;
    ral_present_task                    cpu_task;
    ral_present_task_cpu_create_info    cpu_task_info;
    ral_present_task_io                 cpu_task_output;
    ral_present_task                    gpu_task;

    varia_skybox_get_property            (_skybox,
                                          VARIA_SKYBOX_PROPERTY_PROGRAM_BLOCK_BUFFER,
                                         &block_buffer);
    ral_program_block_buffer_get_property(block_buffer,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &buffer_ral);

    cpu_task_info.cpu_task_callback_user_arg = nullptr;
    cpu_task_info.n_unique_inputs            = 0;
    cpu_task_info.n_unique_outputs           = 1;
    cpu_task_info.pfn_cpu_task_callback_proc = _stage_step_background_update_cpu_data;
    cpu_task_info.unique_inputs              = nullptr;
    cpu_task_info.unique_outputs             = &cpu_task_output;

    cpu_task_output.buffer      = buffer_ral;
    cpu_task_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_task = ral_present_task_create_cpu  (system_hashed_ansi_string_create("Stage step background: matrix update"),
                                            &cpu_task_info);
    gpu_task = varia_skybox_get_present_task(_skybox,
                                             _result_texture_view,
                                             cpu_task);

    ral_present_task_release(cpu_task);

    return gpu_task;
}

/* Please see header for specification */
PUBLIC void stage_step_background_init(ral_context context)
{
    /* Initialize matrix instance */
    _inv_projection = system_matrix4x4_create();
    _mv             = system_matrix4x4_create();

    /* Initialize result texture */
    ral_texture_create_info result_texture_create_info;

    result_texture_create_info.base_mipmap_depth      = 1;
    result_texture_create_info.base_mipmap_height     = main_get_output_resolution()[1];
    result_texture_create_info.base_mipmap_width      = main_get_output_resolution()[0];
    result_texture_create_info.fixed_sample_locations = true;
    result_texture_create_info.format                 = RAL_FORMAT_RGBA16_FLOAT;
    result_texture_create_info.name                   = system_hashed_ansi_string_create("BG: result texture");
    result_texture_create_info.n_layers               = 1;
    result_texture_create_info.n_samples              = 1;
    result_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    result_texture_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                        RAL_TEXTURE_USAGE_SAMPLED_BIT;
    result_texture_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(context,
                                1, /* n_textures */
                               &result_texture_create_info,
                               &_result_texture);

    /* Initialize result texture view */
    ral_texture_view_create_info result_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_result_texture);

    ral_context_create_texture_views(context,
                                     1, /* n_texture_views */
                                    &result_texture_view_create_info,
                                    &_result_texture_view);

    /* Initialize skybox data */
    _skybox_image = gfx_image_create_from_file(system_hashed_ansi_string_create("Skybox image"),
                                               system_hashed_ansi_string_create("galileo_probe.hdr"),
                                               false /* use_alternative_filename_getter */);

    ral_context_create_textures_from_gfx_images(context,
                                                1, /* n_images */
                                               &_skybox_image,
                                               &_skybox_texture);

    _skybox = varia_skybox_create_spherical_projection_texture(context,
                                                               _skybox_texture,
                                                               system_hashed_ansi_string_create("skybox") );
}
