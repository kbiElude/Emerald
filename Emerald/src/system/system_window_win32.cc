/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include "system/system_window_win32.h"


/** TODO */
#define WM_EMERALD_LOCK_MESSAGE_PUMP (WM_USER + 1)


typedef struct _system_window_win32
{
    /* "action forbidden" cursor resource */
    HCURSOR action_forbidden_cursor_resource;
    /* arrow cursor resource */
    HCURSOR arrow_cursor_resource;
    /* cross-hair cursor resource */
    HCURSOR crosshair_cursor_resource;
    /* hand cursor resource */
    HCURSOR hand_cursor_resource;
    /* "horizontal resize" cursor resource */
    HCURSOR horizontal_resize_cursor_resource;
    /* move cursor resource */
    HCURSOR move_cursor_resource;
    /* "vertical resize" cursor resource */
    HCURSOR vertical_resize_cursor_resource;

    HCURSOR current_mouse_cursor_system_resource;

    system_window_dc            system_dc;
    system_window_handle        system_handle;
    system_window               window; /* DO NOT retain */

    bool                 is_message_pump_locked;
    system_event         message_pump_lock_event;
    system_thread_id     message_pump_thread_id;
    system_event         message_pump_unlock_event;
    system_event         teardown_completed_event;

    POINT cursor_position; /* only updated prior to call-back execution! */


    explicit _system_window_win32(system_window in_window)
    {
        action_forbidden_cursor_resource     = 0;
        arrow_cursor_resource                = 0;
        crosshair_cursor_resource            = 0;
        current_mouse_cursor_system_resource = NULL;
        hand_cursor_resource                 = 0;
        horizontal_resize_cursor_resource    = 0;
        is_message_pump_locked               = false;
        message_pump_lock_event              = system_event_create(true); /* manual_reset */
        message_pump_thread_id               = 0;
        message_pump_unlock_event            = system_event_create(true); /* manual_reset */
        move_cursor_resource                 = 0;
        system_dc                            = NULL;
        system_handle                        = NULL;
        teardown_completed_event             = system_event_create(true); /* manual_reset */
        window                               = in_window;
        vertical_resize_cursor_resource      = 0;
    }

    ~_system_window_win32()
    {
        ASSERT_DEBUG_SYNC(!is_message_pump_locked,
                          "System window about to be deinited while message pump is locked!");

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

        if (system_dc != NULL)
        {
            /* TODO? */
        }

        if (system_handle != NULL)
        {
            /* TODO? */
        }

        if (teardown_completed_event != NULL)
        {
            system_event_release(teardown_completed_event);

            teardown_completed_event = NULL;
        }
    }
} _system_window_win32;

/* Forward declarations */
PRIVATE          void _system_window_window_closing_rendering_thread_entrypoint(ogl_context                          context,
                                                                                void*                                user_arg);
PRIVATE volatile void _system_window_teardown_thread_pool_callback             (system_thread_pool_callback_argument arg);


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

