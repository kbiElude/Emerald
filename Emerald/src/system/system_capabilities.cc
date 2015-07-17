/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_capabilities.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"

#ifdef __linux
    #include <unistd.h>
#endif


PRIVATE unsigned int            n_cpu_cores  = 0;
PRIVATE system_resizable_vector screen_modes = NULL; /* holds system_screen_mode instances */

/* Forward declarations */
PRIVATE void _system_capabilities_init_full_screen_modes();
PRIVATE void _system_capabilities_release_devmode       (void* blob);


/** TODO */
PRIVATE void _system_capabilities_init_full_screen_modes()
{
    ASSERT_DEBUG_SYNC(screen_modes == NULL,
                      "Full-screen modes vector already initialized");

    screen_modes = system_resizable_vector_create(64); /* capacity */

    #ifdef _WIN32
    {
        /* For simplicity, always assume the user will only be interested in the default device */
        DEVMODE current_device_mode;
        DWORD   n_mode = 0;

        while (true)
        {
            DEVMODE*           device_mode_ptr = NULL;
            system_screen_mode new_screen_mode = NULL;

            if (::EnumDisplaySettings(NULL, /* lpszDeviceName */
                                      n_mode,
                                     &current_device_mode) == 0)
            {
                /* No more device modes available */
                break;
            }

            /* Make a copy of the devmode instance. */
            device_mode_ptr = new DEVMODE();

            ASSERT_ALWAYS_SYNC(device_mode_ptr != NULL,
                               "Out of memory");

            memcpy(device_mode_ptr,
                  &current_device_mode,
                   sizeof(current_device_mode) );

            /* Create a system_screen_mode instance wrapping the system blob */
            new_screen_mode = system_screen_mode_create(current_device_mode.dmPelsWidth,
                                                        current_device_mode.dmPelsHeight,
                                                        current_device_mode.dmDisplayFrequency,
                                                        device_mode_ptr,
                                                        _system_capabilities_release_devmode);

            /* Stash it */
            system_resizable_vector_push(screen_modes,
                                         (void*) new_screen_mode);

            n_mode++;
        } /* while (true) */
    }
    #else
    {
        todo;
    }
    #endif
}

/** TODO */
PRIVATE void _system_capabilities_release_devmode(void* blob)
{
    ASSERT_DEBUG_SYNC(blob != NULL,
                      "Input blob argument is NULL");

    delete (DEVMODE*) blob;
}

/** Please see header for spec */
PUBLIC void system_capabilities_deinit()
{
    if (screen_modes != NULL)
    {
        system_screen_mode current_screen_mode = NULL;

        while (system_resizable_vector_pop(screen_modes,
                                          &current_screen_mode) )
        {
            system_screen_mode_release(current_screen_mode);
            current_screen_mode = NULL;
        }

        system_resizable_vector_release(screen_modes);
        screen_modes = NULL;
    } /* if (screen_modes != NULL) */
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_capabilities_get(system_capabilities_property property,
                                                void*                        out_result)
{
    switch (property)
    {
        case SYSTEM_CAPABILITIES_PROPERTY_NUMBER_OF_CPU_CORES:
        {
            ASSERT_DEBUG_SYNC(n_cpu_cores != 0,
                              "0 CPU cores are recognized!");

            *(unsigned int*) out_result = n_cpu_cores;

            break;
        }

        case SYSTEM_CAPABILITIES_PROPERTY_NUMBER_OF_SCREEN_MODES:
        {
            system_resizable_vector_get_property(screen_modes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

            ASSERT_DEBUG_SYNC(* (GLuint*) out_result != 0,
                              "Zero screen modes reported");

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized system_capabilities_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API bool system_capabilities_get_screen_mode(unsigned int        n_screen_mode,
                                                            system_screen_mode* out_screen_mode)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(screen_modes != NULL,
                      "Screen modes vector is NULL");

    if (!system_resizable_vector_get_element_at(screen_modes,
                                                n_screen_mode,
                                                out_screen_mode))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid screen modex index requested [%d]",
                          n_screen_mode);

        result = false;
    }

    return result;
}

/** Please see header for spec */
PUBLIC void system_capabilities_init()
{
    /* Number of CPU cores */
#ifdef _WIN32
    SYSTEM_INFO system_info;

    ::GetSystemInfo(&system_info);

    n_cpu_cores = system_info.dwNumberOfProcessors;
#else
    n_cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

    /* Full-screen modes */
    _system_capabilities_init_full_screen_modes();
}
