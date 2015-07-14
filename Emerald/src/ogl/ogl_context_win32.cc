/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_win32.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_types.h"
#include "system/system_window.h"


typedef struct _ogl_context_win32
{
    ogl_context context; /* DO NOT retain */

    HDC     device_context_handle;
    HMODULE opengl32_dll_handle;
    HGLRC   wgl_rendering_context;

    /* WGL extensions */
    bool wgl_swap_control_support;
    bool wgl_swap_control_tear_support;

    PFNWGLCHOOSEPIXELFORMATARBPROC      pWGLChoosePixelFormatARB;
    PFNWGLCREATECONTEXTATTRIBSARBPROC   pWGLCreateContextAttribsARB;
    PFNWGLGETEXTENSIONSSTRINGEXTPROC    pWGLGetExtensionsStringEXT;
    PFNWGLSWAPINTERVALEXTPROC           pWGLSwapIntervalEXT;

    _ogl_context_win32()
    {
        context                       = NULL;
        device_context_handle         = NULL;
        opengl32_dll_handle           = NULL;
        pWGLChoosePixelFormatARB      = NULL;
        pWGLCreateContextAttribsARB   = NULL;
        pWGLGetExtensionsStringEXT    = NULL;
        pWGLSwapIntervalEXT           = NULL;
        wgl_rendering_context         = NULL;
        wgl_swap_control_support      = false;
        wgl_swap_control_tear_support = false;
    }
} _ogl_context_win32;


/** TODO */
PRIVATE void _ogl_context_win32_initialize_wgl_extensions(_ogl_context_win32* context_ptr)
{
    if (context_ptr->pWGLGetExtensionsStringEXT != NULL)
    {
        const char* wgl_extensions = context_ptr->pWGLGetExtensionsStringEXT();

        /* Is EXT_wgl_swap_control supported? */
        context_ptr->wgl_swap_control_support = (strstr(wgl_extensions,
                                                        "WGL_EXT_swap_control") != NULL);

        if (context_ptr->wgl_swap_control_support)
        {
            context_ptr->pWGLSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) ::wglGetProcAddress("wglSwapIntervalEXT");
        }

        /* Is EXT_WGL_swap_control_tear supported? */
        context_ptr->wgl_swap_control_tear_support = (strstr(wgl_extensions,
                                                             "WGL_EXT_swap_control_tear") != NULL);
    } /* if (pWGLGetExtensionsString != NULL) */
}

/** TODO */
PRIVATE bool _ogl_context_win32_set_pixel_format_multisampling(_ogl_context_win32* win32_ptr,
                                                               uint32_t            n_samples)
{
    bool result = false;

    ASSERT_ALWAYS_SYNC(win32_ptr->pWGLChoosePixelFormatARB != NULL,
                       "wglChoosePixelFormat() unavailable - please update your WGL implementation.");

    if (win32_ptr->pWGLChoosePixelFormatARB != NULL)
    {
        int                 alpha_bits  = 0;
        int                 depth_bits  = 0;
        system_pixel_format pfd         = NULL;
        int                 rgb_bits[3] = {0, 0, 0};

        ogl_context_get_property(win32_ptr->context,
                                 OGL_CONTEXT_PROPERTY_PIXEL_FORMAT,
                                &pfd);

        system_pixel_format_get_property(pfd,
                                         SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS,
                                         rgb_bits + 0);
        system_pixel_format_get_property(pfd,
                                         SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS,
                                         rgb_bits + 1);
        system_pixel_format_get_property(pfd,
                                         SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS,
                                         rgb_bits + 2);
        system_pixel_format_get_property(pfd,
                                         SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS,
                                        &alpha_bits);
        system_pixel_format_get_property(pfd,
                                         SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                        &depth_bits);

        int   attributes[]       = {WGL_DRAW_TO_WINDOW_ARB,           GL_TRUE,
                                    WGL_SUPPORT_OPENGL_ARB,           GL_TRUE,
                                    WGL_ACCELERATION_ARB,             WGL_FULL_ACCELERATION_ARB,
                                    WGL_COLOR_BITS_ARB,               rgb_bits[0] + rgb_bits[1] + rgb_bits[2],
                                    WGL_ALPHA_BITS_ARB,               alpha_bits,
                                    WGL_DEPTH_BITS_ARB,               depth_bits,
                                    WGL_STENCIL_BITS_ARB,             0,
                                    WGL_DOUBLE_BUFFER_ARB,            GL_TRUE,
                                    WGL_SAMPLE_BUFFERS_ARB,           GL_TRUE,
                                    WGL_SAMPLES_ARB,                  n_samples,
                                    WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT, GL_TRUE,
                                    0,                                0};
        float float_attributes[] = {0.0f, 0.0f};
        UINT  n_pixel_formats    = 0;
        int   pixel_format       = 0;

        result = (win32_ptr->pWGLChoosePixelFormatARB(win32_ptr->device_context_handle,
                                                      attributes,
                                                      float_attributes,
                                                      1,
                                                      &pixel_format,
                                                      &n_pixel_formats) != 0 && n_pixel_formats > 0);
    }

    return result;
}


