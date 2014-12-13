/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_scene_graph_node.h"
#include "collada/collada_data_scene_graph_node_camera_instance.h"
#include "collada/collada_data_scene_graph_node_geometry_instance.h"
#include "collada/collada_data_scene_graph_node_light_instance.h"
#include "collada/collada_data_scene_graph_node_lookat.h"
#include "collada/collada_data_scene_graph_node_material_instance.h"
#include "collada/collada_data_scene_graph_node_matrix.h"
#include "collada/collada_data_scene_graph_node_rotate.h"
#include "collada/collada_data_scene_graph_node_scale.h"
#include "collada/collada_data_scene_graph_node_skew.h"
#include "collada/collada_data_scene_graph_node_translate.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"


/** TODO */
typedef struct _collada_data_scene_graph_node_item
{
    void*                        data;
    system_hashed_ansi_string    sid;
    _collada_data_node_item_type type;

    explicit _collada_data_scene_graph_node_item()
    {
        data = NULL;
        sid  = NULL;
        type = COLLADA_DATA_NODE_ITEM_TYPE_UNDEFINED;
    }
} _collada_data_scene_graph_node_item;

/** Describes data stored in <node> node */
typedef struct _collada_data_scene_graph_node
{
    system_hashed_ansi_string id;
    system_hashed_ansi_string name;
    _collada_data_node_type   type;

    /** This is:
     *
     *  * _collada_data_node*  if type is not COLLADA_DATA_NODE_TYPE_FAKE_ROOT_NODE
     *  * _collada_data_scene* otherwise.
     **/
    void* parent;

    /* contains _collada_data_node_item* items */
    system_resizable_vector node_items;

    explicit _collada_data_scene_graph_node(void* parent);
            ~_collada_data_scene_graph_node();
} _collada_data_scene_graph_node;


/** TODO */
_collada_data_scene_graph_node::_collada_data_scene_graph_node(void* parent)
{
    id   = system_hashed_ansi_string_get_default_empty_string();
    name = system_hashed_ansi_string_get_default_empty_string();
    type = COLLADA_DATA_NODE_TYPE_NODE;

    node_items = system_resizable_vector_create(1, sizeof(_collada_data_scene_graph_node_item*) );

    this->parent = parent;
}

/** TODO */
_collada_data_scene_graph_node::~_collada_data_scene_graph_node()
{
    if (node_items != NULL)
    {
        _collada_data_scene_graph_node_item* node_item_ptr = NULL;

        while (!system_resizable_vector_pop(node_items, &node_item_ptr) )
        {
            switch (node_item_ptr->type)
            {
                case COLLADA_DATA_NODE_ITEM_TYPE_CAMERA_INSTANCE:
                {
                    collada_data_scene_graph_node_camera_instance_release( (collada_data_scene_graph_node_camera_instance) node_item_ptr->data);

                    node_item_ptr->data = NULL;
                    break;
                }

                case COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE:
                {
                    collada_data_scene_graph_node_geometry_instance_release( (collada_data_scene_graph_node_geometry_instance) node_item_ptr->data);

                    node_item_ptr->data = NULL;
                    break;
                }

                case COLLADA_DATA_NODE_ITEM_TYPE_LIGHT_INSTANCE:
                {
                    collada_data_scene_graph_node_light_instance_release( (collada_data_scene_graph_node_light_instance) node_item_ptr->data);

                    node_item_ptr->data = NULL;
                    break;
                }

                case COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION:
                {
                    collada_data_transformation_release( (collada_data_transformation) node_item_ptr->data);

                    break;
                }
            } /* switch (node_item_ptr->type) */
        } /* while (!system_resizable_vector_pop(node_items, &node_item_ptr) ) */

        system_resizable_vector_release(node_items);
        node_items = NULL;
    } /* if (node_items != NULL) */
}

