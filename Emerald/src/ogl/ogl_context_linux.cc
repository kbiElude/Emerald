/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_win32.h"
#include "ogl/ogl_pixel_format_descriptor.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_types.h"
#include "system/system_window.h"
#include <GL/glx.h>
#include <X11/Xlib.h>


typedef struct _ogl_context_linux
{
    ogl_context context; /* DO NOT retain */

    /* MSAA support */
    uint32_t  n_supported_msaa_samples;
    uint32_t* supported_msaa_samples;

    /* WGL extensions */
    bool glx_ext_swap_control_tear_support;
    bool glx_sgi_swap_control_support;

    _ogl_context_linux()
    {
        context                           = NULL;
        glx_ext_swap_control_tear_support = false;
        glx_sgi_swap_control_support      = false;
        n_supported_msaa_samples          = 0;
        supported_msaa_samples            = NULL;
    }
} _ogl_context_linux;



/** TODO */
PRIVATE bool _ogl_context_linux_set_pixel_format_multisampling(_ogl_context_linux* linux_ptr,
                                                               uint32_t            n_samples)
{
    bool result = false;

#if 0
    ASSERT_ALWAYS_SYNC(win32_ptr->pWGLChoosePixelFormatARB != NULL,
                       "wglChoosePixelFormat() unavailable - please update your WGL implementation.");

    if (win32_ptr->pWGLChoosePixelFormatARB != NULL)
    {
        int                         alpha_bits  = 0;
        int                         depth_bits  = 0;
        ogl_pixel_format_descriptor pfd         = NULL;
        int                         rgb_bits[3] = {0, 0, 0};

        ogl_context_get_property(win32_ptr->context,
                                 OGL_CONTEXT_PROPERTY_PIXEL_FORMAT_DESCRIPTOR,
                                &pfd);

        ogl_pixel_format_descriptor_get_property(pfd,
                                                 OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_RED_BITS,
                                                 rgb_bits + 0);
        ogl_pixel_format_descriptor_get_property(pfd,
                                                 OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_GREEN_BITS,
                                                 rgb_bits + 1);
        ogl_pixel_format_descriptor_get_property(pfd,
                                                 OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_BLUE_BITS,
                                                 rgb_bits + 2);
        ogl_pixel_format_descriptor_get_property(pfd,
                                                 OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_COLOR_BUFFER_ALPHA_BITS,
                                                &alpha_bits);
        ogl_pixel_format_descriptor_get_property(pfd,
                                                 OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_DEPTH_BITS,
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
#endif

    return result;
}


/** Please see header for spec */
PUBLIC void ogl_context_linux_bind_to_current_thread(__in ogl_context_linux context_linux)
{
    _ogl_contextlinux* linux_ptr = (_ogl_context_linux*) context_linux;

#if 0
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
#endif
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_deinit(__in __post_invalid ogl_context_linux context_linux)
{
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");

#if 0
    if (::wglDeleteContext(win32_ptr->wgl_rendering_context) == FALSE)
    {
        LOG_ERROR("wglDeleteContext() failed.");
    }

    if (win32_ptr->opengl32_dll_handle != NULL)
    {
        ::FreeLibrary(win32_ptr->opengl32_dll_handle);

        win32_ptr->opengl32_dll_handle = NULL;
    }

#endif

    if (linux_ptr->supported_msaa_samples != NULL)
    {
        delete [] linux_ptr->supported_msaa_samples;

        linux_ptr->supported_msaa_samples = NULL;
    }

    delete linux_ptr;
    linux_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_deinit_global()
{
    /* Stub */
}

/** Please see header for spec */
PUBLIC void* ogl_context_linux_get_func_ptr(__in ogl_context_linux context_linux,
                                            __in const char*       name)
{
    void*               result    = NULL;
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(name      != NULL &&
                      linux_ptr != NULL,
                      "Input argument is NULL");

#if 0
    result = ::GetProcAddress(win32_ptr->opengl32_dll_handle,
                              name);

    if (result == NULL)
    {
        result = ::wglGetProcAddress(name);
    }
#endif

    return result;
}

/** Please see header for spec */
PUBLIC bool ogl_context_linux_get_property(__in  ogl_context_linux    context_linux,
                                           __in  ogl_context_property property,
                                           __out void*                out_result)
{
    bool                result    = false;
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    switch (property)
    {
#if 0
        case OGL_CONTEXT_PROPERTY_DC:
        {
            *((HDC*) out_result) = win32_ptr->device_context_handle;
                     result      = true;

            break;
        }

        case OGL_CONTEXT_PROPERTY_GL_CONTEXT:
        {
            *((HGLRC*) out_result) = win32_ptr->wgl_rendering_context;
                       result      = true;

            break;
        }
#endif

        case OGL_CONTEXT_PROPERTY_MSAA_N_SUPPORTED_SAMPLES:
        {
            *(uint32_t*) out_result = linux_ptr->n_supported_msaa_samples;
                         result     = true;

            break;
        }

        case OGL_CONTEXT_PROPERTY_MSAA_SUPPORTED_SAMPLES:
        {
            *(uint32_t**) out_result = linux_ptr->supported_msaa_samples;
                          result     = true;

            break;
        }

#if 0
        case OGL_CONTEXT_PROPERTY_RENDERING_CONTEXT:
        {
            *(HGLRC*) out_result = win32_ptr->wgl_rendering_context;
                      result     = true;

            break;
        }
#endif
    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_init(__in ogl_context                     context,
                                   __in PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation)
{
    _ogl_context_linux* new_linux_ptr = new (std::nothrow) _ogl_context_linux;

    ASSERT_DEBUG_SYNC(_display != NULL,
                      "Display is NULL");
    ASSERT_DEBUG_SYNC(new_linux_ptr != NULL,
                      "Out of memory");

    if (new_linux_ptr == NULL)
    {
        goto end;
    }

    new_linux_ptr->context = context;

    ogl_context_set_property(context,
                             OGL_CONTEXT_PROPERTY_PLATFORM_CONTEXT,
                            &new_linux_ptr);

    /* Create the context instance */
    bool                         allow_msaa              = false;
    ogl_pixel_format_descriptor  context_pfd             = NULL;
    GLXFBConfig*                 fb_configs_ptr          = NULL;
    int                          n_compatible_fb_configs = 0;
    uint32_t                     n_depth_bits            = 0;
    uint32_t                     n_rgba_bits[4]          = {0};
    system_window                window                  = NULL;
    XSetWindowAttributes         winattr;
    system_window_handle         window_handle           = NULL;
    XVisualInfo*                 visual_info_ptr         = NULL;

    int                          fb_config_attribute_list[2 * 6 /* rgba bits, depth bits, doublebuffering */ + 1 /* terminator */];
    int                          visual_attribute_list   [2 * 5 /* rgba bits, depth bits */                  + 2 /* doublebuffering, rgba */ + 1 /* terminator */];

    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_ALLOW_MSAA,
                              &allow_msaa);
    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_PIXEL_FORMAT_DESCRIPTOR,
                              &context_pfd);
    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);

    ogl_pixel_format_descriptor_get_property(context_pfd,
                                             OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_ALPHA_BITS,
                                             n_rgba_bits + 3);
    ogl_pixel_format_descriptor_get_property(context_pfd,
                                             OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_BLUE_BITS,
                                             n_rgba_bits + 2);
    ogl_pixel_format_descriptor_get_property(context_pfd,
                                             OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_DEPTH_BITS,
                                            &n_depth_bits);
    ogl_pixel_format_descriptor_get_property(context_pfd,
                                             OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_GREEN_BITS,
                                             n_rgba_bits + 1);
    ogl_pixel_format_descriptor_get_property(context_pfd,
                                             OGL_PIXEL_FORMAT_DESCRIPTOR_PROPERTY_RED_BITS,
                                             n_rgba_bits + 0);

    fb_config_attribute_list[0]  = GLX_RED_SIZE;     fb_config_attribute_list[1]  = n_rgba_bits[0];
    fb_config_attribute_list[2]  = GLX_GREEN_SIZE;   fb_config_attribute_list[3]  = n_rgba_bits[1];
    fb_config_attribute_list[4]  = GLX_BLUE_SIZE;    fb_config_attribute_list[5]  = n_rgba_bits[2];
    fb_config_attribute_list[6]  = GLX_ALPHA_SIZE;   fb_config_attribute_list[7]  = n_rgba_bits[3];
    fb_config_attribute_list[8]  = GLX_DEPTH_SIZE;   fb_config_attribute_list[9]  = n_depth_bits;
    fb_config_attribute_list[10] = GLX_DOUBLEBUFFER; fb_config_attribute_list[11] = GL_TRUE;
    fb_config_attribute_list[12] = None;

    visual_attribute_list[0]  = GLX_RED_SIZE;     visual_attribute_list[1]  = n_rgba_bits[0];
    visual_attribute_list[2]  = GLX_GREEN_SIZE;   visual_attribute_list[3]  = n_rgba_bits[1];
    visual_attribute_list[4]  = GLX_BLUE_SIZE;    visual_attribute_list[5]  = n_rgba_bits[2];
    visual_attribute_list[6]  = GLX_ALPHA_SIZE;   visual_attribute_list[7]  = n_rgba_bits[3];
    visual_attribute_list[8]  = GLX_DEPTH_SIZE;   visual_attribute_list[9]  = n_depth_bits;
    visual_attribute_list[10] = GLX_DOUBLEBUFFER;
    visual_attribute_list[11] = GLX_RGBA;
    visual_attribute_list[12] = None;

    fb_configs_ptr = glXChooseFBConfig(_display,
                                       _default_screen_index,
                                       fb_config_attribute_list,
                                      &n_compatible_fb_configs);

    ASSERT_DEBUG_SYNC(fb_configs_ptr != NULL,
                      "glXChooseFBConfig() returned NULL");
    ASSERT_DEBUG_SYNC(n_compatible_fb_configs != 0,
                      "No compatible framebuffer configs were reported");

    if (fb_configs_ptr          == NULL ||
        n_compatible_fb_configs == 0)
    {
        goto end;
    }

    visual_info_ptr = glXChooseVisual(_display,
                                      _default_screen_index,fb_config_attribute_list,
                                      visual_attribute_list);

    ASSERT_DEBUG_SYNC(visual_info_ptr != NULL,
                      "glXChooseVisual() returned NULL");

    if (visual_info_ptr == NULL)
    {
        goto end;
    }

    sedes;
#if 0
    winattr.background_pixel = 0;
    winattr.border_pixel = 0;
    winattr.colormap = XCreateColormap(d_dpy, root, visinfo->visual, AllocNone);
    winattr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
    unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    printf("Window depth %d, w %d h %d\n", visinfo->depth, width, height);
    d_win = XCreateWindow(d_dpy, root, 0, 0, width, height, 0, 
            visinfo->depth, InputOutput, visinfo->visual, mask, &winattr);
#endif

end:
    /* Clean up */
    if (fb_configs_ptr != NULL)
    {
        XFree(fb_configs_ptr);

        fb_configs_ptr = NULL;
    }

    if (visual_info_ptr != NULL)
    {
        XFree(visual_info_ptr);

        visual_info_ptr = NULL;
    }
)
GLXFBConfig *fbcfg = glXChooseFBConfig(d_dpy, scrnum, NULL, &elemc);
if (!fbcfg) 
    throw std::string("Couldn't get FB configs\n");
else
    printf("Got %d FB configs\n", elemc);

XVisualInfo *visinfo = glXChooseVisual(d_dpy, scrnum, attr);

if (!visinfo)
    throw std::string("Couldn't get a visual\n");

// Window parameters
XSetWindowAttributes winattr;
winattr.background_pixel = 0;

    /* Configure the device context handle to use the desired pixel format. */
    int pixel_format_index = ::ChoosePixelFormat(new_win32_ptr->device_context_handle,
                                                 system_pfd_ptr);

    if (pixel_format_index == 0)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Requested pixel format is unavailable on the running platform!");

        goto end_error;
    }

    /* Set the pixel format. */
    if (!::SetPixelFormat(new_win32_ptr->device_context_handle,
                          pixel_format_index,
                          system_pfd_ptr) )
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

#endif
    /* If context we are to share objects with is not NULL, lock corresponding renderer thread before continuing.
     * Otherwise GL impl could potentially forbid the context creation (happens on NViDiA drivers) */
    ogl_context           parent_context                   = NULL;
    system_window         parent_context_window            = NULL;
    ogl_rendering_handler parent_context_rendering_handler = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_PARENT_CONTEXT,
                            &parent_context);

    if (parent_context != NULL)
    {
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

#if 0
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
#endif

    /* Sharing? Unlock corresponding renderer thread */
    if (parent_context != NULL)
    {
        ogl_rendering_handler_unlock_bound_context(parent_context_rendering_handler);
    }

#if 0
    if (new_win32_ptr->wgl_rendering_context == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not create WGL rendering context. [GetLastError():%d]",
                           ::GetLastError() );

        goto end_error;
    }

    /* Initialize WGL extensions */
    _ogl_context_win32_initialize_wgl_extensions(new_win32_ptr);
#endif

    /* Call the provided function pointer to continue the context initialization process. */
    pInitContextAfterCreation(new_win32_ptr->context);

#if 0
    /* Release the temporary context now. */
    ::wglDeleteContext(temp_wgl_context);
#endif

end:
    return;

end_error:
    if (new_linux_ptr != NULL)
    {
        ogl_context_linux_deinit( (ogl_context_linux) new_linux_ptr);
    }

    return;
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_init_global()
{
    /* Stub */
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_init_msaa(__in ogl_context_linux context_linux)
{
    const ogl_context_gl_limits* limits_ptr       = NULL;
    _ogl_context_linux*          linux_ptr        = (_ogl_context_linux*) context_linux;
    int                          n_stored_entries = 0;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");
    ASSERT_DEBUG_SYNC(linux_ptr->n_supported_msaa_samples == 0,
                      "ogl_context_linux_init_msaa() called more than once for the same context");

    ogl_context_get_property(linux_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    for (int n_iteration = 0;
             n_iteration < 2;
           ++n_iteration)
    {
        if (n_iteration == 1)
        {
            /* If this is second iteration, we know how many entries we need to allocate for the array storing
             * information on supported "amount of samples" setting for given context. */
            linux_ptr->supported_msaa_samples = new uint32_t[linux_ptr->n_supported_msaa_samples];

            ASSERT_ALWAYS_SYNC(linux_ptr->supported_msaa_samples != NULL,
                               "Out of memory while allocating \"multisampling supported sample\" array.");
        }

        for (int n_samples = 1;
                 n_samples < limits_ptr->max_color_texture_samples;
               ++n_samples)
        {
            bool status = _ogl_context_linux_set_pixel_format_multisampling(linux_ptr,
                                                                            n_samples);

            if (status)
            {
                /* If this is first iteration, just count this supported samples setting in. */
                if (n_iteration == 0)
                {
                    linux_ptr->n_supported_msaa_samples++;
                }
                else
                {
                    /* Second one? Store it. */
                    linux_ptr->supported_msaa_samples[n_stored_entries] = n_samples;

                    n_stored_entries++;
                }
            }
        } /* for (int n_samples = 0; n_samples < _result->limits.max_color_texture_samples; ++n_samples) */
    } /* for (int n_iteration = 0; n_iteration < 2; ++n_iteration) */
}

/** Please see header for spec */
PUBLIC bool ogl_context_linux_set_property(__in ogl_context_linux    context_linux,
                                           __in ogl_context_property property,
                                           __in const void*          data)
{
    bool                result    = false;
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");

    switch (property)
    {
        case OGL_CONTEXT_PROPERTY_MSAA_SAMPLES:
        {
            result = _ogl_context_linux_set_pixel_format_multisampling(linux_ptr,
                                                                       *(uint32_t*) data);

            break;
        }

        case OGL_CONTEXT_PROPERTY_VSYNC_ENABLED:
        {
            bool vsync_enabled = *(bool*) data;

            ASSERT_ALWAYS_SYNC(linux_ptr->glx_swap_control_support,
                               "GLX_swap_control extension not supported. Update your drivers.");

#if 0
            if (win32_ptr->pWGLSwapIntervalEXT != NULL)
            {
                int  swap_interval = (win32_ptr->wgl_swap_control_tear_support) ? -1 : /* use adaptive vsync  */
                                                                                   1;  /* force regular vsync */

                result = win32_ptr->pWGLSwapIntervalEXT(vsync_enabled ? swap_interval
                                                                      : 0) == TRUE;

                ASSERT_DEBUG_SYNC(result,
                                  "Failed to set VSync");
            }
#endif

            break;
        }

    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_swap_buffers(__in ogl_context_linux context_linux)
{
    _ogl_context_linux* linux_ptr = (_ogl_context_win32*) context_linux;

#if 0
    ::SwapBuffers(win32_ptr->context_dc);
#endif
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_unbind_from_current_thread()
{
#if 0
    ::wglMakeCurrent(NULL,  /* HDC */
                     NULL); /* HGLRC */
#endif
}
