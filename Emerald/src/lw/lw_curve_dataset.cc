/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "lw/lw_curve_dataset.h"
#include "scene/scene.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_variant.h"
#include <string>


/** Internal type definition */
typedef struct _lw_curve_dataset_item
{
    curve_container           curve;
    system_hashed_ansi_string name;
    system_hashed_ansi_string object_name;

    _lw_curve_dataset_item()
    {
        curve       = NULL;
        name        = NULL;
        object_name = NULL;
    }

    ~_lw_curve_dataset_item()
    {
        if (curve != NULL)
        {
            curve_container_release(curve);

            curve = NULL;
        }
    }
} _lw_curve_dataset_item;

typedef struct _lw_curve_dataset_object
{
    lw_curve_dataset_object_type object_type;

    _lw_curve_dataset_object()
    {
        object_type = LW_CURVE_DATASET_OBJECT_TYPE_UNDEFINED;
    }
} _lw_curve_dataset_object;

typedef enum
{
    LW_CURVE_TYPE_POSITION_X,
    LW_CURVE_TYPE_POSITION_Y,
    LW_CURVE_TYPE_POSITION_Z,
    LW_CURVE_TYPE_ROTATION_B,
    LW_CURVE_TYPE_ROTATION_H,
    LW_CURVE_TYPE_ROTATION_P,
    LW_CURVE_TYPE_SCALE_X,
    LW_CURVE_TYPE_SCALE_Y,
    LW_CURVE_TYPE_SCALE_Z,

    LW_CURVE_TYPE_UNDEFINED
} _lw_curve_type;


typedef struct
{
    float            fps;
    system_hash64map object_to_item_vector_map; /* "object name" hashed_ansi_string -> vector of _lw_curve_dataset_item* items */
    system_hash64map object_to_properties_map;  /* "object name" hashed ansi string -> _lw_curve_dataset_object* */

    /* Helper stuff */
    curve_container zeroCurve;
    curve_container oneCurve;

    REFCOUNT_INSERT_VARIABLES
} _lw_curve_dataset;


/** Internal variables */

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_curve_dataset, lw_curve_dataset, _lw_curve_dataset);


