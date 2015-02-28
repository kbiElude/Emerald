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

typedef enum
{
    /* NOTE: Please keep the ordering exactly as in mesh_material_texture_filtering,
     *       or else: bad stuff.
     */
    SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR,
    SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR,
    SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_NEAREST,
    SCENE_MATERIAL_TEXTURE_FILTERING_NEAREST,
    SCENE_MATERIAL_TEXTURE_FILTERING_NEAREST_LINEAR,
    SCENE_MATERIAL_TEXTURE_FILTERING_NEAREST_NEAREST,
} scene_material_texture_filtering;

enum scene_material_property
{
    SCENE_MATERIAL_PROPERTY_COLOR,                         /*     settable, curve_container[3]               */
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MAG_FILTER,      /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MIN_FILTER,      /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME,       /*     settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_GLOSINESS,                     /*     settable, curve_container                  */
    SCENE_MATERIAL_PROPERTY_LUMINANCE,                     /*     settable, curve_container                  */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME,   /*     settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_MAG_FILTER,  /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_MIN_FILTER,  /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_NAME,                          /* not settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME,      /*     settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_MAG_FILTER,     /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_MIN_FILTER,     /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_OBJECT_MANAGER_PATH,           /* not settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_REFLECTION_RATIO,              /*     settable, curve_container                  */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME,  /*     settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_MAG_FILTER, /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_MIN_FILTER, /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE,               /*     settable, float                            */
    SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO,                /*     settable, curve_container                  */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME,    /*     settable, system_hashed_ansi_string        */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_MAG_FILTER,   /*     settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_MIN_FILTER,   /*     settable, scene_material_texture_filtering */
};

/** TODO */
PUBLIC EMERALD_API scene_material scene_material_create(__in     __notnull system_hashed_ansi_string name,
                                                        __in_opt           system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API void scene_material_get_property(__in  __notnull scene_material,
                                                    __in            scene_material_property,
                                                    __out __notnull void*);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_material scene_material_load(__in __notnull system_file_serializer    serializer,
                                          __in_opt       scene                     owner_scene,
                                          __in_opt       system_hashed_ansi_string object_manager_path);

/** TODO.
 *
 **/
PUBLIC bool scene_material_save(__in     __notnull system_file_serializer serializer,
                                __in     __notnull const scene_material   material,
                                __in_opt           scene                  owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_material_set_property(__in __notnull scene_material,
                                                    __in           scene_material_property,
                                                    __in __notnull const void*);

#endif /* SCENE_MESH_H */
