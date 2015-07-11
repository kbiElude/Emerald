
/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_state_cache.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
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
    system_callback_manager   callback_manager;
    curve_container           focal_distance;
    curve_container           f_stop;
    system_hashed_ansi_string name;
    system_hashed_ansi_string object_manager_path;
    bool                      show_frustum;
    _scene_camera_type        type;
    bool                      use_camera_physical_properties;
    bool                      use_custom_vertical_fov;
    curve_container           yfov_custom;
    float                     zfar;
    float                     znear;
    system_time               zfar_znear_last_recalc_time;
    curve_container           zoom_factor;

    system_time frustum_last_recalc_time;
    float       frustum_far_bottom_left[3];
    float       frustum_far_bottom_right[3];
    float       frustum_far_top_left[3];
    float       frustum_far_top_right[3];
    float       frustum_near_bottom_left[3];
    float       frustum_near_bottom_right[3];
    float       frustum_near_top_left[3];
    float       frustum_near_top_right[3];
    float       frustum_centroid[3];

    scene_graph_node owner_node;
    system_variant   temp_variant;

    REFCOUNT_INSERT_VARIABLES
} _scene_camera;

/* Forward declarations */
PRIVATE void                      _scene_camera_calculate_frustum                (__in     __notnull _scene_camera*            camera_ptr,
                                                                                  __in               system_time               time);
PRIVATE void                      _scene_camera_calculate_zfar_znear             (__in     __notnull _scene_camera*            camera_ptr,
                                                                                  __in               system_time               time);
PRIVATE void                      _scene_camera_init                             (__in     __notnull _scene_camera*            camera_ptr,
                                                                                  __in     __notnull system_hashed_ansi_string name);
PRIVATE void                      _scene_camera_init_default_f_stop_curve        (__in     __notnull _scene_camera*            camera_ptr);
PRIVATE void                      _scene_camera_init_default_focal_distance_curve(__in     __notnull _scene_camera*            camera_ptr);
PRIVATE void                      _scene_camera_init_default_yfov_custom_curve   (__in     __notnull _scene_camera*            camera_ptr);
PRIVATE void                      _scene_camera_init_default_zoom_factor_curve   (__in     __notnull _scene_camera*            camera_ptr);
PRIVATE bool                      _scene_camera_load_curve                       (__in_opt           scene                     owner_scene,
                                                                                  __in_opt           system_hashed_ansi_string object_manager_path,
                                                                                  __in     __notnull curve_container*          curve_ptr,
                                                                                  __in     __notnull system_file_serializer    serializer);
PRIVATE void                      _scene_camera_release                          (                   void*                     data_ptr);
PRIVATE bool                      _scene_camera_save_curve                       (__in_opt           scene                     owner_scene,
                                                                                  __in     __notnull curve_container           in_curve,
                                                                                  __in     __notnull system_file_serializer    serializer);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_camera,
                               scene_camera,
                              _scene_camera);


