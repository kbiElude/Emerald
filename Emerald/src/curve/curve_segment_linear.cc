/**
 *
 * Emerald (kbi/elude @2012)
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
    system_timeline_time start_time;
    system_timeline_time start_time_div_by_hz;
    system_variant       start_value;
    system_timeline_time end_time;
    system_timeline_time end_time_div_by_hz;
    system_variant       end_value;
} _curve_segment_data_lerp;

typedef _curve_segment_data_lerp* _curve_segment_data_lerp_ptr;

/***************************************** (De-)-initializers *********************************************************************/
/** Initializes internal curve segment data storage for a lerp segment. 
 *
 *  @param segment_data *segment_data will be used to store a _curve_segment_data_lerp instance. CANNOT be null.
 *  @param data_type    Data type to use for value storage.
 **/
PRIVATE void init_curve_segment_data_lerp(__deref_out __notnull curve_segment_data*  segment_data,
                                                                system_variant_type  data_type,
                                                                system_timeline_time start_time,
                                                                system_variant       start_value,
                                                                system_timeline_time end_time,
                                                                system_variant       end_value)
{
    _curve_segment_data_lerp* new_segment = new (std::nothrow) _curve_segment_data_lerp;

    ASSERT_ALWAYS_SYNC(new_segment != NULL, "Could not allocate lerp curve segment!");
    if (new_segment != NULL)
    {
        _curve_segment_data_lerp_ptr data_ptr = (_curve_segment_data_lerp_ptr) new_segment;

        data_ptr->start_time           = start_time;
        data_ptr->start_time_div_by_hz = start_time / HZ_PER_SEC;
        data_ptr->end_time             = end_time;
        data_ptr->end_time_div_by_hz   = end_time / HZ_PER_SEC;
        data_ptr->start_value          = system_variant_create(data_type);
        data_ptr->end_value            = system_variant_create(data_type);

        ASSERT_DEBUG_SYNC(data_ptr->start_value != NULL,
                          "Could not allocate start value variant.");
        ASSERT_DEBUG_SYNC(data_ptr->end_value != NULL,
                          "Could not allocate end value variant.");
        ASSERT_DEBUG_SYNC(data_type == SYSTEM_VARIANT_INTEGER ||
                          data_type == SYSTEM_VARIANT_FLOAT   ||
                          data_type == SYSTEM_VARIANT_BOOL,
                          "Lerp segments work only for boolean/float/integer data.");

        system_variant_set(data_ptr->start_value,
                           start_value,
                           false);
        system_variant_set(data_ptr->end_value,
                           end_value,
                           false);
    }

    *segment_data = (curve_segment_data) new_segment;
}

/** Deinitializes a previously initialized instance of _curve_segment_data_lerp structure.
 *
 *  @param segment_data Pointer to where the instance is located. DO NOT use this pointer after
 *                      calling this function.
 **/
PRIVATE void deinit_curve_segment_data_lerp(__deref_out __post_invalid curve_segment_data segment_data)
{
    _curve_segment_data_lerp_ptr data_ptr = (_curve_segment_data_lerp_ptr) segment_data;

    system_variant_release(data_ptr->start_value);
    system_variant_release(data_ptr->end_value);

    delete (_curve_segment_data_lerp_ptr) segment_data;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_add_node(__in  __notnull curve_segment_data     segment_data,
                                          __in            system_timeline_time   node_time,
                                          __in  __notnull system_variant         node_value,
                                          __out __notnull curve_segment_node_id* out_node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_delete_node(__in __notnull curve_segment_data    segment_data,
                                             __in           curve_segment_node_id node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_deinit(__in __notnull curve_segment_data segment_data)
{
    deinit_curve_segment_data_lerp(segment_data);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_amount_of_nodes(__in  __notnull curve_segment_data segment_data,
                                                     __out __notnull uint32_t*          out_result)
{
    *out_result = 2;

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_node(__in  __notnull  curve_segment_data     segment_data,
                                          __in              curve_segment_node_id node_id,
                                          __out __maybenull system_timeline_time* out_node_time,
                                          __out __maybenull system_variant        out_node_value)
{
    _curve_segment_data_lerp_ptr data_ptr = (_curve_segment_data_lerp_ptr) segment_data;

    ASSERT_ALWAYS_SYNC(node_id >= 0 && node_id <= 1, "Node id is invalid for a linear curve segment! [%d]", node_id);
    if (node_id == 0)
    {
        if (out_node_time != NULL)
        {
            *out_node_time = data_ptr->start_time;
        }

        if (out_node_value != NULL)
        {
            system_variant_set(out_node_value,
                               data_ptr->start_value,
                               false);
        }
    }
    else
    {
        if (out_node_time != NULL)
        {
            *out_node_time = data_ptr->end_time;
        }

        if (out_node_value != NULL)
        {
            system_variant_set(out_node_value,
                               data_ptr->end_value,
                               false);
        }
    }

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_node_in_order(__in __notnull  curve_segment_data     segment_data,
                                                   __in            uint32_t               node_index,
                                                   __out __notnull curve_segment_node_id* out_node_id)
{
    ASSERT_DEBUG_SYNC(node_index >= 0 && node_index <= 1,
                      "Requested node index [%d] is invalid for a lerp segment",
                      node_index);

    *out_node_id = node_index;
    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_value(__in    __notnull curve_segment_data   segment_data,
                                                             system_timeline_time time,
                                           __inout __notnull system_variant       out_result,
                                           __in              bool                 should_force)
{
    _curve_segment_data_lerp_ptr data_ptr = (_curve_segment_data_lerp_ptr) segment_data;
    float                        a;
    float                        b;
    float                        end_value;
    float                        start_value;

    ASSERT_DEBUG_SYNC(out_result != NULL, "Output result variant cannot be null.");

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
PUBLIC bool curve_segment_linear_init(__deref_out __notnull curve_segment_data*  segment_data,
                                                            system_variant_type  data_type,
                                                            system_timeline_time start_time,
                                                            system_variant       start_value,
                                                            system_timeline_time end_time,
                                                            system_variant       end_value)
{
    init_curve_segment_data_lerp(segment_data,
                                 data_type,
                                 start_time,
                                 start_value,
                                 end_time,
                                 end_value);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_modify_node_time(__in __notnull curve_segment_data    segment_data,
                                                  __in           curve_segment_node_id node_id,
                                                  __in           system_timeline_time  new_node_time)
{
    _curve_segment_data_lerp_ptr data_ptr = (_curve_segment_data_lerp_ptr) segment_data;

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
PUBLIC bool curve_segment_linear_modify_node_time_value(__in __notnull curve_segment_data    segment_data,
                                                        __in           curve_segment_node_id node_id,
                                                        __in           system_timeline_time  new_node_time,
                                                        __in __notnull system_variant        new_node_value,
                                                        __in           bool                  force)
{
    _curve_segment_data_lerp_ptr data_ptr = (_curve_segment_data_lerp_ptr) segment_data;
    bool                         success  = false;

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
PUBLIC bool curve_segment_linear_set_property(__in __notnull curve_segment_data     segment_data,
                                              __in           curve_segment_property segment_property,
                                              __in __notnull system_variant         value)
{
    ASSERT_DEBUG_SYNC(false,
                      "LERP segment supports no properties.");

    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_linear_get_property(__in __notnull curve_segment_data,
                                              __in           curve_segment_property,
                                              __in __notnull system_variant)
{
    ASSERT_DEBUG_SYNC(false,
                      "LERP segment supports no properties.");

    return false;
}
