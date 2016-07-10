/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Represents a single scene material.
 */
#ifndef SCENE_MATERIAL_H
#define SCENE_MATERIAL_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_material,
                             scene_material);

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
    /* settable, curve_container[3] */
    SCENE_MATERIAL_PROPERTY_COLOR,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MAG_FILTER,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MIN_FILTER,

    /* settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME,

    /* settable, curve_container */
    SCENE_MATERIAL_PROPERTY_GLOSINESS,

    /* settable, curve_container */
    SCENE_MATERIAL_PROPERTY_LUMINANCE,

    /* settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_MAG_FILTER,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_MIN_FILTER,

    /* not settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_NAME,

    /* settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_MAG_FILTER,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_MIN_FILTER,

    /* not settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_OBJECT_MANAGER_PATH,

    /* not settable, scene */
    SCENE_MATERIAL_PROPERTY_OWNER_SCENE,

    /* settable, curve_container */
    SCENE_MATERIAL_PROPERTY_REFLECTION_RATIO,

    /* settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_MAG_FILTER,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_MIN_FILTER,

    /* settable, float */
    SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE,

    /* settable, curve_container */
    SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO,

    /* settable, system_hashed_ansi_string */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_MAG_FILTER,

    /* settable, scene_material_texture_filtering */
    SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_MIN_FILTER,
};

/** TODO.
 *
 *  scene_material DOES NOT increment @param owner_scene reference counter.
 *
 **/
PUBLIC EMERALD_API scene_material scene_material_create(system_hashed_ansi_string name,
                                                        system_hashed_ansi_string object_manager_path,
                                                        scene                     owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_material_get_property(scene_material          material,
                                                    scene_material_property property,
                                                    void*                   out_result_ptr);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_material scene_material_load(system_file_serializer    serializer,
                                          scene                     owner_scene,
                                          system_hashed_ansi_string object_manager_path);

/** TODO.
 *
 **/
PUBLIC bool scene_material_save(system_file_serializer serializer,
                                const scene_material   material,
                                scene                  owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_material_set_property(scene_material          material,
                                                    scene_material_property property,
                                                    const void*             data);

#endif /* SCENE_MESH_H */
