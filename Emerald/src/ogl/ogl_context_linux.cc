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

    /* GLX extensions */
    bool glx_arb_create_context_support;
    bool glx_ext_swap_control_support;
    bool glx_ext_swap_control_tear_support;

    PFNGLXCREATECONTEXTATTRIBSARBPROC pGLXCreateContextAttribsARB; /* GLX_ARB_create_context */
    PFNGLXSWAPINTERVALEXTPROC         pGLXSwapIntervalEXT;         /* GL_EXT_swap_control */

    _ogl_context_linux()
    {
        context                           = NULL;
        glx_arb_create_context_support    = false;
        glx_ext_swap_control_support      = false;
        glx_ext_swap_control_tear_support = false;
        pGLXCreateContextAttribsARB       = NULL;
        pGLXSwapIntervalEXT               = NULL;
        rendering_context                 = NULL;
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
PRIVATE void _ogl_context_linux_initialize_glx_extensions(_ogl_context_linux* context_ptr)
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

    ASSERT_DEBUG_SYNC(context_window_display != NULL,
                      "No display associated with a system_window handle");

    /* Retrieve the GLX extensions */
    glx_extensions = glXQueryExtensionsString(context_window_display,
                                              context_window_screen_index);

    /* Is GLX_ARB_create_context supported? */
    context_ptr->glx_arb_create_context_support = (strstr(glx_extensions,
                                                          "GLX_ARB_create_context") != NULL);

    if (context_ptr->glx_arb_create_context_support)
    {
        context_ptr->pGLXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC) glXGetProcAddress( (const unsigned char*) "glXCreateContextAttribsARB");

        ASSERT_DEBUG_SYNC(context_ptr->pGLXCreateContextAttribsARB != NULL,
                          "glXCreateContextAttribsARB() entry-point is NULL");
    }

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
    /* TODO: Rip out! */
    return false;
}


