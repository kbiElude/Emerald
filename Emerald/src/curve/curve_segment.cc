/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "curve/curve_segment.h"
#include "curve/curve_segment_linear.h"
#include "curve/curve_segment_static.h"
#include "curve/curve_segment_tcb.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_time.h"
#include "system/system_variant.h"

/* Private type definitions */
typedef struct
{
    system_time         modification_time;
    system_variant_type node_value_variant_type;
    curve_segment_data  segment_data;
    curve_segment_type  segment_type;

    void* callback_on_curve_changed_user_arg;

    PFNCURVESEGMENTADDNODE             pfn_add_node;
    PFNCURVESEGMENTONCURVECHANGED      pfn_callback_on_curve_changed;
    PFNCURVESEGMENTDEINIT              pfn_deinit;
    PFNCURVESEGMENTDELETENODE          pfn_delete_node;
    PFNCURVESEGMENTGETAMOUNTOFNODES    pfn_get_amount_of_nodes;
    PFNCURVESEGMENTGETNODE             pfn_get_node;
    PFNCURVESEGMENTGETNODEBYINDEX      pfn_get_node_by_index;
    PFNCURVESEGMENTGETNODEINORDER      pfn_get_node_in_order;
    PFNCURVESEGMENTGETNODEPROPERTY     pfn_get_node_property;
    PFNCURVESEGMENTGETVALUE            pfn_get_value;
    PFNCURVESEGMENTMODIFYNODEPROPERTY  pfn_modify_node_property;
    PFNCURVESEGMENTMODIFYNODETIME      pfn_modify_node_time;
    PFNCURVESEGMENTMODIFYNODETIMEVALUE pfn_modify_node_time_value;
    PFNCURVESEGMENTSETPROPERTY         pfn_set_property;
} _curve_segment;

typedef _curve_segment* _curve_segment_ptr;

/** (De-)-initializers */
/** TODO */
PRIVATE void deinit_curve_segment(__notnull __deallocate(mem) curve_segment segment)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    if (curve_segment_ptr->pfn_deinit != NULL)
    {
        curve_segment_ptr->pfn_deinit(curve_segment_ptr->segment_data);
    }

    delete curve_segment_ptr;
}

/** TODO */
PRIVATE void init_curve_segment(__deref_out __notnull curve_segment* segment)
{
    _curve_segment* new_curve_segment = new (std::nothrow) _curve_segment;

    ASSERT_ALWAYS_SYNC(new_curve_segment != NULL, "Could not allocate memory for curvesegment data!");
    if (new_curve_segment != NULL)
    {
        _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) new_curve_segment;

        curve_segment_ptr->modification_time             = system_time_now();
        curve_segment_ptr->pfn_add_node                  = NULL;
        curve_segment_ptr->pfn_callback_on_curve_changed = NULL;
        curve_segment_ptr->pfn_deinit                    = NULL;
        curve_segment_ptr->pfn_delete_node               = NULL;
        curve_segment_ptr->pfn_get_amount_of_nodes       = NULL;
        curve_segment_ptr->pfn_get_node                  = NULL;
        curve_segment_ptr->pfn_get_node_by_index         = NULL;
        curve_segment_ptr->pfn_get_node_in_order         = NULL;
        curve_segment_ptr->pfn_get_node_property         = NULL;
        curve_segment_ptr->pfn_get_value                 = NULL;
        curve_segment_ptr->pfn_modify_node_property      = NULL;
        curve_segment_ptr->pfn_modify_node_time          = NULL;
        curve_segment_ptr->pfn_modify_node_time_value    = NULL;
        curve_segment_ptr->pfn_set_property              = NULL;
        curve_segment_ptr->segment_data                  = NULL;
    }

    *segment = (curve_segment) new_curve_segment;
}

/** Please see header for specification */
PUBLIC void curve_segment_release(__in __notnull __deallocate(mem) curve_segment segment)
{
    deinit_curve_segment(segment);
}

