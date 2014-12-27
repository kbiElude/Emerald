/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_capabilities.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resources.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include "system/system_time.h"
#include "system/system_types.h"
#include "system/system_window.h"
#include "system/system_variant.h"
#include <crtdbg.h>

#ifdef INCLUDE_WEBCAM_MANAGER
    #include "webcam/webcam_manager.h"
#endif

#ifdef INCLUDE_OBJECT_MANAGER
    #include "object_manager/object_manager_general.h"
#endif

#ifdef INCLUDE_CURVE_EDITOR
    #include "curve_editor/curve_editor_general.h"
#endif

#include "ocl/ocl_general.h"


#include <CommCtrl.h>
#pragma comment(lib, "comctl32.lib")

bool      _deinited        = false;
HINSTANCE _global_instance = NULL;

/* Forward declarations */
int main_deinit();


/* Pelase see header for specification */
PUBLIC EMERALD_API void main_force_deinit()
{
    main_deinit();
}

/** Initializes all sub-modules. Called when DLL is about to be loaded into process' space.
 *
 *  NOTE: Win32 can have problems initializing modules that try to load libraries.
 *        If needed, please load libraries elsewhere.
 */
void main_init()
{
    ::InitCommonControls();

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF);
#endif

    _system_log_init();

    system_callback_manager_init  ();
    system_capabilities_init      ();
    system_hashed_ansi_string_init();

    _system_time_init();
    _system_assertions_init();
    _system_threads_init();
    _system_thread_pool_init();
    _system_matrix4x4_init();
    _system_window_init();
    _system_variants_init();

    #ifdef INCLUDE_WEBCAM_MANAGER
        _webcam_manager_init();
    #endif

    #ifdef INCLUDE_OBJECT_MANAGER
        _object_manager_init();
    #endif

    #ifdef INCLUDE_CURVE_EDITOR
        _curve_editor_init();
    #endif

    #ifdef INCLUDE_OPENCL
        _ocl_init();
    #endif
}

/** Deinitializes all sub-modules. Called when DLL is about to be unloaded from process' space.
 *
 */
int main_deinit()
{
    if (!_deinited)
    {
        _system_resources_deinit();

        _system_assertions_deinit();
        _system_thread_pool_deinit();
        _system_threads_deinit();
        _system_matrix4x4_deinit();
        _system_window_deinit();
        _system_variants_deinit();
        _system_time_deinit();

        #ifdef INCLUDE_WEBCAM_MANAGER
            _webcam_manager_deinit();
        #endif

        #ifdef INCLUDE_OBJECT_MANAGER
            _object_manager_deinit();
        #endif

        #ifdef INCLUDE_CURVE_EDITOR
            _curve_editor_deinit();
        #endif

        #ifdef INCLUDE_OPENCL
            _ocl_deinit();
        #endif

        system_hashed_ansi_string_deinit();

        _system_log_deinit();

        _deinited = true;
    }

    return 0;
}


#ifdef _WIN32
    /** Please see header for specification */
    EMERALD_API BOOL WINAPI DllMain(__in HINSTANCE instance, __in DWORD reason,__in LPVOID reserved)
    {
        switch (reason)
        {
            case DLL_PROCESS_ATTACH:
            {
                _global_instance = instance;

                /* DLL loaded into the virtual address space of the current process */
                main_init();

                break;
            }

            case DLL_PROCESS_DETACH:
            {
                /* DLL unloaded from the virtual address space of the calling process */
                main_deinit();

                break;
            }
        }

        return TRUE;
    }
#endif
