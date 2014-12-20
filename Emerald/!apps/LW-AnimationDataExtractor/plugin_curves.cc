#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_curves.h"
#include <Windows.h>

#include "curve/curve_container.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"
#include "system/system_variant.h"

/* Forward declarations */

/* Type declarations */
typedef enum
{
    KEY_TYPE_LINEAR,
    KEY_TYPE_TCB,

    KEY_TYPE_UNDEFINED
} _key_type;

typedef struct _curve_key
{
    double    tcb[3];
    double    time;
    _key_type type;
    double    value;

    _curve_key()
    {
        time  = -1.0;
        type  = KEY_TYPE_UNDEFINED;
        value = -1.0;

        memset(tcb, 0, sizeof(tcb) );
    }
} _curve_key;


/* Forward declarations. */
PRIVATE curve_container                            CreateCurveFromEnvelope                             (const char*   object_name,
                                                                                                        const char*   curve_name,
                                                                                                        LWEnvelopeID  envelope,
                                                                                                        LWChanGroupID groupID);
PRIVATE curve_container_envelope_boundary_behavior GetCurveContainerEnvelopeBoundaryBehaviorForLWEnvTag(int           behavior);

/* Global variables.
 *
 * Hey, this plug-in is implemented with simplicity in mind! I'm sorry if this
 * offends anyone! :D */
system_resizable_vector curve_containers                   = NULL;
system_hash64map        envelope_id_to_curve_container_map = NULL;

