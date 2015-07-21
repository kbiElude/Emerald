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


/** TODO */
PUBLIC EMERALD_API audio_stream audio_stream_create(system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void audio_stream_release(audio_stream stream);


#endif /* AUDIO_STREAM_H */