/** TODO */
PRIVATE void _scene_camera_calculate_frustum(__in __notnull _scene_camera* camera_ptr,
                                             __in           system_time    time)
{
    float yfov_value;

    if (camera_ptr->use_custom_vertical_fov)
    {
        if (camera_ptr->yfov_custom == NULL)
        {
            _scene_camera_init_default_yfov_custom_curve(camera_ptr);
        }

        curve_container_get_value(camera_ptr->yfov_custom,
                                  time,
                                  false,
                                  camera_ptr->temp_variant);
        system_variant_get_float (camera_ptr->temp_variant,
                                 &yfov_value);
    }
    else
    {
        scene_camera_get_property( (scene_camera) camera_ptr,
                                   SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,
                                   time,
                                  &yfov_value);
    }

    /* Calculate far/near plane properties */
    ASSERT_DEBUG_SYNC(camera_ptr->ar != 0.0f,
                      "AR for the camera is 0");
    ASSERT_DEBUG_SYNC(yfov_value != 0.0f,
                      "YFov for the camera is 0");

    float far_plane_height  = 2.0f * tan(yfov_value * 0.5f) * camera_ptr->zfar;
    float far_plane_width   = far_plane_height              * camera_ptr->ar;
    float near_plane_height = 2.0f * tan(yfov_value * 0.5f) * camera_ptr->znear;
    float near_plane_width  = near_plane_height             * camera_ptr->ar;

    /* We need forward, right & up vectors for the camera. We will calculate those, using
     * the inversed camera matrix.
     */
    system_matrix4x4 owner_node_transformation_matrix = NULL;

    ASSERT_DEBUG_SYNC(camera_ptr->owner_node != NULL,
                      "No scene graph node assigned to a scene_camera instance!");

    scene_graph_node_get_property(camera_ptr->owner_node,
                                  SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                 &owner_node_transformation_matrix);

    /* Transform forward/right/up vectors by the inversed matrix */
    const float forward_vector[4] = {0.0f, 0.0f, -1.0f, 1.0f};
    const float right_vector  [4] = {1.0f, 0.0f,  0.0f, 1.0f};
    const float up_vector     [4] = {0.0f, 1.0f,  0.0f, 1.0f};
    const float zero_point    [4] = {0.0f, 0.0f,  0.0f, 1.0f};

    float transformed_forward    [4] = {0.0f};
    float transformed_forward_dir[4] = {0.0f};
    float transformed_right      [4] = {0.0f};
    float transformed_right_dir  [4] = {0.0f};
    float transformed_up         [4] = {0.0f};
    float transformed_up_dir     [4] = {0.0f};
    float transformed_zero       [4] = {0.0f};

    system_matrix4x4_multiply_by_vector4(owner_node_transformation_matrix,
                                         forward_vector,
                                         transformed_forward);
    system_matrix4x4_multiply_by_vector4(owner_node_transformation_matrix,
                                         right_vector,
                                         transformed_right);
    system_matrix4x4_multiply_by_vector4(owner_node_transformation_matrix,
                                         up_vector,
                                         transformed_up);
    system_matrix4x4_multiply_by_vector4(owner_node_transformation_matrix,
                                         zero_point,
                                         transformed_zero);

    ASSERT_DEBUG_SYNC(transformed_forward[3] != 0.0f &&
                      transformed_right  [3] != 0.0f &&
                      transformed_up     [3] != 0.0f &&
                      transformed_zero   [3] != 0.0f,
                      "Div by zero");

    system_math_vector_mul3_float(transformed_forward,
                                  1.0f / transformed_forward[3],
                                  transformed_forward);
    system_math_vector_mul3_float(transformed_right,
                                  1.0f / transformed_right[3],
                                  transformed_right);
    system_math_vector_mul3_float(transformed_up,
                                  1.0f / transformed_up[3],
                                  transformed_up);
    system_math_vector_mul3_float(transformed_zero,
                                  1.0f / transformed_zero[3],
                                  transformed_zero);

    /* Compute direction vectors */
    system_math_vector_minus3(transformed_forward,
                              transformed_zero,
                              transformed_forward_dir);
    system_math_vector_minus3(transformed_right,
                              transformed_zero,
                              transformed_right_dir);
    system_math_vector_minus3(transformed_up,
                              transformed_zero,
                              transformed_up_dir);

    system_math_vector_normalize3(transformed_forward_dir,
                                  transformed_forward_dir);
    system_math_vector_normalize3(transformed_right_dir,
                                  transformed_right_dir);
    system_math_vector_normalize3(transformed_up_dir,
                                  transformed_up_dir);

    /* Compute clipping points: (camera position + dir * far/near clipping plane distance) */
    float far_plane_clipping_point [3] = {0.0f};
    float near_plane_clipping_point[3] = {0.0f};

    system_math_vector_mul3_float(transformed_forward_dir,
                                  camera_ptr->zfar,
                                  far_plane_clipping_point);
    system_math_vector_mul3_float(transformed_forward_dir,
                                  camera_ptr->znear,
                                  near_plane_clipping_point);

    system_math_vector_add3(far_plane_clipping_point,
                            transformed_zero,
                            far_plane_clipping_point);
    system_math_vector_add3(near_plane_clipping_point,
                            transformed_zero,
                            near_plane_clipping_point);

    /* Proceed with frustum calculation: */
    float right_mul_far_plane_height_half [3] = {0.0f};
    float right_mul_near_plane_height_half[3] = {0.0f};
    float up_mul_far_plane_height_half    [3] = {0.0f};
    float up_mul_near_plane_height_half   [3] = {0.0f};

    system_math_vector_mul3_float(transformed_right_dir,
                                  far_plane_height * 0.5f,
                                  right_mul_far_plane_height_half);
    system_math_vector_mul3_float(transformed_right_dir,
                                  near_plane_height * 0.5f,
                                  right_mul_near_plane_height_half);
    system_math_vector_mul3_float(transformed_up_dir,
                                  far_plane_height * 0.5f,
                                  up_mul_far_plane_height_half);
    system_math_vector_mul3_float(transformed_up_dir,
                                  near_plane_height * 0.5f,
                                  up_mul_near_plane_height_half);

    /* a) far top-left corner */
    system_math_vector_add3  (far_plane_clipping_point,
                              up_mul_far_plane_height_half,
                              camera_ptr->frustum_far_top_left);
    system_math_vector_minus3(camera_ptr->frustum_far_top_left,
                              right_mul_far_plane_height_half,
                              camera_ptr->frustum_far_top_left);

    /* b) far top-right corner */
    system_math_vector_add3(far_plane_clipping_point,
                            up_mul_far_plane_height_half,
                            camera_ptr->frustum_far_top_right);
    system_math_vector_add3(camera_ptr->frustum_far_top_right,
                            right_mul_far_plane_height_half,
                            camera_ptr->frustum_far_top_right);

    /* c) far bottom-left corner */
    system_math_vector_minus3(far_plane_clipping_point,
                              up_mul_far_plane_height_half,
                              camera_ptr->frustum_far_bottom_left);
    system_math_vector_minus3(camera_ptr->frustum_far_bottom_left,
                              right_mul_far_plane_height_half,
                              camera_ptr->frustum_far_bottom_left);

    /* d) far bottom-right corner */
    system_math_vector_minus3(far_plane_clipping_point,
                              up_mul_far_plane_height_half,
                              camera_ptr->frustum_far_bottom_right);
    system_math_vector_add3  (camera_ptr->frustum_far_bottom_right,
                              right_mul_far_plane_height_half,
                              camera_ptr->frustum_far_bottom_right);

    /* e) near top-left corner */
    system_math_vector_add3  (near_plane_clipping_point,
                              up_mul_near_plane_height_half,
                              camera_ptr->frustum_near_top_left);
    system_math_vector_minus3(camera_ptr->frustum_near_top_left,
                              right_mul_near_plane_height_half,
                              camera_ptr->frustum_near_top_left);

    /* f) near top-right corner */
    system_math_vector_add3(near_plane_clipping_point,
                            up_mul_near_plane_height_half,
                            camera_ptr->frustum_near_top_right);
    system_math_vector_add3(camera_ptr->frustum_near_top_right,
                            right_mul_near_plane_height_half,
                            camera_ptr->frustum_near_top_right);

    /* g) near bottom-left corner */
    system_math_vector_minus3(near_plane_clipping_point,
                              up_mul_near_plane_height_half,
                              camera_ptr->frustum_near_bottom_left);
    system_math_vector_minus3(camera_ptr->frustum_near_bottom_left,
                              right_mul_near_plane_height_half,
                              camera_ptr->frustum_near_bottom_left);

    /* h) near bottom-right corner */
    system_math_vector_minus3(near_plane_clipping_point,
                              up_mul_near_plane_height_half,
                              camera_ptr->frustum_near_bottom_right);
    system_math_vector_add3  (camera_ptr->frustum_near_bottom_right,
                              right_mul_near_plane_height_half,
                              camera_ptr->frustum_near_bottom_right);

    /* Finally, compute the frustum centroid */
    system_math_vector_add3(camera_ptr->frustum_far_bottom_left,
                            camera_ptr->frustum_far_bottom_right,
                            camera_ptr->frustum_centroid);
    system_math_vector_add3(camera_ptr->frustum_centroid,
                            camera_ptr->frustum_far_top_left,
                            camera_ptr->frustum_centroid);
    system_math_vector_add3(camera_ptr->frustum_centroid,
                            camera_ptr->frustum_far_top_right,
                            camera_ptr->frustum_centroid);

    system_math_vector_add3(camera_ptr->frustum_centroid,
                            camera_ptr->frustum_near_bottom_left,
                            camera_ptr->frustum_centroid);
    system_math_vector_add3(camera_ptr->frustum_centroid,
                            camera_ptr->frustum_near_bottom_right,
                            camera_ptr->frustum_centroid);
    system_math_vector_add3(camera_ptr->frustum_centroid,
                            camera_ptr->frustum_near_top_left,
                            camera_ptr->frustum_centroid);
    system_math_vector_add3(camera_ptr->frustum_centroid,
                            camera_ptr->frustum_near_top_right,
                            camera_ptr->frustum_centroid);

    system_math_vector_mul3_float(camera_ptr->frustum_centroid,
                                  1.0f / 8.0f,
                                  camera_ptr->frustum_centroid);
}