/** TODO */
PUBLIC void collada_data_scene_graph_node_add_node_item(__in __notnull collada_data_scene_graph_node      node,
                                                        __in __notnull collada_data_scene_graph_node_item node_item)
{
    _collada_data_scene_graph_node* node_ptr = (_collada_data_scene_graph_node*) node;

    system_resizable_vector_push(node_ptr->node_items, node_item);
}

/** TODO */
PUBLIC collada_data_scene_graph_node collada_data_scene_graph_node_create(__in __notnull void* parent)
{
    _collada_data_scene_graph_node* new_node_ptr = new (std::nothrow) _collada_data_scene_graph_node(parent);

    ASSERT_ALWAYS_SYNC(new_node_ptr != NULL, "Out of memory");

    return (collada_data_scene_graph_node) new_node_ptr;
}

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_item_create(__in           _collada_data_node_item_type type,
                                                                                    __in __notnull void*                        data,
                                                                                    __in __notnull system_hashed_ansi_string    sid)
{
    _collada_data_scene_graph_node_item* new_node_item_ptr = new (std::nothrow) _collada_data_scene_graph_node_item();

    ASSERT_ALWAYS_SYNC(new_node_item_ptr != NULL, "Out of memory");
    if (new_node_item_ptr == NULL)
    {
        goto end;
    }

    new_node_item_ptr->data = data;
    new_node_item_ptr->sid  = sid;
    new_node_item_ptr->type = type;

end:
    return (collada_data_scene_graph_node_item) new_node_item_ptr;
}

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_item_create_node(__in     __notnull tinyxml2::XMLElement*         element_ptr,
                                                                                         __in_opt __notnull collada_data_scene_graph_node parent_node,
                                                                                         __in     __notnull collada_data                  collada_data)
{
    _collada_data_scene_graph_node* new_node_ptr = new (std::nothrow) _collada_data_scene_graph_node(parent_node);

    ASSERT_ALWAYS_SYNC(new_node_ptr != NULL, "Out of memory");
    if (new_node_ptr == NULL)
    {
        goto end;
    }

    /* Retrieve ID and name */
    new_node_ptr->id   = system_hashed_ansi_string_create(element_ptr->Attribute("id")   );
    new_node_ptr->name = system_hashed_ansi_string_create(element_ptr->Attribute("name") );

    /* Decide on the type */
    const char* type = element_ptr->Attribute("type");

    new_node_ptr->type = COLLADA_DATA_NODE_TYPE_NODE;

    if (type != NULL)
    {
        if (strcmp(type, "NODE") == 0)
        {
            /* Nothing to do here */
        }
        else
        if (strcmp(type, "JOINT") == 0)
        {
            new_node_ptr->type = COLLADA_DATA_NODE_TYPE_JOINT;
        }
        else
        {
            LOG_FATAL        ("Unrecognized <node> type encountered: [%s]", type);
            ASSERT_DEBUG_SYNC(false, "Unrecognized node type found");
        }
    } /* if (type != NULL) */

    /* Nodes can be described by a number of different node types. Iterate over
     * all children and parse them recursively */
    tinyxml2::XMLElement*              current_node_child_ptr    = element_ptr->FirstChildElement();
    bool                               is_lw10_file              = false;
    float                              last_rotate_pivot     [3] = {0};

    collada_data_get_property(collada_data,
                              COLLADA_DATA_PROPERTY_IS_LW10_COLLADA_FILE,
                             &is_lw10_file);

    while (current_node_child_ptr != NULL)
    {
        system_hashed_ansi_string          current_node_type = system_hashed_ansi_string_create(current_node_child_ptr->Name() );
        collada_data_scene_graph_node_item new_node_item     = NULL;

        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "lookat"))
        {
            new_node_item = collada_data_scene_graph_node_lookat_create(current_node_child_ptr);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "matrix"))
        {
            new_node_item = collada_data_scene_graph_node_matrix_create(current_node_child_ptr);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "rotate"))
        {
            new_node_item = collada_data_scene_graph_node_rotate_create(current_node_child_ptr);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "scale"))
        {
            new_node_item = collada_data_scene_graph_node_scale_create(current_node_child_ptr);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "skew"))
        {
            new_node_item = collada_data_scene_graph_node_skew_create(current_node_child_ptr);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "translate"))
        {
            bool  should_cache_as_last_rotatePivot_transformation = false;
            bool  should_enforce_inversed_rotatePivot_vector      = false;
            float inversed_rotate_pivot[3] =
            {
                -last_rotate_pivot[0],
                -last_rotate_pivot[1],
                -last_rotate_pivot[2]
            };

            if (is_lw10_file)
            {
                /* NOTE: LW exporter has a bug where translation with sid "rotatePivotInverse" (expected to be
                 *       an inverse transformation to a translation with sid "rotatePivot") is the same translation
                 *       as rotatePivot. For this reason, we keep track of rotatePivot translations and enforce
                 *       an inversed vector for rotatePivotInverse translations */
                const char* current_node_sid = current_node_child_ptr->Attribute("sid");

                if (strcmp(current_node_sid, "rotatePivot") == 0)
                {
                    should_cache_as_last_rotatePivot_transformation = true;
                }
                else
                if (strcmp(current_node_sid, "rotatePivotInverse") == 0)
                {
                    should_enforce_inversed_rotatePivot_vector = true;
                }
            } /* if (is_lw10_file) */

            new_node_item = collada_data_scene_graph_node_translate_create(current_node_child_ptr,
                                                                           (should_enforce_inversed_rotatePivot_vector      ? inversed_rotate_pivot : NULL),
                                                                           (should_cache_as_last_rotatePivot_transformation ? last_rotate_pivot     : NULL) );
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "instance_camera"))
        {
            system_hash64map                              cameras_by_id_map   = NULL;
            collada_data_scene_graph_node_camera_instance new_camera_instance = NULL;

            collada_data_get_property(collada_data,
                                      COLLADA_DATA_PROPERTY_CAMERAS_BY_ID_MAP,
                                     &cameras_by_id_map);

            new_camera_instance = collada_data_scene_graph_node_camera_instance_create(current_node_child_ptr,
                                                                                       cameras_by_id_map,
                                                                                       new_node_ptr->name);

            /* Wrap the descriptor in an item descriptor */
            new_node_item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_CAMERA_INSTANCE,
                                                                      new_camera_instance,
                                                                      NULL /* sid */);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "instance_controller"))
        {
            /* TODO: Bones support */
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "instance_geometry"))
        {
            system_hash64map                                geometries_by_id_map  = NULL;
            system_hash64map                                materials_by_id_map   = NULL;
            collada_data_scene_graph_node_geometry_instance new_geometry_instance = NULL;

            collada_data_get_property(collada_data,
                                      COLLADA_DATA_PROPERTY_GEOMETRIES_BY_ID_MAP,
                                     &geometries_by_id_map);
            collada_data_get_property(collada_data,
                                      COLLADA_DATA_PROPERTY_MATERIALS_BY_ID_MAP,
                                     &materials_by_id_map);

            new_geometry_instance = collada_data_scene_graph_node_geometry_instance_create(current_node_child_ptr,
                                                                                           geometries_by_id_map,
                                                                                           new_node_ptr->name,
                                                                                           materials_by_id_map);

            /* Wrap the descriptor in an item */
            new_node_item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE,
                                                                      new_geometry_instance,
                                                                      NULL /* sid */);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "instance_light"))
        {
            system_hash64map                             lights_by_id_map   = NULL;
            collada_data_scene_graph_node_light_instance new_light_instance = NULL;

            collada_data_get_property(collada_data,
                                      COLLADA_DATA_PROPERTY_LIGHTS_BY_ID_MAP,
                                     &lights_by_id_map);

            new_light_instance = collada_data_scene_graph_node_light_instance_create(current_node_child_ptr,
                                                                                     lights_by_id_map,
                                                                                     new_node_ptr->name);

            /* Wrap the descriptor in an item descriptor */
            new_node_item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_LIGHT_INSTANCE,
                                                                      new_light_instance,
                                                                      NULL /* sid */);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "node"))
        {
            _collada_data_scene_graph_node* new_subnode = NULL;

            new_subnode = new (std::nothrow) _collada_data_scene_graph_node(new_node_ptr);

            new_node_item = collada_data_scene_graph_node_item_create_node(current_node_child_ptr,
                                                                           parent_node,
                                                                           collada_data);
        }
        else
        if (system_hashed_ansi_string_is_equal_to_raw_string(current_node_type, "extra"))
        {
            /* We don't support any extra information for nodes */
        }
        else
        {
            LOG_FATAL        ("Unrecognized sub-node type [%s] found under <node> with name [%s]",
                              system_hashed_ansi_string_get_buffer(current_node_type),
                              system_hashed_ansi_string_get_buffer(new_node_ptr->name) );
            ASSERT_DEBUG_SYNC(false, "Unrecognized sub-node type");
        }

        if (new_node_item != NULL)
        {
            system_resizable_vector_push(new_node_ptr->node_items, new_node_item);
        }

        /* Move on */
        current_node_child_ptr = current_node_child_ptr->NextSiblingElement();
    } /* while (current_node_child_ptr != NULL) */

    /* Wrap the node in a node item descriptor */
    _collada_data_scene_graph_node_item* item_ptr = new (std::nothrow) _collada_data_scene_graph_node_item;

    ASSERT_ALWAYS_SYNC(item_ptr != NULL, "Out of memory");
    if (item_ptr == NULL)
    {
        goto end;
    }

    item_ptr->data = new_node_ptr;
    item_ptr->type = COLLADA_DATA_NODE_ITEM_TYPE_NODE;