/** Please see header for specification */
PUBLIC curve_segment curve_segment_create_static(__in __notnull system_variant value)
{
    curve_segment result_curve_segment = NULL;

    init_curve_segment(&result_curve_segment);
    
    if (result_curve_segment != NULL)
    {
        _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) result_curve_segment;

        curve_segment_ptr->node_value_variant_type     = system_variant_get_type(value);
        curve_segment_ptr->pfn_add_node                = curve_segment_static_add_node;
        curve_segment_ptr->pfn_deinit                  = curve_segment_static_deinit;
        curve_segment_ptr->pfn_delete_node             = curve_segment_static_delete_node;
        curve_segment_ptr->pfn_get_amount_of_nodes     = curve_segment_static_get_amount_of_nodes;
        curve_segment_ptr->pfn_get_node                = curve_segment_static_get_node;
        curve_segment_ptr->pfn_get_node_by_index       = curve_segment_static_get_node_in_order;
        curve_segment_ptr->pfn_get_node_in_order       = curve_segment_static_get_node_in_order;
        curve_segment_ptr->pfn_get_node_property       = NULL;
        curve_segment_ptr->pfn_get_value               = curve_segment_static_get_value;
        curve_segment_ptr->pfn_modify_node_property    = NULL;
        curve_segment_ptr->pfn_modify_node_time        = curve_segment_static_modify_node_time;
        curve_segment_ptr->pfn_modify_node_time_value  = curve_segment_static_modify_node_time_value;
        curve_segment_ptr->pfn_set_property            = curve_segment_static_set_property;
        curve_segment_ptr->segment_type                = CURVE_SEGMENT_STATIC;

        if (!curve_segment_static_init(&curve_segment_ptr->segment_data, value) )
        {
            LOG_ERROR("Could not init static segment data!");

            deinit_curve_segment(result_curve_segment);
            result_curve_segment = NULL;
        }
    }

    return result_curve_segment;
}

/** Please see header for specification */
PUBLIC curve_segment curve_segment_create_linear(               system_time    start_time,
                                                 __in __notnull system_variant start_value,
                                                                system_time    end_time,
                                                 __in __notnull system_variant end_value)
{
    curve_segment result_curve_segment = NULL;

    init_curve_segment(&result_curve_segment);

    if (result_curve_segment != NULL)
    {
        _curve_segment_ptr  curve_segment_ptr     = (_curve_segment_ptr) result_curve_segment;
        system_variant_type start_value_data_type = system_variant_get_type(start_value);
        system_variant_type end_value_data_type   = system_variant_get_type(end_value);

        ASSERT_DEBUG_SYNC(start_value_data_type == end_value_data_type, "Start and end value data types must match");

        curve_segment_ptr->node_value_variant_type     = start_value_data_type;
        curve_segment_ptr->pfn_add_node                = curve_segment_linear_add_node;
        curve_segment_ptr->pfn_deinit                  = curve_segment_linear_deinit;
        curve_segment_ptr->pfn_delete_node             = curve_segment_linear_delete_node;
        curve_segment_ptr->pfn_get_amount_of_nodes     = curve_segment_linear_get_amount_of_nodes;
        curve_segment_ptr->pfn_get_node                = curve_segment_linear_get_node;
        curve_segment_ptr->pfn_get_node_by_index       = curve_segment_linear_get_node_in_order;
        curve_segment_ptr->pfn_get_node_in_order       = curve_segment_linear_get_node_in_order;
        curve_segment_ptr->pfn_get_node_property       = NULL;
        curve_segment_ptr->pfn_get_value               = curve_segment_linear_get_value;
        curve_segment_ptr->pfn_modify_node_property    = NULL;
        curve_segment_ptr->pfn_modify_node_time        = curve_segment_linear_modify_node_time;
        curve_segment_ptr->pfn_modify_node_time_value  = curve_segment_linear_modify_node_time_value;
        curve_segment_ptr->pfn_set_property            = curve_segment_linear_set_property;
        curve_segment_ptr->segment_type                = CURVE_SEGMENT_LERP;

        if (start_value_data_type != end_value_data_type ||
            !curve_segment_linear_init(&curve_segment_ptr->segment_data,
                                        start_value_data_type,
                                        start_time,
                                        start_value,
                                        end_time,
                                        end_value) )
        {
            LOG_ERROR("Could not init LERP segment data!");

            deinit_curve_segment(result_curve_segment);
            result_curve_segment = NULL;
        }
    }

    return result_curve_segment;
}