/** TODO */
PRIVATE void _lw_curve_dataset_apply_to_node(__in __notnull _lw_curve_dataset* dataset_ptr,
                                             __in           _lw_curve_type     curve_type,
                                             __in __notnull curve_container    new_curve,
                                             __in __notnull scene_graph        graph,
                                             __in __notnull scene_graph_node   owner_node)
{
    scene_graph_node_tag node_tag = SCENE_GRAPH_NODE_TAG_UNDEFINED;
    bool                 result   = true;

    switch (curve_type)
    {
        case LW_CURVE_TYPE_POSITION_X:
        case LW_CURVE_TYPE_POSITION_Y:
        case LW_CURVE_TYPE_POSITION_Z:
        {
            node_tag = SCENE_GRAPH_NODE_TAG_TRANSLATE;

            break;
        }

        case LW_CURVE_TYPE_ROTATION_B:
        {
            node_tag = SCENE_GRAPH_NODE_TAG_ROTATE_Z;

            break;
        }

        case LW_CURVE_TYPE_ROTATION_H:
        {
            node_tag = SCENE_GRAPH_NODE_TAG_ROTATE_Y;

            break;
        }

        case LW_CURVE_TYPE_ROTATION_P:
        {
            node_tag = SCENE_GRAPH_NODE_TAG_ROTATE_X;

            break;
        }

        case LW_CURVE_TYPE_SCALE_X:
        case LW_CURVE_TYPE_SCALE_Y:
        case LW_CURVE_TYPE_SCALE_Z:
        {
            node_tag = SCENE_GRAPH_NODE_TAG_SCALE;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized LW curve type");
        }
    } /* switch (curve_type) */

    /* Retrieve a scene graph node that describes the transformation we need to update */
    scene_graph_node transformation_node = NULL;

    result = scene_graph_node_get_transformation_node(owner_node,
                                                      node_tag,
                                                     &transformation_node);

    if (!result || transformation_node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve transformation node");

        goto end;
    }

    /* Translation node may have been initialized as a matrix node type. In
     * order to feed curve-driven components, we need to use a different node.
     */
    if (node_tag == SCENE_GRAPH_NODE_TAG_TRANSLATE)
    {
        scene_graph_node_type node_type = SCENE_GRAPH_NODE_TYPE_UNKNOWN;

        scene_graph_node_get_property(transformation_node,
                                      SCENE_GRAPH_NODE_PROPERTY_TYPE,
                                     &node_type);

        if (node_type != SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC)
        {
            curve_container neutral_curves[] =
            {
                dataset_ptr->zeroCurve, /* x */
                dataset_ptr->zeroCurve, /* y */
                dataset_ptr->zeroCurve  /* z */
            };

            /* Create a dynamic translation node */
            scene_graph_node new_translation_node = scene_graph_create_translation_dynamic_node(graph,
                                                                                                neutral_curves,
                                                                                                SCENE_GRAPH_NODE_TAG_TRANSLATE);

            ASSERT_DEBUG_SYNC(new_translation_node != NULL,
                              "Could not spawn a dynamic translation scene graph node");

            scene_graph_node_replace(graph,
                                     transformation_node,
                                     new_translation_node);
        }
    } /* if (node_tag == SCENE_GRAPH_NODE_TAG_TRANSLATE) */
    else
    /* Same holds for scale nodes */
    if (node_tag == SCENE_GRAPH_NODE_TAG_SCALE)
    {
        scene_graph_node_type node_type = SCENE_GRAPH_NODE_TYPE_UNKNOWN;

        scene_graph_node_get_property(transformation_node,
                                      SCENE_GRAPH_NODE_PROPERTY_TYPE,
                                     &node_type);

        if (node_type != SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC)
        {
            curve_container neutral_curves[] =
            {
                dataset_ptr->oneCurve, /* x */
                dataset_ptr->oneCurve, /* y */
                dataset_ptr->oneCurve  /* z */
            };

            /* Create a dynamic scale node */
            scene_graph_node new_scale_node = scene_graph_create_scale_dynamic_node(graph,
                                                                                    neutral_curves,
                                                                                    SCENE_GRAPH_NODE_TAG_SCALE);

            ASSERT_DEBUG_SYNC(new_scale_node != NULL,
                              "Could not spawn a dynamic scale scene graph node");

            scene_graph_node_replace(graph,
                                     transformation_node,
                                     new_scale_node);
        }
    } /* if (node_tag == SCENE_GRAPH_NODE_TAG_SCALE) */

    /* For a proper match, translation curves need to be adjusted:
     *
     * 1) X, Y, Z curves need to be multipled by 100
     * 2) Z curve needs to additionally be multiplied by -1.
     *
     * For rotation curves:
     *
     * 1) X and Y rotations need to be multiplied by -1.
     */
    if (node_tag   == SCENE_GRAPH_NODE_TAG_TRANSLATE ||
        curve_type == LW_CURVE_TYPE_ROTATION_H       ||
        curve_type == LW_CURVE_TYPE_ROTATION_P)
    {
        int            multiplier   = 1;
        system_variant temp_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

        if (node_tag == SCENE_GRAPH_NODE_TAG_TRANSLATE)
        {
            multiplier = 100;
        }

        if (curve_type == LW_CURVE_TYPE_POSITION_Z ||
            curve_type == LW_CURVE_TYPE_ROTATION_H ||
            curve_type == LW_CURVE_TYPE_ROTATION_P)
        {
            multiplier *= -1;
        }

        /* Iterate through all curve nodes and adjust the values */
        unsigned int n_curve_segments = 0;

        curve_container_get_property(new_curve,
                                     CURVE_CONTAINER_PROPERTY_N_SEGMENTS,
                                    &n_curve_segments);

        for (unsigned int n_curve_segment = 0;
                          n_curve_segment < n_curve_segments;
                        ++n_curve_segment)
        {
            unsigned int     n_segment_nodes = -1;
            curve_segment_id segment_id      = -1;

            if (!curve_container_get_segment_id_for_nth_segment(new_curve,
                                                                n_curve_segment,
                                                               &segment_id) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve curve segment at index [%d]",
                                  n_curve_segment);

                continue;
            }

            curve_container_get_segment_property(new_curve,
                                                 segment_id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_N_NODES,
                                                &n_segment_nodes);

            for (unsigned int n_segment_node = 0;
                              n_segment_node < n_segment_nodes;
                            ++n_segment_node)
            {
                curve_segment_node_id node_id    = -1;
                system_timeline_time  node_time  = 0;
                float                 node_value = FLT_MAX;

                if (!curve_container_get_node_id_for_node_at(new_curve,
                                                             segment_id,
                                                             n_segment_node,
                                                            &node_id) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve curve segment node at index [%d]",
                                      n_segment_node);

                    continue;
                }

                if (!curve_container_get_general_node_data(new_curve,
                                                           segment_id,
                                                           node_id,
                                                          &node_time,
                                                           temp_variant) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve curve segment node value");

                    continue;
                }

                system_variant_get_float(temp_variant,
                                        &node_value);

                node_value *= multiplier;

                system_variant_set_float(temp_variant,
                                         node_value);

                if (!curve_container_modify_node(new_curve,
                                                 segment_id,
                                                 node_id,
                                                 node_time,
                                                 temp_variant) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not update node properties");
                }
            } /* for (all curve segment nodes) */
        } /* for (all curve segments) */

        /* Also adjust default value */
        float default_value = FLT_MAX;

        if (!curve_container_get_default_value(new_curve,
                                               false, /* should_force */
                                               temp_variant) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve default curve value");
        }

        system_variant_get_float(temp_variant,
                                &default_value);

        default_value *= multiplier;

        system_variant_set_float(temp_variant,
                                 default_value);

        curve_container_set_default_value(new_curve,
                                          temp_variant);

        /* Release the variant */
        system_variant_release(temp_variant);
    }

    /* The transformation node we now have needs to use a new transformation.
     * There are two cases to consider:
     *
     * 1) There is a 1:1 mapping between COLLADA file and LW curves.
     *    This happens for rotation, where the LW curve describes the
     *    angle, and the rotation axis is predefined & static.
     * 2) LW uses 1D curve per each channel, whereas COLLADA file
     *    defines a single node for the transformation. That's the
     *    case with scale and translation.
     */
    switch (curve_type)
    {
        case LW_CURVE_TYPE_POSITION_X:
        case LW_CURVE_TYPE_SCALE_X:
        {
            scene_graph_node_set_property(transformation_node,
                                          SCENE_GRAPH_NODE_PROPERTY_CURVE_X,
                                         &new_curve);

            break;
        }

        case LW_CURVE_TYPE_POSITION_Y:
        case LW_CURVE_TYPE_SCALE_Y:
        {
            scene_graph_node_set_property(transformation_node,
                                          SCENE_GRAPH_NODE_PROPERTY_CURVE_Y,
                                         &new_curve);

            break;
        }

        case LW_CURVE_TYPE_POSITION_Z:
        case LW_CURVE_TYPE_SCALE_Z:
        {
            scene_graph_node_set_property(transformation_node,
                                          SCENE_GRAPH_NODE_PROPERTY_CURVE_Z,
                                         &new_curve);

            break;
        }

        case LW_CURVE_TYPE_ROTATION_B:
        case LW_CURVE_TYPE_ROTATION_H:
        case LW_CURVE_TYPE_ROTATION_P:
        {
            curve_container rotation_curves[4] = {NULL};

            rotation_curves[0] = new_curve; /* angle */

            if (curve_type == LW_CURVE_TYPE_ROTATION_B)
            {
                rotation_curves[1] = dataset_ptr->zeroCurve; /* x */
                rotation_curves[2] = dataset_ptr->zeroCurve; /* y */
                rotation_curves[3] = dataset_ptr->oneCurve;  /* z */
            }
            else
            if (curve_type == LW_CURVE_TYPE_ROTATION_H)
            {
                rotation_curves[1] = dataset_ptr->zeroCurve; /* x */
                rotation_curves[2] = dataset_ptr->oneCurve;  /* y */
                rotation_curves[3] = dataset_ptr->zeroCurve; /* z */
            }
            else
            {
                rotation_curves[1] = dataset_ptr->oneCurve;  /* x */
                rotation_curves[2] = dataset_ptr->zeroCurve; /* y */
                rotation_curves[3] = dataset_ptr->zeroCurve; /* z */
            }

            scene_graph_node rotation_node = scene_graph_create_rotation_dynamic_node(graph,
                                                                                      rotation_curves,
                                                                                      true,     /* LW exports rotation angles in radians */
                                                                                      node_tag);

            ASSERT_DEBUG_SYNC(rotation_node != NULL,
                              "Could not create a rotation dynamic node");

            /* NOTE: This call releases rotation_node, which suits us just fine. */
            scene_graph_node_replace(graph,
                                     transformation_node,
                                     rotation_node);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve type");
        }
    } /* switch (curve_type) */

