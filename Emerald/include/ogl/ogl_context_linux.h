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
PUBLIC void ogl_context_linux_bind_to_current_thread(ogl_context_linux context_linux);

/** TODO */
PUBLIC void ogl_context_linux_deinit(ogl_context_linux context_linux);

/** TODO */
PUBLIC void ogl_context_linux_deinit_global();

/** TODO
 *
 *  @param out_fb_configs_ptr TODO. Must not be NULL. Result list must be freed with a XFree() call */
PUBLIC void ogl_context_linux_get_fb_configs_for_gl_window(system_window window,
                                                           GLXFBConfig** out_fb_configs_ptr,
                                                           int*          out_n_compatible_fb_configs_ptr);

/** TODO */
PUBLIC void* ogl_context_linux_get_func_ptr(ogl_context_linux context_linux,
                                            const char*       name);

/** TODO */
PUBLIC bool ogl_context_linux_get_property(ogl_context_linux    context_linux,
                                           ogl_context_property property,
                                           void*                out_result);

/** TODO.
 *
 *  @return Requested XVisualInfo instance. Must be released with XFree() when no longer needed.
 */
PUBLIC XVisualInfo* ogl_context_linux_get_visual_info_for_gl_window(system_window window);

/** TODO
 *
 *  @param context TODO. NOT retained at call-time.
 *
 *  @return TODO
 */
PUBLIC void ogl_context_linux_init(ogl_context                     context,
                                   PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation);

/** TODO */
PUBLIC void ogl_context_linux_init_global();

/** TODO */
PUBLIC bool ogl_context_linux_set_property(ogl_context_linux    context_linux,
                                           ogl_context_property property,
                                           const void*          data);

/** TODO */
PUBLIC void ogl_context_linux_swap_buffers(ogl_context_linux context_linux);

/** TODO */
PUBLIC void ogl_context_linux_unbind_from_current_thread(ogl_context_linux context_linux);

#endif /* OGL_CONTEXT_LINUX_H */
