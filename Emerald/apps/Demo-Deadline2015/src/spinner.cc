/** Spinner renderer */
#include "shared.h"
#include "spinner.h"
#include "demo/demo_app.h"
#include "demo/demo_loader.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_math_vector.h"
#include "system/system_time.h"
#include "system/system_window.h"


#define SPINNER_N_SEGMENTS_PER_SPLINE (16)
#define SPINNER_N_SPLINES             (9)

typedef void (*PFNFIRSTFRAMECALLBACKPROC)(struct _spinner_stage* stage_ptr);


typedef struct _spinner_stage
{
    /* Duration of 0: render a single frame.
     * Duration >  0: render animation lasting specified amount of time.
     */
    system_time  duration;
    int          last_frame_index;
    system_time  start_time;

    float        maximum_spline_speed;
    unsigned int n_acceleration_frames;
    bool         should_blit_data_to_default_fbo;
    bool         should_clone_data_bo_on_first_frame;
    bool         should_shake;
    float        spin_acceleration_decay_rate;
    float        spin_traction_constant_rate;  /* acceleration[] -= constant_rate: used for acceleration faster than 0.5 */
    float        spin_traction_decay_rate;     /* acceleration[] *= decay_rate:    used for acceleration      <=     0.5 */

    float        shake_offset_vec2[2];
    float        spline_accelerations[SPINNER_N_SPLINES];
    float        spline_offsets      [SPINNER_N_SPLINES];

    float        size_modifier_end;
    float        size_modifier_start;

    PFNFIRSTFRAMECALLBACKPROC pfn_first_frame_callback_proc;
    void*                     first_frame_callback_user_arg;

    _spinner_stage()
    {
        duration                            = 0;
        maximum_spline_speed                = 0.0f;
        n_acceleration_frames               = 0;
        last_frame_index                    = -1;
        size_modifier_end                   = 1.0f;
        size_modifier_start                 = 1.0f;
        should_blit_data_to_default_fbo     = false;
        should_clone_data_bo_on_first_frame = false;
        should_shake                        = false;
        spin_acceleration_decay_rate        = 0.0f;
        spin_traction_constant_rate         = 0.0f;
        spin_traction_decay_rate            = 0.0f;
        start_time                          = -1;

        memset(shake_offset_vec2,
               0,
               sizeof(shake_offset_vec2) );
        memset(spline_accelerations,
               0,
               sizeof(spline_accelerations) );
        memset(spline_offsets,
               0,
               sizeof(spline_offsets) );

        first_frame_callback_user_arg = NULL;
        pfn_first_frame_callback_proc = NULL;
    }
} _spinner_stage;

typedef struct _spinner
{
    demo_app       app;
    ogl_context    context;
    spinner_stage  current_stage;

    unsigned int   bo_size;
    GLuint         current_frame_bo_id;
    unsigned int   current_frame_bo_start_offset;
    GLuint         previous_frame_bo_id;
    unsigned int   previous_frame_bo_start_offset;
    GLint          recent_viewport[4];
    bool           should_blit_to_default_fbo;
    GLuint         vao_id;

    ogl_program                polygonizer_po;
    unsigned int               polygonizer_po_global_wg_size[3];
    ogl_program_ub             polygonizer_po_propsBuffer_ub;
    GLuint                     polygonizer_po_propsBuffer_ub_bo_id;
    unsigned int               polygonizer_po_propsBuffer_ub_bo_size;
    unsigned int               polygonizer_po_propsBuffer_ub_bo_start_offset;
    GLuint                     polygonizer_po_propsBuffer_innerRingRadius_ub_offset;
    GLuint                     polygonizer_po_propsBuffer_nRingsToSkip_ub_offset;
    GLuint                     polygonizer_po_propsBuffer_outerRingRadius_ub_offset;
    GLuint                     polygonizer_po_propsBuffer_shakeOffset_ub_offset;
    GLuint                     polygonizer_po_propsBuffer_splineOffsets_ub_offset;
    ogl_program                renderer_po;

    ogl_texture    color_to;
    GLuint         render_fbo_id;
    ogl_texture    velocity_to;

    unsigned int    resolution[2];
    _spinner_stage  stages[SPINNER_STAGE_COUNT];

    _spinner()
    {
        context       = NULL;
        current_stage = SPINNER_STAGE_START;

        bo_size                        = 0;
        current_frame_bo_id            = 0;
        current_frame_bo_start_offset  = -1;
        previous_frame_bo_id           = 0;
        previous_frame_bo_start_offset = -1;
        should_blit_to_default_fbo     = true;
        vao_id                         = 0;

        polygonizer_po                                       = NULL;
        polygonizer_po_global_wg_size[3]                     = {0};
        polygonizer_po_propsBuffer_ub                        = NULL;
        polygonizer_po_propsBuffer_ub_bo_id                  = 0;
        polygonizer_po_propsBuffer_ub_bo_size                = 0;
        polygonizer_po_propsBuffer_ub_bo_start_offset        = -1;
        polygonizer_po_propsBuffer_innerRingRadius_ub_offset = -1;
        polygonizer_po_propsBuffer_nRingsToSkip_ub_offset    = -1;
        polygonizer_po_propsBuffer_outerRingRadius_ub_offset = -1;
        polygonizer_po_propsBuffer_shakeOffset_ub_offset     = -1;
        polygonizer_po_propsBuffer_splineOffsets_ub_offset   = -1;
        renderer_po                                          = NULL;

        color_to      = NULL;
        render_fbo_id = 0;
        velocity_to   = NULL;

        memset(recent_viewport,
               0,
               sizeof(recent_viewport) );
        memset(resolution,
               0,
               sizeof(resolution) );
    }
} _spinner;


PRIVATE _spinner spinner;


/* Forward declarations */
PRIVATE void _spinner_copy_spline_offsets              (_spinner_stage* stage_ptr);
PRIVATE void _spinner_deinit_stuff_callback            (void*           unused);
PRIVATE void _spinner_deinit_stuff_rendering_callback  (ogl_context     context,
                                                        void*           user_arg);
PRIVATE void _spinner_draw_animation_rendering_callback(ogl_context     context,
                                                        uint32_t        n_frame,
                                                        system_time     frame_time,
                                                        const int*      rendering_area_px_topdown,
                                                        void*           user_arg);
PRIVATE void _spinner_draw_frame_rendering_callback    (ogl_context     context,
                                                        void*           user_arg);
