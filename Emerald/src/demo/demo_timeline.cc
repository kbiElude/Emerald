/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO: The existing implementation uses the most crude brute-force method on
 *       the planet to determine which video segment should be played at a given
 *       time. There's definitely a lot of space to be won in this area - especially
 *       once we start supporting many video segments defined for a given frame time.
 *       However, in the general case where the timeline is played from the beginning
 *       till the end, and there's only one video segment available for a given time quant,
 *       I'm not expecting huge benefits from optimizing this code.
 *       Things might change in the future when the latter restriction is lifted.
 */
#include "shared.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"                 /* TODO: Remove OpenGL dependency ! */
#include "ogl/ogl_pipeline.h"                /* TODO: Remove OpenGL dependency ! (issue #121) */
#include "system/system_atomics.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"

typedef struct _demo_timeline_segment
{
    float                               aspect_ratio;       /* AR to use for the video segment */
    system_time                         end_time;
    demo_timeline_segment_id            id;
    system_hashed_ansi_string           name;
    uint32_t                            pipeline_stage_id;
    demo_timeline_segment_pipeline_time pipeline_time_mode;
    system_time                         start_time;
    demo_timeline_segment_type          type;

    /* Video segment constructor */
    explicit _demo_timeline_segment(system_hashed_ansi_string  in_name,
                                    system_time                in_start_time,
                                    system_time                in_end_time,
                                    demo_timeline_segment_id   in_id)
    {
        aspect_ratio       = 1.0f;
        end_time           = in_end_time;
        id                 = in_id;
        name               = in_name;
        pipeline_stage_id  = -1;
        pipeline_time_mode = DEMO_TIMELINE_SEGMENT_PIPELINE_TIME_RELATIVE_TO_TIMELINE;
        start_time         = in_start_time;
        type               = DEMO_TIMELINE_SEGMENT_TYPE_VIDEO;
    }
} _demo_timeline_segment;

typedef struct _demo_timeline
{
    float                   aspect_ratio;                    /* AR for frame times, for which no video segments are defined */
    ogl_context             context;
    system_critical_section cs;
    system_time             duration;
    _demo_timeline_segment* last_used_video_segment_ptr;
    ogl_pipeline            rendering_pipeline;
    system_hash64map        segment_id_to_video_segment_map; /* maps system_timeline_segment_id to _demo_timeline_segment instances */
    unsigned int            video_segment_id_counter;
    system_resizable_vector video_segments;                  /* stores _demo_timeline_segment instances;
                                                              * DOES NOT own the segment instances;
                                                              * DOES NOT store the segments ordered by IDs; */

    REFCOUNT_INSERT_VARIABLES;

    explicit _demo_timeline(ogl_context in_context)
    {
        ASSERT_DEBUG_SYNC(in_context != NULL,
                          "Input rendering context is NULL");

        aspect_ratio                    = 1.0f;
        context                         = in_context;
        cs                              = system_critical_section_create();
        duration                        = 0;
        last_used_video_segment_ptr     = NULL;
        rendering_pipeline              = ogl_pipeline_create(in_context,
#ifdef _DEBUG
                                                              true, /* should_overlay_performance_info */
#else
                                                              false, /* should_overlay_performance_info */
#endif
                                                              system_hashed_ansi_string_create("Demo timeline rendering pipeline") );
        segment_id_to_video_segment_map = system_hash64map_create(sizeof(_demo_timeline_segment*) );
        video_segment_id_counter        = 0;
        video_segments                  = system_resizable_vector_create(sizeof(_demo_timeline_segment*) );

        ASSERT_DEBUG_SYNC (rendering_pipeline != NULL,
                           "Could not spawn a rendering pipeline instance.");
        ASSERT_ALWAYS_SYNC(segment_id_to_video_segment_map != NULL,
                           "Out of memory");

        ogl_context_retain(in_context);
    }

    ~_demo_timeline()
    {
        if (cs != NULL)
        {
            system_critical_section_release(cs);

            cs = NULL;
        }

        if (rendering_pipeline != NULL)
        {
            ogl_pipeline_release(rendering_pipeline);

            rendering_pipeline = NULL;
        }

        if (context != NULL)
        {
            ogl_context_release(context);

            context = NULL;
        }

        if (segment_id_to_video_segment_map != NULL)
        {
            _demo_timeline_segment* current_video_segment_ptr = NULL;
            unsigned int            n_video_segments          = 0;

            system_hash64map_get_property(segment_id_to_video_segment_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_video_segments);

            for (uint32_t n_video_segment = 0;
                          n_video_segment < n_video_segments;
                        ++n_video_segment)
            {
                if (!system_hash64map_get_element_at(segment_id_to_video_segment_map,
                                                     n_video_segment,
                                                    &current_video_segment_ptr,
                                                     NULL) ) /* result_hash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve video segment at index [%d]",
                                      n_video_segment);

                    continue;
                }

                delete current_video_segment_ptr;

                current_video_segment_ptr = NULL;
            } /* for (all video segments) */

            system_hash64map_release(segment_id_to_video_segment_map);
            segment_id_to_video_segment_map = NULL;
        } /* if (segment_id_to_video_segment_map != NULL) */

        if (video_segments != NULL)
        {
            system_resizable_vector_release(video_segments);

            video_segments = NULL;
        } /* if (video_segments != NULL) */
    }
} _demo_timeline;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline,
                               demo_timeline,
                              _demo_timeline);

