/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_timeline_video_segment.h"
#include "ogl/ogl_context.h"                 /* TODO: Remove OpenGL dependency ! */
#include "ogl/ogl_pipeline.h"                /* TODO: Remove OpenGL dependency ! (issue #121) */
#include "system/system_assertions.h"
#include "system/system_log.h"

typedef struct _demo_timeline_video_segment
{
    float                     aspect_ratio;       /* AR to use for the video segment */
    ogl_context               context;
    system_hashed_ansi_string name;
    ogl_pipeline              pipeline;
    uint32_t                  pipeline_stage_id;
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
        timeline           = in_timeline;
    }
} _demo_timeline_video_segment;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline_video_segment,
                               demo_timeline_video_segment,
                              _demo_timeline_video_segment);


/* TODO */
PRIVATE void _demo_timeline_video_segment_release(void* segment)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Nothing to release at the moment */
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