PRIVATE void _spinner_init_stuff_rendering_callback    (ogl_context     context,
                                                        void*           unused);


/** TODO */
PRIVATE void _spinner_copy_spline_offsets(_spinner_stage* stage_ptr)
{
    memcpy(stage_ptr->spline_offsets,
           ( ((_spinner_stage*) stage_ptr->first_frame_callback_user_arg)->spline_offsets),
           sizeof(stage_ptr->spline_offsets) );
}

/** TODO */
PRIVATE void _spinner_deinit_stuff_callback(void* unused)
{
    /* POs */
    if (spinner.polygonizer_po != NULL)
    {
        ogl_program_release(spinner.polygonizer_po);

        spinner.polygonizer_po = NULL;
    }

    if (spinner.renderer_po != NULL)
    {
        ogl_program_release(spinner.renderer_po);

        spinner.renderer_po = NULL;
    }

    /* TOs */
    if (spinner.color_to != NULL)
    {
        ogl_texture_release(spinner.color_to);

        spinner.color_to = NULL;
    }

    if (spinner.velocity_to != NULL)
    {
        ogl_texture_release(spinner.velocity_to);

        spinner.velocity_to = NULL;
    }

    /* Request rendering context call-back to deinit GL stuff */
    ogl_context_request_callback_from_context_thread(spinner.context,
                                                     _spinner_deinit_stuff_rendering_callback,
                                                     NULL); /* user_arg */
}

/** TODO */
PRIVATE void _spinner_deinit_stuff_rendering_callback(ogl_context context,
                                                      void*       user_arg)
{
    ogl_buffers                 buffers         = NULL;
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(spinner.context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);
    ogl_context_get_property(spinner.context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* BOs */
    if (spinner.current_frame_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(buffers,
                                       spinner.current_frame_bo_id,
                                       spinner.current_frame_bo_start_offset);

        spinner.current_frame_bo_id           = 0;
        spinner.current_frame_bo_start_offset = -1;
    }

    if (spinner.previous_frame_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(buffers,
                                       spinner.previous_frame_bo_id,
                                       spinner.previous_frame_bo_start_offset);

        spinner.previous_frame_bo_id           = 0;
        spinner.previous_frame_bo_start_offset = -1;
    }

    spinner.bo_size = 0;

    /* FBOs */
    if (spinner.render_fbo_id != 0)
    {
        entrypoints_ptr->pGLDeleteFramebuffers(1,
                                              &spinner.render_fbo_id);

        spinner.render_fbo_id = 0;
    }

    /* VAOs */
    if (spinner.vao_id != 0)
    {
        entrypoints_ptr->pGLDeleteVertexArrays(1,
                                              &spinner.vao_id);

        spinner.vao_id = 0;
    }
}

/** TODO */
PRIVATE void _spinner_draw_animation_rendering_callback(ogl_context context,
                                                        uint32_t    n_frame,
                                                        system_time frame_time,
                                                        const int*  rendering_area_px_topdown,
                                                        void*       user_arg)
{
    GLuint                            default_fbo_id    = -1;
    const ogl_context_gl_entrypoints* entrypoints_ptr   = NULL;
    unsigned int                      resolution[2];
    _spinner_stage*                   stage_ptr         = spinner.stages + spinner.current_stage;
    uint32_t                          target_frame_rate = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                            &default_fbo_id);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    demo_app_get_property   (spinner.app,
                             DEMO_APP_PROPERTY_RESOLUTION,
                             resolution);
    demo_app_get_property   (spinner.app,
                             DEMO_APP_PROPERTY_TARGET_FRAME_RATE,
                            &target_frame_rate);

    /* If this is the first frame and there's a "first frame callback" defined, call it now. */
    if (stage_ptr->pfn_first_frame_callback_proc != NULL)
    {
        stage_ptr->pfn_first_frame_callback_proc(stage_ptr);

        stage_ptr->pfn_first_frame_callback_proc = NULL;
    } /* if (stage_ptr->pfn_first_frame_callback_proc != NULL) */

    /* Update shake factor. */
    if (stage_ptr->should_shake)
    {
        const float texel_size_ss[] = 
        {
            2.0f / float(resolution[0]),
            2.0f / float(resolution[1])
        };
        float t = float((int32_t) n_frame - 50) / 1000.0f;

        if (t > 0.0f)
        {
            stage_ptr->shake_offset_vec2[0] = texel_size_ss[0] * sin(float(n_frame * 1255)) * (t * (10.0f + n_frame / 20));
            stage_ptr->shake_offset_vec2[1] = texel_size_ss[1] * cos(float(n_frame * 3711)) * (t * (10.0f + n_frame / 20));

            if (stage_ptr->shake_offset_vec2[0] < 1.0f * texel_size_ss[0])
            {
                stage_ptr->shake_offset_vec2[0] = 0.0f;
            }

            if (stage_ptr->shake_offset_vec2[1] < 1.0f * texel_size_ss[1])
            {
                stage_ptr->shake_offset_vec2[1] = 0.0f;
            }
        } /* if (t > 0.0f) */
        else
        {
            stage_ptr->shake_offset_vec2[0] = 0.0f;
            stage_ptr->shake_offset_vec2[1] = 0.0f;
        }
    }
    else
    {
        memset(stage_ptr->shake_offset_vec2,
               0,
               sizeof(stage_ptr->shake_offset_vec2) );
    }

    /* Update spline offsets. */
    for (int32_t n_current_frame = stage_ptr->last_frame_index;
                 n_current_frame < (int32_t) n_frame;
               ++n_current_frame)
    {
        if (n_current_frame < (int32_t) stage_ptr->n_acceleration_frames)
        {
            float t = float(n_current_frame) / float(stage_ptr->n_acceleration_frames - 1);

            for (uint32_t n_spline = 0;
                          n_spline < SPINNER_N_SPLINES;
                        ++n_spline)
            {
                float coeff             = t * t * (3.0f - 2.0f * t);
                float spline_speed_base = float(( (n_spline + 1) * 1537) % 7) / 6.0f;

                stage_ptr->spline_accelerations[n_spline] += coeff * (spline_speed_base * stage_ptr->maximum_spline_speed * stage_ptr->spin_acceleration_decay_rate) * 3.14152965f;
            }
        } /* if (n_current_frame < stage_ptr->n_acceleration_frames) */

        for (uint32_t n_spline = 0;
                      n_spline < SPINNER_N_SPLINES;
                    ++n_spline)
        {
            ASSERT_DEBUG_SYNC(stage_ptr->spline_offsets[n_spline] + stage_ptr->spline_accelerations[n_spline] >= stage_ptr->spline_offsets[n_spline],
                              "Acceleration must not be negative");

            stage_ptr->spline_offsets[n_spline] += stage_ptr->spline_accelerations[n_spline];

            if (stage_ptr->spline_accelerations[n_spline] > 0.5f)
            {
                ASSERT_DEBUG_SYNC(stage_ptr->spline_accelerations[n_spline] > stage_ptr->spin_traction_constant_rate,
                                  "Acceleration is smaller than spin traction constant rate");

                stage_ptr->spline_accelerations[n_spline] -= stage_ptr->spin_traction_constant_rate;
            }
            else
            {
                stage_ptr->spline_accelerations[n_spline] *= stage_ptr->spin_traction_decay_rate;
            }

            if (stage_ptr->spline_accelerations[n_spline] < 0.0f)
            {
                stage_ptr->spline_accelerations[n_spline] = 0.0f;
            }
        }
    } /* for (all required frames) */

    stage_ptr->last_frame_index = n_frame;

    /* Draw the frame contents */
    _spinner_draw_frame_rendering_callback(context,
                                           stage_ptr);

    /* Stop the playback if this is an excessive frame */
    if (frame_time >= stage_ptr->duration)
    {
        ogl_rendering_handler rendering_handler = NULL;
        system_window         window            = NULL;

        ogl_context_get_property  (context,
                                   OGL_CONTEXT_PROPERTY_WINDOW,
                                  &window);
        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                  &rendering_handler);

        ogl_rendering_handler_stop(rendering_handler);
    }
}

