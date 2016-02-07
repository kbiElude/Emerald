/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_ub.h"
#include "postprocessing/postprocessing_motion_blur.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

static const char* _postprocessing_motion_blur_po_template_name = "Motion Blur post-processor (src color:[%s] src velocity:[%s] dst color:[%s] texture type:[%s])";


/** Internal type definition */
typedef struct _postprocessing_motion_blur
{
    ral_context context;

    unsigned int                            dst_color_image_n_layer;
    unsigned int                            dst_color_image_n_mipmap;
    postprocessing_motion_blur_image_type   image_type;
    unsigned int                            n_velocity_samples_max;
    ral_program                             po;
    ogl_program_ub                          po_props_ub;
    ral_buffer                              po_props_ub_bo;
    unsigned int                            po_props_ub_bo_size;
    unsigned int                            po_props_ub_bo_image_n_samples_start_offset;
    unsigned int                            po_props_ub_bo_n_velocity_samples_max_start_offset;
    const GLuint                            po_binding_src_color_image;
    const GLuint                            po_binding_src_velocity_image;
    const GLuint                            po_binding_dst_color_image;
    ral_sampler                             sampler; /* owned by context - do not release */
    postprocessing_motion_blur_image_format src_dst_color_image_format;
    unsigned int                            src_color_image_n_layer;
    unsigned int                            src_color_image_n_mipmap;
    postprocessing_motion_blur_image_format src_velocity_image_format;
    unsigned int                            src_velocity_image_n_layer;
    unsigned int                            src_velocity_image_n_mipmap;


    unsigned int wg_local_size_x; /* y, z = 1 */

    explicit _postprocessing_motion_blur(ral_context                             in_context,
                                         postprocessing_motion_blur_image_format in_src_dst_color_image_format,
                                         postprocessing_motion_blur_image_format in_src_velocity_image_format,
                                         postprocessing_motion_blur_image_type   in_image_type)
        :po_binding_src_color_image   (0), /* hard-coded in the CS */
         po_binding_src_velocity_image(1), /* hard-coded in the CS */
         po_binding_dst_color_image   (2)  /* hard-coded in the CS */
    {
        context                                            = in_context;
        dst_color_image_n_layer                            = 0;
        dst_color_image_n_mipmap                           = 0;
        image_type                                         = in_image_type;
        n_velocity_samples_max                             = 32; /* as per documentation */
        po                                                 = NULL;
        po_props_ub                                        = NULL;
        po_props_ub_bo                                     = NULL;
        po_props_ub_bo_n_velocity_samples_max_start_offset = -1;
        po_props_ub_bo_size                                = 0;
        sampler                                            = NULL;
        src_color_image_n_layer                            = 0;
        src_color_image_n_mipmap                           = 0;
        src_dst_color_image_format                         = in_src_dst_color_image_format;
        src_velocity_image_format                          = in_src_velocity_image_format;
        src_velocity_image_n_layer                         = 0;
        src_velocity_image_n_mipmap                        = 0;
        wg_local_size_x                                    = 0;
    }

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_motion_blur;


/** Forward declarations */
PRIVATE GLenum                    _postprocessing_motion_blur_get_blur_image_type_texture_target_glenum(postprocessing_motion_blur_image_type   image_type);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_type_has                  (postprocessing_motion_blur_image_type   image_type);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_type_layout_qualifier_has (postprocessing_motion_blur_image_type   image_type);
PRIVATE GLenum                    _postprocessing_motion_blur_get_blur_image_format_glenum             (postprocessing_motion_blur_image_format image_format);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_format_has                (postprocessing_motion_blur_image_format image_format);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_sampler_for_image_type_has      (postprocessing_motion_blur_image_type   image_type);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_cs_body                              (_postprocessing_motion_blur*            motion_blur_ptr);
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_po_name                              (_postprocessing_motion_blur*            motion_blur_ptr);
PRIVATE void                      _postprocessing_motion_blur_init_po                                  (_postprocessing_motion_blur*            motion_blur_ptr);
PRIVATE void                      _postprocessing_motion_blur_init_po_rendering_callback               (ogl_context                             context,
                                                                                                        void*                                   motion_blur);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_motion_blur,
                               postprocessing_motion_blur,
                              _postprocessing_motion_blur);


