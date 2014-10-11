/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_window.h"

#define DEFAULT_MOVEMENT_DELTA (0.005f)
#define DEFAULT_ROTATION_DELTA (0.02f)

/* Internal type definitions */
typedef enum
{
    KEY_LEFT_BIT  = 1,
    KEY_RIGHT_BIT = 2,
    KEY_UP_BIT    = 4,
    KEY_DOWN_BIT  = 8
} _key_bits;

typedef struct
{
    float movement_delta;
    float rotation_delta;

    float       pitch;
    float       position[4];
    float       yaw;

    float       forward[3];
    float       right  [3];
    float       up     [3];

    ogl_context             context;
    system_critical_section cs;

    bool           is_lbm_down;
    unsigned short lbm_xy      [2];
    float          lbm_pitch;
    float          lbm_yaw;

    _key_bits            key_bits;
    system_timeline_time key_down_time;

    system_matrix4x4 view_matrix;
} _ogl_flyby;

/* Internal variables */
/** Lock _cs before accessing */
system_hash64map _context_to_flyby = system_hash64map_create(sizeof(_ogl_flyby*) );

/** WARNING: TODO, WILL LEAK. but then who cares. ;) */
system_critical_section _cs = system_critical_section_create();

/** 3-dimensional cross product */
inline void _cross(float* a, float* b, float* result)
{
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

/** TODO */
PRIVATE bool _ogl_flyby_key_down_callback(system_window window, unsigned short key_char, void* arg)
{
    _ogl_flyby* descriptor = (_ogl_flyby*) arg;

    system_critical_section_enter(_cs);
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
    system_critical_section_leave(_cs);

    return true;
}

/** TODO */
PRIVATE bool _ogl_flyby_key_up_callback(system_window window, unsigned short key_char, void* arg)
{
    _ogl_flyby* descriptor = (_ogl_flyby*) arg;

    system_critical_section_enter(_cs);
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
                LOG_INFO("Camera pitch:    [%.4f]",             descriptor->pitch);
                LOG_INFO("Camera position: [%.4f, %.4f, %.4f]", descriptor->position[0], descriptor->position[1], descriptor->position[2]);
                LOG_INFO("Camera yaw:      [%.4f]",             descriptor->yaw);

                break;
            }
        }

        if (descriptor->key_bits == 0)
        {
            descriptor->key_down_time = 0;
        }
    }
    system_critical_section_leave(_cs);

    return true;
}

/* todo */
PRIVATE bool _ogl_flyby_lbd(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void* arg)
{
    _ogl_flyby* flyby = (_ogl_flyby*) arg;

    system_critical_section_enter(_cs);
    {
        flyby->is_lbm_down = true;

        flyby->lbm_xy[0]   = x;
        flyby->lbm_xy[1]   = y;

        flyby->lbm_pitch = flyby->pitch;
        flyby->lbm_yaw   = flyby->yaw;
    }
    system_critical_section_leave(_cs);

    return true;
}

/* todo */
PRIVATE bool _ogl_flyby_lbu(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void* arg)
{
    _ogl_flyby* flyby = (_ogl_flyby*) arg;

    system_critical_section_enter(_cs);
    {
        flyby->is_lbm_down = false;
    }
    system_critical_section_leave(_cs);

    return true;
}

