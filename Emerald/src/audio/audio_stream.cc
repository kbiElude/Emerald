/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "bass.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "demo/demo_window.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_time.h"
#include "system/system_window.h"

typedef enum
{
    AUDIO_STREAM_PLAYBACK_STATUS_PAUSED,
    AUDIO_STREAM_PLAYBACK_STATUS_PLAYING,
    AUDIO_STREAM_PLAYBACK_STATUS_STOPPED,
} _audio_stream_playback_status;

typedef struct _audio_stream
{
    system_callback_manager       callback_manager;
    audio_device                  device;
    HSYNC                         finished_playing_sync_event;
    bool                          is_playing;
    _audio_stream_playback_status playback_status;
    system_file_serializer        serializer;
    HSTREAM                       stream;

    REFCOUNT_INSERT_VARIABLES


    explicit _audio_stream(audio_device           in_device,
                           system_file_serializer in_serializer)
    {
        callback_manager            = system_callback_manager_create( (_callback_id) AUDIO_STREAM_CALLBACK_ID_COUNT);
        device                      = in_device;
        finished_playing_sync_event = NULL;
        is_playing                  = false;
        playback_status             = AUDIO_STREAM_PLAYBACK_STATUS_STOPPED;
        serializer                  = in_serializer;
        stream                      = (HSTREAM) NULL;

        system_file_serializer_retain(in_serializer);
    }

    ~_audio_stream()
    {
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        }

        if (serializer != NULL)
        {
            system_file_serializer_release(serializer);

            serializer = NULL;
        } /* if (serializer != NULL) */

        if (stream != (HSTREAM) NULL)
        {
            BASS_StreamFree(stream);

            stream = (HSTREAM) NULL;
        }
    }
} _audio_stream;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(audio_stream,
                               audio_stream,
                              _audio_stream);


/** TODO */
PRIVATE void CALLBACK _audio_stream_on_stream_finished_playing(HSYNC stream,
                                                               DWORD channel,
                                                               DWORD data,
                                                               void* user_arg)
{
    _audio_stream* stream_ptr = (_audio_stream*) user_arg;

    /* When we reach this entry-point, it means the audio stream has finished playing.
     * Call back any subscribers which have registered for this event. */
    ASSERT_DEBUG_SYNC(stream_ptr != NULL,
                      "NULL audio_stream instance reported for the \"on stream finished playing\" BASS sync event");

    system_callback_manager_call_back(stream_ptr->callback_manager,
                                      AUDIO_STREAM_CALLBACK_ID_FINISHED_PLAYING,
                                      stream_ptr);
}

/** TODO */
PRIVATE void _audio_stream_release(void* stream)
{
    /* Nothing to do here - deallocation handled by the destructor. */
}


/** Please see header for spec */
PUBLIC EMERALD_API audio_stream audio_stream_create(audio_device           device,
                                                    system_file_serializer serializer,
                                                    demo_window            window)
{
    bool                      is_device_activated         = false;
    _audio_stream*            new_audio_stream_ptr        = NULL;
    const char*               raw_serializer_storage      = NULL;
    size_t                    raw_serializer_storage_size = 0;
    system_hashed_ansi_string serializer_name             = NULL;
    char                      temp_buffer[256];
    system_window             window_private              = NULL;

    ASSERT_DEBUG_SYNC(device != NULL,
                      "Input audio device instance is NULL");
    ASSERT_DEBUG_SYNC(serializer != NULL,
                      "Input serializer is NULL");
    ASSERT_DEBUG_SYNC(window != NULL,
                      "Input window is NULL");

    demo_window_get_private_property(window,
                                     DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW,
                                    &window_private);

    /* Spawn the new descriptor */
    new_audio_stream_ptr = new (std::nothrow) _audio_stream(device,
                                                            serializer);

    ASSERT_ALWAYS_SYNC(new_audio_stream_ptr != NULL,
                       "Out of memory");

    /* Make sure the device is activated before we attempt to call any BASS function */
    audio_device_get_property(device,
                              AUDIO_DEVICE_PROPERTY_IS_ACTIVATED,
                             &is_device_activated);

    if (!is_device_activated)
    {
        audio_device_activate(device,
                              window_private);
    }

    /* The serializer is retained at this point, so carry on & open the stream handle */
    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,
                                       &serializer_name);
    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                       &raw_serializer_storage);
    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE,
                                       &raw_serializer_storage_size);

    new_audio_stream_ptr->stream = BASS_StreamCreateFile(TRUE, /* mem */
                                                         raw_serializer_storage,
                                                         0, /* offset */
                                                         raw_serializer_storage_size,
                                                         BASS_SAMPLE_FLOAT | BASS_STREAM_PRESCAN);


    ASSERT_ALWAYS_SYNC(new_audio_stream_ptr->stream != (HSTREAM) NULL,
                       "Could not create a BASS stream!");

    if (new_audio_stream_ptr->stream == (HSTREAM) NULL)
    {
        goto end_error;
    }

    /* Register for "stopped playing" call-backs. We need to expose this event to the users, so that
     * demo applications can quit when the sound-track finishes. */
    if ( (new_audio_stream_ptr->finished_playing_sync_event = BASS_ChannelSetSync(new_audio_stream_ptr->stream,
                                                                                  BASS_SYNC_END | BASS_SYNC_MIXTIME,
                                                                                  NULL, /* param - not used */
                                                                                  _audio_stream_on_stream_finished_playing,
                                                                                  new_audio_stream_ptr)) == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                          "Failed to set up a BASS_SYNC_MIXTIME synchronizer");

        goto end_error;
    }

    /* Register the instance in the object manager registry */
    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "Created from system file serializer [%s]",
             system_hashed_ansi_string_get_buffer(serializer_name) );

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_audio_stream_ptr,
                                                   _audio_stream_release,
                                                   OBJECT_TYPE_AUDIO_STREAM,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Audio Streams\\",
                                                                                                           temp_buffer) );

    /* Configure the audio stream as the window's audio stream.
     *
     * TODO: Windows really should not have anything to do with audio streams! :C
     */
    system_window_set_property(window_private,
                               SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                              &new_audio_stream_ptr);

    return (audio_stream) new_audio_stream_ptr;

