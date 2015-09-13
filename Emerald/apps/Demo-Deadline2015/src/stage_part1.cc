#include "stage_part1.h"
#include "audio/audio_stream.h"
#include "ogl/ogl_context.h"
#include "system/system_window.h"


/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_part1_deinit(ogl_context context)
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