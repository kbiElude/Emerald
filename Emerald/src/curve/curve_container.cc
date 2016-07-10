/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "curve/curve_constants.h"
#include "curve/curve_segment.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_variant.h"

#ifdef INCLUDE_OBJECT_MANAGER
    #include "object_manager/object_manager_general.h"
#endif

/** Internal definitions */
typedef struct
{

    system_time   start_time;
    system_time   end_time;
    curve_segment segment;
    float         threshold;

} _curve_container_segment;

typedef struct
{
    system_time    last_read_time;
    system_variant last_read_value;

    curve_container_envelope_boundary_behavior pre_behavior;
    curve_container_envelope_boundary_behavior post_behavior;
    bool                                       pre_post_behavior_status;
    system_variant                             default_value;

    curve_segment_id last_used_segment_id;
    system_time      length;

    system_hash64map        segments;
    system_read_write_mutex segments_read_write_mutex;
    system_resizable_vector segments_order;

    /* Stack overflow protection flags */
    bool is_set_segment_times_call_in_place;
} _curve_container_data;

typedef struct
{
    system_variant_type       data_type;
    _curve_container_data     data;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _curve_container;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(curve_container,
                               curve_container,
                              _curve_container);


typedef _curve_container*         _curve_container_ptr;
typedef _curve_container_segment* _curve_container_segment_ptr;

/** Forward declarations */
PRIVATE uint32_t    _curve_container_find_appropriate_place_for_segment_at_time( _curve_container_data*,
                                                                                 system_time);
PRIVATE bool        _curve_container_get_pre_post_behavior_value               ( curve_container,
                                                                                 _curve_container_data*,
                                                                                 bool,
                                                                                 system_time,
                                                                                system_variant);
PRIVATE system_time _curve_container_get_range                                 ( system_time,
                                                                                 system_time,
                                                                                 system_time);
PRIVATE void        _curve_container_on_curve_segment_changed                  ( void*);
PRIVATE void        _curve_container_on_new_segment_start_time                 ( curve_container,
                                                                                 uint32_t,
                                                                                 curve_segment_id,
                                                                                 system_time);
PRIVATE void        _curve_container_on_new_segment_end_time                   ( curve_container,
                                                                                 uint32_t,
                                                                                 curve_segment_id,
                                                                                 system_time);
PRIVATE void        _curve_container_recalculate_curve_length                  ( curve_container);
PRIVATE void        _deinit_curve_container_data                               (_curve_container_data* data);
PRIVATE void        _init_curve_container_data                                 (_curve_container_data* data,
                                                                                system_variant_type    data_type);


/** Structure de-/initializers */

/** TODO **/
PRIVATE bool _curve_container_add_segment_shared(curve_container   curve,
                                                 system_time       start_time,
                                                 system_time       end_time,
                                                 curve_segment     segment,
                                                 curve_segment_id* out_segment_id_ptr)
{
    _curve_container_ptr curve_ptr = reinterpret_cast<_curve_container_ptr>(curve);

    ASSERT_DEBUG_SYNC(!curve_ptr->data.pre_post_behavior_status,
                      "Segments should not be modified while pre/post-behavior support is enabled");
    ASSERT_DEBUG_SYNC(start_time < end_time,
                      "Requested segment start time is smaller than end time");
    ASSERT_DEBUG_SYNC(segment != nullptr,
                      "Segment data cannot be null");

    if (curve_container_is_range_defined(curve,
                                         start_time,
                                         end_time,
                                         -1) )
    {
        ASSERT_DEBUG_SYNC(false, "Requested time region already contains segments");

        return false;
    }

    /* Create a segment instance. Fill it and insert into segments vector. */
    curve_segment_id new_segment_id = curve_ptr->data.last_used_segment_id+1;

    _curve_container_segment_ptr new_segment = new _curve_container_segment;

    new_segment->start_time = start_time;
    new_segment->end_time   = end_time;
    new_segment->segment    = segment;

    curve_ptr->data.last_used_segment_id++;

    system_read_write_mutex_lock(curve_ptr->data.segments_read_write_mutex,
                                 ACCESS_WRITE);
    {
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(curve_ptr->data.segments,
                                                     new_segment_id),
                          "Curve already uses a segment of id [%d] that is about to be added",
                          new_segment_id);

        system_resizable_vector_insert_element_at(curve_ptr->data.segments_order,
                                                  _curve_container_find_appropriate_place_for_segment_at_time(&curve_ptr->data,
                                                                                                              start_time),
                                                  reinterpret_cast<void*>(static_cast<intptr_t>(new_segment_id)) );
        system_hash64map_insert                   (curve_ptr->data.segments,
                                                  static_cast<system_hash64>(new_segment_id),
                                                  new_segment,
                                                  nullptr,
                                                  nullptr);

        _curve_container_recalculate_curve_length(curve);

        if (out_segment_id_ptr != nullptr)
        {
            *out_segment_id_ptr = new_segment_id;
        }

        curve_ptr->data.last_read_time = -1;
    }
    system_read_write_mutex_unlock(curve_ptr->data.segments_read_write_mutex,
                                   ACCESS_WRITE);

    return true;
}

/** Releases a curve container. The curve container must have been created using curve_container_create()
 *  function. Do not use the container after calling this function.
 *
 *  @param curve_container Curve container to release. Cannot be nullptr.
 */
PRIVATE void _curve_container_release(void* container)
{
    _curve_container_ptr curve_container_ptr = (_curve_container_ptr) container;

    _deinit_curve_container_data(&curve_container_ptr->data);

    /* Inform any subscribers about the event */
    system_callback_manager_call_back(system_callback_manager_get(),
                                      CALLBACK_ID_CURVE_CONTAINER_DELETED,
                                      nullptr);
}

/* TODO */
PRIVATE void _deinit_curve_container_data(_curve_container_data* data)
{
    size_t n_segments = 0;

    system_hash64map_get_property(data->segments,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_segments);
    if (n_segments != 0)
    {
        for (size_t n_segment = 0;
                    n_segment < n_segments;
                  ++n_segment)
        {
            curve_segment segment      = nullptr;
            system_hash64 segment_hash = 0;
            bool          result       = system_hash64map_get_element_at(data->segments,
                                                                         n_segment,
                                                                        &segment,
                                                                        &segment_hash);

            ASSERT_DEBUG_SYNC(result,
                              "Could not retrieve segment at index [%d]",
                              n_segment);

            if (result)
            {
                _curve_container_segment_ptr curve_container_segment_ptr  = (_curve_container_segment_ptr) segment;

                curve_segment_release(curve_container_segment_ptr->segment);
            }
        }
    }

    system_variant_release         (data->default_value);
    system_variant_release         (data->last_read_value);
    system_hash64map_release       (data->segments);
    system_read_write_mutex_release(data->segments_read_write_mutex);
    system_resizable_vector_release(data->segments_order);
}

