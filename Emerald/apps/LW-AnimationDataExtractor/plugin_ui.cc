#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_ui.h"
#include "system/system_assertions.h"
#include "system/system_capabilities.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hash64map.h"
#include "system/system_threads.h"

PRIVATE LWControl**             edit_controls                      = NULL;
PRIVATE unsigned int            n_cpu_cores                        = 0;
PRIVATE LWPanelID               panel_id                           = 0;
PRIVATE system_hash64map        thread_id_to_cpu_core_index_map    = NULL;
PRIVATE system_critical_section thread_id_to_cpu_core_index_map_cs = NULL;

PUBLIC system_event ui_initialized_event = NULL;

/** TODO */
PRIVATE void LaunchPanel()
{
    LWPanControlDesc desc;                      /* for LWPanel macros */
    LWValue          sval              = {LWT_STRING};
    char             text_buffer[1024] = {0};
    char*            text_inactive     = "Inactive";

    panel_funcs_ptr->globalFun = global_func_ptr;

    /* Allocate thread ID->CPU core index map */
    thread_id_to_cpu_core_index_map    = system_hash64map_create       (sizeof(unsigned int) );
    thread_id_to_cpu_core_index_map_cs = system_critical_section_create();

    /* Allocate storage for edit control IDs */
    system_capabilities_get(SYSTEM_CAPABILITIES_PROPERTY_NUMBER_OF_CPU_CORES,
                           &n_cpu_cores);

    edit_controls = new LWControl*[n_cpu_cores];
    ASSERT_ALWAYS_SYNC(edit_controls != NULL,
                       "Out of memory");

    /* Create a panel with read-only edit controls */
    panel_id = PAN_CREATE(panel_funcs_ptr,
                          "Emerald LW Exporter plug-in");

    for (unsigned int n_cpu_core = 0;
                      n_cpu_core < n_cpu_cores;
                    ++n_cpu_core)
    {
        sprintf_s(text_buffer,
                  sizeof(text_buffer),
                  "Thread %d activity: ",
                  n_cpu_core);

        edit_controls[n_cpu_core] = STRRO_CTL(panel_funcs_ptr,
                                              panel_id,
                                              text_buffer,
                                              100);

        SET_STR(edit_controls[n_cpu_core],
                text_inactive,
                strlen(text_inactive) );
    }

    /* All's good to go */
    system_event_set(ui_initialized_event);

    /* Show the panel in a blocking manner */
    panel_funcs_ptr->open(panel_id,
                          PANF_BLOCKING | PANF_NOBUTT);

    /* Clean up */
    if (edit_controls != NULL)
    {
        delete [] edit_controls;

        edit_controls = NULL;
    }
}

/** TODO */
PRIVATE void UIThreadEntryPoint(void* not_used)
{
    /* Set up the UI panel */
    LaunchPanel();
}


/** TODO */
PUBLIC void DeinitUI()
{
    panel_funcs_ptr->close(panel_id);

    system_critical_section_enter(thread_id_to_cpu_core_index_map_cs);
    {
        if (thread_id_to_cpu_core_index_map != NULL)
        {
            system_hash64map_release(thread_id_to_cpu_core_index_map);

            thread_id_to_cpu_core_index_map = NULL;
        }
    }
    system_critical_section_leave(thread_id_to_cpu_core_index_map_cs);

    if (thread_id_to_cpu_core_index_map_cs != NULL)
    {
        system_critical_section_release(thread_id_to_cpu_core_index_map_cs);

        thread_id_to_cpu_core_index_map_cs = NULL;
    }
}

/** Please see header for spec */
PUBLIC void InitUI()
{
    /* Set up events used for inter-thread comms */
    ui_initialized_event = system_event_create(true); /* manual_reset */

    /* Spawn a new UI controller thread. */
    system_threads_spawn(UIThreadEntryPoint,
                         NULL, /* argument */
                         NULL  /* thread_wait_event */);
}

/** Please see header for spec */
PUBLIC void SetActivityDescription(char* text)
{
    /* The plug-in uses system thread pool to execute tasks. This
     * allows us to map thread IDs to specific CPU cores.
     */
    unsigned int core_index = -1;

    system_critical_section_enter(thread_id_to_cpu_core_index_map_cs);
    {
        system_thread_id caller_thread_id = system_threads_get_thread_id();

        if (!system_hash64map_get(thread_id_to_cpu_core_index_map,
                                  (system_hash64) caller_thread_id,
                                 &core_index) )
        {
            system_hash64map_get_property(thread_id_to_cpu_core_index_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &core_index);

            ASSERT_DEBUG_SYNC(core_index < n_cpu_cores,
                              "Invalid CPU core index about to be used");

            system_hash64map_insert(thread_id_to_cpu_core_index_map,
                                    (system_hash64) caller_thread_id,
                                    (void*)         core_index,
                                    NULL,           /* on_remove_callback */
                                    NULL);          /* on_remove_callback_user_arg */
        }
    }
    system_critical_section_leave(thread_id_to_cpu_core_index_map_cs);

    /* Update the edit control contents.
     *
     * NOTE: LW is pretty stubborn and will start ignoring redraw requests
     *       at some point. To work around this, I'm nudging it by posting
     *       mouse move events to its main window. Seems to do the trick,
     *       despite how awful that solution really is.
     */
    LWValue sval = {LWT_STRING};

    SET_STR(edit_controls[core_index],
            text,
            strlen(text) );

    ::PostMessage (host_display_info_ptr->window,
                   WM_MOUSEMOVE,
                   0,
                   0);
}
