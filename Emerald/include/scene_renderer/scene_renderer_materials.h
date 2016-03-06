/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef SCENE_RENDERER_MATERIALS_H
#define SCENE_RENDERER_MATERIALS_H

#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_types.h"
#include "scene_renderer/scene_renderer_types.h"

typedef enum
{
    SPECIAL_MATERIAL_FIRST,

    /* Used for directional/point/spot light SM generation (plain shadow mapping only).
     *
     * Fragment shader is dummy.
     * Vertex shader sets gl_Position to the depth (from clip space)
     */
    SPECIAL_MATERIAL_DEPTH_CLIP = SPECIAL_MATERIAL_FIRST,

    /* Used for directional/point/spot light SM generation (variance shadow mapping only) **/
    SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED,

    /* Used for dual paraboloid SM generation for point lights (plain shadow mapping only) */
    SPECIAL_MATERIAL_DEPTH_CLIP_DUAL_PARABOLOID,

    /* Used for dual paraboloid SM generation for point lights (variance shadow mapping only) */
    SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED_DUAL_PARABOLOID,


    /* Useful for debugging */
    SPECIAL_MATERIAL_NORMALS,

    /* Useful for debugging */
    SPECIAL_MATERIAL_TEXCOORD,

    /* Always last */
    SPECIAL_MATERIAL_COUNT,
    SPECIAL_MATERIAL_UNKNOWN = SPECIAL_MATERIAL_COUNT

} scene_renderer_materials_special_material;

/** TODO.
 *
 *  Internal usage only. This function should ONLY be used by demo_app.
 **/
PUBLIC scene_renderer_materials scene_renderer_materials_create();

/** TODO */
PUBLIC EMERALD_API void scene_renderer_materials_force_mesh_material_shading_property_attachment(scene_renderer_materials          materials,
                                                                                                 mesh_material_shading_property    property,
                                                                                                 mesh_material_property_attachment attachment,
                                                                                                 void*                             attachment_data);

/** TODO */
PUBLIC mesh_material scene_renderer_materials_get_special_material(scene_renderer_materials                  materials,
                                                                   ral_context                               context,
                                                                   scene_renderer_materials_special_material material_type);

/** TODO.
 *
 *  Input scene can be NULL, in which case it is assumed that the returned ogl_uber
 *  need not take lighting into consideration.
 **/
PUBLIC scene_renderer_uber scene_renderer_materials_get_uber(scene_renderer_materials materials,
                                                             mesh_material            material,
                                                             scene                    scene_instance,
                                                             bool                     use_shadow_maps);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void scene_renderer_materials_release(scene_renderer_materials materials);

#endif /* SCENE_RENDERER_MATERIALS_H */