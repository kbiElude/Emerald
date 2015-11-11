/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_PIPELINE_H
#define OGL_PIPELINE_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_pipeline,
                             ogl_pipeline)


/* Used to define a stage with all its steps with a single ogl_pipeline_add_stage_with_steps() call. */
typedef struct
{
    /* Name of the pass. Used when presenting performance statistics */
    system_hashed_ansi_string name;

    /* Function to call back to render stage contents. */
    PFNOGLPIPELINECALLBACKPROC pfn_callback_proc;

    /* User data to pass with the call-back */
    void* user_arg;
} ogl_pipeline_stage_step_declaration;


/** TODO */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage(ogl_pipeline instance);

/** TODO */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage_step(ogl_pipeline                               instance,
                                                        uint32_t                                   n_stage,
                                                        const ogl_pipeline_stage_step_declaration* step_ptr);

/** TODO */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage_with_steps(ogl_pipeline                               pipeline,
                                                              unsigned int                               n_steps,
                                                              const ogl_pipeline_stage_step_declaration* steps);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API ogl_pipeline ogl_pipeline_create(ogl_context               context,
                                                                           bool                      should_overlay_performance_info,
                                                                           system_hashed_ansi_string name);

/** TODO
 *
 *  @param instance          TODO
 *  @param n_stage           TODO
 *  @param time              TODO
 *  @param rendering_area_ss [0]: x1 of the rendering area (in pixels)
 *                           [1]: y1 of the rendering area (in pixels)
 *                           [2]: x2 of the rendering area (in pixels)
 *                           [3]: y2 of the rendering area (in pixels)
 *
 *  @return TODO
 */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API bool ogl_pipeline_draw_stage(ogl_pipeline instance,
                                                                       uint32_t     n_stage,
                                                                       uint32_t     frame_index,
                                                                       system_time  time,
                                                                       const int*   rendering_area_px_topdown);

/** TODO */
PUBLIC EMERALD_API ogl_context ogl_pipeline_get_context(ogl_pipeline instance);

/** INTERNAL USE ONLY. TODO. */
PUBLIC ogl_text ogl_pipeline_get_text_renderer(ogl_pipeline instance);

/** TODO */
PUBLIC EMERALD_API ogl_ui ogl_pipeline_get_ui(ogl_pipeline instance);

#endif /* OGL_PIPELINE_H */
