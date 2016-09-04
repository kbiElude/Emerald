/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "postprocessing/postprocessing_motion_blur.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

static const char* _postprocessing_motion_blur_po_template_name = "Motion Blur post-processor (src color:[%s] src velocity:[%s] dst color:[%s] texture type:[%s])";

static const char* cs_body_template =
    "#version 430 core\n"
    "\n"
    "layout (local_size_x = LOCAL_SIZE_X, local_size_y = 1, local_size_z = 1) in;\n"
    "\n"
    "layout(binding = 0, SRC_DST_COLOR_IMAGE_FORMAT) uniform restrict writeonly IMAGE_TYPE dst_color_image;\n"
    "\n"
    "layout(location = 0) uniform SAMPLER_TYPE src_color_image;\n"
    "layout(location = 1) uniform sampler2D    src_velocity_image;\n"
    "\n"
    "layout(binding = 1, packed) uniform propsUB\n"
    "{\n"
    "IMAGE_UNIFORMS"
    "    uint n_velocity_samples_max;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    const ivec2 texture_size              = textureSize(src_color_image, 0) - ivec2(1);\n"
    "    const  vec2 texel_size                = vec2(1.0) / vec2(texture_size + 1);\n"
    "    const int   global_invocation_id_flat = int(gl_GlobalInvocationID.x);\n"
    "\n"
    "IMAGE_XY_DECLARATIONS"
    "\n"
    "    const vec2 texture_xy = vec2(image_xy) / vec2(texture_size);\n"
    "\n"
    "    if (image_xy.y >= texture_size.y)\n"
    "    {\n"
    "        return;"
    "    }\n"
    "\n"
    "    const vec2 velocity           = textureLod(src_velocity_image, texture_xy, 0).xy;\n"
    "    const int  n_velocity_samples = int(clamp(length(velocity / texel_size), 1.0, float(n_velocity_samples_max) ) );\n"
    "          vec4 data               = vec4(0.0);\n"
    "\n"
    "    vec2 offset = texture_xy - texel_size * vec2(0.5);\n"
    "\n"
    "FETCH_AND_ADD_SAMPLE"
    "\n"
    "    for (uint n_velocity_sample = 1;\n"
    "              n_velocity_sample < n_velocity_samples;\n"
    "            ++n_velocity_sample)\n"
    "    {\n"
    "        vec2 offset = texture_xy + velocity / vec2(n_velocity_samples - 1) * vec2(n_velocity_sample) - texel_size * vec2(0.5);\n"
    "\n"
    "FETCH_AND_ADD_SAMPLE"
    "    }\n"
    "\n"
    "    data /= vec4(float(n_velocity_samples) );\n"
    "\n"
    "STORE_DATA"
    "}\n";

static const char* cs_image2d_xy_declarations =
    "const ivec2 image_xy       = ivec2(  global_invocation_id_flat                    % texture_size.x,\n"
    "                                     global_invocation_id_flat                    / texture_size.x);\n";

static const char* cs_image2dms_xy_declarations =
    "const  int  image_n_sample =         global_invocation_id_flat % image_n_samples;\n"
    "const ivec2 image_xy       = ivec2( (global_invocation_id_flat / image_n_samples) % texture_size.x,\n"
    "                                    (global_invocation_id_flat / image_n_samples) / texture_size.x);\n"
    "\n";

static const char* cs_image2d_uniforms   = "";
static const char* cs_image2dms_uniforms = "int  image_n_samples;\n";

static const char* cs_image2d_fetch_and_add_sample   = "data += textureLod(src_color_image, offset,                 0);\n";
static const char* cs_image2dms_fetch_and_add_sample = "data += textureLod(src_color_image, offset, image_n_sample, 0);\n";

static const char* cs_image2d_store_data   = "imageStore(dst_color_image, image_xy,                 data);\n";
static const char* cs_image2dms_store_data = "imageStore(dst_color_image, image_xy, image_n_sample, data);\n";


