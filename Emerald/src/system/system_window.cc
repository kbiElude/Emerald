/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_types.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"
#include "system/system_threads.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

#ifdef _WIN32
    #include "system/system_window_win32.h"
#else
    #include "system/system_window_linux.h"
#endif

#ifdef INCLUDE_WEBCAM_MANAGER
    #include <Dbt.h>
    #include "webcam/webcam_manager.h"

    #include <INITGUID.H>
    DEFINE_GUID( GUID_DEVCLASS_IMAGE, 0x6bdd1fc6L, 0x810f, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f );
    DEFINE_GUID( GUID_KSCATEGORY_CAPTURE, 0x65E8773D, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
#endif /* INCLUDE_WEBCAM_MANAGER */

/** Internal type definitions  */
#ifdef _WIN32
    typedef system_window_win32 system_window_platform;
#else
    typedef system_window_linux system_window_platform;
#endif

typedef void (*PFNWINDOWCLOSEWINDOWPROC) (system_window_platform window);
typedef void (*PFNWINDOWDEINITWINDOWPROC)(system_window_platform window);
typedef bool (*PFNWINDOWGETPROPERTYPROC) (system_window_platform window,
                                          system_window_property property,
                                          void*                  out_result);
typedef void (*PFNWINDOWHANDLEWINDOWPROC)(system_window_platform window);
typedef bool (*PFNWINDOWOPENWINDOWPROC)  (system_window_platform window,
                                          bool                   is_first_window);
typedef bool (*PFNWINDOWSETPROPERTYPROC) (system_window_platform window,
                                          system_window_property property,
                                          const void*            data);
typedef struct
{
    void*                                pfn_callback;
    system_window_callback_func_priority priority;
    void*                                user_arg;
} _callback_descriptor;

typedef struct
{
    audio_stream               audio_strm;
    ogl_context_type           context_type;
    bool                       is_closing; /* in the process of calling the "window closing" call-backs */
    bool                       is_cursor_visible;
    bool                       is_fullscreen;
    bool                       is_root_window;
    bool                       is_scalable;
    system_window_handle       parent_window_handle;
    system_screen_mode         screen_mode;
    system_hashed_ansi_string  title;
    bool                       visible;
    bool                       vsync_enabled;
    system_window_mouse_cursor window_mouse_cursor;
    int                        x1y1x2y2[4];

    ogl_rendering_handler rendering_handler;

    #ifdef INCLUDE_WEBCAM_MANAGER
        HDEVNOTIFY webcam_device_notification_handle;
    #endif

    system_pixel_format pf;
    ogl_context         system_ogl_context;

    system_event         window_initialized_event;
    system_event         window_safe_to_release_event;
    system_event         window_thread_event;

    system_critical_section callbacks_cs;
    system_resizable_vector char_callbacks;                      /* PFNWINDOWCHARCALLBACKPROC               */
    system_resizable_vector exit_size_move_callbacks;            /* PFNWINDOWEXITSIZEMOVECALLBACKPROC       */
    system_resizable_vector key_down_callbacks;                  /* PFNWINDOWKEYDOWNCALLBACKPROC            */
    system_resizable_vector key_up_callbacks;                    /* PFNWINDOWKEYUPCALLBACKPROC              */
    system_resizable_vector left_button_down_callbacks;          /* PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC     */
    system_resizable_vector left_button_up_callbacks;            /* PFNWINDOWLEFTBUTTONUPCALLBACKPROC       */
    system_resizable_vector left_button_double_click_callbacks;  /* PFNWINDOWLEFTBUTTONDBLCLKCALLBACKPROC   */
    system_resizable_vector middle_button_down_callbacks;        /* PFNWINDOWMIDDLEBUTTONDOWNCALLBACKPROC   */
    system_resizable_vector middle_button_up_callbacks;          /* PFNWINDOWMIDDLEBUTTONUPCALLBACKPROC     */
    system_resizable_vector middle_button_double_click_callbacks;/* PFNWINDOWMIDDLEBUTTONDBLCLKCALLBACKPROC */
    system_resizable_vector mouse_move_callbacks;                /* PFNWINDOWMOUSEMOVECALLBACKPROC          */
    system_resizable_vector mouse_wheel_callbacks;               /* PFNWINDOWMOUSEWHEELCALLBACKPROC         */
    system_resizable_vector right_button_down_callbacks;         /* PFNWINDOWRIGHTBUTTONDOWNCALLBACKPROC    */
    system_resizable_vector right_button_up_callbacks;           /* PFNWINDOWRIGHTBUTTONUPCALLBACKPROC      */
    system_resizable_vector right_button_double_click_callbacks; /* PFNWINDOWRIGHTBUTTONDBLCLKCALLBACKPROC  */
    system_resizable_vector window_closed_callbacks;             /* PFNWINDOWINDOWCLOSEDCALLBACKPROC        */
    system_resizable_vector window_closing_callbacks;            /* PFNWINDOWWINDOWCLOSINGCALLBACKPROC      */

    #ifdef _WIN32
        system_window_win32 window_platform;
    #else
        system_window_linux window_platform;
    #endif

    /* Platform-specific entry-points */
    PFNWINDOWCLOSEWINDOWPROC  pfn_window_close_window;
    PFNWINDOWDEINITWINDOWPROC pfn_window_deinit_window;
    PFNWINDOWGETPROPERTYPROC  pfn_window_get_property;
    PFNWINDOWHANDLEWINDOWPROC pfn_window_handle_window;
    PFNWINDOWOPENWINDOWPROC   pfn_window_open_window;
    PFNWINDOWSETPROPERTYPROC  pfn_window_set_property;
} _system_window;

/** Internal variables */

/* Storage of spawned window handles (contains system_window items). This is used for automatic
 * context sharing upon window creation.
 *
 * When accessing, make sure you lock spawned_windows_cs critical section.
 */
system_resizable_vector spawned_windows = NULL;
/* Critical section for read/write access to spawned_windows. */
system_critical_section spawned_windows_cs = NULL;

/** TODO. Only use when n_total_windows_spawned_cs is locked */
unsigned int            n_total_windows_spawned    = 0;
system_critical_section n_total_windows_spawned_cs = NULL;

/* Root window - holds root rendering context which encapsulates buffer objects, program objects, texutre objects, etc. that
 * all other rendering contexts may use.
 *
 * Deleted when n_windows_spawned drops to 0.
 */
ogl_rendering_handler root_window_rendering_handler = NULL;
system_window         root_window                   = NULL;


/* Forward declarations */
PRIVATE void          _deinit_system_window                                    (_system_window*                      descriptor);
PRIVATE void          _init_system_window                                      (_system_window*                      descriptor);
PRIVATE volatile void _system_window_teardown_thread_pool_callback             (system_thread_pool_callback_argument arg);
PRIVATE void          _system_window_thread_entrypoint                         (void*                                in_arg);
PRIVATE void          _system_window_create_root_window                        (ogl_context_type                     context_type);
PRIVATE system_window _system_window_create_shared                             (ogl_context_type                     context_type,
                                                                                bool                                 is_fullscreen,
                                                                                const int*                           x1y1x2y2,
                                                                                system_screen_mode                   screen_mode,
                                                                                system_hashed_ansi_string            title,
                                                                                bool                                 is_scalable,
                                                                                bool                                 vsync_enabled,
                                                                                system_window_handle                 parent_window_handle,
                                                                                bool                                 visible,
                                                                                bool                                 is_root_window,
                                                                                system_pixel_format                  pf);
PRIVATE void          _system_window_window_closing_rendering_thread_entrypoint(ogl_context                          context,
                                                                                void*                                user_arg);

/** TODO */
PRIVATE void _deinit_system_window(_system_window* window_ptr)
{
    if (window_ptr->callbacks_cs != NULL)
    {
        system_critical_section_release(window_ptr->callbacks_cs);

        window_ptr->callbacks_cs = NULL;
    }

    if (window_ptr->pf != NULL)
    {
        system_pixel_format_release(window_ptr->pf);

        window_ptr->pf = NULL;
    }

    if (window_ptr->window_safe_to_release_event != NULL)
    {
        system_event_release(window_ptr->window_safe_to_release_event);

        window_ptr->window_safe_to_release_event = NULL;
    }

    if (window_ptr->window_initialized_event != NULL)
    {
        system_event_release(window_ptr->window_initialized_event);

        window_ptr->window_initialized_event = NULL;
    }

    if (window_ptr->window_thread_event != NULL)
    {
        system_event_release(window_ptr->window_thread_event);

        window_ptr->window_thread_event = NULL;
    }

    /* Release callback descriptors */
    system_resizable_vector callback_vectors[] =
    {
        window_ptr->char_callbacks,
        window_ptr->exit_size_move_callbacks,
        window_ptr->key_down_callbacks,
        window_ptr->key_up_callbacks,
        window_ptr->left_button_double_click_callbacks,
        window_ptr->left_button_down_callbacks,
        window_ptr->left_button_up_callbacks,
        window_ptr->middle_button_double_click_callbacks,
        window_ptr->middle_button_down_callbacks,
        window_ptr->middle_button_up_callbacks,
        window_ptr->mouse_move_callbacks,
        window_ptr->mouse_wheel_callbacks,
        window_ptr->right_button_double_click_callbacks,
        window_ptr->right_button_down_callbacks,
        window_ptr->right_button_up_callbacks,
        window_ptr->window_closed_callbacks,
        window_ptr->window_closing_callbacks
    };

    for (uint32_t n = 0;
                  n < sizeof(callback_vectors) / sizeof(callback_vectors[0]);
                ++n)
    {
        unsigned int n_callbacks = 0;

        system_resizable_vector_get_property(callback_vectors[n],
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_callbacks);

        while (n_callbacks > 0)
        {
            _callback_descriptor* descriptor_ptr = NULL;

            if (system_resizable_vector_get_element_at(callback_vectors[n],
                                                       0,
                                                      &descriptor_ptr) )
            {
                delete descriptor_ptr;

                system_resizable_vector_delete_element_at(callback_vectors[n],
                                                          0);
            }

            system_resizable_vector_get_property(callback_vectors[n],
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_callbacks);
        }

        system_resizable_vector_release(callback_vectors[n]);
    }

    if (window_ptr->pf != NULL)
    {
        system_pixel_format_release(window_ptr->pf);

        window_ptr->pf = NULL;
    }

    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_release(window_ptr->rendering_handler);

        window_ptr->rendering_handler = NULL;
    }

    /* At this point it should be safe to release the audio stream */
    if (window_ptr->audio_strm != NULL)
    {
        audio_stream_release(window_ptr->audio_strm);

        window_ptr->audio_strm = NULL;
    }
}

