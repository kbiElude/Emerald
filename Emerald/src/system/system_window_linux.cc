/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_linux.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include "system/system_window_linux.h"
#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#error WORK IN PROGRESS - DOES NOT COMPILE


typedef struct _system_window_linux
{
    /* "action forbidden" cursor resource */
    Cursor action_forbidden_cursor_resource;
    /* arrow cursor resource */
    Cursor arrow_cursor_resource;
    /* cross-hair cursor resource */
    Cursor crosshair_cursor_resource;
    /* hand cursor resource */
    Cursor hand_cursor_resource;
    /* "horizontal resize" cursor resource */
    Cursor horizontal_resize_cursor_resource;
    /* move cursor resource */
    Cursor move_cursor_resource;
    /* "vertical resize" cursor resource */
    Cursor vertical_resize_cursor_resource;

    Cursor current_mouse_cursor_system_resource;

    /* Properties of a display connection which hosts the rendering handler */
    int      default_screen_index;
    Window   desktop_window;
    Display* display;

    Colormap             colormap;
    system_window_handle system_handle;
    system_window        window; /* DO NOT retain */

    bool                 is_message_pump_active; /* set to false and send an event to the window to kill the message pump thread */
    bool                 is_message_pump_locked;
    system_event         message_pump_lock_event;
    system_thread_id     message_pump_thread_id;
    system_event         message_pump_unlock_event;
    system_event         teardown_completed_event;

    int cursor_position[2]; /* only updated prior to call-back execution! */


    explicit _system_window_linux(__in system_window in_window)
    {
        action_forbidden_cursor_resource     = 0;
        arrow_cursor_resource                = 0;
        colormap                             = (Colormap) NULL;
        crosshair_cursor_resource            = 0;
        current_mouse_cursor_system_resource = (Cursor) NULL;
        default_screen_index                 = -1;
        desktop_window                       = (Window) NULL;
        display                              = NULL;
        hand_cursor_resource                 = 0;
        horizontal_resize_cursor_resource    = 0;
        is_message_pump_active               = true;
        is_message_pump_locked               = false;
        message_pump_lock_event              = system_event_create(true); /* manual_reset */
        message_pump_thread_id               = 0;
        message_pump_unlock_event            = system_event_create(true); /* manual_reset */
        move_cursor_resource                 = 0;
        system_handle                        = (system_window_handle) NULL;
        teardown_completed_event             = system_event_create(true); /* manual_reset */
        window                               = in_window;
        vertical_resize_cursor_resource      = 0;
    }

    ~_system_window_linux()
    {
        ASSERT_DEBUG_SYNC(!is_message_pump_locked,
                          "System window about to be deinited while message pump is locked!");

        /* Release cursor */
        if (action_forbidden_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        action_forbidden_cursor_resource);

            action_forbidden_cursor_resource = (Cursor) NULL;
        }

        if (arrow_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        arrow_cursor_resource);

            arrow_cursor_resource = (Cursor) NULL;
        }

        if (crosshair_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        crosshair_cursor_resource);

            crosshair_cursor_resource = (Cursor) NULL;
        }

        if (hand_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        hand_cursor_resource);

            hand_cursor_resource = (Cursor) NULL;
        }

        if (horizontal_resize_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        horizontal_resize_cursor_resource);

            horizontal_resize_cursor_resource = (Cursor) NULL;
        }

        if (move_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        move_cursor_resource);

            move_cursor_resource = (Cursor) NULL;
        }

        if (vertical_resize_cursor_resource != (Cursor) NULL)
        {
            XFreeCursor(display,
                        vertical_resize_cursor_resource);

            vertical_resize_cursor_resource = (Cursor) NULL;
        }

        /* Release other stuff */
        if (colormap != (Colormap) NULL)
        {
            XFreeColormap(display,
                          colormap);

            colormap = (Colormap) NULL;
        }

        if (message_pump_lock_event != NULL)
        {
            system_event_release(message_pump_lock_event);

            message_pump_lock_event = NULL;
        }

        if (message_pump_unlock_event != NULL)
        {
            system_event_release(message_pump_unlock_event);

            message_pump_unlock_event = NULL;
        }

        if (system_handle != (system_window_handle) NULL)
        {
            XDestroyWindow(display,
                           system_handle);

            system_handle = (system_window_handle) NULL;
        }

        if (teardown_completed_event != NULL)
        {
            system_event_release(teardown_completed_event);

            teardown_completed_event = NULL;
        }
    }
} _system_window_linux;

