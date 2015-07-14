/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Internal use only.
 */
#ifndef OGL_CONTEXT_WIN32_H
#define OGL_CONTEXT_WIN32_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


/** TODO */
PUBLIC void ogl_context_win32_bind_to_current_thread(ogl_context_win32 context_win32);

/** TODO */
PUBLIC void ogl_context_win32_deinit(ogl_context_win32 context_win32);

/** TODO */
PUBLIC void* ogl_context_win32_get_func_ptr(ogl_context_win32 context_win32,
                                            const char*       name);

/** TODO */
PUBLIC bool ogl_context_win32_get_property(ogl_context_win32    context_win32,
                                           ogl_context_property property,
                                           void*                out_result);

/** TODO
 *
 *  @param context TODO. NOT retained at call-time.
 *
 *  @return TODO
 */
PUBLIC void ogl_context_win32_init(ogl_context                     context,
                                   PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation);

/** TODO */
PUBLIC bool ogl_context_win32_set_property(ogl_context_win32    context_win32,
                                           ogl_context_property property,
                                           const void*          data);

/** TODO */
PUBLIC void ogl_context_win32_swap_buffers(ogl_context_win32 context_win32);

/** TODO */
PUBLIC void ogl_context_win32_unbind_from_current_thread(ogl_context_win32 context_win32);

#endif /* OGL_CONTEXT_WIN32_H */
