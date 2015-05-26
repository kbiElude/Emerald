/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SHARED_H
#define SHARED_H

/* A nifty macro that will allow us to enforce strong type checking for void* handle types */
#ifndef _WIN32
    #ifdef DECLARE_HANDLE
        #undef DECLARE_HANDLE
    #endif /* DECLARE_HANDLE */

    #define DECLARE_HANDLE(name) struct name##__ { int unused; }; \
                                 typedef struct name##__ *name;
#endif /* _WIN32 */

/* CRT debugging */
#ifdef _DEBUG
    #define _CRTDBG_MAP_ALLOC

    #include <stdlib.h>
    #include <crtdbg.h>
#endif /* _DEBUG */

/* Platform-specific typedefs */
#ifdef __linux__
    #include <inttypes.h>

    typedef int64_t  __int64;
    typedef uint64_t __uint64;
#else
    /* __int64 is defined */
    typedef unsigned __int64 __uint64;
#endif

/* Windows-specific bits */
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 
    #endif

    /* Disable security warnings */
    #define _CRT_SECURE_NO_WARNINGS

    /* >= WinXP */
    #ifndef WINVER
        #define WINVER 0x0501
    #endif /* WINVER */

    #include <windows.h>
#endif /* _WIN32 */

/* CMake-driven config */
#include "config.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <new>
#include "ogl/gl3.h"
#include "ogl/glext.h"
#include "system/system_types.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

#ifdef _WIN32
    #include "ogl/wglext.h"
#endif /* _WIN32 */

#ifdef INCLUDE_OBJECT_MANAGER

    #include "object_manager/object_manager_general.h"

    #define GET_OBJECT_PATH(object_name, object_type, scene_name) \
        object_manager_get_object_path(object_name, object_type, scene_name)

    #define REGISTER_REFCOUNTED_OBJECT(object_type, ptr, path) \
        _object_manager_register_refcounted_object(ptr, path, __FILE__, __LINE__, object_type);

    #define UNREGISTER_REFCOUNTED_OBJECT(path) \
        _object_manager_unregister_refcounted_object(path);
#else

    #define GET_OBJECT_PATH             (a, b, c)
    #define REGISTER_REFCOUNTED_OBJECT  (a, b, c)
    #define UNREGISTER_REFCOUNTED_OBJECT(a)

#endif /* INCLUDE_OBJECT_MANAGER */


#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x)        STRINGIZE_HELPER(x)
#define WARNING(text)       message(__FILE__"(" STRINGIZE(__LINE__) "): warning: " #text)

#define REFCOUNT_INSERT_VARIABLES uint32_t                  refcount_counter;                \
                                  void                    (*refcount_pfn_on_release)(void*); \
                                  system_hashed_ansi_string path;

#define REFCOUNT_INSERT_INIT_CODE(private_handle, type, registration_path) \
    private_handle->refcount_counter        = 1;                           \
    private_handle->refcount_pfn_on_release = NULL;                        \
    private_handle->path                    = registration_path;           \
    REGISTER_REFCOUNTED_OBJECT(type, private_handle, path)

#define REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(private_handle, on_release, type, registration_path) \
    private_handle->refcount_counter        = 1;                                                            \
    private_handle->refcount_pfn_on_release = on_release;                                                   \
    private_handle->path                    = registration_path;                                            \
    REGISTER_REFCOUNTED_OBJECT(type, private_handle, registration_path)

/* This one is tricky and will only work with VS. If a resource has already been released and we're using a debug build,
 * the memory will be filled with 0xfeeefeee, so we can easily check whether we are not dealing with a double release problem.
 */
#if defined(_DEBUG) && defined(_WIN32)
    #define REFCOUNT_INSERT_IMPLEMENTATION_HELPER \
        ASSERT_DEBUG_SYNC(ptr->refcount_counter != 0xFEEEFEEE, "This object has already been released!");
#else
    #define REFCOUNT_INSERT_IMPLEMENTATION_HELPER 
#endif

#define REFCOUNT_INSERT_IMPLEMENTATION(prefix, public_handle_type, private_handle_type)         \
    PUBLIC EMERALD_API void prefix##_retain(__in __notnull public_handle_type handle)           \
    {                                                                                           \
        ::InterlockedIncrement( &((private_handle_type*)handle)->refcount_counter);             \
    }                                                                                           \
    PUBLIC EMERALD_API void prefix##_release(public_handle_type& handle)                        \
    {                                                                                           \
        private_handle_type* ptr = (private_handle_type*)handle;                                \
        REFCOUNT_INSERT_IMPLEMENTATION_HELPER                                                   \
        if (::InterlockedDecrement(&ptr->refcount_counter) == 0)                                \
        {                                                                                       \
            if (ptr->refcount_pfn_on_release != NULL)                                           \
            {                                                                                   \
                ptr->refcount_pfn_on_release(ptr);                                              \
            }                                                                                   \
            LOG_INFO("Releasing %p (type:%s)", handle, STRINGIZE(prefix) );                     \
                                                                                                \
            UNREGISTER_REFCOUNTED_OBJECT(ptr->path);                                            \
            delete (private_handle_type*) handle;                                               \
            handle = NULL;                                                                      \
        }                                                                                       \
    }

/* Pretty useful rad->deg and deg->rad macros*/
#define DEG_TO_RAD(x) (x / 360.0f * 2 * 3.14152965f)
#define RAD_TO_DEG(x) (x * 360.0f / 2 / 3.14152965f)

#endif /* SHARED_H */
