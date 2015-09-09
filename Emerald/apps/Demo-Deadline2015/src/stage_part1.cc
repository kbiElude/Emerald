#include "stage_part1.h"
#include "ogl/ogl_context.h"

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_part1_deinit()
{
    /* Nothing to deinit at the moment */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_part1_init(ogl_context context)
{
    /* Nothing to initialize at the moment */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_part1_render(ogl_context context,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused)
{
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Dummy content for now! */
    entrypoints_ptr->pGLClearColor(.0f, .5f, .2f, 1.0f);
    entrypoints_ptr->pGLClear     (GL_COLOR_BUFFER_BIT);
}