/** TODO */
PRIVATE void _scene_camera_calculate_zfar_znear(__in __notnull _scene_camera* camera_ptr,
                                                __in           system_time    time)
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
PRIVATE void _scene_camera_init(__in     __notnull _scene_camera*            camera_ptr,
                                __in     __notnull system_hashed_ansi_string name,
                                __in_opt           system_hashed_ansi_string object_manager_path)
{
    camera_ptr->ar                       = 0.0f;
    camera_ptr->callback_manager         = system_callback_manager_create( (_callback_id) SCENE_CAMERA_CALLBACK_ID_COUNT);
    camera_ptr->focal_distance           = NULL;
    camera_ptr->f_stop                   = NULL;
    camera_ptr->frustum_last_recalc_time = -1;
    camera_ptr->name                     = name;
    camera_ptr->object_manager_path      = object_manager_path;
    camera_ptr->owner_node               = NULL;
    camera_ptr->show_frustum             = true;
    camera_ptr->temp_variant             = system_variant_create(SYSTEM_VARIANT_FLOAT);
    camera_ptr->type                     = SCENE_CAMERA_TYPE_UNDEFINED;
    camera_ptr->yfov_custom              = NULL;
    camera_ptr->zfar                     = 0.0f;
    camera_ptr->znear                    = 0.0f;
    camera_ptr->zoom_factor              = NULL;

    camera_ptr->use_camera_physical_properties = false;
    camera_ptr->use_custom_vertical_fov        = true;
    camera_ptr->zfar_znear_last_recalc_time    = -1;
}

