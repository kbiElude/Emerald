/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "sh_light_editor/sh_light_editor_types.h"
#include "sh_light_editor/sh_light_editor_main_window.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"


/* Private variables */
PRIVATE system_critical_section     global_cs   = NULL;
PRIVATE sh_light_editor_main_window main_window = NULL; /* access only within global_cs */

/* Please see header for specification */
PUBLIC void _sh_light_editor_deinit()
{
    /* Close the main window, if it is up */
    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            sh_light_editor_main_window_release(main_window);

            main_window = NULL;
        }
    }
    system_critical_section_leave(global_cs);

    /* Carry on with deinitialization */
    if (global_cs != NULL)
    {
        system_critical_section_release(global_cs);

        global_cs = NULL;
    }
}

/* Please see header for specification */
PUBLIC void _sh_light_editor_init()
{
    global_cs = system_critical_section_create();
}

/** TODO */
PRIVATE void _sh_light_editor_on_main_window_release()
{
    system_critical_section_enter(global_cs);
    {
        ASSERT_DEBUG_SYNC(main_window != NULL, "Release notification received for an unrecognized object.");
        
        main_window = NULL;
    }
    system_critical_section_leave(global_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API bool sh_light_editor_hide()
{
    bool result = false;

    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            sh_light_editor_main_window_release(main_window);

            main_window = NULL;
        }
    }
    system_critical_section_leave(global_cs);

    return result;
}

/* PLease see header for specification */
PUBLIC EMERALD_API bool sh_light_editor_refresh()
{
    bool result = false;

    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            sh_light_editor_main_window_refresh(main_window);

            result = true;
        }
    }
    system_critical_section_leave(global_cs);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool sh_light_editor_show(ogl_context context)
{
    bool result = false;

    system_critical_section_enter(global_cs);
    {
        if (main_window == NULL)
        {
            main_window = sh_light_editor_main_window_create(_sh_light_editor_on_main_window_release, context);

            ASSERT_DEBUG_SYNC(main_window != NULL, "Could not create SH light editor's main window");
            result = (main_window != NULL);
        }
    }
    system_critical_section_leave(global_cs);

    return result;
}
