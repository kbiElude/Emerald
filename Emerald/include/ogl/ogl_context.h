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
    /* not settable, ogl_context_bo_bindings */
    OGL_CONTEXT_PROPERTY_BO_BINDINGS,

    /* not settable, ogl_context_buffers. */
    OGL_CONTEXT_PROPERTY_BUFFERS,

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

    /* not settable, ogl_flyby */
    OGL_CONTEXT_PROPERTY_FLYBY,

    /* not settable, ogl_context_info* */
    OGL_CONTEXT_PROPERTY_INFO,

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

    /* not settable, uint32_t */
    OGL_CONTEXT_PROPERTY_MAJOR_VERSION,

    /* not settable, uint32_t */
    OGL_CONTEXT_PROPERTY_MINOR_VERSION,

    /* not settable, ogl_materials */
    OGL_CONTEXT_PROPERTY_MATERIALS,

    /* settable, not queriable, uint32_t.
     *
     * Adjusts the rendering context's platform-specific MSAA setting.
     */
    OGL_CONTEXT_PROPERTY_MSAA_SAMPLES,

    /* not settable, uint32_t.
     *
     * Tells how many entries can be accessed under OGL_CONTXT_PROPERTY_MSAA_SUPPORTED_SAMPLES.
     */
    OGL_CONTEXT_PROPERTY_MSAA_N_SUPPORTED_SAMPLES,

    /* settable, uint32_t*.
     *
     * An array of supported MSAA samples.
     */
    OGL_CONTEXT_PROPERTY_MSAA_SUPPORTED_SAMPLES,

    /* not settable, ogl_context */
    OGL_CONTEXT_PROPERTY_PARENT_CONTEXT,

    /* not settable, system_pixel_format */
    OGL_CONTEXT_PROPERTY_PIXEL_FORMAT,

    /* settable, not queriable, ogl_context_linux or ogl_context_win32 */
    OGL_CONTEXT_PROPERTY_PLATFORM_CONTEXT,

    /* not settable, ogl_primitive_renderer */
    OGL_CONTEXT_PROPERTY_PRIMITIVE_RENDERER,

    /* not settable, ogl_programs */
    OGL_CONTEXT_PROPERTY_PROGRAMS,

    /* not settable, ogl_context_handle */
    OGL_CONTEXT_PROPERTY_RENDERING_CONTEXT,

    /* not settable, ogl_context_sampler_bindings */
    OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,

    /* not settable, ogl_context_samplers */
    OGL_CONTEXT_PROPERTY_SAMPLERS,

    /* not settable, ogl_shaders */
    OGL_CONTEXT_PROPERTY_SHADERS,

    /* not settable, ogl_shadow_mapping */
    OGL_CONTEXT_PROPERTY_SHADOW_MAPPING,

    /* not settable, ogl_context_state_cache */
    OGL_CONTEXT_PROPERTY_STATE_CACHE,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_ES_EXT_TEXTURE_BUFFER,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SPARSE_BUFFERS,

    /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_DIRECT_STATE_ACCESS,

    /* not settable, ogl_text_renderer. */
    OGL_CONTEXT_PROPERTY_TEXT_RENDERER,

    /* not settable, ogl_context_texture_compression */
    OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION,

    /* not settable, ogl_context_textures */
    OGL_CONTEXT_PROPERTY_TEXTURES,

    /* not settable, ogl_context_to_bindings */
    OGL_CONTEXT_PROPERTY_TO_BINDINGS,

    /* not settable, ogl_context_type */
    OGL_CONTEXT_PROPERTY_TYPE,

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

    /* settable, bool */
    OGL_CONTEXT_PROPERTY_VSYNC_ENABLED,

#ifdef _WIN32
    /* not settable, HDC */
    OGL_CONTEXT_PROPERTY_DC,
#endif

} ogl_context_property;

typedef void (*PFNINITCONTEXTAFTERCREATIONPROC)(__in ogl_context context);


/** TODO */
PUBLIC void ogl_context_bind_to_current_thread(__in ogl_context);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_context_create_from_system_window(__in __notnull   system_hashed_ansi_string name,
                                                                     __in __notnull   ogl_context_type          type,
                                                                     __in __notnull   system_window             window,
                                                                     __in             bool                      vsync_enabled,
                                                                     __in __maybenull ogl_context               parent_context);

/** TODO
 *
 *  NOTE: Internal use only.
 */
PUBLIC void ogl_context_deinit_global();

/** TODO
 *
 *  @param pf                           TODO. Ownership is NOT claimed by the function.
 *  @param out_n_supported_msaa_samples TODO.
 *  @param out_supported_msaa_samples   TODO.
 *
 */
PUBLIC EMERALD_API void ogl_context_enumerate_supported_msaa_samples(__in  system_pixel_format pf,
                                                                     __out unsigned int*       out_n_supported_msaa_samples,
                                                                     __out unsigned int**      out_supported_msaa_samples);

/** TODO */
PUBLIC ogl_context ogl_context_get_current_context();

/** TODO */
PUBLIC EMERALD_API void ogl_context_get_property(__in  __notnull ogl_context          context,
                                                 __in            ogl_context_property property,
                                                 __out __notnull void*                out_result);

/** TODO
 *
 *  NOTE: Internal use only.
 */
PUBLIC void ogl_context_init_global();

/** TODO */
PUBLIC EMERALD_API bool ogl_context_is_extension_supported(__in __notnull ogl_context,
                                                           __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC bool ogl_context_release_managers(__in __notnull ogl_context);

/** Blocks by default until user-specified entry-point finishes executing. The entry-point WILL be called
 *  a different thread, that is currently bound to GL context described by @param ogl_context.
 *
 *  @param ogl_context                                GL context to use. Cannot be NULL.
 *  @param PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC Call-back entry point. Cannot be NULL.
 *  @param void*                                      User argument to be passed to call-back entry point.
 *
 *  @return true if successful, false otherwise
 **/
PUBLIC EMERALD_API bool ogl_context_request_callback_from_context_thread(__in __notnull ogl_context,
                                                                         __in __notnull PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC,
                                                                         __in           void*,
                                                                         __in           bool block_until_available = true);

/** TODO.
 *
 *  This setter will NOT throw an assertion failure if @param property is not recognized.
 */
PUBLIC bool ogl_context_set_property(__in ogl_context          context,
                                     __in ogl_context_property property,
                                     __in const void*          data);

/** TODO */
PUBLIC void ogl_context_swap_buffers(__in ogl_context context);

/** TODO */
PUBLIC void ogl_context_unbind_from_current_thread(__in __notnull ogl_context context);

#endif /* OGL_CONTEXT_H */