/* Main display which is used to listen for window events.
 *
 * You might have noticed that each system_window_linux instance also holds a display.
 * Each such display is used exclusively for buffer swapping. This is required,
 * since we have a separate rendering thread and a separate thread for watching for
 * the window events. If we used a single display, the rendering pump's thread would have acquired
 * the display lock in order to wait for incoming events, causing the buffer swap op to
 * starve until window event arrives.
 */
Display* main_display = NULL;


/* Forward declarations */
PRIVATE          void _system_window_linux_window_closing_rendering_thread_entrypoint(               ogl_context                          context,
                                                                                                     void*                                user_arg);
PRIVATE volatile void _system_window_linux_teardown_thread_pool_callback             (__in __notnull system_thread_pool_callback_argument arg);


#if 0
/** TODO */
PRIVATE VOID _lock_system_window_message_pump(_system_window_win32* win32_ptr)
{
    /* Only lock if not in message pump AND:
     * 1. the window has not been marked for release, AND
     * 2. the window is in the process of being closed (the window_closing call-back)
     */
    bool is_closed  = false;
    bool is_closing = false;

    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSED,
                              &is_closed);
    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSING,
                              &is_closing);

    if ( system_threads_get_thread_id () != win32_ptr->message_pump_thread_id &&
        !is_closed                                                            &&
        !is_closing)
    {
        /* Only lock if the window is still alive! */
        system_event_reset(win32_ptr->message_pump_lock_event);
        system_event_reset(win32_ptr->message_pump_unlock_event);
        {
            ::PostMessageA(win32_ptr->system_handle,
                           WM_EMERALD_LOCK_MESSAGE_PUMP,
                           0,
                           0);

            system_event_wait_single(win32_ptr->message_pump_lock_event);
        }
    }
}

/** TODO */
PRIVATE VOID _unlock_system_window_message_pump(_system_window_win32* win32_ptr)
{
    /* Only unlock if not in message pump */
    bool is_window_closed  = false;
    bool is_window_closing = false;

    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSED,
                              &is_window_closed);
    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSING,
                              &is_window_closing);

    if ( system_threads_get_thread_id() != win32_ptr->message_pump_thread_id &&
        !is_window_closed                                                    &&
        !is_window_closing)

    {
        ASSERT_DEBUG_SYNC(win32_ptr->is_message_pump_locked,
                          "Cannot unlock system window message pump - not locked.");

        system_event_set(win32_ptr->message_pump_unlock_event);

        /* Wait until message pump confirms it has finished handling WM_EMERALD_LOCK_MESSAGE_PUMP msg */
        while (system_event_wait_single_peek(win32_ptr->message_pump_unlock_event) ){}
    }
}
#endif

