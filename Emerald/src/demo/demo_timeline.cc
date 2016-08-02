/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "system/system_atomics.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"

/* Holds audio/postprocessing/video segment data */
typedef struct _demo_timeline_segment_item
{
    demo_timeline_segment_id   id;
    demo_timeline_segment      segment;
    demo_timeline_segment_type type;

    explicit _demo_timeline_segment_item(demo_timeline              in_timeline,
                                         demo_timeline_segment_id   in_id,
                                         demo_timeline_segment      in_segment,
                                         demo_timeline_segment_type in_segment_type)
    {
        id      = in_id;
        segment = in_segment;
        type    = in_segment_type;
    }

    ~_demo_timeline_segment_item()
    {
        if (segment != nullptr)
        {
            demo_timeline_segment_release(segment);

            segment = nullptr;
        }
    }
} _demo_timeline_segment_item;

typedef struct _demo_timeline
{
    float                   aspect_ratio;                    /* AR for frame times, for which no video segments are defined */
    system_callback_manager callback_manager;
    ral_context             context;
    system_critical_section cs;
    system_time             duration;
    system_hash64map        segment_id_to_postprocessing_segment_map; /* maps system_timeline_segment_id to _demo_timeline_segment_item instance */
    system_hash64map        segment_id_to_video_segment_map;          /* maps system_timeline_segment_id to _demo_timeline_segment_item instance */

    unsigned int            postprocessing_segment_id_counter;
    unsigned int            video_segment_id_counter;

    system_resizable_vector postprocessing_segments;         /* stores _demo_timeline_segment instances;
                                                              * DOES NOT own the segment instances;
                                                              * DOES NOT store the segments ordered by IDs;
                                                              */
    system_resizable_vector video_segments;                  /* stores _demo_timeline_segment instances;
                                                              * DOES NOT own the segment instances;
                                                              * DOES NOT store the segments ordered by IDs; */

    system_resizable_vector segment_cache_vector; /* used internally for segment enumeration  */

    REFCOUNT_INSERT_VARIABLES;

    explicit _demo_timeline(ral_context   in_context,
                            system_window in_window)
    {
        ASSERT_DEBUG_SYNC(in_context != nullptr,
                          "Input rendering context is NULL");

        aspect_ratio                             = 1.0f;
        callback_manager                         = system_callback_manager_create(static_cast<_callback_id>(DEMO_TIMELINE_CALLBACK_ID_COUNT) );
        context                                  = in_context;
        cs                                       = system_critical_section_create();
        duration                                 = 0;
        postprocessing_segments                  = system_resizable_vector_create(4  /* capacity */);
        segment_cache_vector                     = system_resizable_vector_create(16 /* capacity */);
        video_segments                           = system_resizable_vector_create(4  /* capacity */);

        segment_id_to_postprocessing_segment_map = system_hash64map_create(sizeof(_demo_timeline_segment_item*) );
        segment_id_to_video_segment_map          = system_hash64map_create(sizeof(_demo_timeline_segment_item*) );

        postprocessing_segment_id_counter        = 0;
        video_segment_id_counter                 = 0;

        ral_context_retain(in_context);
    }

    ~_demo_timeline()
    {
        if (context != nullptr)
        {
            ral_context_release(context);

            context = nullptr;
        }

        if (cs != nullptr)
        {
            system_critical_section_release(cs);

            cs = nullptr;
        }

        if (postprocessing_segments != nullptr)
        {
            system_resizable_vector_release(postprocessing_segments);

            postprocessing_segments = nullptr;
        }

        if (segment_cache_vector != nullptr)
        {
            system_resizable_vector_release(segment_cache_vector);

            segment_cache_vector = nullptr;
        }

        /* Release segment maps .. */
        system_hash64map* segment_map_ptrs[] =
        {
            &segment_id_to_postprocessing_segment_map,
            &segment_id_to_video_segment_map
        };
        const uint32_t n_segment_map_ptrs = sizeof(segment_map_ptrs) / sizeof(segment_map_ptrs[0]);

        for (uint32_t n_segment_map_ptr = 0;
                      n_segment_map_ptr < n_segment_map_ptrs;
                    ++n_segment_map_ptr)
        {
            system_hash64map* current_segment_map_ptr = segment_map_ptrs[n_segment_map_ptr];

            if (*current_segment_map_ptr != nullptr)
            {
                _demo_timeline_segment_item* current_segment_ptr = nullptr;
                unsigned int                 n_segments          = 0;

                system_hash64map_get_property(*current_segment_map_ptr,
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_segments);

                for (uint32_t n_segment = 0;
                              n_segment < n_segments;
                            ++n_segment)
                {
                    if (!system_hash64map_get_element_at(*current_segment_map_ptr,
                                                         n_segment,
                                                        &current_segment_ptr,
                                                         nullptr) ) /* result_hash */
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve segment descriptor at index [%d]",
                                          n_segment);

                        continue;
                    }

                    delete current_segment_ptr;

                    current_segment_ptr = nullptr;
                }

                system_hash64map_release(*current_segment_map_ptr);
                *current_segment_map_ptr = nullptr;
            }
        }

        if (video_segments != nullptr)
        {
            system_resizable_vector_release(video_segments);

            video_segments = nullptr;
        }


        /* Callback manager needs to be released at the end */
        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }
    }
} _demo_timeline;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline,
                               demo_timeline,
                              _demo_timeline);