end_error:
    /* Tear down the descriptor */
    if (new_audio_stream_ptr != NULL)
    {
        delete new_audio_stream_ptr;

        new_audio_stream_ptr = NULL;
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_stream_get_fft_averages(audio_stream stream,
                                                      uint32_t     n_result_frequency_bands,
                                                      float*       out_band_fft_data_ptr)
{
    float          fft_data[1024];
    float*         fft_data_traveller_ptr = NULL;
    bool           result                 = false;
    _audio_stream* stream_ptr             = (_audio_stream*) stream;

    ASSERT_DEBUG_SYNC( n_result_frequency_bands      <= 64 &&
                      (n_result_frequency_bands % 2) == 0,
                      "Invalid number of frequency bands was requested.");
    ASSERT_DEBUG_SYNC(stream_ptr != NULL,
                      "Input audio_stream instance is NULL");

    /* Retrieve FFT data */
    if ( (BASS_ChannelGetData(stream_ptr->stream,
                              fft_data,
                              BASS_DATA_FFT2048)) <= 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve FFT data");

        goto end;
    }

    /* Group frequency bands into averages */
    memset(out_band_fft_data_ptr,
           0,
           sizeof(float) * n_result_frequency_bands);

    fft_data_traveller_ptr = fft_data;

    for (uint32_t n_band = 0;
                  n_band < n_result_frequency_bands;
                ++n_band)
    {
        for (uint32_t n_fraction = 0;
                      n_fraction < 128 /* number of float vals return by BASS */ / n_result_frequency_bands;
                    ++n_fraction)
        {
            out_band_fft_data_ptr[n_band] += *(fft_data_traveller_ptr++);
        }

        out_band_fft_data_ptr[n_band] /= float(n_result_frequency_bands);
    }

    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void audio_stream_get_property(audio_stream          stream,
                                                  audio_stream_property property,
                                                  void*                 out_result)
{
    _audio_stream* stream_ptr = (_audio_stream*) stream;

    ASSERT_DEBUG_SYNC(stream_ptr != NULL,
                      "Input audio_stream instance is NULL");

    switch (property)
    {
        case AUDIO_STREAM_PROPERTY_AUDIO_DEVICE:
        {
            *(audio_device*) out_result = stream_ptr->device;

            break;
        }

        case AUDIO_STREAM_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result = stream_ptr->callback_manager;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized audio_stream_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_stream_pause(audio_stream stream)
{
    bool           result     = false;
    _audio_stream* stream_ptr = (_audio_stream*) stream;

    ASSERT_DEBUG_SYNC(stream != NULL,
                      "Input audio_stream instance is NULL");
    ASSERT_DEBUG_SYNC(stream_ptr->is_playing,
                      "Invalid audio_stream_play() call: Stream has not been playing at the time of the call");

    if (BASS_ChannelPause(stream_ptr->stream) == FALSE)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not pause audio_stream playback");
    }
    else
    {
        stream_ptr->is_playing = false;
        result                 = true;
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_stream_play(audio_stream stream,
                                          system_time  start_time)
{
    bool           result                = false;
    double         start_time_bass       = 0.0;
    uint32_t       start_time_msec       = 0;
    QWORD          start_time_stream_pos = 0;
    _audio_stream* stream_ptr            = (_audio_stream*) stream;

    ASSERT_DEBUG_SYNC(stream != NULL,
                      "Input audio_stream instance is NULL");
    ASSERT_DEBUG_SYNC(!stream_ptr->is_playing,
                      "Invalid audio_stream_play() call: Stream is already playing");

    system_time_get_msec_for_time(start_time,
                                 &start_time_msec);

    start_time_bass       = double(start_time_msec) / 1000.0;
    start_time_stream_pos = BASS_ChannelSeconds2Bytes(stream_ptr->stream,
                                                      start_time_bass);

    ASSERT_DEBUG_SYNC(start_time_stream_pos != -1,
                      "Could not determine stream position for the requested start time");

    if (start_time_stream_pos != -1)
    {
        result = true;

        if (!audio_device_bind_to_thread(stream_ptr->device) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not bind the audio device to the running thread");

            result = false;
            goto end;
        }

        if (BASS_ChannelSetPosition(stream_ptr->stream,
                                    start_time_stream_pos,
                                    BASS_POS_BYTE) == FALSE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not update stream position");

            result = false;
            goto end;
        }

        if (BASS_ChannelPlay(stream_ptr->stream,
                             FALSE /* restart */) == FALSE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not start stream playback");

            result = false;
            goto end;
        }

        if (BASS_ChannelUpdate(stream_ptr->stream,
                               0) /* length */ == FALSE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not update stream channel contents");

            result = false;
            goto end;
        }
    } /* if (start_time_stream_pos != -1) */

    if (result)
    {
        stream_ptr->is_playing = true;
    }

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_stream_rewind(audio_stream stream,
                                            system_time  new_time)
{
    double         new_time_bass       = 0.0;
    uint32_t       new_time_msec       = 0;
    QWORD          new_time_stream_pos = 0;
    bool           result              = false;
    _audio_stream* stream_ptr          = (_audio_stream*) stream;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(stream != NULL,
                      "Input audio_stream instance is NULL");
    ASSERT_DEBUG_SYNC(stream_ptr->is_playing,
                      "Invalid audio_stream_play() call: Stream is already playing");

    /* Convert the input time to BASS stream position */
    system_time_get_msec_for_time(new_time,
                                 &new_time_msec);

    new_time_bass       = double(new_time_msec) / 1000.0;
    new_time_stream_pos = BASS_ChannelSeconds2Bytes(stream_ptr->stream,
                                                    new_time_bass);

    ASSERT_DEBUG_SYNC(new_time_stream_pos != -1,
                      "Could not determine stream position for the requested playback time");

    result = audio_device_bind_to_thread(stream_ptr->device);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not bind the audio device to the running thread.");

        goto end;
    } /* if (!result) */

    /* Change the playback position */
    if (BASS_ChannelSetPosition(stream_ptr->stream,
                                new_time_stream_pos,
                                BASS_POS_BYTE) == FALSE)
    {
        /* We can hit this location if user tries to rewind "past" the available raw data buffer.
         * Log a fatal error but do not throw an assertion failure. */
        LOG_FATAL("Could not update audio stream playback time (invalid rewind request?)");

        goto end;
    }

    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_stream_resume(audio_stream stream)
{
    bool           result     = false;
    _audio_stream* stream_ptr = (_audio_stream*) stream;

    ASSERT_DEBUG_SYNC(stream_ptr != NULL,
                      "Input audio_stream instance is NULL");
    ASSERT_DEBUG_SYNC(!stream_ptr->is_playing,
                      "Input audio stream is not paused.");

    result = audio_device_bind_to_thread(stream_ptr->device);

    if (!result)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not bind the audio device to the running thread.");

        goto end;
    } /* if (!result) */

    if (BASS_ChannelPlay(stream_ptr->stream,
                         FALSE /* restart */) == FALSE)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not resume the audio stream playback");

        goto end;
    }

    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_stream_stop(audio_stream stream)
{
    bool           result     = false;
    _audio_stream* stream_ptr = (_audio_stream*) stream;

    ASSERT_DEBUG_SYNC(stream != NULL,
                      "Input audio_stream instance is NULL");
    ASSERT_DEBUG_SYNC(stream_ptr->is_playing,
                      "Invalid audio_stream_stop() call: Stream has not been playing at the time of the call");

    if (BASS_ChannelStop(stream_ptr->stream) == FALSE)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not stop audio_stream playback");
    }
    else
    {
        stream_ptr->is_playing = false;
        result                 = true;
    }

    return result;
}