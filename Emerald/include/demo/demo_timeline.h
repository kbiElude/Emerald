/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO
 *
 */
#ifndef DEMO_TIMELINE_H
#define DEMO_TIMELINE_H

#include "system/system_types.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_types.h"

DECLARE_HANDLE(demo_timeline);

REFCOUNT_INSERT_DECLARATIONS(demo_timeline,
                             demo_timeline)

/* Segment ID */
typedef unsigned int demo_timeline_segment_id;

/* Segment type */
typedef enum
{
    DEMO_TIMELINE_SEGMENT_TYPE_FIRST,

    /* TODO: DEMO_TIMELINE_SEGMENT_TYPE_AUDIO, */
    DEMO_TIMELINE_SEGMENT_TYPE_VIDEO = DEMO_TIMELINE_SEGMENT_TYPE_FIRST,

    /* Always last */
    DEMO_TIMELINE_SEGMENT_TYPE_COUNT
} demo_timeline_segment_type;

/* Timeline properties */
typedef enum
{
    /* float; settable.
     *
     * Determines proportions of the viewport for frame times, for which no video segments
     * are defined.
     *
     * Use special demo_timeline_get_aspect_ratio() getter to query the AR value for a specific
     * frame time.
     */
    DEMO_TIMELINE_PROPERTY_ASPECT_RATIO,

    /* system_time; not settable.
     *
     * End time of the segment located the furthest from the beginning of the timeline */
    DEMO_TIMELINE_PROPERTY_DURATION,

    /* unsigned int; not settable. */
    DEMO_TIMELINE_PROPERTY_N_VIDEO_SEGMENTS,

    /* ogl_pipeline; not settable. */
    DEMO_TIMELINE_PROPERTY_PIPELINE,
} demo_timeline_property;

typedef enum
{
    /* Rendering pipeline will be fed frame time relative to the beginning
     * of the segment.
     */
    DEMO_TIMELINE_SEGMENT_PIPELINE_TIME_RELATIVE_TO_SEGMENT,

    /* Rendering pipeline will be fed frame time relative to the beginning
     * of the timeline.
     */
    DEMO_TIMELINE_SEGMENT_PIPELINE_TIME_RELATIVE_TO_TIMELINE,
} demo_timeline_segment_pipeline_time;

/* Timeline segment properties */
typedef enum
{
    /* float; settable.
     *
     * Used by ogl_rendering_handler to specify the rendering area to the rendering call-back handlers. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO,

    /* system_time; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,

    /* system_hashed_ansi_string; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_NAME,

    /* uint32_t; not settable.
     *
     * NOTE: Video segments only. The getter func will throw an assertion failure,
     *       should this property be queried for other segment types.
     */
    DEMO_TIMELINE_SEGMENT_PROPERTY_PIPELINE_STAGE,

    /* demo_timeline_segment_pipeline_time; settable.
     *
     * Default value: DEMO_TIMELINE_SEGMENT_PIPELINE_TIME_RELATIVE_TO_TIMELINE
     */
    DEMO_TIMELINE_SEGMENT_PROPERTY_PIPELINE_TIME,

    /* system_time; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
} demo_timeline_segment_property;

/* Helper structure used to declare a single pass of a video segment.
 *
 * Only used by demo_timeline_add_video_segment()
 */
typedef struct
{
    /* Name of the pass. Used when presenting performance statistics */
    system_hashed_ansi_string name;

    /* Function to call back to render stage contents. */
    PFNOGLPIPELINECALLBACKPROC pfn_callback_proc;

    /* User data to pass with the call-back */
    void* user_arg;
} demo_timeline_segment_pass;

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_add_video_segment(demo_timeline                     timeline,
                                                        system_hashed_ansi_string         name,
                                                        system_time                       start_time,
                                                        system_time                       end_time,
                                                        float                             aspect_ratio,
                                                        unsigned int                      n_passes,
                                                        const demo_timeline_segment_pass* passes,              /* must hold n_passes instances */
                                                        demo_timeline_segment_id*         out_opt_segment_id_ptr);

/** TODO. */
PUBLIC EMERALD_API demo_timeline demo_timeline_create(system_hashed_ansi_string name,
                                                      ogl_context               context);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_delete_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id);

/** TODO */
PUBLIC EMERALD_API float demo_timeline_get_aspect_ratio(demo_timeline timeline,
                                                        system_time   frame_time);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_property(demo_timeline          timeline,
                                                   demo_timeline_property property,
                                                   void*                  out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_segment_at_time(demo_timeline               timeline,
                                                          demo_timeline_segment_type  segment_type,
                                                          system_time                 time,
                                                          demo_timeline_segment_id*   out_segment_id_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_segment_id(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     unsigned int               n_segment,
                                                     demo_timeline_segment_id*  out_segment_id_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           void*                          out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_move_segment(demo_timeline              timeline,
                                                   demo_timeline_segment_type segment_type,
                                                   demo_timeline_segment_id   segment_id,
                                                   system_time                new_segment_start_time);

/** TODO
 *
 *  @param timeline          TODO
 *  @param frame_time        TODO
 *  @param rendering_area_px_topdown [0]: x1 of the rendering area (in pixels)
 *                           [1]: y1 of the rendering area (in pixels)
 *                           [2]: x2 of the rendering area (in pixels)
 *                           [3]: y2 of the rendering area (in pixels)
 *
 *  @return TODO 
 */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_render(demo_timeline timeline,
                                                                    system_time   frame_time,
                                                                    const int*    rendering_area_px_topdown);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_resize_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id,
                                                     system_time                new_segment_duration);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_set_property(demo_timeline          timeline,
                                                   demo_timeline_property property,
                                                   const void*            data);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_set_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           const void*                    data);

#endif /* DEMO_TIMELINE_H */
