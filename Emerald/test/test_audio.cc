/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "gtest/gtest.h"
#include "test_audio.h"
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_file_serializer.h"
#include "system/system_time.h"


TEST(AudioTest, StreamPlaybackTest)
{
    bool                      context_result     = false;
    bool                      result             = false;
    system_file_serializer    test_serializer    = NULL;
    audio_stream              test_stream        = NULL;
    demo_window               window             = NULL;
    demo_window_create_info   window_create_info;
    system_hashed_ansi_string window_name        = system_hashed_ansi_string_create("Test window");
    int                       window_x1y1x2y2[4] = {0};

    window_create_info.target_rate   = 5;
    window_create_info.resolution[0] = 32;
    window_create_info.resolution[1] = 32;
    window_create_info.visible       = false;

    window = demo_app_create_window(window_name,
                                    window_create_info,
                                    RAL_BACKEND_TYPE_GL);

    /* Set up an audio stream.
     *
     * NOTE: test.mp3 was taken from https://archive.org/details/testmp3testfile */
    test_serializer = system_file_serializer_create_for_reading(system_hashed_ansi_string_create("test.mp3") );

    ASSERT_TRUE(test_serializer != NULL);

    test_stream = audio_stream_create(audio_device_get_default_device(),
                                      test_serializer,
                                      window);

    ASSERT_TRUE(test_stream != NULL);

    /* The window now owns the audio stream, so we may safely release the audio stuff */
    audio_stream_release          (test_stream);
    system_file_serializer_release(test_serializer);

    test_serializer = NULL;
    test_stream     = NULL;

    /* Launch the playback */
    printf("Playing from the beginning for 1s..\n");

    result = demo_window_start_rendering(window,
                                         0); /* rendering_start_time */

    /* Wait for 1s */
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(1000000);
#endif

    /* Restart the playback, this time from the second second. */
    printf("Playing from 2s for 1s..\n");

    result = demo_window_stop_rendering(window);
    ASSERT_TRUE(result);

    result = demo_window_start_rendering(window,
                                         system_time_get_time_for_msec(2000) );
    ASSERT_TRUE(result);

    /* Wait for 1s */
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(1000000);
#endif

    demo_app_destroy_window(window_name);
}

