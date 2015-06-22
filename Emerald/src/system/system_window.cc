/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
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
    // TEMP TODO: typedef system_window_linux system_window_platform;
    typedef void* system_window_platform;
#endif

typedef void (*PFNWINDOWCLOSEWINDOWPROC) (__in  system_window_platform window);
typedef void (*PFNWINDOWDEINITWINDOWPROC)(__in  system_window_platform window);
typedef bool (*PFNWINDOWGETPROPERTYPROC) (__in  system_window_platform window,
                                          __in  system_window_property property,
                                          __out void*                  out_result);
typedef void (*PFNWINDOWHANDLEWINDOWPROC)(__in  system_window_platform window);
typedef bool (*PFNWINDOWOPENWINDOWPROC)  (__in  system_window_platform window,
                                          __in  bool                   is_first_window);
typedef bool (*PFNWINDOWSETPROPERTYPROC) (__in system_window_platform  window,
                                          __in system_window_property  property,
                                          __in const void*             data);
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
    bool                       is_closing; /* in the process of calling the "window closing" call-backs */
    bool                       is_cursor_visible;
    bool                       is_fullscreen;
    bool                       is_root_window;
    bool                       is_scalable;
    bool                       multisampling_supported;
    uint16_t                   n_multisampling_samples;
    system_window_handle       parent_window_handle;
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

/** Used to track amount of spawned windows. Also, if it is the first window that is created, window class is initialized.
 *  Always lock n_windows_spawned_cs before accessing (in order to defend against multiple window class creation and counter
 *  corruption)
 */
int                     n_windows_spawned    = 0;
system_critical_section n_windows_spawned_cs = NULL;

/* Root window - holds root rendering context which encapsulates buffer objects, program objects, texutre objects, etc. that
 * all other rendering contexts may use.
 *
 * Deleted when n_windows_spawned drops to 0.
 */
ogl_rendering_handler root_window_rendering_handler = NULL;
system_window         root_window                   = NULL;


/* Forward declarations */
PRIVATE void          _deinit_system_window                                    (                           _system_window*                      descriptor);
PRIVATE void          _init_system_window                                      (                           _system_window*                      descriptor);
PRIVATE volatile void _system_window_teardown_thread_pool_callback             (__in __notnull             system_thread_pool_callback_argument arg);
PRIVATE void          _system_window_thread_entrypoint                         (     __notnull             void*                                in_arg);
PRIVATE void          _system_window_create_root_window                        (__in                       ogl_context_type                     context_type);
PRIVATE system_window _system_window_create_shared                             (__in __notnull             ogl_context_type                     context_type,
                                                                                __in                       bool                                 is_fullscreen,
                                                                                __in __notnull __ecount(4) const int*                           x1y1x2y2,
                                                                                __in __notnull             system_hashed_ansi_string            title,
                                                                                __in __notnull             bool                                 is_scalable,
                                                                                __in __notnull             uint16_t                             n_multisampling_samples,
                                                                                __in                       uint16_t                             fullscreen_bpp,
                                                                                __in                       uint16_t                             fullscreen_freq,
                                                                                __in                       bool                                 vsync_enabled,
                                                                                __in __maybenull           system_window_handle                 parent_window_handle,
                                                                                __in                       bool                                 multisampling_supported,
                                                                                __in                       bool                                 visible,
                                                                                __in                       bool                                 is_root_window);
PRIVATE void          _system_window_window_closing_rendering_thread_entrypoint(                           ogl_context                          context,
                                                                                                           void*                                user_arg);

