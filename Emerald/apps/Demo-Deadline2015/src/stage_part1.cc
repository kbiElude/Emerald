#include "stage_part1.h"
#include "audio/audio_stream.h"
#include "demo/demo_loader.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_video_segment.h"
#include "ogl/ogl_context.h"
#include "system/system_window.h"


/* Forward declarations */
PUBLIC RENDERING_CONTEXT_CALL void stage_part1_render(ogl_context context,
                                                      uint32_t    frame_index,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused);


/** TODO */
PRIVATE void _stage_part1_configure_timeline(demo_timeline timeline,
                                             void*         unused)
{
    const demo_timeline_video_segment_pass part_1_video_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Part 1 (main pass)"),
            stage_part1_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_video_segment            part_1_video_segment = NULL;

    demo_timeline_add_video_segment(timeline,
                                    system_hashed_ansi_string_create("Part 1 video segment"),
                                    system_time_get_time_for_msec(5650), /* start_time */
                                    system_time_get_time_for_msec(25850),/* end_time */
                                    1280.0f / 720.0f,                    /* aspect_ratio */
                                    NULL,                                /* out_opt_segment_id_ptr */
                                    &part_1_video_segment);

    demo_timeline_video_segment_add_passes(part_1_video_segment,
                                           1,  /* n_passes */
                                           part_1_video_segment_passes);
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
PUBLIC RENDERING_CONTEXT_CALL void stage_part1_render(ogl_context context,
                                                      uint32_t    frame_index,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused)
{
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* FFT-based audio sync test for now */
    audio_stream  current_audio_stream = NULL;
    system_window current_window       = NULL;
    float         fft[32];

    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &current_window);
    system_window_get_property(current_window,
                               SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                              &current_audio_stream);

    audio_stream_get_fft_averages(current_audio_stream,
                                  4, /* n_result_frequency_bands */
                                  fft);

    const uint8_t fft_index = 1;

    if (fft[fft_index] < 0.025f)
    {
        fft[fft_index] = 0.0f;
    }

    entrypoints_ptr->pGLClearColor(.2f + fft[fft_index],
                                   .1f + fft[fft_index],
                                   .3f + fft[fft_index],
                                   1.0f);
    entrypoints_ptr->pGLClear     (GL_COLOR_BUFFER_BIT);
}


/** Please see header for specification */
PUBLIC void stage_part1_deinit(ogl_context context)
{
    /* Nothing to deinit at the moment */
}

/** Please see header for specification */
PUBLIC void stage_part1_init(demo_loader loader)
{
    demo_loader_op_configure_timeline configure_timeline_op;

    configure_timeline_op.pfn_configure_timeline_proc = _stage_part1_configure_timeline;
    configure_timeline_op.user_arg                    = NULL;

    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_CONFIGURE_TIMELINE,
                                 &configure_timeline_op);
}
