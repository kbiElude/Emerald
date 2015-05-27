/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_geometry.h"
#include "collada/collada_data_geometry_material_binding.h"
#include "collada/collada_data_material.h"
#include "collada/collada_data_scene_graph_node_material_instance.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <instance_material> node.
 *
 *  Common profile is assumed.
 **/
typedef struct _collada_data_scene_graph_node_material_instance
{
    collada_data_material     material; /* "target" attribute of the node */
    system_hashed_ansi_string symbol_name;

    system_resizable_vector bindings; /* stores collada_data_geometry_material_binding instances */

     _collada_data_scene_graph_node_material_instance();
    ~_collada_data_scene_graph_node_material_instance();
} _collada_data_scene_graph_node_material_instance;


/** TODO */
_collada_data_scene_graph_node_material_instance::_collada_data_scene_graph_node_material_instance()
{
    bindings    = system_resizable_vector_create(4 /* capacity */);
    material    = NULL;
    symbol_name = NULL;
}

/** TODO */
_collada_data_scene_graph_node_material_instance::~_collada_data_scene_graph_node_material_instance()
{
    if (bindings != NULL)
    {
        collada_data_geometry_material_binding binding = NULL;

        while (system_resizable_vector_pop(bindings,
                                          &binding) )
        {
            collada_data_geometry_material_binding_release(binding);

            binding = NULL;
        }

        system_resizable_vector_release(bindings);
        bindings = NULL;
    }
}

/** Please see header for spec */
PUBLIC void collada_data_scene_graph_node_material_instance_add_material_binding(__in __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                 __in           collada_data_geometry_material_binding          binding)
{
    _collada_data_scene_graph_node_material_instance* instance_ptr = (_collada_data_scene_graph_node_material_instance*) instance;

    system_resizable_vector_push(instance_ptr->bindings, binding);
}

/** Please see header for spec */
PUBLIC collada_data_scene_graph_node_material_instance collada_data_scene_graph_node_material_instance_create(__in __notnull tinyxml2::XMLElement* element_ptr,
                                                                                                              __in __notnull system_hash64map      materials_by_id_map)
{
    /* Read the attribute data */
    _collada_data_scene_graph_node_material_instance* instance_ptr = NULL;
    collada_data_material                             material     = NULL;
    const char*                                       symbol_name = element_ptr->Attribute("symbol");
    const char*                                       target_name = element_ptr->Attribute("target");

    ASSERT_DEBUG_SYNC(symbol_name != NULL,
                      "Required <symbol> attribute not defined for a <instance_material> node");
    ASSERT_DEBUG_SYNC(target_name != NULL,
                      "Required <target> attribute not defined for a <instance_material> node");

    if (target_name[0] == '#')
    {
        target_name++;
    }

    /* Find the material descriptor */
    if (!system_hash64map_get(materials_by_id_map,
                              system_hash64_calculate(target_name,
                                                      strlen(target_name) ),
                              &material) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find a material named [%s]",
                          target_name);

        goto end;
    }

    /* Spawn new descriptor */
    instance_ptr = new (std::nothrow) _collada_data_scene_graph_node_material_instance;

    ASSERT_ALWAYS_SYNC(instance_ptr != NULL,
                       "Out of memory");

    if (instance_ptr == NULL)
    {
        goto end;
    }

    /* Fill the descriptor */
    instance_ptr->material    = material;
    instance_ptr->symbol_name = system_hashed_ansi_string_create(symbol_name);

end:
    return (collada_data_scene_graph_node_material_instance) instance_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_material_binding(__in  __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                             __in            unsigned int                                    n_binding,
                                                                                             __out __notnull collada_data_geometry_material_binding*         out_material_binding)
{
    collada_data_geometry_material_binding            binding_ptr  = NULL;
    _collada_data_scene_graph_node_material_instance* instance_ptr = (_collada_data_scene_graph_node_material_instance*) instance;

    if (system_resizable_vector_get_element_at(instance_ptr->bindings,
                                               n_binding,
                                              &binding_ptr) )
    {
        if (out_material_binding != NULL)
        {
            *out_material_binding = binding_ptr;
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve material binding at index [%d]",
                          n_binding);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_properties(__in      __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                       __out_opt           collada_data_material*                          out_material,
                                                                                       __out_opt           system_hashed_ansi_string*                      out_symbol_name,
                                                                                       __out_opt           unsigned int*                                   out_n_bindings)
{
    _collada_data_scene_graph_node_material_instance* instance_ptr = (_collada_data_scene_graph_node_material_instance*) instance;

    if (out_material != NULL)
    {
        *out_material = instance_ptr->material;
    }

    if (out_symbol_name != NULL)
    {
        *out_symbol_name = instance_ptr->symbol_name;
    }

    if (out_n_bindings != NULL)
    {
        *out_n_bindings = system_resizable_vector_get_amount_of_elements(instance_ptr->bindings);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_property(__in      __notnull collada_data_scene_graph_node_material_instance          instance,
                                                                                     __in                collada_data_scene_graph_node_material_instance_property property,
                                                                                     __out_opt           void*                                                    out_result_ptr)
{
    _collada_data_scene_graph_node_material_instance* instance_ptr = (_collada_data_scene_graph_node_material_instance*) instance;

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_SYMBOL_NAME:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = instance_ptr->symbol_name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA material instance property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void collada_data_scene_graph_node_material_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_material_instance instance)
{
    delete (_collada_data_scene_graph_node_material_instance*) instance;

    instance = NULL;
}