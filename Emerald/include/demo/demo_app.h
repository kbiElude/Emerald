/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef DEMO_APP_H
#define DEMO_APP_H

#include "demo/demo_window.h"
#include "demo/demo_types.h"

typedef enum
{
    /* Call-back fired whenever a windows is about to be destroyed.
     *
     * @param arg demo_window instance
     */
    DEMO_APP_CALLBACK_ID_WINDOW_ABOUT_TO_BE_DESTROYED,

    /* Call-back fired whenever a new window is created.
     *
     * @param arg New demo_window instance.
     */
    DEMO_APP_CALLBACK_ID_WINDOW_CREATED,

    /* Always last */
    DEMO_APP_CALLBACK_ID_COUNT
} demo_app_callback_id;

typedef enum
{
    /* not settable; system_callback_manager */
    DEMO_APP_PROPERTY_CALLBACK_MANAGER,

    /* not settable; ral_scheduler */
    DEMO_APP_PROPERTY_GPU_SCHEDULER,

    /* not settable; demo_materials */
    DEMO_APP_PROPERTY_MATERIALS,

    /* not settable; varia_primitive_renderer */
    DEMO_APP_PROPERTY_PRIMITIVE_RENDERER,

    /* not settable; uint32_t */
    DEMO_APP_PROPERTY_N_WINDOWS
} demo_app_property;


/** TODO */
PUBLIC EMERALD_API void demo_app_close();

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_create_window(system_hashed_ansi_string      window_name,
                                                      const demo_window_create_info& create_info,
                                                      ral_backend_type               backend_type,
                                                      bool                           use_timeline = true);

/** TODO */
PUBLIC EMERALD_API bool demo_app_destroy_window(system_hashed_ansi_string window_name);

/** TODO */
PUBLIC EMERALD_API void demo_app_get_property(demo_app_property property,
                                              void*             out_result_ptr);

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_name(system_hashed_ansi_string window_name);

/** TODO */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_index(uint32_t n_window);


#endif /* DEMO_APP_H */
