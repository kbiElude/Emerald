#define _MSWIN

#include "shared.h"
#include "curve/curve_container.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_variant.h"

#include "plugin.h"
#include "plugin_common.h"

/** TODO */
PRIVATE void AdjustCurveNodeValueByDelta(__in __notnull curve_container       curve,
                                         __in           curve_segment_id      segment_id,
                                         __in           curve_segment_node_id node_id,
                                         __in           float                 delta,
                                         __in           system_variant        temp_float_variant,
                                         __in           bool                  update_default_value)
{
    /** TODO: If needed, add integer variant support */
    system_timeline_time node_time = 0;
    float                temp_float;

    if (update_default_value)
    {
        if (!curve_container_get_default_value(curve,
                                               false,
                                               temp_float_variant) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "curve_container_get_default_value() failed.");
        }
    }
    else
    {
        if (!curve_container_get_general_node_data(curve,
                                                   segment_id,
                                                   node_id,
                                                  &node_time,
                                                   temp_float_variant) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "curve_container_get_general_node_data() failed.");
        }
    }

    system_variant_get_float(temp_float_variant,
                            &temp_float);

    temp_float += delta;

    system_variant_set_float(temp_float_variant,
                             temp_float);

    if (update_default_value)
    {
        if (!curve_container_set_default_value(curve,
                                               temp_float_variant) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "curve_container_set_default_value() failed.");
        }
    }
    else
    {
        if (!curve_container_modify_node(curve,
                                         segment_id,
                                         node_id,
                                         node_time,
                                         temp_float_variant) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "curve_container_modify_node() failed.");
        }
    }
}


/** Please see header for spec */
PUBLIC void AdjustCurveByDelta(__in __notnull curve_container curve,
                               __in           float           delta)
{
    uint32_t       n_segments         = 0;
    system_variant temp_float_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_N_SEGMENTS,
                                &n_segments);

    for (uint32_t n_segment = 0;
                  n_segment < n_segments + 1;
                ++n_segment)
    {
        curve_segment_id segment = -1;
        uint32_t         n_nodes = 0;

        curve_container_get_segment_id_for_nth_segment(curve,
                                                       n_segment,
                                                      &segment);

        curve_container_get_segment_property(curve,
                                             segment,
                                             CURVE_CONTAINER_SEGMENT_PROPERTY_N_NODES,
                                            &n_nodes);

        for (uint32_t n_node = 0;
                      n_node < n_nodes;
                    ++n_node)
        {
            curve_segment_node_id node_id = -1;

            curve_container_get_node_id_for_node_at(curve,
                                                    segment,
                                                    n_node,
                                                   &node_id);

            AdjustCurveNodeValueByDelta(curve,
                                        segment,
                                        node_id,
                                        delta,
                                        temp_float_variant,
                                        false);
        } /* for (all nodes) */
    } /* for (all curve segments) */

    /* Also adjust the default value */
    AdjustCurveNodeValueByDelta(curve,
                                0, /* segment_id */
                                0, /* node_id */
                                delta,
                                temp_float_variant,
                                true);

    /* Clean up */
    system_variant_release(temp_float_variant);
    temp_float_variant = NULL;
}

/** Please see header for spec */
PUBLIC LWEnvelopeID FindEnvelope(__in           LWChanGroupID  group_id,
                                 __in __notnull const char*    envelope_name)
{
    LWChannelID  channel_id = channel_info_ptr->nextChannel(group_id,
                                                            NULL);
    LWEnvelopeID result     = 0;

    while (channel_id != 0)
    {
        const char* channel_name = channel_info_ptr->channelName(channel_id);

        if (strcmp(channel_name,
                   envelope_name) == 0)
        {
            result = channel_info_ptr->channelEnvelope(channel_id);

            goto end;
        } /* if (strcmp(channel_name, name) != 0) */

        channel_id = channel_info_ptr->nextChannel(group_id,
                                                   channel_id);
    } /* while (channel_id != 0) */

end:
    return result;
}

/** Please see header for spec */
PUBLIC LWChanGroupID FindGroup(__in __notnull const char* group_name)
{
    LWChanGroupID group_id = channel_info_ptr->nextGroup(0,     /* parent */
                                                         NULL); /* group */
    LWEnvelopeID  result   = 0;

    while (group_id != 0)
    {
        const char* current_group_name = channel_info_ptr->groupName(group_id);

        if (strcmp(group_name,
                   current_group_name) == 0)
        {
            result = group_id;

            goto end;
        }

        group_id = channel_info_ptr->nextGroup(0, /* parent */
                                               group_id);
    } /* while (group_id != 0) */

end:
    return result;
}

