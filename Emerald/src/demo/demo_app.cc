/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_materials.h"
#include "demo/demo_window.h"
#include "main.h"
#include "ral/ral_scheduler.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "varia/varia_primitive_renderer.h"


typedef struct _demo_app
{
    system_callback_manager  callback_manager;
    system_critical_section  cs;
    demo_materials           materials;
    ral_scheduler            scheduler;
    system_hash64map         window_name_to_window_map;
    varia_primitive_renderer primitive_renderer;

    _demo_app()
    {
        callback_manager          = system_callback_manager_create((_callback_id) DEMO_APP_CALLBACK_ID_COUNT);
        cs                        = system_critical_section_create();
        materials                 = demo_materials_create         ();
        scheduler                 = ral_scheduler_create          ();
        window_name_to_window_map = system_hash64map_create       (sizeof(demo_window) );
    }

    void close()
    {
        system_critical_section_enter(cs);
        {
            if (callback_manager != nullptr)
            {
                system_callback_manager_release(callback_manager);

                callback_manager = nullptr;
            }

            if (materials != nullptr)
            {
                demo_materials_release(materials);

                materials = nullptr;
            }

            if (scheduler != nullptr)
            {
                ral_scheduler_release(scheduler);

                scheduler = nullptr;
            }

            if (window_name_to_window_map != nullptr)
            {
                demo_window   dangling_window           = nullptr;
                system_hash64 dangling_window_name_hash = 0;

                while (system_hash64map_get_element_at(window_name_to_window_map,
                                                       0, /* n_element */
                                                      &dangling_window,
                                                      &dangling_window_name_hash) )
                {
                    demo_window_release(dangling_window);

                    if (!system_hash64map_remove(window_name_to_window_map,
                                                 dangling_window_name_hash) )
                    {
                        /* Sigh. */
                        break;
                    }
                }

                system_hash64map_release(window_name_to_window_map);
                window_name_to_window_map = nullptr;
            }
        }
        system_critical_section_leave(cs);

        system_critical_section_release(cs);
        cs = nullptr;
    }
} _demo_app;

PRIVATE _demo_app app;


/** Please see header for specification */
PUBLIC EMERALD_API void demo_app_close()
{
    app.close();

    /* Force Emerald's internal resources deinitialization. */
    main_force_deinit();
}

/** Please see header for specification */
PUBLIC EMERALD_API demo_window demo_app_create_window(system_hashed_ansi_string      window_name,
                                                      const demo_window_create_info& create_info,
                                                      ral_backend_type               backend_type,
                                                      bool                           use_timeline)
{
    demo_window   result           = nullptr;
    system_hash64 window_name_hash = 0;

    /* Sanity checks */
    if (window_name                                       == nullptr ||
        system_hashed_ansi_string_get_length(window_name) == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid window name specified.");

        goto end;
    }

    /* Spawn a new window instance */
    result = demo_window_create(window_name,
                                create_info,
                                backend_type,
                                use_timeline);

    if (result == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "demo_window_create() failed.");

        goto end;
    }

    /* Store it */
    system_critical_section_enter(app.cs);
    {
        window_name_hash = system_hashed_ansi_string_get_hash(window_name);

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(app.window_name_to_window_map,
                                                     window_name_hash),
                          "The specified window name is already reserved");

        system_hash64map_insert(app.window_name_to_window_map,
                                window_name_hash,
                                result,
                                nullptr,  /* on_removal_callback */
                                nullptr); /* on_removal_callback_user_arg */
    }
    system_critical_section_leave(app.cs);

    system_callback_manager_call_back(app.callback_manager,
                                      DEMO_APP_CALLBACK_ID_WINDOW_CREATED,
                                      result);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_app_destroy_window(system_hashed_ansi_string window_name)
{
    bool          result           = false;
    demo_window   window           = nullptr;
    system_hash64 window_name_hash = 0;

    /* Sanity checks */
    if (window_name                                       == nullptr ||
        system_hashed_ansi_string_get_length(window_name) == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid window name specified.");

        goto end;
    }

    /* Retrieve the window's instance */
    window_name_hash = system_hashed_ansi_string_get_hash(window_name);

    system_critical_section_enter(app.cs);
    {
        if (!system_hash64map_get(app.window_name_to_window_map,
                                  window_name_hash,
                                 &window) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No demo_window instance associated with name [%s]",
                              system_hashed_ansi_string_get_buffer(window_name) );

            goto end;
        }

        system_hash64map_remove(app.window_name_to_window_map,
                                window_name_hash);
    }
    system_critical_section_leave(app.cs);

    demo_window_stop_rendering(window);

    /* Notify subscribers about the event */
    system_callback_manager_call_back(app.callback_manager,
                                      DEMO_APP_CALLBACK_ID_WINDOW_ABOUT_TO_BE_DESTROYED,
                                      window);

    /* Deallocate the retrieved window */
    demo_window_release(window);

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_app_get_property(demo_app_property property,
                                              void*             out_result_ptr)
{
    switch (property)
    {
        case DEMO_APP_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = app.callback_manager;

            break;
        }

        case DEMO_APP_PROPERTY_GPU_SCHEDULER:
        {
            *reinterpret_cast<ral_scheduler*>(out_result_ptr) = app.scheduler;

            break;
        }

        case DEMO_APP_PROPERTY_MATERIALS:
        {
            *reinterpret_cast<demo_materials*>(out_result_ptr) = app.materials;

            break;
        }

        case DEMO_APP_PROPERTY_N_WINDOWS:
        {
            system_hash64map_get_property(app.window_name_to_window_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                          out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_app_property value.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_name(system_hashed_ansi_string window_name)
{
    demo_window result = nullptr;

    /* Sanity checks */
    if (window_name                                       == nullptr ||
        system_hashed_ansi_string_get_length(window_name) == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid window name specified.");

        goto end;
    }

    /* Retrieve the window instance */
    system_critical_section_enter(app.cs);
    {
        system_hash64map_get(app.window_name_to_window_map,
                             system_hashed_ansi_string_get_hash(window_name),
                            &result);
    }
    system_critical_section_leave(app.cs);

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API demo_window demo_app_get_window_by_index(uint32_t n_window)
{
    demo_window result = nullptr;

    /* Retrieve the window instance */
    system_critical_section_enter(app.cs);
    {
        system_hash64 temp_hash;

        system_hash64map_get_element_at(app.window_name_to_window_map,
                                        n_window,
                                       &result,
                                       &temp_hash);
    }
    system_critical_section_leave(app.cs);

    /* All done */
    return result;
}