/** Please see header for specification */
PUBLIC curve_segment curve_segment_create_tcb(__in             system_time      start_time,
                                              __in __ecount(3) float*           start_tcb,
                                              __in __notnull   system_variant   start_value,
                                              __in             system_time      end_time,
                                              __in __ecount(3) float*           end_tcb,
                                              __in __notnull   system_variant   end_value,
                                              __in __notnull   curve_container  curve,
                                              __in             curve_segment_id segment_id)
{
    curve_segment result_curve_segment = NULL;

    init_curve_segment(&result_curve_segment);
    
    if (result_curve_segment != NULL)
    {
        _curve_segment_ptr  curve_segment_ptr     = (_curve_segment_ptr) result_curve_segment;
        system_variant_type start_value_data_type = system_variant_get_type(start_value);
        system_variant_type end_value_data_type   = system_variant_get_type(end_value);

        ASSERT_DEBUG_SYNC(start_value_data_type == end_value_data_type, "Start and end value data types must match");

        curve_segment_ptr->node_value_variant_type     = start_value_data_type;
        curve_segment_ptr->pfn_add_node                = curve_segment_tcb_add_node;
        curve_segment_ptr->pfn_deinit                  = curve_segment_tcb_deinit;
        curve_segment_ptr->pfn_delete_node             = curve_segment_tcb_delete_node;
        curve_segment_ptr->pfn_get_amount_of_nodes     = curve_segment_tcb_get_amount_of_nodes;
        curve_segment_ptr->pfn_get_node                = curve_segment_tcb_get_node;
        curve_segment_ptr->pfn_get_node_by_index       = curve_segment_tcb_get_node_id_for_node_index;
        curve_segment_ptr->pfn_get_node_in_order       = curve_segment_tcb_get_node_id_for_node_in_order;
        curve_segment_ptr->pfn_get_node_property       = curve_segment_tcb_get_node_property;
        curve_segment_ptr->pfn_get_value               = curve_segment_tcb_get_value;
        curve_segment_ptr->pfn_modify_node_property    = curve_segment_tcb_modify_node_property;
        curve_segment_ptr->pfn_modify_node_time        = curve_segment_tcb_modify_node_time;
        curve_segment_ptr->pfn_modify_node_time_value  = curve_segment_tcb_modify_node_time_value;
        curve_segment_ptr->pfn_set_property            = curve_segment_tcb_set_property;
        curve_segment_ptr->segment_type                = CURVE_SEGMENT_TCB;

        if (start_value_data_type != end_value_data_type              ||
            !curve_segment_tcb_init(&curve_segment_ptr->segment_data,
                                     curve,
                                     start_time,
                                     start_tcb,
                                     start_value,
                                     end_time,
                                     end_tcb,
                                     end_value,
                                     segment_id) )
        {
            LOG_ERROR("Could not init LERP segment data!");

            deinit_curve_segment(result_curve_segment);
            result_curve_segment = NULL;
        }
    }

    return result_curve_segment;
}