end:
    ;
}

/** TODO */
PRIVATE system_hashed_ansi_string _lw_curve_dataset_get_lw_adjusted_object_name(__in            unsigned int              n_iteration,
                                                                                __out_opt       unsigned int*             out_n_supported_iterations,
                                                                                __in  __notnull system_hashed_ansi_string name)
{
    std::string name_string;

    if (name != NULL)
    {
        name_string = system_hashed_ansi_string_get_buffer(name);

        if (n_iteration & (1 << 0) )
        {
            /* Replace spaces with _, just as the exporter does. */
            std::size_t token_location = std::string::npos;

            while ( (token_location = name_string.find(" ") ) != std::string::npos)
            {
                name_string.replace(token_location, 1, "_");
            }
        }

        if (n_iteration & (1 << 1) )
        {
            /* Append _1 */
            name_string.append("_1");
        }
    } /* if (name != NULL) */

    if (out_n_supported_iterations != NULL)
    {
        *out_n_supported_iterations = 4;
    }

    return system_hashed_ansi_string_create(name_string.c_str() );
}

/** TODO */
PRIVATE void _lw_curve_dataset_release(__in __notnull __deallocate(mem) void* ptr)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) ptr;

    if (dataset_ptr->object_to_item_vector_map != NULL)
    {
        const uint32_t n_entries = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_item_vector_map);

        for (uint32_t n_entry = 0;
                      n_entry < n_entries;
                    ++n_entry)
        {
            system_resizable_vector curve_vector = NULL;
            _lw_curve_dataset_item* item_ptr     = NULL;
            system_hash64           vector_hash  = 0;

            if (!system_hash64map_get_element_at(dataset_ptr->object_to_item_vector_map,
                                                 n_entry,
                                                &curve_vector,
                                                &vector_hash) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Hash map getter failed.");
            }
            else
            {
                while (system_resizable_vector_pop(curve_vector, &item_ptr) )
                {
                    delete item_ptr;

                    item_ptr = NULL;
                }

                system_resizable_vector_release(curve_vector);
                curve_vector = NULL;
            }
        } /* while (the object hash-map contains vectors) */

        system_hash64map_release(dataset_ptr->object_to_item_vector_map);
        dataset_ptr->object_to_item_vector_map = NULL;
    }

    if (dataset_ptr->object_to_properties_map != NULL)
    {
        const uint32_t n_entries = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_properties_map);

        for (uint32_t n_entry = 0;
                      n_entry < n_entries;
                    ++n_entry)
        {
            _lw_curve_dataset_object* item_ptr = NULL;

            if (!system_hash64map_get_element_at(dataset_ptr->object_to_properties_map,
                                                 n_entry,
                                                &item_ptr,
                                                 NULL) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Hash map getter failed.");
            }
            else
            {
                delete item_ptr;
                item_ptr = NULL;
            }
        } /* for (all entries) */

        system_hash64map_release(dataset_ptr->object_to_properties_map);
        dataset_ptr->object_to_properties_map = NULL;
    }

    if (dataset_ptr->oneCurve != NULL)
    {
        curve_container_release(dataset_ptr->oneCurve);

        dataset_ptr->oneCurve = NULL;
    }

    if (dataset_ptr->zeroCurve != NULL)
    {
        curve_container_release(dataset_ptr->zeroCurve);

        dataset_ptr->zeroCurve = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_add_curve(__in __notnull lw_curve_dataset             dataset,
                                                   __in __notnull system_hashed_ansi_string    object_name,
                                                   __in           lw_curve_dataset_object_type object_type,
                                                   __in __notnull system_hashed_ansi_string    curve_name,
                                                   __in __notnull curve_container              curve)
{
    _lw_curve_dataset*      dataset_ptr      = (_lw_curve_dataset*) dataset;
    system_resizable_vector item_vector      = NULL;
    const system_hash64     object_name_hash = system_hashed_ansi_string_get_hash(object_name);

    if (!system_hash64map_get(dataset_ptr->object_to_item_vector_map,
                              system_hashed_ansi_string_get_hash(object_name),
                             &item_vector) )
    {
        /* Create a vector entry */
        item_vector = system_resizable_vector_create(4, /* capacity */
                                                     sizeof(_lw_curve_dataset_item*) );

        ASSERT_DEBUG_SYNC(item_vector != NULL,
                          "Could not create a resizable vector");

        system_hash64map_insert(dataset_ptr->object_to_item_vector_map,
                                object_name_hash,
                                item_vector,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */

        /* Spawn a new object properties descriptor */
        _lw_curve_dataset_object* object_ptr = new (std::nothrow) _lw_curve_dataset_object;

        ASSERT_DEBUG_SYNC(object_ptr != NULL,
                          "Out of memory");
        if (object_ptr == NULL)
        {
            goto end;
        }

        object_ptr->object_type = object_type;

        /* Store it */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(dataset_ptr->object_to_properties_map,
                                                     object_name_hash),
                          "The same name is likely used for objects of different types."
                          " This is unsupported at the moment.");

        system_hash64map_insert(dataset_ptr->object_to_properties_map,
                                object_name_hash,
                                object_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }
    else
    {
        _lw_curve_dataset_object* object_ptr = NULL;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(dataset_ptr->object_to_properties_map,
                                                    object_name_hash),
                          "Object has not been recognized.");

        system_hash64map_get(dataset_ptr->object_to_properties_map,
                             object_name_hash,
                            &object_ptr);

        ASSERT_DEBUG_SYNC(object_ptr->object_type == object_type,
                          "Object types do not match");
    }

    /* Spawn a new descriptor */
    _lw_curve_dataset_item* item_ptr = new (std::nothrow) _lw_curve_dataset_item;

    ASSERT_ALWAYS_SYNC(item_ptr != NULL, "Out of memory");
    if (item_ptr == NULL)
    {
        goto end;
    }

    item_ptr->curve       = curve;
    item_ptr->name        = curve_name;
    item_ptr->object_name = object_name;

    curve_container_retain(curve);

    /* Add the new item */
    system_resizable_vector_push(item_vector,
                                 item_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_apply_to_scene(__in __notnull lw_curve_dataset dataset,
                                                        __in           scene            scene)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) dataset;
    const uint32_t     n_objects   = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_item_vector_map);

    ASSERT_DEBUG_SYNC(n_objects == system_hash64map_get_amount_of_elements(dataset_ptr->object_to_properties_map),
                      "Map corruption detected");

    /* Retrieve scene graph */
    scene_graph graph = NULL;

    scene_get_property(scene,
                       SCENE_PROPERTY_GRAPH,
                      &graph);

    /* Iterate over all objects we have curves for. */
    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        _lw_curve_dataset_item*   item_ptr    = NULL;
        system_hash64             item_hash   = -1;
        system_resizable_vector   item_vector = NULL;
        _lw_curve_dataset_object* object_ptr  = NULL;

        /* Retrieve descriptors */
        if (!system_hash64map_get_element_at(dataset_ptr->object_to_item_vector_map,
                                             n_object,
                                            &item_vector,
                                            &item_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve curve vector for object at index [%d]",
                              n_object);

            goto end;
        }

        if (!system_hash64map_get(dataset_ptr->object_to_properties_map,
                                  item_hash,
                                 &object_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve object descriptor.");

            goto end;
        }

        /* Iterate over all curves defined for the object */
        const uint32_t n_object_curves = system_resizable_vector_get_amount_of_elements(item_vector);

        for (uint32_t n_object_curve = 0;
                      n_object_curve < n_object_curves;
                    ++n_object_curve)
        {
            if (!system_resizable_vector_get_element_at(item_vector,
                                                        n_object_curve,
                                                       &item_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve item descriptor at index [%d] for object at index [%d]",
                                  n_object_curve,
                                  n_object);

                goto end;
            }

            /* Match the curve name to one of the predefined curve types */
            static const system_hashed_ansi_string position_x_curve_name = system_hashed_ansi_string_create("Position.X");
            static const system_hashed_ansi_string position_y_curve_name = system_hashed_ansi_string_create("Position.Y");
            static const system_hashed_ansi_string position_z_curve_name = system_hashed_ansi_string_create("Position.Z");
            static const system_hashed_ansi_string rotation_b_curve_name = system_hashed_ansi_string_create("Rotation.B");
            static const system_hashed_ansi_string rotation_h_curve_name = system_hashed_ansi_string_create("Rotation.H");
            static const system_hashed_ansi_string rotation_p_curve_name = system_hashed_ansi_string_create("Rotation.P");
            static const system_hashed_ansi_string scale_x_curve_name    = system_hashed_ansi_string_create("Scale.X");
            static const system_hashed_ansi_string scale_y_curve_name    = system_hashed_ansi_string_create("Scale.Y");
            static const system_hashed_ansi_string scale_z_curve_name    = system_hashed_ansi_string_create("Scale.Z");

            system_hash64  curve_name_hash = system_hashed_ansi_string_get_hash(item_ptr->name);
            _lw_curve_type curve_type      = LW_CURVE_TYPE_UNDEFINED;

            if (curve_name_hash == system_hashed_ansi_string_get_hash(position_x_curve_name) ) curve_type = LW_CURVE_TYPE_POSITION_X;else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(position_y_curve_name) ) curve_type = LW_CURVE_TYPE_POSITION_Y;else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(position_z_curve_name) ) curve_type = LW_CURVE_TYPE_POSITION_Z;else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(rotation_b_curve_name) ) curve_type = LW_CURVE_TYPE_ROTATION_B;else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(rotation_h_curve_name) ) curve_type = LW_CURVE_TYPE_ROTATION_H;else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(rotation_p_curve_name) ) curve_type = LW_CURVE_TYPE_ROTATION_P;else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(scale_x_curve_name) )    curve_type = LW_CURVE_TYPE_SCALE_X;   else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(scale_y_curve_name) )    curve_type = LW_CURVE_TYPE_SCALE_Y;   else
            if (curve_name_hash == system_hashed_ansi_string_get_hash(scale_z_curve_name) )    curve_type = LW_CURVE_TYPE_SCALE_Z;   else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized curve type");
            }

            /* Retrieve a graph node that owns the object */
            unsigned int     n_lw_name_iterations = 0;
            scene_graph_node owner_node           = NULL;

            _lw_curve_dataset_get_lw_adjusted_object_name(0, /* n_iteration */
                                                         &n_lw_name_iterations,
                                                          NULL);

            switch (object_ptr->object_type)
            {
                case LW_CURVE_DATASET_OBJECT_TYPE_CAMERA:
                {
                    scene_camera found_camera = NULL;

                    for (unsigned int n_lw_name_iteration = 0;
                                      n_lw_name_iteration < n_lw_name_iterations && found_camera == NULL;
                                    ++n_lw_name_iteration)
                    {
                        found_camera = scene_get_camera_by_name(scene,
                                                                _lw_curve_dataset_get_lw_adjusted_object_name(n_lw_name_iteration,
                                                                                                              NULL,
                                                                                                              item_ptr->object_name) );
                    }

                    if (found_camera != NULL)
                    {
                        owner_node = scene_graph_get_node_for_object(graph,
                                                                     SCENE_OBJECT_TYPE_CAMERA,
                                                                     found_camera);
                    }
                    else
                    {
                        LOG_ERROR("Could not find a camera named [%s] - it will not have its curve adjusted.",
                                  system_hashed_ansi_string_get_buffer(item_ptr->object_name) );

                        continue;
                    }

                    break;
                }

                case LW_CURVE_DATASET_OBJECT_TYPE_LIGHT:
                {
                    scene_light found_light = NULL;

                    for (unsigned int n_lw_name_iteration = 0;
                                      n_lw_name_iteration < n_lw_name_iterations && found_light == NULL;
                                    ++n_lw_name_iteration)
                    {
                        found_light = scene_get_light_by_name(scene,
                                                              _lw_curve_dataset_get_lw_adjusted_object_name(n_lw_name_iteration,
                                                                                                            NULL,
                                                                                                            item_ptr->object_name) );
                    }

                    if (found_light != NULL)
                    {
                        owner_node = scene_graph_get_node_for_object(graph,
                                                                     SCENE_OBJECT_TYPE_LIGHT,
                                                                     found_light);
                    }
                    else
                    {
                        LOG_ERROR("Could not find a light named [%s] - it will not have its curve adjusted.",
                                  system_hashed_ansi_string_get_buffer(item_ptr->object_name) );

                        continue;
                    }

                    break;
                }

                case LW_CURVE_DATASET_OBJECT_TYPE_MESH:
                {
                    scene_mesh found_mesh = NULL;

                    for (unsigned int n_lw_name_iteration = 0;
                                      n_lw_name_iteration < n_lw_name_iterations && found_mesh == NULL;
                                    ++n_lw_name_iteration)
                    {
                        found_mesh = scene_get_mesh_instance_by_name(scene,
                                                                     _lw_curve_dataset_get_lw_adjusted_object_name(n_lw_name_iteration,
                                                                                                                   NULL,
                                                                                                                   item_ptr->object_name) );
                    }

                    if (found_mesh != NULL)
                    {
                        owner_node = scene_graph_get_node_for_object(graph,
                                                                     SCENE_OBJECT_TYPE_MESH,
                                                                     found_mesh);
                    }
                    else
                    {
                        LOG_ERROR("Could not find a mesh named [%s] - object will not have its curve adjusted.",
                                  system_hashed_ansi_string_get_buffer(item_ptr->object_name) );

                        continue;
                    }

                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unrecognized LW curve data-set object type");
                }
            } /* switch (object_ptr->object_type) */

            ASSERT_ALWAYS_SYNC(owner_node != NULL,
                               "Could not identify owner node for scene object");

            if (owner_node != NULL)
            {
                /* Owner node is just a placeholder for the items. Make sure the node tag is valid */
                scene_graph_node_tag node_tag = SCENE_GRAPH_NODE_TAG_UNDEFINED;

                scene_graph_node_get_property(owner_node,
                                              SCENE_GRAPH_NODE_PROPERTY_TAG,
                                             &node_tag);

                ASSERT_DEBUG_SYNC(node_tag == SCENE_GRAPH_NODE_TAG_UNDEFINED,
                                  "Node that references items should be a placeholder node!");

                /* Depending on the curve type, we need to retrieve node with a rotation/scale/translation
                 * tag, which actually controls the transformation.
                 */
                _lw_curve_dataset_apply_to_node(dataset_ptr,
                                                curve_type,
                                                item_ptr->curve,
                                                graph,
                                                owner_node);
            } /* if (owner_node != NULL) */
        } /* for (all object curves) */
    } /* for (all objects) */

    /* Also lock the FPS limiter to what the test scene uses */
    scene_set_property(scene,
                       SCENE_PROPERTY_FPS,
                      &dataset_ptr->fps);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    /* Instantiate the object */
    _lw_curve_dataset* result_instance = new (std::nothrow) _lw_curve_dataset;

    ASSERT_DEBUG_SYNC(result_instance != NULL, "Out of memory");
    if (result_instance == NULL)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    /* Set up the fields */
    memset(result_instance,
           0,
           sizeof(*result_instance) );

    result_instance->fps                       = 0.0f;
    result_instance->object_to_item_vector_map = system_hash64map_create(sizeof(_lw_curve_dataset_item*) );
    result_instance->object_to_properties_map  = system_hash64map_create(sizeof(_lw_curve_dataset_object*) );

    /* Set up helper fields */
    system_variant helper_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    result_instance->oneCurve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                               " one curve"),
                                                       SYSTEM_VARIANT_FLOAT);
    result_instance->zeroCurve = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                " zero curve"),
                                                        SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (helper_variant,
                                      1.0f);
    curve_container_set_default_value(result_instance->oneCurve,
                                      helper_variant);

    system_variant_set_float         (helper_variant,
                                      0.0f);
    curve_container_set_default_value(result_instance->zeroCurve,
                                      helper_variant);

    system_variant_release(helper_variant);
    helper_variant = NULL;

    /* Set up reference counting */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_instance,
                                                   _lw_curve_dataset_release,
                                                   OBJECT_TYPE_LW_CURVE_DATASET,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave curve data-sets\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (lw_curve_dataset) result_instance;