/** TODO */
LRESULT CALLBACK _system_window_class_message_loop_entrypoint(HWND   window_handle,
                                                              UINT   message_id,
                                                              WPARAM wparam,
                                                              LPARAM lparam)
{
    _system_window_win32* win32_ptr = (_system_window_win32*) ::GetWindowLongPtr(window_handle,
                                                                                 GWLP_USERDATA);

    switch (message_id)
    {
        case WM_CHAR:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_CHAR,
                                                 (void*) wparam);

            return 0;
        }

        case WM_CLOSE:
        {
            /* If the window is being closed per system request (eg. ALT+F4 was pressed), we need to stop
             * the rendering process first! Otherwise we're very likely to end up with a nasty crash. */
            ogl_context           context           = NULL;
            ogl_rendering_handler rendering_handler = NULL;

            system_window_get_property(win32_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                                      &context);
            system_window_get_property(win32_ptr->window,
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
                                                                 _system_window_window_closing_rendering_thread_entrypoint,
                                                                 win32_ptr);
            }

            /* OK, safe to destroy the system window at this point */
            ::DestroyWindow(window_handle);

            /* Now here's the trick: the call-backs can only be fired AFTER the window thread has shut down.
             * This lets us avoid various thread racing conditions which used to break all hell loose at the
             * tear-down time in the past, and which also let the window messages flow, which is necessary
             * for the closure to finish correctly.
             *
             * The entry-point we're pointing the task at does exactly that.
             */
            system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_CRITICAL,
                                                                                       _system_window_teardown_thread_pool_callback,
                                                                                       (void*) win32_ptr);

            system_thread_pool_submit_single_task(task);

            return 0;
        }

        case WM_DESTROY:
        {
            ::PostQuitMessage(0);

            return 0;
        }

        case WM_EMERALD_LOCK_MESSAGE_PUMP:
        {
            win32_ptr->is_message_pump_locked = true;

            system_event_set        (win32_ptr->message_pump_lock_event);
            system_event_wait_single(win32_ptr->message_pump_unlock_event);

            win32_ptr->is_message_pump_locked = false;

            system_event_reset(win32_ptr->message_pump_unlock_event);
            return 0;
        }

        case WM_EXITSIZEMOVE:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE);

            break;
        }

        case WM_KEYDOWN:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                                 (void*) (LOWORD(wparam) & 0xFF) );

            return 0;
        }

        case WM_KEYUP:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                                 (void*) (LOWORD(wparam) & 0xFF) );

            return 0;
        }

        case WM_LBUTTONDBLCLK:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK,
                                                 (void*) wparam);

            /* WinAPI sucks and is far from reasonable. Most of our apps will expect click notifications and won't watch for double clicks. */
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                                 (void*) wparam);

            break;
        }

        case WM_LBUTTONDOWN:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                                 (void*) wparam);

            break;
        }

        case WM_LBUTTONUP:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                                 (void*) wparam);

            break;
        }

        case WM_MBUTTONDBLCLK:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK,
                                                 (void*) wparam);

            break;
        }

        case WM_MBUTTONDOWN:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN,
                                                 (void*) wparam);

            break;
        }

        case WM_MBUTTONUP:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP,
                                                 (void*) wparam);

            break;
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

        case WM_RBUTTONDBLCLK:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK,
                                                 (void*) wparam);

            break;
        }

        case WM_RBUTTONDOWN:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                                 (void*) wparam);

            break;
        }

        case WM_RBUTTONUP:
        {
            system_window_execute_callback_funcs(win32_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP,
                                                 (void*) wparam);

            break;
        }

        #ifdef INCLUDE_WEBCAM_MANAGER
            case WM_DEVICECHANGE:
            {
                switch(wparam)
                {
                    case DBT_DEVICEARRIVAL:
                    {
                        /* Call back the web-cam manager with notification on new device just been plugged in */
                        system_thread_pool_task_descriptor task_descriptor = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                                    _webcam_manager_on_device_arrival,
                                                                                                                                    NULL);

                        system_thread_pool_submit_single_task(task_descriptor);

                        return TRUE;
                    }

                    case DBT_DEVICEREMOVECOMPLETE:
                    {
                        /* Call back the web-cam manager with notification on a device just been plugged out */
                        system_thread_pool_task_descriptor task_descriptor = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                                    _webcam_manager_on_device_removal,
                                                                                                                                    NULL);

                        system_thread_pool_submit_single_task(task_descriptor);

                        return TRUE;
                    }
                }

                return TRUE;
            }
        #endif
    }

    return ::DefWindowProcA(window_handle,
                            message_id,
                            wparam,
                            lparam);
}

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

/** TODO */
PRIVATE volatile void _system_window_teardown_thread_pool_callback(system_thread_pool_callback_argument arg)
{
    _system_window_win32* window_ptr = (_system_window_win32*) arg;

    /* Call back the subscribers, if any */
    system_window_execute_callback_funcs(window_ptr->window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED);

    system_event_set(window_ptr->teardown_completed_event);
}