/** TODO */
PRIVATE curve_container CreateCurveFromEnvelope(const char*   object_name,
                                                const char*   curve_name,
                                                LWEnvelopeID  envelope,
                                                LWChanGroupID groupID)
{
    /* Extract envelope properties */
    int             envelopePostBehavior = -1;
    int             envelopePreBehavior  = -1;
    bool            result               = true;
    curve_container result_curve         = NULL;
    system_variant  temp_variant_1       = system_variant_create(SYSTEM_VARIANT_FLOAT);
    system_variant  temp_variant_2       = system_variant_create(SYSTEM_VARIANT_FLOAT);

    result &= envelope_ptr->egGet(envelope, groupID, LWENVTAG_POSTBEHAVE, &envelopePostBehavior) != 0;
    result &= envelope_ptr->egGet(envelope, groupID, LWENVTAG_PREBEHAVE,  &envelopePreBehavior)  != 0;

    ASSERT_ALWAYS_SYNC(result,
                       "Could not query envelope properties");

    /* Create the container */
    result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(object_name, curve_name),
                                          SYSTEM_VARIANT_FLOAT);

    ASSERT_DEBUG_SYNC(result_curve != NULL,
                      "Could not create curve container");

    /* Extract all keys and store them in a vector. After all keys are determined, we
     * will be able to create an actual Emerald curve container. */
    LWEnvKeyframeID         currentKey = envelope_ptr->findKey(envelope, 0.0);
    system_resizable_vector keys       = system_resizable_vector_create(4, /* capacity */
                                                                        sizeof(_curve_key*) );

    ASSERT_ALWAYS_SYNC(currentKey != NULL, "Could not find the first envelope key");
    ASSERT_ALWAYS_SYNC(keys       != NULL, "Could not create keys vector");

    while (currentKey != NULL)
    {
        double    keyParams[4] = {0.0};
        int       keyShape     = -1;
        double    keyTCB[3]    = {0.0};
        double    keyTime      = 0.0;
        _key_type keyType      = KEY_TYPE_UNDEFINED;
        double    keyValue     = 0.0;

        /* Query key data */
        result &= envelope_ptr->keyGet(envelope, currentKey, LWKEY_SHAPE, &keyShape) != 0;
        result &= envelope_ptr->keyGet(envelope, currentKey, LWKEY_TIME,  &keyTime)  != 0;
        result &= envelope_ptr->keyGet(envelope, currentKey, LWKEY_VALUE, &keyValue) != 0;

        /* Query spline-specific properties */
        ASSERT_DEBUG_SYNC(result,
                          "Could not query envelope key properties");

        switch (keyShape)
        {
            case 0: /* TCB */
            {
                keyType = KEY_TYPE_TCB;

                result &= envelope_ptr->keyGet(envelope, currentKey, LWKEY_TENSION,    keyTCB + 0) != 0;
                result &= envelope_ptr->keyGet(envelope, currentKey, LWKEY_CONTINUITY, keyTCB + 1) != 0;
                result &= envelope_ptr->keyGet(envelope, currentKey, LWKEY_BIAS,       keyTCB + 2) != 0;

                ASSERT_DEBUG_SYNC(result,
                          "Could not query envelope, spline-specific key properties");

                break;
            }

            case 1: /* Hermite */
            case 2: /* 1D Bezier = Hermite, as per spec */
            case 3: /* Linear: */
            case 4: /* Stepped */
            case 5: /* 2D Bezier */
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unsupported key shape type");

                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized key shape type");
            }
        } /* switch (keyShape) */

        /* Create a key descriptor */
        _curve_key* key_ptr = new _curve_key;

        ASSERT_ALWAYS_SYNC(key_ptr != NULL, "Out of memory");

        memcpy(key_ptr->tcb, keyTCB, sizeof(key_ptr->tcb) );
        key_ptr->time  = keyTime;
        key_ptr->type  = keyType;
        key_ptr->value = keyValue;

        /* Store the key */
        system_resizable_vector_push(keys, key_ptr);

        /* Iterate */
        currentKey = envelope_ptr->nextKey(envelope, currentKey);
    } /* while (currentKey != NULL) */

    /* Now that we know all the keys, we can proceed with curve_container instantiation */
    const uint32_t n_keys = system_resizable_vector_get_amount_of_elements(keys);

    if (n_keys == 1)
    {
        _curve_key*    key       = NULL;
        system_variant key_value = temp_variant_1;

        system_resizable_vector_get_element_at(keys, 0, &key);
        ASSERT_ALWAYS_SYNC(key != NULL, "Curve key descriptor is NULL");

        /* Convert LW value representation to Emerald's */
        system_variant_set_float(key_value, (float) key->value);

        /* Assign the new default value. */
        bool result_bool = curve_container_set_default_value(result_curve, key_value);

        ASSERT_ALWAYS_SYNC(result_bool,
                           "Could not set curve's default value");
    }
    else
    {
        curve_segment_id segment_id = -1;

        for (uint32_t n_key = 0;
                      n_key < n_keys - 1;
                    ++n_key)
        {
            /* Retrieve curve segment key descriptors */
            _curve_key*          next_key       = NULL;
            system_timeline_time next_key_time  = 0;
            system_variant       next_key_value = temp_variant_1;
            _curve_key*          prev_key       = NULL;
            system_timeline_time prev_key_time  = 0;
            system_variant       prev_key_value = temp_variant_2;

            system_resizable_vector_get_element_at(keys, n_key,     &prev_key);
            system_resizable_vector_get_element_at(keys, n_key + 1, &next_key);

            ASSERT_ALWAYS_SYNC(prev_key != NULL, "Previous curve key descriptor is NULL");
            ASSERT_ALWAYS_SYNC(next_key != NULL, "Next curve key descriptor is NULL");

            /* Convert LW time representation to Emerald one */
            next_key_time = system_time_get_timeline_time_for_msec( uint32_t(next_key->time * 1000.0) );
            prev_key_time = system_time_get_timeline_time_for_msec( uint32_t(prev_key->time * 1000.0) );

            /* Convert LW value representation to Emerald one */
            system_variant_set_float(next_key_value, (float) next_key->value);
            system_variant_set_float(prev_key_value, (float) prev_key->value);

            /* Spawn the segment.
             *
             * We're asserting here that envelope uses a single interpolation type, so we're OK
             * to spawn a single segment for the very first key, and then re-use that segment for
             * all subsequent nodes. This may require fixing.
             */
            ASSERT_DEBUG_SYNC(prev_key->type == next_key->type,
                              "Key segment types do not match");

            if (n_key == 0)
            {
                switch (prev_key->type)
                {
                    case KEY_TYPE_TCB:
                    {
                        bool result_bool = false;

                        result_bool = curve_container_add_tcb_segment(result_curve,
                                                                      prev_key_time,
                                                                      next_key_time,
                                                                      prev_key_value,
                                                                      (float) prev_key->tcb[0],
                                                                      (float) prev_key->tcb[1],
                                                                      (float) prev_key->tcb[2],
                                                                      next_key_value,
                                                                      (float) next_key->tcb[0],
                                                                      (float) next_key->tcb[1],
                                                                      (float) next_key->tcb[2],
                                                                     &segment_id);

                        ASSERT_ALWAYS_SYNC(result_bool,
                                           "Could not add a new TCB segment");

                        break;
                    }

                    default:
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Unrecognized interpolation type");
                    }
                } /* switch (prev_key->type) */
            } /* if (n_key == 0) */
            else
            {
                /* Append a new node to the segment */
                curve_segment_node_id new_node_id = -1;
                bool                  result_bool = false;

                result_bool = curve_container_add_tcb_node(result_curve,
                                                           segment_id,
                                                           next_key_time,
                                                           next_key_value,
                                                           (float) next_key->tcb[0],
                                                           (float) next_key->tcb[1],
                                                           (float) next_key->tcb[2],
                                                          &new_node_id);

                ASSERT_ALWAYS_SYNC(result_bool,
                                   "Could not add a new TCB segment node");
            }
        } /* for (all keys) */
    } /* if (n_keys != 1) */

    /* Set envelope-wide properties */
    curve_container_envelope_boundary_behavior post_behavior = GetCurveContainerEnvelopeBoundaryBehaviorForLWEnvTag(envelopePostBehavior);
    curve_container_envelope_boundary_behavior pre_behavior  = GetCurveContainerEnvelopeBoundaryBehaviorForLWEnvTag(envelopePreBehavior);

    ASSERT_ALWAYS_SYNC(post_behavior != CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED,
                       "Unsupported post-behavior");
    ASSERT_ALWAYS_SYNC(pre_behavior != CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED,
                       "Unsupported pre-behavior");

    curve_container_set_property(result_curve,
                                 CURVE_CONTAINER_PROPERTY_POST_BEHAVIOR,
                                &post_behavior);
    curve_container_set_property(result_curve,
                                 CURVE_CONTAINER_PROPERTY_PRE_BEHAVIOR,
                                &pre_behavior);

    /* TODO: This will throw an assertion *if* there's more than one interpolation type used
     *       in a single envelope. If necessary, fix this. */
    if (n_keys > 2)
    {
        const static bool enable_flag = true;

        curve_container_set_property(result_curve,
                                     CURVE_CONTAINER_PROPERTY_PRE_POST_BEHAVIOR_STATUS,
                                    &enable_flag);
    }

    /* That's it :-) */
    system_variant_release(temp_variant_1);
    system_variant_release(temp_variant_2);

    return result_curve;
}

