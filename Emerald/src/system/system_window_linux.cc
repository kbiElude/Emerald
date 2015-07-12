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
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include "system/system_window_linux.h"
#include <unistd.h>
#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>


typedef struct _system_window_linux
{
    Atom delete_window_atom;

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
    Display* display;

    Colormap             colormap;
    system_window_handle system_handle;
    system_window        window; /* DO NOT retain */

    system_event         teardown_completed_event;

    int cursor_position[2]; /* only updated prior to call-back execution! */


    explicit _system_window_linux(system_window in_window)
    {
        action_forbidden_cursor_resource     = 0;
        arrow_cursor_resource                = 0;
        colormap                             = (Colormap) NULL;
        crosshair_cursor_resource            = 0;
        current_mouse_cursor_system_resource = (Cursor) NULL;
        default_screen_index                 = -1;
        delete_window_atom                   = (Atom) NULL;
        display                              = NULL;
        hand_cursor_resource                 = 0;
        horizontal_resize_cursor_resource    = 0;
        move_cursor_resource                 = 0;
        system_handle                        = (system_window_handle) NULL;
        teardown_completed_event             = system_event_create(true); /* manual_reset */
        window                               = in_window;
        vertical_resize_cursor_resource      = 0;
    }

    ~_system_window_linux()
    {
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

        if (display       != 0                      &&
            system_handle != (system_window_handle) NULL)
        {
            XDestroyWindow(display,
                           system_handle);

            system_handle = (system_window_handle) NULL;
        }

        if (display != NULL)
        {
            XCloseDisplay(display);

            display = NULL;
        }

        if (teardown_completed_event != NULL)
        {
            system_event_release(teardown_completed_event);

            teardown_completed_event = NULL;
        }
    }
} _system_window_linux;

/* Forward declarations */
PRIVATE void _system_window_linux_handle_close_window_request(_system_window_linux* linux_ptr,
                                                              bool                  remove_from_window_map);


/* Main display which is used to listen for window events.
 *
 * You might have noticed that each system_window_linux instance also holds a display.
 * Each such display is used exclusively for buffer swapping. This is required,
 * since we have a separate rendering thread and a separate thread for watching for
 * the window events. If we used a single display, the rendering pump's thread would have acquired
 * the display lock in order to wait for incoming events, causing the buffer swap op to
 * starve until window event arrives.
 *
 * Some of these variables are configured in system_window_linux_init_global() and
 * deinitialized in system_window_linux_deinit_global().
 *
 */
PRIVATE const unsigned int client_message_type_register_window = 2;

PRIVATE system_critical_section message_pump_registered_windows_cs  = NULL;
PRIVATE system_hash64map        message_pump_registered_windows_map = NULL; /* maps (system_hash64) Window to system_window_linux */
PRIVATE system_event            message_pump_thread_died_event      = NULL;
PRIVATE _system_window_linux*   root_window_linux_ptr               = NULL;


/** Global UI thread event handler.
 *
 *  @param event_ptr TODO
 *
 **/
