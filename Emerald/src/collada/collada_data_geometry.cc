/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_float_array.h"
#include "collada/collada_data_geometry.h"
#include "collada/collada_data_geometry_material_binding.h"
#include "collada/collada_data_geometry_mesh.h"
#include "collada/collada_data_input.h"
#include "collada/collada_data_polylist.h"
#include "collada/collada_data_scene_graph_node_material_instance.h"
#include "mesh/mesh.h"
#include "system/system_atomics.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"

/** Describes data stored in <geometry> node */
typedef struct _collada_data_geometry
{
    /* Geometry can consist of "convex mesh", "mesh" and "spline"s.
     *
     * We only support meshes at the moment.
     */
    system_hashed_ansi_string  id;
    collada_data_geometry_mesh geometry_mesh;
    system_hashed_ansi_string  name;

    system_resizable_vector    material_instances; /* store collada_data_geometry_material_instance instances */

    mesh                       emerald_mesh;

     _collada_data_geometry();
    ~_collada_data_geometry();
} _collada_data_geometry;

/** TODO */
struct _collada_data_init_geometries_task_attachment
{
    system_event           geometry_processed_event;
    const    unsigned int* n_geometry_elements_ptr;
    volatile unsigned int* n_geometry_elements_processed_ptr;

    system_resizable_vector result_geometries;
    system_hash64map        result_geometries_by_id_map;

    tinyxml2::XMLElement*            mesh_element_ptr;
    volatile _collada_data_geometry* new_geometry_ptr;

    collada_data data;
};

/** TODO */
_collada_data_geometry::~_collada_data_geometry()
{
    if (emerald_mesh != NULL)
    {
        mesh_release(emerald_mesh);

        emerald_mesh = NULL;
    }

    if (material_instances != NULL)
    {
        collada_data_scene_graph_node_material_instance material_instance = NULL;

        while (system_resizable_vector_pop(material_instances,
                                           (void*) &material_instance) )
        {
            collada_data_scene_graph_node_material_instance_release(material_instance);

            material_instance = NULL;
        }

        system_resizable_vector_release(material_instances);
        material_instances = NULL;
    }

    if (geometry_mesh != NULL)
    {
        collada_data_geometry_mesh_release(geometry_mesh);

        geometry_mesh = NULL;
    }
}

/* Forward declarations */
volatile void _collada_data_init_geometries_task(void* attachment);


/** TODO */
_collada_data_geometry::_collada_data_geometry()
{
    emerald_mesh       = NULL;
    geometry_mesh      = NULL;
    id                 = system_hashed_ansi_string_get_default_empty_string();
    material_instances = system_resizable_vector_create                    (4 /* capacity */);
    name               = system_hashed_ansi_string_get_default_empty_string();
}


/* TODO */
volatile void _collada_data_init_geometry_task(void* attachment)
{
    _collada_data_init_geometries_task_attachment* attachment_ptr = (_collada_data_init_geometries_task_attachment*) attachment;

    LOG_INFO("Processing geometry mesh [%s]..",
             system_hashed_ansi_string_get_buffer(attachment_ptr->new_geometry_ptr->id) );

    /* Process the geometry mesh */
    attachment_ptr->new_geometry_ptr->geometry_mesh = collada_data_geometry_mesh_create(attachment_ptr->mesh_element_ptr,
                                                                                        (collada_data_geometry) attachment_ptr->new_geometry_ptr,
                                                                                        attachment_ptr->data);

    /* Store the geometry */
    system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(attachment_ptr->new_geometry_ptr->id),
                                                       system_hashed_ansi_string_get_length(attachment_ptr->new_geometry_ptr->id) );

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(attachment_ptr->result_geometries_by_id_map, entry_hash),
                      "Item already added to a hash-map");

    system_resizable_vector_push(attachment_ptr->result_geometries,
                                 (void*) attachment_ptr->new_geometry_ptr);
    system_hash64map_insert     (attachment_ptr->result_geometries_by_id_map,
                                 entry_hash,
                                 (void*) attachment_ptr->new_geometry_ptr,
                                 NULL,  /* no remove call-back needed */
                                 NULL); /* no remove call-back needed */

    /* Update the waiter thread counter */
    if (system_atomics_increment(attachment_ptr->n_geometry_elements_processed_ptr) == (*attachment_ptr->n_geometry_elements_ptr))
    {
        /* Signal the wait event */
        system_event_set(attachment_ptr->geometry_processed_event);
    }

    LOG_INFO("Processed geometry mesh [%s].",
             system_hashed_ansi_string_get_buffer(attachment_ptr->new_geometry_ptr->id));

    /* Release the attachment */
    delete attachment_ptr;
}


