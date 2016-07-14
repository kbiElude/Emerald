/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "demo/demo_flyby.h"
#include "ral/ral_context.h"
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

typedef struct _demo_flyby
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

    ral_context      context; /* DO NOT retain/release */
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


    ~_demo_flyby()
    {
        if (cs != nullptr)
        {
            system_critical_section_release(cs);

            cs = nullptr;
        }

        if (fake_graph_node != nullptr)
        {
            scene_graph_node_release(fake_graph_node);

            fake_graph_node = nullptr;
        }

        if (fake_scene_camera != nullptr)
        {
            scene_camera_release(fake_scene_camera);

            fake_scene_camera = nullptr;
        }
    }
} _demo_flyby;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_flyby,
                               demo_flyby,
                              _demo_flyby)


/** TODO */
PRIVATE bool _demo_flyby_key_down_callback(system_window window,
                                           unsigned int  key_char,
                                           void*         arg)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(arg);

    system_critical_section_enter(flyby_ptr->cs);
    {
        if (flyby_ptr->key_bits == 0)
        {
            flyby_ptr->key_down_time = system_time_now();
        }

        switch (key_char)
        {
            case 'D':
            case 'd':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) | KEY_RIGHT_BIT);

                break;
            }

            case 'A':
            case 'a':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) | KEY_LEFT_BIT);

                break;
            }

            case 'W':
            case 'w':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) | KEY_UP_BIT);

                break;
            }

            case 'S':
            case 's':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) | KEY_DOWN_BIT);

                break;
            }
        }
    }
    system_critical_section_leave(flyby_ptr->cs);

    return true;
}

/** TODO */
PRIVATE bool _demo_flyby_key_up_callback(system_window window,
                                         unsigned int  key_char,
                                         void*         arg)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(arg);

    system_critical_section_enter(flyby_ptr->cs);
    {
        switch (key_char)
        {
            case 'D':
            case 'd':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) & ~KEY_RIGHT_BIT);

                break;
            }

            case 'A':
            case 'a':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) & ~KEY_LEFT_BIT);

                break;
            }

            case 'W':
            case 'w':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) & ~KEY_UP_BIT);

                break;
            }

            case 'S':
            case 's':
            {
                flyby_ptr->key_bits = (_key_bits) (((int) flyby_ptr->key_bits) & ~KEY_DOWN_BIT);

                break;
            }

            case 'L':
            case 'l':
            {
                /* Log camera info */
                LOG_INFO("Camera pitch:    [%.4f]",
                         flyby_ptr->pitch);
                LOG_INFO("Camera position: [%.4f, %.4f, %.4f]",
                         flyby_ptr->position[0],
                         flyby_ptr->position[1],
                         flyby_ptr->position[2]);
                LOG_INFO("Camera yaw:      [%.4f]",
                         flyby_ptr->yaw);

                break;
            }
        }

        if (flyby_ptr->key_bits == 0)
        {
            flyby_ptr->key_down_time = 0;
        }
    }
    system_critical_section_leave(flyby_ptr->cs);

    return true;
}

/* todo */
PRIVATE bool _demo_flyby_lbd(system_window           window,
                             unsigned short          x,
                             unsigned short          y,
                             system_window_vk_status new_status,
                             void*                   arg)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(arg);

    system_critical_section_enter(flyby_ptr->cs);
    {
        flyby_ptr->is_lbm_down = true;

        flyby_ptr->lbm_xy[0]   = x;
        flyby_ptr->lbm_xy[1]   = y;

        flyby_ptr->lbm_pitch = flyby_ptr->pitch;
        flyby_ptr->lbm_yaw   = flyby_ptr->yaw;
    }
    system_critical_section_leave(flyby_ptr->cs);

    return true;
}

/* todo */
PRIVATE bool _demo_flyby_lbu(system_window           window,
                             unsigned short          x,
                             unsigned short          y,
                             system_window_vk_status new_status,
                             void*                   arg)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(arg);

    system_critical_section_enter(flyby_ptr->cs);
    {
        flyby_ptr->is_lbm_down = false;
    }
    system_critical_section_leave(flyby_ptr->cs);

    return true;
}

