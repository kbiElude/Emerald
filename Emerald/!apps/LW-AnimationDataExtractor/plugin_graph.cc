#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_cameras.h"
#include "plugin_common.h"
#include "plugin_graph.h"
#include "plugin_lights.h"
#include "plugin_meshes.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "scene/scene_types.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_variant.h"

system_hash64map object_id_to_scene_graph_node_map = NULL;


/** TODO */
PRIVATE bool AddObjectToSceneGraph(__in           _scene_object_type object_type,
                                   __in           scene_graph        graph,
                                   __in __notnull curve_container    zero_curve,
                                   __in __notnull curve_container    one_curve,
                                   __in __notnull curve_container    minus_one_curve,
                                   __in __notnull void*              object)
{
    scene_camera     camera        = (scene_camera) object;
    scene_light      light         = (scene_light)  object;
    scene_mesh       mesh_instance = (scene_mesh)   object;
    void*            object_id     = NULL;
    scene_graph_node object_node   = NULL;
    scene_graph_node parent_node   = NULL;
    bool             result        = true;

    /* Is the parent node available? */
    switch (object_type)
    {
        case SCENE_OBJECT_TYPE_CAMERA:
        case SCENE_OBJECT_TYPE_LIGHT:
        case SCENE_OBJECT_TYPE_MESH:
        {
            void* parent_object_id = NULL;

            if (object_type == SCENE_OBJECT_TYPE_CAMERA)
            {
                GetCameraPropertyValue(camera,
                                       CAMERA_PROPERTY_OBJECT_ID,
                                      &object_id);
                GetCameraPropertyValue(camera,
                                       CAMERA_PROPERTY_PARENT_OBJECT_ID,
                                      &parent_object_id);
            }
            else
            if (object_type == SCENE_OBJECT_TYPE_LIGHT)
            {
                GetLightPropertyValue(light,
                                      LIGHT_PROPERTY_OBJECT_ID,
                                     &object_id);
                GetLightPropertyValue(light,
                                      LIGHT_PROPERTY_PARENT_OBJECT_ID,
                                     &parent_object_id);
            }
            else
            {
                GetMeshProperty(mesh_instance,
                                MESH_PROPERTY_OBJECT_ID,
                               &object_id);
                GetMeshProperty(mesh_instance,
                                MESH_PROPERTY_PARENT_OBJECT_ID,
                               &parent_object_id);
            }

            if (parent_object_id == NULL)
            {
                parent_node = scene_graph_get_root_node(graph);

                break;
            }
            else
            {
                if (!system_hash64map_get(object_id_to_scene_graph_node_map,
                                          (system_hash64) parent_object_id,
                                         &parent_node) )
                {
                    /* Parent object has not been added yet */
                    result = false;

                    goto end;
                }
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported scene object type");
        }
    } /* switch (object_type) */

    /* Add a new general node to the graph. */
    object_node = scene_graph_create_general_node(graph);

    scene_graph_add_node(graph,
                         parent_node,
                         object_node);

    /* Prepare rotation transformation curves for the graph. scene_graph takes
     * a vector of 4 curves, where the first three curves represent the rotation
     * axis, and the fourth curve tells the angle.
     */
    curve_container rotation_y[4] = {NULL, zero_curve,      minus_one_curve, zero_curve};
    curve_container rotation_x[4] = {NULL, minus_one_curve, zero_curve,      zero_curve};
    curve_container rotation_z[4] = {NULL, zero_curve,      zero_curve,      one_curve};

    /* Add transformations */
    float*    object_pivot                 = NULL;
    curve_id* object_rotation_curve_ids    = NULL;
    curve_id* object_translation_curve_ids = NULL;

    switch(object_type)
    {
        case SCENE_OBJECT_TYPE_CAMERA:
        {
            GetCameraPropertyValue(camera,
                                   CAMERA_PROPERTY_PIVOT,
                                  &object_pivot);
            GetCameraPropertyValue(camera,
                                   CAMERA_PROPERTY_ROTATION_HPB_CURVE_IDS,
                                  &object_rotation_curve_ids);
            GetCameraPropertyValue(camera,
                                   CAMERA_PROPERTY_TRANSLATION_CURVE_IDS,
                                  &object_translation_curve_ids);

            break;
        }

        case SCENE_OBJECT_TYPE_LIGHT:
        {
            GetLightPropertyValue(light,
                                  LIGHT_PROPERTY_PIVOT,
                                 &object_pivot);
            GetLightPropertyValue(light,
                                  LIGHT_PROPERTY_ROTATION_HPB_CURVE_IDS,
                                 &object_rotation_curve_ids);
            GetLightPropertyValue(light,
                                  LIGHT_PROPERTY_TRANSLATION_CURVE_IDS,
                                 &object_translation_curve_ids);

            break;
        }

        case SCENE_OBJECT_TYPE_MESH:
        {
            GetMeshProperty(mesh_instance,
                            MESH_PROPERTY_PIVOT,
                           &object_pivot);
            GetMeshProperty(mesh_instance,
                            MESH_PROPERTY_ROTATION_HPB_CURVE_IDS,
                           &object_rotation_curve_ids);
            GetMeshProperty(mesh_instance,
                            MESH_PROPERTY_TRANSLATION_CURVE_IDS,
                           &object_translation_curve_ids);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized object type");
        }
    } /* switch (object_type) */

    /* Prepare rotation vectors */
    system_hash64map envelope_id_to_curve_container_map = GetEnvelopeIDToCurveContainerHashMap();

    system_hash64map_get(envelope_id_to_curve_container_map,
                         (system_hash64) object_rotation_curve_ids[0],
                         rotation_y + 0);
    system_hash64map_get(envelope_id_to_curve_container_map,
                         (system_hash64) object_rotation_curve_ids[1],
                         rotation_x + 0);
    system_hash64map_get(envelope_id_to_curve_container_map,
                         (system_hash64) object_rotation_curve_ids[2],
                         rotation_z + 0);

    /* Transformation: translation. */
    if (object_translation_curve_ids[0] != NULL &&
        object_translation_curve_ids[1] != NULL &&
        object_translation_curve_ids[2] != NULL)
    {
        curve_container object_translation_curves[3] = {NULL};
        const bool      negate_xyz_vectors       []  =
        {
            false,
            false,
            true
        };
        scene_graph_node translation_node = NULL;

        for (unsigned int n_component = 0;
                          n_component < 3;
                        ++n_component)
        {
            system_hash64map_get(envelope_id_to_curve_container_map,
                                 (system_hash64) object_translation_curve_ids[n_component],
                                 object_translation_curves + n_component);
        } /* for (all components) */

        translation_node = scene_graph_create_translation_dynamic_node(graph,
                                                                       object_translation_curves,
                                                                       negate_xyz_vectors,
                                                                       SCENE_GRAPH_NODE_TAG_TRANSLATE);

        scene_graph_add_node(graph,
                             object_node,
                             translation_node);

        object_node = translation_node;
    }

    /* Transformation: pivot translation <if necessary> */
    if (fabs(object_pivot[0]) > 1e-5f ||
        fabs(object_pivot[1]) > 1e-5f ||
        fabs(object_pivot[2]) > 1e-5f)
    {
        scene_graph_node pivot_translation_node = scene_graph_create_translation_static_node(graph,
                                                                                             object_pivot,
                                                                                             SCENE_GRAPH_NODE_TAG_UNDEFINED);

        scene_graph_add_node(graph,
                             object_node,
                             pivot_translation_node);

        object_node = pivot_translation_node;
    }

    /* Transformation: rotations */
    if (rotation_y[0] != NULL &&
        rotation_y[1] != NULL &&
        rotation_y[2] != NULL &&
        rotation_y[3] != NULL)
    {
        scene_graph_node rotation_node = scene_graph_create_rotation_dynamic_node(graph,
                                                                                  rotation_y,
                                                                                  true,              /* expressed_in_radians */
                                                                                  SCENE_GRAPH_NODE_TAG_ROTATE_Y);

        scene_graph_add_node(graph,
                             object_node,
                             rotation_node);

        object_node = rotation_node;
    }

    if (rotation_x[0] != NULL &&
        rotation_x[1] != NULL &&
        rotation_x[2] != NULL &&
        rotation_x[3] != NULL)
    {
        scene_graph_node rotation_node = scene_graph_create_rotation_dynamic_node(graph,
                                                                                  rotation_x,
                                                                                  true,              /* expressed_in_radians */
                                                                                  SCENE_GRAPH_NODE_TAG_ROTATE_X);

        scene_graph_add_node(graph,
                             object_node,
                             rotation_node);

        object_node = rotation_node;
    }

    if (rotation_z[0] != NULL &&
        rotation_z[1] != NULL &&
        rotation_z[2] != NULL &&
        rotation_z[3] != NULL)
    {
        scene_graph_node rotation_node = scene_graph_create_rotation_dynamic_node(graph,
                                                                                  rotation_z,
                                                                                  true,              /* expressed_in_radians */
                                                                                  SCENE_GRAPH_NODE_TAG_ROTATE_Z);

        scene_graph_add_node(graph,
                             object_node,
                             rotation_node);

        object_node = rotation_node;
    }

    /* Transformation: cancel pivot translation <if necessary> */
    if (fabs(object_pivot[0]) > 1e-5f ||
        fabs(object_pivot[1]) > 1e-5f ||
        fabs(object_pivot[2]) > 1e-5f)
    {
        object_pivot[0] = -object_pivot[0];
        object_pivot[1] = -object_pivot[1];
        object_pivot[2] = -object_pivot[2];

        scene_graph_node pivot_translation_node = scene_graph_create_translation_static_node(graph,
                                                                                             object_pivot,
                                                                                             SCENE_GRAPH_NODE_TAG_UNDEFINED);

        scene_graph_add_node(graph,
                             object_node,
                             pivot_translation_node);

        object_node = pivot_translation_node;
    }

    /* TODO: Scaling */

    /* Push the object */
    scene_graph_attach_object_to_node(graph,
                                      object_node,
                                      object_type,
                                      object);

    /* Store the node in internal map */
    ASSERT_DEBUG_SYNC(!system_hash64map_contains(object_id_to_scene_graph_node_map,
                                                 (system_hash64) object_id),
                      "Object already stored in the hash-map!");

    system_hash64map_insert(object_id_to_scene_graph_node_map,
                            (system_hash64) object_id,
                            object_node,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */

end:
    return result;
}

/** Please see header for spec */
PUBLIC void DeinitGraphData()
{
    if (object_id_to_scene_graph_node_map != NULL)
    {
        system_hash64map_release(object_id_to_scene_graph_node_map);

        object_id_to_scene_graph_node_map = NULL;
    }
}

/** Please see header for spec */
PUBLIC void FillSceneGraphData(__in __notnull scene in_scene)
{
    scene_graph      graph     = NULL;
    scene_graph_node root_node = NULL;

    scene_get_property(in_scene,
                       SCENE_PROPERTY_GRAPH,
                      &graph);

    root_node = scene_graph_get_root_node(graph);

    /* Initialize helper objects */
    system_variant  float_variant   = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_container minus_one_curve = curve_container_create(system_hashed_ansi_string_create("Minus one curve"),
                                                             SYSTEM_VARIANT_FLOAT);
    curve_container one_curve       = curve_container_create(system_hashed_ansi_string_create("One curve"),
                                                             SYSTEM_VARIANT_FLOAT);
    curve_container zero_curve      = curve_container_create(system_hashed_ansi_string_create("Zero curve"),
                                                             SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (float_variant,   -1.0f);
    curve_container_set_default_value(minus_one_curve, float_variant);

    system_variant_set_float         (float_variant, 1.0f);
    curve_container_set_default_value(one_curve,     float_variant);

    system_variant_set_float         (float_variant, 0.0f);
    curve_container_set_default_value(zero_curve,    float_variant);

    AddCurveContainerToEnvelopeIDToCurveContainerHashMap(minus_one_curve);
    AddCurveContainerToEnvelopeIDToCurveContainerHashMap(one_curve);
    AddCurveContainerToEnvelopeIDToCurveContainerHashMap(zero_curve);

    /* Add objects */
    uint32_t                 n_cameras            = 0;
    uint32_t                 n_lights             = 0;
    uint32_t                 n_mesh_instances     = 0;
    uint32_t                 n_total_objects      = 0;
    const _scene_object_type scene_object_types[] =
    {
        SCENE_OBJECT_TYPE_CAMERA,
        SCENE_OBJECT_TYPE_LIGHT,
        SCENE_OBJECT_TYPE_MESH
    };
    const uint32_t n_scene_object_types = sizeof(scene_object_types) / sizeof(scene_object_types[0]);

    scene_get_property(in_scene,
                        SCENE_PROPERTY_N_CAMERAS,
                       &n_cameras);
    scene_get_property(in_scene,
                       SCENE_PROPERTY_N_LIGHTS,
                      &n_lights);
    scene_get_property(in_scene,
                       SCENE_PROPERTY_N_MESH_INSTANCES,
                      &n_mesh_instances);

    n_total_objects = n_cameras +
                      n_lights  +
                      n_mesh_instances;

    /* Keep looping until all objects are inserted into the graph.
     *
     * NOTE: Objects of type A can have a parent object of type B, which is why
     *       the loop is constructed the way it is below.
     */
    uint32_t n_processed_objects = 0;

    while (n_processed_objects != n_total_objects)
    {
        const uint32_t n_preloop_processed_objects = n_processed_objects;

        for (uint32_t n_scene_object_type = 0;
                      n_scene_object_type < n_scene_object_types;
                    ++n_scene_object_type)
        {
            uint32_t           n_objects   = 0;
            _scene_object_type object_type = scene_object_types[n_scene_object_type];

            switch (object_type)
            {
                case SCENE_OBJECT_TYPE_CAMERA:
                {
                    n_objects = n_cameras;

                    break;
                }

                case SCENE_OBJECT_TYPE_LIGHT:
                {
                    n_objects = n_lights;

                    break;
                }

                case SCENE_OBJECT_TYPE_MESH:
                {
                    n_objects = n_mesh_instances;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized object type");
                }
            } /* switch (object_type) */

            for (uint32_t n_object = 0;
                          n_object < n_objects;
                        ++n_object)
            {
                void*            current_object      = NULL;
                void *           current_object_id   = NULL;
                scene_graph_node current_object_node = NULL;

                /* Retrieve object handle */
                switch (object_type)
                {
                    case SCENE_OBJECT_TYPE_CAMERA:
                    {
                        current_object = scene_get_camera_by_index(in_scene,
                                                                   n_object);

                        GetCameraPropertyValue( (scene_camera) current_object,
                                               CAMERA_PROPERTY_OBJECT_ID,
                                              &current_object_id);

                        break;
                    }

                    case SCENE_OBJECT_TYPE_LIGHT:
                    {
                        current_object = scene_get_light_by_index(in_scene,
                                                                  n_object);

                        GetLightPropertyValue( (scene_light) current_object,
                                              LIGHT_PROPERTY_OBJECT_ID,
                                             &current_object_id);

                        break;
                    }

                    case SCENE_OBJECT_TYPE_MESH:
                    {
                        current_object = scene_get_mesh_instance_by_index(in_scene,
                                                                          n_object);

                        GetMeshProperty( (scene_mesh) current_object,
                                        MESH_PROPERTY_OBJECT_ID,
                                       &current_object_id);

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized object type");
                    }
                } /* switch (object_type) */

                if (current_object == NULL)
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Could not retrieve object of type [%d] at index [%d]",
                                       n_scene_object_type,
                                       n_object);

                    continue;
                }

                /* Try to add the object to the graph, if not already mapped to
                 * a scene graph node.
                 */
                if (!system_hash64map_contains(object_id_to_scene_graph_node_map,
                                               (system_hash64) current_object_id) )
                {
                    bool has_added = AddObjectToSceneGraph(object_type,
                                                           graph,
                                                           zero_curve,
                                                           one_curve,
                                                           minus_one_curve,
                                                           current_object);

                    if (has_added)
                    {
                        n_processed_objects++;
                    }
                }
            } /* for (all objects) */
        } /* for (all object types) */

        ASSERT_DEBUG_SYNC(n_preloop_processed_objects != n_processed_objects,
                  "Processing loop lock-up!");
        ASSERT_DEBUG_SYNC(n_preloop_processed_objects < n_total_objects,
                          "WTF?");
    } /* while (n_processed_objects != n_objects) */

    /* Release helper objects */
    if (float_variant != NULL)
    {
        system_variant_release(float_variant);

        float_variant = NULL;
    }

    if (minus_one_curve != NULL)
    {
        curve_container_release(minus_one_curve);

        minus_one_curve = NULL;
    }

    if (one_curve != NULL)
    {
        curve_container_release(one_curve);

        one_curve = NULL;
    }

    if (zero_curve != NULL)
    {
        curve_container_release(zero_curve);

        zero_curve = NULL;
    }
}

/** Please see header for spec */
PUBLIC void InitGraphData()
{
    object_id_to_scene_graph_node_map = system_hash64map_create(sizeof(scene_graph_node) );
}
