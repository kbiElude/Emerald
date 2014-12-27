#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_ui.h"
#include "system/system_assertions.h"
#include "system/system_capabilities.h"
#include "system/system_event.h"
#include "system/system_threads.h"

PRIVATE LWControl**  edit_controls        = NULL;
PRIVATE LWPanelID    panel_id             = 0;

system_event ui_initialized_event = NULL;

/** TODO */
PRIVATE void LaunchPanel()
{
    LWPanControlDesc desc;                      /* for LWPanel macros */
    unsigned int     n_cpu_cores       = 0;
    LWValue          sval              = {LWT_STRING};
    char             text_buffer[1024] = {0};
    char*            text_inactive     = "Inactive";

    panel_funcs_ptr->globalFun = global_func_ptr;

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
                                              30);

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
}

/** Please see header for spec */
PUBLIC void InitUI()
{
    /* Set up events used for inter-thread comms */
    ui_initialized_event = system_event_create(true,   /* manual_reset */
                                               false); /* start_state  */

    /* Spawn a new UI controller thread. */
    system_threads_spawn(UIThreadEntryPoint,
                         NULL, /* argument */
                         NULL  /* thread_wait_event */);
}