/** TODO */
PUBLIC void collada_data_geometry_add_material_instance(collada_data_geometry                           geometry,
                                                        collada_data_scene_graph_node_material_instance material_instance)
{
    _collada_data_geometry* geometry_ptr = (_collada_data_geometry*) geometry;

    system_resizable_vector_push(geometry_ptr->material_instances,
                                  material_instance);
}

/** TODO */
PUBLIC collada_data_geometry collada_data_geometry_create_async(tinyxml2::XMLElement*   xml_element_ptr,
                                                                collada_data            collada_data,
                                                                system_event            geometry_processed_event,
                                                                volatile unsigned int*  n_geometry_elements_processed_ptr,
                                                                const    unsigned int*  n_geometry_elements_ptr,
                                                                system_resizable_vector result_geometries,
                                                                system_hash64map        result_geometries_by_id_map)
{
    _collada_data_geometry* new_geometry = new (std::nothrow) _collada_data_geometry;

    ASSERT_DEBUG_ASYNC(new_geometry != NULL,
                       "Out of memory");

    if (new_geometry != NULL)
    {
        new_geometry->id   = system_hashed_ansi_string_create(xml_element_ptr->Attribute("id") );
        new_geometry->name = system_hashed_ansi_string_create(xml_element_ptr->Attribute("name") );

        /* <geometry> can describe a convex mesh, mesh or a spline.
         *
         * We currently ONLY support meshes (well shit :) )
         */
        tinyxml2::XMLElement* mesh_element_ptr = xml_element_ptr->FirstChildElement("mesh");

        if (mesh_element_ptr != NULL)
        {
            /* Create a thread pool task and move a _collada_data_init_geometry_mesh() call
             * out to a separate thread. Once the call finishes, the external thread will
             * update both the result_geometries and result_geometries_by_id_map.
             * The attachment we're allocating here will be deallocated in the thread discussed.
             **/
            _collada_data_init_geometries_task_attachment* attachment_ptr = new _collada_data_init_geometries_task_attachment;

            ASSERT_ALWAYS_SYNC(attachment_ptr != NULL,
                               "Out of memory");

            if (attachment_ptr != NULL)
            {
                attachment_ptr->data                              = collada_data;
                attachment_ptr->geometry_processed_event          = geometry_processed_event;
                attachment_ptr->mesh_element_ptr                  = mesh_element_ptr;
                attachment_ptr->new_geometry_ptr                  = new_geometry;
                attachment_ptr->n_geometry_elements_processed_ptr = n_geometry_elements_processed_ptr;
                attachment_ptr->n_geometry_elements_ptr           = n_geometry_elements_ptr;
                attachment_ptr->result_geometries                 = result_geometries;
                attachment_ptr->result_geometries_by_id_map       = result_geometries_by_id_map;

                system_thread_pool_task_descriptor new_task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                     _collada_data_init_geometry_task,
                                                                                                                     attachment_ptr);

                system_thread_pool_submit_single_task(new_task);
            }
        } /* if (mesh_element_ptr != NULL) */
        else
        {
            /* Well, this geometry is not currently supported.. Try to retrieve the name of the geometry 
             * for logging purposes */
            ASSERT_DEBUG_SYNC(false,
                              "Whoops! Unsupported asset");
            LOG_FATAL        ("Unsupported geometry type (name:[%s])",
                              system_hashed_ansi_string_get_buffer(new_geometry->name) );
        }
    } /* if (new_geometry != NULL) */

    return (collada_data_geometry) new_geometry;
}