/** TODO */
PRIVATE void _init_system_window(_system_window* window_ptr)
{
    window_ptr->audio_strm                   = NULL;
    window_ptr->is_cursor_visible            = false;
    window_ptr->is_fullscreen                = false;
    window_ptr->is_scalable                  = false;
    window_ptr->pf                           = NULL;
    window_ptr->title                        = system_hashed_ansi_string_get_default_empty_string();
    window_ptr->window_mouse_cursor          = SYSTEM_WINDOW_MOUSE_CURSOR_ARROW;
    window_ptr->x1y1x2y2[0]                  = 0;
    window_ptr->x1y1x2y2[1]                  = 0;
    window_ptr->x1y1x2y2[2]                  = 0;
    window_ptr->x1y1x2y2[3]                  = 0;
    window_ptr->parent_window_handle         = (system_window_handle) NULL;
    window_ptr->rendering_handler            = NULL;
    window_ptr->system_ogl_context           = NULL;
    window_ptr->window_safe_to_release_event = system_event_create(true); /* manual_reset */
    window_ptr->window_initialized_event     = system_event_create(true); /* manual_reset */

    window_ptr->callbacks_cs                         = system_critical_section_create();
    window_ptr->char_callbacks                       = system_resizable_vector_create(1);
    window_ptr->exit_size_move_callbacks             = system_resizable_vector_create(1);
    window_ptr->key_down_callbacks                   = system_resizable_vector_create(1);
    window_ptr->key_up_callbacks                     = system_resizable_vector_create(1);
    window_ptr->left_button_double_click_callbacks   = system_resizable_vector_create(1);
    window_ptr->left_button_down_callbacks           = system_resizable_vector_create(1);
    window_ptr->left_button_up_callbacks             = system_resizable_vector_create(1);
    window_ptr->middle_button_double_click_callbacks = system_resizable_vector_create(1);
    window_ptr->middle_button_down_callbacks         = system_resizable_vector_create(1);
    window_ptr->middle_button_up_callbacks           = system_resizable_vector_create(1);
    window_ptr->mouse_move_callbacks                 = system_resizable_vector_create(1);
    window_ptr->mouse_wheel_callbacks                = system_resizable_vector_create(1);
    window_ptr->right_button_double_click_callbacks  = system_resizable_vector_create(1);
    window_ptr->right_button_down_callbacks          = system_resizable_vector_create(1);
    window_ptr->right_button_up_callbacks            = system_resizable_vector_create(1);
    window_ptr->window_closed_callbacks              = system_resizable_vector_create(1);
    window_ptr->window_closing_callbacks             = system_resizable_vector_create(1);

    #ifdef INCLUDE_WEBCAM_MANAGER
        window_ptr->webcam_device_notification_handle = NULL;
    #endif

    #ifdef _WIN32
        window_ptr->pfn_window_close_window  = system_window_win32_close_window;
        window_ptr->pfn_window_deinit_window = system_window_win32_deinit;
        window_ptr->pfn_window_get_property  = system_window_win32_get_property;
        window_ptr->pfn_window_handle_window = system_window_win32_handle_window;
        window_ptr->pfn_window_open_window   = system_window_win32_open_window;
        window_ptr->pfn_window_set_property  = system_window_win32_set_property;

        window_ptr->window_platform = system_window_win32_init( (system_window) window_ptr);
    #else
        window_ptr->pfn_window_close_window  = (PFNWINDOWCLOSEWINDOWPROC)  system_window_linux_close_window;
        window_ptr->pfn_window_deinit_window = (PFNWINDOWDEINITWINDOWPROC) system_window_linux_deinit;
        window_ptr->pfn_window_get_property  = (PFNWINDOWGETPROPERTYPROC)  system_window_linux_get_property;
        window_ptr->pfn_window_handle_window = (PFNWINDOWHANDLEWINDOWPROC) system_window_linux_handle_window;
        window_ptr->pfn_window_open_window   = (PFNWINDOWOPENWINDOWPROC)   system_window_linux_open_window;
        window_ptr->pfn_window_set_property  = (PFNWINDOWSETPROPERTYPROC)  system_window_linux_set_property;

        window_ptr->window_platform = system_window_linux_init( (system_window) window_ptr);
    #endif

    ASSERT_ALWAYS_SYNC(window_ptr->window_safe_to_release_event != NULL,
                       "Could not create safe-to-release event.");
    ASSERT_ALWAYS_SYNC(window_ptr->window_initialized_event != NULL,
                       "Could not create window initialized event.");
}