/** TODO */
PRIVATE void _deinit_system_window(_system_window* window_ptr)
{
    if (window_ptr->callbacks_cs != NULL)
    {
        system_critical_section_release(window_ptr->callbacks_cs);

        window_ptr->callbacks_cs = NULL;
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
}

/** TODO */
PRIVATE void _init_system_window(_system_window* window_ptr)
{
    window_ptr->fullscreen_bpp               = 0;
    window_ptr->fullscreen_freq              = 0;
    window_ptr->is_cursor_visible            = false;
    window_ptr->is_fullscreen                = false;
    window_ptr->is_scalable                  = false;
    window_ptr->multisampling_supported      = false;
    window_ptr->n_multisampling_samples      = 0;
    window_ptr->pf                           = NULL;
    window_ptr->title                        = system_hashed_ansi_string_get_default_empty_string();
    window_ptr->window_mouse_cursor          = SYSTEM_WINDOW_MOUSE_CURSOR_ARROW;
    window_ptr->x1y1x2y2[0]                  = 0;
    window_ptr->x1y1x2y2[1]                  = 0;
    window_ptr->x1y1x2y2[2]                  = 0;
    window_ptr->x1y1x2y2[3]                  = 0;
    window_ptr->parent_window_handle         = NULL;
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
        window_ptr->window_platform          = system_window_win32_init( (system_window) window_ptr);
    #else
        window_ptr->pfn_window_close_window  = system_window_linux_close_window;
        window_ptr->pfn_window_deinit_window = system_window_linux_deinit;
        window_ptr->pfn_window_get_property  = system_window_linux_get_property;
        window_ptr->pfn_window_handle_window = system_window_linux_handle_window;
        window_ptr->pfn_window_open_window   = system_window_linux_open_window;
        window_ptr->pfn_window_set_property  = system_window_linux_set_property;
        window_ptr->window_platform          = system_window_linux_init( (system_window) window_ptr);
    #endif

    ASSERT_ALWAYS_SYNC(window_ptr->window_safe_to_release_event != NULL,
                       "Could not create safe-to-release event.");
    ASSERT_ALWAYS_SYNC(window_ptr->window_initialized_event != NULL,
                       "Could not create window initialized event.");
}


/** TODO */
PRIVATE void _system_window_thread_entrypoint(__notnull void* in_arg)
{
    _system_window* window_ptr = (_system_window*) in_arg;

    /* Open the window */
    ASSERT_DEBUG_SYNC(window_ptr->window_platform != NULL,
                      "Platform-specific window instance is NULL");

    if (!window_ptr->is_root_window)
    {
        /* For root windows, the CS will have already been locked. This does not
         * hold for any other window type.
         */
        system_critical_section_enter(n_windows_spawned_cs);
    }

    window_ptr->pfn_window_open_window(window_ptr->window_platform,
                                       n_windows_spawned == 0); /* is_first_window */

    /* Cache the properties of the default framebuffer we will be using for the OpenGL
     * context AND for the window visuals (under Linux).
     */
    window_ptr->pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                8,  /* color_buffer_green_bits */
                                                8,  /* color_buffer_blue_bits  */
                                                0,  /* color_buffer_alpha_bits */
                                                8); /* depth_bits */

    if (window_ptr->pf == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not create pixel format descriptor for RGB8D8 format.");

        goto end;
    }

    ++n_windows_spawned;

    if (!window_ptr->is_root_window)
    {
        system_critical_section_leave(n_windows_spawned_cs);
    }

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
                                                                               window_ptr->pf,
                                                                               window_ptr->vsync_enabled,
                                                                               root_context,
                                                                               window_ptr->multisampling_supported);

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

    /* Bind the context to current thread */
    ogl_context_bind_to_current_thread(window_ptr->system_ogl_context);
    {
        /* Set up multisampling for the window */
        if (window_ptr->multisampling_supported)
        {
            ogl_context_set_property(window_ptr->system_ogl_context,
                                     OGL_CONTEXT_PROPERTY_MSAA_N_SUPPORTED_SAMPLES,
                                    &window_ptr->n_multisampling_samples);
        }
    }
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
        ogl_rendering_handler_release(window_ptr->rendering_handler);

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

        system_critical_section_enter(window_ptr->callbacks_cs);
        {
            /* NOTE: Modifying this? Update system_window_delete_callback_func() as well */
            system_resizable_vector callback_container = NULL;

            switch (callback_func)
            {
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
PUBLIC EMERALD_API bool system_window_close(__in __notnull __deallocate(mem) system_window window)
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

    /* If no other window is around, release the root window */
    system_critical_section_enter(n_windows_spawned_cs);
    {
        ASSERT_DEBUG_SYNC(n_windows_spawned >= 1,
                          "Sanity check failed.");

        --n_windows_spawned;

        if (n_windows_spawned == 1)
        {
            ASSERT_DEBUG_SYNC(root_window                   != NULL &&
                              root_window_rendering_handler != NULL,
                              "Sanity check failed");

            ogl_rendering_handler_release(root_window_rendering_handler);
            root_window_rendering_handler = NULL;

            system_window_close(root_window);
            root_window = NULL;
        } /* if (n_windows_spawned == 0) */
    }
    system_critical_section_leave(n_windows_spawned_cs);

    return true;
}

/** TODO */
PRIVATE void _system_window_create_root_window(__in ogl_context_type context_type)
{
    static ogl_context_type prev_context_type      = (ogl_context_type) 0xFFFFFFFF;
    static const int        root_window_x1y1x2y2[] =
    {
        0,
        0,
        1,
        1
    };

    /* Sanity checks */
    if (prev_context_type != 0xFFFFFFFF)
    {
        ASSERT_DEBUG_SYNC(context_type == prev_context_type,
                          "Fix limitation where either only ES or only GL contexts can be created during app lifespan.");
    }
    else
    {
        prev_context_type = context_type;
    }

    ASSERT_DEBUG_SYNC(root_window                   == NULL &&
                      root_window_rendering_handler == NULL,
                      "Sanity check failed.");

    root_window =  _system_window_create_shared(context_type,
                                                false, /* is_fullscreen */
                                                root_window_x1y1x2y2,
                                                system_hashed_ansi_string_create("Root window"),
                                                false, /* scalable */
                                                0,     /* n_multisampling_samples */
                                                0,     /* fullscreen_bpp */
                                                0,     /* fullscreen_freq */
                                                false, /* vsync_enabled */
                                                0,     /* parent_window_handle */
                                                false, /* multisampling_supported */
                                                false, /* visible */
                                                true   /* is_root_window */);

    ASSERT_DEBUG_SYNC(root_window != NULL,
                      "Root window context creation failed");

    root_window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Root window rendering handler"),
                                                                                 1,     /* desired_fps */
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
PRIVATE system_window _system_window_create_shared(__in __notnull             ogl_context_type          context_type,
                                                   __in                       bool                      is_fullscreen,
                                                   __in __notnull __ecount(4) const int*                x1y1x2y2,
                                                   __in __notnull             system_hashed_ansi_string title,
                                                   __in __notnull             bool                      is_scalable,
                                                   __in __notnull             uint16_t                  n_multisampling_samples,
                                                   __in                       uint16_t                  fullscreen_bpp,
                                                   __in                       uint16_t                  fullscreen_freq,
                                                   __in                       bool                      vsync_enabled,
                                                   __in __maybenull           system_window_handle      parent_window_handle,
                                                   __in                       bool                      multisampling_supported,
                                                   __in                       bool                      visible,
                                                   __in                       bool                      is_root_window)
{
    _system_window* new_window = new (std::nothrow) _system_window;

    ASSERT_ALWAYS_SYNC(new_window != NULL,
                       "Out of memory while creating system window instance");

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
        new_window->is_closing              = false;
        new_window->is_cursor_visible       = false;
        new_window->is_fullscreen           = is_fullscreen;
        new_window->is_root_window          = is_root_window;
        new_window->is_scalable             = is_scalable;
        new_window->multisampling_supported = multisampling_supported;
        new_window->n_multisampling_samples = n_multisampling_samples;
        new_window->parent_window_handle    = parent_window_handle;
        new_window->title                   = title;
        new_window->visible                 = visible;
        new_window->vsync_enabled           = vsync_enabled;

        if (new_window->window_safe_to_release_event != NULL &&
            new_window->window_initialized_event     != NULL)
        {
            /* If this is the first window created during app life-time, spawn a root context window
             * before we continue.
             *
             * NOTE: This will be done repeatedly if appn_windows_spawnedlication opens a window A, kills it, and then
             *       opens a window B. No damage done, but it will have performance implications.
             */
            system_critical_section_enter(n_windows_spawned_cs);
            {
                if ( n_windows_spawned == 0      &&
                    !new_window->is_root_window)
                {
                    _system_window_create_root_window(context_type);
                }
                else
                if (n_windows_spawned > 0      &&
                    new_window->is_root_window)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Sanity check failed");
                }
            }
            system_critical_section_leave(n_windows_spawned_cs);

            /* Spawn window thread */
            system_threads_spawn(_system_window_thread_entrypoint,
                                 new_window,
                                &new_window->window_thread_event);

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
PUBLIC EMERALD_API system_window system_window_create_by_replacing_window(__in system_hashed_ansi_string name,
                                                                          __in ogl_context_type          context_type,
                                                                          __in uint16_t                  n_multisampling_samples,
                                                                          __in bool                      vsync_enabled,
                                                                          __in system_window_handle      parent_window_handle,
                                                                          __in bool                      multisampling_supported)
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
                                                  name,
                                                  false, /* not scalable */
                                                  n_multisampling_samples,
                                                  0,
                                                  0,
                                                  vsync_enabled,
                                                  ::GetParent(parent_window_handle),
                                                  multisampling_supported,
                                                  true  /* visible */,
                                                  false /* is_root_window */);
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
                                        NULL, /* parent_window_handle */
                                        multisampling_supported,
                                        visible,
                                        false /* is_root_window */);
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
                                        NULL, /* parent_window_handle */
                                        multisampling_supported,
                                        true, /* visible */
                                        false /* is_root_window */);
}

