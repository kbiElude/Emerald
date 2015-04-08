/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Helps cache buffer object binding calls. Note that these functions are private only.
 *
 * Makes use of the following GL extensions if available:
 *
 * GL_ARB_multi_bind
 */
#ifndef OGL_CONTEXT_BO_BINDINGS_H
#define OGL_CONTEXT_BO_BINDINGS_H

#include "ogl/ogl_types.h"

enum ogl_context_bo_bindings_sync_bit
{
    BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER              = 1 << 0,
    BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     = 1 << 1,
    BO_BINDINGS_SYNC_BIT_COPY_READ_BUFFER          = 1 << 2,
    BO_BINDINGS_SYNC_BIT_COPY_WRITE_BUFFER         = 1 << 3,
    BO_BINDINGS_SYNC_BIT_DISPATCH_INDIRECT_BUFFER  = 1 << 4,
    BO_BINDINGS_SYNC_BIT_DRAW_INDIRECT_BUFFER      = 1 << 5,
    BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER         = 1 << 6,
    BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER       = 1 << 7,
    BO_BINDINGS_SYNC_BIT_QUERY_BUFFER              = 1 << 8,
    BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     = 1 << 9,
    BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER            = 1 << 10,
    BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER = 1 << 11,
    BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER            = 1 << 12,

    BO_BINDINGS_SYNC_BIT_ALL                       = 0xFFFFFFFF
};

/* Declare private handle */
DECLARE_HANDLE(ogl_context_bo_bindings);

/** TODO */
PUBLIC ogl_context_bo_bindings ogl_context_bo_bindings_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC GLuint ogl_context_bo_bindings_get_general_binding(__in __notnull const ogl_context_bo_bindings bo_bindings,
                                                          __in           GLenum                        target);

/** TODO */
PUBLIC ogl_context_bo_bindings_sync_bit ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(__in GLenum binding_target);

/** TODO */
PUBLIC void ogl_context_bo_bindings_init(__in __notnull ogl_context_bo_bindings                   bindings,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_bo_bindings_release(__in __notnull __post_invalid ogl_context_bo_bindings bindings);

/** TODO */
PUBLIC void ogl_context_bo_bindings_reset_buffer_bindings(__in __notnull ogl_context_bo_bindings bindings,
                                                          __in __notnull const GLuint*           buffers,
                                                          __in           GLint                   n);

/** TODO */
PUBLIC void ogl_context_bo_bindings_set_binding(__in __notnull ogl_context_bo_bindings bindings,
                                                __in           GLenum                  binding_point,
                                                __in           GLint                   bo_id);

/** TODO */
PUBLIC void ogl_context_bo_bindings_set_binding_base(__in __notnull ogl_context_bo_bindings bindings,
                                                     __in           GLenum                  binding_point,
                                                     __in           GLuint                  binding_index,
                                                     __in           GLint                   bo_id);

/** TODO */
PUBLIC void ogl_context_bo_bindings_set_binding_range(__in __notnull ogl_context_bo_bindings bindings,
                                                      __in           GLenum                  binding_point,
                                                      __in           GLuint                  binding_index,
                                                      __in           GLintptr                offset,
                                                      __in           GLsizeiptr              size,
                                                      __in           GLint                   bo_id);

/** TODO */
PUBLIC void ogl_context_bo_bindings_set_bo_storage_size(__in __notnull ogl_context_bo_bindings bindings,
                                                        __in           GLuint                  bo_id,
                                                        __in           GLsizeiptr              bo_size);

/** TODO
 *
 *  @param bindings  TODO
 *  @param sync_bits TODO. Use bits from ogl_context_bo_bindings_sync_bit.
 *
 */
PUBLIC void ogl_context_bo_bindings_sync(__in __notnull ogl_context_bo_bindings bindings,
                                         __in           uint32_t                sync_bits);

#endif /* OGL_CONTEXT_H */
