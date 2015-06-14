/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "curve/curve_segment.h"
#include "curve/curve_segment_tcb.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_variant.h"

#define START_NODES_AMOUNT (4)


/** Internal type definitions */
typedef struct
{
    system_timeline_time time;
    float                value;
    float                tension;
    float                continuity;
    float                bias;
} _curve_segment_data_tcb_node;

typedef struct
{
    curve_container         curve;
    curve_segment_id        id;
    system_resizable_vector nodes;
    system_resizable_vector nodes_order;
} _curve_segment_data_tcb;


/** TODO */
double _get_curve_time_for_time(__in __notnull  _curve_segment_data_tcb* segment_data_ptr,
                                __in            system_timeline_time     time,
                                __out __notnull curve_segment_node_id*   out_node_id,
                                __out __notnull curve_segment_node_id*   out_next_node_id)
{
    uint32_t                      n_nodes_order_elements                = 0;
    curve_segment_node_id         last_nodes_order_id                   = 0;
    curve_segment_node_id         first_nodes_order_id;
    _curve_segment_data_tcb_node* node_at_last_node_in_nodes_order_ptr  = NULL;
    _curve_segment_data_tcb_node* node_at_first_node_in_nodes_order_ptr = NULL;

    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes_order_elements);

    system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                           n_nodes_order_elements-1,
                                          &last_nodes_order_id);
    system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                           0,
                                          &first_nodes_order_id);

    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           last_nodes_order_id,
                                          &node_at_last_node_in_nodes_order_ptr);
    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           first_nodes_order_id,
                                          &node_at_first_node_in_nodes_order_ptr);

    /* Calculate total known time */
    system_timeline_time curve_total_time = node_at_last_node_in_nodes_order_ptr->time - node_at_first_node_in_nodes_order_ptr->time;
    uint32_t             n_node           = 0;

    /* Find the interval requested time belongs to */
    uint32_t             n_node_iterator      = 0;
    uint32_t             n_next_node_iterator = 1;
    bool                 is_interval_found    = false;
    system_timeline_time node_start_time      = 0;
    system_timeline_time next_node_start_time = 0;
    double               result_time          = 0;

    while (n_next_node_iterator != n_nodes_order_elements)
    {
        curve_segment_node_id         next_node_id  = 0;
        _curve_segment_data_tcb_node* next_node_ptr = NULL;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_next_node_iterator,
                                              &next_node_id);

        system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                               next_node_id,
                                              &next_node_ptr);

        next_node_start_time = next_node_ptr->time;

        if (time <= next_node_start_time)
        {
            curve_segment_node_id         node_id  = 0;
            _curve_segment_data_tcb_node* node_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_node_iterator,
                                                  &node_id);

            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            ASSERT_DEBUG_SYNC(time >= node_ptr->time,
                              "");

            node_start_time      = node_ptr->time;
            next_node_start_time = next_node_ptr->time;

            is_interval_found = true;
            break;
        } /* if (time <= next_node_start_time) */

        n_node_iterator      ++;
        n_next_node_iterator ++;
        n_node               ++;
    } /* while (n_next_node_iterator != n_nodes_order_elements) */

    if (!is_interval_found)
    {
        curve_segment_node_id last_node_id = 0;
        curve_segment_node_id end_node_id  = 0;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_node_iterator,
                                              &last_node_id);
        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_nodes_order_elements - 1,
                                              &end_node_id);

        /* Round-up errors. Can also hapen if time is at least 1.0 */
        result_time = 1.0f;

        if (last_node_id == end_node_id)
        {
            /* Move one node backwards, since we're dealing with last nodes here */
            n_next_node_iterator = n_node;
            n_node_iterator      = n_nodes_order_elements - 2;
        } /* if (last_node_id == end_node_id) */
    } /* if (!is_interval_found) */
    else
    {
        if (next_node_start_time == node_start_time)
        {
            /* ? */
            result_time = 0.0f;
        }
        else
        {
            unsigned int n_segment_nodes = 0;

            system_resizable_vector_get_property(segment_data_ptr->nodes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_segment_nodes);

            /* node_iterator contains the start segment, get the normalized time */
            curve_segment_node_id         node_id       = 0;
            curve_segment_node_id         next_node_id  = 0;
            _curve_segment_data_tcb_node* node_ptr      = NULL;
            _curve_segment_data_tcb_node* next_node_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_node_iterator,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_next_node_iterator,
                                                  &next_node_id);

            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   next_node_id,
                                                  &next_node_ptr);

            double interval_length   = double(next_node_ptr->time - node_ptr->time)  / 1000.0f;
            double time_for_interval =       (time                - node_start_time) / (next_node_start_time - node_start_time); // e <0,1>

            result_time = double(n_node + time_for_interval) / double(n_segment_nodes - 1);
        }
    }

    if (out_node_id != NULL)
    {
        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_node_iterator,
                                               out_node_id);
    }

    if (out_next_node_id != NULL)
    {
        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_next_node_iterator,
                                               out_next_node_id);
    }

    ASSERT_DEBUG_SYNC(result_time >= 0.0f &&
                      result_time <= 1.0f,
                      "");

    return result_time;
}