/** TODO */
PRIVATE void _spinner_draw_frame_rendering_callback(ogl_context context,
                                                    void*       user_arg)
{
    /* NOTE: This call-back does not compute the shake factor and the spline
     * offsets for two reasons:
     *
     * - we can't reliably tell which frame index we're dealing with.
     * - the entrypoint is also used to draw single frames (start stage).
     *
     * The offsets are updated in the _draw_animation callback instead.
     */
    system_time                       current_time    = system_time_now();
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _spinner_stage*                   stage_ptr       = spinner.stages + spinner.current_stage;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (stage_ptr->start_time == -1)
    {
        stage_ptr->start_time = current_time;
    }

    /* Configure viewport to use square proportions. Make sure the spinner is centered */
    GLint       new_viewport    [4];
    GLint       precall_viewport[4];
    GLint       precall_viewport_height;
    GLint       precall_viewport_width;
    GLint       spinner_size_px = 0;
    const float spinner_size_ss = 0.8f;

    /* Don't panic. This call will retrieve the info from Emerald's state cache. */
    entrypoints_ptr->pGLGetIntegerv(GL_VIEWPORT,
                                    precall_viewport);

    precall_viewport_width  = precall_viewport[2] - precall_viewport[0];
    precall_viewport_height = precall_viewport[3] - precall_viewport[1];

    if (precall_viewport_width > precall_viewport_height)
    {
        spinner_size_px = GLint(spinner_size_ss * precall_viewport_height);
    }
    else
    {
        spinner_size_px = GLint(spinner_size_ss * precall_viewport_width);
    }

    new_viewport[0] = (precall_viewport_width  - spinner_size_px) / 2;
    new_viewport[1] = (precall_viewport_height - spinner_size_px) / 2;
    new_viewport[2] = (precall_viewport_width  + spinner_size_px) / 2;
    new_viewport[3] = (precall_viewport_height + spinner_size_px) / 2;

    /* Update the spline offsets. */
    ogl_program_ub_set_arrayed_uniform_value(spinner.polygonizer_po_propsBuffer_ub,
                                             spinner.polygonizer_po_propsBuffer_splineOffsets_ub_offset,
                                             stage_ptr->spline_offsets,
                                             0,            /* src_data_flags */
                                             sizeof(float) * SPINNER_N_SPLINES,
                                             0,                  /* dst_array_start_index */
                                             SPINNER_N_SPLINES); /* dst_array_item_count  */

    /* Resize the viewport if necessary. */
    const system_time  t_duration           = (stage_ptr->duration == 0) ? 1 : stage_ptr->duration;
    float              t                    = system_math_smoothstep(float(current_time - stage_ptr->start_time) / float(t_duration), 0.0f, 1.0f);
    const float        size_modifier        = (stage_ptr->size_modifier_start + t * (stage_ptr->size_modifier_end - stage_ptr->size_modifier_start) );
    const float        inner_ring_radius    = 0.1f;
    const unsigned int n_rings_to_skip      = 1;
    const float        outer_ring_radius    = 1.0f;

    float new_viewport_x1y1_size[] =
    {
        float(new_viewport[0]),
        float(new_viewport[1]),
        float(new_viewport[2] - new_viewport[0])
    };
    float resized_viewport_x1y1_size[3];

    system_math_other_resize_quad2d_by_diagonal(new_viewport_x1y1_size,
                                                size_modifier,
                                                resized_viewport_x1y1_size);

    spinner.recent_viewport[0] = GLint(resized_viewport_x1y1_size[0]);
    spinner.recent_viewport[1] = GLint(resized_viewport_x1y1_size[1]);
    spinner.recent_viewport[2] = GLint(resized_viewport_x1y1_size[0] + resized_viewport_x1y1_size[2]);
    spinner.recent_viewport[3] = GLint(resized_viewport_x1y1_size[1] + resized_viewport_x1y1_size[2]);

    entrypoints_ptr->pGLViewport(GLint(resized_viewport_x1y1_size[0]),
                                 GLint(resized_viewport_x1y1_size[1]),
                                 GLint(resized_viewport_x1y1_size[2]),
                                 GLint(resized_viewport_x1y1_size[2]));

    /* Generate mesh data */
    entrypoints_ptr->pGLUseProgram(ogl_program_get_id(spinner.polygonizer_po) );

    ogl_program_ub_set_nonarrayed_uniform_value(spinner.polygonizer_po_propsBuffer_ub,
                                                spinner.polygonizer_po_propsBuffer_innerRingRadius_ub_offset,
                                               &inner_ring_radius,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(spinner.polygonizer_po_propsBuffer_ub,
                                                spinner.polygonizer_po_propsBuffer_nRingsToSkip_ub_offset,
                                               &n_rings_to_skip,
                                                0, /* src_data_flags */
                                                sizeof(int) );
    ogl_program_ub_set_nonarrayed_uniform_value(spinner.polygonizer_po_propsBuffer_ub,
                                                spinner.polygonizer_po_propsBuffer_shakeOffset_ub_offset,
                                                stage_ptr->shake_offset_vec2,
                                                0, /* src_data_flags */
                                                sizeof(stage_ptr->shake_offset_vec2) );
    ogl_program_ub_set_nonarrayed_uniform_value(spinner.polygonizer_po_propsBuffer_ub,
                                                spinner.polygonizer_po_propsBuffer_outerRingRadius_ub_offset,
                                               &outer_ring_radius,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_sync                        (spinner.polygonizer_po_propsBuffer_ub);

    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        spinner.current_frame_bo_id,
                                        spinner.current_frame_bo_start_offset,
                                        spinner.bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        spinner.polygonizer_po_propsBuffer_ub_bo_id,
                                        spinner.polygonizer_po_propsBuffer_ub_bo_start_offset,
                                        spinner.polygonizer_po_propsBuffer_ub_bo_size);

    entrypoints_ptr->pGLDispatchCompute(spinner.polygonizer_po_global_wg_size[0],
                                        spinner.polygonizer_po_global_wg_size[1],
                                        spinner.polygonizer_po_global_wg_size[2]);

    /* If we were asked to, copy the computed buffer contents to the "previous frame" data buffer */
    if (stage_ptr->should_clone_data_bo_on_first_frame)
    {
        entrypoints_ptr->pGLBindBuffer       (GL_COPY_READ_BUFFER,
                                              spinner.current_frame_bo_id);
        entrypoints_ptr->pGLBindBuffer       (GL_COPY_WRITE_BUFFER,
                                              spinner.previous_frame_bo_id);
        entrypoints_ptr->pGLCopyBufferSubData(GL_COPY_READ_BUFFER,
                                              GL_COPY_WRITE_BUFFER,
                                              spinner.current_frame_bo_start_offset,
                                              spinner.previous_frame_bo_start_offset,
                                              spinner.bo_size);

        stage_ptr->should_clone_data_bo_on_first_frame = false;
    }

    /* Clear the color data attachment. Note we need to reconfigure draw buffers, since we do not want to erase
     * velocity buffer contents with anything else than zeroes - bad stuff would happen if we reset it with eg.
     * vec4(1.0, vec3(0.0) ). The rasterization stage below would then only change texels covered by geometry,
     * and the untouched parts would contain the clear color, causing the post processor stage to introduce
     * glitches to the final image.
     */
    static const float  color_buffer_clear_color   [4] = {0.2f, 0.4f, 0.3f, 1.0f};
    static const float  velocity_buffer_clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    static const GLenum render_draw_buffers        []  =
    {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1
    };
    static const unsigned int n_render_draw_buffers = sizeof(render_draw_buffers) / sizeof(render_draw_buffers[0]);

    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        spinner.render_fbo_id);

    entrypoints_ptr->pGLDrawBuffer(GL_COLOR_ATTACHMENT0);
    entrypoints_ptr->pGLClearColor(color_buffer_clear_color[0],
                                   color_buffer_clear_color[1],
                                   color_buffer_clear_color[2],
                                   color_buffer_clear_color[3]);
    entrypoints_ptr->pGLClear     (GL_COLOR_BUFFER_BIT);

    entrypoints_ptr->pGLDrawBuffer(GL_COLOR_ATTACHMENT1);
    entrypoints_ptr->pGLClearColor(velocity_buffer_clear_color[0],
                                   velocity_buffer_clear_color[1],
                                   velocity_buffer_clear_color[2],
                                   velocity_buffer_clear_color[3]);
    entrypoints_ptr->pGLClear     (GL_COLOR_BUFFER_BIT);

    entrypoints_ptr->pGLDrawBuffers(n_render_draw_buffers,
                                    render_draw_buffers);

    /* Render the data */
    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(spinner.renderer_po) );
    entrypoints_ptr->pGLBindVertexArray(spinner.vao_id);

    entrypoints_ptr->pGLDisable      (GL_CULL_FACE);  /* culling is not needed - all triangles are always visible */
    entrypoints_ptr->pGLDisable      (GL_DEPTH_TEST); /* depth test not needed - all triangles are always visible */
    entrypoints_ptr->pGLMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    entrypoints_ptr->pGLDrawArrays   (GL_TRIANGLES,
                                      0, /* first */
                                      SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES * 2 /* triangles */ * 2 /* top half, bottom half */ * 3 /* vertices per triangle */);

    /* Cache the contents of the data buffer for subsequent frame */
    entrypoints_ptr->pGLBindBuffer   (GL_COPY_READ_BUFFER,
                                      spinner.current_frame_bo_id);
    entrypoints_ptr->pGLBindBuffer   (GL_COPY_WRITE_BUFFER,
                                      spinner.previous_frame_bo_id);

    entrypoints_ptr->pGLMemoryBarrier    (GL_BUFFER_UPDATE_BARRIER_BIT);
    entrypoints_ptr->pGLCopyBufferSubData(GL_COPY_READ_BUFFER,
                                          GL_COPY_WRITE_BUFFER,
                                          spinner.current_frame_bo_start_offset,  /* readOffset  */
                                          spinner.previous_frame_bo_start_offset, /* writeOffset */
                                          spinner.bo_size);

    /* Restore the viewport */
    entrypoints_ptr->pGLViewport(precall_viewport[0],
                                 precall_viewport[1],
                                 precall_viewport[2],
                                 precall_viewport[3]);
}

