/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_CONTEXT_H
#define OGL_CONTEXT_H

#include "ogl/ogl_types.h"
#include "ogl/ogl_context_bo_bindings.h"
#include "ogl/ogl_context_sampler_bindings.h"
#include "ogl/ogl_context_texture_compression.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_types.h"


REFCOUNT_INSERT_DECLARATIONS(ogl_context,
                             ogl_context)

/** Optimus: forces High Performance profile */
#ifdef _WIN32
    #define INCLUDE_OPTIMUS_SUPPORT                                          \
        extern "C"                                                           \
        {                                                                    \
        _declspec(dllexport) extern DWORD NvOptimusEnablement = 0x00000001; \
        }
#else
    #define INCLUDE_OPTIMUS_SUPPORT
#endif


typedef enum ogl_context_property
{
    /* not settable, raGL_backend */
    OGL_CONTEXT_PROPERTY_BACKEND,

    /* not settable, ral_backend_type */
    OGL_CONTEXT_PROPERTY_BACKEND_TYPE,

    /* not settable, ogl_context_bo_bindings */
    OGL_CONTEXT_PROPERTY_BO_BINDINGS,

    /* not settable, ral_context */
    OGL_CONTEXT_PROPERTY_CONTEXT_RAL,

    /* not settable, ral_framebuffer */
    OGL_CONTEXT_PROPERTY_DEFAULT_FBO,

    /* not settable, ral_texture_format */
    OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,

    /* not settable, uint32_t[2] */
    OGL_CONTEXT_PROPERTY_DEFAULT_FBO_SIZE,

    /* not settable, ogl_context_es_entrypoints*.
     *
     * Only accessible for ES contexts
     */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,

    /* not settable, ogl-context_es_entrypoints_ext_texture_buffer*.
     *
     * Only accessible for ES contexts
     */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES_EXT_TEXTURE_BUFFER,

    /* not settable, ogl_context_gl_entrypoints*.
     * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,

    /* not settable, ogl_context_entrypoints_arb_buffer_storage*
     *
     * Only accessible for GL contexts
     */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE,

    /* not settable, ogl_context_entrypoints_arb_sparse_buffer*
     *
     * Only accessible for GL contexts
     */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SPARSE_BUFFER,

    /* not settable, ogl_context_entrypoints_ext_direct_state_access*
     *
     * Only accessible for GL contexts
     */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,

    /* not settable, ogl_context_info* */
    OGL_CONTEXT_PROPERTY_INFO,

    /* not settable, bool.
     *
     * True if the platform is using AMD driver, false otherwise */
    OGL_CONTEXT_PROPERTY_IS_AMD_DRIVER,

    /* settable, bool. */
    OGL_CONTEXT_PROPERTY_IS_HELPER_CONTEXT,

    /* not settable, bool.
     *
     * True if the platform is using Intel drivers, false otherwise */
    OGL_CONTEXT_PROPERTY_IS_INTEL_DRIVER,

    /* not settable, bool.
     *
     * True if the platform is using NVIDIA drivers, false otheriwse */
    OGL_CONTEXT_PROPERTY_IS_NV_DRIVER,

    /* not settable, ogl_context_limits* */
    OGL_CONTEXT_PROPERTY_LIMITS,

    /* not settable, ogl_context_limits_arb_sparse_buffer* */
    OGL_CONTEXT_PROPERTY_LIMITS_ARB_SPARSE_BUFFER,

    /* not settable, ogl_context_limits_ext_texture_filter_anisotropic* */
    OGL_CONTEXT_PROPERTY_LIMITS_EXT_TEXTURE_FILTER_ANISOTROPIC,

    /* not settable, uint32_t */
    OGL_CONTEXT_PROPERTY_MAJOR_VERSION,

    /* not settable, uint32_t */
    OGL_CONTEXT_PROPERTY_MINOR_VERSION,

    /* not settable, ogl_context */
    OGL_CONTEXT_PROPERTY_PARENT_CONTEXT,

    /* not settable, system_pixel_format */
    OGL_CONTEXT_PROPERTY_PIXEL_FORMAT,

    /* settable, not queriable, ogl_context_linux or ogl_context_win32 */
    OGL_CONTEXT_PROPERTY_PLATFORM_CONTEXT,

    /* not settable, ogl_context_handle */
    OGL_CONTEXT_PROPERTY_RENDERING_CONTEXT,

    /* not settable, ogl_context_sampler_bindings */
    OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,

    /* not settable, ogl_context_state_cache */
    OGL_CONTEXT_PROPERTY_STATE_CACHE,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_ES_EXT_TEXTURE_BUFFER,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_MULTI_BIND,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SPARSE_BUFFERS,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_DIRECT_STATE_ACCESS,

    /* nots ettable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_TEXTURE_FILTER_ANISOTROPIC,

    /* not settable, varia_text_renderer. */
    OGL_CONTEXT_PROPERTY_TEXT_RENDERER,

    /* not settable, ogl_context_texture_compression */
    OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION,

    /* not settable, ogl_context_to_bindings */
    OGL_CONTEXT_PROPERTY_TO_BINDINGS,

    /* not settable, system_window */
    OGL_CONTEXT_PROPERTY_WINDOW,

    /* not settable, GLuint;
     *
     * ID of a VAO, for which no VAAS were configured.
     * Useful for draw calls which do not rely on input variables.
     * DO NOT MODIFY THE VAO'S CONFIGURATION.
     */
    OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,

    /* not settable, ogl_context_vaos */
    OGL_CONTEXT_PROPERTY_VAOS,

    /* settable, bool
     *
     * NOTE: Under Windows, this property can only be changed if the affected window is not
     *                      assigned a rendering context at the time of the setter call.
     * NOTE: Under Linux,   this property can only be changed for VISIBLE (mapped) windows!
     */
    OGL_CONTEXT_PROPERTY_VSYNC_ENABLED,

#ifdef _WIN32
    /* not settable, HDC */
    OGL_CONTEXT_PROPERTY_DC,
#endif

} ogl_context_property;

