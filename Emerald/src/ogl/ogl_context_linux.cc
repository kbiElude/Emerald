/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#define GLX_GLXEXT_PROTOTYPES

#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_linux.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_types.h"
#include "system/system_window.h"
#include <X11/Xlib.h>


typedef struct _ogl_context_linux
{
    ogl_context context; /* DO NOT retain */
    GLXContext  rendering_context;

    /* MSAA support */
    uint32_t  n_supported_msaa_samples;
    uint32_t* supported_msaa_samples;

    /* GLX extensions */
    bool glx_ext_swap_control_support;
    bool glx_ext_swap_control_tear_support;

    PFNGLXSWAPINTERVALEXTPROC pGLXSwapIntervalEXT; /* GL_EXT_swap_control */

    _ogl_context_linux()
    {
        context                           = NULL;
        glx_ext_swap_control_support      = false;
        glx_ext_swap_control_tear_support = false;
        n_supported_msaa_samples          = 0;
        pGLXSwapIntervalEXT               = NULL;
        rendering_context                 = NULL;
        supported_msaa_samples            = NULL;
    }
} _ogl_context_linux;



/** TODO
 *
 *  @return TODO. Must be freed with delete []
 */
PRIVATE int* _ogl_context_linux_get_fb_config_attribute_list(system_pixel_format pixel_format)
{
    uint32_t n_depth_bits   = 0;
    uint32_t n_rgba_bits[4] = {0};
    int*     result         = new int[13];

    ASSERT_DEBUG_SYNC (pixel_format != NULL,
                       "Input argument is NULL");
    ASSERT_ALWAYS_SYNC(result != NULL,
                       "Out of memory");

    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS,
                                     n_rgba_bits + 3);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS,
                                     n_rgba_bits + 2);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                    &n_depth_bits);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS,
                                     n_rgba_bits + 1);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS,
                                     n_rgba_bits + 0);

    result[0]  = GLX_RED_SIZE;     result[1]  = n_rgba_bits[0];
    result[2]  = GLX_GREEN_SIZE;   result[3]  = n_rgba_bits[1];
    result[4]  = GLX_BLUE_SIZE;    result[5]  = n_rgba_bits[2];
    result[6]  = GLX_ALPHA_SIZE;   result[7]  = n_rgba_bits[3];
    result[8]  = GLX_DEPTH_SIZE;   result[9]  = n_depth_bits;
    result[10] = GLX_DOUBLEBUFFER; result[11] = GL_TRUE;
    result[12] = None;

    return result;
}

/** TODO
 *
 *  @return TODO. Must be freed with delete []
 */

PRIVATE int* _ogl_context_linux_get_visual_attribute_list(system_pixel_format pixel_format)
{
    uint32_t n_depth_bits   = 0;
    uint32_t n_rgba_bits[4] = {0};
    int*     result         = new int[13];

    ASSERT_DEBUG_SYNC (pixel_format != NULL,
                       "Input argument is NULL");
    ASSERT_ALWAYS_SYNC(result != NULL,
                       "Out of memory");

    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS,
                                     n_rgba_bits + 3);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS,
                                     n_rgba_bits + 2);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                    &n_depth_bits);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS,
                                     n_rgba_bits + 1);
    system_pixel_format_get_property(pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS,
                                     n_rgba_bits + 0);

    result[0]  = GLX_RED_SIZE;     result[1]  = n_rgba_bits[0];
    result[2]  = GLX_GREEN_SIZE;   result[3]  = n_rgba_bits[1];
    result[4]  = GLX_BLUE_SIZE;    result[5]  = n_rgba_bits[2];
    result[6]  = GLX_ALPHA_SIZE;   result[7]  = n_rgba_bits[3];
    result[8]  = GLX_DEPTH_SIZE;   result[9]  = n_depth_bits;
    result[10] = GLX_DOUBLEBUFFER;
    result[11] = GLX_RGBA;
    result[12] = None;

    return result;
}

