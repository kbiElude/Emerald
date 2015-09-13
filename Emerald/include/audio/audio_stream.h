/**
 *
 * Emerald (kbi/elude @2015)
 *
 * The object creates a BASS stream which is responsible for decoding the audio stream.
 * It then can be assigned to a system_window instance. When paired with an audio_device
 * instance at the system_window level, the soundtrack will automatically start playing
 * when the rendering process is launched via ogl_rendering_handler.
 *
 * It is recommended to use a memory region-backed system_file_serializer instance for
 * quick instantiation times. If the serializer encapsulates a physical file, its contents
 * have to be fully cached by the serializer, before audio_stream is instantiated.
 */
#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(audio_stream,
                             audio_stream)


typedef enum
{
    /* The audio stream finished playing.
     *
     * callback_proc_data: originating audio_stream instance.
     */
    AUDIO_STREAM_CALLBACK_ID_FINISHED_PLAYING, 

    /* Always last */
    AUDIO_STREAM_CALLBACK_ID_COUNT
} audio_stream_callback_id;


typedef enum
{
    /* not settable, audio_device. */
    AUDIO_STREAM_PROPERTY_AUDIO_DEVICE,

    /* not settable, system_callback_manager */
    AUDIO_STREAM_PROPERTY_CALLBACK_MANAGER
} audio_stream_property;


/** TODO */
PUBLIC EMERALD_API audio_stream audio_stream_create(audio_device           device,
                                                    system_file_serializer serializer,
                                                    system_window          window);

/** TODO */
PUBLIC EMERALD_API bool audio_stream_get_fft_averages(audio_stream stream,
                                                      uint32_t     n_result_frequency_bands,
                                                      float*       out_band_fft_data_ptr);


/** TODO */
PUBLIC EMERALD_API void audio_stream_get_property(audio_stream          stream,
                                                  audio_stream_property property,
                                                  void*                 out_result);

/** TODO */
PUBLIC EMERALD_API bool audio_stream_pause(audio_stream stream);

/** TODO */
PUBLIC EMERALD_API bool audio_stream_resume(audio_stream stream);

/** TODO */
PUBLIC EMERALD_API bool audio_stream_rewind(audio_stream stream,
                                            system_time  new_time);

/** TODO */
PUBLIC EMERALD_API bool audio_stream_play(audio_stream stream,
                                          system_time  start_time);

/** TODO */
PUBLIC EMERALD_API bool audio_stream_stop(audio_stream stream);

#endif /* AUDIO_STREAM_H */
