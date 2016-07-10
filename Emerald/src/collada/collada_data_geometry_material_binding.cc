/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_geometry_material_binding.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <instance_material>/<bind_vertex_input> node.
 *
 *  Common profile is assumed.
 **/
typedef struct _collada_data_geometry_material_binding
{
    system_hashed_ansi_string input_semantic_name;
    unsigned int              input_set;
    system_hashed_ansi_string semantic_name;

    _collada_data_geometry_material_binding();

} _collada_data_geometry_material_binding;


/** TODO */
_collada_data_geometry_material_binding::_collada_data_geometry_material_binding()
{
    input_semantic_name = nullptr;
    input_set           = 0;
    semantic_name       = nullptr;
}

/** Please see header for spec */
PUBLIC collada_data_geometry_material_binding collada_data_geometry_material_binding_create(system_hashed_ansi_string input_semantic_name,
                                                                                            unsigned int              input_set,
                                                                                            system_hashed_ansi_string semantic_name)
{
    _collada_data_geometry_material_binding* binding_ptr = new (std::nothrow) _collada_data_geometry_material_binding;

    ASSERT_ALWAYS_SYNC(binding_ptr != nullptr,
                       "Out of memory");

    if (binding_ptr != nullptr)
    {
        binding_ptr->input_semantic_name = input_semantic_name;
        binding_ptr->input_set           = input_set;
        binding_ptr->semantic_name       = semantic_name;
    }

    return reinterpret_cast<collada_data_geometry_material_binding>(binding_ptr);
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_geometry_material_binding_get_properties(collada_data_geometry_material_binding binding,
                                                                              system_hashed_ansi_string*             out_input_semantic_name_ptr,
                                                                              unsigned int*                          out_input_set_ptr,
                                                                              system_hashed_ansi_string*             out_semantic_name_ptr)
{
    _collada_data_geometry_material_binding* binding_ptr = reinterpret_cast<_collada_data_geometry_material_binding*>(binding);

    if (out_input_semantic_name_ptr != nullptr)
    {
        *out_input_semantic_name_ptr = binding_ptr->input_semantic_name;
    }

    if (out_input_set_ptr != nullptr)
    {
        *out_input_set_ptr = binding_ptr->input_set;
    }

    if (out_semantic_name_ptr != nullptr)
    {
        *out_semantic_name_ptr = binding_ptr->semantic_name;
    }
}

/** Please see header for spec */
PUBLIC void collada_data_geometry_material_binding_release(collada_data_geometry_material_binding binding)
{
    delete reinterpret_cast<_collada_data_geometry_material_binding*>(binding);

    binding = nullptr;
}