/** Internal type definition */
typedef struct _postprocessing_motion_blur
{
    ral_context context;

    ral_command_buffer cached_command_buffer;
    ral_texture_view   cached_input_color_texture_view;
    ral_texture_view   cached_input_velocity_texture_view;
    ral_texture_view   cached_output_texture_view;
    ral_present_task   cached_present_task;

    postprocessing_motion_blur_image_type   image_type;
    unsigned int                            n_velocity_samples_max;
    ral_program                             po;
    ral_program_block_buffer                po_props_ub;
    unsigned int                            po_props_ub_bo_size;
    unsigned int                            po_props_ub_bo_image_n_samples_start_offset;
    unsigned int                            po_props_ub_bo_n_velocity_samples_max_start_offset;
    const uint32_t                          po_binding_src_color_image;
    const uint32_t                          po_binding_src_velocity_image;
    const uint32_t                          po_binding_dst_color_image;
    ral_sampler                             sampler; /* owned by context - do not release */
    postprocessing_motion_blur_image_format src_dst_color_image_format;
    postprocessing_motion_blur_image_format src_velocity_image_format;


    unsigned int wg_local_size_x; /* y, z = 1 */

    explicit _postprocessing_motion_blur(ral_context                             in_context,
                                         postprocessing_motion_blur_image_format in_src_dst_color_image_format,
                                         postprocessing_motion_blur_image_format in_src_velocity_image_format,
                                         postprocessing_motion_blur_image_type   in_image_type)
        :po_binding_src_color_image   (0), /* hard-coded in the CS */
         po_binding_src_velocity_image(1), /* hard-coded in the CS */
         po_binding_dst_color_image   (2)  /* hard-coded in the CS */
    {
        cached_command_buffer                              = nullptr;
        cached_input_color_texture_view                    = nullptr;
        cached_input_velocity_texture_view                 = nullptr;
        cached_output_texture_view                         = nullptr;
        cached_present_task                                = nullptr;
        context                                            = in_context;
        image_type                                         = in_image_type;
        n_velocity_samples_max                             = 32; /* as per documentation */
        po                                                 = nullptr;
        po_props_ub                                        = nullptr;
        po_props_ub_bo_n_velocity_samples_max_start_offset = -1;
        po_props_ub_bo_size                                = 0;
        sampler                                            = nullptr;
        src_dst_color_image_format                         = in_src_dst_color_image_format;
        src_velocity_image_format                          = in_src_velocity_image_format;
        wg_local_size_x                                    = 0;
    }

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_motion_blur;


/** Forward declarations */
PRIVATE ral_format                _postprocessing_motion_blur_get_blur_image_format           (postprocessing_motion_blur_image_format image_format);
PRIVATE ral_texture_type          _postprocessing_motion_blur_get_blur_image_type_texture_type(postprocessing_motion_blur_image_type   image_type);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_type             (postprocessing_motion_blur_image_type   image_type);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_cs_body                     (_postprocessing_motion_blur*            motion_blur_ptr);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_po_name                     (_postprocessing_motion_blur*            motion_blur_ptr);
PRIVATE void                      _postprocessing_motion_blur_init_po                         (_postprocessing_motion_blur*            motion_blur_ptr);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_motion_blur,
                               postprocessing_motion_blur,
                              _postprocessing_motion_blur);


/** TODO */
PRIVATE ral_texture_type _postprocessing_motion_blur_get_blur_image_type_texture_type(postprocessing_motion_blur_image_type image_type)
{
    ral_texture_type result = RAL_TEXTURE_TYPE_UNKNOWN;

    switch (image_type)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:             result = RAL_TEXTURE_TYPE_2D;             break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE: result = RAL_TEXTURE_TYPE_MULTISAMPLE_2D; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postprocessing_motion_blur_image_type value");
        }
    }

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_type(postprocessing_motion_blur_image_type image_type)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    switch (image_type)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:             result = system_hashed_ansi_string_create("2D");             break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE: result = system_hashed_ansi_string_create("2D Multisample"); break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postprocessing_motion_blur_image_type value");
        }
    }

    return result;
}

