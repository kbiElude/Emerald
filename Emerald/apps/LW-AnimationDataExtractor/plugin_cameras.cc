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
    curve_id camera_rotation_hpb_ids   [3];
    curve_id camera_translation_xyz_ids[3];
    void*    object_id;
    void*    parent_object_id;
    float    pivot[3];

    _camera_internal()
    {
        memset(camera_rotation_hpb_ids,
               0,
               sizeof(camera_rotation_hpb_ids) );
        memset(camera_translation_xyz_ids,
               0,
               sizeof(camera_translation_xyz_ids) );
        memset(pivot,
               0,
               sizeof(pivot) );

        object_id        = NULL;
        parent_object_id = NULL;
    }

    ~_camera_internal()
    {
        /* Nothing to do */
    }
} _camera_internal;


PRIVATE system_event     job_done_event                      = NULL;
PRIVATE system_hash64map scene_camera_to_camera_internal_map = NULL;


/** TODO */
volatile void ExtractCameraDataWorkerThreadEntryPoint(void* in_scene_arg)
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

        /* Extract pivot data */
        LWDVector pivot;

        item_info_ptr->param(camera_item_id,
                             LWIP_PIVOT,
                             0, /* time */
                             pivot);

        /* Instantiate a new camera */
        scene_camera new_camera = scene_camera_create(camera_name_has,
                                                      NULL /* scene_camera */);

        ASSERT_ALWAYS_SYNC(new_camera != NULL,
                           "scene_camera_create() failed.");

        /* Retrieve position curves */
        curve_container new_camera_position_x_curve    = NULL;
        curve_id        new_camera_position_x_curve_id = -1;
        curve_container new_camera_position_y_curve    = NULL;
        curve_id        new_camera_position_y_curve_id = -1;
        curve_container new_camera_position_z_curve    = NULL;
        curve_id        new_camera_position_z_curve_id = -1;

        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_TRANSLATION_X,
                                     camera_item_id,
                                    &new_camera_position_x_curve,
                                    &new_camera_position_x_curve_id);
        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_TRANSLATION_Y,
                                     camera_item_id,
                                    &new_camera_position_y_curve,
                                    &new_camera_position_y_curve_id);
        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_TRANSLATION_Z,
                                     camera_item_id,
                                    &new_camera_position_z_curve,
                                    &new_camera_position_z_curve_id);

        /* Adjust translation curves relative to the pivot */
        for (uint32_t n_translation_curve = 0;
                      n_translation_curve < 3;
                    ++n_translation_curve)
        {
            curve_container curve                       = (n_translation_curve == 0) ? new_camera_position_x_curve :
                                                          (n_translation_curve == 1) ? new_camera_position_y_curve :
                                                                                       new_camera_position_z_curve;
            const float     pivot_translation_component = (float) pivot[n_translation_curve];

            AdjustCurveByDelta(curve,
                               -pivot_translation_component);
        } /* for (all three translation curves) */

        /* Retrieve rotation curves */
        curve_container new_camera_rotation_b_curve    = NULL;
        curve_id        new_camera_rotation_b_curve_id = -1;
        curve_container new_camera_rotation_h_curve    = NULL;
        curve_id        new_camera_rotation_h_curve_id = -1;
        curve_container new_camera_rotation_p_curve    = NULL;
        curve_id        new_camera_rotation_p_curve_id = -1;
        curve_container null_curve_container           = NULL;

        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_ROTATION_B,
                                     camera_item_id,
                                    &new_camera_rotation_b_curve,
                                    &new_camera_rotation_b_curve_id);
        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_ROTATION_H,
                                     camera_item_id,
                                    &new_camera_rotation_h_curve,
                                    &new_camera_rotation_h_curve_id);
        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_ROTATION_P,
                                     camera_item_id,
                                    &new_camera_rotation_p_curve,
                                    &new_camera_rotation_p_curve_id);

        /* Retrieve F-Stop data */
        curve_container new_camera_fstop_curve    = NULL;
        curve_id        new_camera_fstop_curve_id = -1;

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_F_STOP,
                                 &null_curve_container);

        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_F_STOP,
                                     camera_item_id,
                                    &new_camera_fstop_curve,
                                    &new_camera_fstop_curve_id);

        /* Retrieve Focal Distance data */
        curve_container new_camera_focal_distance_curve    = NULL;
        curve_id        new_camera_focal_distance_curve_id = -1;

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE,
                                 &null_curve_container);

        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_FOCAL_DISTANCE,
                                     camera_item_id,
                                    &new_camera_focal_distance_curve,
                                    &new_camera_focal_distance_curve_id);

        /* Retrieve Zoom Factor data */
        curve_container new_camera_zoom_factor_curve    = NULL;
        curve_id        new_camera_zoom_factor_curve_id = -1;

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_ZOOM_FACTOR,
                                 &null_curve_container);

        GetCurveContainerForProperty(camera_name_has,
                                     ITEM_PROPERTY_ZOOM_FACTOR,
                                     camera_item_id,
                                    &new_camera_zoom_factor_curve,
                                    &new_camera_zoom_factor_curve_id);

        /* Configure the camera using the properties we've retrieved */
        const bool uses_physical_properties = true;
        const bool uses_custom_yfov         = false;

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE,
                                 &new_camera_focal_distance_curve);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_F_STOP,
                                 &new_camera_fstop_curve);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_ZOOM_FACTOR,
                                 &new_camera_zoom_factor_curve);

        /* NOTE: We do not export properties like vertical FOV from LW (since it's calculated
         *       from Zoom Factor), but we still need to add them to the Curves Module, or
         *       scene_save() will crash.
         *
         * NOTE: We only modify use_custom_vertical_fov after we retrieve vertical fov curve,
         *       because otherwise the getter would throw an assertion failure.
         */
        curve_container yfov_curve = NULL;

        scene_camera_get_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_VERTICAL_FOV_CUSTOM,
                                  0, /* time - irrelevant */
                                 &yfov_curve);

        AddCurveContainerToEnvelopeIDToCurveContainerHashMap(yfov_curve);

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_USE_CUSTOM_VERTICAL_FOV,
                                 &uses_custom_yfov);

        /* Add the camera */
        if (!scene_add_camera(in_scene,
                              new_camera) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not add new camera [%s]",
                               camera_name);
        }

        /* Store the camera position/rotation data in internal data structures */
        curve_id new_camera_positions[] =
        {
            new_camera_position_x_curve_id,
            new_camera_position_y_curve_id,
            new_camera_position_z_curve_id,
        };
        curve_id new_camera_rotations[] =
        {
            new_camera_rotation_h_curve_id,
            new_camera_rotation_p_curve_id,
            new_camera_rotation_b_curve_id
        };
        _camera_internal* new_camera_internal_ptr  = new (std::nothrow) _camera_internal;

        ASSERT_ALWAYS_SYNC(new_camera_internal_ptr != NULL,
                           "Out of memory");

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(scene_camera_to_camera_internal_map,
                                                     (system_hash64) new_camera),
                          "Camera already recognized");

        for (unsigned int n_component = 0;
                          n_component < 3;
                        ++n_component)
        {
            new_camera_internal_ptr->camera_rotation_hpb_ids   [n_component] = new_camera_rotations[n_component];
            new_camera_internal_ptr->camera_translation_xyz_ids[n_component] = new_camera_positions[n_component];
        } /* for (three components) */

        new_camera_internal_ptr->object_id        = camera_item_id;
        new_camera_internal_ptr->parent_object_id = item_info_ptr->parent(camera_item_id);
        new_camera_internal_ptr->pivot[0]         = (float)  pivot[0];
        new_camera_internal_ptr->pivot[1]         = (float)  pivot[1];
        new_camera_internal_ptr->pivot[2]         = (float) -pivot[2];

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
        uint32_t          n_camera_internals  = 0;

        system_hash64map_get_property(scene_camera_to_camera_internal_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_camera_internals);

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
PUBLIC system_event StartCameraDataExtraction(scene in_scene)
{
    job_done_event = system_event_create(false); /* manual_reset */

    /* Spawn a worker thread so that we can report the progress. */
    system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                     ExtractCameraDataWorkerThreadEntryPoint,
                                                                                                     in_scene);

    system_thread_pool_submit_single_task(task);

    return job_done_event;
}

/* Please see header for spec */
PUBLIC void GetCameraPropertyValue(scene_camera   camera,
                                   CameraProperty property,
                                   void*          out_result)
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

        case CAMERA_PROPERTY_PIVOT:
        {
            *(float**) out_result = camera_ptr->pivot;

            break;
        }

        case CAMERA_PROPERTY_ROTATION_HPB_CURVE_IDS:
        {
            *(curve_id**) out_result = camera_ptr->camera_rotation_hpb_ids;

            break;
        }

        case CAMERA_PROPERTY_TRANSLATION_CURVE_IDS:
        {
            *(curve_id**) out_result = camera_ptr->camera_translation_xyz_ids;

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
