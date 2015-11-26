/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_APP_H
#define DEMO_APP_H

#include "demo/demo_types.h"


typedef enum
{
    /* not settable; uint32_t */
    DEMO_APP_PROPERTY_N_WINDOWS
} demo_app_property;


/** TODO */
PUBLIC EMERALD_API demo_window demo_app_create_window(system_hashed_ansi_string window_name,
                                                      ral_backend_type          backend_type);

/** TODO */
PUBLIC EMERALD_API bool demo_app_destroy_window(system_hashed_ansi_string window_name);

/** TODO */
PUBLIC EMERALD_API void demo_app_get_property(demo_app_property property,
                                              void*             out_result_ptr);

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_name(system_hashed_ansi_string window_name);

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_index(uint32_t n_window);

/** TODO */
PUBLIC EMERALD_API void demo_app_close();


#endif /* DEMO_APP_H */