/** TODO */
PRIVATE ral_format _postprocessing_motion_blur_get_blur_image_format(postprocessing_motion_blur_image_format image_format)
{
    ral_format result = RAL_FORMAT_UNKNOWN;

    switch (image_format)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG16F: result = RAL_FORMAT_RG16_FLOAT ; break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG32F: result = RAL_FORMAT_RG32_FLOAT;  break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RGBA8: result = RAL_FORMAT_RGBA8_UNORM; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postporcessing_motion_blur_image_format value");
        }
    }

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_cs_body(_postprocessing_motion_blur* motion_blur_ptr)
{
    /* Determine max local work-group size */
    const uint32_t* max_compute_work_group_size = nullptr;

    ral_context_get_property(motion_blur_ptr->context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,
                            &max_compute_work_group_size);

    motion_blur_ptr->wg_local_size_x  = max_compute_work_group_size[0];

    /* Form the body */
    const ral_texture_type    image_texture_type                          = _postprocessing_motion_blur_get_blur_image_type_texture_type(motion_blur_ptr->image_type);
    system_hashed_ansi_string image_texture_type_layout_qualifier         = nullptr;
    system_hashed_ansi_string sampler_type                                = nullptr;
    const ral_format          src_dst_color_image_format                  = _postprocessing_motion_blur_get_blur_image_format(motion_blur_ptr->src_dst_color_image_format);
    system_hashed_ansi_string src_dst_color_image_format_layout_qualifier = nullptr;
    const ral_format          src_velocity_image_format                   = _postprocessing_motion_blur_get_blur_image_format(motion_blur_ptr->src_velocity_image_format);
    system_hashed_ansi_string src_velocity_image_format_layout_qualifier  = nullptr;

    const system_hashed_ansi_string token_keys[] =
    {
        system_hashed_ansi_string_create("LOCAL_SIZE_X"),
        system_hashed_ansi_string_create("SRC_DST_COLOR_IMAGE_FORMAT"),
        system_hashed_ansi_string_create("SRC_VELOCITY_IMAGE_FORMAT"),
        system_hashed_ansi_string_create("IMAGE_UNIFORMS"),
        system_hashed_ansi_string_create("IMAGE_XY_DECLARATIONS"),
        system_hashed_ansi_string_create("FETCH_AND_ADD_SAMPLE"),
        system_hashed_ansi_string_create("IMAGE_TYPE"),
        system_hashed_ansi_string_create("STORE_DATA"),
        system_hashed_ansi_string_create("SAMPLER_TYPE")
    };
    const uint32_t                  n_token_key_values               = sizeof(token_keys) / sizeof(token_keys[0]);
    system_hashed_ansi_string       token_values[n_token_key_values] = {nullptr};
    char                            temp[1024];

    snprintf(temp,
             sizeof(temp),
             "%u",
             motion_blur_ptr->wg_local_size_x);
    token_values[0] = system_hashed_ansi_string_create(temp);

    ral_utils_get_format_property      (src_dst_color_image_format,
                                        RAL_FORMAT_PROPERTY_IMAGE_LAYOUT_QUALIFIER,
                                       &src_dst_color_image_format_layout_qualifier);
    ral_utils_get_format_property      (src_velocity_image_format,
                                        RAL_FORMAT_PROPERTY_IMAGE_LAYOUT_QUALIFIER,
                                       &src_velocity_image_format_layout_qualifier);
    ral_utils_get_texture_type_property(image_texture_type,
                                        RAL_TEXTURE_TYPE_PROPERTY_GLSL_FLOAT_IMAGE_LAYOUT_QUALIFIER,
                                       &image_texture_type_layout_qualifier);
    ral_utils_get_texture_type_property(image_texture_type,
                                        RAL_TEXTURE_TYPE_PROPERTY_GLSL_FLOAT_SAMPLER_TYPE,
                                       &sampler_type);

    token_values[1] = src_dst_color_image_format_layout_qualifier;
    token_values[2] = src_velocity_image_format_layout_qualifier;
    token_values[6] = image_texture_type_layout_qualifier;
    token_values[8] = sampler_type;

    switch (motion_blur_ptr->image_type)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:
        {
            token_values[3] = system_hashed_ansi_string_create(cs_image2d_uniforms);
            token_values[4] = system_hashed_ansi_string_create(cs_image2d_xy_declarations);
            token_values[5] = system_hashed_ansi_string_create(cs_image2d_fetch_and_add_sample);
            token_values[7] = system_hashed_ansi_string_create(cs_image2d_store_data);

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE:
        {
            token_values[3] = system_hashed_ansi_string_create(cs_image2dms_uniforms);
            token_values[4] = system_hashed_ansi_string_create(cs_image2dms_xy_declarations);
            token_values[5] = system_hashed_ansi_string_create(cs_image2dms_fetch_and_add_sample);
            token_values[7] = system_hashed_ansi_string_create(cs_image2dms_store_data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized image type");
        }
    }

    return system_hashed_ansi_string_create_by_token_replacement(cs_body_template,
                                                                 n_token_key_values,
                                                                 token_keys,
                                                                 token_values);
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_po_name(_postprocessing_motion_blur* motion_blur_ptr)
{
    char temp[1024];

    system_hashed_ansi_string src_dst_color_image_format_layout_qualifier;
    system_hashed_ansi_string src_velocity_image_format_layout_qualifier;

    ral_utils_get_format_property(_postprocessing_motion_blur_get_blur_image_format(motion_blur_ptr->src_dst_color_image_format),
                                  RAL_FORMAT_PROPERTY_IMAGE_LAYOUT_QUALIFIER,
                                 &src_dst_color_image_format_layout_qualifier);
    ral_utils_get_format_property(_postprocessing_motion_blur_get_blur_image_format(motion_blur_ptr->src_velocity_image_format),
                                  RAL_FORMAT_PROPERTY_IMAGE_LAYOUT_QUALIFIER,
                                 &src_velocity_image_format_layout_qualifier);

    snprintf(temp,
             sizeof(temp),
             _postprocessing_motion_blur_po_template_name,
             system_hashed_ansi_string_get_buffer(src_dst_color_image_format_layout_qualifier),
             system_hashed_ansi_string_get_buffer(src_velocity_image_format_layout_qualifier),
             system_hashed_ansi_string_get_buffer(src_dst_color_image_format_layout_qualifier),
             system_hashed_ansi_string_get_buffer(_postprocessing_motion_blur_get_blur_image_type(motion_blur_ptr->image_type)) );

    return system_hashed_ansi_string_create(temp);
}

/** TODO */
PRIVATE void _postprocessing_motion_blur_init_po(_postprocessing_motion_blur* motion_blur_ptr)
{
    system_hashed_ansi_string po_name = _postprocessing_motion_blur_get_po_name(motion_blur_ptr);

    motion_blur_ptr->po = ral_context_get_program_by_name(motion_blur_ptr->context,
                                                          po_name);

    if (motion_blur_ptr->po == nullptr)
    {
        /* Form the compute shader */
        ral_shader                cs      = nullptr;
        system_hashed_ansi_string cs_body = _postprocessing_motion_blur_get_cs_body(motion_blur_ptr);

        const ral_shader_create_info cs_create_info =
        {
            system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(po_name),
                                                                    " CS"),
            RAL_SHADER_TYPE_COMPUTE
        };

        if (!ral_context_create_shaders(motion_blur_ptr->context,
                                        1, /* n_create_info_items */
                                       &cs_create_info,
                                       &cs) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL shader creation failed");
        }

        ral_shader_set_property(cs,
                                RAL_SHADER_PROPERTY_GLSL_BODY,
                               &cs_body);

        /* Form & link the program object */
        const ral_program_create_info po_create_info =
        {
            RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
            po_name
        };

        if (!ral_context_create_programs(motion_blur_ptr->context,
                                         1, /* n_create_info_items */
                                        &po_create_info,
                                        &motion_blur_ptr->po) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL program creation failed");
        }

        if (!ral_program_attach_shader(motion_blur_ptr->po,
                                       cs,
                                       true /* async */))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to attach & link a RAL program");
        }

        /* All done */
        ral_context_delete_objects(motion_blur_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&cs) );
    }
    else
    {
        ral_context_retain_object(motion_blur_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  motion_blur_ptr->po);
    }

    if (motion_blur_ptr->po != nullptr)
    {
        /* Retrieve PO's uniform buffer & variable properties */
        const ral_program_variable* n_velocity_samples_max_variable_ral_ptr = nullptr;
        ral_buffer                  po_props_ub_bo_ral                      = nullptr;

        motion_blur_ptr->po_props_ub = ral_program_block_buffer_create(motion_blur_ptr->context,
                                                                       motion_blur_ptr->po,
                                                                       system_hashed_ansi_string_create("propsUB") );

        ral_program_get_block_variable_by_name(motion_blur_ptr->po,
                                               system_hashed_ansi_string_create("propsUB"),
                                               system_hashed_ansi_string_create("n_velocity_samples_max"),
                                              &n_velocity_samples_max_variable_ral_ptr);

        ASSERT_DEBUG_SYNC(n_velocity_samples_max_variable_ral_ptr != nullptr,
                          "Could not retrieve n_velocity_samples variable descriptor");

        ral_program_block_buffer_get_property(motion_blur_ptr->po_props_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &po_props_ub_bo_ral);
        ral_buffer_get_property              (po_props_ub_bo_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &motion_blur_ptr->po_props_ub_bo_size);

        motion_blur_ptr->po_props_ub_bo_n_velocity_samples_max_start_offset = n_velocity_samples_max_variable_ral_ptr->block_offset;

        /* Some variables in the props UB are optional */
        switch (motion_blur_ptr->image_type)
        {
            case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:
            {
                /* No special properties */
                break;
            }

            case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE:
            {
                const ral_program_variable* image_n_samples_variable_ral_ptr = nullptr;

                ral_program_get_block_variable_by_name(motion_blur_ptr->po,
                                                       system_hashed_ansi_string_create("propsUB"),
                                                       system_hashed_ansi_string_create("image_n_samples"),
                                                      &image_n_samples_variable_ral_ptr);

                ASSERT_DEBUG_SYNC(image_n_samples_variable_ral_ptr != nullptr,
                                  "Could not retrieve image_n_samples variable descriptor");

                motion_blur_ptr->po_props_ub_bo_image_n_samples_start_offset = image_n_samples_variable_ral_ptr->block_offset;
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized image type");
            }
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(motion_blur_ptr->po != nullptr,
                           "Could not prepare the motion blur post-processor program object.");
    }
}

