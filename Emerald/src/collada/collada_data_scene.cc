/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_scene.h"
#include "collada/collada_data_scene_graph_node.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <visual_scene> node */
typedef struct _collada_data_scene
{
    system_hashed_ansi_string id;
    system_hashed_ansi_string name;

    scene                         emerald_scene;
    collada_data_scene_graph_node fake_root_node;

     _collada_data_scene();
    ~_collada_data_scene();
} _collada_data_scene;


/** TODO */
_collada_data_scene::_collada_data_scene()
{
    emerald_scene = NULL;

    id   = system_hashed_ansi_string_get_default_empty_string();
    name = system_hashed_ansi_string_get_default_empty_string();
}

/** TODO */
_collada_data_scene::~_collada_data_scene()
{
    if (emerald_scene != NULL)
    {
        scene_release(emerald_scene);

        emerald_scene = NULL;
    }
}


/** TODO */
PUBLIC collada_data_scene collada_data_scene_create(__in __notnull tinyxml2::XMLElement* scene_element_ptr,
                                                    __in __notnull collada_data          collada_data)
{
    _collada_data_scene* new_scene_ptr = new (std::nothrow) _collada_data_scene;

    ASSERT_DEBUG_SYNC(new_scene_ptr != NULL, "Out of memory");
    if (new_scene_ptr == NULL)
    {
        goto end;
    }

    /* Retrieve ID and name */
    new_scene_ptr->id   = system_hashed_ansi_string_create(scene_element_ptr->Attribute("id")   );
    new_scene_ptr->name = system_hashed_ansi_string_create(scene_element_ptr->Attribute("name") );

    /* Associate a fake node with the scene node */
    new_scene_ptr->fake_root_node = collada_data_scene_graph_node_create(new_scene_ptr);

    /* Iterate through all nodes making up the scene */
    tinyxml2::XMLElement* current_node_element_ptr = scene_element_ptr->FirstChildElement("node");
    system_hash64map      nodes_by_id_map          = NULL;

    collada_data_get_property(collada_data,
                              COLLADA_DATA_PROPERTY_NODES_BY_ID_MAP,
                             &nodes_by_id_map);

    while (current_node_element_ptr != NULL)
    {
        /* Parse the node */
        collada_data_scene_graph_node_item new_node_item = collada_data_scene_graph_node_item_create_node(current_node_element_ptr,
                                                                                                          NULL,
                                                                                                          collada_data);

        /* Add the node to the scene */
        collada_data_scene_graph_node node       = NULL;
        system_hashed_ansi_string     scene_name = NULL;

        collada_data_scene_graph_node_item_get_property(new_node_item,
                                                        COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_DATA_HANDLE,
                                                       &node);
        collada_data_scene_graph_node_get_property     (node,
                                                        COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_NAME,
                                                       &scene_name);

        system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(scene_name),
                                                           system_hashed_ansi_string_get_length(scene_name) );

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(nodes_by_id_map, entry_hash),
                          "Item already added to a hash-map");

        collada_data_scene_graph_node_add_node_item(new_scene_ptr->fake_root_node,
                                                    new_node_item);

        /* Move on */
        current_node_element_ptr = current_node_element_ptr->NextSiblingElement("node");
    } /* while (current_node_element_ptr != NULL) */

end:
    return (collada_data_scene) new_scene_ptr;
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_scene_get_property(__in  __notnull const collada_data_scene          in_scene,
                                                        __in                  collada_data_scene_property property,
                                                        __out __notnull void*                             out_data_ptr)
{
    const _collada_data_scene* scene_ptr = (const _collada_data_scene*) in_scene;

    switch (property)
    {
        case COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE:
        {
            *((scene*) out_data_ptr) = scene_ptr->emerald_scene;

            break;
        }

        case COLLADA_DATA_SCENE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_data_ptr) = scene_ptr->name;

            break;
        }

        case COLLADA_DATA_SCENE_PROPERTY_ROOT_NODE:
        {
            *((collada_data_scene_graph_node*) out_data_ptr) = (collada_data_scene_graph_node) scene_ptr->fake_root_node;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA scene property");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC void collada_data_scene_release(__in __notnull __post_invalid collada_data_scene scene)
{
    delete (_collada_data_scene*) scene;

    scene = NULL;
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_scene_set_property(__in __notnull collada_data_scene          in_scene,
                                                        __in           collada_data_scene_property property,
                                                        __in __notnull void*                       data_ptr)
{
    _collada_data_scene* scene_ptr = (_collada_data_scene*) in_scene;

    switch (property)
    {
        case COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE:
        {
            scene_ptr->emerald_scene = *((scene*) data_ptr);

            break;
        }

        case COLLADA_DATA_SCENE_PROPERTY_NAME:
        {
            scene_ptr->name = *((system_hashed_ansi_string*) data_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA scene property");
        }
    } /* switch (property) */
}
