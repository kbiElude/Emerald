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

DECLARE_HANDLE(demo_timeline);

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
    DEMO_TIMELINE_N_VIDEO_SEGMENTS,
} demo_timeline_property;

/* Timeline segment properties */
typedef enum
{
    /* system_time; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,

    /* system_time; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
} demo_timeline_segment_property;


/** TODO */
PUBLIC bool demo_timeline_add_segment(demo_timeline              timeline,
                                      demo_timeline_segment_type segment_type,
                                      system_time                start_time,
                                      system_time                end_time,
                                      demo_timeline_segment_id*  out_segment_id_ptr);

/** TODO.
 *
 *  NOTE: Internal usage only.
 */
PUBLIC demo_timeline demo_timeline_create();

/** TODO */
PUBLIC bool demo_timeline_delete_segment(demo_timeline              timeline,
                                         demo_timeline_segment_type segment_type,
                                         demo_timeline_segment_id   segment_id);

/** TODO */
PUBLIC bool demo_timeline_get_property(demo_timeline timeline,
                                       demo_timeline_property property,
                                       void*                  out_result_ptr);

/** TODO */
PUBLIC bool demo_timeline_get_segment_at_time(demo_timeline               timeline,
                                              demo_timeline_segment_type  segment_type,
                                              system_time                 time,
                                              demo_timeline_segment_id*   out_segment_id_ptr);

/** TODO */
PUBLIC bool demo_timeline_get_segment_id(demo_timeline              timeline,
                                         demo_timeline_segment_type segment_type,
                                         unsigned int               n_segment,
                                         demo_timeline_segment_id*  out_segment_id_ptr);

/** TODO */
PUBLIC bool demo_timeline_get_segment_property(demo_timeline                  timeline,
                                               demo_timeline_segment_type     segment_type,
                                               demo_timeline_segment_id       segment_id,
                                               demo_timeline_segment_property property,
                                               void*                          out_result_ptr);

/** TODO */
PUBLIC bool demo_timeline_move_segment(demo_timeline              timeline,
                                       demo_timeline_segment_type segment_type,
                                       demo_timeline_segment_id   segment_id,
                                       system_time                new_segment_start_time);

/** TODO.
 *
 *  NOTE: Internal usage only.
 */
PUBLIC void demo_timeline_release(demo_timeline timeline);

/** TODO */
PUBLIC bool demo_timeline_resize_segment(demo_timeline              timeline,
                                         demo_timeline_segment_type segment_type,
                                         demo_timeline_segment_id   segment_id,
                                         system_time                new_segment_duration);


#endif /* DEMO_TIMELINE_H */