/** TODO */
PRIVATE bool _get_nodes_in_order(__in __notnull  _curve_segment_data_tcb* segment_data_ptr,
                                 __in            uint32_t                 n_node,
                                 __out __notnull curve_segment_node_id*   out_node_id)
{
    unsigned int n_nodes_order = 0;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes_order);

    if (n_nodes_order < n_node)
    {
        ASSERT_DEBUG_SYNC(false, "");

        return false;
    }

    return system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                  n_node,
                                                  out_node_id);
}

/** TODO */
PRIVATE void _init_curve_segment_data_tcb(                 curve_segment_data*  segment_data,
                                          __in __notnull   curve_container      curve,
                                          __in             system_timeline_time start_time,
                                          __in __ecount(3) float*               start_tcb,
                                          __in __notnull   system_variant       start_value,
                                          __in             system_timeline_time end_time,
                                          __in __ecount(3) float*               end_tcb,
                                          __in __notnull   system_variant       end_value,
                                          __in             curve_segment_id     segment_id)
{
    _curve_segment_data_tcb* new_segment = new (std::nothrow) _curve_segment_data_tcb;

    ASSERT_ALWAYS_SYNC(new_segment != NULL,
                       "Could not allocate tcb curve segment!");
    if (new_segment != NULL)
    {
        _curve_segment_data_tcb* data_ptr = (_curve_segment_data_tcb*) new_segment;

        data_ptr->curve           = curve;
        data_ptr->id              = segment_id;
        data_ptr->nodes           = system_resizable_vector_create(START_NODES_AMOUNT);
        data_ptr->nodes_order     = system_resizable_vector_create(START_NODES_AMOUNT);

        ASSERT_DEBUG_SYNC(data_ptr->nodes != NULL,
                          "Could not create nodes resizable vector");
        ASSERT_DEBUG_SYNC(data_ptr->nodes != NULL,
                          "Could not create nodes order resizable vector.");

        /* Add start & end nodes. */
        curve_segment_node_id start_node_id = 0;
        curve_segment_node_id end_node_id   = 0;

        if (!curve_segment_tcb_add_node( (curve_segment_data) new_segment,
                                         start_time,
                                         start_value,
                                        &start_node_id) )
        {
            LOG_FATAL("Could not create start node for a TCB segment");

            ASSERT_DEBUG_SYNC(false,
                              "");
        }

        if (!curve_segment_tcb_add_node( (curve_segment_data) new_segment,
                                         end_time,
                                         end_value,
                                        &end_node_id) )
        {
            LOG_FATAL("Could not create end node for a TCB segment");

            ASSERT_DEBUG_SYNC(false,
                              "");
        }

        /* Set TCB for the nodes */
        system_variant float_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);
        {
            system_variant_set_float      (float_variant,
                                           start_tcb[0]);
            curve_segment_tcb_set_property((curve_segment_data) new_segment,
                                           CURVE_SEGMENT_PROPERTY_TCB_START_TENSION,
                                           float_variant);
            system_variant_set_float      (float_variant,
                                           start_tcb[1]);
            curve_segment_tcb_set_property((curve_segment_data) new_segment,
                                           CURVE_SEGMENT_PROPERTY_TCB_START_CONTINUITY,
                                           float_variant);
            system_variant_set_float      (float_variant,
                                           start_tcb[2]);
            curve_segment_tcb_set_property((curve_segment_data) new_segment,
                                           CURVE_SEGMENT_PROPERTY_TCB_START_BIAS,
                                           float_variant);

            system_variant_set_float      (float_variant,
                                           end_tcb[0]);
            curve_segment_tcb_set_property((curve_segment_data) new_segment,
                                           CURVE_SEGMENT_PROPERTY_TCB_END_TENSION,
                                           float_variant);
            system_variant_set_float      (float_variant,
                                           end_tcb[1]);
            curve_segment_tcb_set_property((curve_segment_data) new_segment,
                                           CURVE_SEGMENT_PROPERTY_TCB_END_CONTINUITY,
                                           float_variant);
            system_variant_set_float      (float_variant,
                                           end_tcb[2]);
            curve_segment_tcb_set_property((curve_segment_data) new_segment,
                                           CURVE_SEGMENT_PROPERTY_TCB_END_BIAS,
                                           float_variant);
        }
        system_variant_release(float_variant);
    }

    *segment_data = (curve_segment_data) new_segment;
}