/** TODO */
PUBLIC void DeinitCurveData()
{
    ASSERT_DEBUG_SYNC(curve_containers                   != NULL &&
                      envelope_id_to_curve_container_map != NULL,
                      "Curve data-set already deinitialized?");

    if (curve_containers != NULL)
    {
        curve_container current_curve_container = NULL;

        while (!system_resizable_vector_pop(curve_containers,
                                           &current_curve_container) )
        {
            curve_container_release(current_curve_container);

            current_curve_container = NULL;
        }

        system_resizable_vector_release(curve_containers);
        curve_containers = NULL;
    }

    if (envelope_id_to_curve_container_map != NULL)
    {
        system_hash64map_release(envelope_id_to_curve_container_map);

        envelope_id_to_curve_container_map = NULL;
    }
}

/** TODO */
PRIVATE curve_container_envelope_boundary_behavior GetCurveContainerEnvelopeBoundaryBehaviorForLWEnvTag(int behavior)
{
    curve_container_envelope_boundary_behavior result = CURVE_CONTAINER_BOUNDARY_BEHAVIOR_UNDEFINED;

    /* TODO: This is currently missing support for "Oscillate", "Offset Repeat" and "Linear". Add
     *       if necessary.
     */
    switch (behavior)
    {
        case 0:
        {
            result = CURVE_CONTAINER_BOUNDARY_BEHAVIOR_RESET;

            break;
        }

        case 1:
        {
            result = CURVE_CONTAINER_BOUNDARY_BEHAVIOR_CONSTANT;

            break;
        }

        case 2:
        {
            result = CURVE_CONTAINER_BOUNDARY_BEHAVIOR_REPEAT;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unsupported pre- / post- behavior.");

            break;
        }
    } /* switch (behavior) */

    return result;
}

