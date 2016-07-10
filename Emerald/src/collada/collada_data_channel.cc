/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_camera.h"
#include "collada/collada_data_channel.h"
#include "collada/collada_data_sampler.h"
#include "collada/collada_data_scene.h"
#include "collada/collada_data_scene_graph_node_camera_instance.h"
#include "collada/collada_data_scene_graph_node_geometry_instance.h"
#include "collada/collada_data_scene_graph_node_light_instance.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <animation>/<channel> **/
typedef struct _collada_data_channel
{
    collada_data_sampler                  sampler; /* Corresponds to <source> attribute */
    void*                                 target;
    collada_data_channel_target_component target_component;
    collada_data_channel_target_type      target_type;

    _collada_data_channel();
} _collada_data_channel;


/** TODO */
_collada_data_channel::_collada_data_channel()
{
    sampler     = nullptr;
    target      = nullptr;
    target_type = COLLADA_DATA_CHANNEL_TARGET_TYPE_UNKNOWN;
}

/** TODO */
PRIVATE bool _collada_data_channel_get_target(collada_data                           data,
                                              const char*                            path,
                                              void**                                 out_target_ptr,
                                              collada_data_channel_target_type*      out_target_type_ptr,
                                              collada_data_channel_target_component* out_target_component_ptr)

