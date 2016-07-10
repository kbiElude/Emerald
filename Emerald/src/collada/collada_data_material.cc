/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_material.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes data stored under <material> */
typedef struct _collada_data_material
{
    collada_data_effect       effect;
    system_hashed_ansi_string id;
    system_hashed_ansi_string name;

    _collada_data_material();
} _collada_data_material;


/** TODO */
_collada_data_material::_collada_data_material()
{
    effect = nullptr;
    id     = system_hashed_ansi_string_get_default_empty_string();
    name   = system_hashed_ansi_string_get_default_empty_string();
}


/** TODO */
PUBLIC EMERALD_API collada_data_material collada_data_material_create(tinyxml2::XMLElement* current_material_element_ptr,
                                                                      system_hash64map      effects_by_id_map,
                                                                      system_hash64map      materials_by_id_map)
{
    _collada_data_material* new_material_ptr = new (std::nothrow) _collada_data_material;

    ASSERT_DEBUG_SYNC(new_material_ptr != nullptr,
                      "Out of memory")

    if (new_material_ptr != nullptr)
    {
        /* Find effect descriptor */
        collada_data_effect   effect       = nullptr;
        const char*           effect_name  = nullptr;
        tinyxml2::XMLElement* instance_ptr = current_material_element_ptr->FirstChildElement("instance_effect");

        ASSERT_DEBUG_SYNC(instance_ptr != nullptr,
                          "No effect instance found");

        effect_name = instance_ptr->Attribute("url");

        ASSERT_DEBUG_SYNC(effect_name != nullptr,
                          "<instance_effect> node lacks url attribute");
        ASSERT_DEBUG_SYNC(effect_name[0] == '#',
                          "Effect URL should start with #");

        effect_name += 1; /* Skip # character */

        system_hash64map_get(effects_by_id_map,
                             system_hash64_calculate(effect_name, strlen(effect_name) ),
                            &effect);

        ASSERT_DEBUG_SYNC(effect != nullptr,
                          "Effect [%s] was not found",
                          effect_name);

        /* Form the material descriptor */
        new_material_ptr->effect = effect;
        new_material_ptr->id     = system_hashed_ansi_string_create(current_material_element_ptr->Attribute("id") );
        new_material_ptr->name   = system_hashed_ansi_string_create(current_material_element_ptr->Attribute("name") );
    }

    return reinterpret_cast<collada_data_material>(new_material_ptr);
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_material_get_properties(collada_data_material      material,
                                                             collada_data_effect*       out_effect_ptr,
                                                             system_hashed_ansi_string* out_id_ptr,
                                                             system_hashed_ansi_string* out_name_ptr)
{
    _collada_data_material* material_ptr = reinterpret_cast<_collada_data_material*>(material);

    if (out_effect_ptr != nullptr)
    {
        *out_effect_ptr = reinterpret_cast<collada_data_effect>(material_ptr->effect);
    }

    if (out_id_ptr != nullptr)
    {
        *out_id_ptr = material_ptr->id;
    }

    if (out_name_ptr != nullptr)
    {
        *out_name_ptr = material_ptr->name;
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void collada_data_material_get_property(const collada_data_material    material,
                                                           collada_data_material_property property,
                                                           void*                          out_value_ptr)
{
    const _collada_data_material* material_ptr = reinterpret_cast<const _collada_data_material*>(material);

    switch (property)
    {
        case COLLADA_DATA_MATERIAL_PROPERTY_ID:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_value_ptr) = material_ptr->id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA material property");
        }
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void collada_data_material_release(collada_data_material material)
{
    _collada_data_material* material_ptr = reinterpret_cast<_collada_data_material*>(material);

    delete material_ptr;
    material_ptr = nullptr;
}