/** Forward declarations */
PRIVATE bool _demo_timeline_change_segment_start_end_times(_demo_timeline*                 timeline_ptr,
                                                           _demo_timeline_segment*         segment_ptr,
                                                           system_time                     new_start_time,
                                                           system_time                     new_end_time);
PRIVATE bool _demo_timeline_get_segment_containers        (_demo_timeline*                 timeline_ptr,
                                                           demo_timeline_segment_type      segment_type,
                                                           unsigned int**                  out_segment_id_counter_ptr_ptr,
                                                           system_resizable_vector*        out_segment_vector_ptr,
                                                           system_hash64map*               out_segment_hash64map_ptr);
PRIVATE bool _demo_timeline_is_region_segment_free        (_demo_timeline*                 timeline_ptr,
                                                           demo_timeline_segment_type      segment_type,
                                                           system_time                     start_time,
                                                           system_time                     end_time,
                                                           const demo_timeline_segment_id* opt_excluded_segment_id_ptr,
                                                           bool*                           out_result_ptr);
PRIVATE void _demo_timeline_release                       (void*                           timeline);
PRIVATE bool _demo_timeline_update_duration               (_demo_timeline*                 timeline_ptr);


/** TODO
 *
 *  NOTE: This implementation assumes timeline_ptr::cs is entered at the time of the call!
 */
PRIVATE bool _demo_timeline_change_segment_start_end_times(_demo_timeline*         timeline_ptr,
                                                           _demo_timeline_segment* segment_ptr,
                                                           system_time             new_start_time,
                                                           system_time             new_end_time)
{
    bool is_region_free = false;
    bool result         = false;

    /* Any segments overlapping with the proposed region? */
    if (!_demo_timeline_is_region_segment_free(timeline_ptr,
                                               segment_ptr->type,
                                               new_start_time,
                                               new_end_time,
                                              &segment_ptr->id,
                                              &is_region_free) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "_demo_timeline_is_region_segment_Free() call failed.");

        goto end;
    }

    if (!is_region_free)
    {
        /* Cannot move segment - requested region is already occupied. */
        goto end;
    }

    /* Adjust start & end times of the segment */
    ASSERT_DEBUG_SYNC(new_start_time < new_end_time,
                      "New segment's duration is <= 0!");

    segment_ptr->start_time = new_start_time;
    segment_ptr->end_time   = new_end_time;

    /* All done */
    result = true;

end:
    return result;
}

/** TODO
 *
 *  NOTE: This implementation assumes timeline_ptr::cs is entered at the time of the call!
 */
