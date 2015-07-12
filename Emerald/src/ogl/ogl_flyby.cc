/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_flyby.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_window.h"

#define DEFAULT_FAKE_SCENE_CAMERA_Z_FAR  (100.0f)
#define DEFAULT_FAKE_SCENE_CAMERA_Z_NEAR (0.1f)
#define DEFAULT_MOVEMENT_DELTA           (0.005f)
#define DEFAULT_ROTATION_DELTA           (0.02f)

/* Internal type definitions */
typedef enum
{
    KEY_LEFT_BIT  = 1,
    KEY_RIGHT_BIT = 2,
    KEY_UP_BIT    = 4,
    KEY_DOWN_BIT  = 8
} _key_bits;

typedef struct _ogl_flyby
{
    system_critical_section cs;

    float movement_delta;
    float rotation_delta;

    float pitch;
    float position[4];
    float yaw;

    float forward[3];
    float right  [3];
    float up     [3];

    ogl_context      context; /* DO NOT retain/release - object is owned by the parent context. */
    scene_graph_node fake_graph_node;
    scene_camera     fake_scene_camera;

    bool           is_active;
    bool           is_lbm_down;
    unsigned short lbm_xy      [2];
    float          lbm_pitch;
    float          lbm_yaw;

    _key_bits   key_bits;
    system_time key_down_time;

    system_matrix4x4 view_matrix;

    REFCOUNT_INSERT_VARIABLES


    ~_ogl_flyby()
    {
        if (cs != NULL)
        {
            system_critical_section_release(cs);

            cs = NULL;
        }

        if (fake_graph_node != NULL)
        {
            scene_graph_node_release(fake_graph_node);

            fake_graph_node = NULL;
        }

        if (fake_scene_camera != NULL)
        {
            scene_camera_release(fake_scene_camera);

            fake_scene_camera = NULL;
        }
    }
} _ogl_flyby;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_flyby,
                               ogl_flyby,
                              _ogl_flyby)


/** TODO */
PRIVATE bool _ogl_flyby_key_down_callback(system_window  window,
                                          unsigned short key_char,
                                          void*          arg)
{
    _ogl_flyby* descriptor = (_ogl_flyby*) arg;

    system_critical_section_enter(descriptor->cs);
    {
        if (descriptor->key_bits == 0)
        {
            descriptor->key_down_time = system_time_now();
        }

        switch (key_char)
        {
            case 'D':
            case 'd':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) | KEY_RIGHT_BIT);

                break;
            }

            case 'A':
            case 'a':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) | KEY_LEFT_BIT);

                break;
            }

            case 'W':
            case 'w':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) | KEY_UP_BIT);

                break;
            }

            case 'S':
            case 's':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) | KEY_DOWN_BIT);

                break;
            }
        }
    }
    system_critical_section_leave(descriptor->cs);

    return true;
}

/** TODO */
PRIVATE bool _ogl_flyby_key_up_callback(system_window  window,
                                        unsigned short key_char,
                                        void*          arg)
{
    _ogl_flyby* descriptor = (_ogl_flyby*) arg;

    system_critical_section_enter(descriptor->cs);
    {
        switch (key_char)
        {
            case 'D':
            case 'd':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) & ~KEY_RIGHT_BIT);

                break;
            }

            case 'A':
            case 'a':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) & ~KEY_LEFT_BIT);

                break;
            }

            case 'W':
            case 'w':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) & ~KEY_UP_BIT);

                break;
            }

            case 'S':
            case 's':
            {
                descriptor->key_bits = (_key_bits) (((int) descriptor->key_bits) & ~KEY_DOWN_BIT);

                break;
            }

            case 'L':
            case 'l':
            {
                /* Log camera info */
                LOG_INFO("Camera pitch:    [%.4f]",
                         descriptor->pitch);
                LOG_INFO("Camera position: [%.4f, %.4f, %.4f]",
                         descriptor->position[0],
                         descriptor->position[1],
                         descriptor->position[2]);
                LOG_INFO("Camera yaw:      [%.4f]",
                         descriptor->yaw);

                break;
            }
        }

        if (descriptor->key_bits == 0)
        {
            descriptor->key_down_time = 0;
        }
    }
    system_critical_section_leave(descriptor->cs);

    return true;
}