end:
    return (collada_data_scene_graph_node_item) item_ptr;
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_scene_graph_node_get_property(__in  __notnull collada_data_scene_graph_node          node,
                                                                   __in            collada_data_scene_graph_node_property property,
                                                                   __out __notnull void*                                  out_result)
{
    _collada_data_scene_graph_node* node_ptr = (_collada_data_scene_graph_node*) node;

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_ID:
        {
            *(system_hashed_ansi_string*) out_result = node_ptr->id;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_N_NODE_ITEMS:
        {
            *(uint32_t*) out_result = system_resizable_vector_get_amount_of_elements(node_ptr->node_items);

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = node_ptr->name;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_TYPE:
        {
            *(_collada_data_node_type*) out_result = node_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "UNrecognized collada_data_scene_graph_node_property value");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_get_node_item(__in      __notnull collada_data_scene_graph_node       node,
                                                                    __in                unsigned int                        n_node_item,
                                                                    __out_opt __notnull collada_data_scene_graph_node_item* out_node_item)
{
    _collada_data_scene_graph_node* node_ptr = (_collada_data_scene_graph_node*) node;

    if (out_node_item != NULL)
    {
        system_resizable_vector_get_element_at(node_ptr->node_items, n_node_item, out_node_item);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_item_get_property(__in  __notnull collada_data_scene_graph_node_item          node_item,
                                                                        __in            collada_data_scene_graph_node_item_property property,
                                                                        __out __notnull void*                                       out_result)
{
    _collada_data_scene_graph_node_item* node_item_ptr = (_collada_data_scene_graph_node_item*) node_item;

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_DATA_HANDLE:
        {
            *((void**)out_result) = node_item_ptr->data;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_SID:
        {
            *((system_hashed_ansi_string*) out_result) = node_item_ptr->sid;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_TYPE:
        {
            ASSERT_DEBUG_SYNC(node_item_ptr->type != COLLADA_DATA_NODE_ITEM_TYPE_UNDEFINED,
                              "Unrecognized node item property type");

            *(_collada_data_node_item_type*) out_result = node_item_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_scene_graph_node_iten_property value");
        }
    } /* switch (property) */
}

