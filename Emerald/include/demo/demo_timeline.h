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
    /* system_time; not settable.
     *
     * End time of the segment located the furthest from the beginning of the timeline */
    DEMO_TIMELINE_PROPERTY_DURATION,

    /* unsigned int; not settable. */
    DEMO_TIMELINE_PROPERTY_N_VIDEO_SEGMENTS,
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


/** TODO */
PUBLIC EMERALD_API bool demo_timeline_add_video_segment(demo_timeline              timeline,
                                                        system_hashed_ansi_string  name,
                                                        system_time                start_time,
                                                        system_time                end_time,
                                                        demo_timeline_segment_id*  out_segment_id_ptr);

/** TODO.
 *
 *  NOTE: Internal usage only.
 */
PUBLIC EMERALD_API demo_timeline demo_timeline_create(system_hashed_ansi_string name,
                                                      ogl_context               context);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_delete_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_property(demo_timeline timeline,
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

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_render(demo_timeline timeline,
                                                                    system_time   frame_time);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_resize_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id,
                                                     system_time                new_segment_duration);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_set_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           const void*                    data);

#endif /* DEMO_TIMELINE_H */