/** TODO */
PRIVATE void _init_curve_segment_data_tcb_node(__in __notnull _curve_segment_data_tcb_node* ptr,
                                                              system_timeline_time          time,
                                                              float                         value)
{
    ptr->time       = time;
    ptr->value      = value;
    ptr->tension    = 0;
    ptr->continuity = 0;
    ptr->bias       = 0;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_add_node(__in  __notnull curve_segment_data     segment_data,
                                       __in            system_timeline_time   node_time,
                                       __in  __notnull system_variant         node_value,
                                       __out __notnull curve_segment_node_id* out_node_id)
{
    _curve_segment_data_tcb* segment_data_ptr      = (_curve_segment_data_tcb*) segment_data;
    uint32_t                 n_node_elements       = 0;
    uint32_t                 n_node_order_elements = 0;
    bool                     result                = false;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_node_elements);
    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_node_order_elements);

    #ifdef _DEBUG
    {
        /* Verify the nodes really are in order. */
        uint32_t             n_order_node_ids = n_node_order_elements;
        system_timeline_time previous_time    = -1;

        for (uint32_t n_order_node_id = 0;
                      n_order_node_id < n_order_node_ids;
                    ++n_order_node_id)
        {
            curve_segment_node_id         current_node_id = -1;
            _curve_segment_data_tcb_node* current_node_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_order_node_id,
                                                  &current_node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   current_node_id,
                                                  &current_node_ptr);

            if (previous_time != -1)
            {
                ASSERT_DEBUG_SYNC(current_node_ptr->time > previous_time,
                                  "Nodes are NOT sorted in order");
            }

            previous_time = current_node_ptr->time;
        } /* for (all order node ids) */
    }
    #endif

    /* Make sure no other node exists with given time */
    for (uint32_t n_node = 0;
                  n_node < n_node_elements;
                ++n_node)
    {
        _curve_segment_data_tcb_node* node_ptr = NULL;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                               n_node,
                                              &node_ptr);

        if (node_ptr->time == node_time)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Requested node is already defined");

            return false;
        }
    }

    /* Find suitable place for the node */
    uint32_t n_node_order_to_place_at = -1;

    for (n_node_order_to_place_at = 0;
         n_node_order_to_place_at < n_node_order_elements;
       ++n_node_order_to_place_at)
    {
        curve_segment_node_id         node_order_id       = 0;
        _curve_segment_data_tcb_node* node_descriptor_ptr = 0;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_node_order_to_place_at,
                                              &node_order_id);
        system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                               node_order_id,
                                              &node_descriptor_ptr);

        if (node_time < node_descriptor_ptr->time)
        {
            break;
        }
    }

    /* Add the node */
    uint32_t                      node_id        = n_node_elements;
    float                         new_node_value = 0;
    _curve_segment_data_tcb_node* new_node_ptr   = new (std::nothrow) _curve_segment_data_tcb_node;

    ASSERT_DEBUG_SYNC(new_node_ptr != NULL,
                      "Could not allocate space for new TCB node structure.");

    if (new_node_ptr != NULL)
    {
        system_variant_get_float         (node_value,
                                         &new_node_value);
        _init_curve_segment_data_tcb_node(new_node_ptr,
                                          node_time,
                                          new_node_value);

        /* POTENTIAL CULPRIT: WE DO NOT MODIFY TCB AT THIS POINT AS OPPOSED TO ORIGINAL IMPL */

        system_resizable_vector_push(segment_data_ptr->nodes,
                                     new_node_ptr);

        *out_node_id = node_id;

        /* Insert nodes order entry */
        system_resizable_vector_insert_element_at(segment_data_ptr->nodes_order,
                                                  n_node_order_to_place_at,
                                                  (void*) node_id);

        #ifdef _DEBUG
        {
            /* Verify the nodes really are in order. */
            uint32_t             n_order_node_ids = 0;
            system_timeline_time previous_time    = -1;

            system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_order_node_ids);

            for (uint32_t n_order_node_id = 0;
                          n_order_node_id < n_order_node_ids;
                        ++n_order_node_id)
            {
                curve_segment_node_id         current_node_id = -1;
                _curve_segment_data_tcb_node* current_node_ptr = NULL;

                system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                       n_order_node_id,
                                                      &current_node_id);
                system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                       current_node_id,
                                                      &current_node_ptr);

                if (previous_time != -1)
                {
                    ASSERT_DEBUG_SYNC(current_node_ptr->time > previous_time,
                                      "Nodes are NOT sorted in order");
                }

                previous_time = current_node_ptr->time;
            } /* for (all order node ids) */
        }
        #endif

        /* Update segment if the start or end time are about to change */
        if (n_node_order_elements > 1)
        {
            ASSERT_DEBUG_SYNC(segment_data_ptr->curve != NULL,
                              "Curve is NULL.");

            if (n_node_order_to_place_at == 0)
            {
                curve_container_set_segment_property(segment_data_ptr->curve,
                                                     segment_data_ptr->id,
                                                     CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                     &node_time);
            }
            else
            if (n_node_order_to_place_at == n_node_order_elements)
            {
                curve_container_set_segment_property(segment_data_ptr->curve,
                                                     segment_data_ptr->id,
                                                     CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                     &node_time);
            }
        } /* if (n_node_order_elements > 1) */

        result = true;
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_delete_node(__in __notnull curve_segment_data    segment_data,
                                          __in           curve_segment_node_id node_id)
{
    _curve_segment_data_tcb*      segment_data_ptr      = (_curve_segment_data_tcb*) segment_data;
    uint32_t                      n_node_elements       = 0;
    uint32_t                      n_node_order_elements = 0;
    _curve_segment_data_tcb_node* node_ptr              = NULL;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_node_elements);
    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_node_order_elements);

    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);

    if (n_node_elements <= 2)
    {
        return false;
    }

    if (node_ptr != NULL)
    {
        curve_segment_node_id node_id_at_nodes_order_0    = NULL;
        curve_segment_node_id node_id_at_last_nodes_order = NULL;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               0,
                                              &node_id_at_nodes_order_0);
        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_node_order_elements - 1,
                                              &node_id_at_last_nodes_order);

        if (node_id_at_nodes_order_0 == node_id)
        {
            curve_segment_node_id         node_id_at_nodes_order_1  = NULL;
            _curve_segment_data_tcb_node* node_at_nodes_order_1_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   1,
                                                  &node_id_at_nodes_order_1);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id_at_nodes_order_1,
                                                  &node_at_nodes_order_1_ptr);

            curve_container_set_segment_property(segment_data_ptr->curve,
                                                 segment_data_ptr->id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                &node_at_nodes_order_1_ptr->time);
        }
        else
        if (node_id_at_last_nodes_order == node_id)
        {
            curve_segment_node_id         node_id_at_last_before_last_nodes_order  = NULL;
            _curve_segment_data_tcb_node* node_at_last_before_last_nodes_order_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_node_order_elements - 2,
                                                  &node_id_at_last_before_last_nodes_order);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id_at_last_before_last_nodes_order,
                                                  &node_at_last_before_last_nodes_order_ptr);

            curve_container_set_segment_property(segment_data_ptr->curve,
                                                 segment_data_ptr->id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                &node_at_last_before_last_nodes_order_ptr->time);
        }

        delete node_ptr;
        node_ptr = NULL;

        system_resizable_vector_delete_element_at(segment_data_ptr->nodes,
                                                  node_id);

        /* Update nodes order */
        for (uint32_t n_order_node = 0;
                      n_order_node < n_node_order_elements;
                    ++n_order_node)
        {
            curve_segment_node_id node_id_at_order_node = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_order_node,
                                                  &node_id_at_order_node);

            if (node_id_at_order_node == node_id)
            {
                system_resizable_vector_delete_element_at(segment_data_ptr->nodes_order,
                                                          n_order_node);

                break;
            }
        }

        /* Finally, update all ids so that they correspond to updated vector indexes */
        for (uint32_t n_order_node = 0;
                      n_order_node < n_node_order_elements - 1;
                    ++n_order_node)
        {
            curve_segment_node_id iterated_node_id = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_order_node,
                                                  &iterated_node_id);

            if (iterated_node_id >= node_id)
            {
                iterated_node_id --;

                system_resizable_vector_set_element_at(segment_data_ptr->nodes_order,
                                                       n_order_node,
                                                       (void*)iterated_node_id);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_deinit(__in __notnull __post_invalid curve_segment_data segment_data)
{
    unsigned int             n_nodes          = 0;
    _curve_segment_data_tcb* segment_data_ptr = (_curve_segment_data_tcb*) segment_data;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    while (n_nodes > 0)
    {
        _curve_segment_data_tcb_node* node_ptr = NULL;

        if (system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   0,
                                                  &node_ptr) )
        {
            delete node_ptr;

            system_resizable_vector_delete_element_at(segment_data_ptr->nodes,
                                                      0);
        }
        else
        {
            break;
        }
    }

    system_resizable_vector_release(segment_data_ptr->nodes);
    system_resizable_vector_release(segment_data_ptr->nodes_order);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_amount_of_nodes(__in  __notnull curve_segment_data segment_data,
                                                  __out __notnull uint32_t*          out_result)
{
    _curve_segment_data_tcb* segment_data_ptr = (_curve_segment_data_tcb*) segment_data;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                         out_result);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_node(__in __notnull    curve_segment_data segment_data,
                                       __in              curve_segment_node_id      node_id,
                                       __out __maybenull system_timeline_time*      out_node_time,
                                       __out __maybenull system_variant             out_node_value)
{
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;
    uint32_t                      n_node_elements  = 0;
    _curve_segment_data_tcb_node* node_ptr         = NULL;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_node_elements);

    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);

    if (node_ptr != NULL)
    {
        if (out_node_time != NULL)
        {
            *out_node_time = node_ptr->time;
        }

        if (out_node_value != NULL)
        {
            system_variant_set_float_forced(out_node_value,
                                            node_ptr->value);
        }

        return true;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Requested node was not found.");

        return false;
    }
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_node_id_for_node_index(__in __notnull  curve_segment_data     segment_data,
                                                         __in            uint32_t               node_index,
                                                         __out __notnull curve_segment_node_id* out_node_id)
{
    unsigned int             n_nodes          = 0;
    _curve_segment_data_tcb* segment_data_ptr = (_curve_segment_data_tcb*) segment_data;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    if (n_nodes < node_index)
    {
        ASSERT_DEBUG_SYNC(false, "Invalid node index requested.");

        return false;
    }

    *out_node_id = node_index;

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_node_id_for_node_in_order(__in  __notnull curve_segment_data     segment_data,
                                                            __in            uint32_t               node_index,
                                                            __out __notnull curve_segment_node_id* out_node_id)
{
    unsigned int             n_nodes_order    = 0;
    _curve_segment_data_tcb* segment_data_ptr = (_curve_segment_data_tcb*) segment_data;

    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes_order);

    if (n_nodes_order < node_index)
    {
        ASSERT_DEBUG_SYNC(false, "Invalid node index requested.");

        return false;
    }

    system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                           node_index,
                                           out_node_id);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_node_property(__in    __notnull curve_segment_data          segment_data,
                                                __in              curve_segment_node_id       node_id,
                                                __in              curve_segment_node_property node_property,
                                                __inout __notnull system_variant              value)
{
    bool                          result           = true;
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;
    _curve_segment_data_tcb_node* node_ptr         = NULL;

    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);

    ASSERT_DEBUG_SYNC(node_ptr != NULL,
                      "Could not retrieve node with id [%d]",
                      node_id);

    if (node_ptr != NULL)
    {
        switch(node_property)
        {
            case CURVE_SEGMENT_NODE_PROPERTY_BIAS:
            {
                system_variant_set_float(value,
                                         node_ptr->bias);

                break;
            }

            case CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY:
            {
                system_variant_set_float(value,
                                         node_ptr->continuity);

                break;
            }

            case CURVE_SEGMENT_NODE_PROPERTY_TENSION:
            {
                system_variant_set_float(value,
                                         node_ptr->tension);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Node property [%d] is unsupported",
                                  node_property);

                result = false;
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_property(__in __notnull curve_segment_data     segment_data,
                                           __in           curve_segment_property segment_property,
                                           __in __notnull system_variant         out_variant)
{
    curve_segment_node_id         node_id          = 0;
    _curve_segment_data_tcb_node* node_ptr         = NULL;
    bool                          result           = true;
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;
    uint32_t                      n_nodes          = 0;

    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    switch (segment_property)
    {
        case CURVE_SEGMENT_PROPERTY_TCB_START_BIAS:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   0,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_set_float(out_variant,
                                     node_ptr->bias);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_START_CONTINUITY:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   0,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_set_float(out_variant,
                                     node_ptr->continuity);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_START_TENSION:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   0,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_set_float(out_variant,
                                     node_ptr->tension);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_END_BIAS:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_nodes - 1,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_set_float(out_variant,
                                     node_ptr->bias);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_END_CONTINUITY:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_nodes - 1,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_set_float(out_variant,
                                     node_ptr->continuity);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_END_TENSION:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_nodes - 1,
                                                   &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_set_float(out_variant,
                                     node_ptr->tension);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized TCB segment property requested [%d]",
                              segment_property);

            result = false;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_get_value(__in    __notnull curve_segment_data   segment_data,
                                                          system_timeline_time time,
                                        __inout           system_variant       out_result,
                                        __in              bool                 should_force)
{
    _curve_segment_data_tcb* segment_data_ptr = (_curve_segment_data_tcb*) segment_data;

    /* Sanity check */
    unsigned int n_nodes                = 0;
    unsigned int n_nodes_order_elements = 0;

    system_resizable_vector_get_property(segment_data_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);
    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes_order_elements);

    ASSERT_DEBUG_SYNC(n_nodes >= 2,
                      "Segment is malformed.");

    /* Let's go */
    curve_segment_node_id         previous_node_id              = 0;
    curve_segment_node_id         node_id                       = 0;
    curve_segment_node_id         next_node_id                  = 0;
    curve_segment_node_id         next_next_node_id             = 0;
    _curve_segment_data_tcb_node* prev_node_ptr                 = NULL;
    uint32_t                      prev_node_order_iterator      = n_nodes_order_elements;
    _curve_segment_data_tcb_node* node_ptr                      = NULL;
    uint32_t                      node_order_iterator           = n_nodes_order_elements;
    _curve_segment_data_tcb_node* next_node_ptr                 = NULL;
    uint32_t                      next_node_order_iterator      = n_nodes_order_elements;
    _curve_segment_data_tcb_node* next_next_node_ptr            = NULL;
    uint32_t                      next_next_node_order_iterator = n_nodes_order_elements;
    float                         in;
    float                         out;

    double                        curve_time = _get_curve_time_for_time(segment_data_ptr,
                                                                        time,
                                                                       &node_id,
                                                                       &next_node_id);

    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);
    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           next_node_id,
                                          &next_node_ptr);

    for (node_order_iterator = 0;
         node_order_iterator < n_nodes_order_elements;
       ++node_order_iterator)
    {
        curve_segment_node_id node_order_id = 0;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               node_order_iterator,
                                              &node_order_id);

        if (node_order_id == node_id)
        {
            if (node_order_iterator != 0)
            {
                prev_node_order_iterator = node_order_iterator - 1;
            }

            if (node_order_iterator + 1 != n_nodes_order_elements)
            {
                next_node_order_iterator = node_order_iterator + 1;

                if (next_node_order_iterator + 1 != n_nodes_order_elements)
                {
                    next_next_node_order_iterator = next_node_order_iterator + 1;
                } /* if (next_node_order_iterator + 1 != n_nodes_order_elements) */
            } /* if (node_order_iterator + 1 != n_nodes_order_elements) */

            break;
        } /* if (node_order_id == node_id) */
    } /* for (node_order_iterator = 0; node_order_iterator < n_nodes_order_elements; ++node_order_iterator) */

    /* Outgoing tangent */
    float  a = (1.0f - node_ptr->tension) * (1.0f + node_ptr->continuity) * (1.0f + node_ptr->bias);
    float  b = (1.0f - node_ptr->tension) * (1.0f - node_ptr->continuity) * (1.0f - node_ptr->bias);
    float  d = next_node_ptr->value - node_ptr->value;
    double t;

    if (prev_node_order_iterator != n_nodes_order_elements)
    {
        curve_segment_node_id prev_node_order_id = 0;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               prev_node_order_iterator,
                                              &prev_node_order_id);

        system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                               prev_node_order_id,
                                              &prev_node_ptr);

        t   = float(next_node_ptr->time - node_ptr->time) / float(next_node_ptr->time - prev_node_ptr->time);
        out = float(t * (a * (node_ptr->value - prev_node_ptr->value) + b * d));
    }
    else
    {
        out = b * d;
    }

    /* Incoming tangent */
    a = (1.0f - next_node_ptr->tension) * (1.0f - next_node_ptr->continuity) * (1.0f + next_node_ptr->bias);
    b = (1.0f - next_node_ptr->tension) * (1.0f + next_node_ptr->continuity) * (1.0f - next_node_ptr->bias);
    d = next_node_ptr->value - node_ptr->value;

    if (next_next_node_order_iterator != n_nodes_order_elements)
    {
        curve_segment_node_id next_next_node_order_id = 0;

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               next_next_node_order_iterator,
                                              &next_next_node_order_id);

        system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                               next_next_node_order_id,
                                              &next_next_node_ptr);

        t  = float(next_node_ptr->time - node_ptr->time) / float(next_next_node_ptr->time - node_ptr->time);
        in = float(t * (b * (next_next_node_ptr->value - next_node_ptr->value) + a * d));
    }
    else
    {
        in = a * d;
    }

    /* Calculate Hermite coefficients */
    double curve_time_double = double(time - node_ptr->time) / double(next_node_ptr->time - node_ptr->time);
    double t2                = curve_time_double * curve_time_double;
    double t3                = curve_time_double * t2;
    double h2                = 3.0f * t2 - t3 - t3;
    double h1                = 1.0f - h2;
    double h4                = t3 - t2;
    double h3                = h4 - t2 + curve_time_double;

    system_variant_set_float_forced(out_result,
                                    float(h1 * node_ptr->value + h2 * next_node_ptr->value + h3 * out + h4 * in) );

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_init(__inout __notnull curve_segment_data*  segment_data,
                                   __in    __notnull curve_container      curve,
                                   __in              system_timeline_time start_time,
                                   __in __ecount(3)  float*               start_tcb,
                                   __in __notnull    system_variant       start_value,
                                   __in              system_timeline_time end_time,
                                   __in __ecount(3)  float*               end_tcb,
                                   __in __notnull    system_variant       end_value,
                                   __in              curve_segment_id     segment_id)
{
    _init_curve_segment_data_tcb(segment_data,
                                 curve,
                                 start_time,
                                 start_tcb,
                                 start_value,
                                 end_time,
                                 end_tcb,
                                 end_value,
                                 segment_id);

    return true;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_modify_node_property(__in __notnull curve_segment_data          segment_data,
                                                   __in           curve_segment_node_id       node_id,
                                                   __in           curve_segment_node_property node_property,
                                                   __in __notnull system_variant              value)
{
    bool                          result           = true;
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;
    _curve_segment_data_tcb_node* node_ptr         = NULL;

    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);

    ASSERT_DEBUG_SYNC(node_ptr != NULL,
                      "Could not retrieve node with id [%d]",
                      node_id);

    if (node_ptr != NULL)
    {
        switch(node_property)
        {
            case CURVE_SEGMENT_NODE_PROPERTY_BIAS:
            {
                system_variant_get_float(value,
                                        &node_ptr->bias);

                break;
            }

            case CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY:
            {
                system_variant_get_float(value,
                                        &node_ptr->continuity);

                break;
            }

            case CURVE_SEGMENT_NODE_PROPERTY_TENSION:
            {
                system_variant_get_float(value,
                                        &node_ptr->tension);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Node property [%d] is unsupported",
                                  node_property);

                result = false;
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_modify_node_time(__in __notnull curve_segment_data    segment_data,
                                               __in           curve_segment_node_id node_id,
                                               __in           system_timeline_time  new_node_time)
{
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;
    uint32_t                      n_node_elements  = 0;
    _curve_segment_data_tcb_node* node_ptr         = NULL;

    system_resizable_vector_get_property  (segment_data_ptr->nodes,
                                           SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                          &n_node_elements);
    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);

    if (node_ptr != NULL)
    {
        system_timeline_time former_time = node_ptr->time;

        /* Make sure no other node exists at given time */
        for (uint32_t n_node = 0;
                      n_node < n_node_elements;
                    ++n_node)
        {
            _curve_segment_data_tcb_node* tested_node_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   n_node,
                                                  &tested_node_ptr);

            if (tested_node_ptr       != NULL &&
                tested_node_ptr->time == new_node_time)
            {
                return true;
            }
        }

        /* See if we're dealing with a start/end node */
        uint32_t              n_nodes_order_elements = 0;
        curve_segment_node_id last_node_id           = NULL;
        curve_segment_node_id nodes_order_at_0       = NULL;

        system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_nodes_order_elements);

        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               n_nodes_order_elements - 1,
                                              &last_node_id);
        system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                               0,
                                               &nodes_order_at_0);

        bool is_start_node = (node_id == nodes_order_at_0);
        bool is_end_node   = (node_id == last_node_id);

        if (is_start_node || is_end_node)
        {
            _curve_segment_data_tcb_node* nodes_order_0_node_ptr    = NULL;
            _curve_segment_data_tcb_node* nodes_order_last_node_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   nodes_order_at_0,
                                                  &nodes_order_0_node_ptr);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   last_node_id,
                                                  &nodes_order_last_node_ptr);

            system_timeline_time suggested_start_time = (is_start_node ? new_node_time : nodes_order_0_node_ptr->time);
            system_timeline_time suggested_end_time   = (is_end_node   ? new_node_time : nodes_order_last_node_ptr->time);

            if (curve_container_is_range_defined(segment_data_ptr->curve,
                                                 suggested_start_time,
                                                 suggested_end_time,
                                                 segment_data_ptr->id) )
            {
                return false;
            }
        }

        if (is_start_node)
        {
            curve_container_set_segment_property(segment_data_ptr->curve,
                                                 segment_data_ptr->id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                &new_node_time);
        }
        else
        if (is_end_node)
        {
            curve_container_set_segment_property(segment_data_ptr->curve,
                                                 segment_data_ptr->id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                &new_node_time);
        }

        node_ptr->time = new_node_time;

        /* Update execution order if needed */
        uint32_t n_order_node = 0;

        for (uint32_t n_order_next_node = n_order_node + 1;
                      n_order_next_node != n_nodes_order_elements;
                    ++n_order_next_node, n_order_node++)
        {
            curve_segment_node_id         order_node_id       = NULL;
            curve_segment_node_id         order_next_node_id  = NULL;
            _curve_segment_data_tcb_node* order_node_ptr      = NULL;
            _curve_segment_data_tcb_node* order_next_node_ptr = NULL;

            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_order_node,
                                                  &order_node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_order_next_node,
                                                  &order_next_node_id);

            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   order_node_id,
                                                  &order_node_ptr);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   order_next_node_id,
                                                  &order_next_node_ptr);

            if (order_node_ptr->time > order_next_node_ptr->time)
            {
                if (n_order_node == 0)
                {
                    curve_container_set_segment_property(segment_data_ptr->curve,
                                                         segment_data_ptr->id,
                                                         CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                        &order_next_node_ptr->time);
                }

                if (n_order_next_node == n_nodes_order_elements - 1)
                {
                    curve_container_set_segment_property(segment_data_ptr->curve,
                                                         segment_data_ptr->id,
                                                         CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                        &order_node_ptr->time);
                }

                system_resizable_vector_set_element_at(segment_data_ptr->nodes_order,
                                                       n_order_node,
                                                       (void*) order_next_node_id);
                system_resizable_vector_set_element_at(segment_data_ptr->nodes_order,
                                                       n_order_next_node,
                                                       (void*) order_node_id);

                break;
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_modify_node_time_value(__in __notnull curve_segment_data    segment_data,
                                                     __in           curve_segment_node_id node_id,
                                                     __in           system_timeline_time  new_node_time,
                                                     __in __notnull system_variant        new_node_value,
                                                                    bool                  force)
{
    float                         new_node_value_float;
    _curve_segment_data_tcb_node* node_ptr         = NULL;
    bool                          result           = false;
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;

    system_variant_get_float              (new_node_value,
                                          &new_node_value_float);
    system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                           node_id,
                                          &node_ptr);

    if (node_ptr != NULL)
    {
        node_ptr->value = new_node_value_float;

        result = curve_segment_tcb_modify_node_time(segment_data,
                                                    node_id,
                                                    new_node_time);
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool curve_segment_tcb_set_property(__in __notnull curve_segment_data     segment_data,
                                           __in           curve_segment_property segment_property,
                                           __in __notnull system_variant         value)
{
    curve_segment_node_id         node_id          = 0;
    _curve_segment_data_tcb_node* node_ptr         = NULL;
    bool                          result           = true;
    _curve_segment_data_tcb*      segment_data_ptr = (_curve_segment_data_tcb*) segment_data;
    uint32_t                      n_nodes          = 0;

    system_resizable_vector_get_property(segment_data_ptr->nodes_order,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    switch (segment_property)
    {
        case CURVE_SEGMENT_PROPERTY_TCB_START_BIAS:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   0,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_get_float(value,
                                    &node_ptr->bias);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_START_CONTINUITY:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   0,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_get_float(value,
                                    &node_ptr->continuity);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_START_TENSION:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   0,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_get_float(value,
                                    &node_ptr->tension);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_END_BIAS:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_nodes - 1,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_get_float(value,
                                    &node_ptr->bias);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_END_CONTINUITY:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_nodes - 1,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_get_float(value,
                                    &node_ptr->continuity);

            break;
        }

        case CURVE_SEGMENT_PROPERTY_TCB_END_TENSION:
        {
            system_resizable_vector_get_element_at(segment_data_ptr->nodes_order,
                                                   n_nodes-1,
                                                  &node_id);
            system_resizable_vector_get_element_at(segment_data_ptr->nodes,
                                                   node_id,
                                                  &node_ptr);

            system_variant_get_float(value,
                                    &node_ptr->tension);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized TCB segment property requested [%d]",
                              segment_property);

            result = false;
        }
    }

    return result;
}