/** TODO */
PRIVATE void _postprocessing_motion_blur_release(void* ptr)
{
    _postprocessing_motion_blur* motion_blur_ptr = reinterpret_cast<_postprocessing_motion_blur*>(ptr);

    ral_context_delete_objects(motion_blur_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&motion_blur_ptr->po) );
    ral_context_delete_objects(motion_blur_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&motion_blur_ptr->sampler) );

    if (motion_blur_ptr->cached_command_buffer != nullptr)
    {
        ral_command_buffer_release(motion_blur_ptr->cached_command_buffer);

        motion_blur_ptr->cached_command_buffer = nullptr;
    }

    if (motion_blur_ptr->cached_present_task != nullptr)
    {
        ral_present_task_release(motion_blur_ptr->cached_present_task);

        motion_blur_ptr->cached_present_task = nullptr;
    }

    if (motion_blur_ptr->po_props_ub != nullptr)
    {
        ral_program_block_buffer_release(motion_blur_ptr->po_props_ub);

        motion_blur_ptr->po_props_ub = nullptr;
    }
}

PRIVATE void _postprocessing_motion_blur_update_props_ub(void* blur_raw_ptr)
{
    _postprocessing_motion_blur* blur_ptr = reinterpret_cast<_postprocessing_motion_blur*>(blur_raw_ptr);

    /* Update propsUB binding & data */
    ral_program_block_buffer_set_nonarrayed_variable_value(blur_ptr->po_props_ub,
                                                           blur_ptr->po_props_ub_bo_n_velocity_samples_max_start_offset,
                                                          &blur_ptr->n_velocity_samples_max,
                                                           sizeof(unsigned int) );

    if (blur_ptr->image_type == POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE)
    {
        ral_texture  color_texture_view_parent_texture = nullptr;
        unsigned int n_samples                         = 0;

        ral_texture_view_get_property(blur_ptr->cached_input_color_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &color_texture_view_parent_texture);

        ral_texture_get_property(color_texture_view_parent_texture,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &n_samples);

        ASSERT_DEBUG_SYNC(n_samples != 0,
                          "Zero-sample texture provided as input 2DMS texture!");

        ral_program_block_buffer_set_nonarrayed_variable_value(blur_ptr->po_props_ub,
                                                               blur_ptr->po_props_ub_bo_image_n_samples_start_offset,
                                                              &n_samples,
                                                               sizeof(unsigned int) );
    }

    ral_program_block_buffer_sync_immediately(blur_ptr->po_props_ub);
}


