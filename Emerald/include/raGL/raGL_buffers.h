/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO
 *
 * If sparse buffers are supported, raGL_buffers carves slices of buffer memory out
 * of cached sparse buffers, which are stored in an internal pool.
 *
 * If not, a pool of buffer objects is preallocated at creation time, and filled with
 * data during execution. Alloc requests whose size is larger than the size of each
 * preallocated BO get their exclusive BO.
 *
 *
 * The instantiated regions come from immutable buffer objects.
 *
 * Finally, since raGL_buffers keeps track of all allocated buffers, it can also provide
 * info regarding:
 *
 * a) committed buffer memory (if sparse buffers are supported)
 * b) reserved buffer memory  (otherwise)
 */
#ifndef RAGL_BUFFERS_H
#define RAGL_BUFFERS_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_buffers);


/* NOTE: Buffer memory manager is ref-counted, in order to correctly support context sharing for
 *       the OpenGL back-end. */
REFCOUNT_INSERT_DECLARATIONS(raGL_buffers,
                             raGL_buffers);

/** TODO
 *
 *  NOTE: This functionality should only be accessed via raGL_backend_*(), unless you're directly
 *        using the GL back-end.
 *
 *  TODO
 */
PUBLIC RENDERING_CONTEXT_CALL bool raGL_buffers_allocate_buffer_memory_for_ral_buffer(raGL_buffers buffers,
                                                                                      ral_buffer   buffer,
                                                                                      raGL_buffer* out_buffer_ptr);

/** TODO.
 *
 *  NOTE: This is a temporary function that will be removed after RAL integration is finished.
 *        DO NOT use.
 **/
PUBLIC RENDERING_CONTEXT_CALL bool raGL_buffers_allocate_buffer_memory_for_ral_buffer_create_info(raGL_buffers                  buffers,
                                                                                                  const ral_buffer_create_info* buffer_create_info_ptr,
                                                                                                  raGL_buffer*                  out_buffer_ptr);

/** TODO.
 *
 *  NOTE: This functionality should only be accessed via raGL_backend_*().
 *
 *  TODO
 **/
PUBLIC raGL_buffers raGL_buffers_create(raGL_backend backend,
                                        ogl_context  context);

/** TODO
 *
 *  NOTE: This functionality should only be accessed via raGL_backend_*(), unless you're directly
 *        controlling the back-end.
 *
 *  TODO */
PUBLIC void raGL_buffers_free_buffer_memory(raGL_buffers buffers,
                                            raGL_buffer  buffer);

/** TODO */
PUBLIC void raGL_buffers_get_buffer_property(raGL_buffers                 buffers,
                                             GLuint                       bo_id,
                                             raGL_buffers_buffer_property property,
                                             void*                        out_result_ptr);

#endif /* RAGL_BUFFERS_H */
