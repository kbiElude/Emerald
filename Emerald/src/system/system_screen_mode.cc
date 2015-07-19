/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"

#ifdef __linux
    #include <X11/extensions/Xrandr.h>
    #include <unistd.h>
#endif


typedef struct _system_screen_mode
{
    unsigned int             height;
    PFNRELEASESYSTEMBLOBPROC pfn_release_system_blob;
    unsigned int             refresh_rate;
    void*                    system_blob;
    unsigned int             width;

    explicit _system_screen_mode(const unsigned int&      in_height,
                                 const unsigned int&      in_refresh_rate,
                                 const unsigned int&      in_width,
                                 void*                    in_system_blob,
                                 PFNRELEASESYSTEMBLOBPROC in_pfn_release_system_blob)
    {
        height                  = in_height;
        pfn_release_system_blob = in_pfn_release_system_blob;
        refresh_rate            = in_refresh_rate;
        system_blob             = in_system_blob;
        width                   = in_width;
    }

    ~_system_screen_mode()
    {
        if (pfn_release_system_blob != NULL)
        {
            pfn_release_system_blob(system_blob);

            system_blob = NULL;
        }
    }
} _system_screen_mode;


#ifdef __linux
    /** TODO */
    PRIVATE Display* display = NULL;
    /* TODO */
    PRIVATE XRRScreenConfiguration* screen_configuration_ptr = NULL;
#endif

/* holds system_screen_mode instances. The system blob property is set to:
 *
 * SizeID  value    - under Linux.
 * DEVMODE instance - under Windows.
 **/
PRIVATE system_resizable_vector screen_modes = NULL;


/* Forward declarations */
PRIVATE void _system_screen_mode_init_full_screen_modes();

#ifdef _WIN32
    PRIVATE void _system_screen_mode_release_devmode(void* blob);
#endif

PRIVATE bool _system_screen_mode_sort_descending(void* in_screen_mode_1,
                                                 void* in_screen_mode_2);


/** TODO */
PRIVATE void _system_screen_mode_init_full_screen_modes()
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
                                                        _system_screen_mode_release_devmode);

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
        int            n_screen_sizes   = 0;
        Window         root_window      = (Window) NULL;
        int            screen           = 0;
        XRRScreenSize* screen_sizes_ptr = NULL;

        display = XOpenDisplay(":0");

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
    ;
    }
    #endif

    /* Sort the screen modes. The largest screen resolution with the largest refresh rate should
     * score higher than the same resolution with a smaller refresh rate. */
    system_resizable_vector_sort(screen_modes,
                                 _system_screen_mode_sort_descending);
}

#ifdef _WIN32

/** TODO */
PRIVATE void _system_screen_mode_release_devmode(void* blob)
{
    ASSERT_DEBUG_SYNC(blob != NULL,
                      "Input blob argument is NULL");

    delete (DEVMODE*) blob;
}
#endif

/** TODO */
PRIVATE bool _system_screen_mode_sort_descending(void* in_screen_mode_1,
                                                 void* in_screen_mode_2)
{
    const _system_screen_mode* screen_mode_1_ptr   = (const _system_screen_mode*) in_screen_mode_1;
    uint64_t                   screen_mode_1_score = uint64_t(screen_mode_1_ptr->height)       *
                                                     uint64_t(screen_mode_1_ptr->refresh_rate) *
                                                     uint64_t(screen_mode_1_ptr->width);
    const _system_screen_mode* screen_mode_2_ptr   = (const _system_screen_mode*) in_screen_mode_2;
    uint64_t                   screen_mode_2_score = uint64_t(screen_mode_2_ptr->height)       *
                                                     uint64_t(screen_mode_2_ptr->refresh_rate) *
                                                     uint64_t(screen_mode_2_ptr->width);

    return (screen_mode_1_score >= screen_mode_2_score);
}


/** Please see header for spec */
PUBLIC bool system_screen_mode_activate(system_screen_mode screen_mode)
{
    bool result = true;

    #ifdef _WIN32
    {
        _system_screen_mode* screen_mode_ptr = (_system_screen_mode*) screen_mode;

        if (::ChangeDisplaySettingsA( (DEVMODE*) screen_mode_ptr->system_blob,
                                     CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not switch to the requested screen mode.");

            result = false;
        }
    }
    #else
    {
        _system_screen_mode* screen_mode_ptr = (_system_screen_mode*) screen_mode;

        if (XRRSetScreenConfig(display,
                               screen_configuration_ptr,
                               DefaultRootWindow(display),
                               (SizeID) (intptr_t) screen_mode_ptr->system_blob,
                               0, /* rotation */
                               CurrentTime) != RRSetConfigSuccess)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not switch to the requested screen mode.");

            result = false;
        }
    }
    #endif

    return result;
}

/** Please see header for spec */
PUBLIC system_screen_mode system_screen_mode_create(unsigned int             width,
                                                    unsigned int             height,
                                                    unsigned int             refresh_rate,
                                                    void*                    system_blob,
                                                    PFNRELEASESYSTEMBLOBPROC pfn_release_system_blob)
{
    _system_screen_mode* new_screen_mode_ptr = new (std::nothrow) _system_screen_mode(height,
                                                                                      refresh_rate,
                                                                                      width,
                                                                                      system_blob,
                                                                                      pfn_release_system_blob);

    ASSERT_ALWAYS_SYNC(new_screen_mode_ptr != NULL,
                       "Out of memory");

    return (system_screen_mode) new_screen_mode_ptr;
}

/** Please see header for spec */
PUBLIC void system_screen_mode_deinit()
{
#ifdef __linux
    if (screen_configuration_ptr != NULL)
    {
        XRRFreeScreenConfigInfo(screen_configuration_ptr);

        screen_configuration_ptr = NULL;
    }

    /* Clean up */
    if (display != NULL)
    {
        XCloseDisplay(display);

        display = NULL;
    }
#endif

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
PUBLIC EMERALD_API bool system_screen_mode_get(unsigned int        n_screen_mode,
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
PUBLIC EMERALD_API void system_screen_mode_get_property(system_screen_mode          screen_mode,
                                                        system_screen_mode_property property,
                                                        void*                       out_result)
{
    const _system_screen_mode* screen_mode_ptr = (const _system_screen_mode*) screen_mode;

    switch (property)
    {
        case SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT:
        {
            *(GLuint*) out_result = screen_mode_ptr->height;

            break;
        }

        case SYSTEM_SCREEN_MODE_PROPERTY_REFRESH_RATE:
        {
            *(GLuint*) out_result = screen_mode_ptr->refresh_rate;

            break;
        }

        case SYSTEM_SCREEN_MODE_PROPERTY_SYSTEM_BLOB:
        {
            *(void**) out_result = screen_mode_ptr->system_blob;

            break;
        }

        case SYSTEM_SCREEN_MODE_PROPERTY_WIDTH:
        {
            *(GLuint*) out_result = screen_mode_ptr->width;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_screen_mode_property value");

            break;
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API bool system_screen_mode_get_for_resolution(unsigned int        width,
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
PUBLIC void system_screen_mode_init()
{
    _system_screen_mode_init_full_screen_modes();
}

/** Please see header for spec */
PUBLIC void system_screen_mode_release(system_screen_mode screen_mode)
{
    _system_screen_mode* screen_mode_ptr = (_system_screen_mode*) screen_mode;

    ASSERT_DEBUG_SYNC(screen_mode != NULL,
                      "Input system_screen_mode object is NULL");

    delete screen_mode_ptr;
    screen_mode_ptr = NULL;
}