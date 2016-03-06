/**
 *
 * Spinner loader test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_vao.h"
#include "postprocessing/postprocessing_motion_blur.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_time.h"
#include "system/system_variant.h"
#include "system/system_window.h"
#include "main.h"

#define SPINNER_N_SEGMENTS_PER_SPLINE (16)
#define SPINNER_N_SPLINES             (9)


PRIVATE ral_context      _context             = NULL;
PRIVATE ogl_pipeline     _pipeline            = NULL;
PRIVATE uint32_t         _pipeline_stage_id   = 0;
PRIVATE system_matrix4x4 _projection_matrix   = NULL;
PRIVATE system_event     _window_closed_event = system_event_create(true); /* manual_reset */
PRIVATE int              _window_size[2]      = {1280, 720};
PRIVATE system_matrix4x4 _view_matrix         = NULL;

PRIVATE unsigned int   _spinner_bo_size           = 0;
PRIVATE ral_buffer     _spinner_current_frame_bo  = NULL;
PRIVATE ral_buffer     _spinner_previous_frame_bo = NULL;
PRIVATE GLuint         _spinner_vao_id            = 0;

PRIVATE system_time                _spinner_first_frame_time                                     = 0;
PRIVATE postprocessing_motion_blur _spinner_motion_blur                                          = NULL;
PRIVATE ral_program                _spinner_polygonizer_po                                       = NULL;
PRIVATE unsigned int               _spinner_polygonizer_po_global_wg_size[3]                     = {0};
PRIVATE ral_program_block_buffer   _spinner_polygonizer_po_propsBuffer_ub                        = NULL;
PRIVATE unsigned int               _spinner_polygonizer_po_propsBuffer_ub_bo_ral_size            = 0;
PRIVATE GLuint                     _spinner_polygonizer_po_propsBuffer_innerRingRadius_ub_offset = -1;
PRIVATE GLuint                     _spinner_polygonizer_po_propsBuffer_nRingsToSkip_ub_offset    = -1;
PRIVATE GLuint                     _spinner_polygonizer_po_propsBuffer_outerRingRadius_ub_offset = -1;
PRIVATE GLuint                     _spinner_polygonizer_po_propsBuffer_splineOffsets_ub_offset   = -1;
PRIVATE ral_program                _spinner_renderer_po                                          = NULL;

PRIVATE ral_framebuffer _spinner_blit_fbo         = NULL;
PRIVATE ral_texture     _spinner_color_to         = NULL;
PRIVATE ral_texture     _spinner_color_to_blurred = NULL;
PRIVATE ral_framebuffer _spinner_render_fbo       = NULL;
PRIVATE ral_texture     _spinner_velocity_to      = NULL;


/* Forward declarations */
PRIVATE void _deinit_spinner                 ();
PRIVATE void _init_spinner                   ();
PRIVATE void _render_spinner                 (unsigned int            n_frames_rendered);
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
PRIVATE void _window_closed_callback_handler (system_window           window,
                                              void*                   unused);
PRIVATE void _window_closing_callback_handler(system_window           window,
                                              void*                   unused);


/** TODO */
PRIVATE void _deinit_spinner()
{
    ogl_context                 context_gl      = ral_context_get_gl_context(_context);
    ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* BOs */
    const ral_buffer bos[] =
    {
        _spinner_current_frame_bo,
        _spinner_previous_frame_bo
    };
    const ral_framebuffer fbos[] =
    {
        _spinner_blit_fbo,
        _spinner_render_fbo
    };
    const ral_program pos[] =
    {
        _spinner_polygonizer_po,
        _spinner_renderer_po
    };
    const ral_texture tos[] =
    {
        _spinner_color_to,
        _spinner_color_to_blurred,
        _spinner_velocity_to
    };

    const uint32_t n_bos  = sizeof(bos)  / sizeof(bos[0]);
    const uint32_t n_fbos = sizeof(fbos) / sizeof(fbos[0]);
    const uint32_t n_pos  = sizeof(pos)  / sizeof(pos[0]);
    const uint32_t n_tos  = sizeof(tos)  / sizeof(tos[0]);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                               n_bos,
                               (const void**) bos);
    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                               n_fbos,
                               (const void**) fbos);
    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               n_pos,
                               (const void**) pos);
    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_tos,
                               (const void**) tos);

    _spinner_bo_size = 0;

    /* VAOs */
    if (_spinner_vao_id != 0)
    {
        entrypoints_ptr->pGLDeleteVertexArrays(1,
                                              &_spinner_vao_id);

        _spinner_vao_id = 0;
    }

    /* Post-processors */
    if (_spinner_motion_blur != NULL)
    {
        postprocessing_motion_blur_release(_spinner_motion_blur);

        _spinner_motion_blur = NULL;
    }

    if (_spinner_polygonizer_po_propsBuffer_ub != NULL)
    {
        ral_program_block_buffer_release(_spinner_polygonizer_po_propsBuffer_ub);

        _spinner_polygonizer_po_propsBuffer_ub = NULL;
    }
}

