/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_timeline_video_segment.h"
#include "ogl/ogl_context.h"                 /* TODO: Remove OpenGL dependency ! */
#include "ogl/ogl_pipeline.h"                /* TODO: Remove OpenGL dependency ! (issue #121) */
#include "ogl/ogl_texture.h"                 /* TODO: Remove OpenGL dependency ! */
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


typedef struct _demo_timeline_video_segment_texture_input
{
    ral_texture_format        format;
    system_hashed_ansi_string name;
    unsigned int              n_layers;
    unsigned int              n_mipmaps;
    unsigned int              n_samples;
    ral_texture_type          type;

    ogl_texture bound_texture;

    explicit _demo_timeline_video_segment_texture_input(system_hashed_ansi_string in_name,
                                                        ral_texture_format        in_format,
                                                        unsigned int              in_n_layers,
                                                        unsigned int              in_n_mipmaps,
                                                        unsigned int              in_n_samples,
                                                        ral_texture_type          in_type)
    {
        bound_texture        = NULL;
        format               = in_format;
        name                 = in_name;
        n_layers             = in_n_layers;
        n_mipmaps            = in_n_mipmaps;
        n_samples            = in_n_samples;
        type                 = in_type;
    }

    ~_demo_timeline_video_segment_texture_input()
    {
        // ..
    }
} _demo_timeline_video_segment_texture_input;

typedef struct _demo_timeline_video_segment_texture_output
{
    ral_texture_format        format;
    system_hashed_ansi_string name;
    unsigned int              n_end_layer_index;
    unsigned int              n_end_mipmap_index;
    unsigned int              n_start_layer_index;
    unsigned int              n_start_mipmap_index;
    unsigned int              n_samples;
    ral_texture_type          type;

    ogl_texture bound_texture;

    explicit _demo_timeline_video_segment_texture_output(system_hashed_ansi_string in_name,
                                                         ral_texture_format        in_format,
                                                         unsigned int              in_n_start_layer_index,
                                                         unsigned int              in_n_end_layer_index,
                                                         unsigned int              in_n_start_mipmap_index,
                                                         unsigned int              in_n_end_mipmap_index,
                                                         unsigned int              in_n_samples,
                                                         ral_texture_type          in_type)
    {
        bound_texture        = NULL;
        format               = in_format;
        name                 = in_name;
        n_end_layer_index    = in_n_end_layer_index;
        n_end_mipmap_index   = in_n_end_mipmap_index;
        n_samples            = in_n_samples;
        n_start_layer_index  = in_n_start_layer_index;
        n_start_mipmap_index = in_n_start_mipmap_index;
        type                 = in_type;
    }

    ~_demo_timeline_video_segment_texture_output()
    {
        if (bound_texture != NULL)
        {
            ogl_texture_release(bound_texture);

            bound_texture = NULL;
        } /* if (bound_texture != NULL) */
    }
} _demo_timeline_video_segment_texture_output;

typedef struct _demo_timeline_video_segment
{
    float                     aspect_ratio;       /* AR to use for the video segment */
    ogl_context               context;
    system_hashed_ansi_string name;
    ogl_pipeline              pipeline;
    uint32_t                  pipeline_stage_id;
    system_resizable_vector   texture_inputs;
    system_resizable_vector   texture_outputs;
    demo_timeline             timeline;

    REFCOUNT_INSERT_VARIABLES


    explicit _demo_timeline_video_segment(ogl_context               in_context,
                                          system_hashed_ansi_string in_name,
                                          ogl_pipeline              in_pipeline,
                                          demo_timeline             in_timeline)
    {
        aspect_ratio       = 1.0f;
        context            = in_context;
        name               = in_name;
        pipeline           = in_pipeline;
        pipeline_stage_id  = -1;
        texture_inputs     = system_resizable_vector_create(4 /* capacity */);
        texture_outputs    = system_resizable_vector_create(4 /* capacity */);
        timeline           = in_timeline;

        ASSERT_ALWAYS_SYNC(texture_inputs != NULL,
                           "Could not spawn the video segment texture inputs vector");
        ASSERT_ALWAYS_SYNC(texture_outputs != NULL,
                           "Could not spawn the video segment textureoutputs vector");
    }

    ~_demo_timeline_video_segment()
    {
        if (texture_inputs != NULL)
        {
            _demo_timeline_video_segment_texture_input* texture_input_ptr = NULL;

            while (system_resizable_vector_pop(texture_inputs,
                                              &texture_input_ptr) )
            {
                delete texture_input_ptr;

                texture_input_ptr = NULL;
            }

            system_resizable_vector_release(texture_inputs);
            texture_inputs = NULL;
        } /* if (texture_inputs != NULL) */

        if (texture_outputs != NULL)
        {
            _demo_timeline_video_segment_texture_output* texture_output_ptr = NULL;

            while (system_resizable_vector_pop(texture_outputs,
                                              &texture_output_ptr) )
            {
                delete texture_output_ptr;

                texture_output_ptr = NULL;
            }

            system_resizable_vector_release(texture_outputs);
            texture_outputs = NULL;
        } /* if (texture_outputs != NULL) */
    }
} _demo_timeline_video_segment;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline_video_segment,
                               demo_timeline_video_segment,
                              _demo_timeline_video_segment);


