/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "system/system_types.h"


typedef enum
{
    /* not settable, bool */
    AUDIO_DEVICE_PROPERTY_IS_ACTIVATED,

    /* not settable, bool */
    AUDIO_DEVICE_PROPERTY_IS_DEFAULT,

    /* not settable, system_time.
     *
     * Meaningful value is associated at audio_device_activate() call time.
     */
    AUDIO_DEVICE_PROPERTY_LATENCY,

    /* not settable, const char* */
    AUDIO_DEVICE_PROPERTY_NAME
} audio_device_property;


/** TODO.
 *
 *  TODO: Once activated and associated with the specified window, the device cannot
 *        be unbound. This restriction may be lifted in the future.
 *
 *  NOTE: Internal use only.
 */
PUBLIC bool audio_device_activate(audio_device  device,
                                  system_window owner_window);

/** TODO */
PUBLIC EMERALD_API bool audio_device_bind_to_thread(audio_device device);

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
                                                  void*                 out_result_ptr);

/** TODO.
 *
 *  NOTE: Internal use only.
 */
PUBLIC void audio_device_init();


#endif /* AUDIO_DEVICE_H */
