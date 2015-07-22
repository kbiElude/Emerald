/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "test_audio.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_file_serializer.h"
#include "system/system_pixel_format.h"
#include "system/system_time.h"
#include "system/system_window.h"


TEST(AudioTest, StreamPlaybackTest)
{
    bool                   context_result     = false;
    ogl_rendering_handler  rendering_handler  = NULL;
    bool                   result             = false;
    system_screen_mode     screen_mode        = NULL;
    system_file_serializer test_serializer    = NULL;
    audio_stream           test_stream        = NULL;
    system_window          window             = NULL;
    int                    window_size    [2] = {32, 32};
    int                    window_x1y1x2y2[4] = {0};

    /* Set up a renderer window */
    system_pixel_format window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                               8,  /* color_buffer_green_bits */
                                                               8,  /* color_buffer_blue_bits  */
                                                               0,  /* color_buffer_alpha_bits */
                                                               24, /* depth_buffer_bits       */
                                                               SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                                               0); /* stencil_buffer_bits     */

    ASSERT_TRUE(window_pf != NULL);

    system_window_get_centered_window_position_for_primary_monitor(window_size,
                                                                   window_x1y1x2y2);

    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Test window"),
                                                 false, /* scalable */
                                                 false, /* vsync_enabled */
                                                 false, /* visible */
                                                 window_pf);

    ASSERT_TRUE(window != NULL);

    /* Set up an audio stream.
     *
     * NOTE: test.mp3 was taken from https://archive.org/details/testmp3testfile */
    test_serializer = system_file_serializer_create_for_reading(system_hashed_ansi_string_create("test.mp3") );

    ASSERT_TRUE(test_serializer != NULL);

    test_stream = audio_stream_create(audio_device_get_default_device(),
                                      test_serializer,
                                      window);

    ASSERT_TRUE(test_stream != NULL);

    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                              &test_stream);

    /* The window now owns the audio stream, so we may safely release the audio stuff */
    audio_stream_release          (test_stream);
    system_file_serializer_release(test_serializer);

    test_serializer = NULL;
    test_stream     = NULL;

    /* Set up a rendering handler */
    rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                     5,     /* fps */
                                                                     NULL,  /* rendering_handler */
                                                                     NULL); /* rendering_handler_user_arg */

    ASSERT_TRUE(rendering_handler != NULL);

    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    /* Launch the playback */
    printf("Playing from the beginning for 1s..\n");

    result = ogl_rendering_handler_play(rendering_handler,
                                        0); /* time */
    ASSERT_TRUE(result);

    /* Wait for 1s */
    Sleep(1000);

    /* Restart the playback, this time from the second second. */
    printf("Playing from 2s for 1s..\n");

    result = ogl_rendering_handler_stop(rendering_handler);
    ASSERT_TRUE(result);

    result = ogl_rendering_handler_play(rendering_handler,
                                        system_time_get_time_for_msec(2000) );
    ASSERT_TRUE(result);

    /* Wait for 1s */
    Sleep(1000);

    ogl_rendering_handler_stop(rendering_handler);

    system_window_close(window);
}

