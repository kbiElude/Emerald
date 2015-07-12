/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef OGL_MATERIALS_H
#define OGL_MATERIALS_H

#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_types.h"

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

} _ogl_materials_special_material;

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_materials ogl_materials_create(ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_materials_force_mesh_material_shading_property_attachment(ogl_materials                     materials,
                                                                                      mesh_material_shading_property    property,
                                                                                      mesh_material_property_attachment attachment,
                                                                                      void*                             attachment_data);

/** TODO */
PUBLIC mesh_material ogl_materials_get_special_material(ogl_materials,
                                                        _ogl_materials_special_material);

/** TODO.
 *
 *  Input scene can be NULL, in which case it is assumed that the returned ogl_uber
 *  need not take lighting into consideration.
 **/
PUBLIC ogl_uber ogl_materials_get_uber(ogl_materials,
                                       mesh_material,
                                       scene,
                                       bool          use_shadow_maps);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_materials_release(ogl_materials materials);

#endif /* OGL_MATERIALS_H */