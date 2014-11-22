/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef OGL_CONTEXT_H
#define OGL_CONTEXT_H

#include "ogl/ogl_types.h"
#include "ogl/ogl_context_bo_bindings.h"
#include "ogl/ogl_context_sampler_bindings.h"
#include "ogl/ogl_context_texture_compression.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_context, ogl_context)

/** Optimus: forces High Performance profile */
#define INCLUDE_OPTIMUS_SUPPORT                                          \
    extern "C"                                                           \
    {                                                                    \
     _declspec(dllexport) extern DWORD NvOptimusEnablement = 0x00000001; \
    }

typedef enum ogl_context_property
{
    OGL_CONTEXT_PROPERTY_BO_BINDINGS,                                     /* not settable, ogl_context_bo_bindings */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,                                  /* not settable, ogl_context_es_entrypoints*.
                                                                           * Only accessible for ES contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES_EXT_TEXTURE_BUFFER,               /* not settable, ogl-context_es_entrypoints_ext_texture_buffer*.
                                                                           * Only accessible for ES contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,                                  /* not settable, ogl_context_gl_entrypoints*.
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE,               /* not settable, ogl_context_entrypoints_arb_buffer_storage*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_COMPUTE_SHADER,               /* not settable, ogl_context_entrypoints_arb_compute_shader*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_FRAMEBUFFER_NO_ATTACHMENTS,   /* not settable, ogl_context_entrypoints_arb_framebuffer_no_attachments*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_PROGRAM_INTERFACE_QUERY,      /* not settable, ogl_context_entrypoints_arb_program_interface_query*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SHADER_STORAGE_BUFFER_OBJECT, /* not settable, ogl_context_entrypoints_arb_shader_storage_buffer_object*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_TEXTURE_STORAGE_MULTISAMPLE,  /* not settable, ogl_context_entrypoints_arb_texture_storage_multisample*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,          /* not settable, ogl_context_entrypoints_ext_direct_state_access*
                                                                           * Only accessible for GL contexts */
    OGL_CONTEXT_PROPERTY_INFO,                                            /* not settable, ogl_context_info* */
    OGL_CONTEXT_PROPERTY_LIMITS,                                          /* not settable, ogl_context_limits* */
    OGL_CONTEXT_PROPERTY_LIMITS_ARB_COMPUTE_SHADER,                       /* not settable, ogl_context_limits_arb_compute_shader* */
    OGL_CONTEXT_PROPERTY_LIMITS_ARB_FRAMEBUFFER_NO_ATTACHMENTS,           /* not settable, ogl_context_limits_arb_framebuffer_no_attachments* */
    OGL_CONTEXT_PROPERTY_LIMITS_ARB_SHADER_STORAGE_BUFFER_OBJECT,         /* not settable, ogl_context_limits_arb_shader_storage_buffer_object* */
    OGL_CONTEXT_PROPERTY_PRIMITIVE_RENDERER,                              /* not settable, ogl_primitive_renderer */
    OGL_CONTEXT_PROPERTY_MATERIALS,                                       /* not settable, ogl_materials */
    OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,                                /* not settable, ogl_context_sampler_bindings */
    OGL_CONTEXT_PROPERTY_SAMPLERS,                                        /* not settable, ogl_samplers */
    OGL_CONTEXT_PROPERTY_STATE_CACHE,                                     /* not settable, ogl_context_state_cache */
    OGL_CONTEXT_PROPERTY_SUPPORT_ES_EXT_TEXTURE_BUFFER,                   /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE,                   /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_FRAMEBUFFER_NO_ATTACHMENTS,       /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_PROGRAM_INTERFACE_QUERY,          /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SHADER_STORAGE_BUFFER_OBJECT,     /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_TEXTURE_STORAGE_MULTISAMPLE,      /* not settable, bool */
    OGL_CONTEXT_PROPERTY_SUPPORT_GL_EXT_DIRECT_STATE_ACCESS,              /* not settable, bool */
    OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION,                             /* not settable, ogl_context_texture_compression */
    OGL_CONTEXT_PROPERTY_TEXTURES,                                        /* not settable, ogl_textures */
    OGL_CONTEXT_PROPERTY_TO_BINDINGS,                                     /* not settable, ogl_context_to_bindings */
    OGL_CONTEXT_PROPERTY_TYPE,                                            /* not settable, ogl_context_type */
    OGL_CONTEXT_PROPERTY_WINDOW,                                          /* not settable, system_window */

#ifdef _WIN32
    OGL_CONTEXT_PROPERTY_DC,                                           /* not settable, HDC */
    OGL_CONTEXT_PROPERTY_GL_CONTEXT,                                   /* not settable, HGLRC */
#endif

} ogl_context_property;

/** TODO */
PUBLIC void ogl_context_bind_to_current_thread(__in __notnull ogl_context);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_context_create_from_system_window(__in __notnull   system_hashed_ansi_string   name,
                                                                     __in __notnull   ogl_context_type            type,
                                                                     __in __notnull   system_window               window,
                                                                     __in __notnull   ogl_pixel_format_descriptor in_pfd,
                                                                     __in             bool                        vsync_enabled,
                                                                     __in __maybenull ogl_context                 share_context,
                                                                     __in             bool                        allow_multisampling
                                                                     );

/** TODO */
PUBLIC ogl_context ogl_context_get_current_context();

/** TODO */
PUBLIC EMERALD_API void ogl_context_get_property(__in  __notnull ogl_context          context,
                                                 __in            ogl_context_property property,
                                                 __out __notnull void*                out_result);

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
/** TODO */
PUBLIC EMERALD_API void ogl_context_retrieve_multisampling_info(__in  __notnull ogl_context,
                                                                __out __notnull uint32_t*,
                                                                __out __notnull const uint32_t**);

/** TODO */
PUBLIC EMERALD_API bool ogl_context_set_multisampling(__in __notnull ogl_context, 
                                                      __in           uint32_t);

/** TODO. This function is not exported because it requires ogl_context to be active right before the call. */
PUBLIC bool ogl_context_set_vsync(__in __notnull ogl_context,
                                  __in           bool);

/** TODO */
PUBLIC void ogl_context_unbind_from_current_thread();

#endif /* OGL_CONTEXT_H */