PRIVATE void _system_window_linux_handle_event(const XEvent* event_ptr)
{
    _system_window_linux* linux_ptr = NULL;

    system_critical_section_enter(message_pump_registered_windows_cs);
    {
        system_hash64 key = (system_hash64) event_ptr->xany.window;

        system_hash64map_get(message_pump_registered_windows_map,
                             key,
                            &linux_ptr);
    }
    system_critical_section_leave(message_pump_registered_windows_cs);

    switch (event_ptr->type)
    {
        case ButtonPress:
        case ButtonRelease:
        {
            /* TODO: Re-use x & y information from the event */

            system_window_callback_func callback_func;
            bool                        should_proceed = (linux_ptr != NULL);

            ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                              "Could not find a system_window_linux instance associated with the event's window");

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
            /* There's a couple of client messages we need to handle:
             *
             * 1. REGISTER_WINDOW: new renderer window was spawned. Need to re-configure the main display
             *                     to intercept for certain events generated for the new event.
             * 
             */
            if (event_ptr->xclient.message_type == client_message_type_register_window)
            {
                LOG_FATAL("DEBUG: Register window client message received.");

                /* Iterate over all registered windows and configure event interception for every window */
                system_critical_section_enter(message_pump_registered_windows_cs);
                {
                    unsigned int n_registered_windows = 0;

                    system_hash64map_get_property(message_pump_registered_windows_map,
                                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                 &n_registered_windows);

                    for (unsigned int n_registered_window = 0;
                                      n_registered_window < n_registered_windows;
                                    ++n_registered_window)
                    {
                        system_hash64       hash_window     = 0;
                        system_window_linux internal_window = (system_window_linux) NULL;
                        Window              window          = (Window)              NULL;

                        if (!system_hash64map_get_element_at(message_pump_registered_windows_map,
                                                             n_registered_window,
                                                            &internal_window,
                                                            &hash_window) )
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Could not retrieve a registered window key+value pair for index [%d]",
                                              n_registered_window);

                            continue;
                        }

                        window = (Window) hash_window;

                        ASSERT_DEBUG_SYNC(root_window_linux_ptr != NULL,
                                          "Root window is unknown");

                        XSetWMProtocols(root_window_linux_ptr->display,
                                        window,
                                        &root_window_linux_ptr->delete_window_atom,
                                        1); /* n_atoms */
                    } /* for (all registered windows) */
                }
                system_critical_section_leave(message_pump_registered_windows_cs);
            }
            else
            if (event_ptr->xclient.data.l[0] == linux_ptr->delete_window_atom)
            {
                _system_window_linux_handle_close_window_request(linux_ptr,
                                                                 false /* remove_from_window_map */);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized client message type");
            }

            break;
        }

        case DestroyNotify:
        {
            _system_window_linux_handle_close_window_request(linux_ptr,
                                                             true); /* remove_from_window_map */

            break;
        } /* case DestroyNotify: */

        case MotionNotify:
        {
            /* TODO: Re-use x & y information from the event */
            /* TODO: Provide vk_status info */
            system_window_execute_callback_funcs(linux_ptr->window,
                                                 SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                                 NULL);

            break;
        } /* case MotionNotify: */

        case KeyPress:
        case KeyRelease:
        {
            char           buffer[4];
            XComposeStatus compose_status;
            KeySym         key_symbol;

            XLookupString((XKeyEvent*) event_ptr,
                           buffer,
                           sizeof(buffer),
                          &key_symbol,
                          &compose_status);

            system_window_execute_callback_funcs(linux_ptr->window,
                                                 (event_ptr->type == KeyPress) ? SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN
                                                                               : SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                                 (void*) (intptr_t) (key_symbol & 0xFF) );

            break;
        }
    } /* switch (event_ptr->type) */

#if 0
    switch (message_id)
    {
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

/** TODO */
PRIVATE void _system_window_linux_handle_close_window_request(_system_window_linux* linux_ptr,
                                                              bool                  remove_from_window_map)
{
    /* TODO: ADD WINDOW_CLOSING CALLBACK SUPPORT */

    /* Remove the window from the "registered windows" map */
    ogl_context           context           = NULL;
    ogl_rendering_handler rendering_handler = NULL;

    if (remove_from_window_map)
    {
        system_critical_section_enter(message_pump_registered_windows_cs);
        {
            system_hash64map_remove(message_pump_registered_windows_map,
                                    (system_hash64) linux_ptr->system_handle);
        }
        system_critical_section_leave(message_pump_registered_windows_cs);
    }

    /* If the window is being closed per system request (eg. ALT+F4 was pressed), we need to stop
     * the rendering process first! Otherwise we're very likely to end up with a nasty crash. */
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
    }

    /* Call back the subscribers, if any */
    system_window_execute_callback_funcs(linux_ptr->window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED);

    linux_ptr->system_handle = NULL;

    system_event_set(linux_ptr->teardown_completed_event);
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


/** Please see header for spec */
PUBLIC void system_window_linux_close_window(system_window_linux window)
{
    _system_window_linux* linux_ptr = (_system_window_linux*) window;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");
    ASSERT_DEBUG_SYNC(linux_ptr->system_handle != (system_window_handle) NULL,
                      "No window to close - system handle is NULL.");

    XDestroyWindow(linux_ptr->display,
                   linux_ptr->system_handle);
}

/** Please see header for spec */
PUBLIC void system_window_linux_deinit(system_window_linux window)
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
    ASSERT_DEBUG_SYNC(root_window_linux_ptr != NULL,
                      "Root window has not been initialized by the time system_window_linux_deinit_global() was called.");

    if (message_pump_registered_windows_cs != NULL)
    {
        system_critical_section_enter(message_pump_registered_windows_cs);
        system_critical_section_leave(message_pump_registered_windows_cs);

        system_critical_section_release(message_pump_registered_windows_cs);

        message_pump_registered_windows_cs = NULL;
    }

    if (message_pump_registered_windows_map != NULL)
    {
        system_hash64map_release(message_pump_registered_windows_map);

        message_pump_registered_windows_map = NULL;
    }

    if (message_pump_thread_died_event != NULL)
    {
        system_event_release(message_pump_thread_died_event);

        message_pump_thread_died_event = NULL;
    }
}

