/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <Dbt.h>
#include "ogl/ogl_context.h"
#include "ogl/ogl_pixel_format_descriptor.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_threads.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

#ifdef INCLUDE_WEBCAM_MANAGER
    #include "webcam/webcam_manager.h"

    #include <INITGUID.H>
    DEFINE_GUID( GUID_DEVCLASS_IMAGE, 0x6bdd1fc6L, 0x810f, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f );
    DEFINE_GUID( GUID_KSCATEGORY_CAPTURE, 0x65E8773D, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
#endif /* INCLUDE_WEBCAM_MANAGER */ 

/** Internal type definitions  */
typedef struct
{
    void*                                pfn_callback;
    system_window_callback_func_priority priority;
    void*                                user_arg;
} _callback_descriptor;

typedef struct
{
    /** Full-screen only */
    uint16_t fullscreen_bpp;
    uint16_t fullscreen_freq;

    ogl_context_type           context_type;
    bool                       is_cursor_visible;
    bool                       is_fullscreen;
    bool                       is_scalable;
    bool                       multisampling_supported;
    uint16_t                   n_multisampling_samples;
    HWND                       parent_window_handle;
    system_hashed_ansi_string  title;
    bool                       visible;
    bool                       vsync_enabled;
    system_window_mouse_cursor window_mouse_cursor;
    HCURSOR                    window_mouse_cursor_system_resource;
    int                        x1y1x2y2[4];       

    ogl_rendering_handler rendering_handler;

    #ifdef INCLUDE_WEBCAM_MANAGER
        HDEVNOTIFY webcam_device_notification_handle;
    #endif

    system_window_dc      system_dc;
    system_window_handle  system_handle;
    ogl_context           system_ogl_context;

    bool                 is_message_pump_locked;
    system_event         message_pump_lock_event;
    system_thread_id     message_pump_thread_id;
    system_event         message_pump_unlock_event;

    system_event         window_initialized_event;
    system_event         window_safe_to_release_event;
    system_event         window_thread_event;

    system_resizable_vector exit_size_move_callbacks;            /* PFNWINDOWEXITSIZEMOVECALLBACKPROC       */
    system_resizable_vector mouse_move_callbacks;                /* PFNWINDOWMOUSEMOVECALLBACKPROC          */
    system_resizable_vector left_button_down_callbacks;          /* PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC     */
    system_resizable_vector left_button_up_callbacks;            /* PFNWINDOWLEFTBUTTONUPCALLBACKPROC       */
    system_resizable_vector left_button_double_click_callbacks;  /* PFNWINDOWLEFTBUTTONDBLCLKCALLBACKPROC   */
    system_resizable_vector middle_button_down_callbacks;        /* PFNWINDOWMIDDLEBUTTONDOWNCALLBACKPROC   */
    system_resizable_vector middle_button_up_callbacks;          /* PFNWINDOWMIDDLEBUTTONUPCALLBACKPROC     */
    system_resizable_vector middle_button_double_click_callbacks;/* PFNWINDOWMIDDLEBUTTONDBLCLKCALLBACKPROC */
    system_resizable_vector right_button_down_callbacks;         /* PFNWINDOWRIGHTBUTTONDOWNCALLBACKPROC    */
    system_resizable_vector right_button_up_callbacks;           /* PFNWINDOWRIGHTBUTTONUPCALLBACKPROC      */
    system_resizable_vector right_button_double_click_callbacks; /* PFNWINDOWRIGHTBUTTONDBLCLKCALLBACKPROC  */
    system_resizable_vector mouse_wheel_callbacks;               /* PFNWINDOWMOUSEWHEELCALLBACKPROC         */
    system_resizable_vector char_callbacks;                      /* PFNWINDOWCHARCALLBACKPROC               */
    system_resizable_vector key_down_callbacks;                  /* PFNWINDOWKEYDOWNCALLBACKPROC            */
    system_resizable_vector key_up_callbacks;                    /* PFNWINDOWKEYUPCALLBACKPROC              */
} _system_window;

/** Internal variables */
/** TODO */
#define WM_EMERALD_LOCK_MESSAGE_PUMP (WM_USER + 1)

/* Cached move cursor resource - loaded on first window spawned. */
HCURSOR cached_move_cursor_resource = 0;
/* Cached arrow cursor resource - loaded on first window spawned. */
HCURSOR cached_arrow_cursor_resource = 0;
/* Cached "horizontal resize" cursor resource - loaded on first window spawned. */
HCURSOR cached_horizontal_resize_cursor_resource = 0;
/* Cached "vertical resize" cursor resource - loaded on first window spawned. */
HCURSOR cached_vertical_resize_cursor_resource = 0;
/* Cached cross-hair cursor resource - loaded on first window spawned. */
HCURSOR cached_crosshair_cursor_resource = 0;
/* Cached hand cursor resource - loaded on first window spawned. */
HCURSOR cached_hand_cursor_resource = 0;
/* Cached "action forbidden" cursor resource - loaded on first window spawned. */
HCURSOR cached_action_forbidden_cursor_resource = 0;

/* Storage of spawned window handles (contains system_window items). This is used for automatic
 * context sharing upon window creation.
 *
 * When accessing, make sure you lock spawned_windows_cs critical section.
 */
system_resizable_vector spawned_windows = NULL;
/* Critical section for read/write access to spawned_windows. */
system_critical_section spawned_windows_cs = NULL;

/** Used to track amount of spawned windows. Also, if it is the first window that is created, window class is initialized.
 *  Always lock n_windows_spawned_cs before accessing (in order to defend against multiple window class creation and counter
 *  corruption
 */
int                     n_windows_spawned    = 0;
system_critical_section n_windows_spawned_cs = NULL;

/** TODO */
PRIVATE void _deinit_system_window(_system_window* descriptor)
{
    ASSERT_DEBUG_SYNC(!descriptor->is_message_pump_locked, "System window about to be deinited while message pump is locked!");

    system_event_release(descriptor->window_safe_to_release_event);
    system_event_release(descriptor->window_initialized_event);
    system_event_release(descriptor->message_pump_lock_event);
    system_event_release(descriptor->message_pump_unlock_event);
    system_event_release(descriptor->window_thread_event);

    /* Release callback descriptors */
    system_resizable_vector callback_vectors[] = { descriptor->exit_size_move_callbacks,
                                                   descriptor->mouse_move_callbacks,
                                                   descriptor->left_button_down_callbacks,
                                                   descriptor->left_button_up_callbacks,
                                                   descriptor->left_button_double_click_callbacks,
                                                   descriptor->middle_button_down_callbacks,
                                                   descriptor->middle_button_up_callbacks,
                                                   descriptor->middle_button_double_click_callbacks,
                                                   descriptor->right_button_down_callbacks,
                                                   descriptor->right_button_up_callbacks,
                                                   descriptor->right_button_double_click_callbacks,
                                                   descriptor->mouse_wheel_callbacks,
                                                   descriptor->char_callbacks,
                                                   descriptor->key_down_callbacks,
                                                   descriptor->key_up_callbacks};

    for (uint32_t n = 0; n < sizeof(callback_vectors) / sizeof(callback_vectors[0]); ++n)
    {
        while (system_resizable_vector_get_amount_of_elements(callback_vectors[n]) > 0)
        {
            _callback_descriptor* descriptor_ptr = NULL;

            if (system_resizable_vector_get_element_at(callback_vectors[n], 0, &descriptor_ptr) )
            {
                delete descriptor_ptr;

                system_resizable_vector_delete_element_at(callback_vectors[n], 0);
            }
        }

        system_resizable_vector_release(callback_vectors[n]);
    }

    if (descriptor->rendering_handler != NULL)
    {
        ogl_rendering_handler_release(descriptor->rendering_handler);

        descriptor->rendering_handler = NULL;
    }

    descriptor->window_safe_to_release_event = NULL;
    descriptor->window_initialized_event     = NULL;
    descriptor->message_pump_lock_event      = NULL;
    descriptor->message_pump_unlock_event    = NULL;
    descriptor->window_thread_event          = NULL;
}

/** TODO */
PRIVATE void _init_system_window(_system_window* descriptor)
{
    descriptor->fullscreen_bpp                                   = 0;
    descriptor->fullscreen_freq                                  = 0;
    descriptor->is_cursor_visible                                = false;
    descriptor->is_fullscreen                                    = false;
    descriptor->is_scalable                                      = false;
    descriptor->is_message_pump_locked                           = false;
    descriptor->multisampling_supported                          = false;
    descriptor->n_multisampling_samples                          = 0;
    descriptor->title                                            = system_hashed_ansi_string_get_default_empty_string();
    descriptor->window_mouse_cursor                              = SYSTEM_WINDOW_MOUSE_CURSOR_ARROW;
    descriptor->window_mouse_cursor_system_resource              = 0;
    descriptor->x1y1x2y2[0]                                      = 0;  
    descriptor->x1y1x2y2[1]                                      = 0;  
    descriptor->x1y1x2y2[2]                                      = 0;  
    descriptor->x1y1x2y2[3]                                      = 0;  
    descriptor->parent_window_handle                             = NULL;
    descriptor->rendering_handler                                = NULL;
    descriptor->system_dc                                        = NULL;
    descriptor->system_handle                                    = NULL;
    descriptor->system_ogl_context                               = NULL;
    descriptor->message_pump_lock_event                          = system_event_create(true, false);
    descriptor->message_pump_thread_id                           = 0;
    descriptor->message_pump_unlock_event                        = system_event_create(true, false);
    descriptor->window_safe_to_release_event                     = system_event_create(true, false);
    descriptor->window_initialized_event                         = system_event_create(true, false);
    descriptor->exit_size_move_callbacks                         = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->mouse_move_callbacks                             = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->left_button_down_callbacks                       = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->left_button_up_callbacks                         = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->left_button_double_click_callbacks               = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->middle_button_down_callbacks                     = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->middle_button_up_callbacks                       = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->middle_button_double_click_callbacks             = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->right_button_down_callbacks                      = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->right_button_up_callbacks                        = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->right_button_double_click_callbacks              = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->mouse_wheel_callbacks                            = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->char_callbacks                                   = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->key_down_callbacks                               = system_resizable_vector_create(1, sizeof(void*) );
    descriptor->key_up_callbacks                                 = system_resizable_vector_create(1, sizeof(void*) );

    #ifdef INCLUDE_WEBCAM_MANAGER
        descriptor->webcam_device_notification_handle       = NULL;
    #endif

    ASSERT_ALWAYS_SYNC(descriptor->window_safe_to_release_event != NULL, "Could not create safe-to-release event.");
    ASSERT_ALWAYS_SYNC(descriptor->window_initialized_event     != NULL, "Could not create window initialized event.");
}

/** TODO */
PRIVATE VOID _lock_system_window_message_pump(_system_window* descriptor)
{    
    /* Only lock if not in message pump */
    if (system_threads_get_thread_id() != descriptor->message_pump_thread_id)
    {
        system_event_reset(descriptor->message_pump_lock_event);
        system_event_reset(descriptor->message_pump_unlock_event);
        {
            ::PostMessageA(descriptor->system_handle, WM_EMERALD_LOCK_MESSAGE_PUMP, 0, 0);
            system_event_wait_single_infinite(descriptor->message_pump_lock_event);
        }
    }
}


PRIVATE VOID _unlock_system_window_message_pump(_system_window* descriptor)
{
    /* Only unlock if not in message pump */
    if (system_threads_get_thread_id() != descriptor->message_pump_thread_id)
    {
        ASSERT_DEBUG_SYNC(descriptor->is_message_pump_locked, "Cannot unlock system window message pump - not locked.");

        system_event_set(descriptor->message_pump_unlock_event);

        /* Wait until message pump confirms it has finished handling WM_EMERALD_LOCK_MESSAGE_PUMP msg */
        while (system_event_wait_single_peek(descriptor->message_pump_unlock_event) ){}
    }
}

/** TODO */
LRESULT CALLBACK _system_window_class_message_loop_entrypoint(HWND window_handle, UINT message_id, WPARAM wparam, LPARAM lparam)
{
    _system_window* window = (_system_window*) ::GetWindowLongPtr(window_handle, GWLP_USERDATA);

    switch (message_id)
    {
        case WM_EMERALD_LOCK_MESSAGE_PUMP:
        {
            window->is_message_pump_locked = true;

            system_event_set                 (window->message_pump_lock_event);
            system_event_wait_single_infinite(window->message_pump_unlock_event);

            window->is_message_pump_locked = false;

            system_event_reset(window->message_pump_unlock_event);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->mouse_move_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->mouse_move_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWMOUSEMOVECALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                       point.x,
                                                                                                       point.y, 
                                                                                                       (system_window_vk_status) wparam, 
                                                                                                       descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            if (window->window_mouse_cursor_system_resource != NULL)
            {
                ::SetCursor(window->window_mouse_cursor_system_resource);
            }

            break;
        }

        case WM_EXITSIZEMOVE:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->exit_size_move_callbacks);

            if (n_callbacks != 0)
            {
                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->exit_size_move_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWEXITSIZEMOVECALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_LBUTTONDOWN:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->left_button_down_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->left_button_down_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                            point.x,
                                                                                                            point.y, 
                                                                                                            (system_window_vk_status) wparam,
                                                                                                            descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_LBUTTONUP:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->left_button_up_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->left_button_up_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWLEFTBUTTONUPCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window,
                                                                                                          point.x,
                                                                                                          point.y,
                                                                                                          (system_window_vk_status) wparam, 
                                                                                                          descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_LBUTTONDBLCLK:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->left_button_double_click_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->left_button_double_click_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWLEFTBUTTONDBLCLKCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                              point.x, 
                                                                                                              point.y, 
                                                                                                              (system_window_vk_status) wparam,
                                                                                                              descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            /* WinAPI sucks and is far from reasonable. Most of our apps will expect click notifications and won't watch for double clicks. */
            n_callbacks = system_resizable_vector_get_amount_of_elements(window->left_button_down_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->left_button_down_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                            point.x,
                                                                                                            point.y, 
                                                                                                            (system_window_vk_status) wparam,
                                                                                                            descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_MBUTTONDOWN:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->middle_button_down_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->middle_button_down_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWMIDDLEBUTTONDOWNCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                              point.x,
                                                                                                              point.y, 
                                                                                                              (system_window_vk_status) wparam,
                                                                                                              descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_MBUTTONUP:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->middle_button_up_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos (&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->middle_button_up_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWMIDDLEBUTTONUPCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window,
                                                                                                            point.x,
                                                                                                            point.y,
                                                                                                            (system_window_vk_status) wparam, 
                                                                                                            descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_MBUTTONDBLCLK:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->middle_button_double_click_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->middle_button_double_click_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWMIDDLEBUTTONDBLCLKCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                                point.x, 
                                                                                                                point.y, 
                                                                                                                (system_window_vk_status) wparam,
                                                                                                                descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_RBUTTONDOWN:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->right_button_down_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->right_button_down_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWRIGHTBUTTONDOWNCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                             point.x,
                                                                                                             point.y, 
                                                                                                             (system_window_vk_status) wparam,
                                                                                                             descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_RBUTTONUP:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->right_button_up_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->right_button_up_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWRIGHTBUTTONUPCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window,
                                                                                                           point.x,
                                                                                                           point.y,
                                                                                                           (system_window_vk_status) wparam, 
                                                                                                           descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_RBUTTONDBLCLK:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->right_button_double_click_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->right_button_double_click_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWRIGHTBUTTONDBLCLKCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                               point.x, 
                                                                                                               point.y, 
                                                                                                               (system_window_vk_status) wparam,
                                                                                                               descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            break;
        }

        case WM_MOUSEWHEEL:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->mouse_wheel_callbacks);

            if (n_callbacks != 0)
            {
                POINT point;

                ::GetCursorPos(&point);

                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->mouse_wheel_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWMOUSEWHEELCALLBACKPROC) descriptor_ptr->pfn_callback) ( (system_window) window, 
                                                                                                         point.x, 
                                                                                                         point.y,
                                                                                                         GET_WHEEL_DELTA_WPARAM(wparam),
                                                                                                         (system_window_vk_status) LOWORD(wparam), 
                                                                                                         descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            return 0;
        }

        case WM_CHAR:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->char_callbacks);

            if (n_callbacks != 0)
            {
                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->char_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWCHARCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window,
                                                                                                  (unsigned short) wparam, 
                                                                                                  descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            return 0;
        }

        case WM_KEYDOWN:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->key_down_callbacks);

            if (n_callbacks != 0)
            {
                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->key_down_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWKEYDOWNCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                     (unsigned short) wparam, 
                                                                                                     descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            return 0;
        }

        case WM_KEYUP:
        {
            uint32_t n_callbacks = system_resizable_vector_get_amount_of_elements(window->key_up_callbacks);

            if (n_callbacks != 0)
            {
                for (uint32_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(window->key_up_callbacks, n_callback, &descriptor_ptr) )
                    {
                        bool result = ((PFNWINDOWKEYUPCALLBACKPROC) descriptor_ptr->pfn_callback)( (system_window) window, 
                                                                                                   (unsigned short) wparam, 
                                                                                                   descriptor_ptr->user_arg);

                        if (!result)
                        {
                            break;
                        }
                    }
                }
            }

            return 0;
        }

        case WM_CLOSE:
        {
            ::DestroyWindow(window_handle);

            return 0;
        }
        
        case WM_DESTROY:
        {
            ::PostQuitMessage(0);

            return 0;
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

    return ::DefWindowProcA(window_handle, message_id, wparam, lparam);
}