/** Please see header for specification */
PUBLIC bool curve_segment_get_value(__in __notnull  curve_segment  segment,
                                    __in            system_time    time,
                                    __in            bool           should_force,
                                    __out __notnull system_variant out_result)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    return curve_segment_ptr->pfn_get_value(curve_segment_ptr->segment_data,
                                            time,
                                            out_result,
                                            should_force);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_add_node(__in __notnull  curve_segment          segment,
                                               __in            system_time            node_time,
                                               __in  __notnull system_variant         node_value,
                                               __out __notnull curve_segment_node_id* out_node_id)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;
    bool               result            = curve_segment_ptr->pfn_add_node(curve_segment_ptr->segment_data,
                                                                           node_time,
                                                                           node_value,
                                                                           out_node_id);

    if (result)
    {
        curve_segment_ptr->modification_time = system_time_now();
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_delete_node(__in __notnull curve_segment         segment,
                                                  __in           curve_segment_node_id node_id)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;
    bool               result            = curve_segment_ptr->pfn_delete_node(curve_segment_ptr->segment_data,
                                                                              node_id);

    if (result)
    {
        curve_segment_ptr->modification_time = system_time_now();
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_get_amount_of_nodes(__in  __notnull curve_segment segment,
                                                          __out __notnull uint32_t*     out_result)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    return curve_segment_ptr->pfn_get_amount_of_nodes(curve_segment_ptr->segment_data,
                                                      out_result);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_get_node(__in  __notnull   curve_segment         segment,
                                               __in              curve_segment_node_id segment_node_id,
                                               __out __maybenull system_time*          out_nodetime,
                                               __out __maybenull system_variant        out_node_value)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    return curve_segment_ptr->pfn_get_node(curve_segment_ptr->segment_data,
                                           segment_node_id,
                                           out_nodetime,
                                           out_node_value);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_get_node_property(__in    __notnull curve_segment               segment,
                                                        __in    __notnull curve_segment_node_id       node_id,
                                                        __in    __notnull curve_segment_node_property node_property,
                                                        __inout __notnull system_variant              out_result)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;
    bool               result            = false;

    if (curve_segment_ptr->pfn_get_node_property != NULL)
    {
        result = curve_segment_ptr->pfn_get_node_property(curve_segment_ptr->segment_data,
                                                          node_id,
                                                          node_property,
                                                          out_result);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_modify_node_property(__in __notnull curve_segment               segment,
                                                           __in __notnull curve_segment_node_id       node_id,
                                                           __in __notnull curve_segment_node_property node_property,
                                                           __in __notnull system_variant              value)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;
    bool               result            = false;

    if (curve_segment_ptr->pfn_modify_node_property != NULL)
    {
        result = curve_segment_ptr->pfn_modify_node_property(curve_segment_ptr->segment_data,
                                                             node_id,
                                                             node_property,
                                                             value);
    }

    if (result)
    {
        curve_segment_ptr->modification_time = system_time_now();

        if (curve_segment_ptr->pfn_callback_on_curve_changed != NULL)
        {
            curve_segment_ptr->pfn_callback_on_curve_changed(curve_segment_ptr->callback_on_curve_changed_user_arg);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_modify_node_time(__in __notnull curve_segment         segment,
                                                       __in           curve_segment_node_id segment_node_id,
                                                       __in           system_time           new_node_time)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;
    bool               result            = curve_segment_ptr->pfn_modify_node_time(curve_segment_ptr->segment_data,
                                                                                   segment_node_id,
                                                                                   new_node_time);

    if (result)
    {
        curve_segment_ptr->modification_time = system_time_now();

        if (curve_segment_ptr->pfn_callback_on_curve_changed != NULL)
        {
            curve_segment_ptr->pfn_callback_on_curve_changed(curve_segment_ptr->callback_on_curve_changed_user_arg);
        }
    }
    
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_modify_node_time_value(__in __notnull curve_segment         segment,
                                                             __in           curve_segment_node_id segment_node_id,
                                                             __in           system_time           new_node_time,
                                                             __in __notnull system_variant        new_node_value,
                                                             __in           bool                  should_force)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;
    bool               result            = curve_segment_ptr->pfn_modify_node_time_value(curve_segment_ptr->segment_data,
                                                                                         segment_node_id,
                                                                                         new_node_time,
                                                                                         new_node_value,
                                                                                         should_force);

    if (result)
    {
        curve_segment_ptr->modification_time = system_time_now();

        if (curve_segment_ptr->pfn_callback_on_curve_changed != NULL)
        {
            curve_segment_ptr->pfn_callback_on_curve_changed(curve_segment_ptr->callback_on_curve_changed_user_arg);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_time curve_segment_get_modification_time(__in __notnull curve_segment segment)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    return curve_segment_ptr->modification_time;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool curve_segment_get_node_at_time(__in  __notnull curve_segment          segment,
                                                       __in            system_time            node_time,
                                                       __out __notnull curve_segment_node_id* out_node_id)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment*) segment;
    unsigned int       n_current_node    = 0;
    bool               result            = true;

    while (true)
    {
        /* TODO: If it makes any difference, provide per-interpolation implementation of the routine */
        curve_segment_node_id current_node_id = -1;

        result = curve_segment_ptr->pfn_get_node_by_index(curve_segment_ptr->segment_data,
                                                          n_current_node,
                                                         &current_node_id);

        if (!result)
        {
            break;
        }

        /* What is the time of the node? */
        system_time current_node_time = -1;

        if (!curve_segment_ptr->pfn_get_node(curve_segment_ptr->segment_data,
                                             current_node_id,
                                            &current_node_time,
                                             NULL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "pfn_get_node() failed");

            result = false;
            break;
        }

        if (current_node_time == node_time)
        {
            *out_node_id = current_node_id;

            break;
        }

        /* Iterate */
        n_current_node++;
    } /* while (true) */

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_get_node_by_index(__in  __notnull curve_segment          segment,
                                            __in            uint32_t               node_index,
                                            __out __notnull curve_segment_node_id* out_node_id)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    return curve_segment_ptr->pfn_get_node_by_index(curve_segment_ptr->segment_data,
                                                    node_index,
                                                    out_node_id);
}

/** Please see header for specification */
PUBLIC bool curve_segment_get_node_in_order(__in  __notnull curve_segment          segment,
                                            __in            uint32_t               node_index,
                                            __out __notnull curve_segment_node_id* out_ordered_node_id)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    return curve_segment_ptr->pfn_get_node_in_order(curve_segment_ptr->segment_data,
                                                    node_index,
                                                    out_ordered_node_id);
}

/** Please see header for specification */
PUBLIC bool curve_segment_get_node_value_variant_type(__in            __notnull curve_segment        segment,
                                                      __out_ecount(1) __notnull system_variant_type* out_type)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    *out_type = curve_segment_ptr->node_value_variant_type;
    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_get_type(__in  __notnull curve_segment       segment,
                                   __out __notnull curve_segment_type* out_segment_type)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    *out_segment_type = curve_segment_ptr->segment_type;
    return true;
}

/** Please see header for specification */
PUBLIC void curve_segment_set_on_segment_changed_callback(__in __notnull   curve_segment                 segment,
                                                          __in __maybenull PFNCURVESEGMENTONCURVECHANGED callback_proc,
                                                          __in __maybenull void*                         user_arg)
{
    _curve_segment_ptr curve_segment_ptr = (_curve_segment_ptr) segment;

    curve_segment_ptr->callback_on_curve_changed_user_arg = user_arg;
    curve_segment_ptr->pfn_callback_on_curve_changed      = callback_proc;
}
