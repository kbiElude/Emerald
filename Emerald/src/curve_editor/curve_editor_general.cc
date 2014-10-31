/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_general.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_main_window.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"

/* Private variables */
PRIVATE system_critical_section  global_cs   = NULL;
PRIVATE curve_editor_main_window main_window = NULL; /* access only within global_cs */

/* Please see header for specification */
PUBLIC void _curve_editor_deinit()
{
    /* Close the main window, if it is up */
    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            curve_editor_main_window_release(main_window);

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
PUBLIC void _curve_editor_init()
{
    global_cs = system_critical_section_create();
}

/** TODO */
PRIVATE void _curve_editor_on_main_window_release()
{
    system_critical_section_enter(global_cs);
    {
        ASSERT_DEBUG_SYNC(main_window != NULL, "Release notification received for an unrecognized object.");
        
        main_window = NULL;
    }
    system_critical_section_leave(global_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API bool curve_editor_hide()
{
    bool result = false;

    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            curve_editor_main_window_release(main_window);

            main_window = NULL;
        }
    }
    system_critical_section_leave(global_cs);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void curve_editor_set_property(__in __notnull ogl_context           context,
                                                  __in           curve_editor_property property,
                                                  __in __notnull void*                 data)
{
    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            switch (property)
            {
                case CURVE_EDITOR_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH:
                {
                    curve_editor_main_window_set_property(main_window,
                                                          CURVE_EDITOR_MAIN_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH,
                                                          data);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized curve_editor_property value");
                }
            } /* switch (property) */
        }
    }
    system_critical_section_leave(global_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API bool curve_editor_show(ogl_context context)
{
    bool result = false;

    system_critical_section_enter(global_cs);
    {
        if (main_window == NULL)
        {
            main_window = curve_editor_main_window_create(_curve_editor_on_main_window_release, context);

            ASSERT_DEBUG_SYNC(main_window != NULL, "Could not create curve editor's main window");
            result = (main_window != NULL);
        }
    }
    system_critical_section_leave(global_cs);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void curve_editor_update_curve_list()
{
    system_critical_section_enter(global_cs);
    {
        if (main_window != NULL)
        {
            curve_editor_main_window_update_curve_list(main_window);
        }
    }
    system_critical_section_leave(global_cs);
}