/** TODO */
PRIVATE void _system_window_thread_entrypoint(__notnull void* in_arg)
{
    DWORD           ex_style = 0;
    DWORD           style    = 0;
    _system_window* window   = (_system_window*) in_arg;

    window->message_pump_thread_id = system_threads_get_thread_id();

    /* Create window class, if necessary, and cache mouse cursors */
    {
        system_critical_section_enter(n_windows_spawned_cs);
        {
            if (n_windows_spawned == 0)
            {
                WNDCLASSA window_class;

                window_class.cbClsExtra    = 0;
                window_class.cbWndExtra    = 0;
                window_class.hbrBackground = 0;
                window_class.hCursor       = ::LoadCursorA(NULL, IDC_ARROW);
                window_class.hIcon         = ::LoadIconA  (NULL, IDI_WINLOGO);
                window_class.hInstance     = ::GetModuleHandleA(NULL);
                window_class.lpfnWndProc   = _system_window_class_message_loop_entrypoint;
                window_class.lpszClassName = EMERALD_WINDOW_CLASS_NAME;
                window_class.lpszMenuName  = NULL;
                window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;

                if (!::RegisterClassA(&window_class) )
                {
                    LOG_FATAL("Could not register engine window class.");

                    goto end;
                }
            }

            ++n_windows_spawned;

            /* Cache mouse cursor */
            cached_move_cursor_resource              = ::LoadCursorA(0, IDC_SIZEALL);
            cached_arrow_cursor_resource             = ::LoadCursorA(0, IDC_ARROW);
            cached_horizontal_resize_cursor_resource = ::LoadCursorA(0, IDC_SIZEWE);
            cached_vertical_resize_cursor_resource   = ::LoadCursorA(0, IDC_SIZENS);
            cached_crosshair_cursor_resource         = ::LoadCursorA(0, IDC_CROSS);
            cached_action_forbidden_cursor_resource  = ::LoadCursorA(0, IDC_NO);
            cached_hand_cursor_resource              = ::LoadCursorA(0, IDC_HAND);

            ASSERT_DEBUG_SYNC(cached_move_cursor_resource              != NULL, "Failed to load move cursor resource.");
            ASSERT_DEBUG_SYNC(cached_arrow_cursor_resource             != NULL, "Failed to load arrow cursor resource.");
            ASSERT_DEBUG_SYNC(cached_horizontal_resize_cursor_resource != NULL, "Failed to load horizontal resize cursor resource.");
            ASSERT_DEBUG_SYNC(cached_vertical_resize_cursor_resource   != NULL, "Failed to load vertical resize cursor resource.");
            ASSERT_DEBUG_SYNC(cached_crosshair_cursor_resource         != NULL, "Failed to load crosshair cursor resource.");
            ASSERT_DEBUG_SYNC(cached_action_forbidden_cursor_resource  != NULL, "Failed to load action forbidden cursor resource.");
            ASSERT_DEBUG_SYNC(cached_hand_cursor_resource              != NULL, "Failed to load hand cursor resource.");
        }
        system_critical_section_leave(n_windows_spawned_cs);
    }

    /* If full-screen window was requested, change display mode. */
    if (window->is_fullscreen)
    {
        DEVMODEA new_device_mode;

        memset(&new_device_mode, 0, sizeof(new_device_mode) );

        new_device_mode.dmBitsPerPel       = window->fullscreen_bpp;
        new_device_mode.dmDisplayFrequency = window->fullscreen_freq;
        new_device_mode.dmFields           = DM_BITSPERPEL | DM_DISPLAYFREQUENCY | DM_PELSHEIGHT | DM_PELSWIDTH;
        new_device_mode.dmPelsHeight       = window->x1y1x2y2[2] - window->x1y1x2y2[0];
        new_device_mode.dmPelsWidth        = window->x1y1x2y2[3] - window->x1y1x2y2[1];

        if (::ChangeDisplaySettingsA(&new_device_mode, CDS_FULLSCREEN) )
        {
            LOG_FATAL("Could not switch to full-screen for: width=%d height=%d freq=%d bpp=%d", 
                      window->x1y1x2y2[2] - window->x1y1x2y2[0],
                      window->x1y1x2y2[3] - window->x1y1x2y2[1], 
                      window->fullscreen_freq,
                      window->fullscreen_bpp);

            goto end;
        }

        /* Configure style bitfields accordingly. */
        ex_style = WS_EX_APPWINDOW;
        style    = WS_POPUP;
    }
    else
    {
        /* Non-full-screen window requested. Configure style bitfields accordingly. */
        if (window->parent_window_handle == NULL)
        {
            style = WS_POPUP;
        }
        else
        {
            style = WS_OVERLAPPED | WS_CHILD;
        }

        if (window->is_scalable)
        {
            style |= WS_THICKFRAME;
        }
    }

    /* Get system metrics we need to use for the window */
    int x_border_width = ::GetSystemMetrics(SM_CXSIZEFRAME);
    int y_border_width = ::GetSystemMetrics(SM_CYSIZEFRAME);
    int x1_delta       = 0;
    int y1_delta       = 0;

    if (!window->is_scalable)
    {
        x_border_width = 0;
        y_border_width = 0;
    }

    if (window->parent_window_handle != NULL)
    {
        x1_delta = -::GetSystemMetrics(SM_CXFRAME);
        y1_delta = (window->is_scalable ? -::GetSystemMetrics(SM_CYSIZEFRAME) : -::GetSystemMetrics(SM_CYFRAME) ) - ::GetSystemMetrics(SM_CYCAPTION);
    }

    window->system_handle = ::CreateWindowExA(ex_style,
                                              EMERALD_WINDOW_CLASS_NAME,
                                              system_hashed_ansi_string_get_buffer(window->title),
                                              style,
                                              window->x1y1x2y2[0] + x1_delta,
                                              window->x1y1x2y2[1] + y1_delta,
                                              window->x1y1x2y2[2] - window->x1y1x2y2[0] + x_border_width,
                                              window->x1y1x2y2[3] - window->x1y1x2y2[1] + y_border_width,
                                              window->parent_window_handle,
                                              NULL,                                       /* no menu */
                                              ::GetModuleHandleA(0),
                                              window);

    if (window->system_handle == NULL)
    {
        LOG_FATAL("Could not create window [%s]", system_hashed_ansi_string_get_buffer(window->title) );

        goto end;
    }
    
    #ifdef INCLUDE_WEBCAM_MANAGER
    {
        /* Register for system notifications on webcams */
        DEV_BROADCAST_DEVICEINTERFACE notification_filter = {0};

        notification_filter.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        notification_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        notification_filter.dbcc_classguid  = GUID_KSCATEGORY_CAPTURE;

        window->webcam_device_notification_handle = ::RegisterDeviceNotificationA(window->system_handle, &notification_filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    #endif /* INCLUDE_WEBCAM_MANAGER */

    window->system_dc = ::GetDC(window->system_handle);
    if (window->system_dc == NULL)
    {
        LOG_FATAL("Could not obtain device context for window [%s]", system_hashed_ansi_string_get_buffer(window->title) );

        goto end;
    }

    /* Create OGL context */
    ogl_pixel_format_descriptor pfd = ogl_pixel_format_descriptor_create(window->title, 8, 8, 8, 0, 8);

    if (pfd == NULL)
    {
        LOG_FATAL("Could not create pixel format descriptor for RGB8D8 format.");

        goto end;
    }
    
    /* Set up context sharing between all windows */
    system_critical_section_enter(spawned_windows_cs);
    {
        ogl_context reference_context = NULL;

        if (system_resizable_vector_get_amount_of_elements(spawned_windows) > 0)
        {
            system_window   reference_system_window = NULL;
            bool            result_get              = false;

            result_get = system_resizable_vector_get_element_at(spawned_windows, 0, &reference_system_window);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve reference system window.");

            _system_window* reference_system_window_ptr = (_system_window*) reference_system_window;
        
            reference_context = reference_system_window_ptr->system_ogl_context;
        }

        /* Create the new context now */
        window->system_ogl_context = ogl_context_create_from_system_window(window->title,
                                                                           window->context_type,
                                                                           (system_window) window,
                                                                           pfd,
                                                                           window->vsync_enabled,
                                                                           reference_context,
                                                                           window->multisampling_supported);

        if (window->system_ogl_context == NULL)
        {
            LOG_FATAL("Could not create OGL context for window [%s]", system_hashed_ansi_string_get_buffer(window->title) );

            goto end;
        }

        if (reference_context != NULL)
        {
            ogl_context_bind_to_current_thread(window->system_ogl_context);
        }
    }
    system_critical_section_leave(spawned_windows_cs);

    /* Bind the context to current thread */
    ogl_context_bind_to_current_thread(window->system_ogl_context);
    {
        /* Set up user data field for the window */
        ::SetWindowLongPtr(window->system_handle, GWLP_USERDATA, (LONG) window);

        /* Set up multisampling for the window */
        if (window->multisampling_supported)
        {
            ogl_context_set_multisampling(window->system_ogl_context, window->n_multisampling_samples);
        }
    }
    ogl_context_unbind_from_current_thread();

    /* Show the window if asked */
    if (window->visible)
    {
        ::ShowWindow(window->system_handle, SW_SHOW);
    }

    ::UpdateWindow(window->system_handle);

    /* All done, set the event so the creating thread can carry on */
    system_event_set(window->window_initialized_event);

    /* Pump messages */
    MSG  msg;
    BOOL returned_value;

    while( (returned_value = ::GetMessage(&msg, 0, 0, 0) != 0) )
    {
        if (returned_value == -1)
        {
            /* Wat */
            break;
        }

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

end:;
    if (pfd != NULL)
    {
        ogl_pixel_format_descriptor_release(pfd);
        pfd = NULL;
    }

    ogl_context_release_managers(window->system_ogl_context);

    /* Release rendering handler. This may be odd, but rendering handler retains a context and the context retains the rendering handler
     * so window rendering thread closure looks like one of good spots to insert a release request.
     **/
    if (window->rendering_handler != NULL)
    {
        ogl_rendering_handler_release(window->rendering_handler);
        ogl_rendering_handler_release(window->rendering_handler);

        window->rendering_handler = NULL;
    }

    /* Release OGL context */
    if (window->system_ogl_context != NULL)
    {
        ogl_context_release(window->system_ogl_context);
        window->system_ogl_context = NULL;
    }

    system_event_set(window->window_safe_to_release_event);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_add_callback_func(__in __notnull system_window                        window,
                                                        __in           system_window_callback_func_priority priority,
                                                        __in           system_window_callback_func          callback_func,
                                                        __in __notnull void*                                pfn_callback_func,
                                                        __in __notnull void*                                user_arg)
{
    _callback_descriptor* new_descriptor_ptr = new (std::nothrow) _callback_descriptor;
    bool                  result             = false;
    _system_window*       window_ptr         = (_system_window*) window;

    if (new_descriptor_ptr != NULL)
    {
        new_descriptor_ptr->pfn_callback = pfn_callback_func;
        new_descriptor_ptr->priority     = priority;
        new_descriptor_ptr->user_arg     = user_arg;

        _lock_system_window_message_pump(window_ptr);
        {
            /* NOTE: Modifying this? Update system_window_delete_callback_func() as well */
            system_resizable_vector callback_container = NULL;

            switch (callback_func)
            {
                case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:             callback_container = window_ptr->exit_size_move_callbacks,             result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE:                 callback_container = window_ptr->mouse_move_callbacks,                 result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN:           callback_container = window_ptr->left_button_down_callbacks,           result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP:             callback_container = window_ptr->left_button_up_callbacks,             result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK:   callback_container = window_ptr->left_button_double_click_callbacks,   result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN:         callback_container = window_ptr->middle_button_down_callbacks,         result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP:           callback_container = window_ptr->middle_button_up_callbacks,           result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK: callback_container = window_ptr->middle_button_double_click_callbacks, result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN:          callback_container = window_ptr->right_button_down_callbacks,          result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP:            callback_container = window_ptr->right_button_up_callbacks,            result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK:  callback_container = window_ptr->right_button_double_click_callbacks,  result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL:                callback_container = window_ptr->mouse_wheel_callbacks,                result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:                       callback_container = window_ptr->char_callbacks,                       result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:                     callback_container = window_ptr->key_up_callbacks,                     result = true;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:                   callback_container = window_ptr->key_down_callbacks,                   result = true;  break;
                default:                                                     LOG_ERROR("Unrecognized callback func requested [%d]", callback_func); result = false; break;
            }

            if (result)
            {
                /* Find a fitting place to insert the callback */
                size_t   insertion_index = -1;
                uint32_t n_elements      = system_resizable_vector_get_amount_of_elements(callback_container);

                for (uint32_t n_element = 0; n_element < n_elements; ++n_element)
                {
                    _callback_descriptor* existing_descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(callback_container, n_element, &existing_descriptor_ptr) )
                    {
                        if (existing_descriptor_ptr->priority > priority)
                        {
                            insertion_index = n_element;

                            break;
                        }
                    } /* if (system_resizable_vector_get_element_at(callback_container, n_element, &existing_descriptor_ptr) ) */
                } /* for (uint32_t n_element = 0; n_element < n_elements; ++n_element) */

                /* Insert */
                if (insertion_index == -1)
                {
                    system_resizable_vector_push(callback_container, new_descriptor_ptr);
                } /* if (insertion_index == -1) */
                else
                {
                    system_resizable_vector_insert_element_at(callback_container, insertion_index, new_descriptor_ptr);
                }
            } /* if (result) */
        }
        _unlock_system_window_message_pump(window_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_close(__in __notnull __deallocate(mem) system_window window)
{
    _system_window* window_ptr = (_system_window*) window;

    /* If there is a rendering handler and it is active, stop it before we continue */
    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_playback_status playback_status = ogl_rendering_handler_get_playback_status(window_ptr->rendering_handler);

        if (playback_status != RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED)
        {
            ogl_rendering_handler_stop(window_ptr->rendering_handler);
        }
    }

    #ifdef INCLUDE_WEBCAM_MANAGER
        /* If there is a notification handler installed, deinstall it now */
        if (window_ptr->webcam_device_notification_handle != NULL)
        {
            ::UnregisterDeviceNotification(window_ptr->webcam_device_notification_handle);

            window_ptr->webcam_device_notification_handle = NULL;
        }
    #endif /* INCLUDE_WEBCAM_MANAGER */
        
    ASSERT_DEBUG_SYNC(window_ptr->system_handle != NULL, "No window to close - system handle is NULL.");
    if (window_ptr->system_handle != NULL)
    {
        ::SendMessageA(window_ptr->system_handle, WM_CLOSE, 0, 0);
    }

    system_event_wait_single_infinite(window_ptr->window_safe_to_release_event);
    system_event_wait_single_infinite(window_ptr->window_thread_event);

    /* Remove the window handle from the 'spawned windows' vector */
    system_critical_section_enter(spawned_windows_cs);
    {
        size_t entry_index = system_resizable_vector_find(spawned_windows, window);

        ASSERT_DEBUG_SYNC(entry_index != ITEM_NOT_FOUND, "Handle of a window to be removed could not have been found in the 'spawned windows' vector.");
        if (entry_index != ITEM_NOT_FOUND)
        {
            if (!system_resizable_vector_delete_element_at(spawned_windows, entry_index) )
            {
                LOG_ERROR("Could not remove %dth element from 'spawned windows' vector", entry_index);
            }
        }
    }
    system_critical_section_leave(spawned_windows_cs);

    /* Release the descriptor */
    _deinit_system_window(window_ptr);

    delete window_ptr;

    return true;
}

/** TODO */
PRIVATE system_window _system_window_create_shared(__in __notnull             ogl_context_type          context_type,
                                                   __in                       bool                      is_fullscreen,
                                                   __in __notnull __ecount(4) const int*                x1y1x2y2,
                                                   __in __notnull             system_hashed_ansi_string title,
                                                   __in __notnull             bool                      is_scalable,
                                                   __in __notnull             uint16_t                  n_multisampling_samples,
                                                   __in                       uint16_t                  fullscreen_bpp,
                                                   __in                       uint16_t                  fullscreen_freq,
                                                   __in                       bool                      vsync_enabled,
                                                   __in __maybenull           HWND                      parent_window_handle,
                                                   __in                       bool                      multisampling_supported,
                                                   __in                       bool                      visible)
{
    _system_window* new_window = new (std::nothrow) _system_window;

    ASSERT_ALWAYS_SYNC(new_window != NULL, "Out of memory while creating system window instance");
    if (new_window != NULL)
    {
        _init_system_window(new_window);

        /* Fill the descriptor with input values */
        new_window->context_type            = context_type;
        new_window->fullscreen_bpp          = fullscreen_bpp;
        new_window->fullscreen_freq         = fullscreen_freq;
        new_window->x1y1x2y2[0]             = x1y1x2y2[0];
        new_window->x1y1x2y2[1]             = x1y1x2y2[1];
        new_window->x1y1x2y2[2]             = x1y1x2y2[2];
        new_window->x1y1x2y2[3]             = x1y1x2y2[3];
        new_window->is_cursor_visible       = false;
        new_window->is_fullscreen           = is_fullscreen;
        new_window->is_scalable             = is_scalable;
        new_window->multisampling_supported = multisampling_supported;
        new_window->n_multisampling_samples = n_multisampling_samples;
        new_window->parent_window_handle    = parent_window_handle;
        new_window->title                   = title;
        new_window->visible                 = visible;
        new_window->vsync_enabled           = vsync_enabled;

        if (new_window->window_safe_to_release_event != NULL && new_window->window_initialized_event != NULL)
        {
            /* Spawn window thread */
            system_threads_spawn(_system_window_thread_entrypoint, new_window, &new_window->window_thread_event);

            /* Wait until window finishes initialization */
            system_event_wait_single_infinite(new_window->window_initialized_event);

            /* Insert the window handle into the 'spawned windows' vector */
            system_critical_section_enter(spawned_windows_cs);
            {
                system_resizable_vector_push(spawned_windows, new_window);
            }
            system_critical_section_leave(spawned_windows_cs);

            /* We're done */
            LOG_INFO("Window [title=<%s>] initialized.", system_hashed_ansi_string_get_buffer(title) );
        }
        else
        {
            delete new_window;
            new_window = NULL;
        }
    }


    return (system_window) new_window;
}


/** Please see header for specification */
PUBLIC EMERALD_API system_window system_window_create_by_replacing_window(__in system_hashed_ansi_string name,
                                                                          __in ogl_context_type          context_type,
                                                                          __in uint16_t                  n_multisampling_samples,
                                                                          __in bool                      vsync_enabled,
                                                                          __in system_window_handle      parent_window_handle,
                                                                          __in bool                      multisampling_supported)
{
    BOOL          boolean_result     = FALSE;
    RECT          parent_window_rect;
    RECT          grandparent_window_rect;
    system_window result             = NULL;
    int           x1y1x2y2[4]        = {0};

    boolean_result  = ::GetWindowRect(parent_window_handle,              &parent_window_rect);
    boolean_result &= ::GetWindowRect(::GetParent(parent_window_handle), &grandparent_window_rect);

    ASSERT_ALWAYS_SYNC(boolean_result == TRUE, "Could not retrieve window rectangle for parent window handle [%x]", parent_window_handle);
    if (boolean_result)
    {
        x1y1x2y2[0] = parent_window_rect.left   - grandparent_window_rect.left;
        x1y1x2y2[1] = parent_window_rect.top    - grandparent_window_rect.top;
        x1y1x2y2[2] = parent_window_rect.right  - grandparent_window_rect.left;
        x1y1x2y2[3] = parent_window_rect.bottom - grandparent_window_rect.top;

        result = _system_window_create_shared(context_type,
                                              false,  /* not fullscreen */
                                              x1y1x2y2, 
                                              name,
                                              false, /* not scalable */
                                              n_multisampling_samples,
                                              0,
                                              0,
                                              vsync_enabled,
                                              ::GetParent(parent_window_handle),
                                              multisampling_supported,
                                              true /* visible */);

    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_window system_window_create_not_fullscreen(__in                       ogl_context_type          context_type,
                                                                     __in __notnull __ecount(4) const int*                x1y1x2y2,
                                                                     __in __notnull             system_hashed_ansi_string title,
                                                                     __in                       bool                      scalable,
                                                                     __in                       uint16_t                  n_multisampling_samples,
                                                                     __in                       bool                      vsync_enabled,
                                                                     __in                       bool                      multisampling_supported,
                                                                     __in                       bool                      visible)
{
    return _system_window_create_shared(context_type,
                                        false,
                                        x1y1x2y2,
                                        title,
                                        scalable,
                                        n_multisampling_samples,
                                        0,
                                        0,
                                        vsync_enabled,
                                        HWND_DESKTOP,
                                        multisampling_supported,
                                        visible);
}

/** Please see header for specification */
PUBLIC EMERALD_API system_window system_window_create_fullscreen(__in ogl_context_type context_type,
                                                                 __in uint16_t         width,
                                                                 __in uint16_t         height,
                                                                 __in uint16_t         bpp,
                                                                 __in uint16_t         freq,
                                                                 __in uint16_t         n_multisampling_samples,
                                                                 __in bool             vsync_enabled,
                                                                 __in bool             multisampling_supported)
{
    const int x1y1x2y2[4] = {0, 0, width, height};

    return _system_window_create_shared(context_type,
                                        true,
                                        x1y1x2y2,
                                        system_hashed_ansi_string_get_default_empty_string(),
                                        false,
                                        n_multisampling_samples,
                                        bpp,
                                        freq,
                                        vsync_enabled,
                                        HWND_DESKTOP,
                                        multisampling_supported,
                                        true);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_delete_callback_func(__in __notnull system_window               window_instance,
                                                           __in           system_window_callback_func callback_func,
                                                           __in __notnull void*                       pfn_callback_func,
                                                           __in __notnull void*                       user_arg)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window_instance;

    if (window_ptr != NULL)
    {
        system_resizable_vector callbacks_container = NULL;

        _lock_system_window_message_pump(window_ptr);
        {
            /* NOTE: Modifying this? Update system_window_add_callback_func() as well */
            switch (callback_func)
            {
                case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:             callbacks_container = window_ptr->exit_size_move_callbacks;             break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE:                 callbacks_container = window_ptr->mouse_move_callbacks;                 break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN:           callbacks_container = window_ptr->left_button_down_callbacks;           break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP:             callbacks_container = window_ptr->left_button_up_callbacks;             break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK:   callbacks_container = window_ptr->left_button_double_click_callbacks;   break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN:         callbacks_container = window_ptr->middle_button_down_callbacks;         break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP:           callbacks_container = window_ptr->middle_button_up_callbacks;           break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK: callbacks_container = window_ptr->middle_button_double_click_callbacks; break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN:          callbacks_container = window_ptr->right_button_down_callbacks;          break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP:            callbacks_container = window_ptr->right_button_up_callbacks;            break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK:  callbacks_container = window_ptr->right_button_double_click_callbacks;  break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL:                callbacks_container = window_ptr->mouse_wheel_callbacks;                break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:                       callbacks_container = window_ptr->char_callbacks;                       break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:                     callbacks_container = window_ptr->key_up_callbacks;                     break;
                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:                   callbacks_container = window_ptr->key_down_callbacks;                   break;
                default:                                                     LOG_ERROR("Unrecognized callback func requested [%d]", callback_func);  break;
            }

            if (callbacks_container != NULL)
            {
                size_t n_callbacks = system_resizable_vector_get_amount_of_elements(callbacks_container);

                for (size_t n_callback = 0; n_callback < n_callbacks; ++n_callback)
                {
                    _callback_descriptor* callback_ptr = NULL;

                    if (system_resizable_vector_get_element_at(callbacks_container, n_callback, &callback_ptr) )
                    {
                        if (callback_ptr->pfn_callback == pfn_callback_func && callback_ptr->user_arg == user_arg)
                        {
                            system_resizable_vector_delete_element_at(callbacks_container, n_callback);

                            delete callback_ptr;
                            callback_ptr = NULL;

                            result = true;
                            break;
                        } /* if (callback_ptr->pfn_callback == pfn_callback_func && callback_ptr->user_arg == user_arg) */
                    } /* if (system_resizable_vector_get_element_at(callbacks_container, n_callback, &callback_ptr) ) */
                } /* for (size_t n_callback = 0; n_callback < n_callbacks; ++n_callback) */

                if (!result)
                {
                    LOG_ERROR("Could not delete callback function - corresponding descriptor was not found.");
                } /* if (!result) */
            } /* if (callbacks_container != NULL) */
        }
        _unlock_system_window_message_pump(window_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_centered_window_position_for_primary_monitor(__in_ecount(2)  __notnull const int* dimensions, __out_ecount(4) __notnull int* result_dimensions)
{
    int fullscreen_x = ::GetSystemMetrics(SM_CXFULLSCREEN);
    int fullscreen_y = ::GetSystemMetrics(SM_CYFULLSCREEN);

    if (fullscreen_x >= dimensions[0] && fullscreen_y >= dimensions[1])
    {
        result_dimensions[0] = (fullscreen_x - dimensions[0]) >> 1;
        result_dimensions[1] = (fullscreen_y - dimensions[1]) >> 1;
        result_dimensions[2] = (fullscreen_x + dimensions[0]) >> 1;
        result_dimensions[3] = (fullscreen_y + dimensions[1]) >> 1;

        return true;
    }
    else
    {
        return false;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_handle(__in  __notnull system_window window, __out __notnull system_window_handle* out_window_handle)
{
    _system_window* window_ptr = (_system_window*) window;

    ASSERT_DEBUG_SYNC(window_ptr->system_handle != 0, "System handle is NULL.");
    *out_window_handle = window_ptr->system_handle;

    return true;
}

/** TODO */
PUBLIC EMERALD_API bool system_window_get_position(__in  __notnull system_window window, __out __notnull int* out_x, __out __notnull int* out_y)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window;

    ASSERT_DEBUG_SYNC(window_ptr->system_handle != 0, "System handle is NULL.");
    
    if (window_ptr->system_handle != 0)
    {
        RECT window_rect;

        if (::GetWindowRect(window_ptr->system_handle, &window_rect) == TRUE)
        {
            *out_x = window_rect.left;
            *out_y = window_rect.top;
            result = true;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_rendering_handler(__in  __notnull system_window window, __out __notnull ogl_rendering_handler* out_rendering_handler)
{
    *out_rendering_handler = ((_system_window*) window)->rendering_handler;

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_dc(__in  __notnull system_window window, __out __notnull system_window_dc* out_window_dc)
{
    _system_window* window_ptr = (_system_window*) window;

    ASSERT_DEBUG_SYNC(window_ptr->system_dc != 0, "System DC is NULL.");
    *out_window_dc = window_ptr->system_dc;

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_dimensions(__in __notnull system_window window, __out_ecount(1) __notnull int* out_width, __out_ecount(1) __notnull int* out_height)
{
    _system_window* window_ptr = (_system_window*) window;

    *out_width  = window_ptr->x1y1x2y2[2] - window_ptr->x1y1x2y2[0];
    *out_height = window_ptr->x1y1x2y2[3] - window_ptr->x1y1x2y2[1];

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string system_window_get_name(__in __notnull system_window window)
{
    return ((_system_window*)window)->title;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_context(__in  __notnull system_window window, __out __notnull ogl_context* out_context)
{
    *out_context = ((_system_window*)window)->system_ogl_context;

    return (*out_context != NULL);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_set_cursor(__in __notnull system_window window, system_window_mouse_cursor cursor)
{
    _system_window* window_ptr = (_system_window*) window;
    bool            result     = true;

    _lock_system_window_message_pump(window_ptr);
    {
        switch (cursor)
        {
            case SYSTEM_WINDOW_MOUSE_CURSOR_MOVE:
            {
                window_ptr->window_mouse_cursor                 = cursor;
                window_ptr->window_mouse_cursor_system_resource = cached_move_cursor_resource;

                break;
            }

            case SYSTEM_WINDOW_MOUSE_CURSOR_ARROW:
            {
                window_ptr->window_mouse_cursor                 = cursor;
                window_ptr->window_mouse_cursor_system_resource = cached_arrow_cursor_resource;

                break;
            }

            case SYSTEM_WINDOW_MOUSE_CURSOR_HORIZONTAL_RESIZE:
            {
                window_ptr->window_mouse_cursor                 = cursor;
                window_ptr->window_mouse_cursor_system_resource = cached_horizontal_resize_cursor_resource;

                break;
            }

            case SYSTEM_WINDOW_MOUSE_CURSOR_VERTICAL_RESIZE:
            {
                window_ptr->window_mouse_cursor                 = cursor;
                window_ptr->window_mouse_cursor_system_resource = cached_vertical_resize_cursor_resource;

                break;
            }

            case SYSTEM_WINDOW_MOUSE_CURSOR_CROSSHAIR:
            {
                window_ptr->window_mouse_cursor                 = cursor;
                window_ptr->window_mouse_cursor_system_resource = cached_crosshair_cursor_resource;

                break;
            }

            case SYSTEM_WINDOW_MOUSE_CURSOR_ACTION_FORBIDDEN:
            {
                window_ptr->window_mouse_cursor                 = cursor;
                window_ptr->window_mouse_cursor_system_resource = cached_action_forbidden_cursor_resource;

                break;
            }

            default:
            {
                LOG_ERROR("Unrecognized cursor type [%d] requested", cursor);

                result = false;
            }
        }
    }
    _unlock_system_window_message_pump(window_ptr);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_set_position(__in __notnull system_window window, __in __notnull int x, __in __notnull int y)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window;

    if (window_ptr->system_handle != NULL)
    {
        ::SetWindowPos(window_ptr->system_handle, 
                       window_ptr->parent_window_handle,
                       x,
                       y,
                       0,
                       0,
                       SWP_NOSIZE);

        /* Update internal representation */
        int width  = window_ptr->x1y1x2y2[2] - window_ptr->x1y1x2y2[0];
        int height = window_ptr->x1y1x2y2[3] - window_ptr->x1y1x2y2[1];

        window_ptr->x1y1x2y2[0] = x;
        window_ptr->x1y1x2y2[1] = y;
        window_ptr->x1y1x2y2[2] = x + width;
        window_ptr->x1y1x2y2[3] = y + height;
        
        /* Done */
        result = true;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_set_rendering_handler(__in __notnull system_window window, __in __notnull ogl_rendering_handler rendering_handler)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window;
    
    ASSERT_DEBUG_SYNC(window_ptr->rendering_handler == NULL, "Cannot set a new rendering handler to the window - already set!");
    ASSERT_DEBUG_SYNC(rendering_handler             != NULL, "Cannot set a null rendering buffer!");

    if (window_ptr->rendering_handler == NULL && rendering_handler != NULL)
    {
        window_ptr->rendering_handler = rendering_handler;
        result                        = true;

        ogl_rendering_handler_retain(window_ptr->rendering_handler);
        _ogl_rendering_handler_on_bound_to_context(window_ptr->rendering_handler, ((_system_window*)window)->system_ogl_context);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_set_size(__in __notnull system_window window, __in int width, __in int height)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window;

    if (window_ptr->system_handle != NULL)
    {
        result = (::MoveWindow(window_ptr->system_handle,
                               window_ptr->x1y1x2y2[0] - (window_ptr->is_scalable ? ::GetSystemMetrics(SM_CXSIZEFRAME) : ::GetSystemMetrics(SM_CXFRAME) ),
                               window_ptr->x1y1x2y2[1] - (window_ptr->is_scalable ? ::GetSystemMetrics(SM_CYSIZEFRAME) : ::GetSystemMetrics(SM_CYFRAME) ) - ::GetSystemMetrics(SM_CYCAPTION),
                               width,
                               height,
                               TRUE) == TRUE);
                                 
        /* Update internal representation */
        window_ptr->x1y1x2y2[2] = window_ptr->x1y1x2y2[0] + width;
        window_ptr->x1y1x2y2[3] = window_ptr->x1y1x2y2[1] + height;

    }

    return result;
}

/** Please see header for specification */
PUBLIC void _system_window_init()
{
    spawned_windows      = system_resizable_vector_create(BASE_WINDOW_STORAGE, sizeof(system_window) );
    spawned_windows_cs   = system_critical_section_create();
    n_windows_spawned_cs = system_critical_section_create();
}

/** Please see header for specification */
PUBLIC void _system_window_deinit()
{
    system_resizable_vector_release(spawned_windows);
    system_critical_section_release(spawned_windows_cs);
    system_critical_section_release(n_windows_spawned_cs);
}
