/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef OGL_SHADOW_MAPPING_H
#define OGL_SHADOW_MAPPING_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_shadow_mapping);


/** TODO */
PUBLIC ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context);

/** TODO. **/
PUBLIC void ogl_shadow_mapping_release(__in __notnull ogl_shadow_mapping handler);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_shadow_mapping_toggle(__in __notnull ogl_shadow_mapping handler,
                                                                            __in __notnull scene_light                       light,
                                                                            __in           bool                              should_enable);

#endif /* OGL_SHADOW_MAPPING_H */