/** TODO */
PRIVATE void _ogl_context_linux_initialize_glx_extensions(__inout __notnull _ogl_context_linux* context_ptr)
{
    system_window  context_window              = NULL;
    Display*       context_window_display      = NULL;
    int            context_window_screen_index = -1;
    const char*    glx_extensions              = NULL;

    ogl_context_get_property(context_ptr->context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &context_window);

    ASSERT_DEBUG_SYNC(context_window != NULL,
                      "No window associated with the OpenGL (ES) rendering context");

    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_DISPLAY,
                              &context_window_display);
    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_SCREEN_INDEX,
                              &context_window_screen_index);

    ASSERT_DEBUG_SYNC(context_window_platform != NULL,
                      "No system handle associated with a system_window handle");

    /* Retrieve the GLX extensions */
    glx_extensions = glXQueryExtensionsString(context_window_display,
                                              context_window_screen_index);

    /* Is EXT_wgl_swap_control supported? */
    context_ptr->glx_ext_swap_control_support = (strstr(glx_extensions,
                                                        "GLX_EXT_swap_control") != NULL);

    if (context_ptr->glx_ext_swap_control_support)
    {
        context_ptr->pGLXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress( (const unsigned char*) "glXSwapIntervalEXT");

        ASSERT_DEBUG_SYNC(context_ptr->pGLXSwapIntervalEXT != NULL,
                          "glXSwapIntervalEXT() entry-point is NULL");
    }

    /* Is EXT_WGL_swap_control_tear supported? */
    context_ptr->glx_ext_swap_control_tear_support = (strstr(glx_extensions,
                                                             "GLX_EXT_swap_control_tear") != NULL);
}

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
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

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

    if (linux_ptr->rendering_context != NULL)
    {
        glXDestroyContext(_display,
                          linux_ptr->rendering_context);

        linux_ptr->rendering_context = NULL;
    }

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

/** Please see header for specification */
PUBLIC void ogl_context_linux_get_fb_configs_for_gl_window(system_window window,
                                                           GLXFBConfig** out_fb_configs_ptr,
                                                           int*          out_n_compatible_fb_configs_ptr)
{
    int*                fb_config_attribute_list = NULL;
    Display*            window_display           = NULL;
    system_pixel_format window_pixel_format      = NULL;
    int                 window_screen_index      = -1;

    ASSERT_DEBUG_SYNC(window                          != NULL &&
                      out_fb_configs_ptr              != NULL &&
                      out_n_compatible_fb_configs_ptr != NULL,
                      "Input arguments are NULL");

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DISPLAY,
                              &window_display);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &window_pixel_format);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_SCREEN_INDEX,
                              &window_screen_index);

    fb_config_attribute_list = _ogl_context_linux_get_fb_config_attribute_list(window_pixel_format);
    *out_fb_configs_ptr      = glXChooseFBConfig                              (window_display,
                                                                               window_screen_index,
                                                                               fb_config_attribute_list,
                                                                               out_n_compatible_fb_configs_ptr);

    ASSERT_DEBUG_SYNC(*out_fb_configs_ptr              != NULL &&
                      *out_n_compatible_fb_configs_ptr != 0,
                      "glXChooseFBConfig() returned NULL");

    delete [] fb_config_attribute_list;
    fb_config_attribute_list = NULL;
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

        case OGL_CONTEXT_PROPERTY_RENDERING_CONTEXT:
        {
            *(GLXContext*) out_result = linux_ptr->rendering_context;
                           result     = true;

            break;
        }
    } /* switch (property) */

    return result;
}

/** Please see header for specification */
PUBLIC XVisualInfo* ogl_context_linux_get_visual_info_for_gl_window(system_window window)
{
    int*                fb_config_attribute_list = NULL;
    GLXFBConfig*        fb_configs_ptr           = NULL;
    int                 n_compatible_fb_configs  = 0;
    uint32_t            n_depth_bits             = 0;
    uint32_t            n_rgba_bits[4]           = {0};
    int*                visual_attribute_list    = NULL;
    XVisualInfo*        visual_info_ptr          = NULL;
    Display*            window_display           = NULL;
    system_pixel_format window_pixel_format      = NULL;
    int                 window_screen_index      = -1;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DISPLAY,
                              &window_display);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &window_pixel_format);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_SCREEN_INDEX,
                              &window_screen_index);

    system_pixel_format_get_property(window_pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_ALPHA_BITS,
                                     n_rgba_bits + 3);
    system_pixel_format_get_property(window_pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_BLUE_BITS,
                                     n_rgba_bits + 2);
    system_pixel_format_get_property(window_pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                     n_depth_bits);
    system_pixel_format_get_property(window_pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_GREEN_BITS,
                                     n_rgba_bits + 1);
    system_pixel_format_get_property(window_pixel_format,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_COLOR_BUFFER_RED_BITS,
                                     n_rgba_bits + 0);

    fb_config_attribute_list = _ogl_context_linux_get_fb_config_attribute_list(window_pixel_format);
    visual_attribute_list    = _ogl_context_linux_get_visual_attribute_list   (window_pixel_format);

    ogl_context_linux_get_fb_configs_for_gl_window(window,
                                                  &fb_configs_ptr,
                                                  &n_compatible_fb_configs);

    if (fb_configs_ptr          == NULL ||
        n_compatible_fb_configs == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No compatible framebuffer configurations found.");

        goto end;
    }

    visual_info_ptr = glXChooseVisual(window_display,
                                      window_screen_index,
                                      fb_config_attribute_list,
                                      visual_attribute_list);

    ASSERT_DEBUG_SYNC(visual_info_ptr != NULL,
                      "glXChooseVisual() returned NULL");

