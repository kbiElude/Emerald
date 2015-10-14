#include "stage_intro.h"
#include "demo/demo_loader.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"
#include "spinner.h"

/* Forward declarations */
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_render(ogl_context context,
                                                      uint32_t    frame_index,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused);


/** TODO */
PRIVATE void _stage_intro_configure_timeline(demo_timeline timeline,
                                             void*         unused)
{
    demo_timeline_segment_pass intro_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Intro video segment (main pass)"),
            stage_intro_render,
            NULL /* user_arg */
        }
    };

    demo_timeline_add_video_segment(timeline,
                                    system_hashed_ansi_string_create("Intro video segment"),
                                    system_time_get_time_for_s   (0),    /* start_time */
                                    system_time_get_time_for_msec(5650), /* end_time */
                                    1280.0f / 720.0f,                    /* aspect_ratio */
                                    1,                                   /* n_passes */
                                    intro_segment_passes,
                                    NULL);                               /* out_op_segment_id_ptr */
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
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_render(ogl_context context,
                                                      uint32_t    frame_index,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused)
{
    spinner_render_to_default_fbo(frame_index);
}


/** Please see header for specification */
PUBLIC void stage_intro_deinit(ogl_context context)
{
    /* Nothing to deinit at the moment */
}

/** Please see header for specification */
PUBLIC void stage_intro_init(demo_loader loader)
{
    demo_loader_op_configure_timeline configure_timeline_op;

    configure_timeline_op.pfn_configure_timeline_proc = _stage_intro_configure_timeline;
    configure_timeline_op.user_arg                    = NULL;

    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_CONFIGURE_TIMELINE,
                                 &configure_timeline_op);
}

