#define _MSWIN

#include <stdio.h>     /* for NULL #define */
#include <Windows.h>

#include "plugin.h"
#include "plugin_cameras.h"
#include "plugin_curves.h"
#include "plugin_graph.h"
#include "plugin_lights.h"
#include "plugin_materials.h"
#include "plugin_meshes.h"
#include "plugin_misc.h"
#include "plugin_pack.h"
#include "plugin_ui.h"
#include "plugin_vmaps.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_capabilities.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_packer.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"
#include "system/system_threads.h"
#include "system/system_variant.h"

/* Global variable declarations */
extern "C"
{
LWCameraInfo*    camera_info_ptr       = NULL;
LWChannelInfo*   channel_info_ptr      = NULL;
LWEnvelopeFuncs* envelope_ptr          = NULL;
GlobalFunc*      global_func_ptr       = NULL;
HostDisplayInfo* host_display_info_ptr = NULL;
LWImageList*     image_list_ptr        = NULL;
LWInterfaceInfo* interface_info_ptr    = NULL;
LWItemInfo*      item_info_ptr         = NULL;
LWLightInfo*     light_info_ptr        = NULL;
LWMessageFuncs*  message_funcs_ptr     = NULL;
LWObjectFuncs*   object_funcs_ptr      = NULL;
LWObjectInfo*    object_info_ptr       = NULL;
LWPanelFuncs*    panel_funcs_ptr       = NULL;
LWSceneInfo*     scene_info_ptr        = NULL;
LWSurfaceFuncs*  surface_funcs_ptr     = NULL;
LWTextureFuncs*  texture_funcs_ptr     = NULL;
};

PRIVATE void LaunchPanel           ();
PRIVATE void UIThreadEntryPoint    (void* not_used);
PRIVATE void WorkerThreadEntryPoint(void* not_used);


/** TODO */
PRIVATE void WorkerThreadEntryPoint(void* not_used)
{
    /* Wait for the UI to nestle */
    system_event_wait_single_infinite(ui_initialized_event);

    /* Spawn a new scene */
    scene new_scene = scene_create(NULL, /* ogl_context */
                                   system_hashed_ansi_string_create("Exported scene") );

    /* Extract curve data.
     *
     * NOTE: InitCurveData() is a blocking call, so we need not synchronize. */
    InitCurveData();

    /* Extract camera / light / material / vmap data.
     *
     * These calls are not blocking, so we need to make sure they finish
     * before continuing.
     */
    system_event job_done_events[4] = {NULL};

    InitCameraData();
    job_done_events[0] = StartCameraDataExtraction(new_scene);

    /* Extract light data */
    InitLightData();
    job_done_events[1] = StartLightDataExtraction(new_scene);

    /* Extract surface data */
    job_done_events[2] = StartMaterialDataExtraction(new_scene);

    /* Extract vmap data */
    job_done_events[3] = StartVMapDataExtraction();

    /* Wait for the jobs to finish */
    system_event_wait_multiple_infinite(job_done_events,
                                        sizeof(job_done_events) / sizeof(job_done_events[0]),
                                        true); /* wait_on_all_objects */

    for (unsigned int n_job_event = 0;
                      n_job_event < sizeof(job_done_events) / sizeof(job_done_events[0]);
                    ++n_job_event)
    {
        system_event_release(job_done_events[n_job_event]);

        job_done_events[n_job_event] = NULL;
    }

    /* Extract mesh data. FillSceneWithMeshData() is a blocking call, so no need to sync here. */
    message_funcs_ptr->info("Extracting mesh data..",
                            NULL);

    InitMeshData         ();
    FillSceneWithMeshData(new_scene);

    /* Extract other props */
    FillMiscellaneousData(new_scene);

    /* Form a scene graph */
    message_funcs_ptr->info("Forming scene graph ..",
                            NULL);

    InitGraphData     ();
    FillSceneGraphData(new_scene);

    /* Sounds good! We can now safely copy curves that we have stored internally back to the scene
     * container. */
    FillSceneWithCurveData(new_scene);

    /* Check where the user wants to store the data */
    system_hashed_ansi_string filter_extension = system_hashed_ansi_string_create("*.scene");
    system_hashed_ansi_string filter_name      = system_hashed_ansi_string_create("Emerald Scene blob");

    message_funcs_ptr->info("Please select target file to store the blob.",
                            NULL);

    system_hashed_ansi_string filename = system_file_enumerator_choose_file_via_ui(SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_SAVE,
                                                                                   1, /* n_filters */
                                                                                   &filter_name,
                                                                                   &filter_extension,
                                                                                   system_hashed_ansi_string_create("Select target Emerald Scene blob file") );

    if (filename != NULL)
    {
        message_funcs_ptr->info("Saving data..",
                                NULL);

        /* Store the dataset */
        system_file_serializer serializer = system_file_serializer_create_for_writing(filename);

        scene_save_with_serializer(new_scene,
                                   serializer);

        system_file_serializer_release(serializer);
    }
    else
    {
        message_funcs_ptr->warning("Emerald Scene blob file NOT saved.",
                                   NULL);
    }

    /* Release all plugin modules */
    DeinitCameraData  ();
    DeinitCurveData   ();
    DeinitGraphData   ();
    DeinitLightData   ();
    DeinitMaterialData();
    DeinitMeshData    ();
    DeinitVMapData    ();

    /* Now is the time to pack all files into a final blob! */
    if (filename != NULL)
    {
        system_hashed_ansi_string filename_packed = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(filename),
                                                                                                            ".packed");

        AddFileToFinalBlob(filename);
        SaveFinalBlob     (filename_packed);
    }

    DeinitPackData();

    /* done! Kill the panel. */
    DeinitUI();

    /* Finally release all LW pointers */
    global_func_ptr(LWCAMERAINFO_GLOBAL,      GFUSE_RELEASE);
    global_func_ptr(LWCHANNELINFO_GLOBAL,     GFUSE_RELEASE);
    global_func_ptr(LWENVELOPEFUNCS_GLOBAL,   GFUSE_RELEASE);
    global_func_ptr(LWHOSTDISPLAYINFO_GLOBAL, GFUSE_RELEASE);
    global_func_ptr(LWIMAGELIST_GLOBAL,       GFUSE_RELEASE);
    global_func_ptr(LWINTERFACEINFO_GLOBAL,   GFUSE_RELEASE);
    global_func_ptr(LWITEMINFO_GLOBAL,        GFUSE_RELEASE);
    global_func_ptr(LWLIGHTINFO_GLOBAL,       GFUSE_RELEASE);
    global_func_ptr(LWMESSAGEFUNCS_GLOBAL,    GFUSE_RELEASE);
    global_func_ptr(LWOBJECTFUNCS_GLOBAL,     GFUSE_RELEASE);
    global_func_ptr(LWOBJECTINFO_GLOBAL,      GFUSE_RELEASE);
    global_func_ptr(LWPANELFUNCS_GLOBAL,      GFUSE_RELEASE);
    global_func_ptr(LWSCENEINFO_GLOBAL,       GFUSE_RELEASE);
    global_func_ptr(LWSURFACEFUNCS_GLOBAL,    GFUSE_RELEASE);
    global_func_ptr(LWTEXTUREFUNCS_GLOBAL,    GFUSE_RELEASE);
}