/** TODO */
PRIVATE void _system_window_linux_handle_event(_system_window_linux* linux_ptr,
                                               const XEvent*         event_ptr)
{
    switch (event_ptr->type)
    {
        case ButtonPress:
        case ButtonRelease:
        {
            system_window_callback_func callback_func;
            bool                        should_proceed = true;

            switch (event_ptr->xbutton.button)
            {
                case 1:
                {
                    callback_func = (event_ptr->type == ButtonPress) ? SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN
                                                                     : SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP;

                    break;
                }

                case 2:
                {
                    callback_func = (event_ptr->type == ButtonPress) ? SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN
                                                                     : SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP;

                    break;
                }

                case 3:
                {
                    callback_func = (event_ptr->type == ButtonPress) ? SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN
                                                                     : SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP;

                    break;
                }

                default:
                {
                    /* Unsupported mouse button */
                    should_proceed = false;

                    break;
                }
            } /* switch (event_ptr->xbutton.button) */

            if (should_proceed)
            {
                system_window_execute_callback_funcs(linux_ptr->window,
                                                     callback_func,
                                                    (void*) 0);
            } /* if (should_proceed) */

            break;
        } /* ButtonPress & ButtonRelease cases */

        case ClientMessage:
        {
            event_ptr->xclient.message_type 
            // ...

            break;
        }

        case DestroyNotify:
        {
            if (event_ptr->xdestroywindow.window == linux_ptr->system_handle)
            {
                /* If the window is being closed per system request (eg. ALT+F4 was pressed), we need to stop
                 * the rendering process first! Otherwise we're very likely to end up with a nasty crash. */
                ogl_context           context           = NULL;
                ogl_rendering_handler rendering_handler = NULL;

                system_window_get_property(linux_ptr->window,
                                           SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                                          &context);
                system_window_get_property(linux_ptr->window,
                                           SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                          &rendering_handler);

                if (rendering_handler != NULL)
                {
                    ogl_rendering_handler_playback_status playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;

                    ogl_rendering_handler_get_property(rendering_handler,
                                                       OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,
                                                      &playback_status);

                    if (playback_status != RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED)
                    {
                        ogl_rendering_handler_stop(rendering_handler);
                    } /* if (playback_status != RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED) */

                    /* For the "window closing" call-back, we need to work from the rendering thread, as the caller
                     * may need to release GL stuff.
                     *
                     * This should work out of the box, since we've just stopped the play-back.
                     */
                    ogl_context_request_callback_from_context_thread(context,
                                                                     _system_window_linux_window_closing_rendering_thread_entrypoint,
                                                                     linux_ptr);
                }

                /* OK, safe to destroy the system window at this point */
                XDestroyWindow(_display,
                               linux_ptr->system_handle);

                /* Now here's the trick: the call-backs can only be fired AFTER the window thread has shut down.
                 * This lets us avoid various thread racing conditions which used to break all hell loose at the
                 * tear-down time in the past, and which also let the window messages flow, which is necessary
                 * for the closure to finish correctly.
                 *
                 * The entry-point we're pointing the task at does exactly that.
                 */
                system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_CRITICAL,
                                                                                                                _system_window_linux_teardown_thread_pool_callback,
                                                                                                                (void*) linux_ptr);

                system_thread_pool_submit_single_task(task);
            } /* if (event_ptr->xdestroywindow.window == linux_ptr->system_handle) */

            break;
        } /* case DestroyNotify: */

        case Expose:
        {
            /* Need to re-render the frame */
            ogl_rendering_handler rendering_handler = NULL;

            system_window_get_property(linux_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                      &rendering_handler);

            ASSERT_DEBUG_SYNC(rendering_handler != NULL,
                              "Window rendering handler is NULL");

            if (rendering_handler != NULL)
            {
                system_timeline_time last_frame_time = 0;

                ogl_rendering_handler_get_property(rendering_handler,
                                                   OGL_RENDERING_HANDLER_PROPERTY_LAST_FRAME_TIME,
                                                  &last_frame_time);

                ogl_rendering_handler_play(rendering_handler,
                                           last_frame_time);
            } /* if (rendering_handler != NULL) */

            break;
        }

        case KeyPress:
        {
            system_window_execute_callback_funcs(linux_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                                 (void*) (event_ptr->xkey.keycode & 0xFF) );

            break;
        }

        case KeyRelease:
        {
            system_window_execute_callback_funcs(linux_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                                 (void*) (event_ptr->xkey.keycode & 0xFF) );

            break;
        }
    } /* switch (event_ptr->type) */

