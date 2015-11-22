/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Helps cache texture object binding calls. Note that these functions are private only.
 *
 * Makes use of GL_ARB_multi_bind if available
 */
#ifndef OGL_CONTEXT_TO_BINDINGS_H
#define OGL_CONTEXT_TO_BINDINGS_H

#include "ogl/ogl_types.h"

enum ogl_context_to_bindings_sync_bit
{
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_1D                   = 1 << 0,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_1D_ARRAY             = 1 << 1,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D                   = 1 << 2,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D_ARRAY             = 1 << 3,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D_MULTISAMPLE       = 1 << 4,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D_MULTISAMPLE_ARRAY = 1 << 5,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_3D                   = 1 << 6,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER               = 1 << 7,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_CUBE_MAP             = 1 << 8,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_CUBE_MAP_ARRAY       = 1 << 9,
    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_RECTANGLE            = 1 << 10,

    OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL = 0xFFFFFFFF
};

/* Declare private handle */
DECLARE_HANDLE(ogl_context_to_bindings);

/** TODO */
PUBLIC ogl_context_to_bindings ogl_context_to_bindings_create(ogl_context context);

/** TODO */
PUBLIC GLuint ogl_context_to_bindings_get_bound_texture_id(const ogl_context_to_bindings to_bindings,
                                                           GLuint                        texture_unit,
                                                           GLenum                        target);

/** TODO */
PUBLIC ogl_context_to_bindings_sync_bit ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(GLenum binding_target);

/** TODO */
PUBLIC void ogl_context_to_bindings_init(ogl_context_to_bindings                   bindings,
                                         const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_to_bindings_release(ogl_context_to_bindings bindings);

/** TODO */
PUBLIC void ogl_context_to_bindings_reset_all_bindings_for_texture_unit(ogl_context_to_bindings to_bindings,
                                                                        uint32_t                texture_unit);

/** TODO */
PUBLIC void ogl_context_to_bindings_set_binding(ogl_context_to_bindings bindings,
                                                GLuint                  texture_unit,
                                                GLenum                  target,
                                                GLuint                  texture_id);

/** TODO
 *
 *  @param bindings  TODO
 *
 */
PUBLIC void ogl_context_to_bindings_sync(ogl_context_to_bindings          bindings,
                                         ogl_context_to_bindings_sync_bit sync_bits);

#endif /* OGL_CONTEXT_TO_BINDINGS_H */
