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
#include "system/system_resizable_vector.h"
#include "system/system_variant.h"

/** TODO */
PRIVATE void AddObjectToSceneGraph(__in           _scene_object_type object_type,
                                   __in           scene_graph        graph,
                                   __in __notnull scene_graph_node   parent_node,
                                   __in __notnull curve_container    zero_curve,
                                   __in __notnull curve_container    one_curve,
                                   __in __notnull void*              object)
{
    scene_camera     camera        = (scene_camera) object;
    scene_light      light         = (scene_light)  object;
    scene_mesh       mesh_instance = (scene_mesh)   object;
    scene_graph_node object_node   = scene_graph_create_general_node(graph);

    scene_graph_add_node(graph,
                         parent_node,
                         object_node);

    /* Prepare rotation transformation curves for the graph. scene_graph takes
     * a vector of 4 curves, where the first three curves represent the rotation
     * axis, nd the fourth curve tells the angle.
     */
    curve_container rotation_y[4] = {zero_curve, one_curve,  zero_curve, NULL};
    curve_container rotation_x[4] = {one_curve,  zero_curve, zero_curve, NULL};
    curve_container rotation_z[4] = {zero_curve, zero_curve, one_curve,  NULL};

    /* Add transformations */
    curve_container* object_rotations    = NULL;
    curve_container* object_translations = NULL;

    switch(object_type)
    {
        case SCENE_OBJECT_TYPE_CAMERA:
        {
            GetCameraPropertyValue(camera,
                                   CAMERA_PROPERTY_ROTATION_HPB_CURVES,
                                  &object_rotations);
            GetCameraPropertyValue(camera,
                                   CAMERA_PROPERTY_TRANSLATION_CURVES,
                                  &object_translations);

            break;
        }

        case SCENE_OBJECT_TYPE_LIGHT:
        {
            GetLightPropertyValue(light,
                                  LIGHT_PROPERTY_ROTATION_HPB_CURVES,
                                 &object_rotations);
            GetLightPropertyValue(light,
                                  LIGHT_PROPERTY_TRANSLATION_CURVES,
                                 &object_translations);

            break;
        }

        case SCENE_OBJECT_TYPE_MESH:
        {
            GetMeshProperty(mesh_instance,
                            MESH_PROPERTY_ROTATION_HPB_CURVES,
                           &object_rotations);
            GetMeshProperty(mesh_instance,
                            MESH_PROPERTY_TRANSLATION_CURVES,
                           &object_translations);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized object type");
        }
    } /* switch (object_type) */

    rotation_y[3] = object_rotations[0];
    rotation_x[3] = object_rotations[1];
    rotation_z[3] = object_rotations[2];

    /* Transformation: translation */
    if (object_translations[0] != NULL &&
        object_translations[1] != NULL &&
        object_translations[2] != NULL)
    {
        scene_graph_node translation_node = scene_graph_create_translation_dynamic_node(graph,
                                                                                        object_translations,
                                                                                        SCENE_GRAPH_NODE_TAG_TRANSLATE);

        scene_graph_add_node(graph,
                             object_node,
                             translation_node);

        object_node = translation_node;
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

    /* Push the object */
    scene_graph_attach_object_to_node(graph,
                                      object_node,
                                      object_type,
                                      object);
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
    system_variant  float_variant = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_container one_curve     = curve_container_create(system_hashed_ansi_string_create("One curve"),
                                                           SYSTEM_VARIANT_FLOAT);
    curve_container zero_curve    = curve_container_create(system_hashed_ansi_string_create("Zero curve"),
                                                           SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (float_variant, 1.0f);
    curve_container_set_default_value(one_curve,     float_variant);

    system_variant_set_float         (float_variant, 0.0f);
    curve_container_set_default_value(zero_curve,    float_variant);

    /* Add cameras.
     *
     * TODO: Parenting support
     */
    uint32_t n_cameras = 0;

    scene_get_property(in_scene,
                       SCENE_PROPERTY_N_CAMERAS,
                      &n_cameras);

    for (uint32_t n_camera = 0;
                  n_camera < n_cameras;
                ++n_camera)
    {
        scene_camera     current_camera      = NULL;
        scene_graph_node current_camera_node = NULL;

        current_camera = scene_get_camera_by_index(in_scene,
                                                   n_camera);

        if (current_camera == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve scene_camera at index [%d]",
                               n_camera);

            continue;
        }

        AddObjectToSceneGraph(SCENE_OBJECT_TYPE_CAMERA,
                              graph,
                              root_node,
                              zero_curve,
                              one_curve,
                              current_camera);
    } /* for (all cameras) */

    /* Add lights
     *
     * TODO: Parenting support.
     **/
    uint32_t n_lights = 0;

    scene_get_property(in_scene,
                       SCENE_PROPERTY_N_LIGHTS,
                      &n_lights);

    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light      current_light      = NULL;
        scene_graph_node current_light_node = NULL;

        current_light = scene_get_light_by_index(in_scene,
                                                 n_light);

        if (current_light == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve scene_light at index [%d]",
                               n_light);

            continue;
        }

        AddObjectToSceneGraph(SCENE_OBJECT_TYPE_LIGHT,
                              graph,
                              root_node,
                              zero_curve,
                              one_curve,
                              current_light);
    } /* for (all lights) */

    /* Add meshes */
    uint32_t n_mesh_instances = 0;

    scene_get_property(in_scene,
                       SCENE_PROPERTY_N_MESH_INSTANCES,
                      &n_mesh_instances);

    for (uint32_t n_mesh_instance = 0;
                  n_mesh_instance < n_mesh_instances;
                ++n_mesh_instance)
    {
        scene_mesh       current_mesh_instance             = NULL;
        scene_graph_node current_mesh_instance_node        = NULL;
        scene_mesh       current_mesh_parent_mesh_instance = NULL;

        current_mesh_instance = scene_get_mesh_instance_by_index(in_scene,
                                                                 n_mesh_instance);

        if (current_mesh_instance == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve scene_mesh at index [%d]",
                               n_mesh_instance);

            continue;
        }

        /* Sanity check */
        GetMeshProperty(current_mesh_instance,
                        MESH_PROPERTY_PARENT_SCENE_MESH,
                       &current_mesh_parent_mesh_instance);

        if (current_mesh_parent_mesh_instance != NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Mesh parenting is not supported yet");
        }

        /* Add the object to the graph */
        AddObjectToSceneGraph(SCENE_OBJECT_TYPE_MESH,
                              graph,
                              root_node,
                              zero_curve,
                              one_curve,
                              current_mesh_instance);
    } /* for (all mesh instances) */

    /* Release helper objects */
    if (float_variant != NULL)
    {
        system_variant_release(float_variant);

        float_variant = NULL;
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