/* todo */
PRIVATE bool _ogl_flyby_lbd(system_window           window,
                            unsigned short          x,
                            unsigned short          y,
                            system_window_vk_status new_status,
                            void*                   arg)
{
    _ogl_flyby* flyby = (_ogl_flyby*) arg;

    system_critical_section_enter(flyby->cs);
    {
        flyby->is_lbm_down = true;

        flyby->lbm_xy[0]   = x;
        flyby->lbm_xy[1]   = y;

        flyby->lbm_pitch = flyby->pitch;
        flyby->lbm_yaw   = flyby->yaw;
    }
    system_critical_section_leave(flyby->cs);

    return true;
}

/* todo */
PRIVATE bool _ogl_flyby_lbu(system_window           window,
                            unsigned short          x,
                            unsigned short          y,
                            system_window_vk_status new_status,
                            void*                   arg)
{
    _ogl_flyby* flyby = (_ogl_flyby*) arg;

    system_critical_section_enter(flyby->cs);
    {
        flyby->is_lbm_down = false;
    }
    system_critical_section_leave(flyby->cs);

    return true;
}

/* todo */
PRIVATE bool _ogl_flyby_mouse_move(system_window           window,
                                   unsigned short          x,
                                   unsigned short          y,
                                   system_window_vk_status new_status,
                                   void*                   arg)
{
    _ogl_flyby* flyby = (_ogl_flyby*) arg;

    system_critical_section_enter(flyby->cs);
    {
        if (flyby->is_lbm_down)
        {
            int   x_delta    = int(x) - int(flyby->lbm_xy[0]);
            int   y_delta    = int(y) - int(flyby->lbm_xy[1]);
            float x_delta_ss = float(x_delta) * 0.2f * flyby->rotation_delta;
            float y_delta_ss = float(y_delta) * 0.2f * flyby->rotation_delta;

            flyby->yaw   = flyby->lbm_yaw   - x_delta_ss;
            flyby->pitch = flyby->lbm_pitch - y_delta_ss;

            if (fabs(flyby->pitch) >= DEG_TO_RAD(89) )
            {
                flyby->pitch = (flyby->pitch < 0 ? -1 : 1) * DEG_TO_RAD(89);
            }
        }
    }
    system_critical_section_leave(flyby->cs);

    return true;
}

/** TODO */
PRIVATE void _ogl_flyby_release(void* arg)
{
    _ogl_flyby* flyby_ptr = (_ogl_flyby*) arg;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(arg != NULL,
                      "Input argument is NULL");

    /* Release the flyby fields. The destructor will be called by the reference counter impl. */
    if (flyby_ptr->view_matrix != NULL)
    {
        system_matrix4x4_release(flyby_ptr->view_matrix);

        flyby_ptr->view_matrix = NULL;
    }
}