PRIVATE bool _demo_timeline_get_segment_containers(_demo_timeline*            timeline_ptr,
                                                   demo_timeline_segment_type segment_type,
                                                   unsigned int**             out_segment_id_counter_ptr_ptr,
                                                   system_resizable_vector*   out_segment_vector_ptr,
                                                   system_hash64map*          out_segment_hash64map_ptr)
{
    bool result = true;

    switch (segment_type)
    {
        case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
        {
            if (out_segment_hash64map_ptr != NULL)
            {
                *out_segment_hash64map_ptr = timeline_ptr->segment_id_to_video_segment_map;
            }

            if (out_segment_id_counter_ptr_ptr != NULL)
            {
                *out_segment_id_counter_ptr_ptr = &timeline_ptr->video_segment_id_counter;
            }

            if (out_segment_vector_ptr != NULL)
            {
                *out_segment_vector_ptr = timeline_ptr->video_segments;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized segment type");

            result = false;
        }
    } /* switch (segment_type) */

    return result;
}

/** TODO
 *
 *  NOTE: This implementation assumes timeline_ptr::cs is entered at the time of the call!
 */
PRIVATE bool _demo_timeline_is_region_segment_free(_demo_timeline*                   timeline_ptr,
                                                   demo_timeline_segment_type        segment_type,
                                                   system_time                       start_time,
                                                   system_time                       end_time,
                                                   const demo_timeline_segment_id*   opt_excluded_segment_id_ptr,
                                                   bool*                             out_result_ptr)
{
    bool                    overlap_status  = false;
    unsigned int            n_segments      = 0;
    bool                    result          = false;
    system_resizable_vector segments_vector = NULL;

    if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                               segment_type,
                                               NULL, /* out_segment_id_counter_ptr_ptr */
                                              &segments_vector,
                                               NULL) )/* out_segment_hash64map_ptr */
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot retrieve segments vector for the requested segment type.");

        goto end;
    }

    /* Iterate over all segments and check if any of those overlap with the specified time region.
     * In order to perform the test, we first observe that there are two cases where regions
     * do not overlap:
     *
     * AAA
     *    [delta]BBB
     *
     *           AAA
     * BBB[delta]
     *
     * NOTE: In order to allow for segments to be glued to each other, we require delta
     *       to be larger than zero.
     *
     * Regions overlap if these two cases fail.
     */
    system_resizable_vector_get_property(segments_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_segments);

    for (unsigned int n_segment = 0;
                      n_segment < n_segments;
                    ++n_segment)
    {
        _demo_timeline_segment* current_segment_ptr = NULL;
        bool                    does_not_overlap    = false;
        bool                    does_overlap        = true;

        if (!system_resizable_vector_get_element_at(segments_vector,
                                                    n_segment,
                                                   &current_segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment descriptor at index [%d]",
                              n_segment);

            continue;
        }

        does_not_overlap = (current_segment_ptr->start_time <= start_time  &&
                            current_segment_ptr->end_time   <= start_time) ||
                           (current_segment_ptr->start_time >= end_time    &&
                            current_segment_ptr->end_time   >= end_time);
        does_overlap     = !does_not_overlap;

        if (opt_excluded_segment_id_ptr != NULL)
        {
            if (does_overlap                                            &&
                current_segment_ptr->id == *opt_excluded_segment_id_ptr)
            {
                /* We were explicitly asked to skip this segment. */
                continue;
            }
        } /* if (opt_excluded_segment_id_ptr != NULL) */

        if (does_overlap)
        {
            overlap_status = true;

            break;
        }
    } /* for (all segments) */

    /* All done */
    if (out_result_ptr != NULL)
    {
        *out_result_ptr = !overlap_status;
    }

    result = true;
end:
    return result;
}

/** TODO */
PRIVATE void _demo_timeline_release(void* timeline)
{
    /* Stub: the tear-down process is handled completely by the destructor. */
}

/** TODO
 *
 *  NOTE: This implementation assumes timeline_ptr::cs is entered at the time of the call!
 */
