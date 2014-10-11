/**
 *
 * Emerald (kbi/elude @2014)
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
    effect = NULL;
    id     = system_hashed_ansi_string_get_default_empty_string();
    name   = system_hashed_ansi_string_get_default_empty_string();
}


/** TODO */
PUBLIC EMERALD_API collada_data_material collada_data_material_create(__in __notnull tinyxml2::XMLElement* current_material_element_ptr,
                                                                      __in __notnull system_hash64map      effects_by_id_map,
                                                                      __in __notnull system_hash64map      materials_by_id_map)
{
    _collada_data_material* new_material_ptr = new (std::nothrow) _collada_data_material;

    ASSERT_DEBUG_SYNC(new_material_ptr != NULL, "Out of memory")
    if (new_material_ptr != NULL)
    {
        /* Find effect descriptor */
        collada_data_effect   effect       = NULL;
        const char*           effect_name  = NULL;
        tinyxml2::XMLElement* instance_ptr = current_material_element_ptr->FirstChildElement("instance_effect");

        ASSERT_DEBUG_SYNC(instance_ptr != NULL, "No effect instance found");

        effect_name = instance_ptr->Attribute("url");
        ASSERT_DEBUG_SYNC(effect_name    != NULL, "<instance_effect> node lacks url attribute");
        ASSERT_DEBUG_SYNC(effect_name[0] == '#',  "Effect URL should start with #");

        effect_name += 1; /* Skip # character */

        system_hash64map_get(effects_by_id_map,
                             system_hash64_calculate(effect_name, strlen(effect_name) ),
                            &effect);
        ASSERT_DEBUG_SYNC(effect != NULL, "Effect [%s] was not found", effect_name);

        /* Form the material descriptor */
        new_material_ptr->effect = effect;
        new_material_ptr->id     = system_hashed_ansi_string_create(current_material_element_ptr->Attribute("id") );
        new_material_ptr->name   = system_hashed_ansi_string_create(current_material_element_ptr->Attribute("name") );
    } /* if (new_material_ptr != NULL) */

    return (collada_data_material) new_material_ptr;
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_material_get_properties(__in      __notnull collada_data_material      material,
                                                             __out_opt           collada_data_effect*       out_effect,
                                                             __out_opt           system_hashed_ansi_string* out_id,
                                                             __out_opt           system_hashed_ansi_string* out_name)
{
    _collada_data_material* material_ptr = (_collada_data_material*) material;

    if (out_effect != NULL)
    {
        *out_effect = (collada_data_effect) material_ptr->effect;
    }

    if (out_id != NULL)
    {
        *out_id = material_ptr->id;
    }

    if (out_name != NULL)
    {
        *out_name = material_ptr->name;
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void collada_data_material_get_property(__in  __notnull const collada_data_material          material,
                                                           __in                  collada_data_material_property property,
                                                           __out __notnull       void*                          out_value_ptr)
{
    const _collada_data_material* material_ptr = (const _collada_data_material*) material;

    switch (property)
    {
        case COLLADA_DATA_MATERIAL_PROPERTY_ID:
        {
            * ((system_hashed_ansi_string*) out_value_ptr) = material_ptr->id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA material property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API void collada_data_material_release(__in __notnull __post_invalid collada_data_material material)
{
    _collada_data_material* material_ptr = (_collada_data_material*) material;

    delete material_ptr;
    material_ptr = NULL;
}