/** TODO */
PUBLIC system_hash64map GetEnvelopeIDToCurveContainerHashMap()
{
    ASSERT_DEBUG_SYNC(envelope_id_to_curve_container_map != NULL,
                      "Envelope ID to curve container map is NULL");

    return envelope_id_to_curve_container_map;
}

/** TODO */
PUBLIC void InitCurveData()
{
    ASSERT_DEBUG_SYNC(curve_containers                   == NULL &&
                      envelope_id_to_curve_container_map == NULL,
                      "Curve data-set already initialized?");

    LWItemType items[] =
    {
        LWI_OBJECT,
        LWI_LIGHT,
        LWI_CAMERA
    };

          unsigned int n_item  = 0;
    const unsigned int n_items = sizeof(items) / sizeof(items[0]);

    /* Iterate over all object types .. */
    for (  n_item = 0;
           n_item < n_items;
         ++n_item)
    {
        LWItemID   current_lw_item_id = 0;
        LWItemType current_lw_type    = items[n_item];

        current_lw_item_id = item_info_ptr->first(current_lw_type,
                                                  LWITEM_NULL);

        while (current_lw_item_id != LWITEM_NULL)
        {
            LWChanGroupID current_lw_channel_group      = item_info_ptr->chanGroup   (current_lw_item_id);
            const char*   current_lw_channel_group_name = channel_info_ptr->groupName(current_lw_channel_group);

            /* Group name corresponds to the object name */
            system_hashed_ansi_string object_name = system_hashed_ansi_string_create(current_lw_channel_group_name);

            /* Iterate over all channels defined in the channel group */
            LWChannelID current_lw_channel = NULL;

            while ( (current_lw_channel = channel_info_ptr->nextChannel(current_lw_channel_group,
                                                                        current_lw_channel)) != NULL)
            {
                const char*        current_lw_channel_name     = channel_info_ptr->channelName    (current_lw_channel);
                const LWEnvelopeID current_lw_channel_envelope = channel_info_ptr->channelEnvelope(current_lw_channel);

                /* Channel name corresponds to the curve name */
                system_hashed_ansi_string curve_name = system_hashed_ansi_string_create(current_lw_channel_name);
                curve_container           curve      = CreateCurveFromEnvelope         (system_hashed_ansi_string_get_buffer(object_name),
                                                                                        system_hashed_ansi_string_get_buffer(curve_name),
                                                                                        current_lw_channel_envelope,
                                                                                        current_lw_channel_group);

                /* Store the curve */
                system_resizable_vector_push(curve_containers,
                                             curve);

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(envelope_id_to_curve_container_map,
                                                             (system_hash64) current_lw_channel_envelope),
                                  "Animation envelope already stored in internal hash-map!");

                system_hash64map_insert(envelope_id_to_curve_container_map,
                                        (system_hash64) current_lw_channel_envelope,
                                        curve,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            } /* while (channels exist) */

            /* Iterate to the next item of the current type.*/
            current_lw_item_id = item_info_ptr->next(current_lw_item_id);
        }
    } /* for (all items) */
}
