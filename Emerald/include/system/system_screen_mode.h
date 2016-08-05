/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef SYSTEM_SCREEN_MODE_H
#define SYSTEM_SCREEN_MODE_H

#include "system_types.h"


typedef enum
{
    /* not settable, unsigned int */
    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,

    /* not settable, unsigned int */
    SYSTEM_SCREEN_MODE_PROPERTY_REFRESH_RATE,

    /* not settable, void* */
    SYSTEM_SCREEN_MODE_PROPERTY_SYSTEM_BLOB,

    /* not settable, unsigned int */
    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH
} system_screen_mode_property;


typedef void (*PFNRELEASESYSTEMBLOBPROC)(void* blob);

/** TODO */
PUBLIC bool system_screen_mode_activate(system_screen_mode screen_mode);

/** TODO
 *
 *  NOTE: Internal use only.
 *
 * @param width                   TODO.
 * @param height                  TODO.
 * @param refresh_rate            TODO.
 * @param system_blob             TODO.
 * @param pfn_release_system_blob TODO. May be NULL (in which case no call-back will be made
 *                                at destruction time).
 *
 * @return TODO
 */
PUBLIC system_screen_mode system_screen_mode_create(unsigned int             width,
                                                    unsigned int             height,
                                                    unsigned int             refresh_rate,
                                                    void*                    system_blob,
                                                    PFNRELEASESYSTEMBLOBPROC pfn_release_system_blob);

/** TODO */
PUBLIC void system_screen_mode_deinit();

/** TODO */
PUBLIC EMERALD_API bool system_screen_mode_get(unsigned int        n_screen_mode,
                                               system_screen_mode* out_screen_mode_ptr);

/** TODO */
PUBLIC EMERALD_API bool system_screen_mode_get_for_resolution(unsigned int        width,
                                                              unsigned int        height,
                                                              unsigned int        frequency,
                                                              system_screen_mode* out_screen_mode_ptr);

/** TODO */
PUBLIC EMERALD_API void system_screen_mode_get_property(system_screen_mode          screen_mode,
                                                        system_screen_mode_property property,
                                                        void*                       out_result_ptr);

/** TODO */
PUBLIC void system_screen_mode_init();

/** TODO */
PUBLIC void system_screen_mode_release(system_screen_mode screen_mode);

#endif /* SYSTEM_SCREEN_MODE_H */
