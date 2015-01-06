
/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_file_serializer.h"
#include "system/system_variant.h"

/* Private declarations */
typedef struct
{
    /* NOTE: For serialization to work correctly, curve containers are NULL by default.
     *       scene_camera_get_property() correctly creates default curves if external
     *       caller tries to access any of the curve-based properties, but ALWAYS make
     *       sure a curve_container you need to use is available before accessing it! */
    float                     ar;
    curve_container           focal_distance;
    curve_container           f_stop;
    bool                      dirty;
    system_timeline_time      dirty_last_recalc_time; /* time, for which the last recalc was done */
    system_hashed_ansi_string name;
    _scene_camera_type        type;
    bool                      use_camera_physical_properties;
    curve_container           yfov;
    float                     zfar;
    float                     znear;
    system_timeline_time      zfar_znear_last_recalc_time;
    curve_container           zoom_factor;

    float far_plane_height;
    float far_plane_width;
    float near_plane_height;
    float near_plane_width;

    scene_graph_node owner_node;
    system_variant   temp_variant;

    REFCOUNT_INSERT_VARIABLES
} _scene_camera;

/* Forward declarations */
PRIVATE void _scene_camera_calculate_clipping_plane_data    (__in     __notnull _scene_camera*            camera_ptr,
                                                             __in               system_timeline_time      time);
PRIVATE void _scene_camera_calculate_zfar_znear             (__in     __notnull _scene_camera*            camera_ptr,
                                                             __in               system_timeline_time      time);
PRIVATE void _scene_camera_init                             (__in     __notnull _scene_camera*            camera_ptr,
                                                             __in     __notnull system_hashed_ansi_string name);
PRIVATE void _scene_camera_init_default_f_stop_curve        (__in     __notnull _scene_camera*            camera_ptr);
PRIVATE void _scene_camera_init_default_focal_distance_curve(__in     __notnull _scene_camera*            camera_ptr);
PRIVATE void _scene_camera_init_default_yfov_curve          (__in     __notnull _scene_camera*            camera_ptr);
PRIVATE void _scene_camera_init_default_zoom_factor_curve   (__in     __notnull _scene_camera*            camera_ptr);
PRIVATE bool _scene_camera_load_curve                       (__in_opt           scene                     owner_scene,
                                                             __in     __notnull curve_container*          curve_ptr,
                                                             __in     __notnull system_file_serializer    serializer);
PRIVATE void _scene_camera_release                          (                   void*                     data_ptr);
PRIVATE bool _scene_camera_save_curve                       (__in_opt           scene                     owner_scene,
                                                             __in     __notnull curve_container           in_curve,
                                                             __in     __notnull system_file_serializer    serializer);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_camera, scene_camera, _scene_camera);


/** TODO */
PRIVATE void _scene_camera_calculate_clipping_plane_data(__in __notnull _scene_camera*       camera_ptr,
                                                         __in           system_timeline_time time)
{
    float yfov_value;

    if (camera_ptr->yfov == NULL)
    {
        _scene_camera_init_default_yfov_curve(camera_ptr);
    }

    curve_container_get_value(camera_ptr->yfov,
                              time,
                              false,
                              camera_ptr->temp_variant);
    system_variant_get_float (camera_ptr->temp_variant,
                             &yfov_value);

    /* NOTE: Update get_property() if you start calcing more values here */
    camera_ptr->far_plane_height  = 2.0f * tan(yfov_value * 0.5f) * camera_ptr->zfar;
    camera_ptr->far_plane_width   = camera_ptr->far_plane_height  * camera_ptr->ar;
    camera_ptr->near_plane_height = 2.0f * tan(yfov_value * 0.5f) * camera_ptr->znear;
    camera_ptr->near_plane_width  = camera_ptr->near_plane_height * camera_ptr->ar;
}