/** Please see header for specification */
PUBLIC void system_window_deinit_global()
{
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

        system_critical_section_enter(window_ptr->callbacks_cs);
        {
            /* NOTE: Modifying this? Update system_window_add_callback_func() as well */
            switch (callback_func)
            {
                case SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE:
                {
                    callbacks_container = window_ptr->exit_size_move_callbacks;

                    break;
                }

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

                case SYSTEM_WINDOW_CALLBACK_FUNC_CHAR:
                {
                    callbacks_container = window_ptr->char_callbacks;

                    break;
                }

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
PUBLIC void system_window_execute_callback_funcs(__in __notnull system_window               window,
                                                 __in           system_window_callback_func func,
                                                 __in_opt       void*                       arg1,
                                                 __in_opt       void*                       arg2)
{
    system_resizable_vector callback_vector  = NULL;
    unsigned int            n_callback_funcs = 0;
    bool                    needs_cursor_pos = false;
    _system_window*         window_ptr       = (_system_window*) window;

    switch (func)
    {
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

                        case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN:
                        {
                            result = ((PFNWINDOWKEYDOWNCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                                 (( (intptr_t) arg1) & 0xFF),
                                                                                                 callback_ptr->user_arg);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP:
                        {
                            result = ((PFNWINDOWKEYUPCALLBACKPROC) callback_ptr->pfn_callback)(window,
                                                                                               (( (intptr_t) arg1) & 0xFF),
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
                            result = ((PFNWINDOWWINDOWCLOSEDCALLBACKPROC) callback_ptr->pfn_callback)(window);

                            break;
                        }

                        case SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING:
                        {
                            ((PFNWINDOWWINDOWCLOSINGCALLBACKPROC) callback_ptr->pfn_callback)(window);

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
PUBLIC EMERALD_API bool system_window_get_centered_window_position_for_primary_monitor(__in_ecount(2)  __notnull const int* dimensions,
                                                                                       __out_ecount(4) __notnull int*       result_dimensions)
{
    #ifdef _WIN32
        int fullscreen_x = ::GetSystemMetrics(SM_CXFULLSCREEN);
        int fullscreen_y = ::GetSystemMetrics(SM_CYFULLSCREEN);
    #else
        /* TODO TEMP TEMP */
        int fullscreen_x = sedes;
        int fullscreen_y = sedes;
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
        return false;
    }
}

/* Please see header for spec */
PUBLIC EMERALD_API void system_window_get_property(__in  __notnull system_window          window,
                                                   __in            system_window_property property,
                                                   __out __notnull void*                  out_result)
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
        case SYSTEM_WINDOW_PROPERTY_FULLSCREEN_BPP:
        {
            *(uint16_t*) out_result = window_ptr->fullscreen_bpp;

            break;
        }

        case SYSTEM_WINDOW_PROPERTY_FULLSCREEN_REFRESH_RATE:
        {
            *(uint16_t*) out_result = window_ptr->fullscreen_freq;

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
PUBLIC void system_window_init_global()
{
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
PUBLIC EMERALD_API bool system_window_set_cursor(__in __notnull system_window              window,
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
PUBLIC EMERALD_API bool system_window_set_property(__in system_window          window,
                                                   __in system_window_property property,
                                                   __in const void*            data)
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
PUBLIC void system_window_wait_until_closed(__in __notnull system_window window)
{
    _system_window* window_ptr = (_system_window*) window;

    system_event_wait_single(window_ptr->window_safe_to_release_event);
}

/** Please see header for specification */
PUBLIC void _system_window_init()
{
    spawned_windows      = system_resizable_vector_create(BASE_WINDOW_STORAGE);
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
