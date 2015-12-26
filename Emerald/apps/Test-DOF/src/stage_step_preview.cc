/**
 *
 * DOF test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_dof_scheuermann.h"
#include "stage_step_julia.h"
#include "stage_step_preview.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "raGL/raGL_framebuffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"

/** TODO */
static void _stage_step_preview_execute(ral_context context,
                                        uint32_t    frame_index,
                                        system_time time,
                                        const int*  rendering_area_px_topdown,
                                        void*       not_used)
{
    ral_framebuffer                   combination_fbo         = stage_step_dof_scheuermann_get_combination_fbo();
    raGL_framebuffer                  combination_fbo_raGL    = NULL;
    GLuint                            combination_fbo_raGL_id = 0;
    ogl_context                       context_gl              = NULL;
    ral_framebuffer                   draw_fbo                = NULL;
    raGL_framebuffer                  draw_fbo_raGL           = NULL;
    GLuint                            draw_fbo_raGL_id        = 0;
    const ogl_context_gl_entrypoints* entrypoints             = NULL;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO,
                            &draw_fbo);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);


    combination_fbo_raGL = ral_context_get_framebuffer_gl(context,
                                                          combination_fbo);
    draw_fbo_raGL        = ral_context_get_framebuffer_gl(context,
                                                          draw_fbo);

    raGL_framebuffer_get_property(combination_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                  &combination_fbo_raGL_id);
    raGL_framebuffer_get_property(draw_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &draw_fbo_raGL_id);

    /* Blit our custom framebuffer's contents to the system one */
    entrypoints->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                    combination_fbo_raGL_id);
    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    draw_fbo_raGL_id);
    entrypoints->pGLBlitFramebuffer(0,    /* srcX0 */
                                    0,    /* srcY0 */
                                    main_get_window_width(),  /* srcX1 */
                                    main_get_window_height(), /* srcY1 */
                                    0,    /* dstX0 */
                                    0,    /* dstY0 */
                                    main_get_window_width(),  /* dstX1 */
                                    main_get_window_height(), /* dstY1 */
                                    GL_COLOR_BUFFER_BIT,
                                    GL_NEAREST);
}

/* Please see header for specification */
PUBLIC void stage_step_preview_deinit(ral_context context)
{
    // Nothing to do 
}

/* Please see header for specification */
PUBLIC void stage_step_preview_init(ral_context  context,
                                    ogl_pipeline pipeline,
                                    uint32_t     stage_id)
{
    /* Add ourselves to the pipeline */
    ogl_pipeline_stage_step_declaration blit_stage_step;

    blit_stage_step.name             = system_hashed_ansi_string_create("Blitting");
    blit_stage_step.pfn_callback_proc = _stage_step_preview_execute;
    blit_stage_step.user_arg          = NULL;

    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                               &blit_stage_step);
}