/** TODO */
PRIVATE GLenum _postprocessing_motion_blur_get_blur_image_type_texture_target_glenum(postprocessing_motion_blur_image_type image_type)
{
    GLenum result = GL_NONE;

    switch (image_type)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:             result = GL_TEXTURE_2D;             break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE: result = GL_TEXTURE_2D_MULTISAMPLE; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postprocessing_motion_blur_image_type value");
        }
    } /* switch (image_type) */

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_type_has(postprocessing_motion_blur_image_type image_type)
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
    } /* switch (image_type) */

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_type_layout_qualifier_has(postprocessing_motion_blur_image_type image_type)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    switch (image_type)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:             result = system_hashed_ansi_string_create("image2D");   break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE: result = system_hashed_ansi_string_create("image2DMS"); break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postprocessing_motion_blur_image_type value");
        }
    } /* switch (image_type) */

    return result;
}

/** TODO */
PRIVATE GLenum _postprocessing_motion_blur_get_blur_image_format_glenum(postprocessing_motion_blur_image_format image_format)
{
    GLenum result = GL_NONE;

    switch (image_format)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG16F: result = GL_RG16F; break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG32F: result = GL_RG32F; break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RGBA8: result = GL_RGBA8; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postporcessing_motion_blur_image_format value");
        }
    } /* switch (image_format) */

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_image_format_has(postprocessing_motion_blur_image_format image_format)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    switch (image_format)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG16F: result = system_hashed_ansi_string_create("rg16f"); break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG32F: result = system_hashed_ansi_string_create("rg32f"); break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RGBA8: result = system_hashed_ansi_string_create("rgba8"); break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postprocessing_motion_blur_image_format value");
        }
    } /* switch (image_format) */

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_cs_body(_postprocessing_motion_blur* motion_blur_ptr)
{
    static const char* cs_body_template   = "#version 430 core\n"
                                            "\n"
                                            "layout (local_size_x = LOCAL_SIZE_X, local_size_y = 1, local_size_z = 1) in;\n"
                                            "\n"
                                            "layout(binding = 0, SRC_DST_COLOR_IMAGE_FORMAT) uniform restrict writeonly IMAGE_TYPE dst_color_image;\n"
                                            "\n"
                                            "layout(location = 0) uniform SAMPLER_TYPE src_color_image;\n"
                                            "layout(location = 1) uniform sampler2D    src_velocity_image;\n"
                                            "\n"
                                            "layout(packed) uniform propsUB\n"
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

    static const char* cs_image2d_xy_declarations   = "const ivec2 image_xy       = ivec2(  global_invocation_id_flat                    % texture_size.x,\n"
                                                      "                                     global_invocation_id_flat                    / texture_size.x);\n";
    static const char* cs_image2dms_xy_declarations = "const  int  image_n_sample =         global_invocation_id_flat % image_n_samples;\n"
                                                      "const ivec2 image_xy       = ivec2( (global_invocation_id_flat / image_n_samples) % texture_size.x,\n"
                                                      "                                    (global_invocation_id_flat / image_n_samples) / texture_size.x);\n"
                                                      "\n";

    static const char* cs_image2d_uniforms          = "";
    static const char* cs_image2dms_uniforms        = "int  image_n_samples;\n";

    static const char* cs_image2d_fetch_and_add_sample   = "data += textureLod(src_color_image, offset,                 0);\n";
    static const char* cs_image2dms_fetch_and_add_sample = "data += textureLod(src_color_image, offset, image_n_sample, 0);\n";

    static const char* cs_image2d_store_data   = "imageStore(dst_color_image, image_xy,                 data);\n";
    static const char* cs_image2dms_store_data = "imageStore(dst_color_image, image_xy, image_n_sample, data);\n";

    /* Determine the local work-group size */
    const ogl_context_gl_limits* limits_ptr = NULL;

    ogl_context_get_property(ral_context_get_gl_context(motion_blur_ptr->context),
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    motion_blur_ptr->wg_local_size_x  = limits_ptr->max_compute_work_group_size[0];

    /* Form the body */
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
    system_hashed_ansi_string       token_values[n_token_key_values] = {NULL};
    char                            temp[1024];

    snprintf(temp,
             sizeof(temp),
             "%u",
             motion_blur_ptr->wg_local_size_x);
    token_values[0] = system_hashed_ansi_string_create(temp);

    token_values[1] = _postprocessing_motion_blur_get_blur_image_format_has               (motion_blur_ptr->src_dst_color_image_format);
    token_values[2] = _postprocessing_motion_blur_get_blur_image_format_has               (motion_blur_ptr->src_velocity_image_format);
    token_values[6] = _postprocessing_motion_blur_get_blur_image_type_layout_qualifier_has(motion_blur_ptr->image_type);
    token_values[8] = _postprocessing_motion_blur_get_blur_sampler_for_image_type_has     (motion_blur_ptr->image_type);

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
    } /* switch (motion_blur_ptr->image_type) */

    return system_hashed_ansi_string_create_by_token_replacement(cs_body_template,
                                                                 n_token_key_values,
                                                                 token_keys,
                                                                 token_values);
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_po_name(_postprocessing_motion_blur* motion_blur_ptr)
{
    char temp[1024];

    snprintf(temp,
             sizeof(temp),
             _postprocessing_motion_blur_po_template_name,
             system_hashed_ansi_string_get_buffer(_postprocessing_motion_blur_get_blur_image_format_has(motion_blur_ptr->src_dst_color_image_format)),
             system_hashed_ansi_string_get_buffer(_postprocessing_motion_blur_get_blur_image_format_has(motion_blur_ptr->src_velocity_image_format)),
             system_hashed_ansi_string_get_buffer(_postprocessing_motion_blur_get_blur_image_format_has(motion_blur_ptr->src_dst_color_image_format)),
             system_hashed_ansi_string_get_buffer(_postprocessing_motion_blur_get_blur_image_type_has  (motion_blur_ptr->image_type)) );

    return system_hashed_ansi_string_create(temp);
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_motion_blur_get_blur_sampler_for_image_type_has(postprocessing_motion_blur_image_type image_type)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    switch (image_type)
    {
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D:             result = system_hashed_ansi_string_create("sampler2D");   break;
        case POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE: result = system_hashed_ansi_string_create("sampler2DMS"); break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized postprocessing_motion_blur_image_type value");
        }
    } /* switch (image_type) */

    return result;
}