/** TODO */
PRIVATE void _scene_camera_calculate_zfar_znear(__in __notnull _scene_camera*       camera_ptr,
                                                __in           system_timeline_time time)
{
    /* Make sure the curves are available */
    if (camera_ptr->f_stop == NULL)
    {
        _scene_camera_init_default_f_stop_curve(camera_ptr);
    }

    if (camera_ptr->focal_distance == NULL)
    {
        _scene_camera_init_default_focal_distance_curve(camera_ptr);
    }

    if (camera_ptr->zoom_factor == NULL)
    {
        _scene_camera_init_default_zoom_factor_curve(camera_ptr);
    }

    /* Carry on */
    const float coc = 0.05f;
    float       f_stop;
    float       focal_distance;
    float       zoom_factor;

    curve_container_get_value(camera_ptr->f_stop,
                              time,
                              false,
                              camera_ptr->temp_variant);
    system_variant_get_float (camera_ptr->temp_variant,
                             &f_stop);

    curve_container_get_value(camera_ptr->focal_distance,
                              time,
                              false,
                              camera_ptr->temp_variant);
    system_variant_get_float (camera_ptr->temp_variant,
                             &focal_distance);

    curve_container_get_value(camera_ptr->zoom_factor,
                              time,
                              false,
                              camera_ptr->temp_variant);
    system_variant_get_float (camera_ptr->temp_variant,
                             &zoom_factor);

    float d1           = f_stop * coc;
    float focal_length = zoom_factor / 1.33333f * 0.01f; /* assuming aperture height of 35 mm */
    float h            = focal_length * focal_length / d1;
    float d2           = h + focal_distance - focal_length;

    camera_ptr->znear = h * focal_distance / d2;

    float d = (2.0f * camera_ptr->znear - focal_distance);

    camera_ptr->zfar                        = focal_distance * camera_ptr->znear / d;
    camera_ptr->zfar_znear_last_recalc_time = time;
}

/** TODO */
PRIVATE void _scene_camera_init(__in __notnull _scene_camera*            camera_ptr,
                                __in __notnull system_hashed_ansi_string name)
{
    camera_ptr->ar                     = 0.0f;
    camera_ptr->dirty                  = true;
    camera_ptr->dirty_last_recalc_time = -1;
    camera_ptr->far_plane_height       = 0.0f;
    camera_ptr->far_plane_width        = 0.0f;
    camera_ptr->focal_distance         = NULL;
    camera_ptr->f_stop                 = NULL;
    camera_ptr->name                   = name;
    camera_ptr->near_plane_height      = 0.0f;
    camera_ptr->near_plane_width       = 0.0f;
    camera_ptr->owner_node             = NULL;
    camera_ptr->temp_variant           = system_variant_create(SYSTEM_VARIANT_FLOAT);
    camera_ptr->type                   = SCENE_CAMERA_TYPE_UNDEFINED;
    camera_ptr->yfov                   = NULL;
    camera_ptr->zfar                   = 0.0f;
    camera_ptr->znear                  = 0.0f;
    camera_ptr->zoom_factor            = NULL;

    camera_ptr->use_camera_physical_properties = false;
    camera_ptr->zfar_znear_last_recalc_time    = -1;
}

/** TODO */
PRIVATE void _scene_camera_init_default_f_stop_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->f_stop == NULL,
                      "Overwriting existing F-stop curve with the default one.");

    camera_ptr->f_stop = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                        " F-Stop"),
                                                SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE void _scene_camera_init_default_focal_distance_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->focal_distance == NULL,
                      "Overwriting existing focal distance curve with the default one.");

    camera_ptr->focal_distance = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                                " focal distance"),
                                                        SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE void _scene_camera_init_default_yfov_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->yfov == NULL,
                      "Overwriting existing YFov curve with the default one.");

    camera_ptr->yfov = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                      " YFov"),
                                              SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE void _scene_camera_init_default_zoom_factor_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->zoom_factor == NULL,
                      "Overwriting existing Zoom Factor curve with the default one.");

    camera_ptr->zoom_factor = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                             " Zoom Factor"),
                                                     SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE bool _scene_camera_load_curve(__in_opt           scene                  owner_scene,
                                      __in     __notnull curve_container*       curve_ptr,
                                      __in     __notnull system_file_serializer serializer)
{
    bool result = true;

    if (owner_scene != NULL)
    {
        scene_curve_id curve_id = 0;

        result &= system_file_serializer_read(serializer,
                                              sizeof(curve_id),
                                             &curve_id);

        if (result)
        {
            scene_curve scene_curve = scene_get_curve_by_id(owner_scene,
                                                            curve_id);

            scene_curve_get(scene_curve,
                            SCENE_CURVE_PROPERTY_INSTANCE,
                            curve_ptr);

            curve_container_retain(*curve_ptr);
        }
    }
    else
    {
        result &= system_file_serializer_read_curve_container(serializer,
                                                              curve_ptr);
    }

    return result;
}

