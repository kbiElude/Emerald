/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_po_uniforms.h"
#include "ogl/ogl_sampler.h"
#include "system/system_hash64map.h"

/* TODO: Only template is implemented atm! */







/** TODO */
typedef struct _ogl_context_po_uniforms
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;
    bool        dirty;

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;
} _ogl_context_po_uniforms;


/** Please see header for spec */
PUBLIC ogl_context_po_uniforms ogl_context_po_uniforms_create(__in __notnull ogl_context context)
{
    _ogl_context_po_uniforms* new_instance = new (std::nothrow) _ogl_context_po_uniforms;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        new_instance->context = context;
    } /* if (new_instance != NULL) */

    return (ogl_context_po_uniforms) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_context_po_uniforms_init(__in __notnull ogl_context_po_uniforms                   po_uniforms,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_po_uniforms* po_uniforms_ptr = (_ogl_context_po_uniforms*) po_uniforms;

    /* Cache info in private descriptor */
    po_uniforms_ptr->entrypoints_private_ptr = entrypoints_private_ptr;
}

/** Please see header for spec */
PUBLIC void ogl_context_po_uniforms_release(__in __notnull __post_invalid ogl_context_po_uniforms po_uniforms)
{
    _ogl_context_po_uniforms* po_uniforms_ptr = (_ogl_context_po_uniforms*) po_uniforms;

    /* Done */
    delete po_uniforms_ptr;

    po_uniforms_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_context_po_uniforms_sync(__in __notnull ogl_context_po_uniforms po_uniforms)
{
    /* NOTE: po_uniforms is NULL during rendering context initialization */
    if (po_uniforms != NULL)
    {
        _ogl_context_po_uniforms* po_uniforms_ptr = (_ogl_context_po_uniforms*) po_uniforms;

        // ...
    } /* if (po_uniforms != NULL) */
}