/** TODO */
PRIVATE void _system_window_thread_entrypoint(void* in_arg)
{
    _system_window* window_ptr = (_system_window*) in_arg;

    /* Open the window */
    ASSERT_DEBUG_SYNC(window_ptr->pf != NULL,
                      "Pixel format is not set for the window instance");
    ASSERT_DEBUG_SYNC(window_ptr->window_platform != NULL,
                      "Platform-specific window instance is NULL");

    window_ptr->pfn_window_open_window(window_ptr->window_platform,
                                       root_window == NULL); /* is_first_window */

    /* Set up context sharing between all windows */
    system_critical_section_enter(spawned_windows_cs);
    {
        ogl_context     root_context    = NULL;
        _system_window* root_window_ptr = NULL;

        if (root_window != NULL)
        {
            root_window_ptr = (_system_window*)root_window;
            root_context    = root_window_ptr->system_ogl_context;
        }
        else
        {
            ASSERT_DEBUG_SYNC(window_ptr->is_root_window,
                              "Sanity check failed")
        }

        /* Create the new context now */
        window_ptr->system_ogl_context = ogl_context_create_from_system_window(window_ptr->title,
                                                                               window_ptr->context_type,
                                                                               (system_window) window_ptr,
                                                                               window_ptr->vsync_enabled,
                                                                               root_context);

        if (window_ptr->system_ogl_context == NULL)
        {
            LOG_FATAL("Could not create OGL context for window [%s]",
                      system_hashed_ansi_string_get_buffer(window_ptr->title) );

            goto end;
        }

        if (root_context != NULL)
        {
            ogl_context_bind_to_current_thread(window_ptr->system_ogl_context);
        }
    }
    system_critical_section_leave(spawned_windows_cs);

    /* Bind the context to current thread. As a side effect, stuff like text rendering will
     * be initialized.
     */
    ogl_context_bind_to_current_thread    (window_ptr->system_ogl_context);
    ogl_context_unbind_from_current_thread(window_ptr->system_ogl_context);

    /* All done, set the event so the creating thread can carry on */
    system_event_set(window_ptr->window_initialized_event);

    /* Use this thread to handle the window until it is closed or closes
     * by itself */
    window_ptr->pfn_window_handle_window(window_ptr->window_platform);

end:
    ogl_context_release_managers(window_ptr->system_ogl_context);

    /* Release rendering handler. This may be odd, but rendering handler retains a context and the context retains the rendering handler
     * so window rendering thread closure looks like one of good spots to insert a release request.
     *
     **/
    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_release(window_ptr->rendering_handler);

        if (window_ptr->rendering_handler != NULL)
        {
            ogl_rendering_handler_release(window_ptr->rendering_handler);
        }

        window_ptr->rendering_handler = NULL;
    }

    /* Release OGL context */
    if (window_ptr->system_ogl_context != NULL)
    {
        ogl_context_release(window_ptr->system_ogl_context);

        window_ptr->system_ogl_context = NULL;
    }

    if (window_ptr->window_platform != NULL)
    {
        window_ptr->pfn_window_deinit_window(window_ptr->window_platform);

        window_ptr->window_platform = NULL;
    }

    system_event_set(window_ptr->window_safe_to_release_event);
}