{
    bool result = false;

    collada_data_scene_graph_node         current_node            = nullptr;
    collada_data_scene_graph_node_item    current_node_item       = nullptr;
    void*                                 current_node_item_data  = nullptr;
    _collada_data_node_item_type          current_node_item_type  = COLLADA_DATA_NODE_ITEM_TYPE_UNDEFINED;
    bool                                  has_found               = false;
    system_resizable_vector               nodes                   = system_resizable_vector_create(4 /* capacity */);
    uint32_t                              n_current_node_items    = 0;
    uint32_t                              n_scenes                = 0;
    system_hashed_ansi_string             object_name             = nullptr;
    system_hashed_ansi_string             property_name           = nullptr;
    system_hashed_ansi_string             property_component_name = nullptr;
    collada_data_scene_graph_node         root_node               = nullptr;
    collada_data_scene                    scene                   = nullptr;
    collada_data_scene_graph_node         target_object_node      = nullptr;
    collada_data_channel_target_component target_component        = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_UNKNOWN;
    void*                                 target_property_node    = nullptr;
    collada_data_channel_target_type      target_property_type    = COLLADA_DATA_CHANNEL_TARGET_TYPE_UNKNOWN;
    const char*                           traveller_ptr           = strchr(path,
                                                                           '/');
    const char*                           traveller2_ptr          = nullptr;

    /* Break down the path into actual entities */
    if (traveller_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not identify object name in the channel path");

        goto end;
    }
    else
    {
        object_name = system_hashed_ansi_string_create_substring(path,
                                                                 0,
                                                                 traveller_ptr - path);
    }

    traveller2_ptr = traveller_ptr + 1;
    traveller_ptr  = strchr(traveller2_ptr, '.');

    if (traveller_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not identify property name in the channel path");

        goto end;
    }
    else
    {
        property_name = system_hashed_ansi_string_create_substring(traveller2_ptr,
                                                                   0,
                                                                   traveller_ptr - traveller2_ptr);
    }

    property_component_name = system_hashed_ansi_string_create(traveller_ptr + 1);

    /* Identify the transformation: traverse the scene graph and find the transformation
     * the <channel> node is referring to.
     */
    nodes = system_resizable_vector_create(4 /* capacity */);

    collada_data_get_property(data,
                              COLLADA_DATA_PROPERTY_N_SCENES,
                             &n_scenes);

    ASSERT_DEBUG_SYNC(n_scenes == 1,
                      "COLLADA file describes [%d] scenes, whereas only one scene was expected.",
                      n_scenes);

    collada_data_get_scene(data,
                           0, /* n_scene */
                          &scene);

    ASSERT_DEBUG_SYNC(scene != nullptr,
                      "Could not retrieve scene instance");
    if (scene == nullptr)
    {
        goto end;
    }

    collada_data_scene_get_property(scene,
                                    COLLADA_DATA_SCENE_PROPERTY_ROOT_NODE,
                                   &root_node);

    ASSERT_DEBUG_SYNC(root_node != nullptr,
                      "Could not retrieve root node");

    if (root_node == nullptr)
    {
        goto end;
    }

    /* Good to start traversing */
    system_resizable_vector_push(nodes,
                                 root_node);

    while (target_object_node == nullptr                &&
           system_resizable_vector_pop(nodes,
                                      &current_node) )
    {
        /* Iterate over all node items */
        collada_data_scene_graph_node_get_property(current_node,
                                                   COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_N_NODE_ITEMS,
                                                  &n_current_node_items);

        for (uint32_t n_current_node_item = 0;
                      n_current_node_item < n_current_node_items && (target_object_node== nullptr);
                    ++n_current_node_item)
        {
            collada_data_scene_graph_node_get_node_item(current_node,
                                                        n_current_node_item,
                                                       &current_node_item);

            collada_data_scene_graph_node_item_get_property(current_node_item,
                                                            COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_DATA_HANDLE,
                                                           &current_node_item_data);
            collada_data_scene_graph_node_item_get_property(current_node_item,
                                                            COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_TYPE,
                                                           &current_node_item_type);

            /* Look for a matching node */
            switch (current_node_item_type)
            {
                case COLLADA_DATA_NODE_ITEM_TYPE_NODE:
                {
                    system_hashed_ansi_string sid = nullptr;

                    collada_data_scene_graph_node_get_property( reinterpret_cast<collada_data_scene_graph_node>(current_node_item_data),
                                                                COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_ID,
                                                               &sid);

                    /* Sanity check */
                    ASSERT_DEBUG_SYNC(sid != nullptr,
                                      "sid is nullptr");

                    /* Is this a match? */
                    if (system_hashed_ansi_string_is_equal_to_hash_string(sid, object_name) )
                    {
                        target_object_node = (collada_data_scene_graph_node) current_node_item_data;
                    }
                    else
                    {
                        system_resizable_vector_push(nodes,
                                                     current_node_item_data);
                    }

                    break;
                }

#if 0
                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized COLLADA node item type");
                }
#endif
            }
        }
    }

    if (target_object_node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find an object <channel> refers to.");

        goto end;
    }

    /* Now look through node items and try to find the matching property item */
    system_resizable_vector_clear(nodes);
    system_resizable_vector_push (nodes,
                                  target_object_node);

    while ((target_property_node == nullptr) && system_resizable_vector_pop(nodes,
                                                                        &current_node) )
    {
        collada_data_scene_graph_node_get_property(current_node,
                                                   COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_N_NODE_ITEMS,
                                                  &n_current_node_items);

        for (uint32_t n_current_node_item = 0;
                      n_current_node_item < n_current_node_items && (target_property_node == nullptr);
                    ++n_current_node_item)
        {
            collada_data_scene_graph_node_get_node_item(target_object_node,
                                                        n_current_node_item,
                                                       &current_node_item);

            collada_data_scene_graph_node_item_get_property(current_node_item,
                                                            COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_DATA_HANDLE,
                                                           &current_node_item_data);
            collada_data_scene_graph_node_item_get_property(current_node_item,
                                                            COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_TYPE,
                                                           &current_node_item_type);

            /* Look for a matching node */
            switch (current_node_item_type)
            {
                case COLLADA_DATA_NODE_ITEM_TYPE_NODE:
                {
                    system_resizable_vector_push(nodes,
                                                 current_node_item_data);

                    break;
                }

                case COLLADA_DATA_NODE_ITEM_TYPE_CAMERA_INSTANCE:
                case COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE:
                case COLLADA_DATA_NODE_ITEM_TYPE_LIGHT_INSTANCE:
                case COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION:
                {
                    system_hashed_ansi_string sid = nullptr;

                    /* Retrieve the name */
                    switch(current_node_item_type)
                    {
                        case COLLADA_DATA_NODE_ITEM_TYPE_CAMERA_INSTANCE:
                        {
                            collada_data_scene_graph_node_camera_instance camera_instance = reinterpret_cast<collada_data_scene_graph_node_camera_instance>(current_node_item_data);

                            collada_data_scene_graph_node_camera_instance_get_property(camera_instance,
                                                                                       COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_NAME,
                                                                                      &sid);

                            target_property_type = COLLADA_DATA_CHANNEL_TARGET_TYPE_CAMERA_INSTANCE;

                            break;
                        }

                        case COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE:
                        {
                            collada_data_scene_graph_node_geometry_instance geometry_instance = reinterpret_cast<collada_data_scene_graph_node_geometry_instance>(current_node_item_data);

                            collada_data_scene_graph_node_geometry_instance_get_property(geometry_instance,
                                                                                         COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_NAME,
                                                                                        &sid);

                            target_property_type = COLLADA_DATA_CHANNEL_TARGET_TYPE_GEOMETRY_INSTANCE;

                            break;
                        }

                        case COLLADA_DATA_NODE_ITEM_TYPE_LIGHT_INSTANCE:
                        {
                            collada_data_scene_graph_node_light_instance light_instance = reinterpret_cast<collada_data_scene_graph_node_light_instance>(current_node_item_data);

                            collada_data_scene_graph_node_light_instance_get_property(light_instance,
                                                                                      COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_NAME,
                                                                                     &sid);

                            target_property_type = COLLADA_DATA_CHANNEL_TARGET_TYPE_LIGHT_INSTANCE;

                            break;
                        }

                        case COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION:
                        {
                            collada_data_transformation transformation = reinterpret_cast<collada_data_transformation>(current_node_item_data);

                            collada_data_transformation_get_property(transformation,
                                                                     COLLADA_DATA_TRANSFORMATION_PROPERTY_SID,
                                                                    &sid);

                            target_property_type = COLLADA_DATA_CHANNEL_TARGET_TYPE_TRANSFORMATION;

                            break;
                        }
                    }

                    /* Sanity check */
                    ASSERT_DEBUG_SYNC(sid != nullptr,
                                      "sid is nullptr");

                    /* Is this a match? */
                    if (system_hashed_ansi_string_is_equal_to_hash_string(sid, property_name) )
                    {
                        target_property_node = current_node_item_data;
                    }

                    /* Done */
                    break;
                }
            }
        }
    }

    if (target_property_node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find a property <channel> refers to.");

        goto end;
    }

    /* Finally, determine the component */
    if (system_hashed_ansi_string_is_equal_to_raw_string(property_component_name, "ANGLE") ) target_component = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_ANGLE;else
    if (system_hashed_ansi_string_is_equal_to_raw_string(property_component_name, "X") )     target_component = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_X;    else
    if (system_hashed_ansi_string_is_equal_to_raw_string(property_component_name, "Y") )     target_component = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Y;    else
    if (system_hashed_ansi_string_is_equal_to_raw_string(property_component_name, "Z") )     target_component = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Z;    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not recognized a property component <channel> refers to.");

        goto end;
    }

    /* All done */
    *out_target_ptr           = target_property_node;
    *out_target_component_ptr = target_component;
    *out_target_type_ptr      = target_property_type;
    result                    = true;

