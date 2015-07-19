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
    #include <X11/extensions/Xrandr.h>
    #include <unistd.h>
#endif


PRIVATE unsigned int n_cpu_cores  = 0;

/* holds system_screen_mode instances. The system blob property is set to:
 *
 * SizeID value     - under Linux.
 * DEVMODE instance - under Windows.
 **/
PRIVATE system_resizable_vector screen_modes = NULL;


/* Forward declarations */
PRIVATE void _system_capabilities_init_full_screen_modes();

#ifdef _WIN32
    PRIVATE void _system_capabilities_release_devmode(void* blob);
#endif


/** TODO */
PRIVATE void _system_capabilities_init_full_screen_modes()
{
    ASSERT_DEBUG_SYNC(screen_modes == NULL,
                      "Full-screen modes vector already initialized");

    screen_modes = system_resizable_vector_create(64); /* capacity */

    #ifdef _WIN32
    {
        /* For simplicity, always assume the user will only be interested in the default device */
        DEVMODE current_device_mode = {0};
        DWORD   n_mode              = 0;

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
        /* Open the display connection.
         *
         * TODO: Enumerate all available displays!
         */
        Display*                display                  = XOpenDisplay(":0");
        int                     n_screen_sizes           = 0;
        Window                  root_window              = (Window) NULL;
        int                     screen                   = 0;
        XRRScreenConfiguration* screen_configuration_ptr = NULL;
        XRRScreenSize*          screen_sizes_ptr         = NULL;

        if (display == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                              "Could not open display connection");

            goto end;
        }

        screen                   = DefaultScreen(display);
        root_window              = RootWindow      (display,
                                                    screen);
        screen_configuration_ptr = XRRGetScreenInfo(display,
                                                    root_window);

        if (screen_configuration_ptr == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve screen information");

            goto end;
        }

        screen_sizes_ptr = XRRConfigSizes(screen_configuration_ptr,
                                         &n_screen_sizes);

        if (screen_sizes_ptr == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve screen sizes");

            goto end;
        }

        /* Iterate over all screen sizes and create system_screen_mode instance for each entry */
        for (unsigned int n_screen_size = 0;
                          n_screen_size < n_screen_sizes;
                        ++n_screen_size)
        {
            /* Query what refresh rates are supported for this resolution */
            int    n_refresh_rates = 0;
            short* refresh_rates   = NULL;

            refresh_rates = XRRConfigRates(screen_configuration_ptr,
                                           n_screen_size,
                                          &n_refresh_rates);

            if (refresh_rates == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not retrieve refresh rate array for screen size ID [%d]",
                                   n_screen_size);

                continue;
            }

            for (int n_refresh_rate = 0;
                     n_refresh_rate < n_refresh_rates;
                   ++n_refresh_rate)
            {
                int current_refresh_rate = (int) refresh_rates[n_refresh_rate];

                system_screen_mode new_screen_mode = system_screen_mode_create(screen_sizes_ptr[n_screen_size].width,
                                                                               screen_sizes_ptr[n_screen_size].height,
                                                                               current_refresh_rate,
                                                                               (void*) n_screen_size,
                                                                               NULL); /* pfn_release_system_blob */

                system_resizable_vector_push(screen_modes,
                                             new_screen_mode);
            } /* for (all refresh rates) */
        } /* for (all screen sizes) */

end:
        /* Clean up */
        if (screen_configuration_ptr != NULL)
        {
            XRRFreeScreenConfigInfo(screen_configuration_ptr);

            screen_configuration_ptr = NULL;
        }

        if (display != NULL)
        {
            XCloseDisplay(display);

            display = NULL;
        }
    }
    #endif

}

#ifdef _WIN32

/** TODO */
PRIVATE void _system_capabilities_release_devmode(void* blob)
{
    ASSERT_DEBUG_SYNC(blob != NULL,
                      "Input blob argument is NULL");

    delete (DEVMODE*) blob;
}
#endif

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
PUBLIC EMERALD_API bool system_capabilities_get_screen_mode_for_resolution(unsigned int        width,
                                                                           unsigned int        height,
                                                                           unsigned int        frequency,
                                                                           system_screen_mode* out_screen_mode)
{
    system_screen_mode current_screen_mode  = NULL;
    unsigned int       n_screen_modes      = 0;
    bool               result              = false;

    ASSERT_DEBUG_SYNC(screen_modes != NULL,
                      "Screen modes vector is NULL");

    system_resizable_vector_get_property(screen_modes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_screen_modes);

    ASSERT_DEBUG_SYNC(n_screen_modes > 0,
                      "No screen modes are registered");

    for (unsigned int n_screen_mode = 0;
                      n_screen_mode < n_screen_modes;
                    ++n_screen_mode)
    {
        unsigned int       current_screen_frequency = 0;
        unsigned int       current_screen_height    = 0;
        unsigned int       current_screen_width     = 0;

        if (!system_resizable_vector_get_element_at(screen_modes,
                                                    n_screen_mode,
                                                    &current_screen_mode) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve system_screen_mode_instance at index [%d]",
                              n_screen_mode);

            break;
        }

        system_screen_mode_get_property(current_screen_mode,
                                        SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                       &current_screen_height);
        system_screen_mode_get_property(current_screen_mode,
                                        SYSTEM_SCREEN_MODE_PROPERTY_REFRESH_RATE,
                                       &current_screen_frequency);
        system_screen_mode_get_property(current_screen_mode,
                                        SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                       &current_screen_width);

        if (current_screen_frequency == frequency &&
            current_screen_height    == height    &&
            current_screen_width     == width)
        {
            result = true;

            break;
        }
    } /* for (all screen modes) */

    if (result)
    {
        *out_screen_mode = current_screen_mode;
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
