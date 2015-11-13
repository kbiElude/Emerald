#ifndef RAL_CONTEXT_H
#define RAL_CONTEXT_H

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
PUBLIC void ral_context_get_property(ral_context          context,
                                     ral_context_property property,
                                     void*                out_result_ptr);


#endif /* RAL_CONTEXT_H */