PRIVATE bool _demo_timeline_update_duration(_demo_timeline* timeline_ptr)
{
    system_time max_segment_end_time = 0;
    bool        result               = false;

    /* Iterate over all supported segment types */
    for (demo_timeline_segment_type current_segment_type = DEMO_TIMELINE_SEGMENT_TYPE_FIRST;
                                    current_segment_type < DEMO_TIMELINE_SEGMENT_TYPE_COUNT;
                           ++(int&) current_segment_type)
    {
        unsigned int            n_segments      = 0;
        system_resizable_vector segments_vector = NULL;

        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   current_segment_type,
                                                   NULL,   /* out_segment_id_counter_ptr */
                                                  &segments_vector,
                                                   NULL) ) /* out_segment_hash64map_ptr */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segments vector for the requested segment type.");

            goto end;
        }

        /* Loop over all created segments */
        system_resizable_vector_get_property(segments_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_segments);

        for (unsigned int n_current_segment = 0;
                          n_current_segment < n_segments;
                        ++n_current_segment)
        {
            _demo_timeline_segment* current_segment_ptr = NULL;

            if (!system_resizable_vector_get_element_at(segments_vector,
                                                        n_current_segment,
                                                       &current_segment_ptr))
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve segment descriptor at index [%d]",
                                  n_current_segment);

                goto end;
            }

            if (current_segment_ptr->end_time > max_segment_end_time)
            {
                max_segment_end_time = current_segment_ptr->end_time;
            }
        } /* for (all known segments) */
    } /* for (all known segment types) */

    timeline_ptr->duration = max_segment_end_time;

    /* All done */
    result = true;

end:
    return result;
}