end:
    if (fb_config_attribute_list != NULL)
    {
        delete [] fb_config_attribute_list;

        fb_config_attribute_list = NULL;
    }

    if (fb_configs_ptr != NULL)
    {
        XFree(fb_configs_ptr);

        fb_configs_ptr = NULL;
    }

    if (visual_attribute_list != NULL)
    {
        delete [] visual_attribute_list;

        visual_attribute_list = NULL;
    }

    return visual_info_ptr;
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_init(__in ogl_context                     context,
                                   __in PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation)
{
    int                   attribute_list[32]               = {0}; /* 32 is more than enough */
    int                   context_major_version            = 0;
    int                   context_minor_version            = 0;
    ogl_context_type      context_type                     = OGL_CONTEXT_TYPE_UNDEFINED;
    GLXFBConfig*          fb_configs_ptr                   = NULL;
    int                   n_fb_configs                     = 0;
    _ogl_context_linux*   new_linux_ptr                    = new (std::nothrow) _ogl_context_linux;
    ogl_context           parent_context                   = NULL;
    GLXContext            parent_context_rendering_context = NULL;
    system_window         parent_context_window            = NULL;
    ogl_rendering_handler parent_context_rendering_handler = NULL;
    system_window         window                           = (system_window)        NULL;
    Display*              window_display                   = NULL;
    system_window_handle  window_handle                    = (system_window_handle) NULL;

    ASSERT_DEBUG_SYNC(_display != NULL,
                      "Display is NULL");
    ASSERT_DEBUG_SYNC(new_linux_ptr != NULL,
                      "Out of memory");

    if (new_linux_ptr == NULL)
    {
        goto end;
    }

    ogl_context_set_property(context,
                             OGL_CONTEXT_PROPERTY_PLATFORM_CONTEXT,
                            &new_linux_ptr);

    /* Immediately tell the owner where the platform context instance is */
    new_linux_ptr->context = context;

    /* If context we are to share objects with is not NULL, lock corresponding renderer thread before continuing.  */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_PARENT_CONTEXT,
                            &parent_context);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &window);

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

    /* Retrieve supported FB configs */
    ogl_context_linux_get_fb_configs_for_gl_window(window,
                                                  &fb_configs_ptr,
                                                  &n_fb_configs);

    /* Create the context instance */
    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DISPLAY,
                              &window_display);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_MAJOR_VERSION,
                            &context_major_version);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_MINOR_VERSION,
                            &context_minor_version);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    attribute_list[0] = GLX_CONTEXT_MAJOR_VERSION_ARB;
    attribute_list[1] = context_major_version;

    attribute_list[2] = GLX_CONTEXT_MINOR_VERSION_ARB;
    attribute_list[3] = context_minor_version;

    attribute_list[4] = GLX_CONTEXT_FLAGS_ARB;

    #ifdef _DEBUG
    {
        attribute_list[5] = GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | GLX_CONTEXT_DEBUG_BIT_ARB;
    }
    #else
    {
        attribute_list[5] = GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    }
    #endif

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        attribute_list[6] = GLX_CONTEXT_PROFILE_MASK_ARB;
        attribute_list[7] = GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
        attribute_list[8] = 0;
    }
    else
    {
        attribute_list[6] = 0;
    }

    new_linux_ptr->rendering_context = glXCreateContextAttribsARB(window_display,
                                                                  fb_configs_ptr[0],
                                                                  parent_context_rendering_context,
                                                                  true, /* direct */
                                                                  attribute_list);

    ASSERT_DEBUG_SYNC(new_linux_ptr->rendering_context != NULL,
                      "Could not create a rendering context");

end:
    /* Clean up */
    if (fb_configs_ptr != NULL)
    {
        XFree(fb_configs_ptr);

        fb_configs_ptr = NULL;
    }

    /* Sharing? Unlock corresponding renderer thread */
    if (parent_context != NULL)
    {
        ogl_rendering_handler_unlock_bound_context(parent_context_rendering_handler);
    }

    /* Initialize WGL extensions */
    _ogl_context_linux_initialize_glx_extensions(new_linux_ptr);

    /* Call the provided function pointer to continue the context initialization process. */
    pInitContextAfterCreation(new_linux_ptr->context);

    return;

end_error:
    if (new_linux_ptr != NULL)
    {
        ogl_context_linux_deinit( (ogl_context_linux) new_linux_ptr);

        new_linux_ptr = NULL;
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
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

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
