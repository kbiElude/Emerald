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
PUBLIC EMERALD_API void demo_app_close();

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_create_window(system_hashed_ansi_string window_name,
                                                      ral_backend_type          backend_type,
                                                      bool                      use_timeline = true);

/** TODO */
PUBLIC EMERALD_API bool demo_app_destroy_window(system_hashed_ansi_string window_name);

/** TODO
 *
 *  @param out_supported_samples Array allocated by Emerald. Release with demo_app_free_msaa_samples()
 *                               when no longer needed.
 **/
PUBLIC EMERALD_API void demo_app_enumerate_msaa_samples(ral_backend_type backend_type,
                                                        uint32_t*        out_n_supported_samples,
                                                        uint32_t**       out_supported_samples);

/** TODO */
PUBLIC EMERALD_API void demo_app_free_msaa_samples(uint32_t* supported_samples_array);

/** TODO */
PUBLIC EMERALD_API void demo_app_get_property(demo_app_property property,
                                              void*             out_result_ptr);

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_name(system_hashed_ansi_string window_name);

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_index(uint32_t n_window);


#endif /* DEMO_APP_H */