/** TODO */
PRIVATE void _scene_camera_release(void* data_ptr)
{
    _scene_camera* camera_ptr = (_scene_camera*) data_ptr;

    if (camera_ptr->f_stop != NULL)
    {
        curve_container_release(camera_ptr->f_stop);

        camera_ptr->f_stop = NULL;
    }

    if (camera_ptr->focal_distance != NULL)
    {
        curve_container_release(camera_ptr->focal_distance);

        camera_ptr->focal_distance = NULL;
    }

    if (camera_ptr->temp_variant != NULL)
    {
        system_variant_release(camera_ptr->temp_variant);

        camera_ptr->temp_variant = NULL;
    }

    if (camera_ptr->yfov != NULL)
    {
        curve_container_release(camera_ptr->yfov);

        camera_ptr->yfov = NULL;
    }

    if (camera_ptr->zoom_factor != NULL)
    {
        curve_container_release(camera_ptr->zoom_factor);

        camera_ptr->zoom_factor = NULL;
    }
}

/** TODO */
PRIVATE bool _scene_camera_save_curve(__in_opt           scene                  owner_scene,
                                      __in     __notnull curve_container        in_curve,
                                      __in     __notnull system_file_serializer serializer)
{
    bool result = true;

    if (owner_scene != NULL)
    {
        scene_curve    curve    = scene_get_curve_by_container(owner_scene,
                                                               in_curve);
        scene_curve_id curve_id = 0;

        scene_curve_get(curve,
                        SCENE_CURVE_PROPERTY_ID,
                       &curve_id);

        result &= system_file_serializer_write(serializer,
                                               sizeof(curve_id),
                                              &curve_id);
    }
    else
    {
        result &= system_file_serializer_write_curve_container(serializer,
                                                               in_curve);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_camera scene_camera_create(__in __notnull system_hashed_ansi_string name)
{
    _scene_camera* new_scene_camera = new (std::nothrow) _scene_camera;

    ASSERT_DEBUG_SYNC(new_scene_camera != NULL, "Out of memory");
    if (new_scene_camera != NULL)
    {
        _scene_camera_init(new_scene_camera,
                           name);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_camera,
                                                       _scene_camera_release,
                                                       OBJECT_TYPE_SCENE_CAMERA,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Cameras\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_camera) new_scene_camera;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_camera_get_property(__in  __notnull scene_camera          camera,
                                                  __in            scene_camera_property property,
                                                  __in            system_timeline_time  time,
                                                  __out __notnull void*                 out_result)
{
    _scene_camera* camera_ptr             = (_scene_camera*) camera;
    const bool     recalc_value_requested = (property == SCENE_CAMERA_PROPERTY_FAR_PLANE_HEIGHT  ||
                                             property == SCENE_CAMERA_PROPERTY_FAR_PLANE_WIDTH   ||
                                             property == SCENE_CAMERA_PROPERTY_NEAR_PLANE_HEIGHT ||
                                             property == SCENE_CAMERA_PROPERTY_NEAR_PLANE_WIDTH);

    if (camera_ptr->dirty                                                    ||
        camera_ptr->dirty_last_recalc_time != time && recalc_value_requested)
    {
        _scene_camera_calculate_clipping_plane_data(camera_ptr,
                                                    time);

        camera_ptr->dirty                  = false;
        camera_ptr->dirty_last_recalc_time = time;
    }

    switch (property)
    {
        case SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE:
        {
            if (camera_ptr->use_camera_physical_properties)
            {
                if (camera_ptr->zfar_znear_last_recalc_time != time)
                {
                    _scene_camera_calculate_zfar_znear(camera_ptr,
                                                       time);
                }
            }

            *(float*) out_result = camera_ptr->zfar;

            break;
        }

        case SCENE_CAMERA_PROPERTY_FAR_PLANE_HEIGHT:
        {
            *(float*) out_result = camera_ptr->far_plane_height;

            break;
        }

        case SCENE_CAMERA_PROPERTY_FAR_PLANE_WIDTH:
        {
            *(float*) out_result = camera_ptr->far_plane_width;

            break;
        }

        case SCENE_CAMERA_PROPERTY_F_STOP:
        {
            if (camera_ptr->f_stop == NULL)
            {
                _scene_camera_init_default_f_stop_curve(camera_ptr);
            }

            *(curve_container*) out_result = camera_ptr->f_stop;

            break;
        }

        case SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE:
        {
            if (camera_ptr->focal_distance == NULL)
            {
                _scene_camera_init_default_focal_distance_curve(camera_ptr);
            }

            *(curve_container*) out_result = camera_ptr->focal_distance;

            break;
        }

        case SCENE_CAMERA_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = camera_ptr->name;

            break;
        }

        case SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE:
        {
            if (camera_ptr->use_camera_physical_properties)
            {
                if (camera_ptr->zfar_znear_last_recalc_time != time)
                {
                    _scene_camera_calculate_zfar_znear(camera_ptr,
                                                       time);
                }
            }

            *(float*) out_result = camera_ptr->znear;

            break;
        }

        case SCENE_CAMERA_PROPERTY_NEAR_PLANE_HEIGHT:
        {
            *(float*) out_result = camera_ptr->near_plane_height;

            break;
        }

        case SCENE_CAMERA_PROPERTY_NEAR_PLANE_WIDTH:
        {
            *(float*) out_result = camera_ptr->near_plane_width;

            break;
        }

        case SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE:
        {
            *(scene_graph_node*) out_result = camera_ptr->owner_node;

            break;
        }

        case SCENE_CAMERA_PROPERTY_USE_CAMERA_PHYSICAL_PROPERTIES:
        {
            *(bool*) out_result = camera_ptr->use_camera_physical_properties;

            break;
        }

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV:
        {
            if (camera_ptr->yfov == NULL)
            {
                _scene_camera_init_default_yfov_curve(camera_ptr);
            }

            *(curve_container*) out_result = camera_ptr->yfov;

            break;
        }

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR:
        {
            float zoom_factor = 0.0f;

            curve_container_get_value(camera_ptr->zoom_factor,
                                      time,
                                      false, /* should_force */
                                      camera_ptr->temp_variant);
            system_variant_get_float (camera_ptr->temp_variant,
                                     &zoom_factor);

            *(float*) out_result = 2.0f * atan2(1.0f,
                                                zoom_factor);

            break;
        }

        case SCENE_CAMERA_PROPERTY_ZOOM_FACTOR:
        {
            if (camera_ptr->zoom_factor == NULL)
            {
                _scene_camera_init_default_zoom_factor_curve(camera_ptr);
            }

            *(curve_container*) out_result = camera_ptr->zoom_factor;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_camera property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC scene_camera scene_camera_load(__in     __notnull system_file_serializer serializer,
                                      __in_opt           scene                  owner_scene)
{
    scene_camera   result     = NULL;
    _scene_camera* result_ptr = NULL;

    /* Retrieve camera name */
    system_hashed_ansi_string name = NULL;

    if (!system_file_serializer_read_hashed_ansi_string(serializer,
                                                       &name) )
    {
        goto end_error;
    }

    /* Create a new camera instance */
    result = scene_camera_create(name);

    ASSERT_DEBUG_SYNC(result != NULL, "Could not create a new scene_camera instance");
    if (result == NULL)
    {
        goto end_error;
    }

    result_ptr = (_scene_camera*) result;

    /* Release local curve containers before continuing */
    if (result_ptr->f_stop != NULL)
    {
        curve_container_release(result_ptr->f_stop);

        result_ptr->f_stop = NULL;
    }

    if (result_ptr->focal_distance != NULL)
    {
        curve_container_release(result_ptr->focal_distance);

        result_ptr->focal_distance = NULL;
    }

    if (result_ptr->yfov != NULL)
    {
        curve_container_release(result_ptr->yfov);

        result_ptr->yfov = NULL;
    }

    if (result_ptr->zoom_factor != NULL)
    {
        curve_container_release(result_ptr->zoom_factor);

        result_ptr->zoom_factor = NULL;
    }

    /* Retrieve other camera properties */
    float              camera_ar;
    curve_container    camera_f_stop         = NULL;
    curve_container    camera_focal_distance = NULL;
    _scene_camera_type camera_type;
    bool               camera_use_physical_properties = false;
    curve_container    camera_yfov                    = NULL;
    float              camera_zfar;
    float              camera_znear;
    curve_container    camera_zoom_factor    = NULL;

    if (!system_file_serializer_read (serializer,
                                      sizeof(camera_ar),
                                     &camera_ar)                            ||
        !_scene_camera_load_curve    (owner_scene,
                                    &camera_f_stop,
                                      serializer)                           ||
        !_scene_camera_load_curve    (owner_scene,
                                     &camera_focal_distance,
                                      serializer)                           ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_type),
                                     &camera_type)                          ||
        !_scene_camera_load_curve    (owner_scene,
                                     &camera_yfov,
                                      serializer)                           ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_zfar),
                                     &camera_zfar)                          ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_znear),
                                     &camera_znear)                         ||
        !_scene_camera_load_curve    (owner_scene,
                                     &camera_zoom_factor,
                                      serializer)                           ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_use_physical_properties),
                                      &camera_use_physical_properties) )
    {
        goto end_error;
    }

    /* Set the float properties */
    scene_camera_set_property(result,
                              SCENE_CAMERA_PROPERTY_ASPECT_RATIO,
                             &camera_ar);
    scene_camera_set_property(result,
                              SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                             &camera_zfar);
    scene_camera_set_property(result,
                              SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                             &camera_znear);
    scene_camera_set_property(result,
                              SCENE_CAMERA_PROPERTY_TYPE,
                             &camera_type);
    scene_camera_set_property(result,
                              SCENE_CAMERA_PROPERTY_USE_CAMERA_PHYSICAL_PROPERTIES,
                             &camera_use_physical_properties);

    result_ptr->f_stop         = camera_f_stop;
    result_ptr->focal_distance = camera_focal_distance;
    result_ptr->yfov           = camera_yfov;
    result_ptr->zoom_factor    = camera_zoom_factor;

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false, "Scene camera serialization failed.");

    if (result != NULL)
    {
        scene_camera_release(result);

        result = NULL;
    }