/** Please see header for specification */
PUBLIC EMERALD_API postprocessing_motion_blur postprocessing_motion_blur_create(ral_context                             context,
                                                                                postprocessing_motion_blur_image_format src_dst_color_image_format,
                                                                                postprocessing_motion_blur_image_format src_velocity_image_format,
                                                                                postprocessing_motion_blur_image_type   image_type,
                                                                                system_hashed_ansi_string               name)
{
    /* Instantiate the object */
    _postprocessing_motion_blur* motion_blur_ptr = new (std::nothrow) _postprocessing_motion_blur(context,
                                                                                                  src_dst_color_image_format,
                                                                                                  src_velocity_image_format,
                                                                                                  image_type);

    ASSERT_DEBUG_SYNC(motion_blur_ptr != nullptr,
                      "Out of memory");

    if (motion_blur_ptr == nullptr)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }
    else
    {
        /* Retrieve a sampler object we will use to sample the color & velocity textures */
        ral_sampler_create_info blur_sampler_create_info;

        blur_sampler_create_info.mipmap_mode = RAL_TEXTURE_MIPMAP_MODE_NEAREST;
        blur_sampler_create_info.min_filter  = RAL_TEXTURE_FILTER_LINEAR;
        blur_sampler_create_info.wrap_r      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
        blur_sampler_create_info.wrap_s      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
        blur_sampler_create_info.wrap_t      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;

        ral_context_create_samplers(context,
                                    1, /* n_screate_info_items */
                                   &blur_sampler_create_info,
                                   &motion_blur_ptr->sampler);

        /* Initialize the program object */
        _postprocessing_motion_blur_init_po(motion_blur_ptr);
    }

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(motion_blur_ptr,
                                                   _postprocessing_motion_blur_release,
                                                   OBJECT_TYPE_POSTPROCESSING_MOTION_BLUR,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Post-processing Motion Blur\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (postprocessing_motion_blur) motion_blur_ptr;

end:
    return nullptr;
}

