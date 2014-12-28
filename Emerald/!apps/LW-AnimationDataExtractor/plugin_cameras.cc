#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_cameras.h"
#include "plugin_common.h"
#include "plugin_curves.h"
#include "plugin_ui.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_thread_pool.h"

typedef struct _camera_internal
{
    curve_container camera_rotation_hpb   [3];
    curve_container camera_translation_xyz[3];
    void*           object_id;
    void*           parent_object_id;

    _camera_internal()
    {
        memset(camera_rotation_hpb,
               0,
               sizeof(camera_rotation_hpb) );
        memset(camera_translation_xyz,
               0,
               sizeof(camera_translation_xyz) );

        object_id        = NULL;
        parent_object_id = NULL;
    }

    ~_camera_internal()
    {
        for (unsigned int n_component = 0;
                          n_component < 3;
                        ++n_component)
        {
            if (camera_rotation_hpb[n_component] != NULL)
            {
                curve_container_release(camera_rotation_hpb[n_component]);

                camera_rotation_hpb[n_component] = NULL;
            }

            if (camera_translation_xyz[n_component] != NULL)
            {
                curve_container_release(camera_translation_xyz[n_component]);

                camera_translation_xyz[n_component] = NULL;
            }
        }
    }
} _camera_internal;


PRIVATE system_event     job_done_event                      = NULL;
PRIVATE system_hash64map scene_camera_to_camera_internal_map = NULL;


/** TODO */
volatile void ExtractCameraDataWorkerThreadEntryPoint(__in __notnull void* in_scene_arg)
{
    system_hash64map envelope_id_to_curve_container_map = GetEnvelopeIDToCurveContainerHashMap();
    scene            in_scene                           = (scene) in_scene_arg;
    char             text_buffer[1024];

    /* Extract available cameras */
    LWItemID camera_item_id = item_info_ptr->first(LWI_CAMERA,
                                                   LWITEM_NULL);

    while (camera_item_id != LWITEM_NULL)
    {
        /* Extract camera name */
        const char*               camera_name     = item_info_ptr->name             (camera_item_id);
        system_hashed_ansi_string camera_name_has = system_hashed_ansi_string_create(camera_name);

        sprintf_s(text_buffer,
                  sizeof(text_buffer),
                  "Extracting camera [%s] data..",
                  camera_name);

        SetActivityDescription(text_buffer);

        /* Sanity check: no pivot */
        LWDVector pivot;

        item_info_ptr->param(camera_item_id,
                             LWIP_PIVOT,
                             0, /* time */
                             pivot);

        ASSERT_DEBUG_SYNC(fabs(pivot[0]) < 1e-5f &&
                          fabs(pivot[1]) < 1e-5f &&
                          fabs(pivot[2]) < 1e-5f,
                          "Camera pivoting is not currently supported");

        /* Instantiate a new camera */
        scene_camera new_camera = scene_camera_create(camera_name_has);

        ASSERT_ALWAYS_SYNC(new_camera != NULL,
                           "scene_camera_create() failed.");

        /* Retrieve position curves */
        curve_container new_camera_position_x = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_TRANSLATION_X,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_position_y = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_TRANSLATION_Y,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_position_z = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_TRANSLATION_Z,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);

        /* Retrieve rotation curves */
        curve_container new_camera_rotation_b = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_ROTATION_B,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_rotation_h = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_ROTATION_H,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_rotation_p = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_ROTATION_P,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);

        /* Retrieve F-Stop data */
        curve_container new_camera_fstop = GetCurveContainerForProperty(camera_name_has,
                                                                        ITEM_PROPERTY_F_STOP,
                                                                        camera_item_id,
                                                                        envelope_id_to_curve_container_map);

        /* Retrieve Focal Distance data */
        curve_container new_camera_focal_distance = GetCurveContainerForProperty(camera_name_has,
                                                                                 ITEM_PROPERTY_FOCAL_DISTANCE,
                                                                                 camera_item_id,
                                                                                 envelope_id_to_curve_container_map);

        /* Retrieve Zoom Factor data */
        curve_container new_camera_zoom_factor = GetCurveContainerForProperty(camera_name_has,
                                                                              ITEM_PROPERTY_ZOOM_FACTOR,
                                                                              camera_item_id,
                                                                              envelope_id_to_curve_container_map);

        /* Configure the camera using the properties we've retrieved */
        const bool uses_physical_properties = true;

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE,
                                 &new_camera_focal_distance);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_F_STOP,
                                 &new_camera_fstop);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_ZOOM_FACTOR,
                                 &new_camera_zoom_factor);

        /* Add the camera */
        if (!scene_add_camera(in_scene,
                              new_camera) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not add new camera [%s]",
                               camera_name);
        }

        /* Store the camera position/rotation data in internal data structures */
        curve_container new_camera_positions[] =
        {
            new_camera_position_x,
            new_camera_position_y,
            new_camera_position_z,
        };
        curve_container new_camera_rotations[] =
        {
            new_camera_rotation_h,
            new_camera_rotation_p,
            new_camera_rotation_b
        };
        _camera_internal* new_camera_internal_ptr  = new (std::nothrow) _camera_internal;

        ASSERT_ALWAYS_SYNC(new_camera_internal_ptr != NULL,
                           "Out of memory");

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(scene_camera_to_camera_internal_map,
                                                     (system_hash64) new_camera),
                          "Camera already recognized");

        memcpy(new_camera_internal_ptr->camera_rotation_hpb,
               new_camera_rotations,
               sizeof(new_camera_internal_ptr->camera_rotation_hpb) );
        memcpy(new_camera_internal_ptr->camera_translation_xyz,
               new_camera_positions,
               sizeof(new_camera_internal_ptr->camera_translation_xyz) );

        new_camera_internal_ptr->object_id        = camera_item_id;
        new_camera_internal_ptr->parent_object_id = item_info_ptr->parent(camera_item_id);

        system_hash64map_insert(scene_camera_to_camera_internal_map,
                                (system_hash64) new_camera,
                                new_camera_internal_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */

        /* Move on */
        camera_item_id = item_info_ptr->next(camera_item_id);
    }

    SetActivityDescription("Inactive");

    system_event_set(job_done_event);
}


