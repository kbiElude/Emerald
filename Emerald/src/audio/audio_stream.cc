/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "bass.h"
#include "audio/audio_stream.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"


typedef struct _audio_stream
{
    system_file_serializer serializer;
    HSTREAM                stream;

    explicit _audio_stream(system_file_serializer in_serializer)
    {
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


/** Please see header for spec */
PUBLIC EMERALD_API audio_stream audio_stream_create(system_file_serializer serializer)
{
    _audio_stream* new_audio_stream_ptr        = NULL;
    const char*    raw_serializer_storage      = NULL;
    size_t         raw_serializer_storage_size = 0;

    ASSERT_DEBUG_SYNC(serializer != NULL,
                      "Input serializer is NULL");

    /* Spawn the new descriptor */
    new_audio_stream_ptr = new (std::nothrow) _audio_stream(serializer);

    ASSERT_ALWAYS_SYNC(new_audio_stream_ptr != NULL,
                       "Out of memory");

    /* The serializer is retained at this point, so carry on & open the stream handle */
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
PUBLIC EMERALD_API void audio_stream_release(audio_stream stream)
{
    ASSERT_DEBUG_SYNC(stream != NULL,
                      "Input audio_stream instance is NULL");

    delete (_audio_stream*) stream;
    stream = NULL;
}
