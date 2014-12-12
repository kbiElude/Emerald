
/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_file_serializer.h"
#include "system/system_variant.h"

/* Private declarations */
typedef struct
{
    float                     ar;
    curve_container           focal_distance;
    curve_container           f_stop;
    bool                      dirty;
    system_hashed_ansi_string name;
    _scene_camera_type        type;
    float                     yfov;
    float                     zfar;
    float                     znear;

    float far_plane_height;
    float far_plane_width;
    float near_plane_height;
    float near_plane_width;

    scene_graph_node owner_node;

    REFCOUNT_INSERT_VARIABLES
} _scene_camera;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_camera, scene_camera, _scene_camera);


/** TODO */
PRIVATE void _scene_camera_calculate_clipping_plane_data(__in __notnull _scene_camera* camera_ptr)
{
    camera_ptr->far_plane_height  = 2.0f * tan(camera_ptr->yfov * 0.5f) * camera_ptr->zfar;
    camera_ptr->far_plane_width   = camera_ptr->far_plane_height * camera_ptr->ar;
    camera_ptr->near_plane_height = 2.0f * tan(camera_ptr->yfov * 0.5f) * camera_ptr->znear;
    camera_ptr->near_plane_width  = camera_ptr->near_plane_height * camera_ptr->ar;
}

/** TODO */
PRIVATE void _scene_camera_init(__in __notnull _scene_camera*            camera_ptr,
                                __in __notnull system_hashed_ansi_string name)
{
    camera_ptr->ar                = 0.0f;
    camera_ptr->dirty             = true;
    camera_ptr->far_plane_height  = 0.0f;
    camera_ptr->far_plane_width   = 0.0f;
    camera_ptr->focal_distance    = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " focal distance"),
                                                           SYSTEM_VARIANT_FLOAT);
    camera_ptr->f_stop            = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " F-Stop"),
                                                           SYSTEM_VARIANT_FLOAT);
    camera_ptr->name              = name;
    camera_ptr->near_plane_height = 0.0f;
    camera_ptr->near_plane_width  = 0.0f;
    camera_ptr->owner_node        = NULL;
    camera_ptr->type              = SCENE_CAMERA_TYPE_UNDEFINED;
    camera_ptr->yfov              = 0.0f;
    camera_ptr->zfar              = 0.0f;
    camera_ptr->znear             = 0.0f;

    scene_camera_retain( (scene_camera) camera_ptr);
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
                                                  __out __notnull void*                 out_result)
{
    _scene_camera* camera_ptr = (_scene_camera*) camera;

    if (camera_ptr->dirty)
    {
        _scene_camera_calculate_clipping_plane_data(camera_ptr);

        camera_ptr->dirty = false;
    }

    switch (property)
    {
        case SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE:
        {
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
            *(curve_container*) out_result = camera_ptr->f_stop;

            break;
        }

        case SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE:
        {
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

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV:
        {
            *(float*) out_result = camera_ptr->yfov;

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
PUBLIC scene_camera scene_camera_load(__in __notnull system_file_serializer serializer)
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
    }

    if (result_ptr->focal_distance != NULL)
    {
        curve_container_release(result_ptr->focal_distance);
    }

    /* Retrieve other camera properties */
    float              camera_ar;
    curve_container    camera_f_stop;
    curve_container    camera_focal_distance;
    _scene_camera_type camera_type;
    float              camera_yfov;
    float              camera_zfar;
    float              camera_znear;

    if (!system_file_serializer_read                (serializer,
                                                     sizeof(camera_ar),
                                                    &camera_ar)             ||
        !system_file_serializer_read_curve_container(serializer,
                                                    &camera_f_stop)         ||
        !system_file_serializer_read_curve_container(serializer,
                                                    &camera_focal_distance) ||
        !system_file_serializer_read                (serializer,
                                                     sizeof(camera_type),
                                                    &camera_type)           ||
        !system_file_serializer_read                (serializer,
                                                     sizeof(camera_yfov),
                                                    &camera_yfov)           ||
        !system_file_serializer_read                (serializer,
                                                     sizeof(camera_zfar),
                                                    &camera_zfar)           ||
        !system_file_serializer_read                (serializer,
                                                     sizeof(camera_znear),
                                                    &camera_znear) )
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
                              SCENE_CAMERA_PROPERTY_VERTICAL_FOV,
                             &camera_yfov);

    /* Set the curve_container properties */
    result_ptr->f_stop         = camera_f_stop;
    result_ptr->focal_distance = camera_focal_distance;

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
                              __in __notnull const scene_camera     camera)
{
    const _scene_camera* camera_ptr = (const _scene_camera*) camera;
    bool                 result     = false;

    result  = system_file_serializer_write_hashed_ansi_string(serializer,
                                                              camera_ptr->name);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->ar),
                                                             &camera_ptr->ar);
    result &= system_file_serializer_write_curve_container  (serializer,
                                                             camera_ptr->f_stop);
    result &= system_file_serializer_write_curve_container  (serializer,
                                                             camera_ptr->focal_distance);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->type),
                                                             &camera_ptr->type);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->yfov),
                                                             &camera_ptr->yfov);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->zfar),
                                                             &camera_ptr->zfar);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->znear),
                                                             &camera_ptr->znear);

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
            }

            camera_ptr->f_stop = *(curve_container*) data;
            curve_container_retain(camera_ptr->f_stop);

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
            }

            camera_ptr->focal_distance = *(curve_container*) data;
            curve_container_retain(camera_ptr->focal_distance);

            break;
        }

        case SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE:
        {
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

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV:
        {
            camera_ptr->yfov = *(float*) data;

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
