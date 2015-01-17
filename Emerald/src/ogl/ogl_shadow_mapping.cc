/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shadow_mapping.h"
#include "ogl/ogl_shader.h"
#include "scene/scene.h"
#include "system/system_matrix4x4.h"
#include <string>
#include <sstream>


/** TODO */
typedef struct _ogl_shadow_mapping
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    _ogl_shadow_mapping()
    {
        context = NULL;
    }

} _ogl_shadow_mapping;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_shadow_mapping_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_shadow_mapping_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_context_scene_renderer_shadow_mapping_init_program(__in __notnull _ogl_shadow_mapping* handler_ptr)
{
    // ...
}

/** TODO */
PRIVATE void _ogl_shadow_mapping_release_renderer_callback(__in __notnull ogl_context context,
                                                           __in __notnull void*       handler)
{
    _ogl_shadow_mapping* handler_ptr = (_ogl_shadow_mapping*) handler;

    // ..
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_shadow_mapping_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_shadow_mapping is only supported under GL contexts")
    }
#endif


/** Please see header for spec */
PUBLIC ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context)
{
    _ogl_shadow_mapping* new_instance = new (std::nothrow) _ogl_shadow_mapping;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    if (new_instance != NULL)
    {
        new_instance->context = context;
    } /* if (new_instance != NULL) */

    return (ogl_shadow_mapping) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_shadow_mapping_release(__in __notnull __post_invalid ogl_shadow_mapping handler)
{
    _ogl_shadow_mapping* handler_ptr = (_ogl_shadow_mapping*) handler;

    ogl_context_request_callback_from_context_thread(handler_ptr->context,
                                                     _ogl_shadow_mapping_release_renderer_callback,
                                                     handler_ptr);

    delete handler_ptr;
    handler_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_shadow_mapping_toggle(__in __notnull ogl_shadow_mapping handler,
                                                             __in __notnull scene_light        light,
                                                             __in           bool               should_enable)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _ogl_shadow_mapping*              handler_ptr     = (_ogl_shadow_mapping*) handler;

    /* Configure color/depth masks according to whether SM is being brought up or down */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(handler_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (should_enable)
    {
        entry_points->pGLColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        entry_points->pGLDepthMask(GL_TRUE);
    }
    else
    {
        entry_points->pGLColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        entry_points->pGLDepthMask(GL_TRUE);
    }
}
