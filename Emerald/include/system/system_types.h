/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

/* Shared macros */
#define PUBLIC
#define PRIVATE static
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
#include "dll_exports.h"

#define REFCOUNT_INSERT_DECLARATIONS(prefix, public_handle_type)                                \
    PUBLIC EMERALD_API void prefix##_retain(__in __notnull public_handle_type);                 \
    PUBLIC EMERALD_API void prefix##_release(__inout __notnull public_handle_type&);

/******************* OBJECT MANAGER ***************************************/
#ifdef INCLUDE_OBJECT_MANAGER

    DECLARE_HANDLE(object_manager_directory);
    DECLARE_HANDLE(object_manager_item);

    typedef enum
    {
        OBJECT_TYPE_FIRST,

        OBJECT_TYPE_COLLADA_DATA = OBJECT_TYPE_FIRST,
        OBJECT_TYPE_CONTEXT_MENU,
        OBJECT_TYPE_CURVE_CONTAINER,
        OBJECT_TYPE_GFX_BFG_FONT_TABLE,
        OBJECT_TYPE_GFX_IMAGE,
        OBJECT_TYPE_LW_CURVE_DATASET,
        OBJECT_TYPE_LW_DATASET,
        OBJECT_TYPE_LW_LIGHT_DATASET,
        OBJECT_TYPE_LW_MATERIAL_DATASET,
        OBJECT_TYPE_LW_MESH_DATASET,
        OBJECT_TYPE_MESH,
        OBJECT_TYPE_MESH_MATERIAL,
        OBJECT_TYPE_OCL_CONTEXT,
        OBJECT_TYPE_OCL_KDTREE,
        OBJECT_TYPE_OCL_KERNEL,
        OBJECT_TYPE_OCL_PROGRAM,
        OBJECT_TYPE_OGL_CONTEXT,
        OBJECT_TYPE_OGL_CURVE_RENDERER,
        OBJECT_TYPE_OGL_PRIMITIVE_RENDERER,
        OBJECT_TYPE_OGL_PIPELINE,
        OBJECT_TYPE_OGL_PIXEL_FORMAT_DESCRIPTOR,
        OBJECT_TYPE_OGL_PROGRAM,
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
        OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_UBO,
        OBJECT_TYPE_SHADERS_VERTEX_FULLSCREEN,
        OBJECT_TYPE_SHADERS_VERTEX_M_VP_GENERIC,
        OBJECT_TYPE_SHADERS_VERTEX_MVP_COMBINER,
        OBJECT_TYPE_SHADERS_VERTEX_UBER,
        OBJECT_TYPE_SYSTEM_RANDOMIZER,
        OBJECT_TYPE_SYSTEM_WINDOW,

        OBJECT_TYPE_LAST,
        OBJECT_TYPE_UNKNOWN       = OBJECT_TYPE_LAST
    } object_manager_object_type;

#endif /* INCLUDE_OBJECT_MANAGER */

/************************ WINDOW ******************************************/
DECLARE_HANDLE(system_window);

#ifdef _WIN32
    /** TODO */
    typedef HWND system_window_handle;
    /** TODO */
    typedef HDC  system_window_dc;
#else
    #error Platform unsupported
#endif

/******************** TIME REPRESENTATION *********************************/
typedef uint32_t system_timeline_time;

/** TODO */
typedef enum system_window_mouse_cursor
{
    SYSTEM_WINDOW_MOUSE_CURSOR_MOVE,
    SYSTEM_WINDOW_MOUSE_CURSOR_ARROW,
    SYSTEM_WINDOW_MOUSE_CURSOR_HORIZONTAL_RESIZE,
    SYSTEM_WINDOW_MOUSE_CURSOR_VERTICAL_RESIZE,
    SYSTEM_WINDOW_MOUSE_CURSOR_CROSSHAIR,
    SYSTEM_WINDOW_MOUSE_CURSOR_ACTION_FORBIDDEN
};

