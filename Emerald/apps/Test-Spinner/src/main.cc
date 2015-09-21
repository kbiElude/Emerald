/**
 *
 * Spinner loader test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_vao.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_variant.h"
#include "system/system_window.h"
#include "main.h"

#define SPINNER_N_SEGMENTS_PER_SPLINE (64)
#define SPINNER_N_SPLINES             (16)


PRIVATE ogl_context      _context             = NULL;
PRIVATE ogl_pipeline     _pipeline            = NULL;
PRIVATE uint32_t         _pipeline_stage_id   = 0;
PRIVATE system_matrix4x4 _projection_matrix   = NULL;
PRIVATE system_event     _window_closed_event = system_event_create(true); /* manual_reset */
PRIVATE int              _window_size[2]      = {1280, 720};
PRIVATE system_matrix4x4 _view_matrix         = NULL;

PRIVATE GLuint         _spinner_bo_id                                                = 0;
PRIVATE unsigned int   _spinner_bo_size                                              = 0;
PRIVATE unsigned int   _spinner_bo_start_offset                                      = -1;
PRIVATE ogl_program    _spinner_polygonizer_po                                       = NULL;
PRIVATE unsigned int   _spinner_polygonizer_po_global_wg_size[3]                     = {0};
PRIVATE ogl_program_ub _spinner_polygonizer_po_propsBuffer_ub                        = NULL;
PRIVATE GLuint         _spinner_polygonizer_po_propsBuffer_ub_bo_id                  = 0;
PRIVATE unsigned int   _spinner_polygonizer_po_propsBuffer_ub_bo_size                = 0;
PRIVATE unsigned int   _spinner_polygonizer_po_propsBuffer_ub_bo_start_offset        = -1;
PRIVATE GLuint         _spinner_polygonizer_po_propsBuffer_innerRingRadius_ub_offset = -1;
PRIVATE GLuint         _spinner_polygonizer_po_propsBuffer_outerRingRadius_ub_offset = -1;
PRIVATE ogl_program    _spinner_renderer_po                                          = NULL;
PRIVATE GLuint         _spinner_vao_id                                               = 0;


/* Forward declarations */
PRIVATE void _deinit_spinner                 ();
PRIVATE void _init_spinner                   ();
PRIVATE void _render_spinner                 ();
PRIVATE void _rendering_handler              (ogl_context             context,
                                              uint32_t                n_frames_rendered,
                                              system_time             frame_time,
                                              const int*              rendering_area_px_topdown,
                                              void*                   renderer);
PRIVATE bool _rendering_rbm_callback_handler (system_window           window,
                                              unsigned short          x,
                                              unsigned short          y,
                                              system_window_vk_status new_status,
                                              void*);
PRIVATE void _window_closed_callback_handler (system_window           window);
PRIVATE void _window_closing_callback_handler(system_window           window);


/** TODO */
PRIVATE void _deinit_spinner()
{
    ogl_buffers                 buffers         = NULL;
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (_spinner_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(buffers,
                                       _spinner_bo_id,
                                       _spinner_bo_start_offset);

        _spinner_bo_id           = 0;
        _spinner_bo_size         = 0;
        _spinner_bo_start_offset = -1;
    }

    if (_spinner_polygonizer_po != NULL)
    {
        ogl_program_release(_spinner_polygonizer_po);

        _spinner_polygonizer_po = NULL;
    }

    if (_spinner_renderer_po != NULL)
    {
        ogl_program_release(_spinner_renderer_po);

        _spinner_renderer_po = NULL;
    }

    if (_spinner_vao_id != 0)
    {
        entrypoints_ptr->pGLDeleteVertexArrays(1,
                                              &_spinner_vao_id);

        _spinner_vao_id = 0;
    }
}

