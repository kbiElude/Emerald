#define _MSWIN

#include <stdio.h>     /* for NULL #define */
#include <Windows.h>

#include "plugin.h"
#include "plugin_cameras.h"
#include "plugin_curves.h"
#include "plugin_lights.h"
#include "plugin_materials.h"
#include "plugin_meshes.h"
#include "plugin_vmaps.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"
#include "system/system_variant.h"

/* Forward declarations */
LWCameraInfo*    camera_info_ptr   = NULL;
LWChannelInfo*   channel_info_ptr  = NULL;
LWEnvelopeFuncs* envelope_ptr      = NULL;
LWImageList*     image_list_ptr    = NULL;
LWItemInfo*      item_info_ptr     = NULL;
LWLightInfo*     light_info_ptr    = NULL;
LWMessageFuncs*  message_funcs_ptr = NULL;
LWObjectFuncs*   object_funcs_ptr  = NULL;
LWObjectInfo*    object_info_ptr   = NULL;
LWSceneInfo*     scene_info_ptr    = NULL;
LWSurfaceFuncs*  surface_funcs_ptr = NULL;
LWTextureFuncs*  texture_funcs_ptr = NULL;

/* Forward declarations. */
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
    camera_info_ptr   = (LWCameraInfo*)    global(LWCAMERAINFO_GLOBAL,    GFUSE_TRANSIENT);
    channel_info_ptr  = (LWChannelInfo*)   global(LWCHANNELINFO_GLOBAL,   GFUSE_TRANSIENT);
    envelope_ptr      = (LWEnvelopeFuncs*) global(LWENVELOPEFUNCS_GLOBAL, GFUSE_TRANSIENT);
    image_list_ptr    = (LWImageList*)     global(LWIMAGELIST_GLOBAL,     GFUSE_TRANSIENT);
    item_info_ptr     = (LWItemInfo*)      global(LWITEMINFO_GLOBAL,      GFUSE_TRANSIENT);
    light_info_ptr    = (LWLightInfo*)     global(LWLIGHTINFO_GLOBAL,     GFUSE_TRANSIENT);
    message_funcs_ptr = (LWMessageFuncs*)  global(LWMESSAGEFUNCS_GLOBAL,  GFUSE_TRANSIENT);
    object_funcs_ptr  = (LWObjectFuncs*)   global(LWOBJECTFUNCS_GLOBAL,   GFUSE_TRANSIENT);
    object_info_ptr   = (LWObjectInfo*)    global(LWOBJECTINFO_GLOBAL,    GFUSE_TRANSIENT);
    scene_info_ptr    = (LWSceneInfo*)     global(LWSCENEINFO_GLOBAL,     GFUSE_TRANSIENT);
    surface_funcs_ptr = (LWSurfaceFuncs*)  global(LWSURFACEFUNCS_GLOBAL,  GFUSE_TRANSIENT);
    texture_funcs_ptr = (LWTextureFuncs*)  global(LWTEXTUREFUNCS_GLOBAL,  GFUSE_TRANSIENT);

    /* Spawn a new scene */
    scene new_scene = scene_create(NULL, /* ogl_context */
                                   system_hashed_ansi_string_create("Exported scene") );

    /* Extract curve data */
    message_funcs_ptr->info("Extracting curve data..",
                            NULL);

    InitCurveData();

    /* Extract camera data */
    message_funcs_ptr->info("Extracting camera data",
                            NULL);

    InitCameraData         ();
    FillSceneWithCameraData(new_scene,
                            GetEnvelopeIDToCurveContainerHashMap() );

    /* Extract light data */
    message_funcs_ptr->info("Extracting light data..",
                            NULL);

    FillSceneWithLightData(new_scene,
                           GetEnvelopeIDToCurveContainerHashMap() );

    /* Extract surface data */
    message_funcs_ptr->info("Extracting surface data..",
                            NULL);

    InitMaterialData(GetEnvelopeIDToCurveContainerHashMap() );

    /* Extract vmap data */
    message_funcs_ptr->info("Extracting vmap data..",
                            NULL);

    InitVMapData();

    /* Extract mesh data */
    message_funcs_ptr->info("Extracting mesh data..",
                            NULL);

    InitMeshData         ();
    FillSceneWithMeshData(new_scene,
                          GetEnvelopeIDToCurveContainerHashMap() );

    /* Check where the user wants to store the data */
    message_funcs_ptr->info("Please select target file to store the blob.",
                            NULL);

    system_hashed_ansi_string filename = system_file_enumerator_choose_file_via_ui(SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_SAVE,
                                                                                   system_hashed_ansi_string_create("*"),
                                                                                   system_hashed_ansi_string_create("Emerald Scene blob"),
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

        message_funcs_ptr->info("Emerald Scene blob file saved.",
                                NULL);
    }
    else
    {
        message_funcs_ptr->warning("Emerald Scene blob file NOT saved.",
                                   NULL);
    }

    /* done! */
    DeinitCameraData  ();
    DeinitCurveData   ();
    DeinitMaterialData();
    DeinitMeshData    ();
    DeinitVMapData    ();

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
