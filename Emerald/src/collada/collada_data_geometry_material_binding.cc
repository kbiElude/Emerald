/**
 *
 * Emerald (kbi/elude @2014-2015)
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
    input_semantic_name = NULL;
    input_set           = 0;
    semantic_name       = NULL;
}

/** Please see header for spec */
PUBLIC collada_data_geometry_material_binding collada_data_geometry_material_binding_create(__in __notnull system_hashed_ansi_string input_semantic_name,
                                                                                            __in           unsigned int              input_set,
                                                                                            __in __notnull system_hashed_ansi_string semantic_name)
{
    _collada_data_geometry_material_binding* binding_ptr = new (std::nothrow) _collada_data_geometry_material_binding;

    ASSERT_ALWAYS_SYNC(binding_ptr != NULL,
                       "Out of memory");

    if (binding_ptr != NULL)
    {
        binding_ptr->input_semantic_name = input_semantic_name;
        binding_ptr->input_set           = input_set;
        binding_ptr->semantic_name       = semantic_name;
    }

    return (collada_data_geometry_material_binding) binding_ptr;
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_geometry_material_binding_get_properties(__in __notnull collada_data_geometry_material_binding binding,
                                                                              __out_opt      system_hashed_ansi_string*             out_input_semantic_name,
                                                                              __out_opt      unsigned int*                          out_input_set,
                                                                              __out_opt      system_hashed_ansi_string*             out_semantic_name)
{
    _collada_data_geometry_material_binding* binding_ptr = (_collada_data_geometry_material_binding*) binding;

    if (out_input_semantic_name != NULL)
    {
        *out_input_semantic_name = binding_ptr->input_semantic_name;
    }

    if (out_input_set != NULL)
    {
        *out_input_set = binding_ptr->input_set;
    }

    if (out_semantic_name != NULL)
    {
        *out_semantic_name = binding_ptr->semantic_name;
    }
}

/** Please see header for spec */
PUBLIC void collada_data_geometry_material_binding_release(__in __notnull __post_invalid collada_data_geometry_material_binding binding)
{
    delete (_collada_data_geometry_material_binding*) binding;

    binding = NULL;
}