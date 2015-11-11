#ifndef RAL_CONTEXT_H
#define RAL_CONTEXT_H

#include "ogl/ogl_types.h" /* TODO: Remove OGL dep */
#include "ral/ral_types.h"


REFCOUNT_INSERT_DECLARATIONS(ral_context,
                             ral_context);

/* ral_context call-backs. These should be used by back-ends to notify about RAL object modifications. */
enum
{
    /* One or more ral_framebuffer instance were created.
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

    /* Always last */
    RAL_CONTEXT_CALLBACK_ID_COUNT
};

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


/** TODO */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      system_window             window,
                                      ogl_rendering_handler     rendering_handler);

/** TODO */
PUBLIC bool ral_context_create_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* out_result_framebuffers_ptr);

/** TODO */
PUBLIC void ral_context_delete_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* framebuffers);

/** TODO */
PUBLIC void ral_context_get_property(ral_context          context,
                                     ral_context_property property,
                                     void*                out_result_ptr);


#endif /* RAL_CONTEXT_H */