/** TODO */
PRIVATE void _postprocessing_motion_blur_init_po(_postprocessing_motion_blur* motion_blur_ptr)
{
    system_hashed_ansi_string po_name = _postprocessing_motion_blur_get_po_name(motion_blur_ptr);

    motion_blur_ptr->po = ral_context_get_program_by_name(motion_blur_ptr->context,
                                                          po_name);

    if (motion_blur_ptr->po == NULL)
    {
        /* Form the compute shader */
        ral_shader                cs      = NULL;
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
                                       cs))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to attach & link a RAL program");
        }

        /* All done */
        ral_context_delete_objects(motion_blur_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &cs);
    } /* if (motion_blur_ptr->po == NULL) */
    else
    {
        ral_context_retain_object(motion_blur_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  motion_blur_ptr->po);
    }

    if (motion_blur_ptr->po != NULL)
    {
        /* Retrieve PO's uniform buffer & variable properties */
        const ral_program_variable* n_velocity_samples_max_variable_ptr = NULL;
        raGL_program                po_raGL                             = ral_context_get_program_gl(motion_blur_ptr->context,
                                                                                                     motion_blur_ptr->po);

        raGL_program_get_uniform_block_by_name(po_raGL,
                                               system_hashed_ansi_string_create("propsUB"),
                                              &motion_blur_ptr->po_props_ub);
        raGL_program_get_uniform_by_name      (po_raGL,
                                               system_hashed_ansi_string_create("n_velocity_samples_max"),
                                              &n_velocity_samples_max_variable_ptr);

        ASSERT_DEBUG_SYNC(n_velocity_samples_max_variable_ptr != NULL,
                          "Could not retrieve n_velocity_samples variable descriptor");
        ASSERT_DEBUG_SYNC(motion_blur_ptr->po_props_ub != NULL,
                          "GL does not recognize motion blur post-processor's propsUB uniform block");

        ogl_program_ub_get_property(motion_blur_ptr->po_props_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &motion_blur_ptr->po_props_ub_bo);
        ogl_program_ub_get_property(motion_blur_ptr->po_props_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &motion_blur_ptr->po_props_ub_bo_size);

        motion_blur_ptr->po_props_ub_bo_n_velocity_samples_max_start_offset = n_velocity_samples_max_variable_ptr->block_offset;

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
                const ral_program_variable* image_n_samples_variable_ptr = NULL;

                raGL_program_get_uniform_by_name(po_raGL,
                                                 system_hashed_ansi_string_create("image_n_samples"),
                                                &image_n_samples_variable_ptr);

                ASSERT_DEBUG_SYNC(image_n_samples_variable_ptr != NULL,
                                  "Could not retrieve image_n_samples variable descriptor");

                motion_blur_ptr->po_props_ub_bo_image_n_samples_start_offset = image_n_samples_variable_ptr->block_offset;
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized image type");
            }
        } /* switch (motion_blur_ptr->image_type) */

        /* Request a rendering context call-back to set up texture unit bindings and stuff */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(motion_blur_ptr->context),
                                                         _postprocessing_motion_blur_init_po_rendering_callback,
                                                         motion_blur_ptr);
    }
    else
    {
        ASSERT_ALWAYS_SYNC(motion_blur_ptr->po != NULL,
                           "Could not prepare the motion blur post-processor program object.");
    }
}