/** Please see header for spec */
PUBLIC bool system_window_linux_get_property(system_window_linux    window,
                                             system_window_property property,
                                             void*                  out_result)
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

            XLockDisplay(linux_ptr->display);
            {
                XQueryPointer(linux_ptr->display,
                            linux_ptr->system_handle,
                            &temp_root_window,
                            &temp_child_window,
                            &temp_root_x,
                            &temp_root_y,
                            result_ptr + 0,
                            result_ptr + 1,
                            &temp_mask);
            }
            XUnlockDisplay(linux_ptr->display);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DISPLAY:
        {
            *(Display**) out_result = linux_ptr->display;

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

                XLockDisplay(linux_ptr->display);
                {
                    XGetWindowAttributes(linux_ptr->display,
                                        linux_ptr->system_handle,
                                        &window_attributes);
                }
                XUnlockDisplay(linux_ptr->display);

                ((int*) out_result)[0] = window_attributes.x;
                ((int*) out_result)[1] = window_attributes.y;
            }

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_SCREEN_INDEX:
        {
            *(int*) out_result = linux_ptr->default_screen_index;

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
PUBLIC void system_window_linux_get_screen_size(int* out_screen_width_ptr,
                                                int* out_screen_height_ptr)
{
    Display* display                = NULL;
    Screen*  display_screen         = NULL;
    bool     should_release_display = false;

    ASSERT_DEBUG_SYNC(out_screen_width_ptr  != NULL &&
                      out_screen_height_ptr != NULL,
                      "Input arguments are NULL");

    if (root_window_linux_ptr != NULL)
    {
        display = root_window_linux_ptr->display;
    }
    else
    {
        display                = XOpenDisplay(":0");
        should_release_display = true;

        ASSERT_DEBUG_SYNC(display != NULL,
                          "Cannot open connection to display.");
    }

    XLockDisplay(display);
    {
        display_screen = DefaultScreenOfDisplay(display);
    }
    XUnlockDisplay(display);

    ASSERT_DEBUG_SYNC(display_screen != NULL,
                      "No default screen reported for the display");

    *out_screen_width_ptr  = display_screen->width;
    *out_screen_height_ptr = display_screen->height;

    if (should_release_display)
    {
        XCloseDisplay(display);

        display = NULL;
    }
}

/** Please see header for spec */
PUBLIC void system_window_linux_handle_window(system_window_linux window)
{
    XEvent                current_event;
    bool                  is_visible = false;
    _system_window_linux* linux_ptr  = (_system_window_linux*) window;

    LOG_INFO("system_window_linux_handle_window() starts..");

    system_window_get_property(linux_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_IS_VISIBLE,
                              &is_visible);

    /* Show the window if that's what was requested*/
    if (is_visible)
    {
        XLockDisplay(linux_ptr->display);
        {
            XMapWindow(linux_ptr->display,
                       linux_ptr->system_handle);
        }
        XUnlockDisplay(linux_ptr->display);
    }

    /* Under Linux, we're using a single UI thread which listens for window events
     * and handles them accordingly. Therefore, all we need to do here is to wait
     * until the WM_DELETE_WINDOW message arrives, which is when we can return
     * the execution to the caller. */
    while (true)
    {
        if (XPending(linux_ptr->display) )
        {
            XLockDisplay(linux_ptr->display);
            {
                XNextEvent(linux_ptr->display,
                          &current_event);
            }
            XUnlockDisplay(linux_ptr->display);

            /* Handle the event */
            _system_window_linux_handle_event(&current_event);

            if (current_event.type == DestroyNotify)
            {
                /* Get out of the loop, the window is dead. */
                linux_ptr->window = (Window) NULL;

                break;
            }
        }
        else
        {
            /* TODO: TEMP SOLUTION, FIX WHEN ALL UNIT TESTS PASS */
            usleep(10);
        }
    }

    LOG_INFO("system_window_linux_handle_window() waiting to die gracefully..");

    system_event_wait_single(linux_ptr->teardown_completed_event);

    LOG_INFO("system_window_linux_handle_window() completed.");
}

/** Please see header for spec */
PUBLIC system_window_linux system_window_linux_init(system_window owner)
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

    /* Initialize global stuff */
    message_pump_registered_windows_cs  = system_critical_section_create();
    message_pump_registered_windows_map = system_hash64map_create       (sizeof(_system_window_linux*) );

    message_pump_thread_died_event = system_event_create(true); /* manual_reset */
}


