/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "curve/curve_segment_linear.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_variant.h"

/** Internal type definitions */
typedef struct
{
    system_time    start_time;
    system_time    start_time_div_by_hz;
    system_variant start_value;
    system_time    end_time;
    system_time    end_time_div_by_hz;
    system_variant end_value;
} _curve_segment_data_lerp;


/** Deinitializes a previously initialized instance of _curve_segment_data_lerp structure.
 *
 *  @param segment_data Pointer to where the instance is located. DO NOT use this pointer after
 *                      calling this function.
 **/
PRIVATE void _curve_segment_lerp_deinit(curve_segment_data segment_data)
{
    _curve_segment_data_lerp* data_ptr = reinterpret_cast<_curve_segment_data_lerp*>(segment_data);

    system_variant_release(data_ptr->start_value);
    system_variant_release(data_ptr->end_value);

    delete data_ptr;
}

/** Initializes internal curve segment data storage for a lerp segment.
 *
 *  @param segment_data *segment_data will be used to store a _curve_segment_data_lerp instance. CANNOT be null.
 *  @param data_type    Data type to use for value storage.
 **/
PRIVATE void _curve_segment_lerp_init(curve_segment_data* segment_data_ptr,
                                      system_variant_type data_type,
                                      system_time         start_time,
                                      system_variant      start_value,
                                      system_time         end_time,
                                      system_variant      end_value)
{
    _curve_segment_data_lerp* new_segment_ptr = new (std::nothrow) _curve_segment_data_lerp;

    ASSERT_ALWAYS_SYNC(new_segment_ptr != nullptr,
                       "Could not allocate lerp curve segment!");

    if (new_segment_ptr != nullptr)
    {
        new_segment_ptr->start_time           = start_time;
        new_segment_ptr->start_time_div_by_hz = start_time / HZ_PER_SEC;
        new_segment_ptr->end_time             = end_time;
        new_segment_ptr->end_time_div_by_hz   = end_time / HZ_PER_SEC;
        new_segment_ptr->start_value          = system_variant_create(data_type);
        new_segment_ptr->end_value            = system_variant_create(data_type);

        ASSERT_DEBUG_SYNC(new_segment_ptr->start_value != nullptr,
                          "Could not allocate start value variant.");
        ASSERT_DEBUG_SYNC(new_segment_ptr->end_value != nullptr,
                          "Could not allocate end value variant.");
        ASSERT_DEBUG_SYNC(data_type == SYSTEM_VARIANT_INTEGER ||
                          data_type == SYSTEM_VARIANT_FLOAT   ||
                          data_type == SYSTEM_VARIANT_BOOL,
                          "Lerp segments work only for boolean/float/integer data.");

        system_variant_set(new_segment_ptr->start_value,
                           start_value,
                           false);
        system_variant_set(new_segment_ptr->end_value,
                           end_value,
                           false);
    }

    *segment_data_ptr = reinterpret_cast<curve_segment_data>(new_segment_ptr);
}