#if 0
    switch (message_id)
    {
        case WM_EMERALD_LOCK_MESSAGE_PUMP:
        {
            win32_ptr->is_message_pump_locked = true;

            system_event_set        (win32_ptr->message_pump_lock_event);
            system_event_wait_single(win32_ptr->message_pump_unlock_event);

            win32_ptr->is_message_pump_locked = false;

            system_event_reset(win32_ptr->message_pump_unlock_event);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                                 (void*) wparam);

            if (win32_ptr->current_mouse_cursor_system_resource != NULL)
            {
                ::SetCursor(win32_ptr->current_mouse_cursor_system_resource);
            }

            break;
        }

        case WM_MOUSEWHEEL:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL,
                                                 (void*) GET_WHEEL_DELTA_WPARAM(wparam),
                                                 (void*) LOWORD(wparam) );

            return 0;
        }
    }
    #endif
}

#if 0
/** TODO */
PRIVATE void _system_window_window_closing_rendering_thread_entrypoint(ogl_context context,
                                                                       void*       user_arg)
{
    bool                  is_closing  = false;
    _system_window_win32* window_ptr  = (_system_window_win32*) user_arg;

    system_window_get_property(window_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSING,
                              &is_closing);

    ASSERT_DEBUG_SYNC(!is_closing,
                      "Sanity check failed");

    is_closing = true;
    system_window_set_property(window_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSING,
                              &is_closing);

    system_window_execute_callback_funcs(window_ptr->window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING);

    is_closing = false;
    system_window_set_property(window_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_CLOSING,
                              &is_closing);
}
#endif

/** TODO */
PRIVATE volatile void _system_window_linux_teardown_thread_pool_callback(__in __notnull system_thread_pool_callback_argument arg)
{
    _system_window_linux* window_ptr = (_system_window_win32*) arg;

    /* Call back the subscribers, if any */
    system_window_execute_callback_funcs(window_ptr->window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED);

    system_event_set(window_ptr->teardown_completed_event);
}


/** Please see header for spec */
PUBLIC void system_window_linux_close_window(__in system_window_linux window)
{
    _system_window_linux* linux_ptr = (_system_window_linux*) window;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");
    ASSERT_DEBUG_SYNC(linux_ptr->system_handle != NULL,
                      "No window to close - system handle is NULL.");

#if 0
    if (win32_ptr->system_handle != NULL)
    {
        ::SendMessageA(win32_ptr->system_handle,
                       WM_CLOSE,
                       0,  /* wParam */
                       0); /* lParam */
    }
#endif
}