/** TODO */
typedef enum system_window_vk_status
{
    SYSTEM_WINDOW_VK_STATUS_CONTROL_PRESSED       = MK_CONTROL,
    SYSTEM_WINDOW_VK_STATUS_LEFT_BUTTON_PRESSED   = MK_LBUTTON,
    SYSTEM_WINDOW_VK_STATUS_MIDDLE_BUTTON_PRESSED = MK_MBUTTON,
    SYSTEM_WINDOW_VK_STATUS_RIGHT_BUTTON_PRESSED  = MK_RBUTTON,
    SYSTEM_WINDOW_VK_STATUS_SHIFT_PRESSED         = MK_SHIFT
};

/** TODO */
typedef enum system_window_callback_func
{
    SYSTEM_WINDOW_CALLBACK_FUNC_EXIT_SIZE_MOVE,
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
    SYSTEM_WINDOW_CALLBACK_FUNC_CHAR
};

/** TODO */
typedef bool (*PFNWINDOWEXITSIZEMOVECALLBACKPROC)      (system_window);
/** TODO */
typedef bool (*PFNWINDOWMOUSEMOVECALLBACKPROC)         (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWLEFTBUTTONDOWNCALLBACKPROC)    (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWLEFTBUTTONUPCALLBACKPROC)      (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWLEFTBUTTONDBLCLKCALLBACKPROC)  (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWMIDDLEBUTTONDOWNCALLBACKPROC)  (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWMIDDLEBUTTONUPCALLBACKPROC)    (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWMIDDLEBUTTONDBLCLKCALLBACKPROC)(system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWRIGHTBUTTONDOWNCALLBACKPROC)   (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWRIGHTBUTTONUPCALLBACKPROC)     (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWRIGHTBUTTONDBLCLKCALLBACKPROC) (system_window, LONG, LONG, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWMOUSEWHEELCALLBACKPROC)        (system_window, unsigned short, unsigned short, short /* scroll delta */, system_window_vk_status, void*);
/** TODO */
typedef bool (*PFNWINDOWCHARCALLBACKPROC)              (system_window, unsigned short, void*);
/** TODO */
typedef bool (*PFNWINDOWKEYDOWNCALLBACKPROC)           (system_window, unsigned short, void*);
/** TODO */
typedef bool (*PFNWINDOWKEYUPCALLBACKPROC)             (system_window, unsigned short, void*);

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
/******************** FILE SERIALIZER ************************************/
/* An instance of a file serializer */
DECLARE_HANDLE(system_file_serializer);
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
/************************** DAG ******************************************/
DECLARE_HANDLE(system_dag);

typedef void* system_dag_connection;
typedef void* system_dag_node;
typedef void* system_dag_node_value;

/******************** HASHED ANSI STRING *********************************/
/* An instance of a hashed ansi string */
DECLARE_HANDLE(system_hashed_ansi_string);
/******************* UNROLLED LINKED LIST ********************************/
/* An instance of unrolled linked list */
DECLARE_HANDLE(system_unrolled_linked_list);
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
DECLARE_HANDLE(system_linear_alloc_pin_handle);
/********************** HASH64 MAP ***************************************/
/** 64-bit hash type */
typedef unsigned __int64 system_hash64;
/** Hash64-based map type */
DECLARE_HANDLE(system_hash64map);
/** Type of an argument passed with on-removal call-back */
typedef void* _system_hash64map_on_remove_callback_argument;
/** Hash64-based map on-removal call-back function pointer type */
typedef void (*PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC)(_system_hash64map_on_remove_callback_argument);
/******************** LOOKASIDE LIST *************************************/
/* Lookaside list type */
DECLARE_HANDLE(system_lookaside_list);
/* Type of a single item stored in a lookaside list */
DECLARE_HANDLE(system_lookaside_list_item);
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
typedef DWORD system_thread_id;
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
/************************ CRITICAL SECTION *******************************/
/** Represents a single critical section object */
DECLARE_HANDLE(system_critical_section);
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
DECLARE_HANDLE(system_thread_pool_task_descriptor);
/** Thread pool task group descriptor */
DECLARE_HANDLE(system_thread_pool_task_group_descriptor);

#endif /* SYSTEM_TYPES_H */