/* TODO */
PRIVATE void _init_curve_container(_curve_container_ptr      container,
                                   system_variant_type       data_type,
                                   system_hashed_ansi_string name)
{
    container->data_type = data_type;
    container->name      = name;

    _init_curve_container_data(&container->data,
                               data_type);
}

/* TODO */
PRIVATE void _init_curve_container_data(_curve_container_data* data,
                                        system_variant_type    data_type)
{
    data->default_value                      = system_variant_create(data_type);
    data->is_set_segment_times_call_in_place = false;
    data->last_read_time                     = -1;
    data->last_read_value                    = system_variant_create(data_type);
    data->last_used_segment_id               = 0;
    data->pre_behavior                       = CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED;
    data->pre_post_behavior_status           = false;
    data->post_behavior                      = CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED;
    data->segments                           = system_hash64map_create       (sizeof(_curve_container_segment*) );
    data->segments_read_write_mutex          = system_read_write_mutex_create();
    data->segments_order                     = system_resizable_vector_create(CURVE_CONTAINER_START_SEGMENTS_AMOUNT);

    switch (data_type)
    {
        case SYSTEM_VARIANT_ANSI_STRING:
        {
            system_variant_set_ansi_string(data->default_value,
                                           system_hashed_ansi_string_get_default_empty_string(),
                                           false);

            break;
        }

        case SYSTEM_VARIANT_BOOL:
        {
            system_variant_set_boolean(data->default_value,
                                       false);

            break;
        }

        case SYSTEM_VARIANT_INTEGER:
        {
            system_variant_set_integer(data->default_value,
                                       0,
                                       false);

            break;
        }

        case SYSTEM_VARIANT_FLOAT:
        {
            system_variant_set_float(data->default_value,
                                     0.0f);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized variant type requested [%d]",
                               data_type);
        }
    }
}

/* TODO */
PRIVATE void _init_curve_container_segment(_curve_container_segment_ptr segment,
                                           system_time                  start_time,
                                           system_time                  end_time)
{
    segment->end_time   = end_time;
    segment->segment    = nullptr;
    segment->start_time = start_time;
    segment->threshold  = 0.5f;
}