/** Please see header for spec */
PUBLIC void system_window_linux_deinit(__in system_window_linux window)
{
    _system_window_linux* linux_ptr = (_system_window_linux*) window;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");

    delete linux_ptr;
    linux_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void system_window_linux_deinit_global()
{
    if (_display != NULL)
    {
        /* We don't really care whether this call fails or not */
        XCloseDisplay(_display);

        _display = NULL;
    }

    _desktop_window       = (Window) NULL;
    _default_screen_index = -1;
}

/** Please see header for spec */
PUBLIC bool system_window_linux_get_property(__in  system_window_linux    window,
                                             __in  system_window_property property,
                                             __out void*                  out_result)
{
    _system_window_linux* linux_ptr = (_system_window_linux*) window;
    bool                  result    = true;

    /* Only handle platform-specific queries */
    switch (property)
    {
        case SYSTEM_WINDOW_PROPERTY_CURSOR_POSITION:
        {
            int*         result_ptr = (int*) out_result;
            Window       temp_child_window;
            unsigned int temp_mask;
            Window       temp_root_window;
            int          temp_root_x;
            int          temp_root_y;

            XQueryPointer(_display,
                          linux_ptr->system_handle,
                         &temp_root_window,
                         &temp_child_window,
                         &temp_root_x,
                         &temp_root_y,
                          result_ptr + 0,
                          result_ptr + 1,
                         &temp_mask);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DISPLAY:
        {
            *(Display**) out_result = _display;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_HANDLE:
        {
            ASSERT_DEBUG_SYNC(linux_ptr->system_handle != 0,
                              "System handle is NULL.");

            *(system_window_handle*) out_result = linux_ptr->system_handle;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_POSITION:
        {
            ASSERT_DEBUG_SYNC(linux_ptr->system_handle != 0,
                              "System handle is NULL.");

            if (linux_ptr->system_handle != 0)
            {
                XWindowAttributes window_attributes;

                XGetWindowAttributes(_display,
                                     linux_ptr->system_handle,
                                     &window_attributes);

                ((int*) out_result)[0] = window_attributes.x;
                ((int*) out_result)[1] = window_attributes.y;
            }

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_SCREEN_INDEX:
        {
            *(int*) out_result = _default_screen_index;

            break;
        }

        default:
        {
            /* Fall-back! */
            result = false;
        }
    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC void system_window_linux_get_screen_size(__out int* out_screen_width_ptr,
                                                __out int* out_screen_height_ptr)
{
    Screen* display_screen = NULL;

    ASSERT_DEBUG_SYNC(_display != NULL,
                      "No active display");
    ASSERT_DEBUG_SYNC(out_screen_width_ptr  != NULL &&
                      out_screen_height_ptr != NULL,
                      "Input arguments are NULL");

    display_screen = DefaultScreenOfDisplay(_display);

    ASSERT_DEBUG_SYNC(display_screen != NULL,
                      "No default screen reported for the display");

    *out_screen_width_ptr  = display_screen->width;
    *out_screen_height_ptr = display_screen->height;
}

/** Please see header for spec */
PUBLIC void system_window_linux_handle_window(__in system_window_linux window)
{
    bool                  is_visible = false;
    _system_window_linux* linux_ptr  = (_system_window_linux*) window;

    LOG_INFO("system_window_linux_handle_window() starts..");

    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_VISIBLE,
                              &is_visible);

    if (is_visible)
    {
        /* Show the window */
        XMapWindow(_display,
                   linux_ptr->system_handle);
    }

    /* Cache the message pump thread ID */
    linux_ptr->message_pump_thread_id = system_threads_get_thread_id();

    /* Pump messages */
    while (linux_ptr->is_message_pump_active)
    {
        XEvent current_event;

        /* Block until X event arrives */
        XNextEvent(_display,
                  &current_event);

        /* Handle the event */
        _system_window_linux_handle_event(linux_ptr,
                                         &current_event);
    }

    LOG_INFO("system_window_linux_handle_window() waiting to die gracefully..");

    system_event_wait_single(linux_ptr->teardown_completed_event);

    LOG_INFO("system_window_linux_handle_window() completed.");
}

/** Please see header for spec */
PUBLIC system_window_linux system_window_linux_init(__in __notnull system_window owner)
{
    _system_window_linux* linux_ptr = new (std::nothrow) _system_window_linux(owner);

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Out of memory");

    /* The descriptor is fully initialized in the constructor. */

    return (system_window_linux) linux_ptr;
}

/** Please see header for spec */
PUBLIC void system_window_linux_init_global()
{
    XInitThreads();

    _display = XOpenDisplay(":0");

    if (_display == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not open display at :0");

        goto end;
    }

    _default_screen_index = DefaultScreen(_display);
    _desktop_window       = RootWindow   (_display,
                                          _default_screen_index);

end:
    ;
}

/** Please see header for spec */
PUBLIC bool system_window_linux_open_window(__in system_window_linux window,
                                            __in bool                is_first_window)
{
    bool                      is_window_fullscreen = false;
    bool                      is_window_scalable   = false;
    _system_window_linux*     linux_ptr            = (_system_window_linux*) window;
    uint32_t                  n_depth_bits         = 0;
    uint32_t                  n_rgba_bits[4]       = {0};
    system_window_handle      parent_window_handle = (system_window_handle) NULL;
    bool                      result               = true;
    XSetWindowAttributes      winattr;
    ogl_context_type          window_context_type  = OGL_CONTEXT_TYPE_UNDEFINED;
    system_window_handle      window_handle        = (system_window_handle) NULL;
    system_pixel_format       window_pixel_format  = NULL;
    system_hashed_ansi_string window_title         = NULL;
    int                       x1y1x2y2[4];
    int                       visual_attribute_list [2 * 5 /* rgba bits, depth bits */ + 2 /* doublebuffering, rgba */ + 1 /* terminator */];
    XVisualInfo*              visual_info_ptr      = NULL;

    ASSERT_DEBUG_SYNC(_display != NULL,
                      "Display is NULL");

    /* Cache mouse cursors */
    linux_ptr->action_forbidden_cursor_resource  = XCreateFontCursor(_display,
                                                                     XC_X_cursor);
    linux_ptr->arrow_cursor_resource             = XCreateFontCursor(_display,
                                                                     XC_left_ptr);
    linux_ptr->crosshair_cursor_resource         = XCreateFontCursor(_display,
                                                                     XC_crosshair);
    linux_ptr->hand_cursor_resource              = XCreateFontCursor(_display,
                                                                     XC_hand1);
    linux_ptr->horizontal_resize_cursor_resource = XCreateFontCursor(_display,
                                                                     XC_sb_h_double_arrow);
    linux_ptr->move_cursor_resource              = XCreateFontCursor(_display,
                                                                     XC_right_ptr);
    linux_ptr->vertical_resize_cursor_resource   = XCreateFontCursor(_display,
                                                                     XC_sb_v_double_arrow);

    ASSERT_DEBUG_SYNC(linux_ptr->action_forbidden_cursor_resource != NULL,
                      "Failed to load action forbidden cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->arrow_cursor_resource != NULL,
                      "Failed to load arrow cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->crosshair_cursor_resource != NULL,
                      "Failed to load crosshair cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->hand_cursor_resource != NULL,
                      "Failed to load hand cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->horizontal_resize_cursor_resource != NULL,
                      "Failed to load horizontal resize cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->move_cursor_resource != NULL,
                      "Failed to load move cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->vertical_resize_cursor_resource != NULL,
                      "Failed to load vertical resize cursor resource.");

    /* If full-screen window is requested, throw an assertion. Linux builds do not support it yet.
     * Same goes for scalable windows.
     */
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_FULLSCREEN,
                              &is_window_fullscreen);
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_SCALABLE,
                              &is_window_scalable);
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,
                              &parent_window_handle);
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &window_pixel_format);
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_TITLE,
                              &window_title);
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                               x1y1x2y2);

    ASSERT_DEBUG_SYNC(is_window_scalable,
                      "TODO: Scalable windows are not supported yet");
    ASSERT_DEBUG_SYNC(is_window_fullscreen,
                      "TODO: Full-screen windows are not supported yet");

    /* Determine parent window handle */
    ASSERT_DEBUG_SYNC(parent_window_handle == NULL,
                      "TODO: system_window_linux does not support window embedding.");

    parent_window_handle = RootWindow(_display,
                                      _default_screen_index);

    /* Retrieve XVisualInfo instance, compatible with the requested rendering context */
    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_CONTEXT_TYPE,
                              &window_context_type);

    switch (window_context_type)
    {
        case OGL_CONTEXT_TYPE_ES:
        case OGL_CONTEXT_TYPE_GL:
        {
            visual_info_ptr = ogl_context_linux_get_visual_info_for_gl_window(linux_ptr->window);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unsupported rendering context type");
        }
    } /* switch (window_context_type) */

    ASSERT_DEBUG_SYNC(visual_info_ptr != NULL,
                      "No visual that would be compatible with the requested context properties was found.");

    /* Spawn the window */
    XSetWindowAttributes window_attributes;

    linux_ptr->colormap = XCreateColormap(_display,
                                          parent_window_handle,
                                          visual_info_ptr->visual,
                                          AllocNone);

    window_attributes.background_pixel = 0; /* black */
    window_attributes.border_pixel     = 0; /* black */
    window_attributes.colormap         = linux_ptr->colormap;
    window_attributes.event_mask       = ButtonReleaseMask | ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask;

    linux_ptr->system_handle = XCreateWindow(_display,
                                             parent_window_handle,
                                             x1y1x2y2[0],
                                             x1y1x2y2[1],
                                             x1y1x2y2[2] - x1y1x2y2[0],
                                             x1y1x2y2[3] - x1y1x2y2[1],
                                             0, /* border_width */
                                             visual_info_ptr->depth,
                                             InputOutput,
                                             visual_info_ptr->visual,
                                             CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
                                            &window_attributes);

    if (linux_ptr->system_handle == (system_window_handle) NULL)
    {
        LOG_FATAL("Could not create window [%s]",
                  system_hashed_ansi_string_get_buffer(window_title) );

        ASSERT_DEBUG_SYNC(linux_ptr->system_handle != (Window) NULL,
                          "Could not create a window");

        result = false;
        goto end;
    }

    /* Update the window title */
    XStoreName(_display,
               linux_ptr->system_handle,
               system_hashed_ansi_string_get_buffer(window_title) );

