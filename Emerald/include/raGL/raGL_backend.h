#ifndef RAGL_BACKEND_H
#define RAGL_BACKEND_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


typedef enum
{
    /* not settable; raGL_dep_tracker */
    RAGL_BACKEND_PRIVATE_PROPERTY_DEP_TRACKER,

    /* not settable; raGL_framebuffers */
    RAGL_BACKEND_PRIVATE_PROPERTY_FBOS,

    /* settable; ogl_context. */
    RAGL_BACKEND_PRIVATE_PROPERTY_RENDERING_CONTEXT,

    /* not settable; system_critical_section.
     *
     * CS to serialize GL command execution across multiple contexts. An example class of commands
     * which need to be wrapped inside this CS are glGen*() calls. */
    RAGL_BACKEND_PRIVATE_PROPERTY_RENDERING_CS,

    /* not settable; ral_texture_pool */
    RAGL_BACKEND_PRIVATE_PROPERTY_TEXTURE_POOL,

    /* not settable; raGL_vaos */
    RAGL_BACKEND_PRIVATE_PROPERTY_VAOS,

} raGL_backend_private_property;


/** TODO */
PUBLIC raGL_backend raGL_backend_create(ral_context               context,
                                        system_hashed_ansi_string name,
                                        ral_backend_type          type);

/** Appends the specified sync for execution for all rendering contexts active at the time
 *  of the call, apart from the context, for which the sync object has been created for.
 *
 *  A GPU-side wait will be issued for each rendering context either at raGL_backend_sync()
 *  call time, or at the beginning of next frame.
 *
 *  The back-end takes ownership of the specified sync. The object will be released as soon as all
 *  running rendering contexts have waited on the sync.
 *
 *  If sync object has been scheduled from a backend, from which a sync object is still outstanding for
 *  some of the rendering contexts, it will be released and the newer sync object will be used instead.
 *
 **/
PUBLIC void raGL_backend_enqueue_sync();

/** TODO */
PUBLIC bool raGL_backend_get_buffer(raGL_backend backend,
                                    ral_buffer   buffer_ral,
                                    raGL_buffer* out_buffer_raGL_ptr);

/** TODO */
PUBLIC void raGL_backend_get_buffer_property_by_id(raGL_backend                 backend,
                                                   GLuint                       bo_id,
                                                   raGL_buffers_buffer_property property,
                                                   void*                        out_result_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_command_buffer(raGL_backend         backend,
                                            ral_command_buffer   command_buffer_ral,
                                            raGL_command_buffer* out_command_buffer_raGL_ptr);

/** TODO */
PUBLIC ral_context raGL_backend_get_helper_context(ral_backend_type type);

/** TODO */
PUBLIC void raGL_backend_get_private_property(raGL_backend                  backend,
                                              raGL_backend_private_property property,
                                              void*                         out_result_ptr);

/** TODO */
PUBLIC void raGL_backend_get_property(void*                backend,
                                      ral_context_property property,
                                      void*                out_result_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_program(raGL_backend  backend,
                                     ral_program   program_ral,
                                     raGL_program* out_program_raGL_ptr);

/* TODO */
PUBLIC bool raGL_backend_get_program_by_id(raGL_backend  backend,
                                           GLuint        program_id,
                                           raGL_program* out_program_raGL_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_sampler(raGL_backend  backend,
                                     ral_sampler   sampler_ral,
                                     raGL_sampler* out_sampler_raGL_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_shader(raGL_backend backend,
                                    ral_shader   shader_ral,
                                    raGL_shader* out_shader_raGL_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_texture(raGL_backend  backend,
                                     ral_texture   texture_ral,
                                     raGL_texture* out_texture_raGL_ptr);

/** TODO
 *
 *  NOTE: Don't forget a raGL_texture can either hold a GL renderbuffer OR a GL texture!
 *        This func should be used for GL textures ONLY (namespaces are disjoint for the
 *        two in GL)
 */
PUBLIC bool raGL_backend_get_texture_by_id(raGL_backend  backend,
                                           GLuint        texture_id,
                                           raGL_texture* out_texture_raGL_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_texture_view(raGL_backend     backend,
                                          ral_texture_view texture_view_ral,
                                          raGL_texture*    out_texture_raGL_ptr);

/** TODO */ 
PUBLIC void raGL_backend_init(raGL_backend backend);

/** TODO */
PUBLIC void raGL_backend_release(void* backend); /* raGL_backend instance */

/** TODO */
PUBLIC void raGL_backend_set_private_property(raGL_backend                  backend,
                                              raGL_backend_private_property property,
                                              const void*                   data_ptr);

/** TODO */
PUBLIC void raGL_backend_sync();

#endif /* RAGL_BACKEND_H */