/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"

/** Please see header for specification */
PUBLIC void deinit_ogl_context_gl_info(ogl_context_gl_info* info)
{
    delete [] info->extensions;
    info->extensions = NULL;

    info->renderer                 = NULL;
    info->shading_language_version = NULL;
    info->vendor                   = NULL;
    info->version                  = NULL;
}

/** Please see header for specification */
PUBLIC void init_ogl_context_gl_info(__in __notnull       ogl_context_gl_info*   info,
                                     __in __notnull const ogl_context_gl_limits* limits)
{
    info->extensions = new (std::nothrow) const GLubyte*[limits->num_extensions];

    ASSERT_ALWAYS_SYNC(info->extensions != NULL, 
                       "Out of memory while allocating extension name array (%d entries)", 
                       limits->num_extensions);
}