end:
    if (visual_info_ptr != NULL)
    {
        XFree(visual_info_ptr);

        visual_info_ptr = NULL;
    }

    return result;
}

/** Please see header for property */
PUBLIC bool system_window_linux_set_property(__in system_window_linux    window,
                                             __in system_window_property property,
                                             __in const void*            data)
{
    _system_window_linux* linux_ptr = (_system_window_linux*) window;
    bool                  result    = true;

    switch (property)
    {
        case SYSTEM_WINDOW_PROPERTY_CURSOR:
        {
            system_window_mouse_cursor cursor = *(system_window_mouse_cursor*) data;

            switch (cursor)
            {
                case SYSTEM_WINDOW_MOUSE_CURSOR_ACTION_FORBIDDEN:
                {
                    linux_ptr->current_mouse_cursor_system_resource = linux_ptr->action_forbidden_cursor_resource;

                    break;
                }

                case SYSTEM_WINDOW_MOUSE_CURSOR_ARROW:
                {
                    linux_ptr->current_mouse_cursor_system_resource = linux_ptr->arrow_cursor_resource;

                    break;
                }

                case SYSTEM_WINDOW_MOUSE_CURSOR_CROSSHAIR:
                {
                    linux_ptr->current_mouse_cursor_system_resource = linux_ptr->crosshair_cursor_resource;

                    break;
                }

                case SYSTEM_WINDOW_MOUSE_CURSOR_HORIZONTAL_RESIZE:
                {
                    linux_ptr->current_mouse_cursor_system_resource = linux_ptr->horizontal_resize_cursor_resource;

                    break;
                }

                case SYSTEM_WINDOW_MOUSE_CURSOR_MOVE:
                {
                    linux_ptr->current_mouse_cursor_system_resource = linux_ptr->move_cursor_resource;

                    break;
                }

                case SYSTEM_WINDOW_MOUSE_CURSOR_VERTICAL_RESIZE:
                {
                    linux_ptr->current_mouse_cursor_system_resource = linux_ptr->vertical_resize_cursor_resource;

                    break;
                }

                default:
                {
                    LOG_ERROR("Unrecognized cursor type [%d] requested", cursor);

                    result = false;
                }
            } /* switch (cursor) */

            if (result)
            {
                XDefineCursor(_display,
                              linux_ptr->system_handle,
                              linux_ptr->current_mouse_cursor_system_resource);
            }

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DIMENSIONS:
        {
            const int* width_height_ptr = (const int*) data;

            XResizeWindow(_display,
                          linux_ptr->system_handle,
                          width_height_ptr[0],
                          width_height_ptr[1]);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_POSITION:
        {
            system_window_handle parent_window_handle = (system_window_handle) NULL;
            const int*           xy_ptr               = (const int*) data;

            system_window_get_property(linux_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,
                                      &parent_window_handle);

            XMoveWindow(_display,
                        linux_ptr->system_handle,
                        xy_ptr[0],
                        xy_ptr[1]);

            break;
        }

        default:
        {
            result = false;
        }
    } /* switch (property) */

    return result;
}
