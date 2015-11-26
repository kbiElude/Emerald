/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "main.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"


typedef struct _demo_app
{
    system_critical_section cs;
    system_hash64map        window_name_to_window_map;

    _demo_app()
    {
        cs                        = system_critical_section_create();
        window_name_to_window_map = system_hash64map_create       (sizeof(demo_window) );
    }

    ~_demo_app()
    {
        system_critical_section_enter(cs);
        {
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


/** Please see header for specification */
PUBLIC EMERALD_API demo_app demo_app_create()
{
    _demo_app* new_app_ptr = new _demo_app();

    ASSERT_DEBUG_SYNC(new_app_ptr != NULL,
                      "Out of memory");

    return (demo_app) new_app_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API demo_window demo_app_create_window(demo_app                  app,
                                                      system_hashed_ansi_string window_name,
                                                      ral_backend_type          backend_type)
{
    _demo_app*    app_ptr          = (_demo_app*) app;
    demo_window   result           = NULL;
    system_hash64 window_name_hash = 0;

    /* Sanity checks */
    if (app == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input demo_app instance is NULL");

        goto end;
    }

    if (window_name                                       == NULL ||
        system_hashed_ansi_string_get_length(window_name) == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid window name specified.");

        goto end;
    }

    /* Spawn a new window instance */
    result = demo_window_create(window_name,
                                backend_type);

    if (result == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "demo_window_create() failed.");

        goto end;
    }

    /* Store it */
    system_critical_section_enter(app_ptr->cs);
    {
        window_name_hash = system_hashed_ansi_string_get_hash(window_name);

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(app_ptr->window_name_to_window_map,
                                                     window_name_hash),
                          "The specified window name is already reserved");

        system_hash64map_insert(app_ptr->window_name_to_window_map,
                                window_name_hash,
                                result,
                                NULL,  /* on_removal_callback */
                                NULL); /* on_removal_callback_user_arg */
    }
    system_critical_section_leave(app_ptr->cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_app_destroy_window(demo_app                  app,
                                                system_hashed_ansi_string window_name)
{
    _demo_app*    app_ptr          = (_demo_app*) app;
    bool          result           = false;
    demo_window   window           = NULL;
    system_hash64 window_name_hash = 0;

    /* Sanity checks */
    if (app == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input demo_app instance is NULL");

        goto end;
    }

    if (window_name                                       == NULL ||
        system_hashed_ansi_string_get_length(window_name) == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid window name specified.");

        goto end;
    }

    /* Retrieve the window's instance */
    window_name_hash = system_hashed_ansi_string_get_hash(window_name);

    system_critical_section_enter(app_ptr->cs);
    {
        if (!system_hash64map_get(app_ptr->window_name_to_window_map,
                                  window_name_hash,
                                 &window) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No demo_window instance associated with name [%s]",
                              system_hashed_ansi_string_get_buffer(window_name) );

            goto end;
        }

        system_hash64map_remove(app_ptr->window_name_to_window_map,
                                window_name_hash);
    }
    system_critical_section_leave(app_ptr->cs);

    /* Deallocate the retrieved window */
    demo_window_release(window);

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_app_release(demo_app app)
{
    /* Force Emerald's internal resources deinitialization. This call is optional. */
    main_force_deinit();

    /* Release all stuff */
    delete (_demo_app*) app;
}