/* Please see header for spec */
PUBLIC void collada_data_geometry_get_property(const collada_data_geometry          geometry,
                                                     collada_data_geometry_property property,
                                                     void*                          out_result_ptr)
{
    const _collada_data_geometry* geometry_ptr = (const _collada_data_geometry*) geometry;

    switch (property)
    {
        case COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH:
        {
            *((mesh*) out_result_ptr) = geometry_ptr->emerald_mesh;

            break;
        }

        case COLLADA_DATA_GEOMETRY_PROPERTY_ID:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = geometry_ptr->id;

            break;
        }

        case COLLADA_DATA_GEOMETRY_PROPERTY_MESH:
        {
            *((collada_data_geometry_mesh*) out_result_ptr) = geometry_ptr->geometry_mesh;

            break;
        }

        case COLLADA_DATA_GEOMETRY_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = geometry_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA geometry property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_geometry_get_properties(collada_data_geometry       geometry,
                                                             system_hashed_ansi_string*  out_name,
                                                             collada_data_geometry_mesh* out_geometry_mesh,
                                                             unsigned int*               out_n_material_instances)
{
    _collada_data_geometry* geometry_ptr = (_collada_data_geometry*) geometry;

    if (out_name != NULL)
    {
        *out_name = geometry_ptr->name;
    }

    if (out_geometry_mesh != NULL)
    {
        *out_geometry_mesh = geometry_ptr->geometry_mesh;
    }

    if (out_n_material_instances != NULL)
    {
        system_resizable_vector_get_property(geometry_ptr->material_instances,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             out_n_material_instances);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API bool collada_data_geometry_get_material_binding_index_by_input_semantic_name(collada_data_scene_graph_node_material_instance instance,
                                                                                                unsigned int                                    input_set,
                                                                                                system_hashed_ansi_string                       input_semantic_name,
                                                                                                unsigned int*                                   out_binding_index_ptr)
{
    unsigned int n_bindings = 0;
    bool         result     = false;

    collada_data_scene_graph_node_material_instance_get_properties(instance,
                                                                   NULL, /* out_material */
                                                                   NULL, /* out_symbol_name */
                                                                   &n_bindings);

    for (unsigned int n_binding = 0; n_binding < n_bindings; ++n_binding)
    {
        collada_data_geometry_material_binding binding                     = NULL;
        system_hashed_ansi_string              binding_input_semantic_name = NULL;

        collada_data_scene_graph_node_material_instance_get_material_binding(instance,
                                                                             n_binding,
                                                                             &binding);

        collada_data_geometry_material_binding_get_properties(binding,
                                                             &binding_input_semantic_name,
                                                              NULL,  /* out_input_set */
                                                              NULL); /* out_semantic_name */

        if (system_hashed_ansi_string_is_equal_to_hash_string(binding_input_semantic_name, input_semantic_name) )
        {
            *out_binding_index_ptr = n_binding;
            result                 = true;

            break;
        }
    } /* for (all bindings) */

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API collada_data_scene_graph_node_material_instance collada_data_geometry_get_material_instance(collada_data_geometry geometry,
                                                                                                               unsigned int          n_material_instance)
{
    _collada_data_geometry*                         geometry_ptr = (_collada_data_geometry*) geometry;
    collada_data_scene_graph_node_material_instance result       = NULL;

    if (!system_resizable_vector_get_element_at(geometry_ptr->material_instances, n_material_instance, &result) )
    {
        ASSERT_DEBUG_SYNC(false, "Could not retrieve geometry material instance at index [%d]", n_material_instance);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API collada_data_scene_graph_node_material_instance collada_data_geometry_get_material_instance_by_symbol_name(collada_data_geometry     geometry,
                                                                                                                              system_hashed_ansi_string symbol_name)
{
    _collada_data_geometry*                         geometry_ptr         = (_collada_data_geometry*) geometry;
    unsigned int                                    n_material_instances = 0;
    collada_data_scene_graph_node_material_instance result               = NULL;

    system_resizable_vector_get_property(geometry_ptr->material_instances,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_material_instances);

    for (unsigned int n_material_instance = 0; n_material_instance < n_material_instances; ++n_material_instance)
    {
        collada_data_scene_graph_node_material_instance instance             = NULL;
        system_hashed_ansi_string                       instance_symbol_name = NULL;

        if (system_resizable_vector_get_element_at(geometry_ptr->material_instances, n_material_instance, &instance) )
        {
            collada_data_scene_graph_node_material_instance_get_property(instance,
                                                                         COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_SYMBOL_NAME,
                                                                        &instance_symbol_name);

            if (system_hashed_ansi_string_is_equal_to_hash_string(instance_symbol_name, symbol_name) )
            {
                result = instance;

                goto end;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve geometry material instance at index [%d]", n_material_instance);
        }
    } /* for (all material instances) */

end:
    return result;
}

/* Please see header for spec */
PUBLIC void collada_data_geometry_release(collada_data_geometry geometry)
{
    delete ( (_collada_data_geometry*) geometry);

    geometry = NULL;
}

/* Please see header for spec */
PUBLIC void collada_data_geometry_set_property(collada_data_geometry          geometry,
                                               collada_data_geometry_property property,
                                               void*                          data)
{
    _collada_data_geometry* geometry_ptr = (_collada_data_geometry*) geometry;

    switch (property)
    {
        case COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH:
        {
            geometry_ptr->emerald_mesh = *((mesh*) data);

            break;
        }

        case COLLADA_DATA_GEOMETRY_PROPERTY_ID:
        {
            geometry_ptr->id = *((system_hashed_ansi_string*) data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA geometry property");
        }
    } /* switch (property) */
}