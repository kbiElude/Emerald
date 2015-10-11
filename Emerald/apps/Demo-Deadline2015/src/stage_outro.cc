#include "stage_outro.h"
#include "demo/demo_loader.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"

/* Forward declarations */
PUBLIC RENDERING_CONTEXT_CALL void stage_outro_render(ogl_context context,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused);


/** TODO */
PRIVATE void _stage_outro_configure_timeline(demo_timeline timeline,
                                             void*         unused)
{
    demo_timeline_segment_pass outro_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Outro video segment (main pass)"),
            stage_outro_render,
            NULL /* user_arg */
        }
    };

    demo_timeline_add_video_segment(timeline,
                                    system_hashed_ansi_string_create("Outro video segment"),
                                    system_time_get_time_for_msec(89000), /* start_time */
                                    system_time_get_time_for_msec(98000), /* end_time */
                                    1280.0f / 720.0f,                     /* aspect_ratio */
                                    1,                                    /* n_passes */
                                    outro_segment_passes,
                                    NULL);                                /* out_opt_segment_id_ptr */
}

/** Renders frame contents.
 *
 *  @param context                   Rendering context.
 *  @param frame_time                Frame time (relative to the beginning of the timeline).
 *  @param rendering_area_px_topdown Rendering area, to which frame contents should be rendered.
 *                                   The four ints (x1/y1/x2/y2) are calculated, based on the aspect
 *                                   ratio defined for the video segment.
 *                                   It is your responsibility to update the scissor area / viewport.
 *  @param unused                    Rendering call-back user argument. We're not using this, so
 *                                   the argument is always NULL.
 **/
PUBLIC RENDERING_CONTEXT_CALL void stage_outro_render(ogl_context context,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused)
{
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Dummy content for now! */
    entrypoints_ptr->pGLClearColor(.3f, .4f, .5f, 1.0f);
    entrypoints_ptr->pGLClear     (GL_COLOR_BUFFER_BIT);
}


/** Please see header for specification */
PUBLIC void stage_outro_deinit(ogl_context context)
{
    /* Nothing to deinit at the moment */
}

/** Please see header for specification */
PUBLIC void stage_outro_init(demo_loader loader)
{
    demo_loader_op_configure_timeline configure_timeline_op;

    configure_timeline_op.pfn_configure_timeline_proc = _stage_outro_configure_timeline;
    configure_timeline_op.user_arg                    = NULL;

    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_CONFIGURE_TIMELINE,
                                 &configure_timeline_op);
}