/** Please see header for spec */
PUBLIC void GetCurveContainerForProperty(__in                system_hashed_ansi_string object_name,
                                         __in                _item_property            property,
                                         __in      __notnull LWItemID                  item_id,
                                         __out_opt           curve_container*          out_curve_ptr,
                                         __out_opt           curve_id*                 out_curve_id_ptr)
{
    const char*     property_name = NULL;
    curve_container result_curve  = NULL;
    curve_id        result_id     = -1;

    switch (property)
    {
        case ITEM_PROPERTY_F_STOP:                   property_name = "FStop";            break;
        case ITEM_PROPERTY_FOCAL_DISTANCE:           property_name = "FocalDistance";    break;
        case ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_R:    property_name = "AmbientColor.R";   break;
        case ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_G:    property_name = "AmbientColor.G";   break;
        case ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_B:    property_name = "AmbientColor.B";   break;
        case ITEM_PROPERTY_LIGHT_AMBIENT_INTENSITY:  property_name = "AmbientIntensity"; break;
        case ITEM_PROPERTY_LIGHT_COLOR_R:            property_name = "Color.R";          break;
        case ITEM_PROPERTY_LIGHT_COLOR_G:            property_name = "Color.G";          break;
        case ITEM_PROPERTY_LIGHT_COLOR_B:            property_name = "Color.B";          break;
        case ITEM_PROPERTY_LIGHT_COLOR_INTENSITY:    property_name = "Intensity";        break;
        case ITEM_PROPERTY_LIGHT_CONE_ANGLE:         property_name = "ConeAngle";        break;
        case ITEM_PROPERTY_LIGHT_EDGE_ANGLE:         property_name = "EdgeAngle";        break;
        case ITEM_PROPERTY_LIGHT_RANGE:              property_name = "Range";            break;
        case ITEM_PROPERTY_ROTATION_B:               property_name = "Rotation.B";       break;
        case ITEM_PROPERTY_ROTATION_H:               property_name = "Rotation.H";       break;
        case ITEM_PROPERTY_ROTATION_P:               property_name = "Rotation.P";       break;
        case ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_R:  property_name = "Color.R";          break;
        case ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_G:  property_name = "Color.G";          break;
        case ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_B:  property_name = "Color.B";          break;
        case ITEM_PROPERTY_SURFACE_GLOSINESS:        property_name = SURF_GLOS;          break;
        case ITEM_PROPERTY_SURFACE_LUMINOSITY:       property_name = SURF_LUMI;          break;
        case ITEM_PROPERTY_SURFACE_REFLECTION_RATIO: property_name = SURF_REFL;          break;
        case ITEM_PROPERTY_SURFACE_SPECULAR_RATIO:   property_name = SURF_SPEC;          break;
        case ITEM_PROPERTY_TRANSLATION_X:            property_name = "Position.X";       break;
        case ITEM_PROPERTY_TRANSLATION_Y:            property_name = "Position.Y";       break;
        case ITEM_PROPERTY_TRANSLATION_Z:            property_name = "Position.Z";       break;
        case ITEM_PROPERTY_ZOOM_FACTOR:              property_name = "ZoomFactor";       break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized item property");
        }
    } /* switch (property) */

    /* Identify channel group */
    LWChanGroupID group_id = 0;

    if (item_id != 0)
    {
        group_id = item_info_ptr->chanGroup(item_id);
    }
    else
    {
        group_id = FindGroup("Global");
    }

    /* Identify envelope ID */
    LWEnvelopeID envelope_id = FindEnvelope(group_id,
                                            property_name);

    /* Envelope needs not be defined for all properties we support. Check the special cases
     * and handle them at this point */
    if (envelope_id == 0)
    {
        float          new_value;
        system_variant new_value_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

        switch (property)
        {
            case ITEM_PROPERTY_F_STOP:
            {
                new_value = (float) camera_info_ptr->fStop(item_id,
                                                           0.0); /* time */

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " F Stop"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            } /* case ITEM_PROPERTY_F_STOP: */

            case ITEM_PROPERTY_FOCAL_DISTANCE:
            {
                new_value = (float) camera_info_ptr->focalDistance(item_id,
                                                                   0.0); /* time */

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Focal Distance"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            } /* case ITEM_PROPERTY_FOCAL_DISTANCE: */

            case ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_R:
            case ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_G:
            case ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_B:
            {
                LWDVector ambient_color_data;

                light_info_ptr->ambientRaw(0.0,
                                           ambient_color_data);

                new_value = (float) ((property == ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_R) ? ambient_color_data[0] :
                                     (property == ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_G) ? ambient_color_data[1] :
                                                                                        ambient_color_data[2]);

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              ((property == ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_R) ? " Ambient Color R" :
                                                                                                               (property == ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_G) ? " Ambient Color G" :
                                                                                                                                                                   " Ambient Color B")),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_LIGHT_AMBIENT_INTENSITY:
            {
                new_value = (float) light_info_ptr->ambientIntensity(0.0);

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Ambient Intensity"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_LIGHT_COLOR_R:
            case ITEM_PROPERTY_LIGHT_COLOR_G:
            case ITEM_PROPERTY_LIGHT_COLOR_B:
            {
                LWDVector color_data;

                light_info_ptr->rawColor(item_id,
                                         0.0,
                                         color_data);

                new_value = (float) ((property == ITEM_PROPERTY_LIGHT_COLOR_R) ? color_data[0] :
                                     (property == ITEM_PROPERTY_LIGHT_COLOR_G) ? color_data[1] :
                                                                                 color_data[2]);

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              ((property == ITEM_PROPERTY_LIGHT_COLOR_R) ? " Color R" :
                                                                                                               (property == ITEM_PROPERTY_LIGHT_COLOR_G) ? " Color G" :
                                                                                                                                                           " Color B")),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_LIGHT_COLOR_INTENSITY:
            {
                new_value = (float) light_info_ptr->intensity(item_id,
                                                              0.0);

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Color Intensity"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_LIGHT_CONE_ANGLE:
            case ITEM_PROPERTY_LIGHT_EDGE_ANGLE:
            {
                double edge, radius;

                light_info_ptr->coneAngles(item_id,
                                           0.0,
                                          &radius,
                                          &edge);

                new_value = (property == ITEM_PROPERTY_LIGHT_CONE_ANGLE) ? radius : edge;

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              (property == ITEM_PROPERTY_LIGHT_CONE_ANGLE) ? " Cone Angle" : " Edge Angle"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_LIGHT_RANGE:
            {
                new_value = (float) light_info_ptr->range(item_id,
                                                          0.0);

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Range"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_B:
            case ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_G:
            case ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_R:
            {
                double* color_data = surface_funcs_ptr->getFlt(item_id,
                                                               SURF_COLR);

                new_value = (float) color_data[ (property == ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_R) ? 0
                                              : (property == ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_G) ? 1
                                              :                                                       2];

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              ((property == ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_R) ? " Color R" :
                                                                                                               (property == ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_G) ? " Color G" :
                                                                                                                                                                     " Color B")),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_SURFACE_GLOSINESS:
            {
                new_value = (float) *(surface_funcs_ptr->getFlt(item_id,
                                                                SURF_SPEC) );

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Glosiness"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_SURFACE_LUMINOSITY:
            {
                new_value = (float) *(surface_funcs_ptr->getFlt(item_id,
                                                                SURF_LUMI) );

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Luminosity"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_SURFACE_REFLECTION_RATIO:
            {
                new_value = (float) *(surface_funcs_ptr->getFlt(item_id,
                                                                SURF_REFL) );

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Reflection Ratio"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            }

            case ITEM_PROPERTY_SURFACE_SPECULAR_RATIO:
            {
                new_value = (float) *(surface_funcs_ptr->getFlt(item_id,
                                                                SURF_SPEC) );

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Specular Ratio"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);
            }

            case ITEM_PROPERTY_ZOOM_FACTOR:
            {
                new_value = (float) camera_info_ptr->zoomFactor(item_id,
                                                                0.0); /* time */

                result_curve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(object_name),
                                                                                                              " Zoom Factor"),
                                                      NULL, /* object_manager_path */
                                                      SYSTEM_VARIANT_FLOAT);

                break;
            } /* case ITEM_PROPERTY_ZOOM_FACTOR: */
        } /* switch (property) */

        ASSERT_DEBUG_SYNC(result_curve != NULL,
                          "No curve container created?");

        if (result_curve != NULL)
        {
            result_id = AddCurveContainerToEnvelopeIDToCurveContainerHashMap(result_curve);

            system_variant_set_float         (new_value_variant,
                                              new_value);
            curve_container_set_default_value(result_curve,
                                              new_value_variant);
        }

        system_variant_release(new_value_variant);
    }
    else
    {
        /* Make sure the curve is recognized */
        system_hash64map envelope_id_to_curve_container_map = GetEnvelopeIDToCurveContainerHashMap();

        if (!system_hash64map_get(envelope_id_to_curve_container_map,
                                  (system_hash64) envelope_id,
                                 &result_curve) )

        {
            ASSERT_ALWAYS_SYNC(false,
                               "Envelope ID does not correspond to a recognized curve.");
        }

        result_id = (curve_id) envelope_id;
    }

    if (result_curve == NULL &&
        envelope_id  == 0)
    {
        ASSERT_ALWAYS_SYNC(false,
                          "This should have never happened");

        goto end;
    }

    /* result at this point is set to the requested curve container */
    if (out_curve_id_ptr != NULL)
    {
        *out_curve_id_ptr = result_id;
    }

    if (out_curve_ptr != NULL)
    {
        *out_curve_ptr = result_curve;
    }

end:
    ;
}
