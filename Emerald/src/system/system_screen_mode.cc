/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_screen_mode.h"

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
PUBLIC void system_screen_mode_get_property(system_screen_mode          screen_mode,
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
PUBLIC void system_screen_mode_release(system_screen_mode screen_mode)
{
    _system_screen_mode* screen_mode_ptr = (_system_screen_mode*) screen_mode;

    ASSERT_DEBUG_SYNC(screen_mode != NULL,
                      "Input system_screen_mode object is NULL");

    delete screen_mode_ptr;
    screen_mode_ptr = NULL;
}