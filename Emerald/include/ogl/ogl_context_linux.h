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
PUBLIC void ogl_context_linux_deinit_global();

/** TODO */
PUBLIC void ogl_context_linux_enumerate_supported_msaa_modes(__in ogl_context_linux context_linux);

/** TODO
 *
 *  @param out_fb_configs_ptr TODO. Must not be NULL. Result list must be freed with a XFree() call */
PUBLIC void ogl_context_linux_get_fb_configs_for_gl_window(system_window window,
                                                           GLXFBConfig** out_fb_configs_ptr,
                                                           int*          out_n_compatible_fb_configs_ptr);

/** TODO */
PUBLIC void* ogl_context_linux_get_func_ptr(__in ogl_context_linux context_linux,
                                            __in const char*       name);

/** TODO */
PUBLIC bool ogl_context_linux_get_property(__in  ogl_context_linux    context_linux,
                                           __in  ogl_context_property property,
                                           __out void*                out_result);

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
PUBLIC void ogl_context_linux_init(__in ogl_context                     context,
                                   __in PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation);

/** TODO */
PUBLIC void ogl_context_linux_init_global();

/** TODO */
PUBLIC bool ogl_context_linux_set_property(__in ogl_context_linux    context_linux,
                                           __in ogl_context_property property,
                                           __in const void*          data);

/** TODO */
PUBLIC void ogl_context_linux_swap_buffers(__in ogl_context_linux context_linux);

/** TODO */
PUBLIC void ogl_context_linux_unbind_from_current_thread(__in ogl_context_linux context_linux);

#endif /* OGL_CONTEXT_LINUX_H */
