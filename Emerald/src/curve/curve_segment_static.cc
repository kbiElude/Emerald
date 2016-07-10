/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "curve/curve_segment_static.h"
#include "system/system_assertions.h"
#include "system/system_variant.h"

/** Internal type definitions */
typedef struct
{
    system_variant value;
} _curve_segment_data_static;

typedef _curve_segment_data_static* _curve_segment_data_static_ptr;


/** Deinitializes a previously initialized instance of _curve_segment_data_static structure.
 *
 *  @param segment_data Pointer to where the instance is located. DO NOT use this pointer after
 *                      calling this function.
 **/
PRIVATE void _curve_segment_data_static_deinit(curve_segment_data segment_data)
{
    _curve_segment_data_static_ptr data_ptr = reinterpret_cast<_curve_segment_data_static_ptr>(segment_data);

    system_variant_release(data_ptr->value);

    delete reinterpret_cast<_curve_segment_data_static_ptr>(segment_data);
}

/** Initializes internal curve segment data storage for a static segment.
 *
 *  @param segment_data *segment_data will be used to store a _curve_segment_data_static instance. CANNOT be null.
 *  @param data_type    Data type to use for value storage.
 **/
PRIVATE void _curve_segment_data_static_init(curve_segment_data* segment_data,
                                             system_variant      value)
{
    _curve_segment_data_static* new_segment_data_ptr = new (std::nothrow) _curve_segment_data_static;

    ASSERT_ALWAYS_SYNC(new_segment_data_ptr != nullptr,
                       "Could not allocate static curve segment!");

    if (new_segment_data_ptr != nullptr)
    {
        new_segment_data_ptr->value = system_variant_create(system_variant_get_type(value) );

        system_variant_set(new_segment_data_ptr->value,
                           value,
                           false);
    }

    *segment_data = reinterpret_cast<curve_segment_data>(new_segment_data_ptr);
}


/** Please see header for specification */
PUBLIC bool curve_segment_static_add_node(curve_segment_data     segment_data,
                                          system_time            node_time,
                                          system_variant         node_value,
                                          curve_segment_node_id* out_node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_delete_node(curve_segment_data    segment_data,
                                             curve_segment_node_id node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_deinit(curve_segment_data segment_data)
{
    _curve_segment_data_static_deinit(segment_data);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_amount_of_nodes(curve_segment_data segment_data,
                                                     uint32_t*          out_result)
{
    return 1;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_node(curve_segment_data    segment_data,
                                          curve_segment_node_id node_id,
                                          system_time*          out_node_time,
                                          system_variant        out_node_value)
{
    bool result = false;

    ASSERT_DEBUG_SYNC(node_id == 0,
                      "Invalid node id (%d) requested",
                      node_id);

    if (node_id == 0)
    {
        _curve_segment_data_static_ptr data_ptr = reinterpret_cast<_curve_segment_data_static_ptr>(segment_data);

        if (out_node_time != nullptr)
        {
            *out_node_time = 0;
        }

        if (out_node_value != nullptr)
        {
            system_variant_set(out_node_value,
                               data_ptr->value,
                               false);
        }

        result = true;
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_node_in_order(curve_segment_data    segment_data,
                                                   uint32_t               node_index,
                                                   curve_segment_node_id* out_node_id_ptr)
{
    bool result = false;

    ASSERT_DEBUG_SYNC(node_index >= 0 &&
                      node_index <= 1,
                      "Invalid node index (%d) requested",
                      node_index);

    if (node_index >= 0 && node_index <= 1)
    {
        *out_node_id_ptr = node_index;
        result           = true;
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_property(curve_segment_data     segment_data,
                                              curve_segment_property segment_property,
                                              system_variant         value)
{
    /* Should not be called */
    ASSERT_DEBUG_SYNC(false,
                      "curve_segment_static_get_property() should never be called - other means exist to query the segment's properties.");

    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_value(curve_segment_data segment_data,
                                           system_time        time,
                                           system_variant     out_result,
                                           bool               should_force)
{
    _curve_segment_data_static_ptr data_ptr = reinterpret_cast<_curve_segment_data_static_ptr>(segment_data);

    system_variant_set(out_result,
                       data_ptr->value,
                       should_force);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_init(curve_segment_data* segment_data,
                                      system_variant      value)
{
    _curve_segment_data_static_init(segment_data,
                                    value);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_modify_node_time(curve_segment_data    segment_data,
                                                  curve_segment_node_id node_id,
                                                  system_time           new_node_time)
{
    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_modify_node_time_value(curve_segment_data    segment_data,
                                                        curve_segment_node_id node_id,
                                                        system_time           new_node_time,
                                                        system_variant        new_node_value,
                                                        bool                  unused)
{
    _curve_segment_data_static_ptr data_ptr = reinterpret_cast<_curve_segment_data_static_ptr>(segment_data);

    system_variant_set(data_ptr->value,
                       new_node_value,
                       false);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_set_property(curve_segment_data     segment_data,
                                              curve_segment_property segment_property,
                                              system_variant         value)
{
    /* Should not be called */
    ASSERT_DEBUG_SYNC(false,
                      "curve_segment_static_set_property() should never be called - other means exist to modify the segment's properties.");

    return false;
}