/** TODO */
PRIVATE void _init_spinner()
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
                          "    vertex_v1v2 = vec4(spline_radius)       * base_location;\n"
                          "    vertex_v3v4 = vec4(spline_radius - 0.1) * base_location;\n"
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
                          //"    resultColor = vec4(vec3(shade), 1.0);\n"
                          "    resultColor = vec4( float((vertex_id * 36 / 16 / 9 / 4) % 255) / 255.0, float((vertex_id * 153 / 16 / 9 / 4) % 255) / 255.0, 0.0, 1.0);\n"
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
    const ral_program_variable*     cs_innerRingRadius_ral_ptr = NULL;
    const ral_program_variable*     cs_nRingsToSkip_ral_ptr    = NULL;
    const ral_program_variable*     cs_outerRingRadius_ral_ptr = NULL;
    const ral_program_variable*     cs_splineOffsets_ral_ptr   = NULL;
    ral_buffer_create_info          frame_bo_create_info;
    const unsigned int              n_cs_body_token_values     = sizeof(cs_body_tokens) / sizeof(cs_body_tokens[0]);
    const unsigned int              n_vs_body_token_values     = sizeof(vs_body_tokens) / sizeof(vs_body_tokens[0]);
    char                            temp[1024];

    ogl_context                                               context_gl          = ral_context_get_gl_context(_context);
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    const ogl_context_gl_limits*                              limits_ptr          = NULL;
    ral_shader                                                polygonizer_cs      = NULL;

    const ral_shader_create_info renderer_so_create_info_items[] =
    {
        {
            system_hashed_ansi_string_create("Spinner renderer FS"),
            RAL_SHADER_TYPE_FRAGMENT
        },
        {
            system_hashed_ansi_string_create("Spinner renderer VS"),
            RAL_SHADER_TYPE_VERTEX
        }
    };
    const uint32_t n_renderer_so_create_info_items = sizeof(renderer_so_create_info_items) / sizeof(renderer_so_create_info_items[0]);
    ral_shader     renderer_sos[n_renderer_so_create_info_items];

    const ral_shader_create_info polygonizer_cs_create_info =
    {
        system_hashed_ansi_string_create("Spinner polygonizer CS"),
        RAL_SHADER_TYPE_COMPUTE
    };

    const ral_program_create_info po_create_info_items[] =
    {
        {
            RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
            system_hashed_ansi_string_create("Spinner polygonizer PO")
        },

        {
            RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
            system_hashed_ansi_string_create("Spinner renderer PO"),
        }
    };
    const uint32_t n_po_create_info_items = sizeof(po_create_info_items) / sizeof(po_create_info_items[0]);
    ral_program    result_pos[n_po_create_info_items];


    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    ral_context_create_programs(_context,
                                n_po_create_info_items,
                                po_create_info_items,
                                result_pos);

    _spinner_polygonizer_po = result_pos[0];
    _spinner_renderer_po    = result_pos[1];

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
    ral_context_create_shaders(_context,
                               1, /* n_create_info_items */
                              &polygonizer_cs_create_info,
                              &polygonizer_cs);

    if (polygonizer_cs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create spinner polygonizer CS");

        goto end;
    }

    _spinner_polygonizer_po_global_wg_size[0] = 1 + (2 /* top half, bottom half */ * SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES / limits_ptr->max_compute_work_group_size[0]);
    _spinner_polygonizer_po_global_wg_size[1] = 1;
    _spinner_polygonizer_po_global_wg_size[2] = 1;

    ASSERT_DEBUG_SYNC(_spinner_polygonizer_po_global_wg_size[0] * _spinner_polygonizer_po_global_wg_size[1] * _spinner_polygonizer_po_global_wg_size[2] <= (unsigned int) limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(_spinner_polygonizer_po_global_wg_size[0] < (unsigned int) limits_ptr->max_compute_work_group_count[0] &&
                      _spinner_polygonizer_po_global_wg_size[1] < (unsigned int) limits_ptr->max_compute_work_group_count[1] &&
                      _spinner_polygonizer_po_global_wg_size[2] < (unsigned int) limits_ptr->max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    const system_hashed_ansi_string polygonizer_cs_has = system_hashed_ansi_string_create_by_token_replacement(cs_body,
                                                                                                               n_cs_body_token_values,
                                                                                                               cs_body_tokens,
                                                                                                               cs_body_values);

    ral_shader_set_property(polygonizer_cs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &polygonizer_cs_has);

    ral_program_attach_shader(_spinner_polygonizer_po,
                              polygonizer_cs);

    /* Retrieve polygonizer PO properties */
    ral_buffer         props_buffer_bo_ral         = NULL;
    const raGL_program spinner_polygonizer_po_raGL = ral_context_get_program_gl(_context,
                                                                                _spinner_polygonizer_po);

    _spinner_polygonizer_po_propsBuffer_ub = ral_program_block_buffer_create(_context,
                                                                             _spinner_polygonizer_po,
                                                                             system_hashed_ansi_string_create("propsBuffer") );

    ral_program_block_buffer_get_property(_spinner_polygonizer_po_propsBuffer_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &props_buffer_bo_ral);
    ral_buffer_get_property              (props_buffer_bo_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &_spinner_polygonizer_po_propsBuffer_ub_bo_ral_size);

    ral_program_get_block_variable_by_name(_spinner_polygonizer_po,
                                           system_hashed_ansi_string_create("propsBuffer"),
                                           system_hashed_ansi_string_create("innerRingRadius"),
                                          &cs_innerRingRadius_ral_ptr);
    ral_program_get_block_variable_by_name(_spinner_polygonizer_po,
                                           system_hashed_ansi_string_create("propsBuffer"),
                                           system_hashed_ansi_string_create("nRingsToSkip"),
                                          &cs_nRingsToSkip_ral_ptr);
    ral_program_get_block_variable_by_name(_spinner_polygonizer_po,
                                           system_hashed_ansi_string_create("propsBuffer"),
                                           system_hashed_ansi_string_create("outerRingRadius"),
                                          &cs_outerRingRadius_ral_ptr);
    ral_program_get_block_variable_by_name(_spinner_polygonizer_po,
                                           system_hashed_ansi_string_create("propsBuffer"),
                                           system_hashed_ansi_string_create("splineOffsets[0]"),
                                          &cs_splineOffsets_ral_ptr);

    ASSERT_DEBUG_SYNC(cs_innerRingRadius_ral_ptr != NULL &&
                      cs_nRingsToSkip_ral_ptr    != NULL &&
                      cs_outerRingRadius_ral_ptr != NULL &&
                      cs_splineOffsets_ral_ptr     != NULL,
                      "Spinner props UB variables are not recognized by GL");

    _spinner_polygonizer_po_propsBuffer_innerRingRadius_ub_offset = cs_innerRingRadius_ral_ptr->block_offset;
    _spinner_polygonizer_po_propsBuffer_nRingsToSkip_ub_offset    = cs_nRingsToSkip_ral_ptr->block_offset;
    _spinner_polygonizer_po_propsBuffer_outerRingRadius_ub_offset = cs_outerRingRadius_ral_ptr->block_offset;
    _spinner_polygonizer_po_propsBuffer_splineOffsets_ub_offset   = cs_splineOffsets_ral_ptr->block_offset;

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
    ral_context_create_shaders(_context,
                               n_renderer_so_create_info_items,
                               renderer_so_create_info_items,
                               renderer_sos);

    ASSERT_DEBUG_SYNC(renderer_sos[0] != NULL &&
                      renderer_sos[1] != NULL,
                      "Could not create spinner renderer FS and/or VS");

    const system_hashed_ansi_string renderer_fs_has = system_hashed_ansi_string_create                     (fs_body);
    const system_hashed_ansi_string renderer_vs_has = system_hashed_ansi_string_create_by_token_replacement(vs_body,
                                                                                                            n_vs_body_token_values,
                                                                                                            vs_body_tokens,
                                                                                                            vs_body_values);

    ral_shader_set_property(renderer_sos[0],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &renderer_fs_has);
    ral_shader_set_property(renderer_sos[1],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &renderer_vs_has);

    ral_program_attach_shader(_spinner_renderer_po,
                              renderer_sos[0]);
    ral_program_attach_shader(_spinner_renderer_po,
                              renderer_sos[1]);

    /* Set up BO to hold polygonized spinner data */
    _spinner_bo_size = SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES * 2 /* triangles */ * 3 /* vertices per triangle */ * 4 /* comp's per vertex */ * sizeof(float) * 2 /* top half, bottom half */;

    frame_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    frame_bo_create_info.property_bits    = 0;
    frame_bo_create_info.size             = _spinner_bo_size;
    frame_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT | RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    frame_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(_context,
                               1, /* n_buffers */
                               &frame_bo_create_info,
                              &_spinner_current_frame_bo);
    ral_context_create_buffers(_context,
                               1, /* n_buffers */
                               &frame_bo_create_info,
                              &_spinner_previous_frame_bo);

    /* Set up VAO for the rendering process */
    struct _vao
    {
        ral_buffer prev_frame_bo;
        ral_buffer this_frame_bo;

        GLuint* vao_id_ptr;
    } vaos[] =
    {
        {
            _spinner_previous_frame_bo,
            _spinner_current_frame_bo,
           &_spinner_vao_id
        },
    };
    const unsigned int n_vaos = sizeof(vaos) / sizeof(vaos[0]);

    ral_texture_create_info color_to_create_info;
    ral_texture_create_info color_to_blurred_create_info;
    ral_texture             result_tos[3];
    ral_texture_create_info velocity_to_create_info;
    ral_texture_create_info to_create_info_items[3];

    for (unsigned int n_vao = 0;
                      n_vao < n_vaos;
                    ++n_vao)
    {
        _vao&       current_vao                     = vaos[n_vao];
        raGL_buffer prev_frame_bo_raGL              = NULL;
        GLuint      prev_frame_bo_raGL_id           = 0;
        uint32_t    prev_frame_bo_raGL_start_offset = -1;
        uint32_t    prev_frame_bo_ral_start_offset  = -1;
        raGL_buffer this_frame_bo_raGL              =  NULL;
        GLuint      this_frame_bo_raGL_id           = 0;
        uint32_t    this_frame_bo_raGL_start_offset = -1;
        uint32_t    this_frame_bo_ral_start_offset  = -1;

        prev_frame_bo_raGL = ral_context_get_buffer_gl(_context,
                                                       current_vao.prev_frame_bo);
        this_frame_bo_raGL = ral_context_get_buffer_gl(_context,
                                                       current_vao.this_frame_bo);

        ral_buffer_get_property(current_vao.prev_frame_bo,
                                RAL_BUFFER_PROPERTY_START_OFFSET,
                               &prev_frame_bo_ral_start_offset);
        ral_buffer_get_property(current_vao.this_frame_bo,
                                RAL_BUFFER_PROPERTY_START_OFFSET,
                               &this_frame_bo_ral_start_offset);

        raGL_buffer_get_property(prev_frame_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &prev_frame_bo_raGL_id);
        raGL_buffer_get_property(prev_frame_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &prev_frame_bo_raGL_start_offset);
        raGL_buffer_get_property(this_frame_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &this_frame_bo_raGL_id);
        raGL_buffer_get_property(this_frame_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &this_frame_bo_raGL_start_offset);

        entrypoints_ptr->pGLGenVertexArrays        (1,
                                                    current_vao.vao_id_ptr);
        entrypoints_ptr->pGLBindVertexArray        (*current_vao.vao_id_ptr);

        entrypoints_ptr->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                    this_frame_bo_raGL_id);
        entrypoints_ptr->pGLVertexAttribPointer    (0,                  /* index */
                                                    4,                  /* size */
                                                    GL_FLOAT,
                                                    GL_FALSE,          /* normalized */
                                                    sizeof(float) * 4, /* stride */
                                                    (const GLvoid*) (this_frame_bo_raGL_start_offset + this_frame_bo_ral_start_offset) ); /* pointer */

        entrypoints_ptr->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                                    prev_frame_bo_raGL_id);
        entrypoints_ptr->pGLVertexAttribPointer    (1,                  /* index */
                                                    4,                  /* size */
                                                    GL_FLOAT,
                                                    GL_FALSE,          /* normalized */
                                                    sizeof(float) * 4, /* stride */
                                                    (const GLvoid*) (prev_frame_bo_raGL_start_offset + prev_frame_bo_ral_start_offset) ); /* pointer */

        entrypoints_ptr->pGLEnableVertexAttribArray(0); /* index */
        entrypoints_ptr->pGLEnableVertexAttribArray(1); /* index */
    } /* for (all VAOs to set up) */

    /* Set up the TO to hold pre-processed color & velocity data */
    color_to_create_info.base_mipmap_depth      = 1;
    color_to_create_info.base_mipmap_height     = _window_size[1];
    color_to_create_info.base_mipmap_width      = _window_size[0];
    color_to_create_info.fixed_sample_locations = true;
    color_to_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA8_UNORM;
    color_to_create_info.name                   = system_hashed_ansi_string_create("Color TO");
    color_to_create_info.n_layers               = 1;
    color_to_create_info.n_samples              = 1;
    color_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_to_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT;

    color_to_blurred_create_info.base_mipmap_depth      = 1;
    color_to_blurred_create_info.base_mipmap_height     = _window_size[1];
    color_to_blurred_create_info.base_mipmap_width      = _window_size[0];
    color_to_blurred_create_info.fixed_sample_locations = true;
    color_to_blurred_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA8_UNORM;
    color_to_blurred_create_info.name                   = system_hashed_ansi_string_create("Post-processed color TO");
    color_to_blurred_create_info.n_layers               = 1;
    color_to_blurred_create_info.n_samples              = 1;
    color_to_blurred_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_to_blurred_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT;

    velocity_to_create_info.base_mipmap_depth      = 1;
    velocity_to_create_info.base_mipmap_height     = _window_size[1];
    velocity_to_create_info.base_mipmap_width      = _window_size[0];
    velocity_to_create_info.fixed_sample_locations = true;
    velocity_to_create_info.format                 = RAL_TEXTURE_FORMAT_RG16_FLOAT;
    velocity_to_create_info.name                   = system_hashed_ansi_string_create("Velocity TO");
    velocity_to_create_info.n_layers               = 1;
    velocity_to_create_info.n_samples              = 1;
    velocity_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    velocity_to_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT;

    to_create_info_items[0] = color_to_create_info;
    to_create_info_items[1] = color_to_blurred_create_info;
    to_create_info_items[2] = velocity_to_create_info;

    ral_context_create_textures(_context,
                                sizeof(to_create_info_items) / sizeof(to_create_info_items[0]),
                                to_create_info_items,
                                result_tos);

    _spinner_color_to         = result_tos[0];
    _spinner_color_to_blurred = result_tos[1];
    _spinner_velocity_to      = result_tos[2];


    ral_context_create_framebuffers  (_context,
                                      1, /* n_framebuffers */
                                     &_spinner_render_fbo);
    ral_framebuffer_set_attachment_2D(_spinner_render_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      _spinner_color_to,
                                      0 /* n_mipmap */);
    ral_framebuffer_set_attachment_2D(_spinner_render_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      1, /* index */
                                      _spinner_velocity_to,
                                      0 /* n_mipmap */);

    ral_context_create_framebuffers  (_context,
                                      1, /* n_framebuffers */
                                     &_spinner_blit_fbo);
    ral_framebuffer_set_attachment_2D(_spinner_blit_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      _spinner_color_to_blurred,
                                      0 /* n_mipmap */);

    /* Init motion blur post-processor */
    _spinner_motion_blur = postprocessing_motion_blur_create(_context,
                                                             POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RGBA8,
                                                             POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG32F,
                                                             POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D,
                                                             system_hashed_ansi_string_create("Spinner motion blur") );

    /* All done */