/** TODO */
PRIVATE void _init_spinner()
{
    const char* cs_body = "#version 430 core\n"
                          "\n"
                          "layout (local_size_x = LOCAL_WG_SIZE_X, local_size_y = 1, local_size_z = 1) in;\n"
                          "\n"
                          "writeonly buffer dataBuffer\n"
                          "{\n"
                          "    restrict vec2 data[];\n"
                          "};\n"
                          "\n"
                          "uniform propsBuffer\n"
                          "{\n"
                          "    float innerRingRadius;\n"
                          "    float outerRingRadius;\n"
                          "};\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * 1) +\n"
                          "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)     +\n"
                          "                                            gl_GlobalInvocationID.x);\n"
                          "\n"
                          "    if (global_invocation_id_flat >= N_SEGMENTS_PER_SPLINE * N_SPLINES * 2)\n"
                          "    {\n"
                          "        return;\n"
                          "    }\n"
                          "\n"
                          "    const float PI                  = 3.14152965;\n"
                          "    const bool  is_top_half_segment = ((global_invocation_id_flat % 2) == 0);\n"
                          "    const float domain_offset       = ((is_top_half_segment) ? 0.0 : PI);\n"
                          "    const uint  n_spline_segment    = global_invocation_id_flat % (N_SEGMENTS_PER_SPLINE * 2);\n"
                          "    const uint  n_spline            = global_invocation_id_flat / (N_SEGMENTS_PER_SPLINE * 2);\n"
                          "    const float spline_radius       = mix(innerRingRadius, outerRingRadius, float(n_spline) / float(N_SPLINES - 1) );\n"
                          "    const vec2  segment_s1s2        = vec2( float(n_spline_segment)     / float(N_SEGMENTS_PER_SPLINE - 1),\n"
                          "                                            float(n_spline_segment + 1) / float(N_SEGMENTS_PER_SPLINE - 1) );\n"
                          "\n"
                          /*   v1--v2
                           *   |    |
                           *   v3--v4
                           */
                          "    vec4 segment_v1v2;\n"
                          "    vec4 segment_v3v4;\n"
                          "\n"
                          "    segment_v1v2 = vec4(spline_radius) * vec4(cos(domain_offset + segment_s1s2.x * PI / 2.0),\n"
                          "                                              sin(domain_offset + segment_s1s2.x * PI / 2.0),\n"
                          "                                              cos(domain_offset + segment_s1s2.y * PI / 2.0),\n"
                          "                                              sin(domain_offset + segment_s1s2.y * PI / 2.0) );\n"
                          "    segment_v3v4 = vec4(-segment_v1v2.y,\n"
                          "                         segment_v1v2.x,\n"
                          "                        -segment_v1v2.w,\n"
                          "                         segment_v1v2.z);\n"
                          "\n"
                          /* Store segment vertex data.
                           *
                           * NOTE: Since output "data" variable is 2-component, we need not multiply the start vertex index by 2. */
                          "\n"
                          "    const uint n_start_vertex_index = global_invocation_id_flat * 2 /* triangles */ * 3 /* vertices per triangle */;\n"
                          "\n"
                          /*   v1-v2-v3 triangle */
                          "    data[n_start_vertex_index    ] = segment_v1v2.xy;\n"
                          "    data[n_start_vertex_index + 1] = segment_v1v2.zw;\n"
                          "    data[n_start_vertex_index + 2] = segment_v3v4.xy;\n"

                          /*   v2-v4-v3 triangle */
                          "    data[n_start_vertex_index + 3] = segment_v1v2.zw;\n"
                          "    data[n_start_vertex_index + 4] = segment_v3v4.zw;\n"
                          "    data[n_start_vertex_index + 5] = segment_v3v4.xy;\n"
                          "}\n";
    const char* fs_body = "#version 430 core\n"
                          "\n"
                          "out vec4 result;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          /* todo: temp */
                          "    result = vec4(gl_FragCoord.xy / vec2(1280.0, 720.0), 0.0, 1.0);\n"
                          "}\n";
    const char* vs_body = "#version 430 core\n"
                          "\n"
                          "in vec2 data;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = vec4(data.xy, 0.0, 1.0);\n"
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
    const ogl_program_variable*     cs_innerRingRadius_var_ptr = NULL;
    const ogl_program_variable*     cs_outerRingRadius_var_ptr = NULL;
    const unsigned int              n_cs_body_token_values     = sizeof(cs_body_tokens) / sizeof(cs_body_tokens[0]);
    char                            temp[1024];

    ogl_buffers                       buffers         = NULL;
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    const ogl_context_gl_limits*      limits_ptr      = NULL;
    ogl_shader                        polygonizer_cs  = NULL;
    ogl_shader                        renderer_fs     = NULL;
    ogl_shader                        renderer_vs     = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    _spinner_polygonizer_po = ogl_program_create(_context,
                                                 system_hashed_ansi_string_create("Spinner polygonizer PO"),
                                                 OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);
    _spinner_renderer_po    = ogl_program_create(_context,
                                                 system_hashed_ansi_string_create("Spinner renderer PO"),
                                                 OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    if (_spinner_polygonizer_po == NULL ||
        _spinner_renderer_po    == NULL)
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
    polygonizer_cs = ogl_shader_create(_context,
                                       SHADER_TYPE_COMPUTE,
                                       system_hashed_ansi_string_create("Spinner polygonizer CS") );

    if (polygonizer_cs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create spinner polygonizer CS");

        goto end;
    }


    _spinner_polygonizer_po_global_wg_size[0] = 1 + (SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES / limits_ptr->max_compute_work_group_size[0]);
    _spinner_polygonizer_po_global_wg_size[1] = 1;
    _spinner_polygonizer_po_global_wg_size[2] = 1;

    ASSERT_DEBUG_SYNC(_spinner_polygonizer_po_global_wg_size[0] * _spinner_polygonizer_po_global_wg_size[1] * _spinner_polygonizer_po_global_wg_size[2] <= limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(_spinner_polygonizer_po_global_wg_size[0] < limits_ptr->max_compute_work_group_count[0] &&
                      _spinner_polygonizer_po_global_wg_size[1] < limits_ptr->max_compute_work_group_count[1] &&
                      _spinner_polygonizer_po_global_wg_size[2] < limits_ptr->max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    ogl_shader_set_body_with_token_replacement(polygonizer_cs,
                                               system_hashed_ansi_string_create(cs_body),
                                               n_cs_body_token_values,
                                               cs_body_tokens,
                                               cs_body_values);
    ogl_program_attach_shader                 (_spinner_polygonizer_po,
                                               polygonizer_cs);

    if (!ogl_program_link(_spinner_polygonizer_po) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not link spinner polygonizer PO");

        goto end;
    }

    /* Retrieve polygonizer PO properties */
    ogl_program_get_uniform_block_by_name(_spinner_polygonizer_po,
                                          system_hashed_ansi_string_create("propsBuffer"),
                                         &_spinner_polygonizer_po_propsBuffer_ub);

    ASSERT_DEBUG_SYNC(_spinner_polygonizer_po_propsBuffer_ub != NULL,
                      "propsBuffer uniform block is not recognized by GL");

    ogl_program_ub_get_property(_spinner_polygonizer_po_propsBuffer_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_spinner_polygonizer_po_propsBuffer_ub_bo_size);
    ogl_program_ub_get_property(_spinner_polygonizer_po_propsBuffer_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &_spinner_polygonizer_po_propsBuffer_ub_bo_id);
    ogl_program_ub_get_property(_spinner_polygonizer_po_propsBuffer_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &_spinner_polygonizer_po_propsBuffer_ub_bo_start_offset);

    ogl_program_get_uniform_by_name(_spinner_polygonizer_po,
                                    system_hashed_ansi_string_create("innerRingRadius"),
                                   &cs_innerRingRadius_var_ptr);
    ogl_program_get_uniform_by_name(_spinner_polygonizer_po,
                                    system_hashed_ansi_string_create("outerRingRadius"),
                                   &cs_outerRingRadius_var_ptr);

    ASSERT_DEBUG_SYNC(cs_innerRingRadius_var_ptr != NULL &&
                      cs_outerRingRadius_var_ptr != NULL,
                      "innerRingRadius / outerRingRadius variables are not recognized by GL");

    _spinner_polygonizer_po_propsBuffer_innerRingRadius_ub_offset = cs_innerRingRadius_var_ptr->block_offset;
    _spinner_polygonizer_po_propsBuffer_outerRingRadius_ub_offset = cs_outerRingRadius_var_ptr->block_offset;

    /* Set up renderer PO */
    renderer_fs = ogl_shader_create(_context,
                                    SHADER_TYPE_FRAGMENT,
                                    system_hashed_ansi_string_create("Spinner renderer FS") );
    renderer_vs = ogl_shader_create(_context,
                                    SHADER_TYPE_VERTEX,
                                    system_hashed_ansi_string_create("Spinner renderer VS") );

    ASSERT_DEBUG_SYNC(renderer_fs != NULL &&
                      renderer_vs != NULL,
                      "Could not create spinner renderer FS and/or VS");

    ogl_shader_set_body(renderer_fs,
                        system_hashed_ansi_string_create(fs_body) );
    ogl_shader_set_body(renderer_vs,
                        system_hashed_ansi_string_create(vs_body) );

    ogl_program_attach_shader(_spinner_renderer_po,
                              renderer_fs);
    ogl_program_attach_shader(_spinner_renderer_po,
                              renderer_vs);

    if (!ogl_program_link(_spinner_renderer_po) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not link spinner renderer PO");

        goto end;
    }

    /* Set up BO to hold polygonized spinner data */
    _spinner_bo_size = SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES * 2 /* triangles */ * 3 /* vertices per triangle */ * 2 /* comp's per vertex */ * sizeof(float);

    ogl_buffers_allocate_buffer_memory(buffers,
                                       _spinner_bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_VBO,
                                       0, /* flags */
                                      &_spinner_bo_id,
                                      &_spinner_bo_start_offset);

    /* Set up VAO for the rendering process */
    entrypoints_ptr->pGLGenVertexArrays        (1,
                                               &_spinner_vao_id);
    entrypoints_ptr->pGLBindVertexArray        (_spinner_vao_id);

    entrypoints_ptr->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                _spinner_bo_id);
    entrypoints_ptr->pGLVertexAttribPointer    (0,                                         /* index */
                                                2,                                         /* size */
                                                GL_FLOAT,
                                                GL_FALSE,                                  /* normalized */
                                                sizeof(float) * 2,                         /* stride */
                                                (const GLvoid*) _spinner_bo_start_offset); /* pointer */
    entrypoints_ptr->pGLEnableVertexAttribArray(0);                                        /* index */

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

/** TODO */
PRIVATE void _render_spinner()
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Generate spinner mesh data */
    const float inner_ring_radius = 0.1f;
    const float outer_ring_radius = 2.0f;

    entrypoints_ptr->pGLUseProgram(ogl_program_get_id(_spinner_polygonizer_po) );

    ogl_program_ub_set_nonarrayed_uniform_value(_spinner_polygonizer_po_propsBuffer_ub,
                                                _spinner_polygonizer_po_propsBuffer_innerRingRadius_ub_offset,
                                               &inner_ring_radius,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(_spinner_polygonizer_po_propsBuffer_ub,
                                                _spinner_polygonizer_po_propsBuffer_outerRingRadius_ub_offset,
                                               &outer_ring_radius,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_sync                        (_spinner_polygonizer_po_propsBuffer_ub);

    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        _spinner_bo_id,
                                        _spinner_bo_start_offset,
                                        _spinner_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        _spinner_polygonizer_po_propsBuffer_ub_bo_id,
                                        _spinner_polygonizer_po_propsBuffer_ub_bo_start_offset,
                                        _spinner_polygonizer_po_propsBuffer_ub_bo_size);

    entrypoints_ptr->pGLDispatchCompute(_spinner_polygonizer_po_global_wg_size[0],
                                        _spinner_polygonizer_po_global_wg_size[1],
                                        _spinner_polygonizer_po_global_wg_size[2]);

    entrypoints_ptr->pGLFinish();

    /* Render it */
    entrypoints_ptr->pGLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_spinner_renderer_po) );
    entrypoints_ptr->pGLBindVertexArray(_spinner_vao_id);

    entrypoints_ptr->pGLDisable      (GL_CULL_FACE); /* culling is not needed - all triangles will always be visible */
    entrypoints_ptr->pGLMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    entrypoints_ptr->pGLDrawArrays   (GL_TRIANGLES,
                                      0, /* first */
                                      SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES * 2 /* triangles */);
}


/** TODO */
PRIVATE void _rendering_handler(ogl_context context,
                                uint32_t    n_frames_rendered,
                                system_time frame_time,
                                const int*  rendering_area_px_topdown,
                                void*       renderer)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    const ogl_context_gl_limits*      limits_ptr      = NULL;
    static bool                       is_initialized  = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* If this is the first frame we're rendering, initialize various stuff. */
    if (!is_initialized)
    {
        /* Initialize projection & view matrices */
        _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(DEG_TO_RAD(34),  /* fov_y */
                                                                                   float(_window_size[0]) / float(_window_size[1]),
                                                                                   0.1f,  /* z_near */
                                                                                   10.0f); /* z_far  */
        _view_matrix       = system_matrix4x4_create                              ();

        system_matrix4x4_set_to_identity(_view_matrix);

        _init_spinner();

        /* All done */
        is_initialized = true;
    } /* if (!is_initialized) */

    _render_spinner();
}

/** Event callback handlers */
PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window)
{
    _deinit_spinner();

    if (_projection_matrix != NULL)
    {
        system_matrix4x4_release(_projection_matrix);

        _projection_matrix = NULL;
    }

    if (_view_matrix != NULL)
    {
        system_matrix4x4_release(_view_matrix);

        _view_matrix = NULL;
    }

    /* All done */
    system_event_set(_window_closed_event);
}


/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    system_screen_mode    screen_mode              = NULL;
    system_window         window                   = NULL;
    system_pixel_format   window_pf                = NULL;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                           0); /* stencil_buffer_bits     */

#if 0
    window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                             screen_mode,
                                             true, /* vsync_enabled */
                                             window_pf);
#else
    _window_size[0] /= 2;
    _window_size[1] /= 2;

    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Spinner test window"),
                                                 false, /* scalable */
                                                 true,  /* vsync_enabled */
                                                 true,  /* visible */
                                                 window_pf);
#endif

    /* Set up the rendering contxt */
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,
                                                                            _rendering_handler,
                                                                            NULL); /* user_arg */

    /* Kick off the rendering process */
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    /* Set up mouse click & window tear-down callbacks */
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    /* Launch the rendering process and wait until the window is closed. */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    system_window_close (window);
    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}