end:
    if (result_instance != NULL)
    {
        _lw_curve_dataset_release(result_instance);

        delete result_instance;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_get_property(__in  __notnull lw_curve_dataset          dataset,
                                                      __in            lw_curve_dataset_property property,
                                                      __out __notnull void*                     out_result)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) dataset;

    switch (property)
    {
        case LW_CURVE_DATASET_PROPERTY_FPS:
        {
            *(float*) out_result = dataset_ptr->fps;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_curve_dataset_property value");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                          __in __notnull system_file_serializer    serializer)
{
    lw_curve_dataset result = lw_curve_dataset_create(name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "lw_curve_dataset_create() failed.");
    if (result == NULL)
    {
        goto end;
    }

    /* Load the objects */
    _lw_curve_dataset* result_ptr   = (_lw_curve_dataset*) result;
    uint32_t           n_objects    = 0;
    system_hash64map   object_props = system_hash64map_create(sizeof(_lw_curve_dataset_object*) );

    system_file_serializer_read(serializer,
                                sizeof(result_ptr->fps),
                               &result_ptr->fps);
    system_file_serializer_read(serializer,
                                sizeof(n_objects),
                               &n_objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        uint32_t                  n_items      = 0;
        system_resizable_vector   object_items = system_resizable_vector_create(4, /* capacity */
                                                                                sizeof(_lw_curve_dataset_item*) );
        system_hashed_ansi_string object_name  = NULL; /* this gets set when loading data of the first defined item */

        system_file_serializer_read(serializer,
                                    sizeof(n_items),
                                   &n_items);

        ASSERT_DEBUG_SYNC(n_items != 0,
                          "Zero items defined for a LW curve data set object");

        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            if (n_item == 0)
            {
                lw_curve_dataset_object_type object_type = LW_CURVE_DATASET_OBJECT_TYPE_UNDEFINED;

                system_file_serializer_read_hashed_ansi_string(serializer,
                                                              &object_name);
                system_file_serializer_read                   (serializer,
                                                               sizeof(object_type),
                                                              &object_type);

                ASSERT_DEBUG_SYNC(object_name != NULL,
                                  "Object name remains undefined");

                /* Create a prop descriptor */
                system_hash64             object_name_hash = system_hashed_ansi_string_get_hash(object_name);
                _lw_curve_dataset_object* object_ptr       = NULL;

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(object_props,
                                                             object_name_hash),
                                  "Props hash-map already stores properties for the object");

                object_ptr = new (std::nothrow) _lw_curve_dataset_object;

                ASSERT_ALWAYS_SYNC(object_ptr != NULL,
                                   "Out of memory");
                if (object_ptr == NULL)
                {
                    goto end_error;
                }

                object_ptr->object_type = object_type;

                /* Store it */
                system_hash64map_insert(object_props,
                                        object_name_hash,
                                        object_ptr,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            }

            /* Read the item data */
            curve_container           item_curve = NULL;
            system_hashed_ansi_string item_name  = NULL;

            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &item_name);
            system_file_serializer_read_curve_container   (serializer,
                                                          &item_curve);

            /* Spawn the descriptor */
            _lw_curve_dataset_item* item_ptr = new (std::nothrow) _lw_curve_dataset_item;

            ASSERT_ALWAYS_SYNC(item_ptr != NULL,
                               "Out of memory");
            if (item_ptr == NULL)
            {
                goto end_error;
            }

            item_ptr->curve       = item_curve;
            item_ptr->name        = item_name;
            item_ptr->object_name = object_name;

            /* Store the item */
            system_resizable_vector_push(object_items, item_ptr);
        } /* for (all items) */

        /* Store the vector */
        const system_hash64 object_name_hash = system_hashed_ansi_string_get_hash(object_name);

        system_hash64map_insert(result_ptr->object_to_item_vector_map,
                                object_name_hash,
                                object_items,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    } /* for (all objects) */

    result_ptr->object_to_properties_map = object_props;

    goto end;

end_error:
    if (result != NULL)
    {
        lw_curve_dataset_release(result);

        result = NULL;
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_save(__in __notnull lw_curve_dataset       dataset,
                                              __in __notnull system_file_serializer serializer)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) dataset;

    /* Store the miscellaneous properties */
    system_file_serializer_write(serializer,
                                 sizeof(dataset_ptr->fps),
                                &dataset_ptr->fps);

    /* Store the map entries */
    const uint32_t n_objects = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_item_vector_map);

    system_file_serializer_write(serializer,
                                 sizeof(n_objects),
                                &n_objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        system_resizable_vector items   = NULL;
        uint32_t                n_items = 0;

        if (!system_hash64map_get_element_at(dataset_ptr->object_to_item_vector_map,
                                             n_object,
                                            &items,
                                             NULL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve object vector");

            goto end;
        }

        n_items = system_resizable_vector_get_amount_of_elements(items);

        system_file_serializer_write(serializer,
                                     sizeof(uint32_t),
                                    &n_items);

        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            _lw_curve_dataset_item* item_ptr = NULL;

            if (!system_resizable_vector_get_element_at(items,
                                                        n_item,
                                                       &item_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve item descriptor");

                goto end;
            }

            /* Write the object name & its type, if this is the first item */
            if (n_item == 0)
            {
                _lw_curve_dataset_object* object_ptr = NULL;

                system_hash64map_get(dataset_ptr->object_to_properties_map,
                                     system_hashed_ansi_string_get_hash(item_ptr->object_name),
                                    &object_ptr);

                ASSERT_DEBUG_SYNC(object_ptr != NULL,
                                  "Could not find object descriptor");

                system_file_serializer_write_hashed_ansi_string(serializer,
                                                                item_ptr->object_name);
                system_file_serializer_write                   (serializer,
                                                                sizeof(object_ptr->object_type),
                                                               &object_ptr->object_type);
            }

            /* Write the item data */
            system_file_serializer_write_hashed_ansi_string(serializer,
                                                            item_ptr->name);
            system_file_serializer_write_curve_container   (serializer,
                                                            item_ptr->curve);
        } /* for (all items) */
    } /* for (all objects) */

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_set_property(__in __notnull lw_curve_dataset          dataset,
                                                      __in           lw_curve_dataset_property property,
                                                      __in __notnull void*                     data)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) dataset;

    switch (property)
    {
        case LW_CURVE_DATASET_PROPERTY_FPS:
        {
            dataset_ptr->fps = *(float*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_curve_dataset_property value");
        }
    } /* switch (property) */
}