/** Forward declarations */
PRIVATE bool _demo_timeline_add_segment                   (demo_timeline                   timeline,
                                                           demo_timeline_segment_type      type,
                                                           system_hashed_ansi_string       name,
                                                           system_time                     start_time,
                                                           system_time                     end_time,
                                                           float                           videoonly_aspect_ratio,
                                                           ral_format                      videoonly_output_texture_format,
                                                           demo_timeline_segment_id*       out_opt_segment_id_ptr,
                                                           void*                           out_opt_segment_ptr);
PRIVATE bool _demo_timeline_change_segment_start_end_times(_demo_timeline*                 timeline_ptr,
                                                           _demo_timeline_segment_item*    segment_ptr,
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


/** TODO */
PRIVATE bool _demo_timeline_add_segment(demo_timeline              timeline,
                                        demo_timeline_segment_type type,
                                        system_hashed_ansi_string  name,
                                        system_time                start_time,
                                        system_time                end_time,
                                        float                      videoonly_aspect_ratio,
                                        ral_format                 videoonly_output_texture_format,
                                        demo_timeline_segment_id*  out_opt_segment_id_ptr,
                                        void*                      out_opt_segment_ptr)
{
    demo_timeline_segment        new_segment            = nullptr;
    demo_timeline_segment_id     new_segment_id         = -1;
    _demo_timeline_segment_item* new_segment_ptr        = nullptr;
    uint32_t                     new_segment_stage_id   = -1;
    system_hash64map             owning_hash64map       = nullptr;
    system_resizable_vector      owning_vector          = nullptr;
    bool                         result                 = false;
    unsigned int*                segment_id_counter_ptr = nullptr;
    _demo_timeline*              timeline_ptr           = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(start_time < end_time,
                      "Segment's start time must be smaller than its end time");
    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Timeline instance is NULL");

    if (start_time   >= end_time ||
        timeline_ptr == nullptr)
    {
        goto end;
    }

    /* If we're being asked to create a new postprocessing segment, make sure there's enough room
     * to fit it. This step needs not be executed for video segments. */
    system_critical_section_enter(timeline_ptr->cs);
    {
        if (type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING)
        {
            bool is_region_free = false;

            if (!_demo_timeline_is_region_segment_free(timeline_ptr,
                                                       type,
                                                       start_time,
                                                       end_time,
                                                       nullptr, /* opt_excluded_segment_id_ptr */
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
        }

        /* Determine which hash map & resizable vector the new descriptor should be stored in */
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   type,
                                                  &segment_id_counter_ptr,
                                                  &owning_vector,
                                                  &owning_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not determine what hash map and/or resizable vector the new segment should be stored in");

            goto end;
        }

        new_segment_id  = system_atomics_increment(segment_id_counter_ptr);

        switch (type)
        {
            case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
            {
                /* Spawn a new segment instance */
                new_segment = demo_timeline_segment_create_postprocessing(timeline_ptr->context,
                                                                          reinterpret_cast<demo_timeline>(timeline_ptr),
                                                                          name,
                                                                          start_time,
                                                                          end_time,
                                                                          new_segment_id);

                ASSERT_DEBUG_SYNC(new_segment != nullptr,
                                  "Could not create a new postprocessing segment instance");

                break;
            }

            case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
            {
                /* Spawn a new segment instance */
                new_segment = demo_timeline_segment_create_video(timeline_ptr->context,
                                                                 reinterpret_cast<demo_timeline>(timeline_ptr),
                                                                 videoonly_output_texture_format,
                                                                 name,
                                                                 start_time,
                                                                 end_time,
                                                                 new_segment_id);

                ASSERT_DEBUG_SYNC(new_segment != nullptr,
                                  "Could not create a new video segment instance");

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized segment type requested");
            }
        }

        ASSERT_ALWAYS_SYNC(new_segment != nullptr,
                           "Could not spawn a new segment descriptor");

        /* Spawn a new descriptor */
        new_segment_ptr = new (std::nothrow) _demo_timeline_segment_item(timeline,
                                                                         new_segment_id,
                                                                         new_segment,
                                                                         type);

        if (new_segment_ptr == nullptr)
        {
            goto end;
        }

        /* Store it in the containers */
        if (!system_hash64map_insert(owning_hash64map,
                                     static_cast<system_hash64>(new_segment_id),
                                     new_segment_ptr,
                                     nullptr, /* callback */
                                     nullptr) )/* callback_user_arg */
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

        /* Fire notifications */
        switch (type)
        {
            case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
            {
                system_callback_manager_call_back(timeline_ptr->callback_manager,
                                                  DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_ADDED,
                                                  reinterpret_cast<void*>(new_segment_id) );

                break;
            }

            case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
            {
                system_callback_manager_call_back(timeline_ptr->callback_manager,
                                                  DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_ADDED,
                                                  reinterpret_cast<void*>(new_segment_id) );

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized timeline segment type");
            }
        }

        /* All done */
        if (out_opt_segment_id_ptr != nullptr)
        {
            *out_opt_segment_id_ptr = new_segment_id;
        }

        if (out_opt_segment_ptr != nullptr)
        {
            *reinterpret_cast<demo_timeline_segment*>(out_opt_segment_ptr) = new_segment_ptr->segment;
        }

        result = true;
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    if (!result)
    {
        if (new_segment_ptr != nullptr)
        {
            delete new_segment_ptr;

            new_segment_ptr = nullptr;
        }
    }

    return result;
}

/** TODO
 *
 *  NOTE: This implementation assumes timeline_ptr::cs is entered at the time of the call!
 */
PRIVATE bool _demo_timeline_change_segment_start_end_times(_demo_timeline*              timeline_ptr,
                                                           _demo_timeline_segment_item* segment_ptr,
                                                           system_time                  new_start_time,
                                                           system_time                  new_end_time)
{
    bool        is_region_free     = false;
    bool        result             = false;
    system_time segment_end_time   = 0;
    system_time segment_start_time = 0;

    demo_timeline_segment_get_property(segment_ptr->segment,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                      &segment_end_time);
    demo_timeline_segment_get_property(segment_ptr->segment,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                      &segment_start_time);

    const system_time old_time_delta   = (segment_end_time - segment_start_time);
    const system_time new_time_delta   = (new_end_time     - new_start_time);
    const bool        is_resize_action = (new_time_delta == old_time_delta);

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

    demo_timeline_segment_set_property(segment_ptr->segment,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                      &new_end_time);
    demo_timeline_segment_set_property(segment_ptr->segment,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                      &new_start_time);

    /* Fire notifications */
    switch (segment_ptr->type)
    {
        case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
        case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
        {
            if (is_resize_action)
            {
                system_callback_manager_call_back(timeline_ptr->callback_manager,
                                                  (segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING) ? DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_RESIZED
                                                                                                                   : DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_RESIZED,
                                                  reinterpret_cast<void*>(segment_ptr->id) );

                break;
            }
            else
            {
                system_callback_manager_call_back(timeline_ptr->callback_manager,
                                                  (segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING) ? DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_MOVED
                                                                                                                   : DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_MOVED,
                                                  reinterpret_cast<void*>(segment_ptr->id) );
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized timeline segment type");
        }
    }

    /* All done */
    result = true;

end:
    return result;
}

/** TODO */
PRIVATE bool demo_timeline_get_segments_at_time(_demo_timeline*             timeline_ptr,
                                                demo_timeline_segment_type  segment_type,
                                                system_time                 time,
                                                system_resizable_vector     result_vector)
{
    /* TODO: Optimize! */
    _demo_timeline_segment_item* found_segment_ptr = nullptr;
    unsigned int                 n_segments        = 0;
    bool                         result            = false;
    system_resizable_vector      segments          = nullptr;

    ASSERT_DEBUG_SYNC(result_vector != nullptr,
                      "Result vector is NULL");
    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Input timeline instance is NULL");

    system_resizable_vector_clear(result_vector);

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr, /* out_segment_id_counter_ptr_ptr*/
                                                   &segments,
                                                   nullptr) ) /* out_segment_hash64map_ptr */
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
            system_time found_segment_end_time   = 0;
            system_time found_segment_start_time = 0;

            if (!system_resizable_vector_get_element_at(segments,
                                                        n_segment,
                                                       &found_segment_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve segment descriptor at index [%d]",
                                  n_segment);

                continue;
            }

            demo_timeline_segment_get_property(found_segment_ptr->segment,
                                               DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                              &found_segment_end_time);
            demo_timeline_segment_get_property(found_segment_ptr->segment,
                                               DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                              &found_segment_start_time);

            if (found_segment_start_time <= time &&
                found_segment_end_time   >  time)
            {
                /* Found a match */
                result = true;

                system_resizable_vector_push(result_vector,
                                             found_segment_ptr->segment);

                /* Only one post-processing segment can be defined for a given time region. This means
                 * we can break at this point if that's the segment type the user was looking for */
                if (segment_type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING)
                {
                    break;
                }
            }

            /* Move on */
            found_segment_ptr = nullptr;
        }
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

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
        case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
        {
            if (out_segment_hash64map_ptr != nullptr)
            {
                *out_segment_hash64map_ptr = timeline_ptr->segment_id_to_postprocessing_segment_map;
            }

            if (out_segment_id_counter_ptr_ptr != nullptr)
            {
                *out_segment_id_counter_ptr_ptr = &timeline_ptr->postprocessing_segment_id_counter;
            }

            if (out_segment_vector_ptr != nullptr)
            {
                *out_segment_vector_ptr = timeline_ptr->postprocessing_segments;
            }

            break;
        }

        case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
        {
            if (out_segment_hash64map_ptr != nullptr)
            {
                *out_segment_hash64map_ptr = timeline_ptr->segment_id_to_video_segment_map;
            }

            if (out_segment_id_counter_ptr_ptr != nullptr)
            {
                *out_segment_id_counter_ptr_ptr = &timeline_ptr->video_segment_id_counter;
            }

            if (out_segment_vector_ptr != nullptr)
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
    }

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
    system_resizable_vector segments_vector = nullptr;

    if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                               segment_type,
                                               nullptr, /* out_segment_id_counter_ptr_ptr */
                                              &segments_vector,
                                               nullptr) )/* out_segment_hash64map_ptr */
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
        _demo_timeline_segment_item* current_segment_ptr = nullptr;
        bool                         does_not_overlap    = false;
        bool                         does_overlap        = true;
        system_time                  segment_end_time    = 0;
        system_time                  segment_start_time  = 0;

        if (!system_resizable_vector_get_element_at(segments_vector,
                                                    n_segment,
                                                   &current_segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve segment descriptor at index [%d]",
                              n_segment);

            continue;
        }

        demo_timeline_segment_get_property(current_segment_ptr->segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                          &segment_end_time);
        demo_timeline_segment_get_property(current_segment_ptr->segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                          &segment_start_time);

        does_not_overlap = (segment_start_time <= start_time  &&
                            segment_end_time   <= start_time) ||
                           (segment_start_time >= end_time    &&
                            segment_end_time   >= end_time);
        does_overlap     = !does_not_overlap;

        if (opt_excluded_segment_id_ptr != nullptr)
        {
            if (does_overlap                                            &&
                current_segment_ptr->id == *opt_excluded_segment_id_ptr)
            {
                /* We were explicitly asked to skip this segment. */
                continue;
            }
        }

        if (does_overlap)
        {
            overlap_status = true;

            break;
        }
    }

    /* All done */
    if (out_result_ptr != nullptr)
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
           ++reinterpret_cast<int&>(current_segment_type))
    {
        unsigned int            n_segments      = 0;
        system_resizable_vector segments_vector = nullptr;

        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   current_segment_type,
                                                   nullptr,   /* out_segment_id_counter_ptr */
                                                  &segments_vector,
                                                   nullptr) ) /* out_segment_hash64map_ptr */
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
            system_time                  current_segment_end_time = 0;
            _demo_timeline_segment_item* current_segment_ptr      = nullptr;

            if (!system_resizable_vector_get_element_at(segments_vector,
                                                        n_current_segment,
                                                       &current_segment_ptr))
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve segment descriptor at index [%d]",
                                  n_current_segment);

                goto end;
            }

            demo_timeline_segment_get_property(current_segment_ptr->segment,
                                               DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                              &current_segment_end_time);

            if (current_segment_end_time > max_segment_end_time)
            {
                max_segment_end_time = current_segment_end_time;
            }
        }
    }

    timeline_ptr->duration = max_segment_end_time;

    /* All done */
    result = true;

