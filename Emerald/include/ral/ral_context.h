#ifndef RAL_CONTEXT_H
#define RAL_CONTEXT_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h" /* TODO: Remove OGL dep */
#include "ral/ral_types.h"


REFCOUNT_INSERT_DECLARATIONS(ral_context,
                             ral_context);

/* ral_context call-backs. These should be used by back-ends to notify about RAL object modifications. */
enum
{
    /* One or more ral_buffer instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_buffers_created_callback_arg
     */
    RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,

    /* One or more existing ral_buffer instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_buffers_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,


    /* One or more ral_framebuffer instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_framebuffers_created_callback_arg
     */
    RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,

    /* One or more existing ral_framebuffer instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_framebuffers_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,


    /* One or more ral_sampler instances were created.
     *
     * NOTE: This callbacK ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_samplers_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,

    /* One or more existing ral_sampler instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_samplers_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,


    /* One or more ral_texture instances were created.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_textures_created_callback_arg
     **/
    RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,

    /* One or more existing ral_texture instances were deleted.
     *
     * NOTE: This callback ID only supports synchronous call-backs.
     *
     * arg: ral_context_callback_textures_deleted_callback_arg instance.
     */
    RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,


    /* Always last */
    RAL_CONTEXT_CALLBACK_ID_COUNT
};

typedef struct
{
    ral_buffer* created_buffers;
    uint32_t    n_buffers;

} ral_context_callback_buffers_created_callback_arg;

typedef struct
{
    ral_buffer* deleted_buffers;
    uint32_t    n_buffers;

} ral_context_callback_buffers_deleted_callback_arg;

typedef struct
{
    ral_framebuffer* created_framebuffers;
    uint32_t         n_framebuffers;

} ral_context_callback_framebuffers_created_callback_arg;

typedef struct
{
    ral_framebuffer* deleted_framebuffers;
    uint32_t         n_framebuffers;

} ral_context_callback_framebuffers_deleted_callback_arg;

typedef struct
{
    ral_sampler* created_samplers;
    uint32_t     n_samplers;

} ral_context_callback_samplers_created_callback_arg;

typedef struct
{
    ral_sampler* deleted_samplers;
    uint32_t     n_samplers;

} ral_context_callback_samplers_deleted_callback_arg;

typedef struct
{
    ral_texture* created_textures;
    uint32_t     n_textures;

} ral_context_callback_textures_created_callback_arg;

typedef struct
{
    ral_texture* deleted_textures;
    uint32_t     n_textures;

} ral_context_callback_textures_deleted_callback_arg;

/** TODO */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      system_window             window,
                                      ogl_rendering_handler     rendering_handler);

/** TODO */
PUBLIC bool ral_context_create_buffers(ral_context                   context,
                                       uint32_t                      n_buffers,
                                       const ral_buffer_create_info* buffer_create_info_ptr,
                                       ral_buffer*                   out_result_buffers_ptr);

/** TODO */
PUBLIC bool ral_context_create_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* out_result_framebuffers_ptr);

/** TODO */
PUBLIC bool ral_context_create_samplers(ral_context                    context,
                                        uint32_t                       n_samplers,
                                        const ral_sampler_create_info* sampler_create_info_ptr,
                                        ral_sampler*                   out_result_samplers_ptr);

/** TODO */
PUBLIC bool ral_context_create_textures(ral_context                    context,
                                        uint32_t                       n_textures,
                                        const ral_texture_create_info* texture_create_info_ptr,
                                        ral_texture*                   out_result_textures_ptr);

/** TODO */
PUBLIC bool ral_context_create_textures_from_file_names(ral_context                      context,
                                                        uint32_t                         n_file_names,
                                                        const system_hashed_ansi_string* file_names_ptr,
                                                        ral_texture*                     out_result_textures_ptr);

PUBLIC bool ral_context_create_textures_from_gfx_images(ral_context      context,
                                                        uint32_t         n_images,
                                                        const gfx_image* images,
                                                        ral_texture*     out_result_textures_ptr);
/** TODO */
PUBLIC bool ral_context_delete_buffers(ral_context context,
                                       uint32_t    n_buffers,
                                       ral_buffer* buffers);

/** TODO */
PUBLIC bool ral_context_delete_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* framebuffers);

/** TODO */
PUBLIC bool ral_context_delete_samplers(ral_context  context,
                                        uint32_t     n_samplers,
                                        ral_sampler* samplers);

/** TODO */
PUBLIC bool ral_context_delete_textures(ral_context  context,
                                        uint32_t     n_textures,
                                        ral_texture* textures);

/** TODO */
PUBLIC void ral_context_get_property(ral_context          context,
                                     ral_context_property property,
                                     void*                out_result_ptr);

/** TODO */
PUBLIC ral_sampler ral_context_get_sampler_by_create_info(ral_context              context,
                                                          ral_sampler_create_info* create_info_ptr);

/** TODO */
PUBLIC ral_texture ral_context_get_texture_by_file_name(ral_context               context,
                                                        system_hashed_ansi_string file_name);

/** TODO
 *
 *  TEMPORARY. Will be removed during RAL integration */
PUBLIC ogl_context ral_context_get_gl_context(ral_context context);

/** TODO
 *
 *  TEMPORARY. Will be removed during RAL integration */
PUBLIC GLuint ral_context_get_sampler_gl_id(ral_context context,
                                            ral_sampler sampler);

/** TODO
 *
 *  TEMPORARY. Will be removed during RAL integration */
PUBLIC raGL_texture ral_context_get_texture_gl(ral_context context,
                                               ral_texture texture);

/** TODO
 *
 *  TEMPORARY. Will be removed during RAL integration */
PUBLIC GLuint ral_context_get_texture_gl_id(ral_context context,
                                            ral_texture texture);

/** TODO */
PUBLIC ral_texture ral_context_get_texture_by_name(ral_context               context,
                                                   system_hashed_ansi_string name);

#endif /* RAL_CONTEXT_H */