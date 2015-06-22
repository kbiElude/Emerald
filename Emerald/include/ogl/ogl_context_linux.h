/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Internal use only.
 */
#ifndef OGL_CONTEXT_LINUX_H
#define OGL_CONTEXT_LINUX_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


/** TODO */
PUBLIC void ogl_context_linux_bind_to_current_thread(__in ogl_context_linux context_linux);

/** TODO */
PUBLIC void ogl_context_linux_deinit(__in __post_invalid ogl_context_linux context_linux);

/** TODO */
PUBLIC void* ogl_context_linux_get_func_ptr(__in ogl_context_linux context_linux,
                                            __in const char*       name);

/** TODO */
PUBLIC bool ogl_context_linux_get_property(__in  ogl_context_linux    context_linux,
                                           __in  ogl_context_property property,
                                           __out void*                out_result);

/** TODO
 *
 *  @param context TODO. NOT retained at call-time.
 *
 *  @return TODO
 */
PUBLIC void ogl_context_linux_init(__in ogl_context                     context,
                                   __in PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation);

/** TODO */
PUBLIC void ogl_context_linux_init_msaa(__in ogl_context_linux context_linux);

/** TODO */
PUBLIC bool ogl_context_linux_set_property(__in ogl_context_linux    context_linux,
                                           __in ogl_context_property property,
                                           __in const void*          data);

/** TODO */
PUBLIC void ogl_context_linux_swap_buffers(__in ogl_context_linux context_linux);

/** TODO */
PUBLIC void ogl_context_linux_unbind_from_current_thread();

#endif /* OGL_CONTEXT_LINUX_H */