/** PLease see header for specification */
PUBLIC EMERALD_API ral_present_task postprocessing_motion_blur_get_present_task(postprocessing_blur_poisson motion_blur,
                                                                                ral_texture_view            input_color_texture_view,
                                                                                ral_texture_view            input_velocity_texture_view,
                                                                                ral_texture_view            output_texture_view)
{
    ral_command_buffer           dispatch_command_buffer;
    _postprocessing_motion_blur* motion_blur_ptr         = reinterpret_cast<_postprocessing_motion_blur*>(motion_blur);
    ral_buffer                   po_props_ub_bo          = nullptr;
    ral_present_task             result                  = nullptr;

    if (motion_blur_ptr->cached_present_task                != nullptr                     &&
        motion_blur_ptr->cached_input_color_texture_view    == input_color_texture_view    &&
        motion_blur_ptr->cached_input_velocity_texture_view == input_velocity_texture_view &&
        motion_blur_ptr->cached_output_texture_view         == output_texture_view)
    {
        result = motion_blur_ptr->cached_present_task;

        goto end;
    }

    /* Retrieve values of those properties of the input color texture that affect
     * the global work-group size.
     */
    unsigned int     input_color_texture_n_samples           = 0;
    unsigned int     input_color_texture_view_mipmap_height  = 0;
    unsigned int     input_color_texture_view_mipmap_width   = 0;
    ral_texture      input_color_texture_view_parent_texture = nullptr;
    ral_texture_type input_color_texture_view_type;
    ral_texture_type input_velocity_texture_view_type;

    ral_texture_view_get_property(input_color_texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                 &input_color_texture_view_parent_texture);
    ral_texture_view_get_property(input_color_texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                 &input_color_texture_view_type);
    ral_texture_view_get_property(input_velocity_texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                 &input_velocity_texture_view_type);

    ral_texture_get_property(input_color_texture_view_parent_texture,
                              RAL_TEXTURE_PROPERTY_N_SAMPLES,
                             &input_color_texture_n_samples);

    ral_texture_view_get_mipmap_property(input_color_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &input_color_texture_view_mipmap_height);
    ral_texture_view_get_mipmap_property(input_color_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &input_color_texture_view_mipmap_width);

    /* Sanity checks */
    #ifdef _DEBUG
    {
        unsigned int     input_color_texture_view_n_mipmaps         = 0;
        unsigned int     input_velocity_texture_n_samples           = 0;
        unsigned int     input_velocity_texture_view_mipmap_height  = 0;
        unsigned int     input_velocity_texture_view_mipmap_width   = 0;
        unsigned int     input_velocity_texture_view_n_mipmaps      = 0;
        ral_texture      input_velocity_texture_view_parent_texture = nullptr;
        unsigned int     output_texture_view_mipmap_height          = 0;
        unsigned int     output_texture_view_mipmap_width           = 0;
        unsigned int     output_texture_view_n_mipmaps              = 0;
        ral_texture      output_texture_view_parent_texture         = nullptr;
        ral_texture_type output_texture_view_type;
        unsigned int     output_texture_n_samples = 0;

        ral_texture_view_get_property(input_color_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                     &input_color_texture_view_n_mipmaps);
        ral_texture_view_get_property(input_velocity_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                     &input_velocity_texture_view_n_mipmaps);
        ral_texture_view_get_property(input_velocity_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &input_velocity_texture_view_parent_texture);
        ral_texture_view_get_property(output_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &output_texture_view_parent_texture);
        ral_texture_view_get_property(output_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                     &output_texture_view_n_mipmaps);
        ral_texture_view_get_property(output_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                     &output_texture_view_type);

        ral_texture_get_property(input_velocity_texture_view_parent_texture,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &input_velocity_texture_n_samples);
        ral_texture_get_property(output_texture_view_parent_texture,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &output_texture_n_samples);

        ASSERT_DEBUG_SYNC(input_color_texture_view_type    == input_velocity_texture_view_type &&
                          input_velocity_texture_view_type == output_texture_view_type         &&
                          output_texture_view_type         == motion_blur_ptr->image_type,
                          "Image type mismatch");
        ASSERT_DEBUG_SYNC(input_color_texture_n_samples    == input_velocity_texture_n_samples &&
                          input_velocity_texture_n_samples == output_texture_n_samples,
                          "No of samples mismatch");

        ral_texture_view_get_mipmap_property(input_velocity_texture_view,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                            &input_velocity_texture_view_mipmap_height);
        ral_texture_view_get_mipmap_property(input_velocity_texture_view,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                            &input_velocity_texture_view_mipmap_width);
        ral_texture_view_get_mipmap_property(output_texture_view,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                            &output_texture_view_mipmap_height);
        ral_texture_view_get_mipmap_property(output_texture_view,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                            &output_texture_view_mipmap_width);

        ASSERT_DEBUG_SYNC(input_color_texture_view_mipmap_height    == input_velocity_texture_view_mipmap_height &&
                          input_velocity_texture_view_mipmap_height == output_texture_view_mipmap_height         &&
                          input_color_texture_view_mipmap_width     == input_velocity_texture_view_mipmap_width  &&
                          input_velocity_texture_view_mipmap_width  == output_texture_view_mipmap_width,
                          "Mipmap resolution mismatch");
    }
    #endif

    /* Record the commands */
    if (motion_blur_ptr->cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info dispatch_command_buffer_create_info;

        ral_program_block_buffer_get_property(motion_blur_ptr->po_props_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &po_props_ub_bo);

        dispatch_command_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT;
        dispatch_command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        dispatch_command_buffer_create_info.is_resettable                           = true;
        dispatch_command_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(motion_blur_ptr->context,
                                           1, /* n_command_buffers */
                                           &dispatch_command_buffer_create_info,
                                           &motion_blur_ptr->cached_command_buffer);
    }

    dispatch_command_buffer = motion_blur_ptr->cached_command_buffer;

    ral_command_buffer_start_recording(dispatch_command_buffer);
    {
        uint32_t                                    dispatch_xyz[3];
        ral_command_buffer_set_binding_command_info set_binding_commands[4];

        /* Record commands */
        if (input_color_texture_n_samples == 0)
        {
            input_color_texture_n_samples = 1;
        }

        set_binding_commands[0].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        set_binding_commands[0].name                               = system_hashed_ansi_string_create("src_color_image");
        set_binding_commands[0].sampled_image_binding.sampler      = motion_blur_ptr->sampler;
        set_binding_commands[0].sampled_image_binding.texture_view = input_color_texture_view;

        set_binding_commands[1].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        set_binding_commands[1].name                               = system_hashed_ansi_string_create("src_velocity_image");
        set_binding_commands[1].sampled_image_binding.sampler      = motion_blur_ptr->sampler;
        set_binding_commands[1].sampled_image_binding.texture_view = input_velocity_texture_view;

        set_binding_commands[2].binding_type                       = RAL_BINDING_TYPE_STORAGE_IMAGE;
        set_binding_commands[2].name                               = system_hashed_ansi_string_create("dst_color_image");
        set_binding_commands[2].storage_image_binding.access_bits  = RAL_IMAGE_ACCESS_WRITE;
        set_binding_commands[2].storage_image_binding.texture_view = output_texture_view;

        set_binding_commands[3].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        set_binding_commands[3].name                          = system_hashed_ansi_string_create("propsUB");
        set_binding_commands[3].uniform_buffer_binding.buffer = po_props_ub_bo;
        set_binding_commands[3].uniform_buffer_binding.offset = 0;
        set_binding_commands[3].uniform_buffer_binding.size   = motion_blur_ptr->po_props_ub_bo_size;

        dispatch_xyz[0] = 1 + (input_color_texture_view_mipmap_height * input_color_texture_view_mipmap_width * input_color_texture_n_samples / motion_blur_ptr->wg_local_size_x);
        dispatch_xyz[1] = 1;
        dispatch_xyz[2] = 1;


        ral_command_buffer_record_set_bindings(dispatch_command_buffer,
                                               sizeof(set_binding_commands) / sizeof(set_binding_commands[0]),
                                               set_binding_commands);
        ral_command_buffer_record_set_program (dispatch_command_buffer,
                                               motion_blur_ptr->po);
        ral_command_buffer_record_dispatch    (dispatch_command_buffer,
                                               dispatch_xyz);
    }
    ral_command_buffer_stop_recording(dispatch_command_buffer);

    /* Create the present task */
    {
        ral_present_task                    dispatch_task;
        ral_present_task_gpu_create_info    dispatch_task_create_info;
        ral_present_task_io                 dispatch_task_unique_inputs[3];
        ral_present_task_io                 dispatch_task_unique_output;
        ral_present_task_group_create_info  result_task_create_info;
        ral_present_task_ingroup_connection result_task_ingroup_connection;
        ral_present_task_group_mapping      result_task_input_mappings[2];
        ral_present_task_group_mapping      result_task_output_mapping;
        ral_present_task                    result_task_present_tasks[2];
        ral_present_task                    update_props_task;
        ral_present_task_cpu_create_info    update_props_task_create_info;
        ral_present_task_io                 update_props_task_unique_output;

        dispatch_task_unique_inputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
        dispatch_task_unique_inputs[0].buffer      = po_props_ub_bo;

        dispatch_task_unique_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        dispatch_task_unique_inputs[1].texture_view = input_color_texture_view;

        dispatch_task_unique_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        dispatch_task_unique_inputs[2].texture_view = input_velocity_texture_view;

        dispatch_task_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        dispatch_task_unique_output.texture_view = output_texture_view;

        dispatch_task_create_info.command_buffer   = dispatch_command_buffer;
        dispatch_task_create_info.n_unique_inputs  = sizeof(dispatch_task_unique_inputs) / sizeof(dispatch_task_unique_inputs[0]);
        dispatch_task_create_info.n_unique_outputs = 1;
        dispatch_task_create_info.unique_inputs    = dispatch_task_unique_inputs;
        dispatch_task_create_info.unique_outputs   = &dispatch_task_unique_output;

        dispatch_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Motion blur: dispatch task"),
                                                   &dispatch_task_create_info);


        update_props_task_unique_output.buffer      = po_props_ub_bo;
        update_props_task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        update_props_task_create_info.cpu_task_callback_user_arg = motion_blur_ptr;
        update_props_task_create_info.n_unique_inputs            = 0;
        update_props_task_create_info.n_unique_outputs           = 1;
        update_props_task_create_info.pfn_cpu_task_callback_proc = _postprocessing_motion_blur_update_props_ub;
        update_props_task_create_info.unique_inputs              = nullptr;
        update_props_task_create_info.unique_outputs             = &update_props_task_unique_output;

        update_props_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Motion blur: Props BO update"),
                                                       &update_props_task_create_info);


        result_task_present_tasks[0] = update_props_task;
        result_task_present_tasks[1] = dispatch_task;

        result_task_ingroup_connection.input_present_task_index     = 1;
        result_task_ingroup_connection.input_present_task_io_index  = 0; /* props UB */
        result_task_ingroup_connection.output_present_task_index    = 0;
        result_task_ingroup_connection.output_present_task_io_index = 0; /* props UB */

        result_task_input_mappings[0].group_task_io_index   = 0;
        result_task_input_mappings[0].n_present_task        = 1;
        result_task_input_mappings[0].present_task_io_index = 1; /* color_texture_view */

        result_task_input_mappings[1].group_task_io_index   = 1;
        result_task_input_mappings[1].n_present_task        = 1;
        result_task_input_mappings[1].present_task_io_index = 2; /* velocity_texture_view */

        result_task_create_info.ingroup_connections                      = &result_task_ingroup_connection;
        result_task_create_info.n_ingroup_connections                    = 1;
        result_task_create_info.n_present_tasks                          = sizeof(result_task_present_tasks)  / sizeof(result_task_present_tasks[0]);
        result_task_create_info.n_total_unique_inputs                    = sizeof(result_task_input_mappings) / sizeof(result_task_input_mappings[0]);
        result_task_create_info.n_total_unique_outputs                   = 1;
        result_task_create_info.n_unique_input_to_ingroup_task_mappings  = result_task_create_info.n_total_unique_inputs;
        result_task_create_info.n_unique_output_to_ingroup_task_mappings = result_task_create_info.n_total_unique_outputs;
        result_task_create_info.present_tasks                            = result_task_present_tasks;
        result_task_create_info.unique_input_to_ingroup_task_mapping     = result_task_input_mappings;
        result_task_create_info.unique_output_to_ingroup_task_mapping    = &result_task_output_mapping;

        result = ral_present_task_create_group(system_hashed_ansi_string_create("Motion blur: processing"),
                                               &result_task_create_info);
    }

    motion_blur_ptr->cached_present_task = result;
    ral_present_task_retain(motion_blur_ptr->cached_present_task);

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void postprocessing_motion_blur_get_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                void*                               out_value_ptr)
{
    const _postprocessing_motion_blur* motion_blur_ptr = reinterpret_cast<_postprocessing_motion_blur*>(motion_blur);

    switch (property)
    {
        case POSTPROCESSING_MOTION_BLUR_PROPERTY_N_VELOCITY_SAMPLES_MAX:
        {
            *reinterpret_cast<unsigned int*>(out_value_ptr) = motion_blur_ptr->n_velocity_samples_max;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid postprocessing_motion_blur_property value.");
        }
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void postprocessing_motion_blur_set_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                const void*                         value_ptr)
{
    _postprocessing_motion_blur* motion_blur_ptr = reinterpret_cast<_postprocessing_motion_blur*>(motion_blur);

    switch (property)
    {
        case POSTPROCESSING_MOTION_BLUR_PROPERTY_N_VELOCITY_SAMPLES_MAX:
        {
            motion_blur_ptr->n_velocity_samples_max = *reinterpret_cast<const unsigned int*>(value_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid postprocessing_motion_blur_property value.");
        }
    }
}