/** TODO */
PUBLIC void DeinitCameraData()
{
    if (scene_camera_to_camera_internal_map != NULL)
    {
        _camera_internal* camera_internal_ptr = NULL;
        const uint32_t    n_camera_internals  = system_hash64map_get_amount_of_elements(scene_camera_to_camera_internal_map);

        for (uint32_t n_camera_internal = 0;
                      n_camera_internal < n_camera_internals;
                    ++n_camera_internal)
        {
            if (!system_hash64map_get_element_at(scene_camera_to_camera_internal_map,
                                                 n_camera_internal,
                                                &camera_internal_ptr,
                                                 NULL) ) /* outHash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not extract camera internal descriptor");

                continue;
            }

            delete camera_internal_ptr;
            camera_internal_ptr = NULL;
        }

        system_hash64map_release(scene_camera_to_camera_internal_map);
        scene_camera_to_camera_internal_map = NULL;
    }
}

/** TODO */
PUBLIC system_event StartCameraDataExtraction(__in __notnull scene in_scene)
{
    job_done_event = system_event_create(false,  /* manual_reset */
                                         false); /* start_state */

    /* Spawn a worker thread so that we can report the progress. */
    system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                     ExtractCameraDataWorkerThreadEntryPoint,
                                                                                                     in_scene);

    system_thread_pool_submit_single_task(task);

    return job_done_event;
}

/* Please see header for spec */
PUBLIC void GetCameraPropertyValue(__in  __notnull scene_camera   camera,
                                   __in            CameraProperty property,
                                   __out __notnull void*          out_result)
{
    _camera_internal* camera_ptr = NULL;

    if (!system_hash64map_get(scene_camera_to_camera_internal_map,
                              (system_hash64) camera,
                             &camera_ptr) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not find descriptor for input scene_camera");

        goto end;
    }

    switch (property)
    {
        case CAMERA_PROPERTY_OBJECT_ID:
        {
            *(void**) out_result = camera_ptr->object_id;

            break;
        }

        case CAMERA_PROPERTY_PARENT_OBJECT_ID:
        {
            *(void**) out_result = camera_ptr->parent_object_id;

            break;
        }

        case CAMERA_PROPERTY_ROTATION_HPB_CURVES:
        {
            *(curve_container**) out_result = camera_ptr->camera_rotation_hpb;

            break;
        }

        case CAMERA_PROPERTY_TRANSLATION_CURVES:
        {
            *(curve_container**) out_result = camera_ptr->camera_translation_xyz;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized CameraProperty value");
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for spec */
PUBLIC void InitCameraData()
{
    ASSERT_DEBUG_SYNC(scene_camera_to_camera_internal_map == NULL,
                      "Camera data already initialized");

    scene_camera_to_camera_internal_map = system_hash64map_create(sizeof(_camera_internal*) );
}
