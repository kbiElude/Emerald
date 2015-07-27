/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include "shared.h"


/* Shared macros */
#define PUBLIC
#define PRIVATE                static
#define RENDERING_CONTEXT_CALL


/*********************** ASSERTION CHECKS ********************************/
typedef enum
{
    DEBUG_ONLY_ASSERTION,
    RELEASE_ONLY_ASSERTION,
    ALWAYS_ASSERTION
} system_assertion_type;

#include <stdint.h>

/* General includes */
#define REFCOUNT_INSERT_DECLARATIONS(prefix, public_handle_type)  \
    PUBLIC EMERALD_API void prefix##_retain(public_handle_type);  \
    PUBLIC EMERALD_API void prefix##_release(public_handle_type&);

/******************* OBJECT MANAGER ***************************************/
#ifdef INCLUDE_OBJECT_MANAGER

    DECLARE_HANDLE(object_manager_directory);
    DECLARE_HANDLE(object_manager_item);

    typedef enum
    {
        OBJECT_TYPE_FIRST,

        OBJECT_TYPE_AUDIO_STREAM = OBJECT_TYPE_FIRST,
        OBJECT_TYPE_COLLADA_DATA,
        OBJECT_TYPE_CONTEXT_MENU,
        OBJECT_TYPE_SYSTEM_CRITICAL_SECTION,
        OBJECT_TYPE_CURVE_CONTAINER,
        OBJECT_TYPE_GFX_BFG_FONT_TABLE,
        OBJECT_TYPE_GFX_IMAGE,
        OBJECT_TYPE_MESH,
        OBJECT_TYPE_MESH_MATERIAL,
        OBJECT_TYPE_OCL_CONTEXT,
        OBJECT_TYPE_OCL_KDTREE,
        OBJECT_TYPE_OCL_KERNEL,
        OBJECT_TYPE_OCL_PROGRAM,
        OBJECT_TYPE_OGL_CONTEXT,
        OBJECT_TYPE_OGL_CURVE_RENDERER,
        OBJECT_TYPE_OGL_FLYBY,
        OBJECT_TYPE_OGL_PRIMITIVE_RENDERER,
        OBJECT_TYPE_OGL_PIPELINE,
        OBJECT_TYPE_OGL_PROGRAM,
        OBJECT_TYPE_OGL_PROGRAMS,
        OBJECT_TYPE_OGL_RENDERING_HANDLER,
        OBJECT_TYPE_OGL_SAMPLER,
        OBJECT_TYPE_OGL_SHADER,
        OBJECT_TYPE_OGL_SHADER_CONSTRUCTOR,
        OBJECT_TYPE_OGL_SKYBOX,
        OBJECT_TYPE_OGL_TEXT,
        OBJECT_TYPE_OGL_TEXTURE,
        OBJECT_TYPE_OGL_UBER,
        OBJECT_TYPE_OGL_UI,
        OBJECT_TYPE_POSTPROCESSING_BLOOM,
        OBJECT_TYPE_POSTPROCESSING_BLUR_GAUSSIAN,
        OBJECT_TYPE_POSTPROCESSING_BLUR_POISSON,
        OBJECT_TYPE_POSTPROCESSING_REINHARD_TONEMAP,
        OBJECT_TYPE_PROCEDURAL_MESH_BOX,
        OBJECT_TYPE_PROCEDURAL_MESH_SPHERE,
        OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_CURVEBACKGROUND,
        OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_LERP,
        OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_QUADSELECTOR,
        OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_STATIC,
        OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_TCB,
        OBJECT_TYPE_SCENE,
        OBJECT_TYPE_SCENE_CAMERA,
        OBJECT_TYPE_SCENE_CURVE,
        OBJECT_TYPE_SCENE_LIGHT,
        OBJECT_TYPE_SCENE_MATERIAL,
        OBJECT_TYPE_SCENE_MESH,
        OBJECT_TYPE_SCENE_SURFACE,
        OBJECT_TYPE_SCENE_TEXTURE,
        OBJECT_TYPE_SH_PROJECTOR,
        OBJECT_TYPE_SH_PROJECTOR_COMBINER,
        OBJECT_TYPE_SH_ROT,
        OBJECT_TYPE_SH_SAMPLES,
        OBJECT_TYPE_SH_SHADERS_OBJECT,
        OBJECT_TYPE_SHADERS_FRAGMENT_STATIC,
        OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_FILMIC,
        OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE,
        OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_LINEAR,
        OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_PLAIN,
        OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_REINHARDT,
        OBJECT_TYPE_SHADERS_FRAGMENT_RGB_TO_YXY,
        OBJECT_TYPE_SHADERS_FRAGMENT_UBER,
        OBJECT_TYPE_SHADERS_FRAGMENT_YXY_TO_RGB,
        OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_GENERIC,
        OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_SIMPLIFIED_TWO_POINT,
        OBJECT_TYPE_SHADERS_VERTEX_FULLSCREEN,
        OBJECT_TYPE_SHADERS_VERTEX_UBER,
        OBJECT_TYPE_SYSTEM_FILE_SERIALIZER,
        OBJECT_TYPE_SYSTEM_RANDOMIZER,
        OBJECT_TYPE_SYSTEM_WINDOW,

        OBJECT_TYPE_LAST,
        OBJECT_TYPE_UNKNOWN       = OBJECT_TYPE_LAST
    } object_manager_object_type;

