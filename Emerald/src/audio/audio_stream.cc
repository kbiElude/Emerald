/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "bass.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_time.h"

typedef struct _audio_stream
{
    audio_device           device;
    bool                   is_playing;
    system_file_serializer serializer;
    HSTREAM                stream;

    REFCOUNT_INSERT_VARIABLES


    explicit _audio_stream(audio_device           in_device,
                           system_file_serializer in_serializer)
    {
        device     = in_device;
        is_playing = false;
        serializer = in_serializer;
        stream     = NULL;

        system_file_serializer_retain(in_serializer);
    }

    ~_audio_stream()
    {
        if (serializer != NULL)
        {
            system_file_serializer_release(serializer);

            serializer = NULL;
        } /* if (serializer != NULL) */

        if (stream != NULL)
        {
            BASS_StreamFree(stream);

            stream = NULL;
        }
    }
} _audio_stream;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(audio_stream,
                               audio_stream,
                              _audio_stream);


/** TODO */
PRIVATE void _audio_stream_release(void* stream)
{
    ASSERT_DEBUG_SYNC(stream != NULL,
                      "Input audio_stream instance is NULL");

    delete (_audio_stream*) stream;
    stream = NULL;
}


/** Please see header for spec */
PUBLIC EMERALD_API audio_stream audio_stream_create(audio_device           device,
                                                    system_file_serializer serializer)
{
    _audio_stream*            new_audio_stream_ptr        = NULL;
    const char*               raw_serializer_storage      = NULL;
    size_t                    raw_serializer_storage_size = 0;
    system_hashed_ansi_string serializer_name             = NULL;
    char                      temp_buffer[256];

    ASSERT_DEBUG_SYNC(device != NULL,
                      "Input audio device instance is NULL");
    ASSERT_DEBUG_SYNC(serializer != NULL,
                      "Input serializer is NULL");

    /* Spawn the new descriptor */
    new_audio_stream_ptr = new (std::nothrow) _audio_stream(device,
                                                            serializer);

    ASSERT_ALWAYS_SYNC(new_audio_stream_ptr != NULL,
                       "Out of memory");

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

    ASSERT_ALWAYS_SYNC(new_audio_stream_ptr->stream != NULL,
                       "Could not create a BASS stream!");

    if (new_audio_stream_ptr->stream == NULL)
    {
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
                                          system_time  start_time,
                                          bool         should_resume)
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

        audio_device_bind_to_thread(stream_ptr->device);

        if (BASS_ChannelSetPosition(stream_ptr->stream,
                                    start_time_stream_pos,
                                    BASS_POS_BYTE) == FALSE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not update stream position");

            result = false;
        }

        if (BASS_ChannelPlay(stream_ptr->stream,
                             should_resume ? TRUE : FALSE) == FALSE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not start stream playback");

            result = false;
        }
    } /* if (start_time_stream_pos != -1) */

    if (result)
    {
        stream_ptr->is_playing = true;
    }

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