/* TODO */
PRIVATE void _demo_timeline_video_segment_release(void* segment)
{
    /* Tear-down handled by the destructor. */
}


/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_passes(demo_timeline_video_segment             segment,
                                                               unsigned int                            n_passes,
                                                               const demo_timeline_video_segment_pass* passes)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");
    ASSERT_DEBUG_SYNC(n_passes > 0,
                      "No video segment passes to add");

    /* Create a new pipeline stage.. */
    segment_ptr->pipeline_stage_id = ogl_pipeline_add_stage(segment_ptr->pipeline);

    /* Set up the pipeline stage steps using the pass info structures */
    for (unsigned int n_pass = 0;
                      n_pass < n_passes;
                    ++n_pass)
    {
        /* Note: we don't need to store the step id so we ignore the return value */
        ogl_pipeline_add_stage_step(segment_ptr->pipeline,
                                    segment_ptr->pipeline_stage_id,
                                    passes[n_pass].name,
                                    passes[n_pass].pfn_callback_proc,
                                    passes[n_pass].user_arg);
    } /* for (all rendering passes) */

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_texture_input(demo_timeline_video_segment           segment,
                                                                      system_hashed_ansi_string             input_name,
                                                                      ral_texture_type                      texture_type,
                                                                      ral_texture_format                    texture_format,
                                                                      unsigned int                          texture_n_layers,
                                                                      unsigned int                          texture_n_mipmaps,
                                                                      unsigned int                          texture_n_samples,
                                                                      demo_timeline_video_segment_input_id* out_input_id_ptr)
{
    bool                          result      = false;
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    ASSERT_DEBUG_SYNC(texture_format != RAL_TEXTURE_FORMAT_UNKNOWN,
                      "Unknown texture format is not allowed");
    ASSERT_DEBUG_SYNC(texture_type != RAL_TEXTURE_TYPE_UNKNOWN,
                      "Unknown texture type is not allowed");
    ASSERT_DEBUG_SYNC(texture_n_layers >= 1,
                      "Number of texture layers must be at least 1.");
    ASSERT_DEBUG_SYNC(texture_n_mipmaps >= 1,
                      "Number of texture mipmaps must be at least 1.");
    ASSERT_DEBUG_SYNC(texture_n_samples >= 1,
                      "Number of texture samples must be at least 1");

    ASSERT_DEBUG_SYNC(!(texture_n_layers > 1 && !(ral_utils_is_texture_type_cubemap(texture_type) ||
                                                  ral_utils_is_texture_type_layered(texture_type) )),
                      "Specified texture type is neither layered nor a cubemap type, but texture_n_layers is [%d]",
                      texture_n_layers);

    ASSERT_DEBUG_SYNC(!(texture_n_mipmaps > 1 && !ral_utils_is_texture_type_mipmappable(texture_type) ),
                      "Specified texture type is not mipmappable, but texture_n_mipmaps is [%d]",
                      texture_n_mipmaps);

    ASSERT_DEBUG_SYNC(!(texture_n_samples > 1 && !ral_utils_is_texture_type_multisample(texture_type) ),
                      "Specified texture type is not multisample but texture_n_samples is [%d]",
                      texture_n_samples);

    /* Create a new texture output instance */
    _demo_timeline_video_segment_texture_input* new_texture_input_ptr = new (std::nothrow) _demo_timeline_video_segment_texture_input(input_name,
                                                                                                                                      texture_format,
                                                                                                                                      texture_n_layers,
                                                                                                                                      texture_n_mipmaps,
                                                                                                                                      texture_n_samples,
                                                                                                                                      texture_type);

    ASSERT_DEBUG_SYNC(new_texture_input_ptr != NULL,
                      "Out of memory");

    if (new_texture_input_ptr != NULL)
    {
        /* Store it */
        system_resizable_vector_get_property(segment_ptr->texture_inputs,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             out_input_id_ptr);

        system_resizable_vector_push(segment_ptr->texture_inputs,
                                     new_texture_input_ptr);

        result = true;
    } /* if (new_texture_input_ptr != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_texture_output(demo_timeline_video_segment            segment,
                                                                       system_hashed_ansi_string              output_name,
                                                                       ral_texture_type                       texture_type,
                                                                       ral_texture_format                     texture_format,
                                                                       unsigned int                           texture_n_start_layer_index,
                                                                       unsigned int                           texture_n_end_layer_index,
                                                                       unsigned int                           texture_n_start_mipmap_index,
                                                                       unsigned int                           texture_n_end_mipmap_index,
                                                                       unsigned int                           texture_n_samples,
                                                                       demo_timeline_video_segment_output_id* out_output_id_ptr)
{
    bool                          result      = false;
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    ASSERT_DEBUG_SYNC(texture_format != RAL_TEXTURE_FORMAT_UNKNOWN,
                      "Unknown texture format is not allowed");
    ASSERT_DEBUG_SYNC(texture_type != RAL_TEXTURE_TYPE_UNKNOWN,
                      "Unknown texture type is not allowed");
    ASSERT_DEBUG_SYNC(texture_n_start_layer_index <= texture_n_end_layer_index,
                      "Start layer index must be smaller than or equal to the end layer index");
    ASSERT_DEBUG_SYNC(texture_n_start_mipmap_index <= texture_n_end_mipmap_index,
                      "Start mipmap index must be smaller than or equal to the end mipmap index");
    ASSERT_DEBUG_SYNC(texture_n_samples >= 1,
                      "Output texture must not use 0 samples.");

    /* Create a new texture output instance */
    _demo_timeline_video_segment_texture_output* new_texture_output_ptr = new (std::nothrow) _demo_timeline_video_segment_texture_output(output_name,
                                                                                                                                         texture_format,
                                                                                                                                         texture_n_start_layer_index,
                                                                                                                                         texture_n_end_layer_index,
                                                                                                                                         texture_n_start_mipmap_index,
                                                                                                                                         texture_n_end_mipmap_index,
                                                                                                                                         texture_n_samples,
                                                                                                                                         texture_type);

    ASSERT_DEBUG_SYNC(new_texture_output_ptr != NULL,
                      "Out of memory");

    if (new_texture_output_ptr != NULL)
    {
        /* Store it */
        system_resizable_vector_get_property(segment_ptr->texture_outputs,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             out_output_id_ptr);

        system_resizable_vector_push(segment_ptr->texture_outputs,
                                     new_texture_output_ptr);

        result = true;
    } /* if (new_texture_output_ptr != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_attach_texture_to_texture_input(demo_timeline_video_segment          segment,
                                                                        demo_timeline_video_segment_input_id input_id,
                                                                        ogl_texture                          texture,
                                                                        unsigned int                         n_start_layer,
                                                                        unsigned int                         n_start_mipmap)
{
    bool                                        result            = false;
    _demo_timeline_video_segment*               segment_ptr       = (_demo_timeline_video_segment*) segment;
    _demo_timeline_video_segment_texture_input* texture_input_ptr = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment_ptr != NULL,
                      "Input video segment is NULL");

    if (!system_resizable_vector_get_element_at(segment_ptr->texture_inputs,
                                                input_id,
                                               &texture_input_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture input with ID [%d]",
                          input_id);

        goto end;
    } /* if (texture input descriptor could not've been retrieved) */

    // ...

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC demo_timeline_video_segment demo_timeline_video_segment_create(ogl_context                      context,
                                                                      ogl_pipeline                     pipeline,
                                                                      demo_timeline                    owner_timeline,
                                                                      system_hashed_ansi_string        name,
                                                                      demo_timeline_video_segment_type type)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != NULL,
                      "Rendering context handle is NULL");
    ASSERT_DEBUG_SYNC(owner_timeline != NULL,
                      "Owner timeline is NULL");
    ASSERT_DEBUG_SYNC(name != NULL                                  &&
                      system_hashed_ansi_string_get_length(name) > 0,
                      "Invalid video segment name specified");
    ASSERT_DEBUG_SYNC(type == DEMO_TIMELINE_VIDEO_SEGMENT_TYPE_PASS_BASED,
                      "Only pass-based video segments are supported");

    /* Create a new instance */
    _demo_timeline_video_segment* new_segment_ptr = new (std::nothrow) _demo_timeline_video_segment(context,
                                                                                                    name,
                                                                                                    pipeline,
                                                                                                    owner_timeline);

    ASSERT_ALWAYS_SYNC(new_segment_ptr != NULL,
                       "Out of memory");

    if (new_segment_ptr != NULL)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_segment_ptr,
                                                       _demo_timeline_video_segment_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE_VIDEO_SEGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timeline Segment (Video)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_segment_ptr != NULL) */

    return (demo_timeline_video_segment) new_segment_ptr;
}

/** Please see header for specification */
PUBLIC void demo_timeline_video_segment_get_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     void*                                out_result_ptr)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_ASPECT_RATIO:
        {
            *(float*) out_result_ptr = segment_ptr->aspect_ratio;

            break;
        }

        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = segment_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized video segment property value.")
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_video_segment_render(demo_timeline_video_segment segment,
                                                                                  uint32_t                    frame_index,
                                                                                  system_time                 rendering_pipeline_time,
                                                                                  const int*                  rendering_area_px_topdown)
{
    bool                          result;
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    result = ogl_pipeline_draw_stage(segment_ptr->pipeline,
                                     segment_ptr->pipeline_stage_id,
                                     frame_index,
                                     rendering_pipeline_time,
                                     rendering_area_px_topdown);

    return result;

}

/** Please see header for specification */
PUBLIC void demo_timeline_video_segment_set_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     const void*                          data)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_ASPECT_RATIO:
        {
            segment_ptr->aspect_ratio = *(float*) data;

            ASSERT_DEBUG_SYNC(segment_ptr->aspect_ratio > 0.0f,
                              "A segment AR of <= 0.0 value was requested!");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized video segment property requested.");
        }
    } /* switch (property) */
}