/** Please see header for specification */
PUBLIC bool curve_segment_linear_add_node(curve_segment_data     segment_data,
                                          system_time            node_time,
                                          system_variant         node_value,
                                          curve_segment_node_id* out_node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_delete_node(curve_segment_data    segment_data,
                                             curve_segment_node_id node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_deinit(curve_segment_data segment_data)
{
    _curve_segment_lerp_deinit(segment_data);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_amount_of_nodes(curve_segment_data segment_data,
                                                     uint32_t*          out_result)
{
    *out_result = 2;

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_node(curve_segment_data    segment_data,
                                          curve_segment_node_id node_id,
                                          system_time*          out_node_time,
                                          system_variant        out_node_value)
{
    _curve_segment_data_lerp* data_ptr = reinterpret_cast<_curve_segment_data_lerp*>(segment_data);

    ASSERT_ALWAYS_SYNC(node_id >= 0 && node_id <= 1,
                       "Node id is invalid for a linear curve segment! [%d]",
                       node_id);

    if (node_id == 0)
    {
        if (out_node_time != nullptr)
        {
            *out_node_time = data_ptr->start_time;
        }

        if (out_node_value != nullptr)
        {
            system_variant_set(out_node_value,
                               data_ptr->start_value,
                               false);
        }
    }
    else
    {
        if (out_node_time != nullptr)
        {
            *out_node_time = data_ptr->end_time;
        }

        if (out_node_value != nullptr)
        {
            system_variant_set(out_node_value,
                               data_ptr->end_value,
                               false);
        }
    }

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_node_in_order(curve_segment_data     segment_data,
                                                   uint32_t               node_index,
                                                   curve_segment_node_id* out_node_id)
{
    ASSERT_DEBUG_SYNC(node_index >= 0 && node_index <= 1,
                      "Requested node index [%d] is invalid for a lerp segment",
                      node_index);

    *out_node_id = node_index;
    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_property(curve_segment_data     segment_data,
                                              curve_segment_property segment_property,
                                              system_variant         value)
{
    ASSERT_DEBUG_SYNC(false,
                      "LERP segment supports no properties.");

    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_value(curve_segment_data segment_data,
                                           system_time        time,
                                           system_variant     out_result,
                                           bool               should_force)
{
    _curve_segment_data_lerp* data_ptr = reinterpret_cast<_curve_segment_data_lerp*>(segment_data);
    float                     a;
    float                     b;
    float                     end_value;
    float                     start_value;

    ASSERT_DEBUG_SYNC(out_result != nullptr,
                      "Output result variant cannot be null.");

    system_variant_get_float(data_ptr->start_value,
                            &start_value);
    system_variant_get_float(data_ptr->end_value,
                            &end_value);

    a = (end_value - start_value) / (data_ptr->end_time - data_ptr->start_time);
    b = start_value - a * data_ptr->start_time;

    system_variant_set_float_forced(out_result, a * time + b);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_init(curve_segment_data* segment_data_ptr,
                                      system_variant_type data_type,
                                      system_time         start_time,
                                      system_variant      start_value,
                                      system_time         end_time,
                                      system_variant      end_value)
{
    _curve_segment_lerp_init(segment_data_ptr,
                             data_type,
                             start_time,
                             start_value,
                             end_time,
                             end_value);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_modify_node_time(curve_segment_data    segment_data,
                                                  curve_segment_node_id node_id,
                                                  system_time           new_node_time)
{
    _curve_segment_data_lerp* data_ptr = reinterpret_cast<_curve_segment_data_lerp*>(segment_data);

    switch (node_id)
    {
        case 0:
        {
            ASSERT_DEBUG_SYNC(data_ptr->end_time >= new_node_time,
                              "New start time would be larger than the end time for the segment!");

            if (data_ptr->end_time >= new_node_time)
            {
                data_ptr->start_time           = new_node_time;
                data_ptr->start_time_div_by_hz = new_node_time / HZ_PER_SEC;
            }

            break;
        }

        case 1:
        {
            ASSERT_DEBUG_SYNC(data_ptr->start_time < new_node_time,
                              "New end time would be set to before the segment's start time!");

            if (data_ptr->start_time < new_node_time)
            {
                data_ptr->end_time           = new_node_time;
                data_ptr->end_time_div_by_hz = new_node_time / HZ_PER_SEC;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported node id [%d] requested for LERP segment",
                              node_id);
        }
    }
    
    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_modify_node_time_value(curve_segment_data    segment_data,
                                                        curve_segment_node_id node_id,
                                                        system_time           new_node_time,
                                                        system_variant        new_node_value,
                                                        bool                  force)
{
    _curve_segment_data_lerp* data_ptr = reinterpret_cast<_curve_segment_data_lerp*>(segment_data);
    bool                      success  = false;

    switch (node_id)
    {
        case 0:
        {
            if (data_ptr->end_time >= new_node_time || force)
            {
                data_ptr->start_time           = new_node_time;
                data_ptr->start_time_div_by_hz = new_node_time / HZ_PER_SEC;

                system_variant_set(data_ptr->start_value,
                                   new_node_value,
                                   false);

                success = true;
            }

            break;
        }

        case 1:
        {
            if (data_ptr->start_time < new_node_time || force)
            {
                data_ptr->end_time           = new_node_time;
                data_ptr->end_time_div_by_hz = new_node_time / HZ_PER_SEC;

                system_variant_set(data_ptr->end_value,
                                   new_node_value,
                                   false);

                success = true;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported node id [%d] requested for LERP segment",
                              node_id);
        }
    }
    
    return success;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_set_property(curve_segment_data     segment_data,
                                              curve_segment_property segment_property,
                                              system_variant         value)
{
    ASSERT_DEBUG_SYNC(false,
                      "LERP segment supports no properties.");

    return false;
}