/* todo */
PRIVATE bool _demo_flyby_mouse_move(system_window           window,
                                    unsigned short          x,
                                    unsigned short          y,
                                    system_window_vk_status new_status,
                                    void*                   arg)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(arg);

    system_critical_section_enter(flyby_ptr->cs);
    {
        if (flyby_ptr->is_lbm_down)
        {
            int   x_delta    = int(x) - int(flyby_ptr->lbm_xy[0]);
            int   y_delta    = int(y) - int(flyby_ptr->lbm_xy[1]);
            float x_delta_ss = float(x_delta) * 0.2f * flyby_ptr->rotation_delta;
            float y_delta_ss = float(y_delta) * 0.2f * flyby_ptr->rotation_delta;

            flyby_ptr->yaw   = flyby_ptr->lbm_yaw   - x_delta_ss;
            flyby_ptr->pitch = flyby_ptr->lbm_pitch - y_delta_ss;

            if (fabs(flyby_ptr->pitch) >= DEG_TO_RAD(89) )
            {
                flyby_ptr->pitch = (flyby_ptr->pitch < 0 ? -1 : 1) * DEG_TO_RAD(89);
            }
        }
    }
    system_critical_section_leave(flyby_ptr->cs);

    return true;
}

/** TODO */
PRIVATE void _demo_flyby_release(void* arg)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(arg);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(arg != nullptr,
                      "Input argument is NULL");

    /* Release the flyby fields. The destructor will be called by the reference counter impl. */
    if (flyby_ptr->view_matrix != nullptr)
    {
        system_matrix4x4_release(flyby_ptr->view_matrix);

        flyby_ptr->view_matrix = nullptr;
    }
}


/** Please see header for specification */
PUBLIC demo_flyby demo_flyby_create(ral_context context)
{
    /* Instantiate new descriptor */
    _demo_flyby* new_flyby_ptr = new (std::nothrow) _demo_flyby;

    ASSERT_DEBUG_SYNC(new_flyby_ptr != nullptr,
                      "Out of memory");

    if (new_flyby_ptr != nullptr)
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
                                                                          nullptr /* scene_name */);
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
                                                       _demo_flyby_release,
                                                       OBJECT_TYPE_DEMO_FLYBY,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Fly-by instances\\",
                                                                                                               temp_flyby_name) );

        /* Set up fake scene camera graph node */
        system_matrix4x4 matrix_identity = system_matrix4x4_create();

        system_matrix4x4_set_to_identity(matrix_identity);

        new_flyby_ptr->fake_graph_node = scene_graph_create_static_matrix4x4_transformation_node(nullptr, /* graph */
                                                                                                 matrix_identity,
                                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

        system_matrix4x4_release(matrix_identity);
        matrix_identity = nullptr;

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
        system_window context_window = nullptr;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                &context_window);

        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                        reinterpret_cast<void*>(_demo_flyby_key_down_callback),
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                        reinterpret_cast<void*>(_demo_flyby_key_up_callback),
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        reinterpret_cast<void*>(_demo_flyby_lbd),
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                        reinterpret_cast<void*>(_demo_flyby_lbu),
                                        new_flyby_ptr);
        system_window_add_callback_func(context_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                        reinterpret_cast<void*>(_demo_flyby_mouse_move),
                                        new_flyby_ptr);

        /* Issue a manual update - useful for pipeline object */
        demo_flyby_update(reinterpret_cast<demo_flyby>(new_flyby_ptr) );
    }

    return (demo_flyby) new_flyby_ptr;
}