/** Please see header for spec */
PUBLIC void ogl_context_win32_bind_to_current_thread(ogl_context_win32 context_win32)
{
    _ogl_context_win32* win32_ptr = (_ogl_context_win32*) context_win32;

    if (win32_ptr != NULL)
    {
        ::wglMakeCurrent(win32_ptr->device_context_handle,
                         win32_ptr->wgl_rendering_context);
    }
    else
    {
        ::wglMakeCurrent(NULL,  /* HDC   */
                         NULL); /* HGLRC */
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_win32_deinit(ogl_context_win32 context_win32)
{
    _ogl_context_win32* win32_ptr = (_ogl_context_win32*) context_win32;

    ASSERT_DEBUG_SYNC(win32_ptr != NULL,
                      "Input argument is NULL");

    if (::wglDeleteContext(win32_ptr->wgl_rendering_context) == FALSE)
    {
        LOG_ERROR("wglDeleteContext() failed.");
    }

    if (win32_ptr->opengl32_dll_handle != NULL)
    {
        ::FreeLibrary(win32_ptr->opengl32_dll_handle);

        win32_ptr->opengl32_dll_handle = NULL;
    }

    delete win32_ptr;
    win32_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void* ogl_context_win32_get_func_ptr(ogl_context_win32 context_win32,
                                            const char*       name)
{
    void*               result    = NULL;
    _ogl_context_win32* win32_ptr = (_ogl_context_win32*) context_win32;

    ASSERT_DEBUG_SYNC(name      != NULL &&
                      win32_ptr != NULL,
                      "Input argument is NULL");

    result = ::GetProcAddress(win32_ptr->opengl32_dll_handle,
                              name);

    if (result == NULL)
    {
        result = ::wglGetProcAddress(name);
    }

    return result;
}

/** Please see header for spec */
PUBLIC bool ogl_context_win32_get_property(ogl_context_win32    context_win32,
                                           ogl_context_property property,
                                           void*                out_result)
{
    bool                result    = false;
    _ogl_context_win32* win32_ptr = (_ogl_context_win32*) context_win32;

    switch (property)
    {
        case OGL_CONTEXT_PROPERTY_DC:
        {
            *((HDC*) out_result) = win32_ptr->device_context_handle;
                     result      = true;

            break;
        }

        case OGL_CONTEXT_PROPERTY_RENDERING_CONTEXT:
        {
            *(HGLRC*) out_result = win32_ptr->wgl_rendering_context;
                      result     = true;

            break;
        }
    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_win32_init(ogl_context                     context,
                                   PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation)
{
    _ogl_context_win32* new_win32_ptr = new (std::nothrow) _ogl_context_win32;

    ASSERT_DEBUG_SYNC(new_win32_ptr != NULL,
                      "Out of memory");

    if (new_win32_ptr == NULL)
    {
        goto end;
    }

    new_win32_ptr->context = context;

    ogl_context_set_property(context,
                             OGL_CONTEXT_PROPERTY_PLATFORM_CONTEXT,
                            &new_win32_ptr);

    /* Load up the ICD */
    new_win32_ptr->opengl32_dll_handle = ::LoadLibraryA("opengl32.dll");

    ASSERT_ALWAYS_SYNC(new_win32_ptr->opengl32_dll_handle != NULL,
                       "Could not load opengl32.dll");

    /* Create the context instance */
    system_pixel_format          context_pf    = NULL;
    const PIXELFORMATDESCRIPTOR* system_pf_ptr = NULL;
    system_window                window        = NULL;
    system_window_handle         window_handle = NULL;

    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_PIXEL_FORMAT,
                              &context_pf);
    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DC,
                              &new_win32_ptr->device_context_handle);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);

    system_pixel_format_get_property(context_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DESCRIPTOR_PTR,
                                    &system_pf_ptr);

    if (system_pf_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve pixel format descriptor");

        goto end_error;
    }

    /* Configure the device context handle to use the desired pixel format. */
    int pixel_format_index = ::ChoosePixelFormat(new_win32_ptr->device_context_handle,
                                                 system_pf_ptr);

    if (pixel_format_index == 0)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Requested pixel format is unavailable on the running platform!");

        goto end_error;
    }

    /* Set the pixel format. */
    if (!::SetPixelFormat(new_win32_ptr->device_context_handle,
                          pixel_format_index,
                          system_pf_ptr) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not set pixel format.");

        goto end_error;
    }

    /* Create a temporary WGL context that we will use to initialize WGL context for the target version of OpenGL */
    HGLRC temp_wgl_context = ::wglCreateContext(new_win32_ptr->device_context_handle);

    ::wglMakeCurrent(new_win32_ptr->device_context_handle,
                     temp_wgl_context);

    /* Create WGL rendering context */
    new_win32_ptr->pWGLChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC)    ::wglGetProcAddress("wglChoosePixelFormatARB");
    new_win32_ptr->pWGLCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) ::wglGetProcAddress("wglCreateContextAttribsARB");
    new_win32_ptr->pWGLGetExtensionsStringEXT  = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)  ::wglGetProcAddress("wglGetExtensionsStringEXT");

    /* Okay, let's check the func ptr */
    if (new_win32_ptr->pWGLCreateContextAttribsARB == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not obtain func ptr to wglCreateContextAttribsARB! Update your drivers.");

        goto end_error;
    }

    /* If context we are to share objects with is not NULL, lock corresponding renderer thread before continuing.
     * Otherwise GL impl could potentially forbid the context creation (happens on NViDiA drivers) */
    ogl_context           parent_context                   = NULL;
    system_window         parent_context_window            = NULL;
    HGLRC                 parent_context_rendering_context = NULL;
    ogl_rendering_handler parent_context_rendering_handler = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_PARENT_CONTEXT,
                            &parent_context);

    if (parent_context != NULL)
    {
        ogl_context_get_property(parent_context,
                                 OGL_CONTEXT_PROPERTY_RENDERING_CONTEXT,
                                &parent_context_rendering_context);
        ogl_context_get_property(parent_context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &parent_context_window);

        system_window_get_property(parent_context_window,
                                   SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                  &parent_context_rendering_handler);

        ASSERT_DEBUG_SYNC(parent_context_rendering_handler != NULL,
                          "No rendering handler attached to the parent context");

        ogl_rendering_handler_lock_bound_context(parent_context_rendering_handler);
    }

    /* Okay, try creating the context */
    int              context_major_version      = 0;
    int              context_minor_version      = 0;
    int              context_profile_mask_key   = 0;
    int              context_profile_mask_value = 0;
    ogl_context_type context_type               = OGL_CONTEXT_TYPE_UNDEFINED;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_MAJOR_VERSION,
                            &context_major_version);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_MINOR_VERSION,
                            &context_minor_version);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        context_profile_mask_key   = WGL_CONTEXT_PROFILE_MASK_ARB;
        context_profile_mask_value = WGL_CONTEXT_ES2_PROFILE_BIT_EXT;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type requested");
    }

    const int wgl_attrib_list[]     = {WGL_CONTEXT_MAJOR_VERSION_ARB, context_major_version,
                                       WGL_CONTEXT_MINOR_VERSION_ARB, context_minor_version,
                                       WGL_CONTEXT_FLAGS_ARB,
    #ifdef _DEBUG
                                                                      WGL_CONTEXT_DEBUG_BIT_ARB,
    #else
                                                                      0,
    #endif
                                      WGL_CONTEXT_FLAGS_ARB,          WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                      context_profile_mask_key,       context_profile_mask_value,
                                      0
                                      };

    new_win32_ptr->wgl_rendering_context = new_win32_ptr->pWGLCreateContextAttribsARB(new_win32_ptr->device_context_handle,
                                                                                      parent_context_rendering_context,
                                                                                      wgl_attrib_list);

    /* Sharing? Unlock corresponding renderer thread */
    if (parent_context != NULL)
    {
        ogl_rendering_handler_unlock_bound_context(parent_context_rendering_handler);
    }

    if (new_win32_ptr->wgl_rendering_context == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not create WGL rendering context. [GetLastError():%d]",
                           ::GetLastError() );

        goto end_error;
    }

    /* Initialize WGL extensions */
    _ogl_context_win32_initialize_wgl_extensions(new_win32_ptr);

    /* Call the provided function pointer to continue the context initialization process. */
    pInitContextAfterCreation(new_win32_ptr->context);

    /* Release the temporary context now. */
    ::wglDeleteContext(temp_wgl_context);

