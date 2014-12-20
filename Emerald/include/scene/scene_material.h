/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Represents a single scene material.
 */
#ifndef SCENE_MATERIAL_H
#define SCENE_MATERIAL_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_material, scene_material);

enum scene_material_property
{
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME,      /*     settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_GLOSINESS,                    /* not settable, curve_container           */
    SCENE_MATERIAL_PROPERTY_LUMINANCE,                    /* not settable, curve_container           */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME,  /*     settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_NAME,                         /* not settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME,     /*     settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_REFLECTION_RATIO,             /* not settable, curve_container           */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME, /*     settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE,              /*     settable, float                     */
    SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO,               /* not settable, curve_container           */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME,   /*     settable, system_hashed_ansi_string */
};

/** TODO */
PUBLIC EMERALD_API scene_material scene_material_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scene_material_get_property(__in  __notnull scene_material,
                                                    __in            scene_material_property,
                                                    __out __notnull void*);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_material scene_material_load(__in __notnull system_file_serializer serializer);

/** TODO.
 *
 **/
PUBLIC bool scene_material_save(__in __notnull system_file_serializer serializer,
                                __in __notnull const scene_material   material);

/** TODO */
PUBLIC EMERALD_API void scene_material_set_property(__in __notnull scene_material,
                                                    __in           scene_material_property,
                                                    __in __notnull const void*);

#endif /* SCENE_MESH_H */