/** TODO */
PRIVATE void _scene_camera_init_default_f_stop_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->f_stop == NULL,
                      "Overwriting existing F-stop curve with the default one.");

    camera_ptr->f_stop = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                        " F-Stop"),
                                                camera_ptr->object_manager_path,
                                                SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE void _scene_camera_init_default_focal_distance_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->focal_distance == NULL,
                      "Overwriting existing focal distance curve with the default one.");

    camera_ptr->focal_distance = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                                " focal distance"),
                                                        camera_ptr->object_manager_path,
                                                        SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE void _scene_camera_init_default_yfov_custom_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->yfov_custom == NULL,
                      "Overwriting existing YFov curve with the default one.");

    /* Use a default Yfov of 45 degrees */
    camera_ptr->yfov_custom = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                             " YFov"),
                                                     camera_ptr->object_manager_path,
                                                     SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (camera_ptr->temp_variant,
                                      DEG_TO_RAD(45.0f) );
    curve_container_set_default_value(camera_ptr->yfov_custom,
                                      camera_ptr->temp_variant);
}

/** TODO */
PRIVATE void _scene_camera_init_default_zoom_factor_curve(__in __notnull _scene_camera* camera_ptr)
{
    ASSERT_DEBUG_SYNC(camera_ptr->zoom_factor == NULL,
                      "Overwriting existing Zoom Factor curve with the default one.");

    camera_ptr->zoom_factor = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(camera_ptr->name),
                                                                                                             " Zoom Factor"),
                                                     camera_ptr->object_manager_path,
                                                     SYSTEM_VARIANT_FLOAT);
}