/** Please see header for specification */
PUBLIC ogl_flyby ogl_flyby_create(ogl_context context)
{
    /* Instantiate new descriptor */
    _ogl_flyby* new_flyby_ptr = new (std::nothrow) _ogl_flyby;

    ASSERT_DEBUG_SYNC(new_flyby_ptr != NULL,
                      "Out of memory");

    if (new_flyby_ptr != NULL)
    {
        /* Form the flyby name */
        static int n_flyby = 0;
        char       temp_flyby_camera_name[128];
        char       temp_flyby_name       [128];

        snprintf(temp_flyby_camera_name,
                 sizeof(temp_flyby_camera_name),
                 "Flyby [%d] fake scene camera",
                 n_flyby++);
        snprintf(temp_flyby_name,
                 sizeof(temp_flyby_name),
                 "Flyby instance [%d]",
                 n_flyby++);

        /* Set up fields */
        new_flyby_ptr->context           = context;
        new_flyby_ptr->cs                = system_critical_section_create();
        new_flyby_ptr->fake_scene_camera = scene_camera_create           (system_hashed_ansi_string_create(temp_flyby_camera_name),
                                                                          NULL /* scene_name */);
        new_flyby_ptr->is_active         = false;
        new_flyby_ptr->is_lbm_down       = false;
        new_flyby_ptr->key_bits          = (_key_bits) 0;
        new_flyby_ptr->movement_delta    = DEFAULT_MOVEMENT_DELTA;
        new_flyby_ptr->pitch             = 0;
        new_flyby_ptr->position[0]       = 0.0f;
        new_flyby_ptr->position[1]       = 0.0f;
        new_flyby_ptr->position[2]       = 0.0f;
        new_flyby_ptr->position[3]       = 0.0f;
        new_flyby_ptr->rotation_delta    = DEFAULT_ROTATION_DELTA;
        new_flyby_ptr->view_matrix       = system_matrix4x4_create();
        new_flyby_ptr->yaw               = 0;

        memset(new_flyby_ptr->forward,
               0,
               sizeof(float)*3);
        memset(new_flyby_ptr->right,
               0,
               sizeof(float)*3);
        memset(new_flyby_ptr->up,
               0,
               sizeof(float)*3);

        new_flyby_ptr->up[2] = 1;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_flyby_ptr,
                                                       _ogl_flyby_release,
                                                       OBJECT_TYPE_OGL_FLYBY,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Fly-by instances\\",
                                                                                                               temp_flyby_name) );

        /* Set up fake scene camera graph node */
        system_matrix4x4 matrix_identity = system_matrix4x4_create();

        system_matrix4x4_set_to_identity(matrix_identity);

        new_flyby_ptr->fake_graph_node = scene_graph_create_static_matrix4x4_transformation_node(NULL, /* graph */
                                                                                                 matrix_identity,
                                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

        system_matrix4x4_release(matrix_identity);
        matrix_identity = NULL;

        /* Set up fake scene camera */
        const float default_z_far  = DEFAULT_FAKE_SCENE_CAMERA_Z_FAR;
        const float default_z_near = DEFAULT_FAKE_SCENE_CAMERA_Z_NEAR;

        scene_camera_set_property(new_flyby_ptr->fake_scene_camera,
                                  SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                 &default_z_far);
        scene_camera_set_property(new_flyby_ptr->fake_scene_camera,
                                  SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                                 &default_z_near);
        scene_camera_set_property(new_flyby_ptr->fake_scene_camera,
                                  SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                 &new_flyby_ptr->fake_graph_node);

        /* Register for callbacks */
        system_window context_window = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &context_window);

        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                        (void*) _ogl_flyby_key_down_callback,
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                        (void*) _ogl_flyby_key_up_callback,
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        (void*) _ogl_flyby_lbd,
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                        (void*) _ogl_flyby_lbu,
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                        (void*) _ogl_flyby_mouse_move,
                                        new_flyby_ptr);

        /* Issue a manual update - useful for pipeline object */
        ogl_flyby_update( (ogl_flyby) new_flyby_ptr);
    }

    return (ogl_flyby) new_flyby_ptr;
}