/** TODO */
PRIVATE void _spinner_init_stuff_rendering_callback(ogl_context context,
                                                    void*       unused)
{
    const char* cs_body = "#version 430 core\n"
                          "\n"
                          "layout (local_size_x = LOCAL_WG_SIZE_X, local_size_y = 1, local_size_z = 1) in;\n"
                          "\n"
                          "layout(std140) writeonly buffer thisFrameDataBuffer\n"
                          "{\n"
                          /* vertex: x, y
                           * uv:     s, t
                           */
                          "    restrict vec4 data[];\n"
                          "} thisFrame;\n"
                          "\n"
                          "uniform propsBuffer\n"
                          "{\n"
                          "    int   nRingsToSkip;\n"
                          "    float innerRingRadius;\n"
                          "    float outerRingRadius;\n"
                          "    vec2  shakeOffset;\n"
                          "    float splineOffsets[N_SPLINES * 2];\n"
                          "} thisFrameProps;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * 1) +\n"
                          "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)     +\n"
                          "                                            gl_GlobalInvocationID.x);\n"
                          "\n"
                          "    if (global_invocation_id_flat <  N_SEGMENTS_PER_SPLINE * thisFrameProps.nRingsToSkip * 2 ||\n"
                          "        global_invocation_id_flat >= N_SEGMENTS_PER_SPLINE * N_SPLINES                   * 2)\n"
                          "    {\n"
                          "        return;\n"
                          "    }\n"
                          "\n"
                          "    const float PI                  = 3.14152965;\n"
                          "    const uint  n_spline_segment    = (global_invocation_id_flat)     % (N_SEGMENTS_PER_SPLINE);\n"
                          "    const uint  n_spline            = (global_invocation_id_flat / 2) / (N_SEGMENTS_PER_SPLINE);\n"
                          "    const bool  is_top_half_segment = (((global_invocation_id_flat / N_SEGMENTS_PER_SPLINE) % 2) == 0);\n"
                          "    const float domain_offset       = thisFrameProps.splineOffsets[n_spline] + PI / 4 + ((is_top_half_segment) ? 0.0 : PI);\n"
                          "    const float spline_radius       = mix(thisFrameProps.innerRingRadius, thisFrameProps.outerRingRadius, float(n_spline) / float(N_SPLINES - 1) );\n"
                          "    const vec2  segment_s1s2        = vec2( float(n_spline_segment)     / float(N_SEGMENTS_PER_SPLINE),\n"
                          "                                            float(n_spline_segment + 1) / float(N_SEGMENTS_PER_SPLINE) );\n"
                          "\n"
                          /*   v1--v2
                           *   |    |
                           *   v3--v4
                           */
                          "    vec4 uv_v1v2;\n"
                          "    vec4 uv_v3v4;\n"
                          "    vec4 vertex_v1v2;\n"
                          "    vec4 vertex_v3v4;\n"
                          "\n"
                          "    vec4 base_location = vec4(cos(domain_offset + segment_s1s2.x * PI / 2.0),\n"
                          "                              sin(domain_offset + segment_s1s2.x * PI / 2.0),\n"
                          "                              cos(domain_offset + segment_s1s2.y * PI / 2.0),\n"
                          "                              sin(domain_offset + segment_s1s2.y * PI / 2.0) );\n"
                          "\n"
                          "    uv_v1v2     = vec4(0.0, 0.0, 1.0, 0.0);\n"
                          "    uv_v3v4     = vec4(0.0, 1.0, 1.0, 1.0);\n"
                          "    vertex_v1v2 = vec4(spline_radius)       * base_location + thisFrameProps.shakeOffset.xyxy;\n"
                          "    vertex_v3v4 = vec4(spline_radius - 0.1) * base_location + thisFrameProps.shakeOffset.xyxy;\n"
                          "\n"
                          /* Store segment vertex data.
                           *
                           * NOTE: Since output "data" variable is 4-component, we need not multiply the start vertex index by 4. */
                          "\n"
                          "    const uint n_start_vertex_index = global_invocation_id_flat * 2 /* triangles */ * 3 /* vertices per triangle */;\n"
                          "\n"
                          /*   v1-v2-v3 triangle */
                          "    thisFrame.data[n_start_vertex_index    ] = vec4(vertex_v1v2.xy, uv_v1v2.xy);\n"
                          "    thisFrame.data[n_start_vertex_index + 1] = vec4(vertex_v1v2.zw, uv_v1v2.zw);\n"
                          "    thisFrame.data[n_start_vertex_index + 2] = vec4(vertex_v3v4.xy, uv_v3v4.xy);\n"

                          /*   v2-v4-v3 triangle */
                          "    thisFrame.data[n_start_vertex_index + 3] = vec4(vertex_v1v2.zw, uv_v1v2.zw);\n"
                          "    thisFrame.data[n_start_vertex_index + 4] = vec4(vertex_v3v4.zw, uv_v3v4.zw);\n"
                          "    thisFrame.data[n_start_vertex_index + 5] = vec4(vertex_v3v4.xy, uv_v3v4.xy);\n"
                          "}\n";
    const char* fs_body = "#version 430 core\n"
                          "\n"
                          "flat in int vertex_id;\n"
                          "flat in  int  is_top_half;\n"
                          "     in  vec2 velocity;\n"
                          "     in  vec2 uv;\n"
                          "     in  vec4 prevThisPosition;\n"
                          "\n"
                          "layout(location = 0) out vec4 resultColor;\n"
                          "layout(location = 1) out vec2 resultSpeed;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    float shade = (is_top_half == 0) ? 1.0 : 0.1;\n"
                          "\n"
                          "    if (uv.y >= 0.8) shade = mix(1.0, 0.2, (uv.y - 0.8) / 0.2);\n"
                          "\n"
                          "    resultColor = vec4(vec3(shade), 1.0);\n"
                          //"    resultColor = vec4( float((vertex_id * 36 / 16 / 9 / 4) % 255) / 255.0, float((vertex_id * 153 / 16 / 9 / 4) % 255) / 255.0, 0.0, 1.0);\n"
                          "    resultSpeed = (prevThisPosition.xy - prevThisPosition.zw) * vec2(0.5);\n"
                          "}\n";
    const char* vs_body = "#version 430 core\n"
                          "\n"
                          "layout(location = 0) in vec4 data;\n"
                          "layout(location = 1) in vec4 prevFrameData;\n"
                          "\n"
                          "flat out int  vertex_id;\n"
                          "flat out int  is_top_half;\n"
                          "     out vec4 prevThisPosition;\n"
                          "     out vec2 uv;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    is_top_half = (gl_VertexID / 6 / N_SEGMENTS_PER_SPLINE) % 2;\n"
                          "\n"
                          "    gl_Position      = vec4(data.xy, 0.0, 1.0);\n"
                          "    prevThisPosition = vec4(prevFrameData.xy, data.xy);\n"
                          "    uv               = data.zw;\n"
                          "vertex_id = gl_VertexID;\n"
                          "}\n";

    const system_hashed_ansi_string cs_body_tokens[] =
    {
        system_hashed_ansi_string_create("LOCAL_WG_SIZE_X"),
        system_hashed_ansi_string_create("N_SEGMENTS_PER_SPLINE"),
        system_hashed_ansi_string_create("N_SPLINES")
    };
    system_hashed_ansi_string       cs_body_values[] =
    {
        NULL,
        NULL,
        NULL
    };
    const system_hashed_ansi_string vs_body_tokens[] =
    {
        system_hashed_ansi_string_create("N_SEGMENTS_PER_SPLINE"),
        system_hashed_ansi_string_create("N_SPLINES")
    };
    system_hashed_ansi_string       vs_body_values[] =
    {
        NULL,
        NULL
    };
    const ogl_program_variable*     cs_innerRingRadius_var_ptr = NULL;
    const ogl_program_variable*     cs_nRingsToSkip_var_ptr    = NULL;
    const ogl_program_variable*     cs_outerRingRadius_var_ptr = NULL;
    const ogl_program_variable*     cs_shakeOffset_var_ptr     = NULL;
    const ogl_program_variable*     cs_splineOffsets_var_ptr   = NULL;
    const unsigned int              n_cs_body_token_values     = sizeof(cs_body_tokens) / sizeof(cs_body_tokens[0]);
    const unsigned int              n_vs_body_token_values     = sizeof(vs_body_tokens) / sizeof(vs_body_tokens[0]);
    char                            temp[1024];

    ogl_buffers                                               buffers             = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    const ogl_context_gl_limits*                              limits_ptr          = NULL;
    ogl_shader                                                polygonizer_cs      = NULL;
    ogl_shader                                                renderer_fs         = NULL;
    ogl_shader                                                renderer_vs         = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    spinner.context        = context;
    spinner.polygonizer_po = ogl_program_create(context,
                                                system_hashed_ansi_string_create("Spinner polygonizer PO"),
                                                OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);
    spinner.renderer_po    = ogl_program_create(context,
                                                system_hashed_ansi_string_create("Spinner renderer PO"),
                                                OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    if (spinner.polygonizer_po == NULL ||
        spinner.renderer_po    == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create spinner PO(s)");

        goto end;
    }

    /* Prepare final polygonizer shader body */
    snprintf(temp,
             sizeof(temp),
             "%d",
             limits_ptr->max_compute_work_group_size[0]);
    cs_body_values[0] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%d",
             SPINNER_N_SEGMENTS_PER_SPLINE);
    cs_body_values[1] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%d",
             SPINNER_N_SPLINES);
    cs_body_values[2] = system_hashed_ansi_string_create(temp);

    /* Set up the polygonizer program object */
    polygonizer_cs = ogl_shader_create(context,
                                       RAL_SHADER_TYPE_COMPUTE,
                                       system_hashed_ansi_string_create("Spinner polygonizer CS") );

    if (polygonizer_cs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create spinner polygonizer CS");

        goto end;
    }

    spinner.polygonizer_po_global_wg_size[0] = 1 + (2 /* top half, bottom half */ * SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES / limits_ptr->max_compute_work_group_size[0]);
    spinner.polygonizer_po_global_wg_size[1] = 1;
    spinner.polygonizer_po_global_wg_size[2] = 1;

    ASSERT_DEBUG_SYNC(spinner.polygonizer_po_global_wg_size[0] * spinner.polygonizer_po_global_wg_size[1] * spinner.polygonizer_po_global_wg_size[2] <= (unsigned int) limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(spinner.polygonizer_po_global_wg_size[0] < (unsigned int) limits_ptr->max_compute_work_group_count[0] &&
                      spinner.polygonizer_po_global_wg_size[1] < (unsigned int) limits_ptr->max_compute_work_group_count[1] &&
                      spinner.polygonizer_po_global_wg_size[2] < (unsigned int) limits_ptr->max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    ogl_shader_set_body      (polygonizer_cs,
                              system_hashed_ansi_string_create_by_token_replacement(cs_body,
                                                                                    n_cs_body_token_values,
                                                                                    cs_body_tokens,
                                                                                    cs_body_values) );
    ogl_program_attach_shader(spinner.polygonizer_po,
                              polygonizer_cs);

    if (!ogl_program_link(spinner.polygonizer_po) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not link spinner polygonizer PO");

        goto end;
    }

    /* Retrieve polygonizer PO properties */
    ogl_program_get_uniform_block_by_name(spinner.polygonizer_po,
                                          system_hashed_ansi_string_create("propsBuffer"),
                                         &spinner.polygonizer_po_propsBuffer_ub);

    ASSERT_DEBUG_SYNC(spinner.polygonizer_po_propsBuffer_ub != NULL,
                      "propsBuffer uniform block is not recognized by GL");

    ogl_program_ub_get_property(spinner.polygonizer_po_propsBuffer_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &spinner.polygonizer_po_propsBuffer_ub_bo_size);
    ogl_program_ub_get_property(spinner.polygonizer_po_propsBuffer_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &spinner.polygonizer_po_propsBuffer_ub_bo_id);
    ogl_program_ub_get_property(spinner.polygonizer_po_propsBuffer_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &spinner.polygonizer_po_propsBuffer_ub_bo_start_offset);

    ogl_program_ub_get_variable_by_name(spinner.polygonizer_po_propsBuffer_ub,
                                        system_hashed_ansi_string_create("innerRingRadius"),
                                       &cs_innerRingRadius_var_ptr);
    ogl_program_ub_get_variable_by_name(spinner.polygonizer_po_propsBuffer_ub,
                                        system_hashed_ansi_string_create("nRingsToSkip"),
                                       &cs_nRingsToSkip_var_ptr);
    ogl_program_ub_get_variable_by_name(spinner.polygonizer_po_propsBuffer_ub,
                                        system_hashed_ansi_string_create("outerRingRadius"),
                                       &cs_outerRingRadius_var_ptr);
    ogl_program_ub_get_variable_by_name(spinner.polygonizer_po_propsBuffer_ub,
                                        system_hashed_ansi_string_create("shakeOffset"),
                                       &cs_shakeOffset_var_ptr);
    ogl_program_ub_get_variable_by_name(spinner.polygonizer_po_propsBuffer_ub,
                                        system_hashed_ansi_string_create("splineOffsets[0]"),
                                       &cs_splineOffsets_var_ptr);

    ASSERT_DEBUG_SYNC(cs_innerRingRadius_var_ptr != NULL &&
                      cs_nRingsToSkip_var_ptr    != NULL &&
                      cs_outerRingRadius_var_ptr != NULL &&
                      cs_shakeOffset_var_ptr     != NULL &&
                      cs_splineOffsets_var_ptr   != NULL,
                      "Spinner props UB variables are not recognized by GL");

    spinner.polygonizer_po_propsBuffer_innerRingRadius_ub_offset = cs_innerRingRadius_var_ptr->block_offset;
    spinner.polygonizer_po_propsBuffer_nRingsToSkip_ub_offset    = cs_nRingsToSkip_var_ptr->block_offset;
    spinner.polygonizer_po_propsBuffer_outerRingRadius_ub_offset = cs_outerRingRadius_var_ptr->block_offset;
    spinner.polygonizer_po_propsBuffer_shakeOffset_ub_offset     = cs_shakeOffset_var_ptr->block_offset;
    spinner.polygonizer_po_propsBuffer_splineOffsets_ub_offset   = cs_splineOffsets_var_ptr->block_offset;

    /* Prepare renderer vertex shader key+token values */
    snprintf(temp,
             sizeof(temp),
             "%d",
             SPINNER_N_SEGMENTS_PER_SPLINE);
    vs_body_values[0] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%d",
             SPINNER_N_SPLINES);
    vs_body_values[1] = system_hashed_ansi_string_create(temp);

    /* Set up renderer PO */
    renderer_fs = ogl_shader_create(context,
                                    RAL_SHADER_TYPE_FRAGMENT,
                                    system_hashed_ansi_string_create("Spinner renderer FS") );
    renderer_vs = ogl_shader_create(context,
                                    RAL_SHADER_TYPE_VERTEX,
                                    system_hashed_ansi_string_create("Spinner renderer VS") );

    ASSERT_DEBUG_SYNC(renderer_fs != NULL &&
                      renderer_vs != NULL,
                      "Could not create spinner renderer FS and/or VS");

    ogl_shader_set_body(renderer_fs,
                        system_hashed_ansi_string_create(fs_body) );
    ogl_shader_set_body(renderer_vs,
                        system_hashed_ansi_string_create_by_token_replacement(vs_body,
                                                                              n_vs_body_token_values,
                                                                              vs_body_tokens,
                                                                              vs_body_values) );

    ogl_program_attach_shader(spinner.renderer_po,
                              renderer_fs);
    ogl_program_attach_shader(spinner.renderer_po,
                              renderer_vs);

    if (!ogl_program_link(spinner.renderer_po) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not link spinner renderer PO");

        goto end;
    }

    /* Set up BO to hold polygonized spinner data */
    spinner.bo_size = SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES * 2 /* triangles */ * 3 /* vertices per triangle */ * 4 /* comp's per vertex */ * sizeof(float) * 2 /* top half, bottom half */;

    ogl_buffers_allocate_buffer_memory(buffers,
                                       spinner.bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_VBO,
                                       0,
                                      &spinner.current_frame_bo_id,
                                      &spinner.current_frame_bo_start_offset);
    ogl_buffers_allocate_buffer_memory(buffers,
                                       spinner.bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_VBO,
                                       0,
                                      &spinner.previous_frame_bo_id,
                                      &spinner.previous_frame_bo_start_offset);

    /* Set up VAO for the rendering process */
    struct _vao
    {
        GLuint  prev_frame_bo_id;
        GLuint  prev_frame_bo_start_offset;

        GLuint  this_frame_bo_id;
        GLuint  this_frame_bo_start_offset;

        GLuint* vao_id_ptr;
    } vaos[] =
    {
        {
            spinner.previous_frame_bo_id,
            spinner.previous_frame_bo_start_offset,
            spinner.current_frame_bo_id,
            spinner.current_frame_bo_start_offset,
           &spinner.vao_id},
    };
    const unsigned int n_vaos = sizeof(vaos) / sizeof(vaos[0]);

    for (unsigned int n_vao = 0;
                      n_vao < n_vaos;
                    ++n_vao)
    {
        _vao& current_vao = vaos[n_vao];

        entrypoints_ptr->pGLGenVertexArrays        (1,
                                                    current_vao.vao_id_ptr);
        entrypoints_ptr->pGLBindVertexArray        (*current_vao.vao_id_ptr);

        entrypoints_ptr->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                    current_vao.this_frame_bo_id);
        entrypoints_ptr->pGLVertexAttribPointer    (0,                                            /* index */
                                                    4,                                            /* size */
                                                    GL_FLOAT,
                                                    GL_FALSE,                                                /* normalized */
                                                    sizeof(float) * 4,                                       /* stride */
                                                    (const GLvoid*) current_vao.this_frame_bo_start_offset); /* pointer */

        entrypoints_ptr->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                    current_vao.prev_frame_bo_id);
        entrypoints_ptr->pGLVertexAttribPointer    (1,                                            /* index */
                                                    4,                                            /* size */
                                                    GL_FLOAT,
                                                    GL_FALSE,                                                /* normalized */
                                                    sizeof(float) * 4,                                       /* stride */
                                                    (const GLvoid*) current_vao.prev_frame_bo_start_offset); /* pointer */

        entrypoints_ptr->pGLEnableVertexAttribArray(0);                                           /* index */
        entrypoints_ptr->pGLEnableVertexAttribArray(1);                                           /* index */
    } /* for (all VAOs to set up) */

    /* Set up the TO to hold pre-processed color & velocity data */
    const GLenum render_fbo_draw_buffers[] =
    {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1
    };
    const GLint  n_render_fbo_draw_buffers = sizeof(render_fbo_draw_buffers) / sizeof(render_fbo_draw_buffers[0]);
    const float  zero_vec4[4]              = { 0.0f, 0.0f, 0.0f, 0.0f };

    spinner.color_to    = ogl_texture_create_empty(context,
                                                   system_hashed_ansi_string_create("Color TO"));
    spinner.velocity_to = ogl_texture_create_empty(context,
                                                   system_hashed_ansi_string_create("Velocity TO"));

    entrypoints_ptr->pGLBindTexture (GL_TEXTURE_2D,
                                     spinner.color_to);
    entrypoints_ptr->pGLTexStorage2D(GL_TEXTURE_2D,
                                     1, /* levels */
                                     GL_RGBA8,
                                     spinner.resolution[0],
                                     spinner.resolution[1]);

    entrypoints_ptr->pGLBindTexture (GL_TEXTURE_2D,
                                     spinner.velocity_to);
    entrypoints_ptr->pGLTexStorage2D(GL_TEXTURE_2D,
                                     1, /* levels */
                                     GL_RG16F,
                                     spinner.resolution[0],
                                     spinner.resolution[1]);

    entrypoints_ptr->pGLGenFramebuffers     (1,
                                            &spinner.render_fbo_id);
    entrypoints_ptr->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER,
                                             spinner.render_fbo_id);
    entrypoints_ptr->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                             GL_COLOR_ATTACHMENT0,
                                             GL_TEXTURE_2D,
                                             spinner.color_to,
                                             0); /* level */
    entrypoints_ptr->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                             GL_COLOR_ATTACHMENT1,
                                             GL_TEXTURE_2D,
                                             spinner.velocity_to,
                                             0); /* level */
    entrypoints_ptr->pGLDrawBuffers         (n_render_fbo_draw_buffers,
                                             render_fbo_draw_buffers);

    ASSERT_DEBUG_SYNC(entrypoints_ptr->pGLCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                      "Spinner render FBO is incomplete");

    /* All done */