/** Please see header for spec */
PUBLIC bool system_window_linux_open_window(system_window_linux window,
                                            bool                is_first_window)
{
    bool                      is_window_fullscreen = false;
    bool                      is_window_scalable   = false;
    XKeyboardControl          keyboard_control;
    _system_window_linux*     linux_ptr            = (_system_window_linux*) window;
    uint32_t                  n_depth_bits         = 0;
    uint32_t                  n_rgba_bits[4]       = {0};
    system_window_handle      parent_window_handle = (system_window_handle) NULL;
    XClientMessageEvent       register_window_message;
    bool                      result               = true;
    XSetWindowAttributes      winattr;
    ogl_context_type          window_context_type  = OGL_CONTEXT_TYPE_UNDEFINED;
    system_window_handle      window_handle        = (system_window_handle) NULL;
    system_pixel_format       window_pixel_format  = NULL;
    system_hashed_ansi_string window_title         = NULL;
    int                       x1y1x2y2[4];
    int                       visual_attribute_list [2 * 5 /* rgba bits, depth bits */ + 2 /* doublebuffering, rgba */ + 1 /* terminator */];
    XVisualInfo*              visual_info_ptr      = NULL;

    /* Start up a new display for the window */
    linux_ptr->display = XOpenDisplay(":0");

    if (linux_ptr->display == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not open display at :0");

        goto end;
    }

    /* Configure the keyboard auto-repeat mode. We want it disabled, so that the key events arrives
     * just like under Windows.
     */
    keyboard_control.auto_repeat_mode = AutoRepeatModeOff;

    XChangeKeyboardControl(linux_ptr->display,
                           KBAutoRepeatMode,
                          &keyboard_control);

    /* Initialize global root window properties */
    if (is_first_window)
    {
        root_window_linux_ptr = linux_ptr;
    }

    linux_ptr->delete_window_atom = XInternAtom(linux_ptr->display,
                                                "WM_DELETE_WINDOW",
                                                 False);

    /* Cache mouse cursors */
    linux_ptr->action_forbidden_cursor_resource  = XCreateFontCursor(linux_ptr->display,
                                                                     XC_X_cursor);
    linux_ptr->arrow_cursor_resource             = XCreateFontCursor(linux_ptr->display,
                                                                     XC_left_ptr);
    linux_ptr->crosshair_cursor_resource         = XCreateFontCursor(linux_ptr->display,
                                                                     XC_crosshair);
    linux_ptr->hand_cursor_resource              = XCreateFontCursor(linux_ptr->display,
                                                                     XC_hand1);
    linux_ptr->horizontal_resize_cursor_resource = XCreateFontCursor(linux_ptr->display,
                                                                     XC_sb_h_double_arrow);
    linux_ptr->move_cursor_resource              = XCreateFontCursor(linux_ptr->display,
                                                                     XC_right_ptr);
    linux_ptr->vertical_resize_cursor_resource   = XCreateFontCursor(linux_ptr->display,
                                                                     XC_sb_v_double_arrow);

    ASSERT_DEBUG_SYNC(linux_ptr->action_forbidden_cursor_resource != (Cursor) NULL,
                      "Failed to load action forbidden cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->arrow_cursor_resource != (Cursor) NULL,
                      "Failed to load arrow cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->crosshair_cursor_resource != (Cursor) NULL,
                      "Failed to load crosshair cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->hand_cursor_resource != (Cursor) NULL,
                      "Failed to load hand cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->horizontal_resize_cursor_resource != (Cursor) NULL,
                      "Failed to load horizontal resize cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->move_cursor_resource != (Cursor) NULL,
                      "Failed to load move cursor resource.");
    ASSERT_DEBUG_SYNC(linux_ptr->vertical_resize_cursor_resource != (Cursor) NULL,
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

    ASSERT_DEBUG_SYNC(!is_window_scalable,
                      "TODO: Scalable windows are not supported yet");
    ASSERT_DEBUG_SYNC(!is_window_fullscreen,
                      "TODO: Full-screen windows are not supported yet");

    /* Determine parent window handle */
    ASSERT_DEBUG_SYNC(parent_window_handle == (Cursor) NULL,
                      "TODO: system_window_linux does not support window embedding.");

    linux_ptr->default_screen_index = DefaultScreen(linux_ptr->display);
    parent_window_handle            = RootWindow   (linux_ptr->display,
                                                    linux_ptr->default_screen_index);

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

    linux_ptr->colormap = XCreateColormap(linux_ptr->display,
                                          parent_window_handle,
                                          visual_info_ptr->visual,
                                          AllocNone);

    window_attributes.colormap = linux_ptr->colormap;

    linux_ptr->system_handle = XCreateWindow(linux_ptr->display,
                                             parent_window_handle,
                                             x1y1x2y2[0],
                                             x1y1x2y2[1],
                                             x1y1x2y2[2] - x1y1x2y2[0],
                                             x1y1x2y2[3] - x1y1x2y2[1],
                                             0, /* border_width */
                                             visual_info_ptr->depth,
                                             InputOutput,
                                             visual_info_ptr->visual,
                                             CWColormap,
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

    XSelectInput   (linux_ptr->display,
                    linux_ptr->system_handle,
                    ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | StructureNotifyMask);
    XSetWMProtocols(linux_ptr->display,
                    linux_ptr->system_handle,
                   &linux_ptr->delete_window_atom,
                    1); /* n_atoms */

    /* Register the window */
    system_critical_section_enter(message_pump_registered_windows_cs);
    {
        system_hash64 system_handle_hash = (system_hash64) linux_ptr->system_handle;

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(message_pump_registered_windows_map,
                                                     system_handle_hash),
                          "Newly created window is already recognized by the registered windows map");

        system_hash64map_insert(message_pump_registered_windows_map,
                                system_handle_hash,
                                linux_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }
    system_critical_section_leave(message_pump_registered_windows_cs);

    register_window_message.format       = 32; /* set anything, really. */
    register_window_message.message_type = client_message_type_register_window;
    register_window_message.type         = ClientMessage;

    XLockDisplay(root_window_linux_ptr->display);
    {
        Window root_window_system_handle = (Window) NULL;

        system_window_get_property(root_window_linux_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_HANDLE,
                                  &root_window_system_handle);

        XSendEvent (root_window_linux_ptr->display,
                    root_window_system_handle,
                    False, /* propagate */
                    0,     /* event_mask */
                    (XEvent*) &register_window_message);
    }
    XUnlockDisplay(root_window_linux_ptr->display);

    /* Update the window title */
    XStoreName(linux_ptr->display,
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
PUBLIC bool system_window_linux_set_property(system_window_linux    window,
                                             system_window_property property,
                                             const void*            data)
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
                XLockDisplay(linux_ptr->display);
                {
                    XDefineCursor(linux_ptr->display,
                                  linux_ptr->system_handle,
                                  linux_ptr->current_mouse_cursor_system_resource);
                }
                XUnlockDisplay(linux_ptr->display);
            }

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DIMENSIONS:
        {
            const int* width_height_ptr = (const int*) data;

            XLockDisplay(linux_ptr->display);
            {
                XResizeWindow(linux_ptr->display,
                              linux_ptr->system_handle,
                              width_height_ptr[0],
                              width_height_ptr[1]);
            }
            XUnlockDisplay(linux_ptr->display);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_POSITION:
        {
            system_window_handle parent_window_handle = (system_window_handle) NULL;
            const int*           xy_ptr               = (const int*) data;

            system_window_get_property(linux_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,
                                      &parent_window_handle);

            XLockDisplay(linux_ptr->display);
            {
                XMoveWindow(linux_ptr->display,
                            linux_ptr->system_handle,
                            xy_ptr[0],
                            xy_ptr[1]);
            }
            XUnlockDisplay(linux_ptr->display);

            break;
        }

        default:
        {
            result = false;
        }
    } /* switch (property) */

    return result;
}
