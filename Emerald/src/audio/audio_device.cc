/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "bass.h"
#include "audio/audio_device.h"
#include "system/system_atomics.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"
#include "system/system_window.h"

typedef struct _audio_device
{
    int         device_index; /* strictly private ! */
    bool        is_activated;
    bool        is_default;
    system_time latency;
    const char* name;

    explicit _audio_device(int         in_device_index,
                           bool        in_is_default,
                           const char* in_name)
    {
        ASSERT_DEBUG_SYNC(in_name != nullptr,
                          "Audio device name is nullptr");

        device_index = in_device_index;
        is_activated = false;
        is_default   = in_is_default;
        latency      = 0;
        name         = in_name;
    }
} _audio_device;


/** TODO */
PRIVATE system_resizable_vector audio_devices             = nullptr;
PRIVATE audio_device            default_audio_device      = nullptr;
PRIVATE unsigned int            n_audio_devices_activated = 0;


/** Please see header for spec */
PUBLIC bool audio_device_activate(audio_device  device,
                                  system_window owner_window)
{
    _audio_device* device_ptr = (_audio_device*) device;
    bool           result     = false;

    ASSERT_DEBUG_SYNC(device_ptr != nullptr,
                      "Input audio_device instance is nullptr");
    ASSERT_DEBUG_SYNC(owner_window != nullptr,
                      "Owner window instance cannot be nullptr");
    ASSERT_DEBUG_SYNC(!device_ptr->is_activated,
                      "Input audio_device instance is already activated.");

    if (!device_ptr->is_activated)
    {
        BASS_INFO            binding_info;
        system_window_handle window_handle = (system_window_handle) nullptr;

        /* Retrieve the window system handle. We need it for latency calculations */
        system_window_get_property(owner_window,
                                   SYSTEM_WINDOW_PROPERTY_HANDLE,
                                  &window_handle);

        /* Initialize the audio device.
         *
         * NOTE: This call binds the device to the calling thread.
         */
        result = (BASS_Init(device_ptr->device_index,
                            44100,               /* freq - ignored */
                            BASS_DEVICE_LATENCY, /* also calculate the latency */
#ifdef _WIN32
                            window_handle,
#else
                            nullptr,                /* win - ignored */
#endif
                            nullptr) == TRUE);      /* dsguid - ignored */

        ASSERT_DEBUG_SYNC(result,
                          "Failed to initialize an audio device");

        if (result)
        {
            /* Retrieve the latency information */
            result = (BASS_GetInfo(&binding_info) == TRUE);

            ASSERT_DEBUG_SYNC(result,
                              "Failed to retrieve device information");

            if (result)
            {
                device_ptr->is_activated = true;
                device_ptr->latency      = system_time_get_time_for_msec(binding_info.latency);

                if (system_atomics_increment(&n_audio_devices_activated) != 1)
                {
                    /* NOTE: BASS can handle more than 1 audio device at the time but, in
                     *       order to avoid the thread<->audio device binding management
                     *       hell, we introduce this little limitation.
                     */
                    ASSERT_ALWAYS_SYNC(false,
                                       "Only one audio device may be activated at the same time");
                }
            }
        }
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool audio_device_bind_to_thread(audio_device device)
{
    _audio_device* device_ptr = (_audio_device*) device;
    bool           result     = false;

    ASSERT_DEBUG_SYNC(device != nullptr,
                      "Input audio_device instance is nullptr.");

    if (BASS_SetDevice(device_ptr->device_index) == FALSE)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not bind the audio_device instance to the current thread.");
    }
    else
    {
        result = true;
    }

    return result;
}

/** Please see header for spec */
PUBLIC void audio_device_deinit()
{
    _audio_device* current_audio_device_ptr = nullptr;

    if (audio_devices != nullptr)
    {
        while (system_resizable_vector_pop(audio_devices,
                                          &current_audio_device_ptr) )
        {
            delete current_audio_device_ptr;

            current_audio_device_ptr = nullptr;
        }

        system_resizable_vector_release(audio_devices);
        audio_devices = nullptr;
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API audio_device audio_device_get_default_device()
{
    return default_audio_device;
}

/** Please see header for spec */
PUBLIC EMERALD_API void audio_device_get_property(audio_device          device,
                                                  audio_device_property property,
                                                  void*                 out_result_ptr)
{
    const _audio_device* device_ptr = reinterpret_cast<const _audio_device*>(device);

    switch (property)
    {
        case AUDIO_DEVICE_PROPERTY_IS_ACTIVATED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = device_ptr->is_activated;

            break;
        }

        case AUDIO_DEVICE_PROPERTY_IS_DEFAULT:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = device_ptr->is_default;

            break;
        }

        case AUDIO_DEVICE_PROPERTY_LATENCY:
        {
            *reinterpret_cast<system_time*>(out_result_ptr) = device_ptr->latency;

            break;
        }

        case AUDIO_DEVICE_PROPERTY_NAME:
        {
            *reinterpret_cast<const char**>(out_result_ptr) = device_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized audio_device_property value");
        }
    }
}

/** Please see header for spec */
PUBLIC void audio_device_init()
{
    BASS_DEVICEINFO current_device_info;

#ifdef _WIN32
    unsigned int n_current_device = 1; /* audible devices start from 1 under Windows BASS */
#else
    unsigned int n_current_device = 1; /* different situation under Linux */
#endif

    ASSERT_DEBUG_SYNC(audio_devices == nullptr,
                      "Audio devices vector is not nullptr");

    audio_devices = system_resizable_vector_create(16); /* capacity */

    ASSERT_DEBUG_SYNC(audio_devices != nullptr,
                      "Could not create the audio devices vector");

    /* Verify that the right BASS version was loaded before proceeding  */
    if (HIWORD(BASS_GetVersion() ) != BASSVERSION)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Incompatible BASS version was loaded");

        goto end;
    }

    /* Enumerate all available audio devices */
    while (true)
    {
        if (!BASS_GetDeviceInfo(n_current_device,
                               &current_device_info) )
        {
            /* Ran out of audio devices */
            break;
        }

        /* Extract information we intend to expose and form an _audio_device instance
         * using the data. */
        _audio_device* new_device_ptr = new (std::nothrow) _audio_device(n_current_device,
                                                                         (current_device_info.flags & BASS_DEVICE_DEFAULT) != 0, /* is_device_enabled */
                                                                         current_device_info.name);

        ASSERT_ALWAYS_SYNC(new_device_ptr != nullptr,
                           "Out of memory");

        if (new_device_ptr->is_default)
        {
            ASSERT_DEBUG_SYNC(default_audio_device == nullptr,
                              "More than one default audio devices were reported!");

            default_audio_device = (audio_device) new_device_ptr;
        }

        /* Stash it */
        system_resizable_vector_push(audio_devices,
                                     new_device_ptr);

        /* Move on */
        ++n_current_device;
    }

end:
    ;
}