end:
    if (nodes != nullptr)
    {
        system_resizable_vector_release(nodes);

        nodes = nullptr;
    }

    return result;
}


/** Please see header for spec */
PUBLIC collada_data_channel collada_data_channel_create(tinyxml2::XMLElement* channel_element_ptr,
                                                        collada_data_sampler  sampler,
                                                        collada_data          data)
{
    _collada_data_channel*                channel_ptr      = nullptr;
    system_hashed_ansi_string             sampler_id       = nullptr;
    const char*                           target_name      = nullptr;
    void*                                 target           = nullptr;
    collada_data_channel_target_component target_component = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_UNKNOWN;
    collada_data_channel_target_type      target_type      = COLLADA_DATA_CHANNEL_TARGET_TYPE_UNKNOWN;

    /* Retrieve the source instance */
    const char* source_name = channel_element_ptr->Attribute("source");

    ASSERT_DEBUG_SYNC(source_name != nullptr,
                      "Source attribute not defined for <channel> node");

    if (source_name == nullptr)
    {
        goto end;
    }

    if (source_name[0] == '#')
    {
        source_name++;
    }

    /* Is this the right sampler? */
    collada_data_sampler_get_property(sampler,
                                      COLLADA_DATA_SAMPLER_PROPERTY_ID,
                                     &sampler_id);

    if (!system_hashed_ansi_string_is_equal_to_hash_string(sampler_id,
                                                           system_hashed_ansi_string_create(source_name) ))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find sampler source node with id [%s]",
                          source_name);

        goto end;
    }

    /* Identify the target instance */
    target_name = channel_element_ptr->Attribute("target");

    ASSERT_DEBUG_SYNC(target_name != nullptr,
                      "Target attribute not defined for <channel> node");

    if (target_name == nullptr)
    {
        goto end;
    }

    if (!_collada_data_channel_get_target(data,
                                          target_name,
                                         &target,
                                         &target_type,
                                         &target_component) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find target for path [%s]",
                          target_name);

        goto end;
    }

    /* Spawn the descriptor */
    channel_ptr = new (std::nothrow) _collada_data_channel;

    ASSERT_ALWAYS_SYNC(channel_ptr != nullptr,
                       "Out of memory");

    if (channel_ptr != nullptr)
    {
        /* Fill it with pointers we earlier came up with */
        channel_ptr->sampler          = sampler;
        channel_ptr->target           = target;
        channel_ptr->target_component = target_component;
        channel_ptr->target_type      = target_type;
    }

end:
    return reinterpret_cast<collada_data_channel>(channel_ptr);
}

/** Please see header for spec */
PUBLIC void collada_data_channel_get_property(collada_data_channel          channel,
                                              collada_data_channel_property property,
                                              void*                         out_result_ptr)
{
    _collada_data_channel* channel_ptr = reinterpret_cast<_collada_data_channel*>(channel);

    switch (property)
    {
        case COLLADA_DATA_CHANNEL_PROPERTY_SAMPLER:
        {
            *reinterpret_cast<collada_data_sampler*>(out_result_ptr) = channel_ptr->sampler;

            break;
        }

        case COLLADA_DATA_CHANNEL_PROPERTY_TARGET:
        {
            *reinterpret_cast<void**>(out_result_ptr) = channel_ptr->target;

            break;
        }

        case COLLADA_DATA_CHANNEL_PROPERTY_TARGET_COMPONENT:
        {
            *reinterpret_cast<collada_data_channel_target_component*>(out_result_ptr) = channel_ptr->target_component;

            break;
        }

        case COLLADA_DATA_CHANNEL_PROPERTY_TARGET_TYPE:
        {
            *reinterpret_cast<collada_data_channel_target_type*>(out_result_ptr) = channel_ptr->target_type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_channel_property value");
        }
    }
}

/** Please see header for spec */
PUBLIC void collada_data_channel_release(collada_data_channel channel)
{
    delete reinterpret_cast<_collada_data_channel*>(channel);

    channel = nullptr;
}