end:
    if (polygonizer_cs != NULL)
    {
        ogl_shader_release(polygonizer_cs);

        polygonizer_cs = NULL;
    }

    if (renderer_fs != NULL)
    {
        ogl_shader_release(renderer_fs);

        renderer_fs = NULL;
    }

    if (renderer_vs != NULL)
    {
        ogl_shader_release(renderer_vs);

        renderer_vs = NULL;
    }
}


/** Please see header for specification */
PUBLIC void spinner_get_property(spinner_property property,
                                 void*            out_result_ptr)
{
    switch (property)
    {
        case SPINNER_PROPERTY_RESULT_COLOR_TO:
        {
            *(ogl_texture*) out_result_ptr = spinner.color_to;

            break;
        }

        case SPINNER_PROPERTY_RESULT_VELOCITY_TO:
        {
            *(ogl_texture*) out_result_ptr = spinner.velocity_to;

            break;
        }

        case SPINNER_PROPERTY_VIEWPORT:
        {
            memcpy(out_result_ptr,
                   spinner.recent_viewport,
                   sizeof(spinner.recent_viewport) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized spinner_property value.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC void spinner_init(demo_app    app,
                         demo_loader loader)
{
    /* Cache a few things first. */
    demo_app_get_property(app,
                          DEMO_APP_PROPERTY_RESOLUTION,
                          spinner.resolution);

    spinner.app = app;

    /* Store predefined spinner stage parameters */
    spinner.stages[SPINNER_STAGE_START].should_clone_data_bo_on_first_frame = true;

    spinner.stages[SPINNER_STAGE_FIRST_SPIN].duration                     = system_time_get_time_for_msec(1800);
    spinner.stages[SPINNER_STAGE_FIRST_SPIN].maximum_spline_speed         = 0.6f;
    spinner.stages[SPINNER_STAGE_FIRST_SPIN].n_acceleration_frames        = 10;
    spinner.stages[SPINNER_STAGE_FIRST_SPIN].spin_acceleration_decay_rate = 1.0f/90.0f;
    spinner.stages[SPINNER_STAGE_FIRST_SPIN].spin_traction_constant_rate  = 0.006f;
    spinner.stages[SPINNER_STAGE_FIRST_SPIN].spin_traction_decay_rate     = 0.9f;

    spinner.stages[SPINNER_STAGE_SECOND_SPIN].duration                      = system_time_get_time_for_msec(1800);
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].first_frame_callback_user_arg = spinner.stages + SPINNER_STAGE_FIRST_SPIN;
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].pfn_first_frame_callback_proc = _spinner_copy_spline_offsets;
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].maximum_spline_speed          = 0.8f;
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].n_acceleration_frames         = 10;
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].spin_acceleration_decay_rate  = 1.0f/90.0f;
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].spin_traction_constant_rate   = 0.006f;
    spinner.stages[SPINNER_STAGE_SECOND_SPIN].spin_traction_decay_rate      = 0.9f;

    spinner.stages[SPINNER_STAGE_ANIMATION].duration                      = system_time_get_time_for_msec(10000 /* 5650 */);
    spinner.stages[SPINNER_STAGE_ANIMATION].first_frame_callback_user_arg = spinner.stages + SPINNER_STAGE_SECOND_SPIN;
    spinner.stages[SPINNER_STAGE_ANIMATION].pfn_first_frame_callback_proc = _spinner_copy_spline_offsets;
    spinner.stages[SPINNER_STAGE_ANIMATION].maximum_spline_speed          = 100.0f;
    spinner.stages[SPINNER_STAGE_ANIMATION].n_acceleration_frames         = 3000;
    spinner.stages[SPINNER_STAGE_ANIMATION].size_modifier_end             = 6.0f;
    spinner.stages[SPINNER_STAGE_ANIMATION].size_modifier_start           = 1.0f;
    spinner.stages[SPINNER_STAGE_ANIMATION].should_shake                  = true;
    spinner.stages[SPINNER_STAGE_ANIMATION].spin_acceleration_decay_rate  = 1.0f/180.0f;
    spinner.stages[SPINNER_STAGE_ANIMATION].spin_traction_constant_rate   = 0.006f;
    spinner.stages[SPINNER_STAGE_ANIMATION].spin_traction_decay_rate      = 0.9f;

    /* We don't need to separate the initialization process into separate steps,
     * so request a single rendering thread call-back.
     */
    demo_loader_op_request_rendering_thread_callback init_stuff_op;

    init_stuff_op.pfn_rendering_callback_proc = _spinner_init_stuff_rendering_callback;
    init_stuff_op.pfn_teardown_func_ptr       = _spinner_deinit_stuff_callback;
    init_stuff_op.should_swap_buffers         = false;
    init_stuff_op.teardown_callback_user_arg  = NULL;
    init_stuff_op.user_arg                    = NULL;

    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK,
                                 &init_stuff_op);
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void spinner_render(uint32_t    n_frame,
                                                  system_time frame_time)
{
    _spinner_draw_animation_rendering_callback(spinner.context,
                                               n_frame,
                                               frame_time,
                                               NULL,  /* rendering_area_px_topdown - unused */
                                               NULL); /* user_arg                  - unused */
}

/** Please see header for specification */
PUBLIC void spinner_set_property(spinner_property property,
                                 const void*      data)
{
    switch (property)
    {
        case SPINNER_PROPERTY_CURRENT_STAGE:
        {
            spinner.current_stage = *(spinner_stage*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized spinner_property value.");
        }
    } /* switch (property) */
}