/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_add_general_node(curve_container        curve,
                                                         curve_segment_id       segment_id,
                                                         system_time            time,
                                                         system_variant         value,
                                                         curve_segment_node_id* out_node_id)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_WRITE);
    {
        curve_segment segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &segment);
        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain segment with id [%d]",
                          segment_id);

        if (result)
        {
            _curve_container_segment_ptr curve_segment_ptr = (_curve_container_segment_ptr) segment;

            ASSERT_DEBUG_SYNC(curve_segment_ptr->segment != nullptr,
                              "Segment data is null!");

            if (curve_segment_ptr->segment != nullptr)
            {
                result = curve_segment_add_node(segment,
                                                time,
                                                value,
                                                out_node_id);
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_WRITE);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_add_lerp_segment(curve_container   curve,
                                                         system_time       start_time,
                                                         system_time       end_time,
                                                         system_variant    start_value,
                                                         system_variant    end_value,
                                                         curve_segment_id* out_segment_id_ptr)
{
    _curve_container* curve_ptr   = reinterpret_cast<_curve_container*>(curve);
    curve_segment     new_segment = curve_segment_create_linear(start_time,
                                                                start_value,
                                                                end_time,
                                                                end_value);

    if (new_segment != nullptr)
    {
        curve_segment_set_on_segment_changed_callback(new_segment,
                                                      _curve_container_on_curve_segment_changed,
                                                      &curve_ptr->data);

        return _curve_container_add_segment_shared(curve,
                                                   start_time,
                                                   end_time,
                                                   new_segment,
                                                   out_segment_id_ptr);
    }
    else
    {
        return false;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_add_static_value_segment(curve_container   curve,
                                                                 system_time       start_time,
                                                                 system_time       end_time,
                                                                 system_variant    value,
                                                                 curve_segment_id* out_segment_id_ptr)
{
    _curve_container* curve_ptr   = reinterpret_cast<_curve_container*>(curve);
    curve_segment     new_segment = curve_segment_create_static(value);

    if (new_segment != nullptr)
    {
        curve_segment_set_on_segment_changed_callback(new_segment,
                                                      _curve_container_on_curve_segment_changed,
                                                      &curve_ptr->data);

        return _curve_container_add_segment_shared(curve,
                                                   start_time,
                                                   end_time,
                                                   new_segment,
                                                   out_segment_id_ptr);
    }
    else
    {
        return false;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_add_tcb_node(curve_container        curve,
                                                     curve_segment_id       segment_id,
                                                     system_time            time,
                                                     system_variant         value,
                                                     float                  tension,
                                                     float                  continuity,
                                                     float                  bias,
                                                     curve_segment_node_id* out_node_id)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_WRITE);
    {
        curve_segment segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &segment);
        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain segment with id [%d]",
                          segment_id);

        if (result)
        {
            _curve_container_segment_ptr curve_segment_ptr = (_curve_container_segment_ptr) segment;

            ASSERT_DEBUG_SYNC(curve_segment_ptr->segment != nullptr,
                              "Segment data is null!");

            if (curve_segment_ptr->segment != nullptr)
            {
                result = curve_segment_add_node(curve_segment_ptr->segment,
                                                time,
                                                value,
                                                out_node_id);

                if (result)
                {
                    system_variant property_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

                    system_variant_set_float          (property_variant,
                                                       bias);
                    curve_segment_modify_node_property(curve_segment_ptr->segment,
                                                      *out_node_id,
                                                       CURVE_SEGMENT_NODE_PROPERTY_BIAS,
                                                       property_variant);

                    system_variant_set_float          (property_variant,
                                                       continuity);
                    curve_segment_modify_node_property(curve_segment_ptr->segment,
                                                      *out_node_id,
                                                       CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY,
                                                       property_variant);

                    system_variant_set_float          (property_variant,
                                                       tension);
                    curve_segment_modify_node_property(curve_segment_ptr->segment,
                                                      *out_node_id,
                                                       CURVE_SEGMENT_NODE_PROPERTY_TENSION,
                                                       property_variant);

                    system_variant_release(property_variant);
                }
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_WRITE);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_add_tcb_segment(curve_container   curve,
                                                        system_time       start_time,
                                                        system_time       end_time,
                                                        system_variant    start_value,
                                                        float             start_tension,
                                                        float             start_continuity,
                                                        float             start_bias,
                                                        system_variant    end_value,
                                                        float             end_tension,
                                                        float             end_continuity,
                                                        float             end_bias,
                                                        curve_segment_id* out_segment_id)
{
    float start_tcb[3] = {start_tension,
                          start_continuity,
                          start_bias};
    float end_tcb  [3] = {end_tension,
                          end_continuity,
                          end_bias};

    _curve_container_ptr   curve_container      = (_curve_container_ptr) curve;
    _curve_container_data* curve_container_data = &curve_container->data;

    /* The "curve_dimension_ptr->last_used_segment_id+1" part is nasty, but oh well */
    curve_segment new_segment = curve_segment_create_tcb(start_time,
                                                         start_tcb,
                                                         start_value,
                                                         end_time,
                                                         end_tcb,
                                                         end_value,
                                                         curve,
                                                         curve_container_data->last_used_segment_id + 1);

    if (new_segment != nullptr)
    {
        curve_segment_set_on_segment_changed_callback(new_segment,
                                                      _curve_container_on_curve_segment_changed,
                                                      curve_container_data);

        return _curve_container_add_segment_shared(curve,
                                                   start_time,
                                                   end_time,
                                                   new_segment,
                                                   out_segment_id);
    }
    else
    {
        return false;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API curve_container curve_container_create(system_hashed_ansi_string name,
                                                          system_hashed_ansi_string scene_name,
                                                          system_variant_type       data_type)
{
    _curve_container* new_container = new (std::nothrow) _curve_container;

    ASSERT_DEBUG_SYNC(new_container != nullptr,
                      "Could not allocate _curve_container");

    if (new_container != nullptr)
    {
        _init_curve_container( (_curve_container_ptr) new_container,
                               data_type,
                               name);

        _curve_container_recalculate_curve_length( (curve_container) new_container);
    }

    /* Register the container */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_container,
                                                   _curve_container_release,
                                                   OBJECT_TYPE_CURVE_CONTAINER,
                                                   GET_OBJECT_PATH(name,
                                                                   OBJECT_TYPE_CURVE_CONTAINER,
                                                                   scene_name) );

    system_callback_manager_call_back(system_callback_manager_get(),
                                      CALLBACK_ID_CURVE_CONTAINER_ADDED,
                                      nullptr); /* callback_proc_data */

    /* Return the container */
    return (curve_container) new_container;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_delete_node(curve_container       curve,
                                                    curve_segment_id      segment_id,
                                                    curve_segment_node_id node_id)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        _curve_container_segment_ptr curve_segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &curve_segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not find segment descriptor");

        if (result)
        {
            ASSERT_DEBUG_SYNC(curve_segment->segment != nullptr,
                              "Segment is null.");

            if (curve_segment->segment != nullptr)
            {
                result = curve_segment_delete_node(curve_segment->segment,
                                                   node_id);
            }
            else
            {
                result = false;
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_delete_segment(curve_container  curve,
                                                       curve_segment_id segment_id)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    ASSERT_DEBUG_SYNC(!curve_data_ptr->pre_post_behavior_status,
                      "Do not touch segments when pre/post behavior support is on");

    if (!curve_data_ptr->pre_post_behavior_status)
    {
        system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                     ACCESS_WRITE);
        {
            if (!system_hash64map_contains(curve_data_ptr->segments,
                                           segment_id))
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Segment with id [%d] was not found in the curve",
                                  segment_id);

                system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                               ACCESS_WRITE);

                goto end;
            }

            // Remove the segment from order vector.
            size_t order_item_iterator = system_resizable_vector_find(curve_data_ptr->segments_order,
                                                                      reinterpret_cast<void*>(static_cast<intptr_t>(segment_id) ));

            if (order_item_iterator != ITEM_NOT_FOUND)
            {
                system_resizable_vector_delete_element_at(curve_data_ptr->segments_order,
                                                          order_item_iterator);
            }

            // Now for the actual item.
            system_hash64map_remove(curve_data_ptr->segments,
                                    segment_id);

            curve_data_ptr->last_read_time = -1;
            result                         = true;

            _curve_container_recalculate_curve_length(curve);
        }
        system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                       ACCESS_WRITE);
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_default_value(curve_container curve,
                                                          bool            should_force,
                                                          system_variant  out_value)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;

    system_variant_set(out_value,
                       curve_data_ptr->default_value,
                       should_force);

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_general_node_data(curve_container       curve,
                                                              curve_segment_id      segment_id,
                                                              curve_segment_node_id node_id,
                                                              system_time*          out_node_time_ptr,
                                                              system_variant        out_node_value)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        curve_segment segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain segment with id [%d]",
                          segment_id);
        if (result)
        {
            _curve_container_segment_ptr curve_segment = (_curve_container_segment_ptr) segment;

            ASSERT_DEBUG_SYNC(curve_segment->segment != nullptr,
                              "Null segment with id [%d] encountered",
                              segment_id);

            if (curve_segment->segment != nullptr)
            {
                result = curve_segment_get_node(curve_segment->segment,
                                                node_id,
                                                out_node_time_ptr,
                                                out_node_value);
            }
            else
            {
                result = false;
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_node_id_for_node_at(curve_container        curve,
                                                                curve_segment_id       segment_id,
                                                                uint32_t               n_node,
                                                                curve_segment_node_id* out_result)
{
    curve_segment segment = curve_container_get_segment(curve,
                                                        segment_id);

    if (segment != nullptr)
    {
        return curve_segment_get_node_by_index(segment,
                                               n_node,
                                               out_result);
    }
    else
    {
        return nullptr;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_node_id_for_node_in_order(curve_container        curve,
                                                                      curve_segment_id       segment_id,
                                                                      uint32_t               node_index,
                                                                      curve_segment_node_id* out_node_id_ptr)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        uint32_t                     n_node        = 0;
        _curve_container_segment_ptr curve_segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &curve_segment);
        if (result)
        {
            result = curve_segment_get_node_in_order(curve_segment->segment,
                                                     n_node,
                                                     out_node_id_ptr);
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_node_property(curve_container             curve,
                                                          curve_segment_id            segment_id,
                                                          curve_segment_node_id       node_id,
                                                          curve_segment_node_property node_property,
                                                          system_variant              out_result)
{
    _curve_container_ptr   curve_container = (_curve_container_ptr) curve;
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        curve_segment segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain segment with id [%d]",
                          segment_id);
        if (result)
        {
            _curve_container_segment_ptr curve_segment = (_curve_container_segment_ptr) segment;

            ASSERT_DEBUG_SYNC(curve_segment->segment != nullptr,
                              "Null segment with id [%d] encountered",
                              segment_id);

            if (curve_segment->segment != nullptr)
            {
                result = curve_segment_get_node_property(curve_segment->segment,
                                                         node_id,
                                                         node_property,
                                                         out_result);
            }
            else
            {
                result = false;
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void curve_container_get_property(curve_container          curve,
                                                     curve_container_property property,
                                                     void*                    out_result_ptr)
{
    _curve_container* curve_ptr = reinterpret_cast<_curve_container*>(curve);

    switch (property)
    {
        case CURVE_CONTAINER_PROPERTY_DATA_TYPE:
        {
            *reinterpret_cast<system_variant_type*>(out_result_ptr) = curve_ptr->data_type;

            break;
        }

        case CURVE_CONTAINER_PROPERTY_LENGTH:
        {
            *reinterpret_cast<system_time*>(out_result_ptr) = curve_ptr->data.length;

            break;
        }

        case CURVE_CONTAINER_PROPERTY_N_SEGMENTS:
        {
            system_read_write_mutex_lock(curve_ptr->data.segments_read_write_mutex,
                                         ACCESS_READ);
            {
                system_hash64map_get_property(curve_ptr->data.segments,
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                              out_result_ptr);
            }
            system_read_write_mutex_unlock(curve_ptr->data.segments_read_write_mutex,
                                           ACCESS_READ);

            break;
        }

        case CURVE_CONTAINER_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = (reinterpret_cast<_curve_container_ptr>(curve))->name;

            break;
        }

        case CURVE_CONTAINER_PROPERTY_POST_BEHAVIOR:
        {
            *reinterpret_cast<curve_container_envelope_boundary_behavior*>(out_result_ptr) = curve_ptr->data.post_behavior;

            break;
        }

        case CURVE_CONTAINER_PROPERTY_PRE_BEHAVIOR:
        {
            *reinterpret_cast<curve_container_envelope_boundary_behavior*>(out_result_ptr) = curve_ptr->data.pre_behavior;

            break;
        }

        case CURVE_CONTAINER_PROPERTY_PRE_POST_BEHAVIOR_STATUS:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = curve_ptr->data.pre_post_behavior_status;

            break;
        }

        case CURVE_CONTAINER_PROPERTY_START_TIME:
        {
            size_t n_segments = 0;

            system_hash64map_get_property(curve_ptr->data.segments,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_segments);

            ASSERT_DEBUG_SYNC(n_segments > 0,
                              "Start time was requested for a curve without any segments.");

            if (n_segments > 0)
            {
                curve_segment_id start_segment_id = 0;
                bool             result_get       = false;

                result_get = static_cast<curve_segment_id>(system_resizable_vector_get_element_at(curve_ptr->data.segments_order,
                                                                                                  0,
                                                                                                 &start_segment_id) );

                ASSERT_DEBUG_SYNC(result_get,
                                  "Could not retrieve start segment id.");

                _curve_container_segment_ptr curve_segment = nullptr;
                bool                         result        = system_hash64map_get(curve_ptr->data.segments,
                                                                                  start_segment_id,
                                                                                 &curve_segment);

                ASSERT_DEBUG_SYNC(result,
                                  "Could not obtain start segment descriptor");

                *reinterpret_cast<system_time*>(out_result_ptr) = curve_segment->start_time;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_container_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_segment_id_for_nth_segment(curve_container   curve,
                                                                       uint32_t          n_segment,
                                                                       curve_segment_id* out_segment_id_ptr)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        size_t           n_segments = 0;
        curve_segment_id segment_id = 0;

        system_resizable_vector_get_property(curve_data_ptr->segments_order,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_segments);

        if (n_segment < n_segments)
        {
            system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                   n_segment,
                                                   out_segment_id_ptr);

            result = true;
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_segment_id_relative_to_time(bool              should_move,
                                                                        bool              move_towards_past,
                                                                        curve_container   curve,
                                                                        system_time       time,
                                                                        curve_segment_id  segment_id_to_exclude,
                                                                        curve_segment_id* out_segment_id)
{
    bool result = false;

    if (time >= 0)
    {
        _curve_container_ptr   curve_container     = reinterpret_cast<_curve_container_ptr>(curve);
        _curve_container_data* curve_data_ptr      = &curve_container->data;
        size_t                 n_segments          = 0;
        bool                   has_found_segment   = false;
        uint32_t               segment_id_iterator = 0;
        uint32_t               n_segment_ids       = 0;
        curve_segment_id       segment_id          = static_cast<curve_segment_id>(-1);

        system_hash64map_get_property(curve_data_ptr->segments,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_segments);

        system_resizable_vector_get_property(curve_data_ptr->segments_order,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_segment_ids);

        for (;
             segment_id_iterator < n_segment_ids;
           ++segment_id_iterator)
        {
            _curve_container_segment_ptr curve_segment = nullptr;
            bool                         result_get    = false;

            result_get = static_cast<curve_segment_id>(system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                                                              segment_id_iterator,
                                                                                             &segment_id) );
            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve segment id.");

            system_hash64map_get(curve_data_ptr->segments,
                                 segment_id,
                                &curve_segment);

            if (curve_segment != nullptr)
            {
                if (time       >= curve_segment->start_time &&
                    time       <= curve_segment->end_time   &&
                    segment_id != segment_id_to_exclude)
                {
                    has_found_segment = true;

                    break;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(curve_segment != nullptr,
                                  "Curve segment extracted from segments hash64map is null.");

                goto end;
            }
        }

        if (has_found_segment)
        {
            *out_segment_id = segment_id;
            result          = true;

            goto end;
        }
        else
        if (n_segments > 0 && should_move)
        {
            segment_id_iterator = (move_towards_past ? (n_segment_ids - 1) : 0);

            while (true)
            {
                _curve_container_segment_ptr curve_segment = nullptr;
                bool                         result_get    = false;

                result_get = system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                                    segment_id_iterator,
                                                                   &segment_id);
                ASSERT_DEBUG_SYNC(result_get,
                                  "Could not retrieve segment id");

                system_hash64map_get(curve_data_ptr->segments,
                                     segment_id,
                                    &curve_segment);

                if (( move_towards_past && (curve_segment->start_time < time || curve_segment->end_time < time) && segment_id != segment_id_to_exclude) ||
                    (!move_towards_past && (curve_segment->start_time > time || curve_segment->end_time > time) && segment_id != segment_id_to_exclude) )
                {
                    *out_segment_id = segment_id;
                    result          = true;

                    goto end;
                }

                if (move_towards_past)
                {
                    --segment_id_iterator;

                    if (segment_id_iterator == 0)
                    {
                        break;
                    }
                }
                else
                {
                    ++segment_id_iterator;

                    if (segment_id_iterator == n_segment_ids)
                    {
                        break;
                    }
                }
            }
        }
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API curve_segment curve_container_get_segment(curve_container  curve,
                                                             curve_segment_id segment_id)
{
    _curve_container*         curve_ptr      = reinterpret_cast<_curve_container*>(curve);
    _curve_container_data*    curve_data_ptr = &curve_ptr->data;
    _curve_container_segment* result         = nullptr;

    system_hash64map_get(curve_data_ptr->segments,
                         segment_id,
                        &result);

    if (result != nullptr)
    {
        return result->segment;
    }
    else
    {
        return nullptr;
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API bool curve_container_get_segment_property(curve_container                  container,
                                                             curve_segment_id                 segment_id,
                                                             curve_container_segment_property property,
                                                             void*                            out_result_ptr)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(container);
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = true;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        _curve_container_segment_ptr curve_segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &curve_segment);

        if (result)
        {
            switch (property)
            {
                case CURVE_CONTAINER_SEGMENT_PROPERTY_N_NODES:
                {
                    curve_segment_get_amount_of_nodes(curve_segment->segment,
                                                      reinterpret_cast<uint32_t*>(out_result_ptr) );

                    break;
                }

                case CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME:
                {
                    *reinterpret_cast<system_time*>(out_result_ptr) = curve_segment->start_time;

                    break;
                }

                case CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME:
                {
                    *reinterpret_cast<system_time*>(out_result_ptr) = curve_segment->end_time;

                    break;
                }

                case CURVE_CONTAINER_SEGMENT_PROPERTY_TYPE:
                {
                    curve_segment_get_type(curve_segment->segment,
                                           static_cast<curve_segment_type*>(out_result_ptr) );

                    break;
                }

                case CURVE_CONTAINER_SEGMENT_PROPERTY_THRESHOLD:
                {
                    *reinterpret_cast<float*>(out_result_ptr) = curve_segment->threshold;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized curve_container_segment_property value");
                }
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_get_value(curve_container curve,
                                                  system_time     time,
                                                  bool            should_force,
                                                  system_variant  out_value)
{
    _curve_container_ptr   curve_container_ptr      = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr           = &curve_container_ptr->data;
    bool                   should_get_default_value = true;
    bool                   result                   = true;

    if (curve_data_ptr->last_read_time == time)
    {
        system_variant_set(out_value,
                           curve_data_ptr->last_read_value,
                           false);

        return true;
    }

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        uint32_t n_segments_order = 0;

        system_resizable_vector_get_property(curve_data_ptr->segments_order,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_segments_order);

        for (uint32_t segments_order_iterator = 0;
                      segments_order_iterator < n_segments_order;
                    ++segments_order_iterator)
        {
            curve_segment_id             curve_segmentid = 0;
            _curve_container_segment_ptr curve_segment   = nullptr;
            bool                         result_get      = false;

            result_get = system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                                segments_order_iterator,
                                                               &curve_segmentid);
            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve curve segment id");

            system_hash64map_get(curve_data_ptr->segments,
                                 curve_segmentid,
                                &curve_segment);

            if (curve_segment->start_time <= time &&
                curve_segment->end_time   >= time)
            {
                /* Assuming non-normalized time is needed for segments */
                result                   = curve_segment_get_value(curve_segment->segment,
                                                                   time,
                                                                   should_force,
                                                                   out_value);
                should_get_default_value = false;

                break;
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    if (should_get_default_value)
    {
        _curve_container_segment_ptr start_curve_segment = nullptr;
        unsigned int                 n_segments_order    = 0;

        system_resizable_vector_get_property(curve_data_ptr->segments_order,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_segments_order);

        if (n_segments_order > 0)
        {
            curve_segment_id start_segment_id = 0;
            bool             result_get       = false;

            result_get = system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                                0,
                                                               &start_segment_id);
            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve start segment id.");

            system_hash64map_get(curve_data_ptr->segments,
                                 start_segment_id,
                                &start_curve_segment);
        }

        if (!curve_data_ptr->pre_post_behavior_status                          ||
            (start_curve_segment != 0 && start_curve_segment->end_time > time))
        {
            // Return default value if no segment was found.
            result = true;

            system_variant_set(out_value,
                               curve_data_ptr->default_value,
                               should_force);
        }
        else
        {
            ASSERT_DEBUG_SYNC(start_curve_segment != nullptr,
                             "Start curve segment is null!");

            if (start_curve_segment != nullptr)
            {
                result = _curve_container_get_pre_post_behavior_value( (curve_container) curve_container_ptr,
                                                                       curve_data_ptr,
                                                                       (start_curve_segment->start_time > time),
                                                                       time,
                                                                       out_value);
                ASSERT_DEBUG_SYNC(result,
                                  "Could not get pre/post-behavior value.");
            }
        }
    }

    /* Cache the result */
    if (result)
    {
        curve_data_ptr->last_read_time = time;

        system_variant_set(curve_data_ptr->last_read_value,
                           out_value,
                           should_force);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_is_equal(curve_container curve_a,
                                                 curve_container curve_b)
{
    bool                 result      = true;
    _curve_container_ptr curve_a_ptr = reinterpret_cast<_curve_container_ptr>(curve_a);
    _curve_container_ptr curve_b_ptr = reinterpret_cast<_curve_container_ptr>(curve_b);

    if (curve_a_ptr->data_type == curve_b_ptr->data_type)
    {
        _curve_container_data* curve_a_data = &curve_a_ptr->data;
        _curve_container_data* curve_b_data = &curve_b_ptr->data;

        if (!(system_hash64map_is_equal       (curve_a_data->segments,
                                               curve_b_data->segments)                 &&
              system_resizable_vector_is_equal(curve_a_data->segments_order,
                                               curve_b_data->segments_order)           &&
              system_variant_is_equal         (curve_a_data->default_value,
                                               curve_b_data->default_value)            &&
              curve_a_data->last_used_segment_id == curve_b_data->last_used_segment_id &&
              curve_a_data->length               == curve_b_data->length)
           )
        {
            result = false;
        }
    }
    else
    {
        result = false;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_modify_node(curve_container       curve,
                                                    curve_segment_id      segment_id,
                                                    curve_segment_node_id node_id,
                                                    system_time           new_node_time,
                                                    system_variant        new_node_value)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr  = &curve_container->data;
    bool                   result          = false;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_WRITE);
    {
        _curve_container_segment_ptr curve_segment = nullptr;

        result = system_hash64map_get(curve_data_ptr->segments,
                                      segment_id,
                                     &curve_segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain curve segment");

        if (result)
        {
            result = (curve_segment->segment != nullptr);

            ASSERT_DEBUG_SYNC(result,
                              "Curve segment is null!");

            if (result)
            {
                result = curve_segment_modify_node_time_value(curve_segment->segment,
                                                              node_id,
                                                              new_node_time,
                                                              new_node_value,
                                                              false);

                if (result)
                {
                    curve_segment_type segment_type = CURVE_SEGMENT_UNDEFINED;

                    curve_segment_get_type(curve_segment->segment,
                                          &segment_type);

                    if (segment_type == CURVE_SEGMENT_LERP)
                    {
                        if (node_id == 0)
                        {
                            if (!curve_container_is_range_defined(curve,
                                                                  new_node_time,
                                                                  curve_segment->end_time,
                                                                  segment_id))
                            {
                                curve_segment->start_time = new_node_time;
                            }
                        }
                        else
                        {
                            ASSERT_DEBUG_SYNC(node_id == 1,
                                              "Node id [%d] is invalid",
                                              node_id);

                            if (!curve_container_is_range_defined(curve,
                                                                  curve_segment->start_time,
                                                                  new_node_time,
                                                                  segment_id))
                            {
                                curve_segment->end_time = new_node_time;
                            }
                        }
                    }
                }
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_WRITE);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_set_default_value(curve_container curve,
                                                          system_variant  value)
{
    _curve_container_ptr   curve_container      = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_container_data = &curve_container->data;

    system_variant_set(curve_container_data->default_value,
                       value,
                       false);

    return true;
}

/** Please see header for spec */
PUBLIC EMERALD_API void curve_container_set_property(curve_container          container,
                                                     curve_container_property property,
                                                     const void*              data)
{
    _curve_container* container_ptr = reinterpret_cast<_curve_container*>(container);

    switch (property)
    {
        case CURVE_CONTAINER_PROPERTY_POST_BEHAVIOR:
        {
            container_ptr->data.post_behavior = *reinterpret_cast<const curve_container_envelope_boundary_behavior*>(data);

            break;
        }

        case CURVE_CONTAINER_PROPERTY_PRE_BEHAVIOR:
        {
            container_ptr->data.pre_behavior = *reinterpret_cast<const curve_container_envelope_boundary_behavior*>(data);

            break;
        }

        case CURVE_CONTAINER_PROPERTY_PRE_POST_BEHAVIOR_STATUS:
        {
            if (*reinterpret_cast<const bool*>(data) )
            {
                uint32_t n_segments = 0;

                system_hash64map_get_property(container_ptr->data.segments,
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_segments);

                ASSERT_DEBUG_SYNC(container_ptr->data.pre_behavior != CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED,
                                  "");
                ASSERT_DEBUG_SYNC(container_ptr->data.post_behavior != CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED,
                                  "");
                ASSERT_DEBUG_SYNC(n_segments == 1,
                                  "");

                if (container_ptr->data.pre_behavior  != CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED &&
                    container_ptr->data.post_behavior != CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED &&
                    n_segments                        == 1)
                {
                    container_ptr->data.pre_post_behavior_status = true;
                }
            }
            else
            {
                container_ptr->data.pre_post_behavior_status = false;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_container_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void curve_container_set_segment_property(curve_container                  curve,
                                                             curve_segment_id                 segment_id,
                                                             curve_container_segment_property property,
                                                             const void*                      in_data)
{
    _curve_container_ptr         curve_container_ptr  = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data*       curve_container_data = &curve_container_ptr->data;
    bool                         result               = false;
    _curve_container_segment_ptr segment_ptr          = nullptr;

    system_hash64map_get(curve_container_data->segments,
                         segment_id,
                        &segment_ptr);

    ASSERT_DEBUG_SYNC(segment_ptr != nullptr,
                      "Could not retrieve segment of id [%d]",
                      segment_id);

    if (segment_ptr != nullptr)
    {
        switch (property)
        {
            case CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME:
            {
                segment_ptr->start_time = *reinterpret_cast<const system_time*>(in_data);

                break;
            }

            case CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME:
            {
#if 0
                /* NOTE: This call used to be here but messed up stuff. The call modifies
                 *       nodes_order which can be used by the caller.
                 *
                 *       If you really need to make this call, refactor curve_container.
                 *       It's a bloody mess anyway.
                 */
                curve_container_set_segment_times(curve,
                                                  segment_id,
                                                  segment_ptr->start_time,
                                                  *(system_time*) in_data);
#endif
                segment_ptr->end_time = *reinterpret_cast<const system_time*>(in_data);

                break;
            }

            case CURVE_CONTAINER_SEGMENT_PROPERTY_THRESHOLD:
            {
                segment_ptr->threshold = *reinterpret_cast<const float*>(in_data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized curve_container_segment_property value");
            }
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_container_set_segment_times(curve_container  curve,
                                                          curve_segment_id segment_id,
                                                          system_time      new_segment_start_time,
                                                          system_time      new_segment_end_time)
{
    _curve_container_segment_ptr curr_place_segment           = nullptr;
    size_t                       curr_segments_order_iterator = -1;
    _curve_container_ptr         curve_container              = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data*       curve_container_data         = &curve_container->data;
    system_resizable_vector      internal_node_order          = nullptr;
    system_time                  former_start_time            = 0;
    system_time                  former_end_time              = 0;
    uint32_t                     n_nodes                      = 0;
    size_t                       n_segment_orders             = 0;
    bool                         needs_update                 = false;
    bool                         result                       = false;
    curve_segment                segment                      = nullptr;

    if (curve_container_data->is_set_segment_times_call_in_place)
    {
        /* This is a recursive call, taking place due to node movement action. Bail out! */
        return true;
    }
    else
    {
        curve_container_data->is_set_segment_times_call_in_place = true;
    }

    /* This mutex is unlocked at end: */
    system_read_write_mutex_lock(curve_container_data->segments_read_write_mutex,
                                 ACCESS_WRITE);

    curr_segments_order_iterator = system_resizable_vector_find(curve_container_data->segments_order,
                                                                reinterpret_cast<void*>(static_cast<intptr_t>(segment_id)));

    system_resizable_vector_get_property(curve_container_data->segments_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_segment_orders);

    ASSERT_DEBUG_SYNC(curr_segments_order_iterator != ITEM_NOT_FOUND,
                      "Could not find requested segment [%d]",
                      segment_id);

    if (curr_segments_order_iterator == ITEM_NOT_FOUND)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Curve segment not found");

        goto end;
    }

    if (new_segment_start_time < 0                     ||
        new_segment_end_time   < 0                     ||
        new_segment_start_time >= new_segment_end_time)
    {
        ASSERT_DEBUG_SYNC(false,
                          "New segment times are invalid");

        goto end;
    }

    if (curve_container_is_range_defined(curve,
                                         new_segment_start_time,
                                         new_segment_end_time,
                                         segment_id) )
    {
        goto end;
    }

    // Cache current times
    system_hash64map_get(curve_container_data->segments,
                         segment_id,
                        &curr_place_segment);

    former_start_time = curr_place_segment->start_time;
    former_end_time   = curr_place_segment->end_time;

    // Update time of encapsulated nodes. We need to cache node order before starting the operation because the order
    // usually changes as we iterate.
    segment = curr_place_segment->segment;
    result  = curve_segment_get_amount_of_nodes(segment,
                                               &n_nodes);

    ASSERT_DEBUG_SYNC(result,
                      "Could not retrieve amount of curve nodes.");

    internal_node_order = system_resizable_vector_create(n_nodes);

    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        uint32_t n_ordered_node = 0;

        // Order nodes in following order: first, last, second, third, etc..
        result = curve_segment_get_node_in_order(segment,
                                                 (n_node == 0 ? 0 :
                                                                (n_node == 1 ? n_nodes - 1 :
                                                                               n_node  - 1)),
                                                &n_ordered_node);

        ASSERT_DEBUG_SYNC(result,
                          "curve_segment_get_node_in_order() failed.");

        system_resizable_vector_push(internal_node_order,
                                     reinterpret_cast<void*>(static_cast<intptr_t>(n_ordered_node) ));
    }

    for (uint32_t n_internal_order_node = 0;
                  n_internal_order_node < n_nodes;
                ++n_internal_order_node)
    {
        curve_segment_node_id node_id        = 0;
        uint32_t              n_ordered_node = 0;
        bool                  result_get;

        result_get = system_resizable_vector_get_element_at(internal_node_order,
                                                            n_internal_order_node,
                                                           &n_ordered_node);
        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve element from node order vector.");

        result = curve_segment_get_node_by_index(segment,
                                                 n_ordered_node,
                                                &node_id);

        ASSERT_DEBUG_SYNC(result,
                          "Could not get node id for node with index [%d]",
                          n_ordered_node);
        if (result)
        {
            system_time node_time;

            result = curve_segment_get_node(segment,
                                            node_id,
                                           &node_time,
                                            nullptr);
            ASSERT_DEBUG_SYNC(result,
                              "Could not get node info [id=%d]",
                              node_id);

            if (result)
            {
                system_time time_delta;

                /* Move nodes relative to the new segment start time.
                 * For the very last node, use the end time. */
                if (n_ordered_node != 1)
                {
                    time_delta = new_segment_start_time - former_start_time;
                }
                else
                {
                    time_delta = new_segment_end_time - former_end_time;
                }

                node_time += time_delta;
                result     = curve_segment_modify_node_time(segment,
                                                            node_id,
                                                            node_time);

                ASSERT_DEBUG_SYNC(result,
                                  "Could not modify node.");

                if (!result)
                {
                    goto end;
                }
            }
        }
    }

    // Verify that the segment order we have at the moment is still correct.
    if (curr_segments_order_iterator != 0)
    {
        curve_segment_id             prev_segment_id    = 0;
        _curve_container_segment_ptr prev_curve_segment = nullptr;
        bool                         result_get         = false;

        result_get = system_resizable_vector_get_element_at(curve_container_data->segments_order,
                                                            curr_segments_order_iterator - 1,
                                                           &prev_segment_id);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve previous curve segment id.");

        system_hash64map_get(curve_container_data->segments,
                             prev_segment_id,
                            &prev_curve_segment);

        if (prev_curve_segment->end_time > curr_place_segment->start_time)
        {
            needs_update = true;
        }
    }

    if (!needs_update &&
        (curr_segments_order_iterator + 1 != n_segment_orders) )
    {
        curve_segment_id             next_segment_id    = 0;
        _curve_container_segment_ptr next_curve_segment = nullptr;
        bool                         result_get         = false;

        result_get = system_resizable_vector_get_element_at(curve_container_data->segments_order,
                                                            curr_segments_order_iterator + 1,
                                                           &next_segment_id);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve next segment id.");

        system_hash64map_get(curve_container_data->segments,
                             next_segment_id,
                            &next_curve_segment);

        if (curr_place_segment->end_time > next_curve_segment->start_time)
        {
            needs_update = true;
        }
    }

    // If segment start & end times have not changed during node update, update the times manually.
    if (former_start_time == curr_place_segment->start_time)
    {
        curr_place_segment->start_time = new_segment_start_time;
    }

    if (former_end_time == curr_place_segment->end_time)
    {
        curr_place_segment->end_time = new_segment_end_time;
    }

    if (needs_update)
    {
        // Segment order needs refreshing. Do a resort.
        size_t n_segments = 0;

        system_resizable_vector_empty(curve_container_data->segments_order);

        system_hash64map_get_property(curve_container_data->segments,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_segments);

        for (size_t n_segment = 0;
                    n_segment < n_segments;
                  ++n_segment)
        {
            system_hash64                curve_segment_id = 0;
            _curve_container_segment_ptr curve_segment    = nullptr;

            result = system_hash64map_get_element_at(curve_container_data->segments,
                                                     n_segment,
                                                    &curve_segment,
                                                   &curve_segment_id);

            ASSERT_DEBUG_SYNC(result,
                              "Error occured while extracting curve segment info");

            uint32_t new_location = _curve_container_find_appropriate_place_for_segment_at_time(curve_container_data,
                                                                                                curve_segment->start_time);

            system_resizable_vector_insert_element_at(curve_container_data->segments_order,
                                                      new_location,
                                                      reinterpret_cast<void*>(static_cast<intptr_t>(curve_segment_id) ));
        }
    }

    // Dealloc objects we allocated if this point is reached.
    system_resizable_vector_release(internal_node_order);

end:
    curve_container_data->is_set_segment_times_call_in_place = false;

    system_read_write_mutex_unlock(curve_container_data->segments_read_write_mutex,
                                   ACCESS_WRITE);

    return result;
}

/************************************************* PRIVATE IMPLEMENTATIONS ******************************************************/
/** TODO */
PRIVATE void _curve_container_recalculate_curve_length(curve_container curve)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr  = &curve_container->data;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_WRITE);
    {
        size_t n_segments = 0;

        system_hash64map_get_property(curve_data_ptr->segments,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_segments);

        if (n_segments > 0)
        {
            curve_segment_id start_segment_id = 0;
            bool             result_get       = false;

            result_get = system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                                0,
                                                               &start_segment_id);

            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve start segment id");

            _curve_container_segment_ptr start_segment = nullptr;
            bool                         result        = system_hash64map_get(curve_data_ptr->segments,
                                                                              start_segment_id,
                                                                             &start_segment);

            ASSERT_DEBUG_SYNC(result,
                              "Could not obtain start segment descriptor");

            system_time min_segment_start_time = start_segment->start_time;
            system_time max_segment_end_time   = start_segment->end_time;

            for (uint32_t n_segment = 0;
                          n_segment < n_segments;
                        ++n_segment)
            {
                _curve_container_segment_ptr curve_segment    = nullptr;
                system_hash64                curve_segment_id = 0;

                result = system_hash64map_get_element_at(curve_data_ptr->segments,
                                                         n_segment,
                                                        &curve_segment,
                                                        &curve_segment_id);

                ASSERT_DEBUG_SYNC(result,
                                  "");

                if (min_segment_start_time > curve_segment->start_time)
                {
                    min_segment_start_time = curve_segment->start_time;
                }

                if (max_segment_end_time < curve_segment->end_time)
                {
                    max_segment_end_time = curve_segment->end_time;
                }
            }

            curve_data_ptr->length = max_segment_end_time - min_segment_start_time;
        }
        else
        {
            curve_data_ptr->length = 0;
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_WRITE);
}

/** TODO */
PRIVATE void _curve_container_on_new_segment_start_time(curve_container  curve,
                                                        curve_segment_id segment_id,
                                                        system_time      new_start_time)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr  = &curve_container->data;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        _curve_container_segment_ptr curve_segment = nullptr;
        bool                         result        = system_hash64map_get(curve_data_ptr->segments,
                                                                          segment_id,
                                                                         &curve_segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain segment descriptor");

        if (result)
        {
            curve_segment->start_time = new_start_time;
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);
}

/** TODO */
PRIVATE void _curve_container_on_new_segment_end_time(curve_container  curve,
                                                      curve_segment_id segment_id,
                                                      system_time      new_end_time)
{
    _curve_container_ptr   curve_container = reinterpret_cast<_curve_container_ptr>(curve);
    _curve_container_data* curve_data_ptr  = &curve_container->data;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        _curve_container_segment_ptr curve_segment = nullptr;
        bool                         result        = system_hash64map_get(curve_data_ptr->segments,
                                                                          segment_id,
                                                                         &curve_segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not obtain segment descriptor");

        if (result)
        {
            curve_segment->end_time = new_end_time;
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);
}

/** TODO */
PRIVATE uint32_t _curve_container_find_appropriate_place_for_segment_at_time(_curve_container_data* curve_data_ptr,
                                                                             system_time            start_time)
{
    uint32_t n_segment_orders = 0;
    uint32_t place_iterator   = n_segment_orders;

    system_resizable_vector_get_property(curve_data_ptr->segments_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_segment_orders);

    for (uint32_t place_it  = 0;
                  place_it != n_segment_orders;
                ++place_it)
    {
        curve_segment_id curve_segmentid = 0;
        bool             result_get      = false;

        result_get = system_resizable_vector_get_element_at(curve_data_ptr->segments_order,
                                                            place_it,
                                                           &curve_segmentid);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve curve segment id");

        _curve_container_segment_ptr curve_segment = nullptr;
        bool                         result        = system_hash64map_get(curve_data_ptr->segments,
                                                                          curve_segmentid,
                                                                         &curve_segment);

        ASSERT_DEBUG_SYNC(result,
                          "Could not get curve segment descriptor.");

        if (curve_segment->start_time > start_time)
        {
            place_iterator = place_it;

            break;
        }
    }

    return place_iterator;
}

/** TODO */
PRIVATE bool _curve_container_get_pre_post_behavior_value(curve_container        curve,
                                                          _curve_container_data* curve_data,
                                                          bool                   use_pre_behavior_value,
                                                          system_time            time,
                                                          system_variant         out_result)
{
    bool result = false;

    ASSERT_DEBUG_SYNC(curve_data->pre_post_behavior_status,
                      "Pre post behavior status is disabled - cannot provide with pre/post behavior value.");

    if (curve_data->pre_post_behavior_status)
    {
        result = true;

        switch(curve_data->pre_behavior)
        {
            case CURVE_CONTAINER_BOUNDARY_BEHAVIOR_RESET:
            {
                system_variant_set_float_forced(out_result,
                                                0.0f);

                break;
            }

            case CURVE_CONTAINER_BOUNDARY_BEHAVIOR_REPEAT:
            {
                system_time                  first_node_time     = 0;
                system_time                  last_node_time      = 0;
                _curve_container_segment_ptr first_curve_segment = nullptr;
                curve_segment_id             segment_id          = 0;
                bool                         result_get          = false;

                result_get = system_resizable_vector_get_element_at(curve_data->segments_order,
                                                                    0,
                                                                   &segment_id);

                ASSERT_DEBUG_SYNC(result_get,
                                  "Could not retrieve first segment's id.");

                system_hash64map_get(curve_data->segments,
                                     segment_id,
                                    &first_curve_segment);

                first_node_time = first_curve_segment->start_time;
                last_node_time  = first_curve_segment->end_time;

                system_time time_to_use = _curve_container_get_range(time,
                                                                     first_node_time,
                                                                     last_node_time);

                // Protect against infinite recursion.
                if (time_to_use < first_node_time)
                {
                    time_to_use = first_node_time;
                }
                else
                if (time_to_use > last_node_time)
                {
                    time_to_use = last_node_time;
                }

                result = curve_container_get_value(curve,
                                                   time_to_use,
                                                   true,
                                                   out_result);

                break;
            }

            case CURVE_CONTAINER_BOUNDARY_BEHAVIOR_CONSTANT:
            {
                system_time                  node_time           = 0;
                _curve_container_segment_ptr first_curve_segment = nullptr;
                curve_segment_id             segment_id          = 0;
                bool                         result_get          = false;

                result_get = system_resizable_vector_get_element_at(curve_data->segments_order,
                                                                    0,
                                                                   &segment_id);

                ASSERT_DEBUG_SYNC(result_get,
                                  "Could not retrieve first segment's id.");

                system_hash64map_get(curve_data->segments,segment_id,
                                    &first_curve_segment);

                if (use_pre_behavior_value)
                {
                    result = curve_segment_get_node( first_curve_segment->segment,
                                                     0,
                                                    &node_time,
                                                     out_result);
                }
                else
                {
                    uint32_t n_nodes = 0;

                    curve_segment_get_amount_of_nodes( first_curve_segment->segment,
                                                      &n_nodes);

                    result = curve_segment_get_node( first_curve_segment->segment,
                                                     n_nodes - 1,
                                                    &node_time,
                                                     out_result);
                }
                ASSERT_DEBUG_SYNC(result,
                                  "Could not get node data.");

                break;
            }

            default:
            {
                if (use_pre_behavior_value)
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Pre-behavior value was requested for unsupported boundary behavior [%d]",
                                       curve_data->pre_behavior);
                }
                else
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Post-behavior value was requested for unsupported boundary behavior [%d]",
                                       curve_data->post_behavior);
                }
            }
        }
    }

    return result;
}

/** TODO */
PUBLIC bool curve_container_is_range_defined(curve_container  curve,
                                             system_time      start_time,
                                             system_time      end_time,
                                             curve_segment_id excluded_segment_id)
{
    bool                   result         = false;
    _curve_container*      curve_ptr      = reinterpret_cast<_curve_container*>(curve);
    _curve_container_data* curve_data_ptr = &curve_ptr->data;

    system_read_write_mutex_lock(curve_data_ptr->segments_read_write_mutex,
                                 ACCESS_READ);
    {
        size_t n_segments = 0;

        system_hash64map_get_property(curve_data_ptr->segments,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_segments);

        for (size_t n_segment = 0;
                    n_segment < n_segments;
                  ++n_segment)
        {
            _curve_container_segment_ptr curve_segment = nullptr;
            system_hash64                segment_id    = 0;

            bool b_result = system_hash64map_get_element_at(curve_data_ptr->segments,
                                                            n_segment,
                                                           &curve_segment,
                                                           &segment_id);

            ASSERT_DEBUG_SYNC(b_result,
                              "Could not find [%d]th segment",
                              n_segment);

            if (segment_id == excluded_segment_id)
            {
                continue;
            }

            ASSERT_DEBUG_SYNC(curve_segment != nullptr,
                              "Curve segment is nullptr!");

            if (MAX(start_time,                end_time)                > MIN(curve_segment->start_time, curve_segment->end_time) &&
                MAX(curve_segment->start_time, curve_segment->end_time) > MIN(start_time,                end_time) )
            {
                result = true;

                break;
            }
        }
    }
    system_read_write_mutex_unlock(curve_data_ptr->segments_read_write_mutex,
                                   ACCESS_READ);

    return result;
}

/* TODO */
PRIVATE system_time _curve_container_get_range(system_time v,
                                               system_time lo,
                                               system_time hi)
{
    system_time r      = hi - lo;
    system_time result = lo;

    if (r != 0)
    {
        result = v - r * system_time(floor(double(v - lo) / double(r) ) );
    }

    return result;
}

/* TODO */
PRIVATE void _curve_container_on_curve_segment_changed(void* user_arg)
{
    _curve_container_data* data_ptr = reinterpret_cast<_curve_container_data*>(user_arg);

    /* Reset the 'read time' so that we force the value to be probed on next "get" call */
    data_ptr->last_read_time = -1;
}