end:
    ral_shader shaders_to_release[] =
    {
        polygonizer_cs,
        renderer_sos[0],
        renderer_sos[1]
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}

/** TODO */
PRIVATE void _render_spinner(unsigned int n_frames_rendered)
{
    ogl_context                       context_gl      = ral_context_get_gl_context(_context);
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (_spinner_first_frame_time == -1)
    {
        _spinner_first_frame_time = system_time_now();
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

    entrypoints_ptr->pGLViewport(new_viewport[0],
                                 new_viewport[1],
                                 new_viewport[2] - new_viewport[0],
                                 new_viewport[3] - new_viewport[1]);

    /* Update the spline offsets.
     *
     * NOTE: The same offset should be used for top & bottom half. Otherwise the segments
     *       will start overlapping at some point.
     */
    uint32_t    animation_time_msec = 0;
    system_time current_time        = system_time_now();

    system_time_get_msec_for_time(current_time - _spinner_first_frame_time,
                                 &animation_time_msec);

    static bool  spinned = false;
    static float spline_accelerations[SPINNER_N_SPLINES];
    static float spline_offsets      [SPINNER_N_SPLINES];
    static float spline_speeds       [SPINNER_N_SPLINES];

    if (n_frames_rendered >= 60 && n_frames_rendered <= 65)
    {
        memset(spline_offsets,
               0,
               sizeof(spline_offsets) );
        memset(spline_speeds,
               0,
               sizeof(spline_speeds) );

        float t = float(n_frames_rendered - 60) / 6.0f;

        for (uint32_t n_spline = 0;
                      n_spline < SPINNER_N_SPLINES;
                    ++n_spline)
        {
            float    coeff        = t * t * (3.0f - 2.0f * t);
            uint32_t spline_speed = ( (n_spline + 1) * 1537) % 7;

            spline_accelerations[n_spline] += coeff * (float(spline_speed) / 90.0f /* 80 is also ok */) * 3.14152965f;
        }

        spinned = true;
    }

    for (uint32_t n_spline = 0;
                  n_spline < SPINNER_N_SPLINES;
                ++n_spline)
    {
        spline_offsets[n_spline] += spline_accelerations[n_spline];

        if (spline_accelerations[n_spline] > 0.5f)
        {
            spline_accelerations[n_spline] -= 0.001f;
        }
        else
        {
            spline_accelerations[n_spline] *= 0.95f;
        }

        if (spline_accelerations[n_spline] < 0.0f)
        {
            spline_accelerations[n_spline] = 0.0f;
        }

        ral_program_block_buffer_set_arrayed_variable_value(_spinner_polygonizer_po_propsBuffer_ub,
                                                            _spinner_polygonizer_po_propsBuffer_splineOffsets_ub_offset,
                                                            spline_offsets + n_spline,
                                                            sizeof(float),
                                                            n_spline,
                                                            1);           /* dst_array_item_count */
    }

    /* Generate spinner mesh data */
    const float        inner_ring_radius = 0.1f;
    const unsigned int n_rings_to_skip   = 1;
    const float        outer_ring_radius = 1.0f;
    raGL_buffer        spinner_current_frame_bo_raGL                              = NULL;
    GLuint             spinner_current_frame_bo_raGL_id                           = 0;
    uint32_t           spinner_current_frame_bo_raGL_start_offset                 = -1;
    uint32_t           spinner_current_frame_bo_ral_start_offset                  = -1;
    raGL_buffer        spinner_polygonizer_po_propsBuffer_ub_bo_raGL              = NULL;
    GLuint             spinner_polygonizer_po_propsBuffer_ub_bo_raGL_id           = 0;
    uint32_t           spinner_polygonizer_po_propsBuffer_ub_bo_raGL_start_offset = -1;
    ral_buffer         spinner_polygonizer_po_propsBuffer_ub_bo_ral               = NULL;
    uint32_t           spinner_polygonizer_po_propsBuffer_ub_bo_ral_start_offset  = -1;
    raGL_buffer        spinner_previous_frame_bo_raGL                             = NULL;
    GLuint             spinner_previous_frame_bo_raGL_id                          = 0;
    uint32_t           spinner_previous_frame_bo_raGL_start_offset                = -1;
    GLuint             spinner_previous_frame_bo_ral_start_offset                 = -1;

    ral_program_block_buffer_get_property(_spinner_polygonizer_po_propsBuffer_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &spinner_polygonizer_po_propsBuffer_ub_bo_ral);

    spinner_current_frame_bo_raGL                 = ral_context_get_buffer_gl(_context,
                                                                              _spinner_current_frame_bo);
    spinner_polygonizer_po_propsBuffer_ub_bo_raGL = ral_context_get_buffer_gl(_context,
                                                                              spinner_polygonizer_po_propsBuffer_ub_bo_ral);
    spinner_previous_frame_bo_raGL                = ral_context_get_buffer_gl(_context,
                                                                              _spinner_previous_frame_bo);

    raGL_buffer_get_property(spinner_current_frame_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                             &spinner_current_frame_bo_raGL_id);
    raGL_buffer_get_property(spinner_current_frame_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                             &spinner_current_frame_bo_raGL_start_offset);
    ral_buffer_get_property (_spinner_current_frame_bo,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &spinner_current_frame_bo_ral_start_offset);

    raGL_buffer_get_property(spinner_polygonizer_po_propsBuffer_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                             &spinner_polygonizer_po_propsBuffer_ub_bo_raGL_id);
    raGL_buffer_get_property(spinner_polygonizer_po_propsBuffer_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                             &spinner_polygonizer_po_propsBuffer_ub_bo_raGL_start_offset);
    ral_buffer_get_property (spinner_polygonizer_po_propsBuffer_ub_bo_ral,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &spinner_polygonizer_po_propsBuffer_ub_bo_ral_start_offset);

    raGL_buffer_get_property(spinner_previous_frame_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                             &spinner_previous_frame_bo_raGL_id);
    raGL_buffer_get_property(spinner_previous_frame_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                             &spinner_previous_frame_bo_raGL_start_offset);
    ral_buffer_get_property (_spinner_previous_frame_bo,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                             &spinner_previous_frame_bo_ral_start_offset);

    const raGL_program spinner_polygonizer_po_raGL    = ral_context_get_program_gl(_context,
                                                                                   _spinner_polygonizer_po);
    GLuint             spinner_polygonizer_po_raGL_id = 0;

    raGL_program_get_property(spinner_polygonizer_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &spinner_polygonizer_po_raGL_id);

    entrypoints_ptr->pGLUseProgram(spinner_polygonizer_po_raGL_id);

    ral_program_block_buffer_set_nonarrayed_variable_value(_spinner_polygonizer_po_propsBuffer_ub,
                                                           _spinner_polygonizer_po_propsBuffer_innerRingRadius_ub_offset,
                                                          &inner_ring_radius,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_spinner_polygonizer_po_propsBuffer_ub,
                                                           _spinner_polygonizer_po_propsBuffer_nRingsToSkip_ub_offset,
                                                          &n_rings_to_skip,
                                                           sizeof(int) );
    ral_program_block_buffer_set_nonarrayed_variable_value(_spinner_polygonizer_po_propsBuffer_ub,
                                                           _spinner_polygonizer_po_propsBuffer_outerRingRadius_ub_offset,
                                                          &outer_ring_radius,
                                                           sizeof(float) );
    ral_program_block_buffer_sync                         (_spinner_polygonizer_po_propsBuffer_ub);

    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        spinner_current_frame_bo_raGL_id,
                                        spinner_current_frame_bo_raGL_start_offset + spinner_current_frame_bo_ral_start_offset,
                                        _spinner_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        spinner_polygonizer_po_propsBuffer_ub_bo_raGL_id,
                                        spinner_polygonizer_po_propsBuffer_ub_bo_raGL_start_offset + spinner_polygonizer_po_propsBuffer_ub_bo_ral_start_offset,
                                        _spinner_polygonizer_po_propsBuffer_ub_bo_ral_size);

    entrypoints_ptr->pGLDispatchCompute(_spinner_polygonizer_po_global_wg_size[0],
                                        _spinner_polygonizer_po_global_wg_size[1],
                                        _spinner_polygonizer_po_global_wg_size[2]);

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
    static const unsigned int n_render_draw_buffers      = sizeof(render_draw_buffers) / sizeof(render_draw_buffers[0]);
    raGL_framebuffer          spinner_render_fbo_raGL    = ral_context_get_framebuffer_gl(_context,
                                                                                          _spinner_render_fbo);
    GLuint                    spinner_render_fbo_raGL_id = 0;

    raGL_framebuffer_get_property(spinner_render_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &spinner_render_fbo_raGL_id);

    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        spinner_render_fbo_raGL_id);

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
    const raGL_program spinner_renderer_po_raGL    = ral_context_get_program_gl(_context,
                                                                                _spinner_renderer_po);
    GLuint             spinner_renderer_po_raGL_id = 0;

    raGL_program_get_property(spinner_renderer_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &spinner_renderer_po_raGL_id);

    entrypoints_ptr->pGLUseProgram     (spinner_renderer_po_raGL_id);
    entrypoints_ptr->pGLBindVertexArray(_spinner_vao_id);

    entrypoints_ptr->pGLDisable      (GL_CULL_FACE);  /* culling is not needed - all triangles are always visible */
    entrypoints_ptr->pGLDisable      (GL_DEPTH_TEST); /* depth test not needed - all triangles are always visible */
    entrypoints_ptr->pGLMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    entrypoints_ptr->pGLDrawArrays   (GL_TRIANGLES,
                                      0, /* first */
                                      SPINNER_N_SEGMENTS_PER_SPLINE * SPINNER_N_SPLINES * 2 /* triangles */ * 2 /* top half, bottom half */ * 3 /* vertices per triangle */);

    /* Cache the contents of the data buffer for subsequent frame */
    entrypoints_ptr->pGLBindBuffer   (GL_COPY_READ_BUFFER,
                                      spinner_current_frame_bo_raGL_id);
    entrypoints_ptr->pGLBindBuffer   (GL_COPY_WRITE_BUFFER,
                                      spinner_previous_frame_bo_raGL_id);

    entrypoints_ptr->pGLMemoryBarrier    (GL_BUFFER_UPDATE_BARRIER_BIT);
    entrypoints_ptr->pGLCopyBufferSubData(GL_COPY_READ_BUFFER,
                                          GL_COPY_WRITE_BUFFER,
                                          spinner_current_frame_bo_raGL_start_offset  + spinner_current_frame_bo_ral_start_offset,  /* readOffset  */
                                          spinner_previous_frame_bo_raGL_start_offset + spinner_previous_frame_bo_ral_start_offset, /* writeOffset */
                                          _spinner_bo_size);

    /* Apply motion blur to the rendered contents */
    postprocessing_motion_blur_execute(_spinner_motion_blur,
                                       _spinner_color_to,
                                       _spinner_velocity_to,
                                       _spinner_color_to_blurred);

    /* Blit the contents to the default FBO */
    ral_framebuffer  default_fbo              = NULL;
    raGL_framebuffer default_fbo_raGL         = NULL;
    unsigned int     default_fbo_raGL_id      = 0;
    raGL_framebuffer spinner_blit_fbo_raGL    = ral_context_get_framebuffer_gl(_context,
                                                                               _spinner_blit_fbo);
    GLuint           spinner_blit_fbo_raGL_id = 0;

    ral_context_get_property(_context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS,
                            &default_fbo);

    default_fbo_raGL = ral_context_get_framebuffer_gl(_context,
                                                      default_fbo);

    raGL_framebuffer_get_property(default_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &default_fbo_raGL_id);
    raGL_framebuffer_get_property(spinner_blit_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &spinner_blit_fbo_raGL_id);

    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        default_fbo_raGL_id);
    entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                        spinner_blit_fbo_raGL_id);

    entrypoints_ptr->pGLMemoryBarrier  (GL_FRAMEBUFFER_BARRIER_BIT);
    entrypoints_ptr->pGLBlitFramebuffer(0,               /* srcX0 */
                                        0,               /* srcY0 */
                                        _window_size[0], /* srcX1 */
                                        _window_size[1], /* srcY1 */
                                        0,               /* dstX0 */
                                        0,               /* dstY0 */
                                        _window_size[0], /* dstX1 */
                                        _window_size[1], /* dstY1 */
                                        GL_COLOR_BUFFER_BIT,
                                        GL_NEAREST);

    /* Restore the viewport */
    entrypoints_ptr->pGLViewport(precall_viewport[0],
                                 precall_viewport[1],
                                 precall_viewport[2],
                                 precall_viewport[3]);
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

    _render_spinner(n_frames_rendered);
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

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
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
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc = _rendering_handler;
    ogl_rendering_handler                   rendering_handler = NULL;
    demo_window                             window            = NULL;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name       = system_hashed_ansi_string_create("Spinner test app");

    window_create_info.resolution[0] = _window_size[0];
    window_create_info.resolution[1] = _window_size[1];

    window = demo_app_create_window(window_name,
                                    window_create_info,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    /* Set up mouse click & window tear-down callbacks */
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_rbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _window_closing_callback_handler,
                                  NULL);

    /* Launch the rendering process and wait until the window is closed. */
    demo_window_start_rendering(window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    demo_app_destroy_window(window_name);
    system_event_release   (_window_closed_event);

    main_force_deinit();

    return 0;
}