/** TODO */
PRIVATE void _postprocessing_motion_blur_init_po_rendering_callback(ogl_context context,
                                                                    void*       motion_blur)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _postprocessing_motion_blur*      motion_blur_ptr = (_postprocessing_motion_blur*) motion_blur;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Set up texture bindings. Sampler uniform locations are predefined in the shader */
    const raGL_program po_raGL    = ral_context_get_program_gl(motion_blur_ptr->context,
                                                               motion_blur_ptr->po);
    GLuint             po_raGL_id = 0;

    raGL_program_get_property(po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &po_raGL_id);

    entrypoints_ptr->pGLProgramUniform1i(po_raGL_id,
                                         0, /* location for src_color_image */
                                         0);
    entrypoints_ptr->pGLProgramUniform1i(po_raGL_id,
                                         1, /* location for src_velocity_image */
                                         1);
}

/** TODO */
PRIVATE void _postprocessing_motion_blur_release(void* ptr)
{
    _postprocessing_motion_blur* motion_blur_ptr = (_postprocessing_motion_blur*) ptr;

    ral_context_delete_objects(motion_blur_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &motion_blur_ptr->po);
    ral_context_delete_objects(motion_blur_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                               1, /* n_objects */
                               (const void**) &motion_blur_ptr->sampler);
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

    ASSERT_DEBUG_SYNC(motion_blur_ptr != NULL,
                      "Out of memory");

    if (motion_blur_ptr == NULL)
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
    return NULL;
}