end:
    return result;
}

/* Please see header for specification */
PUBLIC bool scene_camera_save(__in __notnull system_file_serializer serializer,
                              __in __notnull const scene_camera     camera,
                              __in __notnull scene                  owner_scene)
{
    const _scene_camera* camera_ptr = (const _scene_camera*) camera;
    bool                 result     = false;

    result  = system_file_serializer_write_hashed_ansi_string(serializer,
                                                              camera_ptr->name);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->ar),
                                                             &camera_ptr->ar);
    result &= _scene_camera_save_curve                      (owner_scene,
                                                             camera_ptr->f_stop,
                                                             serializer);
    result &= _scene_camera_save_curve                      (owner_scene,
                                                             camera_ptr->focal_distance,
                                                             serializer);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->type),
                                                             &camera_ptr->type);
    result &= _scene_camera_save_curve                      (owner_scene,
                                                             camera_ptr->yfov,
                                                             serializer);
    result &= system_file_serializer_write                  (serializer,
                                                              sizeof(camera_ptr->zfar),
                                                             &camera_ptr->zfar);
    result &= system_file_serializer_write                  (serializer,
                                                              sizeof(camera_ptr->znear),
                                                             &camera_ptr->znear);
    result &= _scene_camera_save_curve                      (owner_scene,
                                                             camera_ptr->zoom_factor,
                                                             serializer);
    result &= system_file_serializer_write                  (serializer,
                                                             sizeof(camera_ptr->use_camera_physical_properties),
                                                            &camera_ptr->use_camera_physical_properties);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_camera_set_property(__in __notnull scene_camera          camera,
                                                  __in           scene_camera_property property,
                                                  __in __notnull const void*           data)
{
    _scene_camera* camera_ptr = (_scene_camera*) camera;

    switch (property)
    {
        case SCENE_CAMERA_PROPERTY_ASPECT_RATIO:
        {
            camera_ptr->ar = *(float*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE:
        {
            ASSERT_DEBUG_SYNC(!camera_ptr->use_camera_physical_properties,
                              "Replacing far plane setting while use-camera-physical-props mode is on makes no sense");

            camera_ptr->zfar = *(float*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_F_STOP:
        {
            /* TODO: This is bad and can/will backfire in gazillion ways in indeterministic
             *       future. Please implement a property copy constructor-like function for
             *       curve containers.
             */
            if (camera_ptr->f_stop != NULL)
            {
                curve_container_release(camera_ptr->f_stop);

                camera_ptr->f_stop = NULL;
            }

            camera_ptr->f_stop = *(curve_container*) data;

            if (camera_ptr->f_stop != NULL)
            {
                curve_container_retain(camera_ptr->f_stop);
            }

            break;
        }

        case SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE:
        {
            /* TODO: This is bad and can/will backfire in gazillion ways in indeterministic
             *       future. Please implement a property copy constructor-like function for
             *       curve containers.
             */
            if (camera_ptr->focal_distance != NULL)
            {
                curve_container_release(camera_ptr->focal_distance);

                camera_ptr->focal_distance = NULL;
            }

            camera_ptr->focal_distance = *(curve_container*) data;

            if (camera_ptr->focal_distance != NULL)
            {
                curve_container_retain(camera_ptr->focal_distance);
            }

            break;
        }

        case SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE:
        {
            ASSERT_DEBUG_SYNC(!camera_ptr->use_camera_physical_properties,
                              "Replacing near plane distance value while use-camera-physical-props mode is on makes no sense");

            camera_ptr->znear = *(float*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE:
        {
             /* This setter should only be called once during scene_camera lifetime */
            ASSERT_DEBUG_SYNC(camera_ptr->owner_node == NULL,
                              "Owner scene graph node already set for a scene_camera instance");

            camera_ptr->owner_node = *(scene_graph_node*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_TYPE:
        {
            camera_ptr->type = *(_scene_camera_type*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_USE_CAMERA_PHYSICAL_PROPERTIES:
        {
            camera_ptr->use_camera_physical_properties = *(bool*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV:
        {
            /* TODO: This is bad and can/will backfire in gazillion ways in indeterministic
             *       future. Please implement a property copy constructor-like function for
             *       curve containers.
             */
            if (camera_ptr->yfov != NULL)
            {
                curve_container_release(camera_ptr->yfov);

                camera_ptr->yfov = NULL;
            }

            camera_ptr->yfov = *(curve_container*) data;

            if (camera_ptr->yfov != NULL)
            {
                curve_container_retain(camera_ptr->yfov);
            }

            break;
        }

        case SCENE_CAMERA_PROPERTY_ZOOM_FACTOR:
        {
            /* TODO: This is bad and can/will backfire in gazillion ways in indeterministic
             *       future. Please implement a property copy constructor-like function for
             *       curve containers.
             */
            if (camera_ptr->zoom_factor != NULL)
            {
                curve_container_release(camera_ptr->zoom_factor);

                camera_ptr->zoom_factor = NULL;
            }

            camera_ptr->zoom_factor = *(curve_container*) data;

            if (camera_ptr->zoom_factor != NULL)
            {
                curve_container_retain(camera_ptr->zoom_factor);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_camera property");
        }
    } /* switch (property) */

    camera_ptr->dirty = true;
}