#endif /* INCLUDE_OBJECT_MANAGER */

/********************* AUDIO DEVICE ***************************************/
DECLARE_HANDLE(audio_device);

/********************* AUDIO STREAM ***************************************/
DECLARE_HANDLE(audio_stream);

/************************ WINDOW ******************************************/
DECLARE_HANDLE(system_window);

#ifdef _WIN32
    DECLARE_HANDLE(system_window_win32);

    typedef HWND system_window_handle;
    typedef HDC  system_window_dc;
#elif __linux
    #include <X11/Xlib.h>

    DECLARE_HANDLE(system_window_linux);

    typedef Window system_window_handle;
#else
    #error Platform unsupported
#endif

/******************** TIME REPRESENTATION *********************************/
typedef int32_t system_time;

/** TODO */
typedef enum
{
    SYSTEM_WINDOW_MOUSE_CURSOR_MOVE,
    SYSTEM_WINDOW_MOUSE_CURSOR_ARROW,
    SYSTEM_WINDOW_MOUSE_CURSOR_HORIZONTAL_RESIZE,
    SYSTEM_WINDOW_MOUSE_CURSOR_VERTICAL_RESIZE,
    SYSTEM_WINDOW_MOUSE_CURSOR_CROSSHAIR,
    SYSTEM_WINDOW_MOUSE_CURSOR_ACTION_FORBIDDEN
} system_window_mouse_cursor;

/** TODO */
#ifdef _WIN32
    #define SYSTEM_WINDOW_VK_STATUS_ENTRY(entry_name, entry_value) entry_name = entry_value
#else
    #define SYSTEM_WINDOW_VK_STATUS_ENTRY(entry_name, entry_value) entry_name
#endif

typedef enum
{
#ifdef _WIN32
    SYSTEM_WINDOW_VK_STATUS_ENTRY(SYSTEM_WINDOW_VK_STATUS_CONTROL_PRESSED,       MK_CONTROL),
    SYSTEM_WINDOW_VK_STATUS_ENTRY(SYSTEM_WINDOW_VK_STATUS_LEFT_BUTTON_PRESSED,   MK_LBUTTON),
    SYSTEM_WINDOW_VK_STATUS_ENTRY(SYSTEM_WINDOW_VK_STATUS_MIDDLE_BUTTON_PRESSED, MK_MBUTTON),
    SYSTEM_WINDOW_VK_STATUS_ENTRY(SYSTEM_WINDOW_VK_STATUS_RIGHT_BUTTON_PRESSED,  MK_RBUTTON),
    SYSTEM_WINDOW_VK_STATUS_ENTRY(SYSTEM_WINDOW_VK_STATUS_SHIFT_PRESSED,         MK_SHIFT)
#else
    /* NOTE: Not reported under Linux. TODO */
    SYSTEM_WINDOW_VK_STATUS_CONTROL_PRESSED       = 1 << 0,
    /* NOTE: Not reported under Linux. TODO */
    SYSTEM_WINDOW_VK_STATUS_LEFT_BUTTON_PRESSED   = 1 << 1,
    /* NOTE: Not reported under Linux. TODO */
    SYSTEM_WINDOW_VK_STATUS_MIDDLE_BUTTON_PRESSED = 1 << 2,
    /* NOTE: Not reported under Linux. TODO */
    SYSTEM_WINDOW_VK_STATUS_RIGHT_BUTTON_PRESSED  = 1 << 3,
    /* NOTE: Not reported under Linux. TODO */
    SYSTEM_WINDOW_VK_STATUS_SHIFT_PRESSED         = 1 << 4
#endif
} system_window_vk_status;