/** Please see header for spec */
PUBLIC void system_window_win32_close_window(system_window_win32 window)
{
    _system_window_win32* win32_ptr = (_system_window_win32*) window;

    ASSERT_DEBUG_SYNC(win32_ptr != NULL,
                      "Input argument is NULL");
    ASSERT_DEBUG_SYNC(win32_ptr->system_handle != NULL,
                      "No window to close - system handle is NULL.");

    if (win32_ptr->system_handle != NULL)
    {
        ::SendMessageA(win32_ptr->system_handle,
                       WM_CLOSE,
                       0,  /* wParam */
                       0); /* lParam */
    }
}

/** Please see header for spec */
PUBLIC void system_window_win32_deinit(system_window_win32 window)
{
    _system_window_win32* win32_ptr = (_system_window_win32*) window;

    ASSERT_DEBUG_SYNC(win32_ptr != NULL,
                      "Input argument is NULL");

    delete win32_ptr;
    win32_ptr = NULL;
}

/** Please see header for spec */
PUBLIC bool system_window_win32_get_property(  system_window_win32    window,
                                               system_window_property property,
                                              void*                  out_result)
{
    bool                  result    = true;
    _system_window_win32* win32_ptr = (_system_window_win32*) window;

    /* Only handle platform-specific queries */
    switch (property)
    {
        case SYSTEM_WINDOW_PROPERTY_CURSOR_POSITION:
        {
            POINT cursor_position;
            int*  result_ptr = (int*) out_result;

            ::GetCursorPos(&cursor_position);

            result_ptr[0] = cursor_position.x;
            result_ptr[1] = cursor_position.y;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DC:
        {
            ASSERT_DEBUG_SYNC(win32_ptr->system_dc != 0,
                              "System DC is NULL.");

            *(system_window_dc*) out_result = win32_ptr->system_dc;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_HANDLE:
        {
            ASSERT_DEBUG_SYNC(win32_ptr->system_handle != 0,
                              "System handle is NULL.");

            *(system_window_handle*) out_result = win32_ptr->system_handle;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_POSITION:
        {
            ASSERT_DEBUG_SYNC(win32_ptr->system_handle != 0,
                              "System handle is NULL.");

            if (win32_ptr->system_handle != 0)
            {
                RECT window_rect;

                if (::GetWindowRect(win32_ptr->system_handle,
                                   &window_rect) == TRUE)
                {
                    ((int*) out_result)[0] = window_rect.left;
                    ((int*) out_result)[1] = window_rect.top;
                }
            }

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
PUBLIC void system_window_win32_get_screen_size(int* out_screen_width_ptr,
                                                int* out_screen_height_ptr)
{
    ASSERT_DEBUG_SYNC(out_screen_width_ptr  != NULL &&
                      out_screen_height_ptr != NULL,
                      "Input arguments are NULL");

    *out_screen_width_ptr  = ::GetSystemMetrics(SM_CXSCREEN);
    *out_screen_height_ptr = ::GetSystemMetrics(SM_CYSCREEN);
}

/** Please see header for spec */
PUBLIC system_window_win32 system_window_win32_init(system_window owner)
{
    _system_window_win32* win32_ptr = new (std::nothrow) _system_window_win32(owner);

    ASSERT_DEBUG_SYNC(win32_ptr != NULL,
                      "Out of memory");

    /* The descriptor is fully initialized in the constructor. */

    return (system_window_win32) win32_ptr;
}

/** Please see header for spec */
PUBLIC void system_window_win32_handle_window(system_window_win32 window)
{
    bool                  is_visible = false;
    _system_window_win32* win32_ptr  = (_system_window_win32*) window;

    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_VISIBLE,
                              &is_visible);

    if (is_visible)
    {
        ::ShowWindow(win32_ptr->system_handle,
                     SW_SHOW);
    }

    ::UpdateWindow(win32_ptr->system_handle);

    /* Cache the message pump thread ID */
    win32_ptr->message_pump_thread_id = system_threads_get_thread_id();

    /* Pump messages */
    MSG  msg;
    BOOL returned_value;

    while( (returned_value = ::GetMessage(&msg,
                                          0,         /* hWnd          */
                                          0,         /* wMsgFilterMin */
                                          0) != 0) ) /* wMsgFilterMax */
    {
        if (returned_value == -1)
        {
            /* Wat */
            break;
        }

        ::TranslateMessage(&msg);
        ::DispatchMessage (&msg);
    }

    system_event_wait_single(win32_ptr->teardown_completed_event);
}

/** Please see header for spec */
PUBLIC bool system_window_win32_open_window(system_window_win32 window,
                                            bool                is_first_window)
{
    DWORD                 ex_style       = 0;
    bool                  result         = true;
    DWORD                 style          = 0;
    _system_window_win32* win32_ptr      = (_system_window_win32*) window;
    int                   x_border_width = 0;
    int                   x1_delta       = 0;
    int                   y_border_width = 0;
    int                   y1_delta       = 0;

    /* Create window class, if necessary, and cache mouse cursors */
    if (is_first_window)
    {
        /* Set up the window class. */
        WNDCLASSA   window_class;
        static bool window_class_registered = false;

        if (!window_class_registered)
        {
            window_class.cbClsExtra    = 0;
            window_class.cbWndExtra    = 0;
            window_class.hbrBackground = 0;
            window_class.hCursor       = ::LoadCursorA     (NULL,
                                                            IDC_ARROW);
            window_class.hIcon         = ::LoadIconA       (NULL,
                                                            IDI_WINLOGO);
            window_class.hInstance     = ::GetModuleHandleA(NULL);
            window_class.lpfnWndProc   = _system_window_class_message_loop_entrypoint;
            window_class.lpszClassName = EMERALD_WINDOW_CLASS_NAME;
            window_class.lpszMenuName  = NULL;
            window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;

            if (!::RegisterClassA(&window_class) )
            {
                LOG_FATAL("Could not register engine window class.");

                result = false;
                goto end;
            }

            window_class_registered = true;
        } /* if (!window_class_registered) */
    } /* if (is_first_window) */

    /* Cache mouse cursor */
    win32_ptr->action_forbidden_cursor_resource  = ::LoadCursorA(0, IDC_NO);
    win32_ptr->arrow_cursor_resource             = ::LoadCursorA(0, IDC_ARROW);
    win32_ptr->crosshair_cursor_resource         = ::LoadCursorA(0, IDC_CROSS);
    win32_ptr->hand_cursor_resource              = ::LoadCursorA(0, IDC_HAND);
    win32_ptr->horizontal_resize_cursor_resource = ::LoadCursorA(0, IDC_SIZEWE);
    win32_ptr->move_cursor_resource              = ::LoadCursorA(0, IDC_SIZEALL);
    win32_ptr->vertical_resize_cursor_resource   = ::LoadCursorA(0, IDC_SIZENS);

    ASSERT_DEBUG_SYNC(win32_ptr->action_forbidden_cursor_resource != NULL,
                      "Failed to load action forbidden cursor resource.");
    ASSERT_DEBUG_SYNC(win32_ptr->arrow_cursor_resource != NULL,
                      "Failed to load arrow cursor resource.");
    ASSERT_DEBUG_SYNC(win32_ptr->crosshair_cursor_resource != NULL,
                      "Failed to load crosshair cursor resource.");
    ASSERT_DEBUG_SYNC(win32_ptr->hand_cursor_resource != NULL,
                      "Failed to load hand cursor resource.");
    ASSERT_DEBUG_SYNC(win32_ptr->horizontal_resize_cursor_resource != NULL,
                      "Failed to load horizontal resize cursor resource.");
    ASSERT_DEBUG_SYNC(win32_ptr->move_cursor_resource != NULL,
                      "Failed to load move cursor resource.");
    ASSERT_DEBUG_SYNC(win32_ptr->vertical_resize_cursor_resource != NULL,
                      "Failed to load vertical resize cursor resource.");

    /* If full-screen window was requested, change display mode. */
    bool                      is_window_fullscreen = false;
    bool                      is_window_scalable   = false;
    system_window_handle      parent_window_handle = NULL;
    system_hashed_ansi_string window_title         = NULL;
    int                       x1y1x2y2[4];

    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_FULLSCREEN,
                              &is_window_fullscreen);
    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_SCALABLE,
                              &is_window_scalable);
    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,
                              &parent_window_handle);
    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_TITLE,
                              &window_title);

    system_window_get_property(win32_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                               x1y1x2y2);

    if (is_window_fullscreen)
    {
        system_screen_mode screen_mode = NULL;

        system_window_get_property(win32_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_SCREEN_MODE,
                                  &screen_mode);

        if (!system_screen_mode_activate(screen_mode) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not switch to the requested screen mode.");

            goto end;
        }

        /* Configure style bitfields accordingly. */
        ex_style = WS_EX_APPWINDOW;
        style    = WS_POPUP;
    } /* if (is_window_fullscreen) */
    else
    {
        /* Non-full-screen window requested. Configure style bitfields accordingly. */
        if (parent_window_handle == NULL)
        {
            style = WS_POPUP;
        }
        else
        {
            style = WS_OVERLAPPED | WS_CHILD;
        }

        if (is_window_scalable)
        {
            style |= WS_THICKFRAME;
        }

        /* Get system metrics we need to use for the window */
        x_border_width = ::GetSystemMetrics(SM_CXSIZEFRAME);
        y_border_width = ::GetSystemMetrics(SM_CYSIZEFRAME);

        if (!is_window_scalable)
        {
            x_border_width = 0;
            y_border_width = 0;
        }

        if (parent_window_handle != NULL)
        {
            x1_delta = -::GetSystemMetrics(SM_CXFRAME);
            y1_delta = (is_window_scalable ? -::GetSystemMetrics(SM_CYSIZEFRAME) :
                                             -::GetSystemMetrics(SM_CYFRAME) )     - ::GetSystemMetrics(SM_CYCAPTION);
        }
    }

    win32_ptr->system_handle = ::CreateWindowExA(ex_style,
                                                 EMERALD_WINDOW_CLASS_NAME,
                                                 system_hashed_ansi_string_get_buffer(window_title),
                                                 style,
                                                 x1y1x2y2[0] + x1_delta,
                                                 x1y1x2y2[1] + y1_delta,
                                                 x1y1x2y2[2] - x1y1x2y2[0] + x_border_width,
                                                 x1y1x2y2[3] - x1y1x2y2[1] + y_border_width,
                                                 parent_window_handle,
                                                 NULL,                                       /* no menu */
                                                 ::GetModuleHandleA(0),
                                                 window);

    if (win32_ptr->system_handle == NULL)
    {
        LOG_FATAL("Could not create window [%s]",
                  system_hashed_ansi_string_get_buffer(window_title) );

        result = false;
        goto end;
    }
    
    #ifdef INCLUDE_WEBCAM_MANAGER
    {
        /* Register for system notifications on webcams */
        DEV_BROADCAST_DEVICEINTERFACE notification_filter = {0};

        notification_filter.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        notification_filter.dbcc_classguid  = GUID_KSCATEGORY_CAPTURE;

        window->webcam_device_notification_handle = ::RegisterDeviceNotificationA(win32_ptr->system_handle,
                                                                                 &notification_filter,
                                                                                  DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    #endif /* INCLUDE_WEBCAM_MANAGER */

    win32_ptr->system_dc = ::GetDC(win32_ptr->system_handle);

    if (win32_ptr->system_dc == NULL)
    {
        LOG_FATAL("Could not obtain device context for window [%s]",
                  system_hashed_ansi_string_get_buffer(window_title) );

        result = false;
        goto end;
    }

    /* Set up user data field for the window */
    ::SetWindowLongPtr(win32_ptr->system_handle,
                       GWLP_USERDATA,
                       (LONG) win32_ptr);

end:
    return result;
}

/** Please see header for property */
PUBLIC bool system_window_win32_set_property(system_window_win32    window,
                                             system_window_property property,
                                             const void*            data)
{
    bool                  result    = true;
    _system_window_win32* win32_ptr = (_system_window_win32*) window;

    switch (property)
    {
        case SYSTEM_WINDOW_PROPERTY_CURSOR:
        {
            _lock_system_window_message_pump(win32_ptr);
            {
                system_window_mouse_cursor cursor = *(system_window_mouse_cursor*) data;

                switch (cursor)
                {
                    case SYSTEM_WINDOW_MOUSE_CURSOR_ACTION_FORBIDDEN:
                    {
                        win32_ptr->current_mouse_cursor_system_resource = win32_ptr->action_forbidden_cursor_resource;

                        break;
                    }

                    case SYSTEM_WINDOW_MOUSE_CURSOR_ARROW:
                    {
                        win32_ptr->current_mouse_cursor_system_resource = win32_ptr->arrow_cursor_resource;

                        break;
                    }

                    case SYSTEM_WINDOW_MOUSE_CURSOR_CROSSHAIR:
                    {
                        win32_ptr->current_mouse_cursor_system_resource = win32_ptr->crosshair_cursor_resource;

                        break;
                    }

                    case SYSTEM_WINDOW_MOUSE_CURSOR_HORIZONTAL_RESIZE:
                    {
                        win32_ptr->current_mouse_cursor_system_resource = win32_ptr->horizontal_resize_cursor_resource;

                        break;
                    }

                    case SYSTEM_WINDOW_MOUSE_CURSOR_MOVE:
                    {
                        win32_ptr->current_mouse_cursor_system_resource = win32_ptr->move_cursor_resource;

                        break;
                    }

                    case SYSTEM_WINDOW_MOUSE_CURSOR_VERTICAL_RESIZE:
                    {
                        win32_ptr->current_mouse_cursor_system_resource = win32_ptr->vertical_resize_cursor_resource;

                        break;
                    }

                    default:
                    {
                        LOG_ERROR("Unrecognized cursor type [%d] requested", cursor);

                        result = false;
                    }
                }
            }
            _unlock_system_window_message_pump(win32_ptr);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DIMENSIONS:
        {
            bool       is_scalable      = false;
            const int* width_height_ptr = (const int*) data;
            int        x1y1x2y2[4] = { 0 };

            system_window_get_property(win32_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_IS_SCALABLE,
                                      &is_scalable);
            system_window_get_property(win32_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                                       x1y1x2y2);

            result = (::MoveWindow(win32_ptr->system_handle,
                                   x1y1x2y2[0] - (is_scalable ? ::GetSystemMetrics(SM_CXSIZEFRAME) : ::GetSystemMetrics(SM_CXFRAME) ),
                                   x1y1x2y2[1] - (is_scalable ? ::GetSystemMetrics(SM_CYSIZEFRAME) : ::GetSystemMetrics(SM_CYFRAME) ) - ::GetSystemMetrics(SM_CYCAPTION),
                                   width_height_ptr[0],
                                   width_height_ptr[1],
                                   TRUE) == TRUE);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_POSITION:
        {
            system_window_handle parent_window_handle = NULL;
            const int*           xy_ptr               = (const int*) data;

            system_window_get_property(win32_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,
                                      &parent_window_handle);

            result = (::SetWindowPos(win32_ptr->system_handle,
                                     parent_window_handle,
                                     xy_ptr[0],
                                     xy_ptr[1],
                                     0,
                                     0,
                                     SWP_NOSIZE) ) != 0;

            break;
        }

        default:
        {
            result = false;
        }
    } /* switch (property) */

    return result;
}