/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_add_video_segment(demo_timeline                     timeline,
                                                        system_hashed_ansi_string         name,
                                                        system_time                       start_time,
                                                        system_time                       end_time,
                                                        float                             aspect_ratio,
                                                        unsigned int                      n_passes,
                                                        const demo_timeline_segment_pass* passes,              /* must hold n_passes instances */
                                                        demo_timeline_segment_id*         out_opt_segment_id_ptr)
{
    bool                     is_region_free         = false;
    demo_timeline_segment_id new_segment_id         = -1;
    uint32_t                 new_segment_stage_id   = -1;
    _demo_timeline_segment*  new_segment_ptr        = NULL;
    system_hash64map         owning_hash64map       = NULL;
    system_resizable_vector  owning_vector          = NULL;
    bool                     result                 = false;
    unsigned int*            segment_id_counter_ptr = NULL;
    _demo_timeline*          timeline_ptr           = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(start_time < end_time,
                      "Segment's start time must be smaller than its end time");
    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Timeline instance is NULL");

    if (start_time         >= end_time ||
        timeline_ptr       == NULL)
    {
        goto end;
    }

    /* Make sure there's enough room to fit the new segment */
    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_is_region_segment_free(timeline_ptr,
                                                   DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                                   start_time,
                                                   end_time,
                                                   NULL, /* opt_excluded_segment_id_ptr */
                                                   &is_region_free) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "_demo_timeline_is_region_segment_free() call failed.");

            goto end;
        }

        if (!is_region_free)
        {
            LOG_ERROR("Cannot add a segment to the timeline - region is already occupied.")

            goto end;
        }

        /* Determine which hash map & resizable vector the new descriptor should be stored in */
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                                  &segment_id_counter_ptr,
                                                  &owning_vector,
                                                  &owning_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not determine what hash map and/or resizable vector the new segment should be stored in");

            goto end;
        }

        /* Spawn a new descriptor */
        new_segment_id  = system_atomics_increment(segment_id_counter_ptr);
        new_segment_ptr = new (std::nothrow) _demo_timeline_segment(name,
                                                                    start_time,
                                                                    end_time,
                                                                    new_segment_id);

        ASSERT_ALWAYS_SYNC(new_segment_ptr != NULL,
                           "Could not spawn a new segment descriptor");

        if (new_segment_ptr == NULL)
        {
            goto end;
        }

        new_segment_stage_id               = ogl_pipeline_add_stage(timeline_ptr->rendering_pipeline);
        new_segment_ptr->pipeline_stage_id = new_segment_stage_id;

        /* Store it in the containers */
        if (!system_hash64map_insert(owning_hash64map,
                                     (system_hash64) new_segment_id,
                                     new_segment_ptr,
                                     NULL, /* callback */
                                     NULL) )/* callback_user_arg */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not store the new segment descriptor in the corresponding hash map.");

            goto end;
        }

        system_resizable_vector_push(owning_vector,
                                     new_segment_ptr);

        /* Update the timeline duration, if the new segment's end time exceeds it */
        if (end_time > timeline_ptr->duration)
        {
            timeline_ptr->duration = end_time;
        }

        /* Set up the pipeline stage steps using the pass info structures */
        for (unsigned int n_pass = 0;
                          n_pass < n_passes;
                        ++n_pass)
        {
            uint32_t step_id = ogl_pipeline_add_stage_step(timeline_ptr->rendering_pipeline,
                                                           new_segment_stage_id,
                                                           passes[n_pass].name,
                                                           passes[n_pass].pfn_callback_proc,
                                                           passes[n_pass].user_arg);

            new_segment_ptr->aspect_ratio = aspect_ratio;
        } /* for (all rendering passes) */

        /* All done */
        if (out_opt_segment_id_ptr != NULL)
        {
            *out_opt_segment_id_ptr = new_segment_id;
        }

        result = true;
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    if (!result)
    {
        if (new_segment_ptr != NULL)
        {
            delete new_segment_ptr;

            new_segment_ptr = NULL;
        } /* if (new_segment_ptr != NULL) */
    } /* if (!result) */

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API demo_timeline demo_timeline_create(system_hashed_ansi_string name,
                                                      ogl_context               context)
{
    _demo_timeline* timeline_ptr = new (std::nothrow) _demo_timeline(context);

    ASSERT_ALWAYS_SYNC(timeline_ptr != NULL,
                       "Out of memory");

    if (timeline_ptr != NULL)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(timeline_ptr,
                                                       _demo_timeline_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timelines\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (demo_timeline) timeline_ptr;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_delete_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id)
{
    system_hash64map        owning_hash64map     = NULL;
    system_resizable_vector owning_vector        = NULL;
    bool                    result               = false;
    _demo_timeline_segment* segment_ptr          = NULL;
    size_t                  segment_vector_index = ITEM_NOT_FOUND;
    _demo_timeline*         timeline_ptr         = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Input timeline is NULL");

    /* Identify segment containers */
    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL, /* out_segment_id_counter_ptr_ptr */
                                                  &owning_vector,
                                                  &owning_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not identify containers for the requested segment type.");

            goto end;
        }

        /* Purge the segment from the hash-map. */
        if (!system_hash64map_get(owning_hash64map,
                                  (system_hash64) segment_id,
                                 &segment_ptr) )
        {
            LOG_ERROR("Could not retrieve a segment descriptor for segment id [%d]",
                      (int) segment_id);

            goto end;
        }

        system_hash64map_remove(owning_hash64map,
                                (system_hash64) segment_id);

        /* Do the same for the vector storage */
        segment_vector_index = system_resizable_vector_find(owning_vector,
                                                            (void*) segment_ptr);

        if (segment_vector_index != ITEM_NOT_FOUND)
        {
            system_resizable_vector_delete_element_at(owning_vector,
                                                      segment_vector_index);
        } /* if (segment_vector_index != ITEM_NOT_FOUND) */

        /* Update the timeline's duration, if the removed segment's end time matches
         * the current duration */
        if (timeline_ptr->duration == segment_ptr->end_time)
        {
            if (!_demo_timeline_update_duration(timeline_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Failed to update timeline duration.");
            }
        } /* if (timeline_ptr->duration == segment_ptr->end_time) */

        /* Release the descriptor */
        delete segment_ptr;
        segment_ptr = NULL;

        /* All done */
        result = true;
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API float demo_timeline_get_aspect_ratio(demo_timeline timeline,
                                                        system_time   frame_time)
{
    float                    result             = 0.0f;
    _demo_timeline*          timeline_ptr       = (_demo_timeline*) timeline;
    demo_timeline_segment_id video_segment_id   = 0;
    _demo_timeline_segment*  video_segment_ptr  = NULL;
    system_hash64map         video_segments_map = NULL;

    ASSERT_DEBUG_SYNC(timeline != NULL,
                      "Input timeline instance is NULL");

    if (!demo_timeline_get_segment_at_time(timeline,
                                           DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                           frame_time,
                                          &video_segment_id) )
    {
        /* No video segment defined for the requested time frame */
        result = timeline_ptr->aspect_ratio;
    }
    else
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                                   NULL, /* out_segment_id_counter_ptr_ptr */
                                                   NULL, /* out_segment_vector_ptr */
                                                  &video_segments_map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve video segments hash-map.");

            goto end;
        }

        if (!system_hash64map_get(video_segments_map,
                                  (system_hash64) video_segment_id,
                                 &video_segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve video segment descriptor for video segment id [%d]",
                              video_segment_id);

            goto end;
        }

        result = video_segment_ptr->aspect_ratio;
    }

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_property(demo_timeline          timeline,
                                                   demo_timeline_property property,
                                                   void*                  out_result_ptr)
{
    bool            result       = true;
    _demo_timeline* timeline_ptr = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Input timeline instance is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_PROPERTY_ASPECT_RATIO:
        {
            *(float*) out_result_ptr = timeline_ptr->aspect_ratio;

            break;
        }

        case DEMO_TIMELINE_PROPERTY_DURATION:
        {
            system_critical_section_enter(timeline_ptr->cs);
            {
                *(system_time*) out_result_ptr = timeline_ptr->duration;
            }
            system_critical_section_leave(timeline_ptr->cs);

            break;
        }

        case DEMO_TIMELINE_PROPERTY_N_VIDEO_SEGMENTS:
        {
            system_critical_section_enter(timeline_ptr->cs);
            {
                system_resizable_vector_get_property(timeline_ptr->video_segments,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                     out_result_ptr);
            }
            system_critical_section_leave(timeline_ptr->cs);

            break;
        }

        case DEMO_TIMELINE_PROPERTY_PIPELINE:
        {
            *(ogl_pipeline*) out_result_ptr = timeline_ptr->rendering_pipeline;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_property value");

            result = false;
        }
    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_segment_at_time(demo_timeline               timeline,
                                                          demo_timeline_segment_type  segment_type,
                                                          system_time                 time,
                                                          demo_timeline_segment_id*   out_segment_id_ptr)
{
    /* TODO: Optimize! */
    _demo_timeline_segment* found_segment_ptr = NULL;
    unsigned int            n_segments        = 0;
    bool                    result            = false;
    system_resizable_vector segments          = NULL;
    _demo_timeline*         timeline_ptr      = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (segment_type == DEMO_TIMELINE_SEGMENT_TYPE_VIDEO)
        {
            /* Chances are the user needs to access exactly the same video segment, as
             * the last time. Is that the case? */
            if (timeline_ptr->last_used_video_segment_ptr != NULL             &&
                timeline_ptr->last_used_video_segment_ptr->start_time >= time &&
                timeline_ptr->last_used_video_segment_ptr->end_time   <= time )
            {
                /* Ha */
                *out_segment_id_ptr = timeline_ptr->last_used_video_segment_ptr->id;
                 result             = true;

                 goto end;
            }
        } /* if (segment_type == DEMO_TIMELINE_SEGMENT_TYPE_VIDEO) */

        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL, /* out_segment_id_counter_ptr_ptr*/
                                                   &segments,
                                                   NULL) ) /* out_segment_hash64map_ptr */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve resizable vector for the requested segment type");

            goto end;
        }

        system_resizable_vector_get_property(segments,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_segments);

        for (unsigned int n_segment = 0;
                          n_segment < n_segments;
                        ++n_segment)
        {
            if (!system_resizable_vector_get_element_at(segments,
                                                        n_segment,
                                                       &found_segment_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve segment descriptor at index [%d]",
                                  n_segment);

                continue;
            }

            if (found_segment_ptr->start_time <= time &&
                found_segment_ptr->end_time   >= time)
            {
                /* Found the match */
                result = true;

                break;
            }

            /* Move on */
            found_segment_ptr = NULL;
        } /* for (all segments of the requested type) */

        if (result)
        {
            timeline_ptr->last_used_video_segment_ptr = found_segment_ptr;

            if (out_segment_id_ptr != NULL)
            {
                *out_segment_id_ptr = found_segment_ptr->id;
            } /* if (out_segment_id_ptr != NULL) */
        } /* if (result) */
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_segment_id(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     unsigned int               n_segment,
                                                     demo_timeline_segment_id*  out_segment_id_ptr)
{
    uint32_t                n_segments    = 0;
    system_resizable_vector owning_vector = NULL;
    bool                    result        = false;
    _demo_timeline_segment* segment_ptr   = NULL;
    _demo_timeline*         timeline_ptr  = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(out_segment_id_ptr != NULL,
                      "Result segment ID pointer is NULL");
    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL,  /* out_segment_id_counter_ptr_ptr */
                                                  &owning_vector,
                                                   NULL)) /* out_segment_hash64map_ptr */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve owning vector for the requested segment type");

            goto end;
        }

        if (!system_resizable_vector_get_element_at(owning_vector,
                                                    n_segment,
                                                    &segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment descriptor for index [%d]",
                              n_segment);

            goto end;
        }

        *out_segment_id_ptr = segment_ptr->id;
         result             = true;
     }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           void*                          out_result_ptr)
{
    system_hash64map        owning_hash64map = NULL;
    bool                    result           = false;
    _demo_timeline_segment* segment_ptr      = NULL;
    _demo_timeline*         timeline_ptr     = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL,  /* out_segment_id_counter_ptr_ptr */
                                                   NULL,  /* out_segment_vector_ptr */
                                                  &owning_hash64map))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve owning hash-map for the requested segment type");

            goto end;
        }

        if (!system_hash64map_get(owning_hash64map,
                                  (system_hash64) segment_id,
                                 &segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment descriptor for segment ID [%d]",
                              segment_id);

            goto end;
        }

        result = true;

        switch (property)
        {
            case DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO:
            {
                *(float*) out_result_ptr = segment_ptr->aspect_ratio;

                break;
            }

            case DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME:
            {
                *(system_time*) out_result_ptr = segment_ptr->end_time;

                break;
            }

            case DEMO_TIMELINE_SEGMENT_PROPERTY_NAME:
            {
                *(system_hashed_ansi_string*) out_result_ptr = segment_ptr->name;

                break;
            }

            case DEMO_TIMELINE_SEGMENT_PROPERTY_PIPELINE_TIME:
            {
                *(demo_timeline_segment_pipeline_time*) out_result_ptr = segment_ptr->pipeline_time_mode;

                break;
            }

            case DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME:
            {
                *(system_time*) out_result_ptr = segment_ptr->start_time;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized demo_timeline_segment_property value");
            }
        } /* switch (property) */
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_move_segment(demo_timeline              timeline,
                                                   demo_timeline_segment_type segment_type,
                                                   demo_timeline_segment_id   segment_id,
                                                   system_time                new_segment_start_time)
{
    bool                    needs_duration_update = false;
    bool                    result                = false;
    system_time             segment_duration      = 0;
    system_hash64map        segment_hash64map     = NULL;
    _demo_timeline_segment* segment_ptr           = NULL;
    _demo_timeline*         timeline_ptr          = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline != NULL,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL, /* out_segment_id_counter_ptr_ptr */
                                                   NULL, /* out_segment_vector_ptr */
                                                  &segment_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment hash-map for the specified segment type.");

            goto end;
        }

        if (!system_hash64map_get(segment_hash64map,
                                  (system_hash64) segment_id,
                                 &segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not identify segment with ID [%d]",
                              segment_id);

            goto end;
        }

        /* Move the segment, if feasible. */
        needs_duration_update = (timeline_ptr->duration == segment_ptr->end_time);
        segment_duration      = segment_ptr->end_time - segment_ptr->start_time;

        result = _demo_timeline_change_segment_start_end_times(timeline_ptr,
                                                               segment_ptr,
                                                               new_segment_start_time,
                                                               new_segment_start_time + segment_duration);

        if (result && needs_duration_update)
        {
            _demo_timeline_update_duration(timeline_ptr);
        }
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_render(demo_timeline timeline,
                                                                    system_time   frame_time,
                                                                    const int*    rendering_area_px_topdown)
{
    bool                     result           = false;
    _demo_timeline*          timeline_ptr     = (_demo_timeline*) timeline;
    demo_timeline_segment_id video_segment_id = 0;

    system_critical_section_enter(timeline_ptr->cs);
    {
        system_time             rendering_pipeline_time = frame_time;
        _demo_timeline_segment* video_segment_ptr       = NULL;

        if (!demo_timeline_get_segment_at_time(timeline,
                                              DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                              frame_time,
                                             &video_segment_id) )
        {
            /* No video segment at specified range - nothing to do. */
            goto end;
        }

        if (!system_hash64map_get(timeline_ptr->segment_id_to_video_segment_map,
                                  (system_hash64) video_segment_id,
                                 &video_segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Video segment with ID [%d] was not found",
                              video_segment_id);

            goto end;
        }

        /* Calculate frame time we should use for calling back the rendering pipeline object */
        switch (video_segment_ptr->pipeline_time_mode)
        {
            case DEMO_TIMELINE_SEGMENT_PIPELINE_TIME_RELATIVE_TO_SEGMENT:
            {
                ASSERT_DEBUG_SYNC(rendering_pipeline_time >= video_segment_ptr->start_time,
                                  "Video segment's rendering pipeline object will be fed negative frame time!");

                rendering_pipeline_time -= video_segment_ptr->start_time;
                break;
            }

            case DEMO_TIMELINE_SEGMENT_PIPELINE_TIME_RELATIVE_TO_TIMELINE:
            {
                /* Use the frame time we already have. */
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                   "Unrecognized pipeline time mode.");
            }
        } /* switch (video_segment_ptr->pipeline_time_mode) */

        /* OK, render the stage corresponding to the video segment */
        result = ogl_pipeline_draw_stage(timeline_ptr->rendering_pipeline,
                                         video_segment_ptr->pipeline_stage_id,
                                         rendering_pipeline_time,
                                         rendering_area_px_topdown);
    }

    /* All done */