/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_get_property(ogl_flyby          flyby,
                                               ogl_flyby_property property,
                                               void*              out_result)
{
    _ogl_flyby* flyby_ptr = (_ogl_flyby*) flyby;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(flyby != NULL,
                      "Input flyby instance is NULL");

    /* Retrieve the result */
    system_critical_section_enter(flyby_ptr->cs);
    {
        switch (property)
        {
            case OGL_FLYBY_PROPERTY_CAMERA_LOCATION:
            {
                memcpy(out_result,
                       flyby_ptr->position,
                       sizeof(float) * 3);

                break;
            }

            case OGL_FLYBY_PROPERTY_FAKE_SCENE_CAMERA:
            {
                *(scene_camera*) out_result = flyby_ptr->fake_scene_camera;

                break;
            }

            case OGL_FLYBY_PROPERTY_IS_ACTIVE:
            {
                *(bool*) out_result = flyby_ptr->is_active;

                break;
            }

            case OGL_FLYBY_PROPERTY_MOVEMENT_DELTA:
            {
                *(float*) out_result = flyby_ptr->movement_delta;

                break;
            }

            case OGL_FLYBY_PROPERTY_PITCH:
            {
                *(float*) out_result = flyby_ptr->pitch;

                break;
            }

            case OGL_FLYBY_PROPERTY_ROTATION_DELTA:
            {
                *(float*) out_result = flyby_ptr->rotation_delta;

                break;
            }

            case OGL_FLYBY_PROPERTY_YAW:
            {
                *(float*) out_result = flyby_ptr->yaw;

                break;
            }

            case OGL_FLYBY_PROPERTY_VIEW_MATRIX:
            {
                system_matrix4x4_set_from_matrix4x4(*(system_matrix4x4*) out_result,
                                                    flyby_ptr->view_matrix);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ogl_flyby_property value");
            }
        } /* switch (property) */
    }
    system_critical_section_leave(flyby_ptr->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_lock(ogl_flyby flyby)
{
    system_critical_section_enter( ((_ogl_flyby*) flyby)->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_set_property(ogl_flyby          flyby,
                                               ogl_flyby_property property,
                                               const void*        data)
{
    _ogl_flyby* flyby_ptr = (_ogl_flyby*) flyby;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(flyby != NULL,
                      "Input flyby instance is NULL");

    /* Retrieve the result */
    system_critical_section_enter(flyby_ptr->cs);
    {
        switch (property)
        {
            case OGL_FLYBY_PROPERTY_CAMERA_LOCATION:
            {
                memcpy(flyby_ptr->position,
                       data,
                       sizeof(float) * 3);

                break;
            }

            case OGL_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_FAR:
            {
                scene_camera_set_property(flyby_ptr->fake_scene_camera,
                                          SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                          data);

                break;
            }

            case OGL_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_NEAR:
            {
                scene_camera_set_property(flyby_ptr->fake_scene_camera,
                                          SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                                          data);

                break;
            }

            case OGL_FLYBY_PROPERTY_IS_ACTIVE:
            {
                flyby_ptr->is_active = *(bool*) data;

                break;
            }

            case OGL_FLYBY_PROPERTY_MOVEMENT_DELTA:
            {
                flyby_ptr->movement_delta = *(float*) data;

                break;
            }

            case OGL_FLYBY_PROPERTY_PITCH:
            {
                flyby_ptr->pitch = *(float*) data;

                break;
            }

            case OGL_FLYBY_PROPERTY_ROTATION_DELTA:
            {
                flyby_ptr->rotation_delta = *(float *) data;

                break;
            }

            case OGL_FLYBY_PROPERTY_YAW:
            {
                flyby_ptr->yaw = *(float *) data;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ogl_flyby_property value");
            }
        } /* switch (property) */
    }
    system_critical_section_leave(flyby_ptr->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_unlock(ogl_flyby flyby)
{
    system_critical_section_leave( ((_ogl_flyby*) flyby)->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_update(ogl_flyby flyby)
{
    _ogl_flyby* flyby_ptr = (_ogl_flyby*) flyby;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(flyby != NULL,
                      "Input ogl_flyby instance is NULL");

    /* Calculate current time */
    system_critical_section_enter(flyby_ptr->cs);
    {
        system_time time_now         = system_time_now();
        system_time time_delta       = time_now - flyby_ptr->key_down_time;
        uint32_t    time_delta_msec  = 0;
        float       time_delta_s     = 0;
        float       ease_in_modifier = 1;

        system_time_get_msec_for_time(time_delta,
                                     &time_delta_msec);

        /* Smooth down the movement */
        ease_in_modifier = float(time_delta_msec) / 1000.0f * 3.0f;
        if (ease_in_modifier > 1)
        {
            ease_in_modifier = 1;
        }

        /* Calculate movement vectors */
        const float forward    [3] = {sin(flyby_ptr->yaw) * cos(flyby_ptr->pitch),
                                      sin(flyby_ptr->pitch),
                                      cos(flyby_ptr->pitch) * cos(flyby_ptr->yaw) };
        const float up         [3] = {0,
                                      1,
                                      0};
        float       right      [3] = {0};
        bool        redo_target    = false;

        system_math_vector_cross3    (forward,
                                      up,
                                      right);
        system_math_vector_normalize3(right,
                                      right);

        memcpy(flyby_ptr->forward,
               forward,
               sizeof(float) * 3);
        memcpy(flyby_ptr->right,
               right,
               sizeof(float) * 3);
        memcpy(flyby_ptr->up,
               up,
               sizeof(float) * 3);

        if (flyby_ptr->key_bits & KEY_RIGHT_BIT)
        {
            flyby_ptr->position[0] += ease_in_modifier * right[0] * flyby_ptr->movement_delta;
            flyby_ptr->position[1] += ease_in_modifier * right[1] * flyby_ptr->movement_delta;
            flyby_ptr->position[2] += ease_in_modifier * right[2] * flyby_ptr->movement_delta;

            redo_target = true;
        }

        if (flyby_ptr->key_bits & KEY_LEFT_BIT)
        {
            flyby_ptr->position[0] -= ease_in_modifier * right[0] * flyby_ptr->movement_delta;
            flyby_ptr->position[1] -= ease_in_modifier * right[1] * flyby_ptr->movement_delta;
            flyby_ptr->position[2] -= ease_in_modifier * right[2] * flyby_ptr->movement_delta;

            redo_target = true;
        }

        if (flyby_ptr->key_bits & KEY_UP_BIT)
        {
            flyby_ptr->position[0] += ease_in_modifier * forward[0] * flyby_ptr->movement_delta;
            flyby_ptr->position[1] += ease_in_modifier * forward[1] * flyby_ptr->movement_delta;
            flyby_ptr->position[2] += ease_in_modifier * forward[2] * flyby_ptr->movement_delta;
        }

        if (flyby_ptr->key_bits & KEY_DOWN_BIT)
        {
            flyby_ptr->position[0] -= ease_in_modifier * forward[0] * flyby_ptr->movement_delta;
            flyby_ptr->position[1] -= ease_in_modifier * forward[1] * flyby_ptr->movement_delta;
            flyby_ptr->position[2] -= ease_in_modifier * forward[2] * flyby_ptr->movement_delta;
        }


        /* Update view matrix */
        const float target[3] =
        {
            flyby_ptr->position[0] + forward[0],
            flyby_ptr->position[1] + forward[1],
            flyby_ptr->position[2] + forward[2]
        };

        system_matrix4x4_set_to_identity   (flyby_ptr->view_matrix);
        system_matrix4x4_multiply_by_lookat(flyby_ptr->view_matrix,
                                            flyby_ptr->position,
                                            target,
                                            up);

        /* Update fake scene camera */
        float                   camera_ar;
        system_matrix4x4        node_transformation_matrix = NULL;
        ogl_context_state_cache state_cache                = NULL;
        GLint                   viewport[4];

        ogl_context_get_property            (flyby_ptr->context,
                                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                            &state_cache);
        ogl_context_state_cache_get_property(state_cache,
                                             OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT,
                                             viewport);

        camera_ar = float(viewport[2]) / float(viewport[3]);

        scene_camera_set_property         (flyby_ptr->fake_scene_camera,
                                           SCENE_CAMERA_PROPERTY_ASPECT_RATIO,
                                          &camera_ar);

        scene_graph_node_get_property      (flyby_ptr->fake_graph_node,
                                            SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                           &node_transformation_matrix);
        system_matrix4x4_set_from_matrix4x4(node_transformation_matrix,
                                            flyby_ptr->view_matrix);
    }
    system_critical_section_leave(flyby_ptr->cs);
}
