/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "main.h"
#include "scene_renderer/scene_renderer_materials.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"


typedef struct _demo_app
{
    system_callback_manager  callback_manager;
    system_critical_section  cs;
    scene_renderer_materials materials;
    system_hash64map         window_name_to_window_map;

    _demo_app()
    {
        callback_manager          = system_callback_manager_create ((_callback_id) DEMO_APP_CALLBACK_ID_COUNT);
        cs                        = system_critical_section_create ();
        materials                 = scene_renderer_materials_create();
        window_name_to_window_map = system_hash64map_create        (sizeof(demo_window) );
    }

    void close()
    {
        system_critical_section_enter(cs);
        {
            if (callback_manager != NULL)
            {
                system_callback_manager_release(callback_manager);

                callback_manager = NULL;
            }

            if (materials != NULL)
            {
                scene_renderer_materials_release(materials);

                materials = NULL;
            }

            if (window_name_to_window_map != NULL)
            {
                demo_window   dangling_window           = NULL;
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
                } /* while (dangling windows remain) */

                system_hash64map_release(window_name_to_window_map);
                window_name_to_window_map = NULL;
            }
        }
        system_critical_section_leave(cs);

        system_critical_section_release(cs);
        cs = NULL;
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
    demo_window   result           = NULL;
    system_hash64 window_name_hash = 0;

    /* Sanity checks */
    if (window_name                                       == NULL ||
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

    if (result == NULL)
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
                                NULL,  /* on_removal_callback */
                                NULL); /* on_removal_callback_user_arg */
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
    demo_window   window           = NULL;
    system_hash64 window_name_hash = 0;

    /* Sanity checks */
    if (window_name                                       == NULL ||
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
            *(system_callback_manager*) out_result_ptr = app.callback_manager;

            break;
        }

        case DEMO_APP_PROPERTY_MATERIAL_MANAGER:
        {
            *(scene_renderer_materials*) out_result_ptr = app.materials;

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
    demo_window result = NULL;

    /* Sanity checks */
    if (window_name                                       == NULL ||
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
    demo_window result = NULL;

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

