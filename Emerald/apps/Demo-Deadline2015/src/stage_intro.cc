#include "stage_intro.h"
#include "ogl/ogl_context.h"

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_deinit(ogl_context context)
{
    /* Nothing to deinit at the moment */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_init(ogl_context context)
{
    /* Nothing to initialize at the moment */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_render(ogl_context context,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused)
{
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Dummy content for now! */
    entrypoints_ptr->pGLClearColor(.2f, .1f, .3f, 1.0f);
    entrypoints_ptr->pGLClear     (GL_COLOR_BUFFER_BIT);
}