/** TODO */
PRIVATE bool _scene_camera_load_curve(__in_opt           scene                     owner_scene,
                                      __in_opt           system_hashed_ansi_string object_manager_path,
                                      __in     __notnull curve_container*          curve_ptr,
                                      __in     __notnull system_file_serializer    serializer)
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
                                                              object_manager_path,
                                                              curve_ptr);
    }

    return result;
}

/** TODO */
PRIVATE void _scene_camera_release(void* data_ptr)
{
    _scene_camera* camera_ptr = (_scene_camera*) data_ptr;

    if (camera_ptr->callback_manager != NULL)
    {
        system_callback_manager_release(camera_ptr->callback_manager);

        camera_ptr->callback_manager = NULL;
    }

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

    if (camera_ptr->yfov_custom != NULL)
    {
        curve_container_release(camera_ptr->yfov_custom);

        camera_ptr->yfov_custom = NULL;
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
PUBLIC EMERALD_API scene_camera scene_camera_create(__in     __notnull system_hashed_ansi_string name,
                                                    __in_opt           system_hashed_ansi_string object_manager_path)
{
    _scene_camera* new_scene_camera = new (std::nothrow) _scene_camera;

    ASSERT_DEBUG_SYNC(new_scene_camera != NULL,
                      "Out of memory");

    if (new_scene_camera != NULL)
    {
        /* Initialize the camera */
        _scene_camera_init(new_scene_camera,
                           name,
                           object_manager_path);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_camera,
                                                       _scene_camera_release,
                                                       OBJECT_TYPE_SCENE_CAMERA,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_SCENE_CAMERA,
                                                                       object_manager_path) );
    }

    return (scene_camera) new_scene_camera;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_camera_get_property(__in  __notnull scene_camera          camera,
                                                  __in            scene_camera_property property,
                                                  __in            system_time           time,
                                                  __out __notnull void*                 out_result)
{
    _scene_camera* camera_ptr             = (_scene_camera*) camera;
    const bool     recalc_value_requested = (property == SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT   ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT  ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT      ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT     ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT  ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT     ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT    ||
                                             property == SCENE_CAMERA_PROPERTY_FRUSTUM_CENTROID);

    if (recalc_value_requested &&
        camera_ptr->frustum_last_recalc_time != time)
    {
        /* NOTE: This mechanism does not account for curve value changes! */
        _scene_camera_calculate_frustum(camera_ptr,
                                        time);

        camera_ptr->frustum_last_recalc_time = time;
    }

    switch (property)
    {
        case SCENE_CAMERA_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result = camera_ptr->callback_manager;

            break;
        }

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

        case SCENE_CAMERA_PROPERTY_FRUSTUM_CENTROID:
        {
            memcpy(out_result,
                   camera_ptr->frustum_centroid,
                   sizeof(camera_ptr->frustum_centroid) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_far_bottom_left,
                   sizeof(camera_ptr->frustum_far_bottom_left) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_far_bottom_right,
                   sizeof(camera_ptr->frustum_far_bottom_right) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_far_top_left,
                   sizeof(camera_ptr->frustum_far_top_left) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_far_top_right,
                   sizeof(camera_ptr->frustum_far_top_right) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_near_bottom_left,
                   sizeof(camera_ptr->frustum_near_bottom_left) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_near_bottom_right,
                   sizeof(camera_ptr->frustum_near_bottom_right) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_near_top_left,
                   sizeof(camera_ptr->frustum_near_top_left) );

            break;
        }

        case SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT:
        {
            memcpy(out_result,
                   camera_ptr->frustum_near_top_right,
                   sizeof(camera_ptr->frustum_near_top_right) );

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

        case SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE:
        {
            *(scene_graph_node*) out_result = camera_ptr->owner_node;

            break;
        }

        case SCENE_CAMERA_PROPERTY_SHOW_FRUSTUM:
        {
            *(bool*) out_result = camera_ptr->show_frustum;

            break;
        }

        case SCENE_CAMERA_PROPERTY_USE_CAMERA_PHYSICAL_PROPERTIES:
        {
            *(bool*) out_result = camera_ptr->use_camera_physical_properties;

            break;
        }

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV_CUSTOM:
        {
            if (!camera_ptr->use_custom_vertical_fov)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "VERTICAL_FOV_CUSTOM query issued against a scene_camera, whose "
                                  "USE_CUSTOM_VERTICAL_FOV is false");
            }
            else
            {
                if (camera_ptr->yfov_custom == NULL)
                {
                    _scene_camera_init_default_yfov_custom_curve(camera_ptr);
                }

                *(curve_container*) out_result = camera_ptr->yfov_custom;
            }

            break;
        }

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR:
        {
            if (camera_ptr->use_custom_vertical_fov)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "VERTICAL_FOV_FROM_ZOOM_FACTOR query issued against a scene_camera"
                                  ", whose USE_CUSTOM_VERTICAL_FOV is true");
            }
            else
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
            }

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
PUBLIC scene_camera scene_camera_load(__in     __notnull ogl_context               context,
                                      __in     __notnull system_file_serializer    serializer,
                                      __in_opt           scene                     owner_scene,
                                      __in_opt           system_hashed_ansi_string object_manager_path)
{
    float              camera_ar;
    curve_container    camera_f_stop                  = NULL;
    curve_container    camera_focal_distance          = NULL;
    _scene_camera_type camera_type;
    bool               camera_use_custom_vertical_fov = false;
    bool               camera_use_physical_properties = false;
    curve_container    camera_yfov                    = NULL;
    float              camera_zfar;
    float              camera_znear;
    curve_container    camera_zoom_factor             = NULL;
    scene_camera       result                         = NULL;
    _scene_camera*     result_ptr                     = NULL;

    /* Retrieve camera name */
    system_hashed_ansi_string name = NULL;

    if (!system_file_serializer_read_hashed_ansi_string(serializer,
                                                       &name) )
    {
        goto end_error;
    }

    /* Create a new camera instance */
    result = scene_camera_create(name,
                                 object_manager_path);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not create a new scene_camera instance");

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

    if (result_ptr->yfov_custom != NULL)
    {
        curve_container_release(result_ptr->yfov_custom);

        result_ptr->yfov_custom = NULL;
    }

    if (result_ptr->zoom_factor != NULL)
    {
        curve_container_release(result_ptr->zoom_factor);

        result_ptr->zoom_factor = NULL;
    }

    /* Retrieve other camera properties */
    if (!system_file_serializer_read (serializer,
                                      sizeof(camera_ar),
                                     &camera_ar)                            ||
        !_scene_camera_load_curve    (owner_scene,
                                      object_manager_path,
                                    &camera_f_stop,
                                      serializer)                           ||
        !_scene_camera_load_curve    (owner_scene,
                                      object_manager_path,
                                     &camera_focal_distance,
                                      serializer)                           ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_type),
                                     &camera_type)                          ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_use_custom_vertical_fov),
                                     &camera_use_custom_vertical_fov)       ||
        !_scene_camera_load_curve    (owner_scene,
                                      object_manager_path,
                                     &camera_yfov,
                                      serializer)                           ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_zfar),
                                     &camera_zfar)                          ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_znear),
                                     &camera_znear)                         ||
        !_scene_camera_load_curve    (owner_scene,
                                      object_manager_path,
                                     &camera_zoom_factor,
                                      serializer)                           ||
        !system_file_serializer_read (serializer,
                                      sizeof(camera_use_physical_properties),
                                      &camera_use_physical_properties) )
    {
        goto end_error;
    }

    /* If camera AR is 0, try to retrieve current viewport's size.
     *
     * This will not work if we're not in a rendering context, in
     * which case throw an assertion failure.
     */
    if (fabs(camera_ar) < 1e-5f)
    {
        ogl_context_state_cache state_cache = NULL;

        ASSERT_DEBUG_SYNC(context != NULL,
                          "No active rendering context but camera AR is set at 0.0!");
        if (context != NULL)
        {
            float current_viewport_height;
            GLint current_viewport_size[4];
            float current_viewport_width;

            ogl_context_get_property            (context,
                                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                                &state_cache);
            ogl_context_state_cache_get_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT,
                                                 current_viewport_size);

            current_viewport_height = (float) current_viewport_size[3];
            current_viewport_width  = (float) current_viewport_size[2];

            ASSERT_DEBUG_SYNC(current_viewport_height > 0 &&
                              current_viewport_width  > 0,
                              "Invalid viewport's dimensions");

            camera_ar = current_viewport_width / current_viewport_height;
        } /* if (context != NULL) */
    } /* if (fabs(camera_ar) < 1e-5f) */

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
    scene_camera_set_property(result,
                              SCENE_CAMERA_PROPERTY_USE_CUSTOM_VERTICAL_FOV,
                             &camera_use_custom_vertical_fov);

    result_ptr->f_stop         = camera_f_stop;
    result_ptr->focal_distance = camera_focal_distance;
    result_ptr->yfov_custom    = camera_yfov;
    result_ptr->zoom_factor    = camera_zoom_factor;

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false,
                      "Scene camera serialization failed.");

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
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(camera_ptr->use_custom_vertical_fov),
                                                             &camera_ptr->use_custom_vertical_fov);
    result &= _scene_camera_save_curve                      (owner_scene,
                                                             camera_ptr->yfov_custom,
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

        case SCENE_CAMERA_PROPERTY_SHOW_FRUSTUM:
        {
            camera_ptr->show_frustum = *(bool*) data;

            system_callback_manager_call_back(camera_ptr->callback_manager,
                                              SCENE_CAMERA_CALLBACK_ID_SHOW_FRUSTUM_CHANGED,
                                              camera_ptr);

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

        case SCENE_CAMERA_PROPERTY_USE_CUSTOM_VERTICAL_FOV:
        {
            camera_ptr->use_custom_vertical_fov = *(bool*) data;

            break;
        }

        case SCENE_CAMERA_PROPERTY_VERTICAL_FOV_CUSTOM:
        {
            /* TODO: This is bad and can/will backfire in gazillion ways in indeterministic
             *       future. Please implement a property copy constructor-like function for
             *       curve containers.
             */
            if (camera_ptr->yfov_custom != NULL)
            {
                curve_container_release(camera_ptr->yfov_custom);

                camera_ptr->yfov_custom = NULL;
            }

            camera_ptr->yfov_custom = *(curve_container*) data;

            if (camera_ptr->yfov_custom != NULL)
            {
                curve_container_retain(camera_ptr->yfov_custom);
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
}