/** Please see header for spec */
PUBLIC void ogl_context_linux_bind_to_current_thread(ogl_context_linux context_linux)
{
    system_window        context_window          = NULL;
    system_window_handle context_window_platform = (system_window_handle) NULL;
    Display*             display                 = NULL;
    _ogl_context_linux*  linux_ptr               = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "NULL ogl_context_linux instance passed to ogl_context_linux_bind_to_current_thread()");

    if (linux_ptr != NULL)
    {
        ogl_context_get_property(linux_ptr->context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &context_window);

        ASSERT_DEBUG_SYNC(context_window != NULL,
                         "No window associated with the rendering context");

        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_DISPLAY,
                                  &display);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_HANDLE,
                                  &context_window_platform);

        XLockDisplay(display);
        {
            glXMakeCurrent(display,
                           context_window_platform,
                           linux_ptr->rendering_context);
        }
        XUnlockDisplay(display);
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_deinit(ogl_context_linux context_linux)
{
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");

    if (linux_ptr->rendering_context != NULL)
    {
        system_window context_window         = NULL;
        Display*      context_window_display = NULL;

        ogl_context_get_property(linux_ptr->context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &context_window);

        ASSERT_DEBUG_SYNC(context_window != NULL,
                          "No window associated with the rendering context");

        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_DISPLAY,
                                  &context_window_display);

        glXDestroyContext(context_window_display,
                          linux_ptr->rendering_context);

        linux_ptr->rendering_context = NULL;
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
PUBLIC void* ogl_context_linux_get_func_ptr(ogl_context_linux context_linux,
                                            const char*       name)
{
    void*               result    = NULL;
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(name      != NULL &&
                      linux_ptr != NULL,
                      "Input argument is NULL");

    result = (void*) glXGetProcAddress( (const unsigned char*) name);

    return result;
}

/** Please see header for spec */
PUBLIC bool ogl_context_linux_get_property(ogl_context_linux    context_linux,
                                           ogl_context_property property,
                                           void*                out_result)
{
    bool                result    = false;
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    switch (property)
    {
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
                                    &n_depth_bits);
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
PUBLIC void ogl_context_linux_init(ogl_context                     context,
                                   PFNINITCONTEXTAFTERCREATIONPROC pInitContextAfterCreation)
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

    ASSERT_DEBUG_SYNC(context != NULL,
                      "Input context is NULL");
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

    /* Initialize WGL extensions */
    _ogl_context_linux_initialize_glx_extensions(new_linux_ptr);

    new_linux_ptr->rendering_context = new_linux_ptr->pGLXCreateContextAttribsARB(window_display,
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
PUBLIC bool ogl_context_linux_set_property(ogl_context_linux    context_linux,
                                           ogl_context_property property,
                                           const void*          data)
{
    bool                result    = false;
    _ogl_context_linux* linux_ptr = (_ogl_context_linux*) context_linux;

    ASSERT_DEBUG_SYNC(linux_ptr != NULL,
                      "Input argument is NULL");

    switch (property)
    {
        case OGL_CONTEXT_PROPERTY_VSYNC_ENABLED:
        {
            system_window        context_window          = NULL;
            Display*             context_window_display  = NULL;
            system_window_handle context_window_platform = (system_window_handle) NULL;
            bool                 vsync_enabled           = *(bool*) data;

            ASSERT_ALWAYS_SYNC(linux_ptr->glx_ext_swap_control_support,
                               "GLX_swap_control extension not supported. Update your drivers.");

            ogl_context_get_property(linux_ptr->context,
                                     OGL_CONTEXT_PROPERTY_WINDOW,
                                    &context_window);

            ASSERT_DEBUG_SYNC(context_window != NULL,
                              "No renderer window associated with OpenGL context");

            system_window_get_property(context_window,
                                       SYSTEM_WINDOW_PROPERTY_DISPLAY,
                                      &context_window_display);
            system_window_get_property(context_window,
                                       SYSTEM_WINDOW_PROPERTY_HANDLE,
                                      &context_window_platform);

            if (linux_ptr->pGLXSwapIntervalEXT != NULL)
            {
                int swap_interval = (linux_ptr->glx_ext_swap_control_tear_support) ? -1 : /* use adaptive vsync  */
                                                                                      1;  /* force regular vsync */

                linux_ptr->pGLXSwapIntervalEXT(context_window_display,
                                               context_window_platform,
                                               vsync_enabled ? swap_interval
                                                             : 0);

                result = true;
            }

            break;
        } /* case OGL_CONTEXT_PROPERTY_VSYNC_ENABLED: */

    } /* switch (property) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_swap_buffers(ogl_context_linux context_linux)
{
    _ogl_context_linux*  linux_ptr       = (_ogl_context_linux*) context_linux;
    system_window        window          = NULL;
    Display*             window_display  = NULL;
    system_window_handle window_platform = (system_window_handle) NULL;

    ogl_context_get_property(linux_ptr->context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &window);

    ASSERT_DEBUG_SYNC(window != NULL,
                      "No window associated with the rendering context");

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DISPLAY,
                              &window_display);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_platform);

    ASSERT_DEBUG_SYNC(window_display != NULL,
                      "No display associated with the system_window instance.");

    glXSwapBuffers(window_display,
                   window_platform);
}

/** Please see header for spec */
PUBLIC void ogl_context_linux_unbind_from_current_thread(ogl_context_linux context_linux)
{
    system_window        context_window = NULL;
    Display*             display        = NULL;
    _ogl_context_linux*  linux_ptr      = (_ogl_context_linux*) context_linux;

    ogl_context_get_property(linux_ptr->context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &context_window);

    ASSERT_DEBUG_SYNC(context_window != NULL,
                      "No window associated with the rendering context");

    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_DISPLAY,
                              &display);

    XLockDisplay(display);
    {
        glXMakeCurrent(              display,
                       (GLXDrawable) NULL,  /* drawable */
                                     NULL); /* ctx      */
    }
    XUnlockDisplay(display);
}