typedef void (*PFNINITCONTEXTAFTERCREATIONPROC)( ogl_context context);


/** TODO */
PUBLIC void ogl_context_bind_to_current_thread( ogl_context);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_context_create_from_system_window(system_hashed_ansi_string name,
                                                                     ral_context               context,
                                                                     raGL_backend              backend,
                                                                     system_window             window,
                                                                     ogl_context               parent_context);

/** TODO
 *
 *  NOTE: Internal use only.
 */
PUBLIC void ogl_context_deinit_global();

/** Returns information about the supported numbers of samples that can be used for color,
 *  depth and stencil attachments, given user-specified pixel format requirements.
 *
 *  Instead of rendering to the back buffer, Emerald uses a multisample render-buffer. Once
 *  the frame rendering process finishes, it blits the contents of the render-buffer's color
 *  attachment to the back buffer and performs the swap operation.
 *  The approach was mostly dictated by two things:
 *
 *  1) Linux and Windows builds would require significantly different platform-specific code
 *     paths to provide the same information for back buffer-based MSAA.
 *  2) Use of multisample render-targets better fits APIs we will need to support in the future.
 *
 *  NOTE: The returned data is sorted in a descending order.
 *
 *  NOTE: It is caller's responsibility to ensure only one ogl_context_enumerate_msaa_samples() call
 *        is active at the same time. The function uses root context to retrieve
 *        API-specific information and badness may/will happen if this requirement is ignored.
 *
 *  @param backend_type            TODO.
 *  @param pf                      TODO. Ownership is NOT claimed by the function.
 *  @param out_n_supported_samples TODO.
 *  @param out_supported_samples   TODO.
 *
 */
PUBLIC EMERALD_API void ogl_context_enumerate_msaa_samples(ral_backend_type    backend_type,
                                                           system_pixel_format pf,
                                                           unsigned int*       out_n_supported_samples,
                                                           unsigned int**      out_supported_samples);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_context_get_current_context();

/** TODO */
PUBLIC EMERALD_API void ogl_context_get_property(ogl_context          context,
                                                 ogl_context_property property,
                                                 void*                out_result);

/** TODO
 *
 *  NOTE: Internal use only.
 */
PUBLIC void ogl_context_init_global();

/** TODO */
PUBLIC EMERALD_API bool ogl_context_is_extension_supported(ogl_context,
                                                           system_hashed_ansi_string);

/** TODO */
PUBLIC bool ogl_context_release_managers(ogl_context);

/** Blocks by default until user-specified entry-point finishes executing. The entry-point WILL be called
 *  a different thread, that is currently bound to GL context described by @param ogl_context.
 *
 *  @param ogl_context                                GL context to use. Cannot be NULL.
 *  @param PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC Call-back entry point. Cannot be NULL.
 *  @param void*                                      User argument to be passed to call-back entry point.
 *
 *  @return true if successful, false otherwise
 **/
PUBLIC EMERALD_API bool ogl_context_request_callback_from_context_thread(ogl_context                                 context,
                                                                         PFNRAGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc,
                                                                         void*                                       user_arg,
                                                                         bool                                        swap_buffers_afterward = false,
                                                                         raGL_rendering_handler_execution_mode       execution_mode         = RAGL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_BLOCK_TILL_FINISHED);

/** TODO.
 *
 *  This setter will NOT throw an assertion failure if @param property is not recognized.
 */
PUBLIC bool ogl_context_set_property(ogl_context          context,
                                     ogl_context_property property,
                                     const void*          data);

/** TODO */
PUBLIC void ogl_context_swap_buffers(ogl_context context);

/** TODO */
PUBLIC void ogl_context_unbind_from_current_thread(ogl_context context);

#endif /* OGL_CONTEXT_H */