/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_add_callback_func(system_window                        window,
                                                        system_window_callback_func_priority priority,
                                                        system_window_callback_func          callback_func,
                                                        void*                                pfn_callback_func,
                                                        void*                                user_arg)
{
    _callback_descriptor* new_descriptor_ptr = new (std::nothrow) _callback_descriptor;
    bool                  result             = false;
    _system_window*       window_ptr         = (_system_window*) window;

    if (new_descriptor_ptr != NULL)
    {
        new_descriptor_ptr->pfn_callback = pfn_callback_func;
        new_descriptor_ptr->priority     = priority;
        new_descriptor_ptr->user_arg     = user_arg;

        system_critical_section_enter(window_ptr->callbacks_cs);
        {
            /* NOTE: Modifying this? Update system_window_delete_callback_func() as well */
            system_resizable_vector callback_container = NULL;

            switch (callback_func)
            {
#ifdef _WIN32
                case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:
                {
                    callback_container = window_ptr->char_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:
                {
                    callback_container = window_ptr->exit_size_move_callbacks;
                    result             = true;

                    break;
                }
#endif

                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:
                {
                    callback_container = window_ptr->key_up_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:
                {
                    callback_container = window_ptr->key_down_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK:
                {
                    callback_container = window_ptr->left_button_double_click_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN:
                {
                    callback_container = window_ptr->left_button_down_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP:
                {
                    callback_container = window_ptr->left_button_up_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK:
                {
                    callback_container = window_ptr->middle_button_double_click_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN:
                {
                    callback_container = window_ptr->middle_button_down_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP:
                {
                    callback_container = window_ptr->middle_button_up_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE:
                {
                    callback_container = window_ptr->mouse_move_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL:
                {
                    callback_container = window_ptr->mouse_wheel_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK:
                {
                    callback_container = window_ptr->right_button_double_click_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN:
                {
                    callback_container = window_ptr->right_button_down_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP:
                {
                    callback_container = window_ptr->right_button_up_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED:
                {
                    callback_container = window_ptr->window_closed_callbacks;
                    result             = true;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING:
                {
                    callback_container = window_ptr->window_closing_callbacks;
                    result             = true;

                    break;
                }

                default:
                {
                    LOG_ERROR("Unrecognized callback func requested [%d]",
                              callback_func);

                    result = false;
                    break;
                }
            } /* switch (callback_func) */

            if (result)
            {
                /* Find a fitting place to insert the callback */
                size_t   insertion_index = -1;
                uint32_t n_elements      = 0;

                system_resizable_vector_get_property(callback_container,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_elements);

                for (uint32_t n_element = 0;
                              n_element < n_elements;
                            ++n_element)
                {
                    _callback_descriptor* existing_descriptor_ptr = NULL;

                    if (system_resizable_vector_get_element_at(callback_container,
                                                               n_element,
                                                              &existing_descriptor_ptr) )
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
                    system_resizable_vector_push(callback_container,
                                                 new_descriptor_ptr);
                } /* if (insertion_index == -1) */
                else
                {
                    system_resizable_vector_insert_element_at(callback_container,
                                                              insertion_index,
                                                              new_descriptor_ptr);
                }
            } /* if (result) */
        }
        system_critical_section_leave(window_ptr->callbacks_cs);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_close(system_window window)
{
    _system_window* window_ptr = (_system_window*) window;

    /* If there is a rendering handler and it is active, stop it before we continue */
    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_playback_status playback_status;

        ogl_rendering_handler_get_property(window_ptr->rendering_handler,
                                           OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,
                                          &playback_status);

        if (playback_status != RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED)
        {
            ogl_rendering_handler_stop(window_ptr->rendering_handler);
        }

        /* Release the rendering handler */
        ogl_rendering_handler_release(window_ptr->rendering_handler);
    }

    #ifdef INCLUDE_WEBCAM_MANAGER
        /* If there is a notification handler installed, deinstall it now */
        if (window_ptr->webcam_device_notification_handle != NULL)
        {
            ::UnregisterDeviceNotification(window_ptr->webcam_device_notification_handle);

            window_ptr->webcam_device_notification_handle = NULL;
        }
    #endif /* INCLUDE_WEBCAM_MANAGER */

    window_ptr->pfn_window_close_window(window_ptr->window_platform);

    system_event_wait_single(window_ptr->window_safe_to_release_event);
    system_event_wait_single(window_ptr->window_thread_event);

    /* Remove the window handle from the 'spawned windows' vector */
    system_critical_section_enter(spawned_windows_cs);
    {
        size_t entry_index = system_resizable_vector_find(spawned_windows,
                                                          window);

        ASSERT_DEBUG_SYNC(entry_index != ITEM_NOT_FOUND,
                          "Handle of a window to be removed could not have been found in the 'spawned windows' vector.");

        if (entry_index != ITEM_NOT_FOUND)
        {
            if (!system_resizable_vector_delete_element_at(spawned_windows,
                                                           entry_index) )
            {
                LOG_ERROR("Could not remove %uth element from 'spawned windows' vector",
                          (unsigned int) entry_index);
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
PRIVATE void _system_window_create_root_window(ogl_context_type context_type)
{
    static ogl_context_type prev_context_type      = (ogl_context_type) 0xFFFFFFFF;
    static const int        root_window_x1y1x2y2[] =
    {
        0,
        0,
        64,
        64
    };

    /* Sanity checks */
    if (prev_context_type != 0xFFFFFFFF)
    {
        ASSERT_DEBUG_SYNC(context_type == prev_context_type,
                          "TODO: Fix limitation where either only ES or only GL contexts can be created during app lifespan.");
    }
    else
    {
        prev_context_type = context_type;
    }

    ASSERT_DEBUG_SYNC(root_window                   == NULL &&
                      root_window_rendering_handler == NULL,
                      "Sanity check failed.");

    /* Spawn the root window.
     *
     * NOTE: root_window_pf is taken over by the root window, so we need not release
     *       the instance we're creating here.
     *
     * NOTE: The pixel format we use to initialize the root window uses 0 bits for the
     *       color, depth and stencil attachments. That's fine, we don't really need
     *       to render anything to the window; we only use it as a mean to share objects
     *       between contexts.
     */
    system_pixel_format root_window_pf = system_pixel_format_create(8,  /* red_bits     */
                                                                    8,  /* green_bits   */
                                                                    8,  /* blue_bits    */
                                                                    0,  /* alpha_bits   */
                                                                    24, /* depth_bits   */
                                                                    1,  /* n_samples    */
                                                                    0); /* stencil_bits */
    root_window =  _system_window_create_shared(context_type,
                                                false, /* is_fullscreen */
                                                root_window_x1y1x2y2,
                                                NULL, /* screen_mode */
                                                system_hashed_ansi_string_create("Root window"),
                                                false, /* scalable */
                                                false, /* vsync_enabled */
                                                0,     /* parent_window_handle */
                                                false, /* visible */
                                                true,  /* is_root_window */
                                                root_window_pf);

    ASSERT_DEBUG_SYNC(root_window != NULL,
                      "Root window context creation failed");

    root_window_rendering_handler = ogl_rendering_handler_create_with_render_per_request_policy(system_hashed_ansi_string_create("Root window rendering handler"),
                                                                                                NULL,  /* pfn_rendering_callback */
                                                                                                NULL); /* user_arg */

    ASSERT_DEBUG_SYNC(root_window_rendering_handler != NULL,
                      "Root window rendering handler creation failed");

    system_window_set_property(root_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &root_window_rendering_handler);

    /* The setter takes ownership of root_window, but for the sake of code maintainability,
     * retain the RH so that we can release it at the moment root_window is also released. */
    ogl_rendering_handler_retain(root_window_rendering_handler);
}

/** TODO */
PRIVATE system_window _system_window_create_shared(ogl_context_type          context_type,
                                                   bool                      is_fullscreen,
                                                   const int*                x1y1x2y2,
                                                   system_screen_mode        screen_mode,
                                                   system_hashed_ansi_string title,
                                                   bool                      is_scalable,
                                                   bool                      vsync_enabled,
                                                   system_window_handle      parent_window_handle,
                                                   bool                      visible,
                                                   bool                      is_root_window,
                                                   system_pixel_format       pf)
{
    _system_window* new_window = new (std::nothrow) _system_window;

    ASSERT_ALWAYS_SYNC(new_window != NULL,
                       "Out of memory while creating system window instance");

    if (new_window != NULL)
    {
        _init_system_window(new_window);

        /* Fill the descriptor with input values */
        new_window->context_type         = context_type;
        new_window->is_closing           = false;
        new_window->is_cursor_visible    = false;
        new_window->is_fullscreen        = is_fullscreen;
        new_window->is_root_window       = is_root_window;
        new_window->is_scalable          = is_scalable;
        new_window->parent_window_handle = parent_window_handle;
        new_window->pf                   = pf;
        new_window->screen_mode          = screen_mode;
        new_window->title                = title;
        new_window->visible              = visible;
        new_window->vsync_enabled        = vsync_enabled;

        if (x1y1x2y2 != NULL)
        {
            new_window->x1y1x2y2[0] = x1y1x2y2[0];
            new_window->x1y1x2y2[1] = x1y1x2y2[1];
            new_window->x1y1x2y2[2] = x1y1x2y2[2];
            new_window->x1y1x2y2[3] = x1y1x2y2[3];
        }
        else
        {
            ASSERT_DEBUG_SYNC(screen_mode != NULL,
                              "Both x1y1x2y2 and screen_mode arguments are NULL (screen mode requested by app is unsupported?)");

            new_window->x1y1x2y2[0] = 0;
            new_window->x1y1x2y2[1] = 0;

            system_screen_mode_get_property(screen_mode,
                                            SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                            new_window->x1y1x2y2 + 2);
            system_screen_mode_get_property(screen_mode,
                                            SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                            new_window->x1y1x2y2 + 3);
        }

        /* Activate the requested screen mode, if we're dealing with a full-screen window.
         * Chances are the root window is already around (eg. the user has earlier queried
         * for the supported MSAA samples), in which case we must release it before we change
         * the resolution. Reason is all sorts of bad stuff can happen if we switch the 
         * resolution after a rendering context has been created. As an example, Windows NV
         * driver is OK with this, but Linux version starts throwing OOMs right afterward.
         *
         * NOTE: Release of the root window is ONLY OK if the first window ever created is
         *       full-screen. Any combinations of non-full-screen and full-screen windows
         *       are unsupported, as their availability would be much more time-consuming to
         *       make happen.. and frankly? we don't need it.
         */
        if (is_fullscreen)
        {
            static int n_fullscreen_windows_spawned = 0;

            ASSERT_DEBUG_SYNC(n_fullscreen_windows_spawned == 0,
                              "Only one full-screen window can be created during application's life-time");
            ASSERT_DEBUG_SYNC(n_total_windows_spawned == 0,
                              "No other windows can be created alongside a full-screen window");
            ASSERT_DEBUG_SYNC(screen_mode != NULL,
                              "NULL screen_mode instance for a full-screen window");

            if (root_window != NULL)
            {
                /* Need to release the root window before we continue. */
                system_window_close(root_window);

                root_window = NULL;
            }

            system_screen_mode_activate(screen_mode);

            n_fullscreen_windows_spawned++;
        }

        if (new_window->window_safe_to_release_event != NULL &&
            new_window->window_initialized_event     != NULL)
        {
            /* Spawn root window, if there's none around. */
            int  current_n_total_windows_spawned;
            char temp_buffer[128];

            system_critical_section_enter(n_total_windows_spawned_cs);
            {
                current_n_total_windows_spawned = n_total_windows_spawned;

                ++n_total_windows_spawned;
            }
            system_critical_section_leave(n_total_windows_spawned_cs);

            if (!is_root_window && root_window == NULL)
            {
                _system_window_create_root_window(context_type);
            }

             /* Spawn window thread */
            snprintf(temp_buffer,
                     sizeof(temp_buffer),
                     "Window [%s] thread",
                     system_hashed_ansi_string_get_buffer(title) );

            system_threads_spawn(_system_window_thread_entrypoint,
                                 new_window,
                                &new_window->window_thread_event,
                                 system_hashed_ansi_string_create(temp_buffer) );

            /* Wait until window finishes initialization */
            system_event_wait_single(new_window->window_initialized_event);

            /* Insert the window handle into the 'spawned windows' vector */
            system_critical_section_enter(spawned_windows_cs);
            {
                system_resizable_vector_push(spawned_windows,
                                             new_window);
            }
            system_critical_section_leave(spawned_windows_cs);

            /* We're done */
            LOG_INFO("Window [title=<%s>] initialized.",
                     system_hashed_ansi_string_get_buffer(title) );
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
PUBLIC EMERALD_API system_window system_window_create_by_replacing_window(system_hashed_ansi_string name,
                                                                          ogl_context_type          context_type,
                                                                          bool                      vsync_enabled,
                                                                          system_window_handle      parent_window_handle,
                                                                          system_pixel_format       pf)
{
    system_window result = NULL;

    #ifdef _WIN32
    {
        BOOL boolean_result     = FALSE;
        RECT parent_window_rect;
        RECT grandparent_window_rect;
        int  x1y1x2y2[4]        = {0};

        boolean_result  = ::GetWindowRect(parent_window_handle,
                                         &parent_window_rect);
        boolean_result &= ::GetWindowRect(::GetParent(parent_window_handle),
                                         &grandparent_window_rect);

        ASSERT_ALWAYS_SYNC(boolean_result == TRUE,
                           "Could not retrieve window rectangle for parent window handle [%x]",
                           parent_window_handle);

        if (boolean_result)
        {
            x1y1x2y2[0] = parent_window_rect.left   - grandparent_window_rect.left;
            x1y1x2y2[1] = parent_window_rect.top    - grandparent_window_rect.top;
            x1y1x2y2[2] = parent_window_rect.right  - grandparent_window_rect.left;
            x1y1x2y2[3] = parent_window_rect.bottom - grandparent_window_rect.top;

            result = _system_window_create_shared(context_type,
                                                  false,  /* not fullscreen */
                                                  x1y1x2y2,
                                                  NULL,  /* screen_mode */
                                                  name,
                                                  false, /* not scalable */
                                                  vsync_enabled,
                                                  ::GetParent(parent_window_handle),
                                                  true   /* visible */,
                                                  false, /* is_root_window */
                                                  pf);
        }
    }
    #else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "system_window_create_by_replacing_window() is only supported under Windows");
    }
    #endif

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_window system_window_create_not_fullscreen(ogl_context_type          context_type,
                                                                     const int*                x1y1x2y2,
                                                                     system_hashed_ansi_string title,
                                                                     bool                      scalable,
                                                                     bool                      vsync_enabled,
                                                                     bool                      visible,
                                                                     system_pixel_format       pf)
{
    return _system_window_create_shared(context_type,
                                        false,
                                        x1y1x2y2,
                                        NULL, /* screen_mode */
                                        title,
                                        scalable,
                                        vsync_enabled,
                                        (system_window_handle) NULL, /* parent_window_handle */
                                        visible,
                                        false, /* is_root_window */
                                        pf);
}

/** Please see header for specification */
PUBLIC EMERALD_API system_window system_window_create_fullscreen(ogl_context_type    context_type,
                                                                 system_screen_mode  mode,
                                                                 bool                vsync_enabled,
                                                                 system_pixel_format pf)
{
    int x1y1x2y2[4] = {0, 0};

    return _system_window_create_shared(context_type,
                                        true,
                                        NULL, /* x1y1x2y2 */
                                        mode,
                                        system_hashed_ansi_string_get_default_empty_string(),
                                        false, /* scalable */
                                        vsync_enabled,
                                        (system_window_handle) NULL,  /* parent_window_handle */
                                        true,  /* visible */
                                        false, /* is_root_window */
                                        pf);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_delete_callback_func(system_window               window_instance,
                                                           system_window_callback_func callback_func,
                                                           void*                       pfn_callback_func,
                                                           void*                       user_arg)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window_instance;

    if (window_ptr != NULL)
    {
        system_resizable_vector callbacks_container = NULL;

        system_critical_section_enter(window_ptr->callbacks_cs);
        {
            /* NOTE: Modifying this? Update system_window_add_callback_func() as well */
            switch (callback_func)
            {
#ifdef _WIN32
                case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:
                {
                    callbacks_container = window_ptr->exit_size_move_callbacks;

                    break;
                }
#endif

                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE:
                {
                    callbacks_container = window_ptr->mouse_move_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN:
                {
                    callbacks_container = window_ptr->left_button_down_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP:
                {
                    callbacks_container = window_ptr->left_button_up_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK:
                {
                    callbacks_container = window_ptr->left_button_double_click_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN:
                {
                    callbacks_container = window_ptr->middle_button_down_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP:
                {
                    callbacks_container = window_ptr->middle_button_up_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK:
                {
                    callbacks_container = window_ptr->middle_button_double_click_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN:
                {
                    callbacks_container = window_ptr->right_button_down_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP:
                {
                    callbacks_container = window_ptr->right_button_up_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK:
                {
                    callbacks_container = window_ptr->right_button_double_click_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL:
                {
                    callbacks_container = window_ptr->mouse_wheel_callbacks;

                    break;
                }

#ifdef _WIN32
                case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:
                {
                    callbacks_container = window_ptr->char_callbacks;

                    break;
                }
#endif

                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:
                {
                    callbacks_container = window_ptr->key_up_callbacks;

                    break;
                }

                case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:
                {
                    callbacks_container = window_ptr->key_down_callbacks;

                    break;
                }

                default:
                {
                    LOG_ERROR("Unrecognized callback func requested [%d]",
                              callback_func);

                    break;
                }
            } /* switch (callback_func) */

            if (callbacks_container != NULL)
            {
                size_t n_callbacks = 0;

                system_resizable_vector_get_property(callbacks_container,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_callbacks);

                for (size_t n_callback = 0;
                            n_callback < n_callbacks;
                          ++n_callback)
                {
                    _callback_descriptor* callback_ptr = NULL;

                    if (system_resizable_vector_get_element_at(callbacks_container,
                                                               n_callback,
                                                              &callback_ptr) )
                    {
                        if (callback_ptr->pfn_callback == pfn_callback_func &&
                            callback_ptr->user_arg     == user_arg)
                        {
                            system_resizable_vector_delete_element_at(callbacks_container,
                                                                      n_callback);

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
        system_critical_section_leave(window_ptr->callbacks_cs);
    }

    return result;
}

/** Please see header for specification */
PUBLIC void system_window_execute_callback_funcs(system_window               window,
                                                 system_window_callback_func func,
                                                 void*                       arg1,
                                                 void*                       arg2)
{
    system_resizable_vector callback_vector  = NULL;
    unsigned int            n_callback_funcs = 0;
    bool                    needs_cursor_pos = false;
    _system_window*         window_ptr       = (_system_window*) window;

    switch (func)
    {
#ifdef _WIN32
        case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:
        {
            callback_vector = window_ptr->char_callbacks;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:
        {
            callback_vector = window_ptr->exit_size_move_callbacks;

            break;
        }
#endif

        case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:
        {
            callback_vector = window_ptr->key_down_callbacks;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:
        {
            callback_vector = window_ptr->key_up_callbacks;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK:
        {
            callback_vector  = window_ptr->left_button_double_click_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN:
        {
            callback_vector  = window_ptr->left_button_down_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP:
        {
            callback_vector  = window_ptr->left_button_up_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK:
        {
            callback_vector  = window_ptr->middle_button_double_click_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN:
        {
            callback_vector  = window_ptr->middle_button_down_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP:
        {
            callback_vector  = window_ptr->middle_button_up_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE:
        {
            callback_vector  = window_ptr->mouse_move_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL:
        {
            callback_vector  = window_ptr->mouse_wheel_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK:
        {
            callback_vector  = window_ptr->right_button_double_click_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN:
        {
            callback_vector  = window_ptr->right_button_down_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP:
        {
            callback_vector  = window_ptr->right_button_up_callbacks;
            needs_cursor_pos = true;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED:
        {
            callback_vector = window_ptr->window_closed_callbacks;

            break;
        }

        case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING:
        {
            callback_vector = window_ptr->window_closing_callbacks;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized call-back function type");
        }
    } /* switch (func) */

    if (callback_vector != NULL)
    {
        int cursor_position[2];

        system_critical_section_enter(window_ptr->callbacks_cs);
        {
            system_resizable_vector_get_property(callback_vector,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_callback_funcs);

            /* We need to know cursor position for a handful of call-back types */
            if (needs_cursor_pos)
            {
                window_ptr->pfn_window_get_property(window_ptr->window_platform,
                                                    SYSTEM_WINDOW_PROPERTY_CURSOR_POSITION,
                                                    &cursor_position);
            }

            /* Iterate over all call-back funcs and fire the calls */
            for (uint32_t n_callback_func = 0;
                          n_callback_func < n_callback_funcs;
                        ++n_callback_func)
            {
                _callback_descriptor* callback_ptr = NULL;

                if (system_resizable_vector_get_element_at(callback_vector,
                                                           n_callback_func,
                                                          &callback_ptr) )
                {
                    bool result = false;

                    switch (func)
                    {
#ifdef _WIN32
                        case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:
                        {
                            result = ((PFNWINDOWCHARCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                              (( (intptr_t) arg1) & 0xFF),
                                                                                              callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:
                        {
                            result = ((PFNWINDOWEXITSIZEMOVECALLBACKPROC) callback_ptr->pfn_callback)(window);

                            break;
                        }
#endif

                        case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:
                        {
                            result = ((PFNWINDOWKEYDOWNCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                 (intptr_t) arg1,
                                                                                                 callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:
                        {
                            result = ((PFNWINDOWKEYUPCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                               (intptr_t) arg1,
                                                                                               callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK:
                        {
                            result = ((PFNWINDOWLEFTBUTTONDBLCLKCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                          cursor_position[0],
                                                                                                          cursor_position[1],
                                                                                                          (system_window_vk_status) (intptr_t) arg1,
                                                                                                          callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN:
                        {
                            result = ((PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                        cursor_position[0],
                                                                                                        cursor_position[1],
                                                                                                        (system_window_vk_status) (intptr_t) arg1,
                                                                                                        callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP:
                        {
                            result = ((PFNWINDOWLEFTBUTTONUPCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                      cursor_position[0],
                                                                                                      cursor_position[1],
                                                                                                      (system_window_vk_status) (intptr_t) arg1,
                                                                                                      callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK:
                        {
                            result = ((PFNWINDOWMIDDLEBUTTONDBLCLKCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                            cursor_position[0],
                                                                                                            cursor_position[1],
                                                                                                            (system_window_vk_status) (intptr_t) arg1,
                                                                                                            callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN:
                        {
                            result = ((PFNWINDOWMIDDLEBUTTONDOWNCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                          cursor_position[0],
                                                                                                          cursor_position[1],
                                                                                                          (system_window_vk_status) (intptr_t) arg1,
                                                                                                          callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP:
                        {
                            result = ((PFNWINDOWMIDDLEBUTTONUPCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                        cursor_position[0],
                                                                                                        cursor_position[1],
                                                                                                        (system_window_vk_status) (intptr_t) arg1,
                                                                                                        callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE:
                        {
                            result = ((PFNWINDOWMOUSEMOVECALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                   cursor_position[0],
                                                                                                   cursor_position[1],
                                                                                                   (system_window_vk_status) (intptr_t) arg1,
                                                                                                   callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL:
                        {
                            result = ((PFNWINDOWMOUSEWHEELCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                    cursor_position[0],
                                                                                                    cursor_position[1],
                                                                                                    (short)                   (intptr_t) arg1,
                                                                                                    (system_window_vk_status) (intptr_t) arg2,
                                                                                                    callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK:
                        {
                            result = ((PFNWINDOWRIGHTBUTTONDBLCLKCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                           cursor_position[0],
                                                                                                           cursor_position[1],
                                                                                                           (system_window_vk_status) (intptr_t) arg1,
                                                                                                           callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN:
                        {
                            result = ((PFNWINDOWRIGHTBUTTONDOWNCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                         cursor_position[0],
                                                                                                         cursor_position[1],
                                                                                                         (system_window_vk_status) (intptr_t) arg1,
                                                                                                         callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP:
                        {
                            result = ((PFNWINDOWRIGHTBUTTONUPCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                       cursor_position[0],
                                                                                                       cursor_position[1],
                                                                                                       (system_window_vk_status) (intptr_t) arg1,
                                                                                                       callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED:
                        {
                            result = ((PFNWINDOWWINDOWCLOSEDCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                      callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING:
                        {
                            ((PFNWINDOWWINDOWCLOSINGCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                              callback_ptr->user_arg);

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized call-back function type");
                        }
                    } /* switch (func) */

                    if (!result)
                    {
                        break;
                    }
                } /* if (callback descriptor was retrieved) */
            } /* for (all call-back functions) */
        }
        system_critical_section_leave(window_ptr->callbacks_cs);
    } /* if (callback_vector != NULL) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_get_centered_window_position_for_primary_monitor(const int* dimensions,
                                                                                       int*       result_dimensions)
{
    int fullscreen_x = 0;
    int fullscreen_y = 0;

    #ifdef _WIN32
    {
        system_window_win32_get_screen_size(&fullscreen_x,
                                            &fullscreen_y);
    }
    #else
    {
        system_window_linux_get_screen_size(&fullscreen_x,
                                            &fullscreen_y);
    }
    #endif

    if (fullscreen_x >= dimensions[0] &&
        fullscreen_y >= dimensions[1])
    {
        result_dimensions[0] = (fullscreen_x - dimensions[0]) >> 1;
        result_dimensions[1] = (fullscreen_y - dimensions[1]) >> 1;
        result_dimensions[2] = (fullscreen_x + dimensions[0]) >> 1;
        result_dimensions[3] = (fullscreen_y + dimensions[1]) >> 1;

        return true;
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Requested window is larger than the screen's size.");

        return false;
    }
}

/* Please see header for spec */
PUBLIC EMERALD_API void system_window_get_property(system_window          window,
                                                   system_window_property property,
                                                   void*                  out_result)
{
    _system_window* window_ptr = (_system_window*) window;

    if (window_ptr->window_platform != NULL)
    {
        bool result = window_ptr->pfn_window_get_property(window_ptr->window_platform,
                                                          property,
                                                          out_result);

        if (result)
        {
            /* Result value returned. */
            goto end;
        }
    } /* if (window_ptr->window_platform != NULL) */

    switch (property)
    {
        case SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM:
        {
            *(audio_stream*) out_result = window_ptr->audio_strm;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_CONTEXT_TYPE:
        {
            *(ogl_context_type*) out_result = window_ptr->context_type;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_DIMENSIONS:
        {
            ((int*) out_result)[0] = window_ptr->x1y1x2y2[2] - window_ptr->x1y1x2y2[0];
            ((int*) out_result)[1] = window_ptr->x1y1x2y2[3] - window_ptr->x1y1x2y2[1];

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_CLOSED:
        {
            *(bool*) out_result = system_event_wait_single_peek(window_ptr->window_safe_to_release_event);

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_CLOSING:
        {
            *(bool*) out_result = window_ptr->is_closing;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_FULLSCREEN:
        {
            *(bool*) out_result = window_ptr->is_fullscreen;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_ROOT_WINDOW:
        {
            *(bool*) out_result = window_ptr->is_root_window;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_SCALABLE:
        {
            *(bool*) out_result = window_ptr->is_scalable;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_VISIBLE:
        {
            *(bool*) out_result = window_ptr->visible;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_IS_VSYNC_ENABLED:
        {
            *(bool*) out_result = window_ptr->vsync_enabled;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = window_ptr->title;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE:
        {
            *(system_window_handle*) out_result = window_ptr->parent_window_handle;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT:
        {
            *(system_pixel_format*) out_result = window_ptr->pf;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT:
        {
            *(ogl_context*) out_result = window_ptr->system_ogl_context;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER:
        {
            *(ogl_rendering_handler*) out_result = window_ptr->rendering_handler;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_SCREEN_MODE:
        {
            *(system_screen_mode*) out_result = window_ptr->screen_mode;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_TITLE:
        {
            *(system_hashed_ansi_string*) out_result = window_ptr->title;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2:
        {
            memcpy(out_result,
                   window_ptr->x1y1x2y2,
                   sizeof(window_ptr->x1y1x2y2) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_window_property value.");
        }
    } /* switch (property) */

end:
    ;
}

/** Please see header for specification */
PUBLIC system_window system_window_get_root_window(ogl_context_type context_type)
{
    if (root_window == NULL)
    {
        _system_window_create_root_window(context_type);
    }

    ASSERT_DEBUG_SYNC(root_window != NULL,
                      "Could not instantiate the root window.");

    return root_window;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_set_cursor(system_window              window,
                                                 system_window_mouse_cursor cursor)
{
    _system_window* window_ptr = (_system_window*) window;
    bool            result     = true;

    /* Redirect to platform-specific handler */
    result = window_ptr->pfn_window_set_property(window_ptr->window_platform,
                                                 SYSTEM_WINDOW_PROPERTY_CURSOR,
                                                &cursor);

    if (result)
    {
        window_ptr->window_mouse_cursor = cursor;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_window_set_property(system_window          window,
                                                   system_window_property property,
                                                   const void*            data)
{
    bool            result     = false;
    _system_window* window_ptr = (_system_window*) window;

    result = window_ptr->pfn_window_set_property(window_ptr->window_platform,
                                                 property,
                                                 data);

    if (!result)
    {
        switch (property)
        {
            case SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM:
            {
                ogl_rendering_handler_playback_status playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;

                if (window_ptr->rendering_handler != NULL)
                {
                    ogl_rendering_handler_get_property(window_ptr->rendering_handler,
                                                       OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,
                                                      &playback_status);
                } /* if (window_ptr->rendering_handler != NULL) */

                ASSERT_DEBUG_SYNC(playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED,
                                  "Cannot assign an audio device to a rendering window while the rendering playback is in progress.");

                if (playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED)
                {
                    audio_device stream_audio_device           = NULL;
                    bool         stream_audio_device_activated = false;

                    ASSERT_DEBUG_SYNC(window_ptr->audio_strm == NULL,
                                      "TODO: Support for switching audio streams in system_window.");

                    window_ptr->audio_strm = *(audio_stream*) data;

                    ASSERT_DEBUG_SYNC(window_ptr->audio_strm != NULL,
                                      "A NULL audio stream was assigned to a window instance");

                    /* Retain the stream. We don't want the instance to die in the middle of playback */
                    audio_stream_retain(window_ptr->audio_strm);

                    /* TODO: This assumes only one window will ever be assigned up to one audio stream.
                     *       Make more flexible, if needed.
                     */
                    audio_stream_get_property(window_ptr->audio_strm,
                                              AUDIO_STREAM_PROPERTY_AUDIO_DEVICE,
                                             &stream_audio_device);

                    ASSERT_DEBUG_SYNC(stream_audio_device != NULL,
                                      "No audio device associated with the audio_stream instance?!");

                    audio_device_get_property(stream_audio_device,
                                              AUDIO_DEVICE_PROPERTY_IS_ACTIVATED,
                                              &stream_audio_device_activated);

                    if (!stream_audio_device_activated)
                    {
                        if (!audio_device_activate(stream_audio_device,
                                                   window) )
                        {
                            ASSERT_ALWAYS_SYNC(false,
                                               "Could not activate the audio device associated with the audio_stream instance");
                        }
                    } /* if (!stream_audio_device_initialize) */
                } /* if (playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED) */

                break;
            }

            case SYSTEM_WINDOW_PROPERTY_IS_CLOSING:
            {
                result                 = true;
                window_ptr->is_closing = *(bool*) data;

                break;
            }

            case SYSTEM_WINDOW_PROPERTY_POSITION:
            {
                const int* xy = (const int*) data;

                result = window_ptr->pfn_window_set_property(window_ptr->window_platform,
                                                             SYSTEM_WINDOW_PROPERTY_POSITION,
                                                             xy);

                /* Update internal representation */
                int width  = window_ptr->x1y1x2y2[2] - window_ptr->x1y1x2y2[0];
                int height = window_ptr->x1y1x2y2[3] - window_ptr->x1y1x2y2[1];

                window_ptr->x1y1x2y2[0] = xy[0];
                window_ptr->x1y1x2y2[1] = xy[1];
                window_ptr->x1y1x2y2[2] = xy[0] + width;
                window_ptr->x1y1x2y2[3] = xy[1] + height;

                break;
            }

            case SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER:
            {
                ogl_rendering_handler new_rendering_handler = *(ogl_rendering_handler*) data;

                ASSERT_DEBUG_SYNC(new_rendering_handler != NULL,
                                  "Cannot set a null rendering buffer!");

                if (window_ptr->rendering_handler == NULL &&
                    new_rendering_handler         != NULL)
                {
                    window_ptr->rendering_handler = new_rendering_handler;
                    result                        = true;

                    ogl_rendering_handler_retain              (window_ptr->rendering_handler);
                    _ogl_rendering_handler_on_bound_to_context(window_ptr->rendering_handler,
                                                               ((_system_window*)window)->system_ogl_context);
                }

                break;
            }

            case SYSTEM_WINDOW_PROPERTY_DIMENSIONS:
            {
                const int* width_height = (const int*) data;

                /* Update internal representation */
                window_ptr->x1y1x2y2[2] = window_ptr->x1y1x2y2[0] + width_height[0];
                window_ptr->x1y1x2y2[3] = window_ptr->x1y1x2y2[1] + width_height[1];

                /* Update platform-specific window instance */
                result = window_ptr->pfn_window_set_property(window_ptr->window_platform,
                                                             SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                                             width_height);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized system_window_property value");
            }
        } /* switch (property) */
    }

    return result;
}

/** Please see header for specification */
PUBLIC void system_window_wait_until_closed(system_window window)
{
    _system_window* window_ptr = (_system_window*) window;

    system_event_wait_single(window_ptr->window_safe_to_release_event);
}

/** Please see header for specification */
PUBLIC void _system_window_init()
{
    n_total_windows_spawned_cs = system_critical_section_create();
    spawned_windows            = system_resizable_vector_create(BASE_WINDOW_STORAGE);
    spawned_windows_cs         = system_critical_section_create();

    #ifdef _WIN32
    {
        /* Nothing to do here */
    }
    #else
    {
        system_window_linux_init_global();
    }
    #endif

}

/** Please see header for specification */
PUBLIC void _system_window_deinit()
{
    /* Release the root window if one is around */
    if (root_window != NULL)
    {
        ASSERT_DEBUG_SYNC(root_window_rendering_handler != NULL,
                          "Root window's rendering handler is NULL");

        ogl_rendering_handler_release(root_window_rendering_handler);
        root_window_rendering_handler = NULL;

        system_window_close(root_window);
        root_window = NULL;
    }

    system_critical_section_release(n_total_windows_spawned_cs);
    system_resizable_vector_release(spawned_windows);
    system_critical_section_release(spawned_windows_cs);

    #ifdef _WIN32
    {
        /* Nothing to do here */
    }
    #else
    {
        system_window_linux_deinit_global();
    }
    #endif
}
