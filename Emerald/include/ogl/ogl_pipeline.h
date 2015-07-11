/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_PIPELINE_H
#define OGL_PIPELINE_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"

typedef void (*PFNOGLPIPELINECALLBACKPROC)(ogl_context,
                                           system_time,
                                           void*);

REFCOUNT_INSERT_DECLARATIONS(ogl_pipeline,
                             ogl_pipeline)

/** TODO */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage(__in __notnull ogl_pipeline instance);

/** TODO */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage_step(__in __notnull ogl_pipeline               instance,
                                                        __in           uint32_t                   n_stage,
                                                        __in __notnull system_hashed_ansi_string  step_name,
                                                        __in __notnull PFNOGLPIPELINECALLBACKPROC pfn_step_callback,
                                                        __in __notnull void*                      step_callback_user_arg);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API ogl_pipeline ogl_pipeline_create(__in __notnull ogl_context               context,
                                                                           __in           bool                      should_overlay_performance_info,
                                                                           __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API bool ogl_pipeline_draw_stage(__in __notnull ogl_pipeline instance,
                                                                       __in           uint32_t     n_stage,
                                                                       __in           system_time  time);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_pipeline_get_context(__in __notnull ogl_pipeline instance);

/** INTERNAL USE ONLY. TODO. */
PUBLIC ogl_text ogl_pipeline_get_text_renderer(__in __notnull ogl_pipeline instance);

/** TODO */
PUBLIC EMERALD_API ogl_ui ogl_pipeline_get_ui(__in __notnull ogl_pipeline instance);

#endif /* OGL_PIPELINE_H */
