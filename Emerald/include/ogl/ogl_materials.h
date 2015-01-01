/**
 *
 * Emerald (kbi/elude @2014)
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

    SPECIAL_MATERIAL_NORMALS  = SPECIAL_MATERIAL_FIRST,
    SPECIAL_MATERIAL_TEXCOORD,

    /* Always last */
    SPECIAL_MATERIAL_COUNT
} _ogl_materials_special_material;

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_materials ogl_materials_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_materials_force_mesh_material_shading_property_attachment(__in __notnull ogl_materials                     materials,
                                                                                      __in           mesh_material_shading_property    property,
                                                                                      __in           mesh_material_property_attachment attachment,
                                                                                      __in __notnull void*                             attachment_data);

/** TODO */
PUBLIC mesh_material ogl_materials_get_special_material(__in __notnull ogl_materials,
                                                        __in           _ogl_materials_special_material);

/** TODO.
 *
 *  Input scene can be NULL, in which case it is assumed that the returned ogl_uber
 *  need not take lighting into consideration.
 **/
PUBLIC ogl_uber ogl_materials_get_uber(__in     __notnull ogl_materials,
                                       __in     __notnull mesh_material,
                                       __in_opt           scene);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_materials_release(__in __notnull ogl_materials materials);

#endif /* OGL_MATERIALS_H */