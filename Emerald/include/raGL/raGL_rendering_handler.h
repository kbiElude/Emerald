#ifndef RAGL_RENDERING_HANDLER_H
#define RAGL_RENDERING_HANDLER_H

#include "raGL/raGL_types.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_types.h"

typedef enum
{
    /* ogl_context; settable. */
    RAGL_RENDERING_HANDLER_PROPERTY_CALL_PASSTHROUGH_CONTEXT,

    /* bool; settable. */
    RAGL_RENDERING_HANDLER_PROPERTY_CALL_PASSTHROUGH_MODE,

} raGL_rendering_handler_property;

/** TODO */
PUBLIC void* raGL_rendering_handler_create(ral_rendering_handler rendering_handler_ral);

/** TODO */
PUBLIC void raGL_rendering_handler_enumerate_custom_wait_event_handlers(void*                                                   rendering_handler_backend,
                                                                        uint32_t*                                               opt_out_n_custom_wait_event_handlers_ptr,
                                                                        const ral_rendering_handler_custom_wait_event_handler** opt_out_custom_wait_event_handlers_ptr);

/** TODO. Should only be used by ral_rendering_handler */
PUBLIC void raGL_rendering_handler_execute_present_job(void*           rendering_handler_raGL,
                                                       ral_present_job present_job);

/** TODO */
PUBLIC void raGL_rendering_handler_init_from_rendering_thread(ral_context           context_ral,
                                                              ral_rendering_handler rendering_handler_ral,
                                                              void*                 rendering_handler_raGL);

/** TODO */
PUBLIC void raGL_rendering_handler_lock_bound_context(raGL_rendering_handler rendering_handler);

/** TODO */
PUBLIC void raGL_rendering_handler_post_draw_frame(void* rendering_handler_raBackend,
                                                   bool  has_rendered_frame);

/** TODO */
PUBLIC void raGL_rendering_handler_pre_draw_frame(void* rendering_handler_raBackend);

/** TODO */
PUBLIC void raGL_rendering_handler_present_frame(void*                   rendering_handler_raBackend,
                                                 system_critical_section rendering_cs);

/** TODO */
PUBLIC void raGL_rendering_handler_release(void* rendering_handler);

/* TODO. Should only be used by ral_rendering_handler */
PUBLIC bool raGL_rendering_handler_request_callback_for_ral_rendering_handler(void*                                   rendering_handler_backend,
                                                                              PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                                              void*                                   user_arg,
                                                                              bool                                    present_after_executed,
                                                                              ral_rendering_handler_execution_mode    execution_mode);

/** TODO */
PUBLIC void raGL_rendering_handler_set_property(raGL_rendering_handler           rendering_handler,
                                                raGL_rendering_handler_property  property,
                                                const void*                      value);

/** TODO */
PUBLIC void raGL_rendering_handler_unlock_bound_context(raGL_rendering_handler rendering_handler);

#endif /* RAGL_RENDERING_HANDLER_H */