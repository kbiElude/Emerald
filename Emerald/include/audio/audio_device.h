/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "system/system_types.h"


typedef enum
{
    /* not settable, int */
    AUDIO_DEVICE_PROPERTY_DEVICE_INDEX,

    /* not settable, bool */
    AUDIO_DEVICE_PROPERTY_IS_ACTIVATED,

    /* not settable, bool */
    AUDIO_DEVICE_PROPERTY_IS_DEFAULT,

    /* not settable, const char* */
    AUDIO_DEVICE_PROPERTY_NAME
} audio_device_property;


/** TODO.
 *
 *  NOTE: Internal use only.
 */
PUBLIC bool audio_device_activate(audio_device device);

/** TODO.
 *
 *  NOTE: Internal use only.
 */
PUBLIC void audio_device_deinit();

/** TODO */
PUBLIC EMERALD_API audio_device audio_device_get_default_device();

/** TODO */
PUBLIC EMERALD_API void audio_device_get_property(audio_device          device,
                                                  audio_device_property property,
                                                  void*                 out_result);

/** TODO.
 *
 *  NOTE: Internal use only.
 */
PUBLIC void audio_device_init();


#endif /* AUDIO_DEVICE_H */