end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_resize_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id,
                                                     system_time                new_segment_duration)
{
    bool                    result            = false;
    system_hash64map        segment_hash64map = NULL;
    _demo_timeline_segment* segment_ptr       = NULL;
    _demo_timeline*         timeline_ptr      = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline != NULL,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL, /* out_segment_id_counter_ptr_ptr */
                                                   NULL, /* out_segment_vector_ptr */
                                                  &segment_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment hash-map for the specified segment type.");

            goto end;
        }

        if (!system_hash64map_get(segment_hash64map,
                                  (system_hash64) segment_id,
                                 &segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not identify segment with ID [%d]",
                              segment_id);

            goto end;
        }

        /* Change the segment's duration, if feasible. */
        result = _demo_timeline_change_segment_start_end_times(timeline_ptr,
                                                               segment_ptr,
                                                               segment_ptr->start_time,
                                                               segment_ptr->start_time + new_segment_duration);
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_set_property(demo_timeline          timeline,
                                                   demo_timeline_property property,
                                                   const void*            data)
{
    bool            result       = true;
    _demo_timeline* timeline_ptr = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline_ptr != NULL,
                      "Input timeline instance is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_PROPERTY_ASPECT_RATIO:
        {
            timeline_ptr->aspect_ratio = *(float*) data;

            ASSERT_DEBUG_SYNC(timeline_ptr->aspect_ratio > 0.0f,
                              "An aspect ratio of a value <= 0.0 was requested!");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_property value.");

            result = false;
        }
    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_set_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           const void*                    data)
{
    bool                    result            = false;
    system_hash64map        segment_hash64map = NULL;
    _demo_timeline_segment* segment_ptr       = NULL;
    _demo_timeline*         timeline_ptr      = (_demo_timeline*) timeline;

    ASSERT_DEBUG_SYNC(timeline != NULL,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   NULL, /* out_segment_id_counter_ptr_ptr */
                                                   NULL, /* out_segment_vector_ptr */
                                                  &segment_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment hash-map for the specified segment type.");

            goto end;
        }

        if (!system_hash64map_get(segment_hash64map,
                                  (system_hash64) segment_id,
                                 &segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not identify segment with ID [%d]",
                              segment_id);

            goto end;
        }

        result = true;

        switch (property)
        {
            case DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO:
            {
                segment_ptr->aspect_ratio = *(float*) data;

                ASSERT_DEBUG_SYNC(segment_ptr->aspect_ratio > 0.0f,
                                  "A segment AR of <= 0.0 value was requested!");
                break;
            }

            case DEMO_TIMELINE_SEGMENT_PROPERTY_PIPELINE_TIME:
            {
                segment_ptr->pipeline_time_mode = *(demo_timeline_segment_pipeline_time*) data;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized demo_timeline_segment_property value used.");
            }
        } /* switch (property) */
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}