/** PLease see header for specification */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void postprocessing_motion_blur_execute(postprocessing_motion_blur motion_blur,
                                                                                  ral_texture                input_color_texture,
                                                                                  ral_texture                input_velocity_texture,
                                                                                  ral_texture                output_texture)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr             = NULL;
    bool                              input_color_is_rbo          = false;
    GLuint                            input_color_texture_id      = 0;
    raGL_texture                      input_color_texture_raGL    = NULL;
    bool                              input_velocity_is_rbo       = false;
    raGL_texture                      input_velocity_texture_raGL = NULL;
    GLuint                            input_velocity_texture_id   = 0;
    _postprocessing_motion_blur*      motion_blur_ptr             = (_postprocessing_motion_blur*) motion_blur;
    bool                              output_is_rbo               = false;
    GLuint                            output_texture_id           = 0;
    raGL_texture                      output_texture_raGL         = NULL;
    ral_texture                       output_texture_ral          = NULL;

    ogl_context_get_property(ral_context_get_gl_context(motion_blur_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    input_color_texture_raGL    = ral_context_get_texture_gl(motion_blur_ptr->context,
                                                             input_color_texture);
    input_velocity_texture_raGL = ral_context_get_texture_gl(motion_blur_ptr->context,
                                                             input_velocity_texture);
    output_texture_raGL         = ral_context_get_texture_gl(motion_blur_ptr->context,
                                                             output_texture);

    raGL_texture_get_property(input_color_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &input_color_texture_id);
    raGL_texture_get_property(input_color_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                             &input_color_is_rbo);

    raGL_texture_get_property(input_velocity_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &input_velocity_texture_id);
    raGL_texture_get_property(input_velocity_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                             &input_velocity_is_rbo);

    raGL_texture_get_property(output_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &output_texture_id);
    raGL_texture_get_property(output_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                             &output_is_rbo);

    ASSERT_DEBUG_SYNC(!input_color_is_rbo    &&
                      !input_velocity_is_rbo &&
                      !output_is_rbo,
                      "TODO");

    /* Retrieve values of those properties of the input color texture that affect
     * the global work-group size.
     */
    ral_texture_type input_color_texture_type;
    unsigned int     input_color_texture_mipmap_height = 0;
    unsigned int     input_color_texture_mipmap_width  = 0;
    unsigned int     input_color_texture_n_samples     = 0;
    ral_texture_type input_velocity_texture_type;

    ral_texture_get_property       (input_color_texture,
                                    RAL_TEXTURE_PROPERTY_TYPE,
                                   &input_color_texture_type);
    ral_texture_get_property       (input_velocity_texture,
                                    RAL_TEXTURE_PROPERTY_TYPE,
                                   &input_velocity_texture_type);
    ral_texture_get_property       (input_color_texture,
                                    RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                   &input_color_texture_n_samples);
    ral_texture_get_mipmap_property(input_color_texture,
                                    0, /* n_layer */
                                    motion_blur_ptr->src_color_image_n_mipmap,
                                    RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                   &input_color_texture_mipmap_height);
    ral_texture_get_mipmap_property(input_color_texture,
                                    0, /* n_layer */
                                    motion_blur_ptr->src_color_image_n_mipmap,
                                    RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                   &input_color_texture_mipmap_width);

    /* Sanity checks */
    #ifdef _DEBUG
    {
        unsigned int     input_color_texture_n_mipmaps        = 0;
        unsigned int     input_velocity_texture_mipmap_height = 0;
        unsigned int     input_velocity_texture_mipmap_width  = 0;
        unsigned int     input_velocity_texture_n_mipmaps     = 0;
        unsigned int     input_velocity_texture_n_samples     = 0;
        unsigned int     output_texture_mipmap_height         = 0;
        unsigned int     output_texture_mipmap_width          = 0;
        unsigned int     output_texture_n_mipmaps             = 0;
        unsigned int     output_texture_n_samples             = 0;
        ral_texture_type output_texture_type;

        ral_texture_get_property(input_color_texture,
                                 RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                &input_color_texture_n_mipmaps);
        ral_texture_get_property(input_velocity_texture,
                                 RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                &input_velocity_texture_n_mipmaps);
        ral_texture_get_property(input_velocity_texture,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &input_velocity_texture_n_samples);
        ral_texture_get_property(output_texture,
                                 RAL_TEXTURE_PROPERTY_TYPE,
                                &output_texture_type);
        ral_texture_get_property(output_texture,
                                 RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                &output_texture_n_mipmaps);
        ral_texture_get_property(output_texture,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &output_texture_n_samples);

        ASSERT_DEBUG_SYNC(input_color_texture_type    == input_velocity_texture_type &&
                          input_velocity_texture_type == output_texture_type         &&
                          output_texture_type         == motion_blur_ptr->image_type,
                          "Image type mismatch");
        ASSERT_DEBUG_SYNC(input_color_texture_n_samples    == input_velocity_texture_n_samples &&
                          input_velocity_texture_n_samples == output_texture_n_samples,
                          "No of samples mismatch");
        ASSERT_DEBUG_SYNC(motion_blur_ptr->dst_color_image_n_mipmap    < output_texture_n_mipmaps         &&
                          motion_blur_ptr->src_color_image_n_mipmap    < input_color_texture_n_mipmaps    &&
                          motion_blur_ptr->src_velocity_image_n_mipmap < input_velocity_texture_n_mipmaps,
                          "Invalid image mipmap level requested");

        ral_texture_get_mipmap_property(input_velocity_texture,
                                        0, /* n_layer */
                                        motion_blur_ptr->src_velocity_image_n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                       &input_velocity_texture_mipmap_height);
        ral_texture_get_mipmap_property(input_velocity_texture,
                                        0, /* n_layer */
                                        motion_blur_ptr->src_velocity_image_n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                       &input_velocity_texture_mipmap_width);
        ral_texture_get_mipmap_property(output_texture,
                                        0, /* n_layer */
                                        motion_blur_ptr->dst_color_image_n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                       &output_texture_mipmap_height);
        ral_texture_get_mipmap_property(output_texture,
                                        0, /* n_layer */
                                        motion_blur_ptr->dst_color_image_n_mipmap,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                       &output_texture_mipmap_width);

        ASSERT_DEBUG_SYNC(input_color_texture_mipmap_height    == input_velocity_texture_mipmap_height &&
                          input_velocity_texture_mipmap_height == output_texture_mipmap_height         &&
                          input_color_texture_mipmap_width     == input_velocity_texture_mipmap_width  &&
                          input_velocity_texture_mipmap_width  == output_texture_mipmap_width,
                          "Mipmap resolution mismatch");
    }
    #endif

    /* Bind the images */
    const GLenum color_texture_target    = _postprocessing_motion_blur_get_blur_image_type_texture_target_glenum(motion_blur_ptr->image_type);
    GLuint       sampler_id              = 0;
    raGL_sampler sampler_raGL            = NULL;
    const GLenum velocity_texture_target = GL_TEXTURE_2D;

    sampler_raGL = ral_context_get_sampler_gl(motion_blur_ptr->context,
                                              motion_blur_ptr->sampler);

    raGL_sampler_get_property(sampler_raGL,
                              RAGL_SAMPLER_PROPERTY_ID,
                             &sampler_id);

    entrypoints_ptr->pGLActiveTexture(GL_TEXTURE0);
    entrypoints_ptr->pGLBindSampler  (0,
                                      sampler_id);
    entrypoints_ptr->pGLBindTexture  (color_texture_target,
                                      input_color_texture_id);

    entrypoints_ptr->pGLActiveTexture(GL_TEXTURE1);
    entrypoints_ptr->pGLBindSampler  (1,
                                      sampler_id);
    entrypoints_ptr->pGLBindTexture  (color_texture_target,
                                      input_velocity_texture_id);

    entrypoints_ptr->pGLBindImageTexture(0, /* index */
                                         output_texture_id,
                                         motion_blur_ptr->dst_color_image_n_mipmap,
                                         GL_FALSE, /* layered */
                                         motion_blur_ptr->dst_color_image_n_layer,
                                         GL_WRITE_ONLY,
                                         _postprocessing_motion_blur_get_blur_image_format_glenum(motion_blur_ptr->src_dst_color_image_format) );

    /* Update propsUB binding & data */
    ogl_program_ub_set_nonarrayed_uniform_value(motion_blur_ptr->po_props_ub,
                                                motion_blur_ptr->po_props_ub_bo_n_velocity_samples_max_start_offset,
                                               &motion_blur_ptr->n_velocity_samples_max,
                                                0, /* src_data_flags */
                                                sizeof(unsigned int) );

    if (motion_blur_ptr->image_type == POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE)
    {
        unsigned int n_samples = 0;

        ral_texture_get_property(input_color_texture,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &n_samples);

        ASSERT_DEBUG_SYNC(n_samples != 0,
                          "Zero-sample texture provided as input 2DMS texture!");

        ogl_program_ub_set_nonarrayed_uniform_value(motion_blur_ptr->po_props_ub,
                                                    motion_blur_ptr->po_props_ub_bo_image_n_samples_start_offset,
                                                   &n_samples,
                                                    0, /* src_data_flags */
                                                    sizeof(unsigned int) );
    } /* if (motion_blur_ptr->image_dimensionality == POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE) */

    ogl_program_ub_sync(motion_blur_ptr->po_props_ub);

    GLuint      po_props_ub_bo_id           = 0;
    raGL_buffer po_props_ub_bo_raGL         = NULL;
    uint32_t    po_props_ub_bo_start_offset = -1;

    po_props_ub_bo_raGL = ral_context_get_buffer_gl(motion_blur_ptr->context,
                                                    motion_blur_ptr->po_props_ub_bo);

    raGL_buffer_get_property(po_props_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &po_props_ub_bo_id);
    raGL_buffer_get_property(po_props_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &po_props_ub_bo_start_offset);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        po_props_ub_bo_id,
                                        po_props_ub_bo_start_offset,
                                        motion_blur_ptr->po_props_ub_bo_size);

    /* Launch the compute jobs */
    const raGL_program po_raGL    = ral_context_get_program_gl(motion_blur_ptr->context,
                                                               motion_blur_ptr->po);
    GLuint             po_raGL_id = 0;

    raGL_program_get_property(po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &po_raGL_id);

    if (input_color_texture_n_samples == 0)
    {
        input_color_texture_n_samples = 1;
    }

    entrypoints_ptr->pGLUseProgram     (po_raGL_id);
    entrypoints_ptr->pGLDispatchCompute(1 + (input_color_texture_mipmap_height * input_color_texture_mipmap_width * input_color_texture_n_samples / motion_blur_ptr->wg_local_size_x), /* num_groups_x */
                                        1,  /* num_groups_y */
                                        1); /* num_groups_z */
}