/* Please see header for specification */
PUBLIC EMERALD_API void demo_flyby_get_property(const demo_flyby    flyby,
                                                demo_flyby_property property,
                                                void*               out_result_ptr)
{
    const _demo_flyby* flyby_ptr = reinterpret_cast<const _demo_flyby*>(flyby);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(flyby != nullptr,
                      "Input flyby instance is NULL");

    /* Retrieve the result */
    system_critical_section_enter(flyby_ptr->cs);
    {
        switch (property)
        {
            case DEMO_FLYBY_PROPERTY_CAMERA_LOCATION:
            {
                memcpy(out_result_ptr,
                       flyby_ptr->position,
                       sizeof(float) * 3);

                break;
            }

            case DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA:
            {
                *reinterpret_cast<scene_camera*>(out_result_ptr) = flyby_ptr->fake_scene_camera;

                break;
            }

            case DEMO_FLYBY_PROPERTY_IS_ACTIVE:
            {
                *reinterpret_cast<bool*>(out_result_ptr) = flyby_ptr->is_active;

                break;
            }

            case DEMO_FLYBY_PROPERTY_MOVEMENT_DELTA:
            {
                *reinterpret_cast<float*>(out_result_ptr) = flyby_ptr->movement_delta;

                break;
            }

            case DEMO_FLYBY_PROPERTY_PITCH:
            {
                *reinterpret_cast<float*>(out_result_ptr) = flyby_ptr->pitch;

                break;
            }

            case DEMO_FLYBY_PROPERTY_ROTATION_DELTA:
            {
                *reinterpret_cast<float*>(out_result_ptr) = flyby_ptr->rotation_delta;

                break;
            }

            case DEMO_FLYBY_PROPERTY_YAW:
            {
                *reinterpret_cast<float*>(out_result_ptr) = flyby_ptr->yaw;

                break;
            }

            case DEMO_FLYBY_PROPERTY_VIEW_MATRIX:
            {
                system_matrix4x4_set_from_matrix4x4(*reinterpret_cast<system_matrix4x4*>(out_result_ptr),
                                                    flyby_ptr->view_matrix);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ogl_flyby_property value");
            }
        }
    }
    system_critical_section_leave(flyby_ptr->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void demo_flyby_lock(demo_flyby flyby)
{
    system_critical_section_enter(reinterpret_cast<_demo_flyby*>(flyby)->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void demo_flyby_set_property(demo_flyby          flyby,
                                                demo_flyby_property property,
                                                const void*         data)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(flyby);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(flyby != nullptr,
                      "Input flyby instance is NULL");

    /* Retrieve the result */
    system_critical_section_enter(flyby_ptr->cs);
    {
        switch (property)
        {
            case DEMO_FLYBY_PROPERTY_CAMERA_LOCATION:
            {
                memcpy(flyby_ptr->position,
                       data,
                       sizeof(float) * 3);

                break;
            }

            case DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_FAR:
            {
                scene_camera_set_property(flyby_ptr->fake_scene_camera,
                                          SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                          data);

                break;
            }

            case DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_NEAR:
            {
                scene_camera_set_property(flyby_ptr->fake_scene_camera,
                                          SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                                          data);

                break;
            }

            case DEMO_FLYBY_PROPERTY_IS_ACTIVE:
            {
                flyby_ptr->is_active = *reinterpret_cast<const bool*>(data);

                break;
            }

            case DEMO_FLYBY_PROPERTY_MOVEMENT_DELTA:
            {
                flyby_ptr->movement_delta = *reinterpret_cast<const float*>(data);

                break;
            }

            case DEMO_FLYBY_PROPERTY_PITCH:
            {
                flyby_ptr->pitch = *reinterpret_cast<const float*>(data);

                break;
            }

            case DEMO_FLYBY_PROPERTY_ROTATION_DELTA:
            {
                flyby_ptr->rotation_delta = *reinterpret_cast<const float *>(data);

                break;
            }

            case DEMO_FLYBY_PROPERTY_YAW:
            {
                flyby_ptr->yaw = *reinterpret_cast<const float *>(data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized demo_flyby_property value");
            }
        }
    }
    system_critical_section_leave(flyby_ptr->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void demo_flyby_unlock(demo_flyby flyby)
{
    system_critical_section_leave( reinterpret_cast<_demo_flyby*>(flyby)->cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void demo_flyby_update(demo_flyby flyby)
{
    _demo_flyby* flyby_ptr = reinterpret_cast<_demo_flyby*>(flyby);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(flyby != nullptr,
                      "Input demo_flyby instance is NULL");

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

        /* Update the fake scene camera */
        float            camera_ar;
        system_matrix4x4 node_transformation_matrix = nullptr;
        system_window    window                     = nullptr;
        uint32_t         viewport[2];

        ral_context_get_property  (flyby_ptr->context,
                                   RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                  &window);
        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   viewport);

        camera_ar = float(viewport[1]) / float(viewport[0]);

        scene_camera_set_property(flyby_ptr->fake_scene_camera,
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