/** TODO */
typedef enum
{
    SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
    SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOUBLE_CLICK,
    SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOWN,
    SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_UP,
    SYSTEM_WINDOW_CALLBACK_FUNC_MIDDLE_BUTTON_DOUBLE_CLICK,
    SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
    SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP,
    SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOUBLE_CLICK,
    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL,

#ifdef _WIN32
    SYSTEM_WINDOW_CALLBACK_FUNC_CHAR,
    SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE,
#endif

    /* Called right after a rendering window has been closed. This
     * can happen both per user's, and per system's request. */
    SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,

    /* Called the last moment before a rendering window gets closed.
     * The call-back occurs from a rendering thread. While the call-backs
     * are executed, the window message thread is locked up.
     */
    SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
} system_window_callback_func;

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: ascii key code. (unsigned char)
 *  arg2: not used.
 */
typedef bool (*PFNWINDOWCHARCALLBACKPROC)(system_window,
                                          unsigned char keycode,
                                          void*);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: not used.
 *  arg2: not used.
 */
typedef bool (*PFNWINDOWEXITSIZEMOVECALLBACKPROC)(system_window window);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: ascii key code or SYSTEM_WINDOW_KEY_*
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWKEYDOWNCALLBACKPROC)(system_window window,
                                             unsigned int  keycode,
                                             void*         user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: ascii key code or SYSTEM_WINDOW_KEY_*
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWKEYUPCALLBACKPROC)(system_window window,
                                           unsigned int  keycode,
                                           void*         user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWLEFTBUTTONDBLCLKCALLBACKPROC)(system_window           window,
                                                      int                     x,
                                                      int                     y,
                                                      system_window_vk_status key_status,
                                                      void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC)(system_window           window,
                                                    int                     x,
                                                    int                     y,
                                                    system_window_vk_status key_status,
                                                    void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWLEFTBUTTONUPCALLBACKPROC)(system_window           window,
                                                  int                     x,
                                                  int                     y,
                                                  system_window_vk_status key_status,
                                                  void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWMIDDLEBUTTONDBLCLKCALLBACKPROC)(system_window           window,
                                                        int                     x,
                                                        int                     y,
                                                        system_window_vk_status key_status,
                                                        void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWMIDDLEBUTTONDOWNCALLBACKPROC)(system_window           window,
                                                      int                     x,
                                                      int                     y,
                                                      system_window_vk_status key_status,
                                                      void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWMIDDLEBUTTONUPCALLBACKPROC)(system_window           window,
                                                    int                     x,
                                                    int                     y,
                                                    system_window_vk_status key_status,
                                                    void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWMOUSEMOVECALLBACKPROC)(system_window           window,
                                               int                     x,
                                               int                     y,
                                               system_window_vk_status key_status,
                                               void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: short representing the magnitude of the rotation. Under Windows, this value
 *        is a multiple of WHEEL_DELTA.
 *  arg2: system_window_vk_status
 *
 */
typedef bool (*PFNWINDOWMOUSEWHEELCALLBACKPROC)(system_window           window,
                                                int                     x,
                                                int                     y,
                                                short                   scroll_delta,
                                                system_window_vk_status key_status,
                                                void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWRIGHTBUTTONDBLCLKCALLBACKPROC)(system_window           window,
                                                       int                     x,
                                                       int                     y,
                                                       system_window_vk_status key_status,
                                                       void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWRIGHTBUTTONDOWNCALLBACKPROC)(system_window           window,
                                                     int                     x,
                                                     int                     y,
                                                     system_window_vk_status key_status,
                                                     void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: system_window_vk_status
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWRIGHTBUTTONUPCALLBACKPROC)(system_window           window,
                                                   int                     x,
                                                   int                     y,
                                                   system_window_vk_status key_status,
                                                   void*                   user_arg);

/** TODO.
 *
 *  Under Windows, the call-back is made directly from the window message loop thread.
 *  If needs be, only post window messages. Never send messages to the window, as this
 *  will result in a lock-up.
 *
 *  arg1: not used.
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWWINDOWCLOSEDCALLBACKPROC)(system_window window);

/** Call-back made FROM A RENDERING CONTEXT. Use it to release any GL objects before the
 *  window can be safely destroyed and the rendering handler released.
 *
 *  Under Windows, DO NOT send window messages from within these call-back handlers.
 *  Doing so will result in a lock-up.
 *
 *  arg1: not used.
 *  arg2: not used.
 *
 */
typedef bool (*PFNWINDOWWINDOWCLOSINGCALLBACKPROC)(system_window window);


/********************** CONTEXT MENU *************************************/
DECLARE_HANDLE(system_context_menu);

typedef void*                          system_context_menu_callback_argument;
typedef void (*PFNCONTEXTMENUCALLBACK)(system_context_menu_callback_argument);

/************************* EVENT *****************************************/
/** An instance of an event */
DECLARE_HANDLE(system_event);
/******************** FILE ENUMERATOR ************************************/
/** An instance of a file enumerator */
DECLARE_HANDLE(system_file_enumerator);
/*********************** FILE PACKER *************************************/
DECLARE_HANDLE(system_file_packer);
/******************** FILE SERIALIZER ************************************/
/* An instance of a file serializer */
DECLARE_HANDLE(system_file_serializer);
/********************* FILE UNPACKER *************************************/
DECLARE_HANDLE(system_file_unpacker);
/******************* FILE MULTIUNPACKER **********************************/
DECLARE_HANDLE(system_file_multiunpacker);
/*********************** 4X4 MATRIX **************************************/
/* An instance of 4x4 matrix object */
DECLARE_HANDLE(system_matrix4x4);
/********************** RESOURCE POOL ************************************/
/* An instance of system resource pool object */
DECLARE_HANDLE(system_resource_pool);
/* Block stored in system resource pool */
DECLARE_HANDLE(system_resource_pool_block);
/* Function pointer that will be used to initialize a new item whenever allocated for
 * the first time from resource pool.
 */
typedef void (*PFNSYSTEMRESOURCEPOOLINITBLOCK)(system_resource_pool_block);
/* Function pointer that will be used to deinitialize items that have already been initialized
 * and returned to the pool.
 */
typedef void (*PFNSYSTEMRESOURCEPOOLDEINITBLOCK)(system_resource_pool_block);

/************************ SCREEN MODE *************************************/
DECLARE_HANDLE(system_screen_mode);

/******************** MEMORY MANAGER **********************************/
DECLARE_HANDLE(system_memory_manager);
/******************** PIXEL FORMAT ***************************************/
DECLARE_HANDLE(system_pixel_format);
/************************** DAG ******************************************/
DECLARE_HANDLE(system_dag);

typedef void* system_dag_connection;
typedef void* system_dag_node;
typedef void* system_dag_node_value;

/******************** HASHED ANSI STRING *********************************/
/* An instance of a hashed ansi string */
DECLARE_HANDLE(system_hashed_ansi_string);
/******************* BIDIRECTIONAL LINKED LIST ********************************/
/* An instance of an bidirectional linked list */
DECLARE_HANDLE(system_list_bidirectional);

/* An instance of an bidirectional linked list item */
DECLARE_HANDLE(system_list_bidirectional_item);

/*********************** VARIANTS ****************************************/
/** Supported variant types */
typedef enum
{
    SYSTEM_VARIANT_ANSI_STRING,
    SYSTEM_VARIANT_BOOL,
    SYSTEM_VARIANT_INTEGER,
    SYSTEM_VARIANT_FLOAT,

    SYSTEM_VARIANT_UNDEFINED
} system_variant_type;

/** A variant instance */
DECLARE_HANDLE(system_variant);
/********************* LINEAR ALLOC **************************************/
/** Linear alloc handle */
DECLARE_HANDLE(system_linear_alloc_pin);
/********************** HASH64 MAP ***************************************/
/** 64-bit hash type */
typedef __uint64 system_hash64;
/** Hash64-based map type */
DECLARE_HANDLE(system_hash64map);
/** Type of an argument passed with on-removal call-back */
typedef void* _system_hash64map_on_remove_callback_argument;
/** Hash64-based map on-removal call-back function pointer type */
typedef void (*PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC)(_system_hash64map_on_remove_callback_argument);
/******************** RESIZABLE VECTOR ***********************************/
/** Represents a resizable vector instance */
DECLARE_HANDLE(system_resizable_vector);
/*************************** LOGGING *************************************/
/** Log priorites */
typedef enum
{
    LOGLEVEL_TRACE,          /* Tracing - stores information on called functions */
    LOGLEVEL_INFORMATION,    /* Info    - generic info helpful for debugging */
    LOGLEVEL_ERROR,          /* Error   - error notifications that will most likely not be followed by a crash */
    LOGLEVEL_FATAL           /* Fatal   - a crash following very likely. */
} system_log_priority;
/*************************** THREADS *************************************/
/** Thread id */
#ifdef _WIN32
    typedef HANDLE system_thread;
    typedef DWORD  system_thread_id;
#else
    typedef pthread_t system_thread;
    typedef pid_t     system_thread_id;
#endif

/** Thread entry point fucntion argument */
typedef void* system_threads_entry_point_argument;
/** Thread entry point function pointer */
typedef void (*PFNSYSTEMTHREADSENTRYPOINTPROC)(system_threads_entry_point_argument);
/************************** SEMAPHORES ***********************************/
/** Represents a single semaphore object */
DECLARE_HANDLE(system_semaphore);
/*********************** READ/WRITE MUTEXES ******************************/
/** Represents a single r/w mutex object */
DECLARE_HANDLE(system_read_write_mutex);
/** Defines supported access operations for r/w mutex object */
typedef enum
{
    ACCESS_READ,    /* Read access only  - allows multiple threads to read from the object */
    ACCESS_WRITE    /* Write access only - allows only one thread to access the object at a time */
} system_read_write_mutex_access_type;
/**************************** BARRIERS ***********************************/
DECLARE_HANDLE(system_barrier);
/******************************* BST *************************************/
/** TODO . true if <, false otherwise.*/
typedef bool (*system_bst_value_lower_func)(size_t key_size, void*, void*);
/** TODO . true if ==, false otherwise */
typedef bool (*system_bst_value_equals_func)(size_t key_size, void*, void*);

/** TODO */
DECLARE_HANDLE(system_bst);
/** TODO */
DECLARE_HANDLE(system_bst_value);
/** TODO */
DECLARE_HANDLE(system_bst_key);
/********************** CONDITION VARIABLE *******************************/
/** Represents a single condition variable */
DECLARE_HANDLE(system_cond_variable);
/************************ CRITICAL SECTION *******************************/
/** Represents a single critical section object */
DECLARE_HANDLE(system_critical_section);
/************************** RANDOMIZER ***********************************/
/** TODO */
DECLARE_HANDLE(system_randomizer);
/********************** CALL-BACK MANAGER *********************************/
DECLARE_HANDLE(system_callback_manager);
/************************* THREAD POOL ***********************************/
typedef enum
{
    THREAD_POOL_TASK_PRIORITY_FIRST,

    THREAD_POOL_TASK_PRIORITY_IDLE      = THREAD_POOL_TASK_PRIORITY_FIRST,
    THREAD_POOL_TASK_PRIORITY_NORMAL,
    THREAD_POOL_TASK_PRIORITY_CRITICAL,

    /* Always last */
    THREAD_POOL_TASK_PRIORITY_COUNT
} system_thread_pool_task_priority;
/** Typedef of an argument that will be passed with call-back. */
typedef void* system_thread_pool_callback_argument;
/** Thread pool call-back function pointer type */
typedef volatile void (*PFNSYSTEMTHREADPOOLCALLBACKPROC)(system_thread_pool_callback_argument);
/** Thread pool task descriptor */
DECLARE_HANDLE(system_thread_pool_task);
/** Thread pool task group descriptor */
DECLARE_HANDLE(system_thread_pool_task_group);

#endif /* SYSTEM_TYPES_H */
