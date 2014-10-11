/**
 *
 * Emerald (kbi/elude @2012)
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

/***************************************** (De-)-initializers *********************************************************************/
/** Initializes internal curve segment data storage for a static segment.
 *
 *  @param segment_data *segment_data will be used to store a _curve_segment_data_static instance. CANNOT be null.
 *  @param data_type    Data type to use for value storage.
 **/
PRIVATE void init_curve_segment_data_static(__deref_out __notnull curve_segment_data* segment_data,
                                                                  system_variant      value)
{
    _curve_segment_data_static* new_segment_data = new (std::nothrow) _curve_segment_data_static;

    ASSERT_ALWAYS_SYNC(new_segment_data != NULL,
                       "Could not allocate static curve segment!");

    if (new_segment_data != NULL)
    {
        _curve_segment_data_static_ptr data_ptr = (_curve_segment_data_static_ptr) new_segment_data;

        data_ptr->value = system_variant_create(system_variant_get_type(value) );

        system_variant_set(data_ptr->value,
                           value,
                           false);
    }

    *segment_data = (curve_segment_data) new_segment_data;
}

/** Deinitializes a previously initialized instance of _curve_segment_data_static structure.
 *
 *  @param segment_data Pointer to where the instance is located. DO NOT use this pointer after
 *                      calling this function.
 **/
PRIVATE void deinit_curve_segment_data_static(__deref_out __post_invalid curve_segment_data segment_data)
{
    _curve_segment_data_static_ptr data_ptr = (_curve_segment_data_static_ptr) segment_data;

    system_variant_release(data_ptr->value);

    delete (_curve_segment_data_static_ptr) segment_data;
}

/******************************************************** PUBLIC FUNCTIONS *********************************************************/
/** Please see header for specification */
PUBLIC bool curve_segment_static_add_node(__in  __notnull curve_segment_data     segment_data,
                                          __in            system_timeline_time   node_time,
                                          __in  __notnull system_variant         node_value,
                                          __out __notnull curve_segment_node_id* out_node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_delete_node(__in __notnull curve_segment_data    segment_data,
                                             __in           curve_segment_node_id node_id)
{
    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_deinit(__in __notnull curve_segment_data segment_data)
{
    deinit_curve_segment_data_static(segment_data);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_amount_of_nodes(__in  __notnull curve_segment_data segment_data,
                                                     __out __notnull uint32_t*          out_result)
{
    return 1;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_node(__in __notnull    curve_segment_data    segment_data,
                                          __in              curve_segment_node_id node_id,
                                          __out __maybenull system_timeline_time* out_node_time,
                                          __out __maybenull system_variant        out_node_value)
{
    bool result = false;

    ASSERT_DEBUG_SYNC(node_id == 0,
                      "Invalid node id (%d) requested",
                      node_id);
    if (node_id == 0)
    {
        _curve_segment_data_static_ptr data_ptr = (_curve_segment_data_static_ptr) segment_data;

        if (out_node_time != NULL)
        {
            *out_node_time = 0;
        }

        if (out_node_value != NULL)
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
PUBLIC bool curve_segment_static_get_node_in_order(__in __notnull  curve_segment_data    segment_data,
                                                   __in            uint32_t               node_index,
                                                   __out __notnull curve_segment_node_id* out_node_id)
{
    bool result = false;

    ASSERT_DEBUG_SYNC(node_index >= 0 &&
                      node_index <= 1,
                      "Invalid node index (%d) requested",
                      node_index);

    if (node_index >= 0 && node_index <= 1)
    {
        *out_node_id = node_index;
        result       = true;
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_value(__in __notnull curve_segment_data segment_data,
                                                   system_timeline_time      time,
                                           __inout system_variant            out_result,
                                           __in    bool                      should_force)
{
    _curve_segment_data_static_ptr data_ptr = (_curve_segment_data_static_ptr) segment_data;

    system_variant_set(out_result,
                       data_ptr->value,
                       should_force);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_init(__inout __notnull curve_segment_data* segment_data,
                                                        system_variant      value)
{
    init_curve_segment_data_static(segment_data,
                                   value);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_modify_node_time(__in __notnull curve_segment_data    segment_data,
                                                  __in           curve_segment_node_id node_id,
                                                  __in           system_timeline_time  new_node_time)
{
    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_modify_node_time_value(__in __notnull curve_segment_data    segment_data,
                                                        __in           curve_segment_node_id node_id,
                                                        __in           system_timeline_time  new_node_time,
                                                        __in __notnull system_variant        new_node_value,
                                                        __in           bool)
{
    _curve_segment_data_static_ptr data_ptr = (_curve_segment_data_static_ptr) segment_data;

    system_variant_set(data_ptr->value,
                       new_node_value,
                       false);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_set_property(__in __notnull curve_segment_data     segment_data,
                                              __in           curve_segment_property segment_property,
                                              __in __notnull system_variant         value)
{
    /* Should not be called */
    ASSERT_DEBUG_SYNC(false,
                      "curve_segment_static_set_property() should never be called - other means exist to modify the segment's properties.");

    return false;
}

/** Please see header for specification */
PUBLIC bool curve_segment_static_get_property(__in __notnull curve_segment_data,
                                              __in           curve_segment_property,
                                              __in __notnull system_variant)
{
    /* Should not be called */
    ASSERT_DEBUG_SYNC(false, "curve_segment_static_get_property() should never be called - other means exist to query the segment's properties.");

    return false;
}
