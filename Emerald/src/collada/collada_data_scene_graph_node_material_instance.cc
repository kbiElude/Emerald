/**
 *
 * Emerald (kbi/elude @2014-2016)
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
    material    = nullptr;
    symbol_name = nullptr;
}

/** TODO */
_collada_data_scene_graph_node_material_instance::~_collada_data_scene_graph_node_material_instance()
{
    if (bindings != nullptr)
    {
        collada_data_geometry_material_binding binding = nullptr;

        while (system_resizable_vector_pop(bindings,
                                          &binding) )
        {
            collada_data_geometry_material_binding_release(binding);

            binding = nullptr;
        }

        system_resizable_vector_release(bindings);
        bindings = nullptr;
    }
}

/** Please see header for spec */
PUBLIC void collada_data_scene_graph_node_material_instance_add_material_binding(collada_data_scene_graph_node_material_instance instance,
                                                                                 collada_data_geometry_material_binding          binding)
{
    _collada_data_scene_graph_node_material_instance* instance_ptr = reinterpret_cast<_collada_data_scene_graph_node_material_instance*>(instance);

    system_resizable_vector_push(instance_ptr->bindings,
                                 binding);
}

/** Please see header for spec */
PUBLIC collada_data_scene_graph_node_material_instance collada_data_scene_graph_node_material_instance_create(tinyxml2::XMLElement* element_ptr,
                                                                                                              system_hash64map      materials_by_id_map)
{
    /* Read the attribute data */
    _collada_data_scene_graph_node_material_instance* instance_ptr = nullptr;
    collada_data_material                             material     = nullptr;
    const char*                                       symbol_name = element_ptr->Attribute("symbol");
    const char*                                       target_name = element_ptr->Attribute("target");

    ASSERT_DEBUG_SYNC(symbol_name != nullptr,
                      "Required <symbol> attribute not defined for a <instance_material> node");
    ASSERT_DEBUG_SYNC(target_name != nullptr,
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

    ASSERT_ALWAYS_SYNC(instance_ptr != nullptr,
                       "Out of memory");

    if (instance_ptr == nullptr)
    {
        goto end;
    }

    /* Fill the descriptor */
    instance_ptr->material    = material;
    instance_ptr->symbol_name = system_hashed_ansi_string_create(symbol_name);

end:
    return reinterpret_cast<collada_data_scene_graph_node_material_instance>(instance_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_material_binding(collada_data_scene_graph_node_material_instance instance,
                                                                                             unsigned int                                    n_binding,
                                                                                             collada_data_geometry_material_binding*         out_material_binding_ptr)
{
    collada_data_geometry_material_binding            binding_ptr  = nullptr;
    _collada_data_scene_graph_node_material_instance* instance_ptr = reinterpret_cast<_collada_data_scene_graph_node_material_instance*>(instance);

    if (system_resizable_vector_get_element_at(instance_ptr->bindings,
                                               n_binding,
                                              &binding_ptr) )
    {
        if (out_material_binding_ptr != nullptr)
        {
            *out_material_binding_ptr = binding_ptr;
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
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_properties(collada_data_scene_graph_node_material_instance instance,
                                                                                       collada_data_material*                          out_material_ptr,
                                                                                       system_hashed_ansi_string*                      out_symbol_name_ptr,
                                                                                       unsigned int*                                   out_n_bindings_ptr)
{
    _collada_data_scene_graph_node_material_instance* instance_ptr = reinterpret_cast<_collada_data_scene_graph_node_material_instance*>(instance);

    if (out_material_ptr != nullptr)
    {
        *out_material_ptr = instance_ptr->material;
    }

    if (out_symbol_name_ptr != nullptr)
    {
        *out_symbol_name_ptr = instance_ptr->symbol_name;
    }

    if (out_n_bindings_ptr != nullptr)
    {
        system_resizable_vector_get_property(instance_ptr->bindings,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             out_n_bindings_ptr);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_property(collada_data_scene_graph_node_material_instance          instance,
                                                                                     collada_data_scene_graph_node_material_instance_property property,
                                                                                     void*                                                    out_result_ptr)
{
    _collada_data_scene_graph_node_material_instance* instance_ptr = reinterpret_cast<_collada_data_scene_graph_node_material_instance*>(instance);

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_SYMBOL_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = instance_ptr->symbol_name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA material instance property");
        }
    }
}

/** Please see header for spec */
PUBLIC void collada_data_scene_graph_node_material_instance_release(collada_data_scene_graph_node_material_instance instance)
{
    delete reinterpret_cast<_collada_data_scene_graph_node_material_instance*>(instance);

    instance = nullptr;
}