/** Please see header for spec */
PUBLIC EMERALD_API void postprocessing_motion_blur_get_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                void*                               out_value_ptr)
{
    const _postprocessing_motion_blur* motion_blur_ptr = (_postprocessing_motion_blur*) motion_blur;

    switch (property)
    {
        case POSTPROCESSING_MOTION_BLUR_PROPERTY_DST_COLOR_IMAGE_LAYER:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->dst_color_image_n_layer;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_DST_COLOR_IMAGE_MIPMAP_LEVEL:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->dst_color_image_n_mipmap;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_N_VELOCITY_SAMPLES_MAX:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->n_velocity_samples_max;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_LAYER:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->src_color_image_n_layer;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_MIPMAP_LEVEL:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->src_color_image_n_mipmap;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_VELOCITY_IMAGE_LAYER:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->src_velocity_image_n_layer;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_VELOCITY_IMAGE_MIPMAP_LEVEL:
        {
            *(unsigned int*) out_value_ptr = motion_blur_ptr->src_velocity_image_n_mipmap;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid postprocessing_motion_blur_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API void postprocessing_motion_blur_set_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                const void*                         value_ptr)
{
    _postprocessing_motion_blur* motion_blur_ptr = (_postprocessing_motion_blur*) motion_blur;

    switch (property)
    {
        case POSTPROCESSING_MOTION_BLUR_PROPERTY_DST_COLOR_IMAGE_LAYER:
        {
            motion_blur_ptr->dst_color_image_n_layer = *(unsigned int*) value_ptr;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_DST_COLOR_IMAGE_MIPMAP_LEVEL:
        {
            motion_blur_ptr->dst_color_image_n_mipmap = *(unsigned int*) value_ptr;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_N_VELOCITY_SAMPLES_MAX:
        {
            motion_blur_ptr->n_velocity_samples_max = *(unsigned int*) value_ptr;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_LAYER:
        {
            motion_blur_ptr->src_color_image_n_layer = *(unsigned int*) value_ptr;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_MIPMAP_LEVEL:
        {
            motion_blur_ptr->src_color_image_n_mipmap = *(unsigned int*) value_ptr;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_VELOCITY_IMAGE_LAYER:
        {
            motion_blur_ptr->src_velocity_image_n_layer = *(unsigned int*) value_ptr;

            break;
        }

        case POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_VELOCITY_IMAGE_MIPMAP_LEVEL:
        {
            motion_blur_ptr->src_velocity_image_n_mipmap = *(unsigned int*) value_ptr;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid postprocessing_motion_blur_property value.");
        }
    } /* switch (property) */
}