end:
    return result;
}


/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_add_postprocessing_segment(demo_timeline             timeline,
                                                                 system_hashed_ansi_string name,
                                                                 system_time               start_time,
                                                                 system_time               end_time,
                                                                 demo_timeline_segment_id* out_opt_segment_id_ptr,
                                                                 demo_timeline_segment*    out_opt_postprocessing_segment_ptr)
{
    return _demo_timeline_add_segment(timeline,
                                      DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                      name,
                                      start_time,
                                      end_time,
                                      0.0f,               /* aspect ratio - irrelevant */
                                      RAL_FORMAT_UNKNOWN, /* will be determined later */
                                      out_opt_segment_id_ptr,
                                      out_opt_postprocessing_segment_ptr);
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_add_video_segment(demo_timeline             timeline,
                                                        system_hashed_ansi_string name,
                                                        system_time               start_time,
                                                        system_time               end_time,
                                                        float                     aspect_ratio,
                                                        ral_format                output_texture_format,
                                                        demo_timeline_segment_id* out_opt_segment_id_ptr,
                                                        demo_timeline_segment*    out_opt_video_segment_ptr)
{
    return _demo_timeline_add_segment(timeline,
                                      DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                      name,
                                      start_time,
                                      end_time,
                                      aspect_ratio,
                                      output_texture_format,
                                      out_opt_segment_id_ptr,
                                      out_opt_video_segment_ptr);
}

/** Please see header for spec */
PUBLIC EMERALD_API demo_timeline demo_timeline_create(system_hashed_ansi_string name,
                                                      ral_context               context,
                                                      system_window             window)
{
    _demo_timeline* timeline_ptr = new (std::nothrow) _demo_timeline(context,
                                                                     window);

    ASSERT_ALWAYS_SYNC(timeline_ptr != nullptr,
                       "Out of memory");

    if (timeline_ptr != nullptr)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(timeline_ptr,
                                                       _demo_timeline_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timelines\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return reinterpret_cast<demo_timeline>(timeline_ptr);
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_delete_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id)
{
    system_hash64map             owning_hash64map     = nullptr;
    system_resizable_vector      owning_vector        = nullptr;
    bool                         result               = false;
    system_time                  segment_end_time     = 0;
    _demo_timeline_segment_item* segment_ptr          = nullptr;
    size_t                       segment_vector_index = ITEM_NOT_FOUND;
    _demo_timeline*              timeline_ptr         = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Input timeline is NULL");

    /* Identify segment containers */
    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr, /* out_segment_id_counter_ptr_ptr */
                                                  &owning_vector,
                                                  &owning_hash64map) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not identify containers for the requested segment type.");

            goto end;
        }

        /* Purge the segment from the hash-map. */
        if (!system_hash64map_get(owning_hash64map,
                                  static_cast<system_hash64>(segment_id),
                                 &segment_ptr) )
        {
            LOG_ERROR("Could not retrieve a segment descriptor for segment id [%d]",
                      static_cast<int>(segment_id) );

            goto end;
        }

        system_hash64map_remove(owning_hash64map,
                                static_cast<system_hash64>(segment_id) );

        /* Do the same for the vector storage */
        segment_vector_index = system_resizable_vector_find(owning_vector,
                                                            reinterpret_cast<void*>(segment_ptr) );

        if (segment_vector_index != ITEM_NOT_FOUND)
        {
            system_resizable_vector_delete_element_at(owning_vector,
                                                      segment_vector_index);
        }

        /* Update the timeline's duration, if the removed segment's end time matches
         * the current duration */
        demo_timeline_segment_get_property(segment_ptr->segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                          &segment_end_time);

        if (timeline_ptr->duration == segment_end_time)
        {
            if (!_demo_timeline_update_duration(timeline_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Failed to update timeline duration.");
            }
        }

        /* Release the descriptor */
        delete segment_ptr;
        segment_ptr = nullptr;

        /* All done */
        result = true;
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void demo_timeline_free_segment_ids(demo_timeline_segment_id* segment_ids)
{
    delete [] segment_ids;
}

/** Please see header for spec */
PUBLIC EMERALD_API float demo_timeline_get_aspect_ratio(demo_timeline timeline,
                                                        system_time   frame_time)
{
    demo_timeline_segment postprocessing_segment = nullptr;
    float                 result                 = 0.0f;
    _demo_timeline*       timeline_ptr           = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline != nullptr,
                      "Input timeline instance is NULL");

    system_resizable_vector_clear(timeline_ptr->segment_cache_vector);

    if (!demo_timeline_get_segments_at_time(timeline_ptr,
                                            DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                            frame_time,
                                            timeline_ptr->segment_cache_vector) )
    {
        /* No post-processing segment defined for the requested time frame */
        result = timeline_ptr->aspect_ratio;
    }
    else
    {
        system_resizable_vector_get_element_at(timeline_ptr->segment_cache_vector,
                                               0, /* index */
                                              &postprocessing_segment);

        demo_timeline_segment_get_property(postprocessing_segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO,
                                          &result);
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_property(const demo_timeline    timeline,
                                                   demo_timeline_property property,
                                                   void*                  out_result_ptr)
{
    bool            result       = true;
    _demo_timeline* timeline_ptr = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Input timeline instance is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_PROPERTY_ASPECT_RATIO:
        {
            *reinterpret_cast<float*>(out_result_ptr) = timeline_ptr->aspect_ratio;

            break;
        }

        case DEMO_TIMELINE_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = timeline_ptr->callback_manager;

            break;
        }

        case DEMO_TIMELINE_PROPERTY_DURATION:
        {
            system_critical_section_enter(timeline_ptr->cs);
            {
                *reinterpret_cast<system_time*>(out_result_ptr) = timeline_ptr->duration;
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

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_property value");

            result = false;
        }
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_segment_by_id(demo_timeline              timeline,
                                                        demo_timeline_segment_type segment_type,
                                                        demo_timeline_segment_id   segment_id,
                                                        demo_timeline_segment*     out_segment_ptr)
{
    bool                         result           = false;
    _demo_timeline_segment_item* segment_item_ptr = nullptr;
    system_hash64map             segment_map      = nullptr;
    _demo_timeline*              timeline_ptr     = reinterpret_cast<_demo_timeline*>(timeline);

    /* Sanity checks */
    if (out_segment_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    if (timeline == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Timeline instance is NULL");

        goto end;
    }

    /* Retrieve the segment descriptor */
    switch (segment_type)
    {
        case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
        {
            segment_map = timeline_ptr->segment_id_to_postprocessing_segment_map;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
        {
            segment_map = timeline_ptr->segment_id_to_video_segment_map;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized segment type");

            goto end;
        }
    }

    if (!system_hash64map_get(segment_map,
                              (system_hash64) segment_id,
                             &segment_item_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "No segment with the specified ID [%d] was found",
                          segment_id);

        goto end;
    }

    *out_segment_ptr = segment_item_ptr->segment;

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_segment_id(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     unsigned int               n_segment,
                                                     demo_timeline_segment_id*  out_segment_id_ptr)
{
    uint32_t                     n_segments    = 0;
    system_resizable_vector      owning_vector = nullptr;
    bool                         result        = false;
    _demo_timeline_segment_item* segment_ptr   = nullptr;
    _demo_timeline*              timeline_ptr  = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(out_segment_id_ptr != nullptr,
                      "Result segment ID pointer is NULL");
    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr,  /* out_segment_id_counter_ptr_ptr */
                                                  &owning_vector,
                                                   nullptr)) /* out_segment_hash64map_ptr */
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
PUBLIC EMERALD_API bool demo_timeline_get_segment_ids(demo_timeline              timeline,
                                                      demo_timeline_segment_type segment_type,
                                                      unsigned int*              out_n_segment_ids_ptr,
                                                      demo_timeline_segment_id** out_segment_ids_ptr)
{
    uint32_t                  n_segments         = 0;
    bool                      result             = false;
    demo_timeline_segment_id* result_segment_ids = nullptr;
    system_resizable_vector   segments_vector    = nullptr;
    _demo_timeline*           timeline_ptr       = reinterpret_cast<_demo_timeline*>(timeline);

    /* Sanity checks */
    if (out_n_segment_ids_ptr == nullptr ||
        out_segment_ids_ptr   == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out arguments are NULL");

        goto end;
    }

    if (timeline == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Timeline instance is NULL");

        goto end;
    }

    /* Retrieve the segment IDs */
    switch (segment_type)
    {
        case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
        {
            segments_vector = timeline_ptr->postprocessing_segments;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
        {
            segments_vector = timeline_ptr->video_segments;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized segment type");

            goto end;
        }
    }

    system_resizable_vector_get_property(segments_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_segments);

    if (n_segments > 0)
    {
        result_segment_ids = new demo_timeline_segment_id[n_segments];

        if (result_segment_ids == nullptr)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            goto end;
        }

        for (uint32_t n_segment = 0;
                      n_segment < n_segments;
                    ++n_segment)
        {
            _demo_timeline_segment_item* current_segment_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(segments_vector,
                                                        n_segment,
                                                       &current_segment_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve segment at index [%d]",
                                  n_segment);

                goto end;
            }

            demo_timeline_segment_get_property(current_segment_ptr->segment,
                                               DEMO_TIMELINE_SEGMENT_PROPERTY_ID,
                                               result_segment_ids + n_segment);
        }
    }

    /* All done */
    *out_n_segment_ids_ptr = n_segments;
    *out_segment_ids_ptr   = result_segment_ids;

    result = true;
end:
    if (!result)
    {
        if (result_segment_ids != nullptr)
        {
            delete [] result_segment_ids;

            result_segment_ids = nullptr;
        }
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_get_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           void*                          out_result_ptr)
{
    system_hash64map             owning_hash64map = nullptr;
    bool                         result           = false;
    _demo_timeline_segment_item* segment_ptr      = nullptr;
    _demo_timeline*              timeline_ptr     = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr,  /* out_segment_id_counter_ptr_ptr */
                                                   nullptr,  /* out_segment_vector_ptr */
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
            case DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME:
            case DEMO_TIMELINE_SEGMENT_PROPERTY_NAME:
            case DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME:
            {
                demo_timeline_segment_get_property(segment_ptr->segment,
                                                   property,
                                                   out_result_ptr);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized demo_timeline_segment_property value");
            }
        }
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
    bool                         needs_duration_update = false;
    bool                         result                = false;
    system_time                  segment_duration      = 0;
    system_time                  segment_end_time      = 0;
    system_hash64map             segment_hash64map     = nullptr;
    _demo_timeline_segment_item* segment_ptr           = nullptr;
    system_time                  segment_start_time    = 0;
    _demo_timeline*              timeline_ptr          = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline != nullptr,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr, /* out_segment_id_counter_ptr_ptr */
                                                   nullptr, /* out_segment_vector_ptr */
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
        demo_timeline_segment_get_property(segment_ptr->segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,
                                          &segment_end_time);
        demo_timeline_segment_get_property(segment_ptr->segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                          &segment_start_time);

        needs_duration_update = (timeline_ptr->duration == segment_end_time);
        segment_duration      = segment_end_time - segment_start_time;

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
PUBLIC ral_present_job demo_timeline_render(demo_timeline timeline,
                                            uint32_t      frame_index,
                                            system_time   frame_time,
                                            const int*    rendering_area_px_topdown)
{
    ral_present_job                 result_present_job        = ral_present_job_create();
    demo_timeline_segment           postprocessing_segment    = nullptr;
    uint32_t                        n_postprocessing_segments = 0;
    bool                            result                    = false;
    demo_timeline_segment_time_mode time_mode;
    _demo_timeline*                 timeline_ptr              = reinterpret_cast<_demo_timeline*>(timeline);
    demo_timeline_segment_id        video_segment_id          = 0;

    system_critical_section_enter(timeline_ptr->cs);
    {
        system_time rendering_pipeline_time = frame_time;

        /* Get post-processing segment for specified frame time */
        system_resizable_vector_clear(timeline_ptr->segment_cache_vector);

        if (!demo_timeline_get_segments_at_time(timeline_ptr,
                                                DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                                frame_time,
                                                timeline_ptr->segment_cache_vector) )
        {
            /* No post-processing segment at specified frame time - nothing to do. */
            goto end;
        }

        system_resizable_vector_get_property(timeline_ptr->segment_cache_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             &n_postprocessing_segments);

        if (n_postprocessing_segments == 0)
        {
            goto end;
        }

        system_resizable_vector_get_element_at(timeline_ptr->segment_cache_vector,
                                               0, /* index */
                                              &postprocessing_segment);

        /* Calculate frame time we should use for calling back the rendering pipeline object */
        demo_timeline_segment_get_property(postprocessing_segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_TIME_MODE,
                                          &time_mode);

        switch (time_mode)
        {
            case DEMO_TIMELINE_SEGMENT_TIME_MODE_RELATIVE_TO_SEGMENT:
            {
                system_time segment_start_time = 0;

                demo_timeline_segment_get_property(postprocessing_segment,
                                                   DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                                  &segment_start_time);

                ASSERT_DEBUG_SYNC(rendering_pipeline_time >= segment_start_time,
                                  "Video segment's rendering pipeline object will be fed negative frame time!");

                rendering_pipeline_time -= segment_start_time;
                break;
            }

            case DEMO_TIMELINE_SEGMENT_TIME_MODE_RELATIVE_TO_TIMELINE:
            {
                /* Use the frame time we already have. */
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                   "Unrecognized pipeline time mode.");
            }
        }

        /* Render the segment */
        ASSERT_DEBUG_SYNC(false,
                          "TODO");

        result = demo_timeline_segment_render(postprocessing_segment,
                                              frame_index,
                                              rendering_pipeline_time,
                                              rendering_area_px_topdown);
    }

    /* All done */
end:
    system_critical_section_leave(timeline_ptr->cs);

    return result_present_job;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_resize_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id,
                                                     system_time                new_segment_duration)
{
    bool                         result             = false;
    system_hash64map             segment_hash64map  = nullptr;
    _demo_timeline_segment_item* segment_ptr        = nullptr;
    system_time                  segment_start_time = 0;
    _demo_timeline*              timeline_ptr       = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline != nullptr,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr, /* out_segment_id_counter_ptr_ptr */
                                                   nullptr, /* out_segment_vector_ptr */
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
        demo_timeline_segment_get_property(segment_ptr->segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,
                                          &segment_start_time);

        result = _demo_timeline_change_segment_start_end_times(timeline_ptr,
                                                               segment_ptr,
                                                               segment_start_time,
                                                               segment_start_time + new_segment_duration);
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
    _demo_timeline* timeline_ptr = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline_ptr != nullptr,
                      "Input timeline instance is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_PROPERTY_ASPECT_RATIO:
        {
            timeline_ptr->aspect_ratio = *reinterpret_cast<const float*>(data);

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
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool demo_timeline_set_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           const void*                    data)
{
    bool                         result            = false;
    system_hash64map             segment_hash64map = nullptr;
    _demo_timeline_segment_item* segment_ptr       = nullptr;
    _demo_timeline*              timeline_ptr      = reinterpret_cast<_demo_timeline*>(timeline);

    ASSERT_DEBUG_SYNC(timeline != nullptr,
                      "Input timeline instance is NULL");

    system_critical_section_enter(timeline_ptr->cs);
    {
        if (!_demo_timeline_get_segment_containers(timeline_ptr,
                                                   segment_type,
                                                   nullptr, /* out_segment_id_counter_ptr_ptr */
                                                   nullptr, /* out_segment_vector_ptr */
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

        demo_timeline_segment_set_property(segment_ptr->segment,
                                           property,
                                           data);
    }

end:
    system_critical_section_leave(timeline_ptr->cs);

    return result;
}