end:
    return;

end_error:
    if (new_win32_ptr != NULL)
    {
        ogl_context_win32_deinit( (ogl_context_win32) new_win32_ptr);
    }

    return;
}

/** Please see header for spec */
PUBLIC bool ogl_context_win32_set_property(ogl_context_win32    context_win32,
                                           ogl_context_property property,
                                           const void*          data)
{
    bool                result    = false;
    _ogl_context_win32* win32_ptr = (_ogl_context_win32*) context_win32;

    ASSERT_DEBUG_SYNC(win32_ptr != NULL,
                      "Input argument is NULL");

    switch (property)
    {
        case OGL_CONTEXT_PROPERTY_VSYNC_ENABLED:
        {
            bool vsync_enabled = *(bool*) data;

            ASSERT_ALWAYS_SYNC(win32_ptr->wgl_swap_control_support,
                               "WGL_EXT_swap_control extension not supported. Update your drivers.");

            if (win32_ptr->pWGLSwapIntervalEXT != NULL)
            {
                int  swap_interval = (win32_ptr->wgl_swap_control_tear_support) ? -1 : /* use adaptive vsync  */
                                                                                   1;  /* force regular vsync */

                result = win32_ptr->pWGLSwapIntervalEXT(vsync_enabled ? swap_interval
                                                                      : 0) == TRUE;

                ASSERT_DEBUG_SYNC(result,
                                  "Failed to set VSync");
            }

            break;
        }

    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_win32_swap_buffers(ogl_context_win32 context_win32)
{
    _ogl_context_win32* win32_ptr = (_ogl_context_win32*) context_win32;

    ::SwapBuffers(win32_ptr->device_context_handle);
}

/** Please see header for spec */
PUBLIC void ogl_context_win32_unbind_from_current_thread(ogl_context_win32 context_win32)
{
    ::wglMakeCurrent(NULL,  /* HDC */
                     NULL); /* HGLRC */
}