/* TODO */
XCALL_(int) ExportData(int         version,
                       GlobalFunc* global,
                       void*       local,
                       void*       serverData)
{
    if (version != LWLAYOUTGENERIC_VERSION)
    {
       return AFUNC_BADVERSION;
    }

    /* Retrieve func ptr structures */
    global_func_ptr = global;

    camera_info_ptr       = (LWCameraInfo*)    global(LWCAMERAINFO_GLOBAL,      GFUSE_ACQUIRE);
    channel_info_ptr      = (LWChannelInfo*)   global(LWCHANNELINFO_GLOBAL,     GFUSE_ACQUIRE);
    envelope_ptr          = (LWEnvelopeFuncs*) global(LWENVELOPEFUNCS_GLOBAL,   GFUSE_ACQUIRE);
    host_display_info_ptr = (HostDisplayInfo*) global(LWHOSTDISPLAYINFO_GLOBAL, GFUSE_ACQUIRE);
    image_list_ptr        = (LWImageList*)     global(LWIMAGELIST_GLOBAL,       GFUSE_ACQUIRE);
    interface_info_ptr    = (LWInterfaceInfo*) global(LWINTERFACEINFO_GLOBAL,   GFUSE_ACQUIRE);
    item_info_ptr         = (LWItemInfo*)      global(LWITEMINFO_GLOBAL,        GFUSE_ACQUIRE);
    light_info_ptr        = (LWLightInfo*)     global(LWLIGHTINFO_GLOBAL,       GFUSE_ACQUIRE);
    message_funcs_ptr     = (LWMessageFuncs*)  global(LWMESSAGEFUNCS_GLOBAL,    GFUSE_ACQUIRE);
    object_funcs_ptr      = (LWObjectFuncs*)   global(LWOBJECTFUNCS_GLOBAL,     GFUSE_ACQUIRE);
    object_info_ptr       = (LWObjectInfo*)    global(LWOBJECTINFO_GLOBAL,      GFUSE_ACQUIRE);
    panel_funcs_ptr       = (LWPanelFuncs*)    global(LWPANELFUNCS_GLOBAL,      GFUSE_ACQUIRE);
    scene_info_ptr        = (LWSceneInfo*)     global(LWSCENEINFO_GLOBAL,       GFUSE_ACQUIRE);
    surface_funcs_ptr     = (LWSurfaceFuncs*)  global(LWSURFACEFUNCS_GLOBAL,    GFUSE_ACQUIRE);
    texture_funcs_ptr     = (LWTextureFuncs*)  global(LWTEXTUREFUNCS_GLOBAL,    GFUSE_ACQUIRE);

    /* Set up packer module */
    InitPackData();

    /* Set up UI */
    InitUI();

    /* Spawn the worker thread which is going to do all the dirty work.
     * We need to give back the execution to LW ASAP, so that the user can
     * continue using LW.
     */
    system_threads_spawn(WorkerThreadEntryPoint,
                         NULL, /* argument */
                         NULL  /* thread_wait_event */);

    return AFUNC_OK;
}


/*
======================================================================
This is the server description.  LightWave looks at this first to
determine what plug-ins the file contains.  It lists each plug-in's
class and internal name, along with a pointer to the activation
function.  You can optionally add a user name, or more than one in
different languages, if you like.
====================================================================== */
ServerRecord ServerDesc[] =
{
   {LWLAYOUTGENERIC_CLASS, "Emerald: Export Scene to Emerald blob", ExportData},
   {NULL}
};