/* todo */
PRIVATE bool _ogl_flyby_mouse_move(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void* arg)
{
    _ogl_flyby* flyby = (_ogl_flyby*) arg;

    system_critical_section_enter(_cs);
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
    system_critical_section_leave(_cs);

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_activate(__in __notnull ogl_context context, __in_ecount(3) __notnull const float* camera_pos)
{
    system_critical_section_enter(_cs);
    {
        if (!system_hash64map_contains(_context_to_flyby, (system_hash64) context) )
        {
            /* Instantiate new descriptor */
            _ogl_flyby* new_entry = new (std::nothrow) _ogl_flyby;

            ASSERT_DEBUG_SYNC(new_entry != NULL, "Cannot activate flyby - out of memory");
            if (new_entry != NULL)
            {
                new_entry->is_lbm_down    = false;
                new_entry->key_bits       = (_key_bits) 0;
                new_entry->movement_delta = DEFAULT_MOVEMENT_DELTA;
                new_entry->pitch          = 0;
                new_entry->position[0]    = camera_pos[0];
                new_entry->position[1]    = camera_pos[1];
                new_entry->position[2]    = camera_pos[2];
                new_entry->position[3]    = 0.0f;
                new_entry->rotation_delta = DEFAULT_ROTATION_DELTA;
                new_entry->view_matrix    = system_matrix4x4_create();
                new_entry->yaw            = 0;

                memset(new_entry->forward, 0, sizeof(float)*3);
                memset(new_entry->right,   0, sizeof(float)*3);
                memset(new_entry->up,      0, sizeof(float)*3);

                new_entry->up[2] = 1;

                /* Register for callbacks */
                system_window context_window = NULL;

                ogl_context_get_property(context,
                                         OGL_CONTEXT_PROPERTY_WINDOW,
                                        &context_window);

                system_window_add_callback_func(context_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,         _ogl_flyby_key_down_callback, new_entry);
                system_window_add_callback_func(context_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,           _ogl_flyby_key_up_callback,   new_entry);
                system_window_add_callback_func(context_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN, _ogl_flyby_lbd,               new_entry);
                system_window_add_callback_func(context_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,   _ogl_flyby_lbu,               new_entry);
                system_window_add_callback_func(context_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,       _ogl_flyby_mouse_move,        new_entry);

                /* Store the descriptor */
                system_hash64map_insert(_context_to_flyby, (system_hash64) context, new_entry, NULL, NULL);

                /* Issue a manual update - useful for pipeline object */
                ogl_flyby_update(context);
            }
        }
        else
        {
            LOG_ERROR("Cannot activate fly-by: already activated!");
        }
    }
    system_critical_section_leave(_cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_deactivate(__in __notnull ogl_context context)
{
    system_critical_section_enter(_cs);
    {
        if (system_hash64map_contains(_context_to_flyby, (system_hash64) context) )
        {
            /* Remove the descriptor */
            _ogl_flyby* existing_entry = NULL;

            system_hash64map_get(_context_to_flyby, (system_hash64) context, &existing_entry);

            ASSERT_DEBUG_SYNC(existing_entry != NULL, "Should never happen");
            if (existing_entry != NULL)
            {
                system_matrix4x4_release(existing_entry->view_matrix);
            }

            system_hash64map_remove(_context_to_flyby, (system_hash64) context);

            delete existing_entry;
            existing_entry = NULL;
        }
    }
    system_critical_section_leave(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API const float* ogl_flyby_get_camera_location(__in __notnull ogl_context context)
{
    const float* result = NULL;

    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            result = entry->position;
        }
    }
    system_critical_section_leave(_cs);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_get_up_forward_right_vectors(__in            __notnull ogl_context context,
                                                               __out_ecount(3) __notnull float*      up,
                                                               __out_ecount(3) __notnull float*      forward,
                                                               __out_ecount(3) __notnull float*      right)
{
    const float* result = NULL;

    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            memcpy(forward, entry->forward, sizeof(float) * 3);
            memcpy(right,   entry->right,   sizeof(float) * 3);
            memcpy(up,      entry->up,      sizeof(float) * 3);
        }
    }
    system_critical_section_leave(_cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_flyby_get_view_matrix(__in __notnull ogl_context      context,
                                                  __in __notnull system_matrix4x4 result)
{
    bool b_result = false;

    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            system_matrix4x4_set_from_matrix4x4(result, entry->view_matrix);

            b_result = true;
        }
    }
    system_critical_section_leave(_cs);

    return b_result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ogl_flyby_is_active(__in __notnull ogl_context context)
{
    return system_hash64map_contains(_context_to_flyby, (system_hash64) context);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_lock()
{
    system_critical_section_enter(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_set_movement_delta(__in __notnull ogl_context context, __in float new_value)
{
    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            entry->movement_delta = new_value;
        }
    }
    system_critical_section_leave(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_set_pitch_yaw(__in __notnull ogl_context context, __in float pitch, __in float yaw)
{
    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            entry->pitch = pitch;
            entry->yaw   = yaw;
        }
    }
    system_critical_section_leave(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_set_position(__in __notnull ogl_context context, __in float x, __in float y, __in float z)
{
    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            entry->position[0] = x;
            entry->position[1] = y;
            entry->position[2] = z;
        }
    }
    system_critical_section_leave(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_set_rotation_delta(__in __notnull ogl_context context, __in float new_value)
{
    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            entry->rotation_delta = new_value;
        }
    }
    system_critical_section_leave(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_unlock()
{
    system_critical_section_leave(_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_flyby_update(__in __notnull ogl_context context)
{
    system_critical_section_enter(_cs);
    {
        _ogl_flyby* entry = NULL;

        if (system_hash64map_get(_context_to_flyby, (system_hash64) context, &entry) )
        {
            /* Calculate current time */
            system_timeline_time time_now         = system_time_now();
            system_timeline_time time_delta       = time_now - entry->key_down_time;
            uint32_t             time_delta_msec  = 0;
            float                time_delta_s     = 0;
            float                ease_in_modifier = 1;

            system_time_get_msec_for_timeline_time(time_delta, &time_delta_msec);

            /* Smooth down the movement */
            ease_in_modifier = float(time_delta_msec) / 1000.0f * 3.0f;
            if (ease_in_modifier > 1)
            {
                ease_in_modifier = 1;
            }

            /* Calculate movement vectors */
            float forward    [3] = {sin(entry->yaw) * cos(entry->pitch), sin(entry->pitch), cos(entry->pitch) * cos(entry->yaw) };
            float up         [3] = {0, 1, 0};
            float right      [3] = {0};
            bool  redo_target    = false;

            _cross(forward, up, right);

            float l = sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
            right[0]/=l;
            right[1]/=l;
            right[2]/=l;

            memcpy(entry->forward, forward, sizeof(float) * 3);
            memcpy(entry->right,   right,   sizeof(float) * 3);
            memcpy(entry->up,      up,      sizeof(float) * 3);

            if (entry->key_bits & KEY_RIGHT_BIT)
            {
                entry->position[0] += ease_in_modifier * right[0] * entry->movement_delta;
                entry->position[1] += ease_in_modifier * right[1] * entry->movement_delta;
                entry->position[2] += ease_in_modifier * right[2] * entry->movement_delta;

                redo_target = true;
            }

            if (entry->key_bits & KEY_LEFT_BIT)
            {
                entry->position[0] -= ease_in_modifier * right[0] * entry->movement_delta;
                entry->position[1] -= ease_in_modifier * right[1] * entry->movement_delta;
                entry->position[2] -= ease_in_modifier * right[2] * entry->movement_delta;

                redo_target = true;
            }

            if (entry->key_bits & KEY_UP_BIT)
            {
                entry->position[0] += ease_in_modifier * forward[0] * entry->movement_delta;
                entry->position[1] += ease_in_modifier * forward[1] * entry->movement_delta;
                entry->position[2] += ease_in_modifier * forward[2] * entry->movement_delta;
            }
            
            if (entry->key_bits & KEY_DOWN_BIT)
            {
                entry->position[0] -= ease_in_modifier * forward[0] * entry->movement_delta;
                entry->position[1] -= ease_in_modifier * forward[1] * entry->movement_delta;
                entry->position[2] -= ease_in_modifier * forward[2] * entry->movement_delta;
            }


            /* Update view matrix */
            float target[3] = {entry->position[0] + forward[0], entry->position[1] + forward[1], entry->position[2] + forward[2]};

            system_matrix4x4_set_to_identity   (entry->view_matrix);
            system_matrix4x4_multiply_by_lookat(entry->view_matrix, entry->position, target, up);
        }
    }
    system_critical_section_leave(_cs);
}
