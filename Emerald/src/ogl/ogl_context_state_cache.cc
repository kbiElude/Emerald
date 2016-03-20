/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_vaos.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_vao.h"
#include "raGL/raGL_backend.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"

/** TODO */
typedef struct _ogl_context_state_cache
{
    GLfloat active_clear_color_context[4];
    GLfloat active_clear_color_local  [4];

    GLdouble active_clear_depth_context;
    GLdouble active_clear_depth_local;

    GLboolean active_color_mask_context[4];
    GLboolean active_color_mask_local  [4];

    GLenum active_cull_face_context;
    GLenum active_cull_face_local;

    GLenum active_depth_func_context;
    GLenum active_depth_func_local;

    GLboolean active_depth_mask_context;
    GLboolean active_depth_mask_local;

    GLuint active_draw_fbo_context;
    GLuint active_draw_fbo_local;

    GLenum active_front_face_context;
    GLenum active_front_face_local;

    GLuint active_program_context;
    GLuint active_program_local;

    GLuint active_read_fbo_context;
    GLuint active_read_fbo_local;

    GLuint active_rbo_context;
    GLuint active_rbo_local;

    GLint active_scissor_box_context[4];
    GLint active_scissor_box_local  [4];

    GLuint active_texture_unit_context;
    GLuint active_texture_unit_local;

    GLuint active_vertex_array_object_context;
    GLuint active_vertex_array_object_local;

    GLint  active_viewport_context[4];
    GLint  active_viewport_local  [4];

    /* Blending */
    GLfloat blend_color_context[4];
    GLfloat blend_color_local  [4];
    GLenum  blend_equation_alpha_context;
    GLenum  blend_equation_alpha_local;
    GLenum  blend_equation_rgb_context;
    GLenum  blend_equation_rgb_local;
    GLenum  blend_function_dst_alpha_context;
    GLenum  blend_function_dst_alpha_local;
    GLenum  blend_function_dst_rgb_context;
    GLenum  blend_function_dst_rgb_local;
    GLenum  blend_function_src_alpha_context;
    GLenum  blend_function_src_alpha_local;
    GLenum  blend_function_src_rgb_context;
    GLenum  blend_function_src_rgb_local;

    /* Rendering mode states.
     *
     * For simplicity, we currently ignore indexed modes (eg. GL_CLIP_DISTANCEn) */
    GLboolean active_rendering_mode_blend_context;
    GLboolean active_rendering_mode_blend_local;
    GLboolean active_rendering_mode_color_logic_op_context;
    GLboolean active_rendering_mode_color_logic_op_local;
    GLboolean active_rendering_mode_cull_face_context;
    GLboolean active_rendering_mode_cull_face_local;
    GLboolean active_rendering_mode_depth_clamp_context;
    GLboolean active_rendering_mode_depth_clamp_local;
    GLboolean active_rendering_mode_depth_test_context;
    GLboolean active_rendering_mode_depth_test_local;
    GLboolean active_rendering_mode_dither_context;
    GLboolean active_rendering_mode_dither_local;
    GLboolean active_rendering_mode_framebuffer_srgb_context;
    GLboolean active_rendering_mode_framebuffer_srgb_local;
    GLboolean active_rendering_mode_line_smooth_context;
    GLboolean active_rendering_mode_line_smooth_local;
    GLboolean active_rendering_mode_multisample_context;
    GLboolean active_rendering_mode_multisample_local;
    GLboolean active_rendering_mode_polygon_offset_fill_context;
    GLboolean active_rendering_mode_polygon_offset_fill_local;
    GLboolean active_rendering_mode_polygon_offset_line_context;
    GLboolean active_rendering_mode_polygon_offset_line_local;
    GLboolean active_rendering_mode_polygon_smooth_context;
    GLboolean active_rendering_mode_polygon_smooth_local;
    GLboolean active_rendering_mode_primitive_restart_context;
    GLboolean active_rendering_mode_primitive_restart_local;
    GLboolean active_rendering_mode_primitive_restart_fixed_index_context;
    GLboolean active_rendering_mode_primitive_restart_fixed_index_local;
    GLboolean active_rendering_mode_program_point_size_context;
    GLboolean active_rendering_mode_program_point_size_local;
    GLboolean active_rendering_mode_rasterizer_discard_context;
    GLboolean active_rendering_mode_rasterizer_discard_local;
    GLboolean active_rendering_mode_sample_alpha_to_coverage_context;
    GLboolean active_rendering_mode_sample_alpha_to_coverage_local;
    GLboolean active_rendering_mode_sample_alpha_to_one_context;
    GLboolean active_rendering_mode_sample_alpha_to_one_local;
    GLboolean active_rendering_mode_sample_coverage_context;
    GLboolean active_rendering_mode_sample_coverage_local;
    GLboolean active_rendering_mode_sample_shading_context;
    GLboolean active_rendering_mode_sample_shading_local;
    GLboolean active_rendering_mode_sample_mask_context;
    GLboolean active_rendering_mode_sample_mask_local;
    GLboolean active_rendering_mode_scissor_test_context;
    GLboolean active_rendering_mode_scissor_test_local;
    GLboolean active_rendering_mode_stencil_test_context;
    GLboolean active_rendering_mode_stencil_test_local;
    GLboolean active_rendering_mode_texture_cube_map_seamless_context;
    GLboolean active_rendering_mode_texture_cube_map_seamless_local;

    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;
} _ogl_context_state_cache;


/** Please see header for spec */
PUBLIC ogl_context_state_cache ogl_context_state_cache_create(ogl_context context)
{
    _ogl_context_state_cache* new_cache = new (std::nothrow) _ogl_context_state_cache;

    ASSERT_ALWAYS_SYNC(new_cache != NULL,
                       "Out of memory");

    if (new_cache != NULL)
    {
        new_cache->context = context;
    } /* if (new_bindings != NULL) */

    return (ogl_context_state_cache) new_cache;
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_get_property(const ogl_context_state_cache    cache,
                                                 ogl_context_state_cache_property property,
                                                 void*                            out_result)
{
    const _ogl_context_state_cache* cache_ptr = (const _ogl_context_state_cache*) cache;

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR:
        {
            memcpy(out_result,
                   cache_ptr->blend_color_local,
                   sizeof(cache_ptr->blend_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            *(GLenum*) out_result = cache_ptr->blend_equation_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB:
        {
            *(GLenum*) out_result = cache_ptr->blend_equation_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_dst_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_dst_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_src_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_src_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK:
        {
            memcpy(out_result,
                   cache_ptr->active_color_mask_local,
                   sizeof(cache_ptr->active_color_mask_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE:
        {
            *(GLenum*) out_result = cache_ptr->active_cull_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC:
        {
            *(GLenum*) out_result = cache_ptr->active_depth_func_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK:
        {
            *(GLboolean*) out_result = cache_ptr->active_depth_mask_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER:
        {
            *(GLuint*) out_result = cache_ptr->active_draw_fbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE:
        {
            *(GLuint*) out_result = cache_ptr->active_front_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            *((GLuint*) out_result) = cache_ptr->active_program_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER:
        {
            *(GLuint*) out_result = cache_ptr->active_read_fbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER:
        {
            *(GLuint*) out_result = cache_ptr->active_rbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_BLEND:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_blend_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_COLOR_LOGIC_OP:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_color_logic_op_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_cull_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_CLAMP:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_depth_clamp_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_TEST:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_depth_test_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DITHER:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_dither_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_FRAMEBUFFER_SRGB:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_framebuffer_srgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_LINE_SMOOTH:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_line_smooth_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_MULTISAMPLE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_multisample_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_FILL:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_polygon_offset_fill_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_LINE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_polygon_offset_line_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_SMOOTH:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_polygon_smooth_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_primitive_restart_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART_FIXED_INDEX:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PROGRAM_POINT_SIZE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_program_point_size_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_RASTERIZER_DISCARD:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_rasterizer_discard_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_COVERAGE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_ONE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_sample_alpha_to_one_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_COVERAGE:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_sample_coverage_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_SHADING:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_sample_shading_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_MASK:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_sample_mask_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SCISSOR_TEST:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_scissor_test_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_STENCIL_TEST:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_stencil_test_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_TEXTURE_CUBE_MAP_SEAMLESS:
        {
            *(GLboolean*) out_result = cache_ptr->active_rendering_mode_texture_cube_map_seamless_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX:
        {
            memcpy(out_result,
                   cache_ptr->active_scissor_box_local,
                   sizeof(cache_ptr->active_scissor_box_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT:
        {
            *((GLuint*) out_result) = cache_ptr->active_texture_unit_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT:
        {
            *((GLuint*) out_result) = cache_ptr->active_vertex_array_object_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT:
        {
            memcpy(out_result,
                   cache_ptr->active_viewport_local,
                   sizeof(cache_ptr->active_viewport_local) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_state_cache property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_init(ogl_context_state_cache                   cache,
                                         const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_state_cache*    cache_ptr  = (_ogl_context_state_cache*) cache;
    const ogl_context_gl_limits* limits_ptr = NULL;

    /* Cache info in private descriptor */
    cache_ptr->entrypoints_private_ptr = entrypoints_private_ptr;

    /* Set default state: active color mask */
    cache_ptr->active_color_mask_context[0] = GL_TRUE;
    cache_ptr->active_color_mask_context[1] = GL_TRUE;
    cache_ptr->active_color_mask_context[2] = GL_TRUE;
    cache_ptr->active_color_mask_context[3] = GL_TRUE;

    memcpy(cache_ptr->active_color_mask_local,
           cache_ptr->active_color_mask_context,
           sizeof(cache_ptr->active_color_mask_local) );

    /* Set default state: active depth mask */
    cache_ptr->active_depth_mask_context = GL_TRUE;
    cache_ptr->active_depth_mask_local   = GL_TRUE;

    /* Set default state: active draw framebuffer */
    cache_ptr->active_draw_fbo_context = 0;
    cache_ptr->active_draw_fbo_local   = 0;

    /* Set default state: active front face */
    cache_ptr->active_front_face_context = GL_CCW;
    cache_ptr->active_front_face_local   = GL_CCW;

    /* Set default state: active program */
    cache_ptr->active_program_context      = 0;
    cache_ptr->active_program_local        = 0;

    /* Set default state: active renderbuffer binding */
    cache_ptr->active_rbo_context = 0;
    cache_ptr->active_rbo_local   = 0;

    /* Set default state: active read framebuffer */
    cache_ptr->active_read_fbo_context = 0;
    cache_ptr->active_read_fbo_local   = 0;

    /* Set default state: active texture unit */
    cache_ptr->active_texture_unit_context = 0;
    cache_ptr->active_texture_unit_local   = 0;

    /* Set default state: active VAO */
    cache_ptr->active_vertex_array_object_context = 0;
    cache_ptr->active_vertex_array_object_local   = 0;

    /* Set default state: blend color */
    memset(cache_ptr->blend_color_context,
           0,
           sizeof(cache_ptr->blend_color_context) );
    memset(cache_ptr->blend_color_local,
           0,
           sizeof(cache_ptr->blend_color_local) );

    /* Set default state: blend equation (alpha) */
    cache_ptr->blend_equation_alpha_context = GL_FUNC_ADD;
    cache_ptr->blend_equation_alpha_local   = GL_FUNC_ADD;

    /* Set default state: blend equation (RGB) */
    cache_ptr->blend_equation_rgb_context = GL_FUNC_ADD;
    cache_ptr->blend_equation_rgb_local   = GL_FUNC_ADD;

    /* Set default state: blend function (destination, alpha) */
    cache_ptr->blend_function_dst_alpha_context = GL_ZERO;
    cache_ptr->blend_function_dst_alpha_local   = GL_ZERO;

    /* Set default state: blend function (destination, RGB) */
    cache_ptr->blend_function_dst_rgb_context = GL_ZERO;
    cache_ptr->blend_function_dst_rgb_local   = GL_ZERO;

    /* Set default state: blend function (source, alpha) */
    cache_ptr->blend_function_src_alpha_context = GL_ONE;
    cache_ptr->blend_function_src_alpha_local   = GL_ONE;

    /* Set default state: blend function (source, RGB) */
    cache_ptr->blend_function_src_rgb_context = GL_ONE;
    cache_ptr->blend_function_src_rgb_local   = GL_ONE;

    /* Set default state: clear color */
    memset(cache_ptr->active_clear_color_context,
           0,
           sizeof(cache_ptr->active_clear_color_context) );
    memset(cache_ptr->active_clear_color_local,
           0,
           sizeof(cache_ptr->active_clear_color_local) );

    /* Set default state: clear depth value */
    cache_ptr->active_clear_depth_context = 1.0f;
    cache_ptr->active_clear_depth_local   = 1.0f;

    /* Set default state: cull face */
    cache_ptr->active_cull_face_context = GL_BACK;
    cache_ptr->active_cull_face_local   = GL_BACK;

    /* Set default state: depth func */
    cache_ptr->active_depth_func_context = GL_LESS;
    cache_ptr->active_depth_func_local   = GL_LESS;

    /* Set default state: rendering modes */
    cache_ptr->active_rendering_mode_blend_context                         = false;
    cache_ptr->active_rendering_mode_blend_local                           = false;
    cache_ptr->active_rendering_mode_color_logic_op_context                = false;
    cache_ptr->active_rendering_mode_color_logic_op_local                  = false;
    cache_ptr->active_rendering_mode_cull_face_context                     = false;
    cache_ptr->active_rendering_mode_cull_face_local                       = false;
    cache_ptr->active_rendering_mode_depth_clamp_context                   = false;
    cache_ptr->active_rendering_mode_depth_clamp_local                     = false;
    cache_ptr->active_rendering_mode_depth_test_context                    = false;
    cache_ptr->active_rendering_mode_depth_test_local                      = false;
    cache_ptr->active_rendering_mode_dither_context                        = true;
    cache_ptr->active_rendering_mode_dither_local                          = true;
    cache_ptr->active_rendering_mode_framebuffer_srgb_context              = false;
    cache_ptr->active_rendering_mode_framebuffer_srgb_local                = false;
    cache_ptr->active_rendering_mode_line_smooth_context                   = false;
    cache_ptr->active_rendering_mode_line_smooth_local                     = false;
    cache_ptr->active_rendering_mode_multisample_context                   = false; /* actual value retrieved later on */
    cache_ptr->active_rendering_mode_multisample_local                     = false; /* actual value retrieved later on */
    cache_ptr->active_rendering_mode_polygon_offset_fill_context           = false;
    cache_ptr->active_rendering_mode_polygon_offset_fill_local             = false;
    cache_ptr->active_rendering_mode_polygon_offset_line_context           = false;
    cache_ptr->active_rendering_mode_polygon_offset_line_local             = false;
    cache_ptr->active_rendering_mode_polygon_smooth_context                = false;
    cache_ptr->active_rendering_mode_polygon_smooth_local                  = false;
    cache_ptr->active_rendering_mode_primitive_restart_context             = false;
    cache_ptr->active_rendering_mode_primitive_restart_local               = false;
    cache_ptr->active_rendering_mode_primitive_restart_fixed_index_context = false;
    cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local   = false;
    cache_ptr->active_rendering_mode_program_point_size_context            = false;
    cache_ptr->active_rendering_mode_program_point_size_local              = false;
    cache_ptr->active_rendering_mode_rasterizer_discard_context            = false;
    cache_ptr->active_rendering_mode_rasterizer_discard_local              = false;
    cache_ptr->active_rendering_mode_sample_alpha_to_coverage_context      = false;
    cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local        = false;
    cache_ptr->active_rendering_mode_sample_alpha_to_one_context           = false;
    cache_ptr->active_rendering_mode_sample_alpha_to_one_local             = false;
    cache_ptr->active_rendering_mode_sample_coverage_context               = false;
    cache_ptr->active_rendering_mode_sample_coverage_local                 = false;
    cache_ptr->active_rendering_mode_sample_shading_context                = false;
    cache_ptr->active_rendering_mode_sample_shading_local                  = false;
    cache_ptr->active_rendering_mode_sample_mask_context                   = false;
    cache_ptr->active_rendering_mode_sample_mask_local                     = false;
    cache_ptr->active_rendering_mode_scissor_test_context                  = false;
    cache_ptr->active_rendering_mode_scissor_test_local                    = false;
    cache_ptr->active_rendering_mode_stencil_test_context                  = false;
    cache_ptr->active_rendering_mode_stencil_test_local                    = false;
    cache_ptr->active_rendering_mode_texture_cube_map_seamless_context     = false;
    cache_ptr->active_rendering_mode_texture_cube_map_seamless_local       = false;

    entrypoints_private_ptr->pGLGetBooleanv(GL_MULTISAMPLE,
                                           &cache_ptr->active_rendering_mode_multisample_context);

    cache_ptr->active_rendering_mode_multisample_local = cache_ptr->active_rendering_mode_multisample_context;

    /* Set default state: scissor box */
    entrypoints_private_ptr->pGLGetIntegerv(GL_SCISSOR_BOX,
                                            cache_ptr->active_scissor_box_context);
    memcpy                                 (cache_ptr->active_scissor_box_local,
                                            cache_ptr->active_scissor_box_context,
                                            sizeof(cache_ptr->active_scissor_box_local) );

    /* Set default state: viewport */
    entrypoints_private_ptr->pGLGetIntegerv(GL_VIEWPORT,
                                            cache_ptr->active_viewport_context);
    memcpy                                 (cache_ptr->active_viewport_local,
                                            cache_ptr->active_viewport_context,
                                            sizeof(cache_ptr->active_viewport_local) );
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_release(ogl_context_state_cache cache)
{
    _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

    /* Done */
    delete cache_ptr;

    cache_ptr = NULL;
}

/* Please see header for spec */
PUBLIC void ogl_context_state_cache_set_property(ogl_context_state_cache          cache,
                                                 ogl_context_state_cache_property property,
                                                 const void*                      data)
{
    _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR:
        {
            memcpy(cache_ptr->blend_color_local,
                   data,
                   sizeof(cache_ptr->blend_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            cache_ptr->blend_equation_alpha_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB:
        {
            cache_ptr->blend_equation_rgb_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA:
        {
            cache_ptr->blend_function_dst_alpha_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB:
        {
            cache_ptr->blend_function_dst_rgb_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA:
        {
            cache_ptr->blend_function_src_alpha_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB:
        {
            cache_ptr->blend_function_src_rgb_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR:
        {
            memcpy(cache_ptr->active_clear_color_local,
                   data,
                   sizeof(cache_ptr->active_clear_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH:
        {
            cache_ptr->active_clear_depth_local = *(GLdouble*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK:
        {
            memcpy(cache_ptr->active_color_mask_local,
                   data,
                   sizeof(cache_ptr->active_color_mask_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE:
        {
            cache_ptr->active_cull_face_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC:
        {
            cache_ptr->active_depth_func_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK:
        {
            cache_ptr->active_depth_mask_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER:
        {
            cache_ptr->active_draw_fbo_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE:
        {
            cache_ptr->active_front_face_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            cache_ptr->active_program_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER:
        {
            cache_ptr->active_read_fbo_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER:
        {
            cache_ptr->active_rbo_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_BLEND:
        {
            cache_ptr->active_rendering_mode_blend_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_COLOR_LOGIC_OP:
        {
            cache_ptr->active_rendering_mode_color_logic_op_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE:
        {
            cache_ptr->active_rendering_mode_cull_face_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_CLAMP:
        {
            cache_ptr->active_rendering_mode_depth_clamp_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_TEST:
        {
            cache_ptr->active_rendering_mode_depth_test_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DITHER:
        {
            cache_ptr->active_rendering_mode_dither_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_FRAMEBUFFER_SRGB:
        {
            cache_ptr->active_rendering_mode_framebuffer_srgb_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_LINE_SMOOTH:
        {
            cache_ptr->active_rendering_mode_line_smooth_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_MULTISAMPLE:
        {
            cache_ptr->active_rendering_mode_multisample_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_FILL:
        {
            cache_ptr->active_rendering_mode_polygon_offset_fill_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_LINE:
        {
            cache_ptr->active_rendering_mode_polygon_offset_line_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_SMOOTH:
        {
            cache_ptr->active_rendering_mode_polygon_smooth_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART:
        {
            cache_ptr->active_rendering_mode_primitive_restart_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART_FIXED_INDEX:
        {
            cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PROGRAM_POINT_SIZE:
        {
            cache_ptr->active_rendering_mode_program_point_size_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_RASTERIZER_DISCARD:
        {
            cache_ptr->active_rendering_mode_rasterizer_discard_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_COVERAGE:
        {
            cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_ONE:
        {
            cache_ptr->active_rendering_mode_sample_alpha_to_one_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_COVERAGE:
        {
            cache_ptr->active_rendering_mode_sample_coverage_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_SHADING:
        {
            cache_ptr->active_rendering_mode_sample_shading_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_MASK:
        {
            cache_ptr->active_rendering_mode_sample_mask_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SCISSOR_TEST:
        {
            cache_ptr->active_rendering_mode_scissor_test_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_STENCIL_TEST:
        {
            cache_ptr->active_rendering_mode_stencil_test_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_TEXTURE_CUBE_MAP_SEAMLESS:
        {
            cache_ptr->active_rendering_mode_texture_cube_map_seamless_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX:
        {
            memcpy(cache_ptr->active_scissor_box_local,
                   data,
                   sizeof(cache_ptr->active_scissor_box_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT:
        {
            cache_ptr->active_texture_unit_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT:
        {
            cache_ptr->active_vertex_array_object_local = *((GLuint*) data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT:
        {
            memcpy(cache_ptr->active_viewport_local,
                   data,
                   sizeof(cache_ptr->active_viewport_local) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_state_cache property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_sync(ogl_context_state_cache cache,
                                         uint32_t                sync_bits)
{
    /* NOTE: cache is NULL during rendering context initialization */
    if (cache != NULL)
    {
        _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

        /* Blending configuration */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_BLENDING) )
        {
            if (cache_ptr->blend_function_dst_alpha_context != cache_ptr->blend_function_dst_alpha_local ||
                cache_ptr->blend_function_dst_rgb_context   != cache_ptr->blend_function_dst_rgb_local   ||
                cache_ptr->blend_function_src_alpha_context != cache_ptr->blend_function_src_alpha_local ||
                cache_ptr->blend_function_src_rgb_context   != cache_ptr->blend_function_src_rgb_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLBlendFuncSeparate(cache_ptr->blend_function_src_rgb_local,
                                                                         cache_ptr->blend_function_dst_rgb_local,
                                                                         cache_ptr->blend_function_src_alpha_local,
                                                                         cache_ptr->blend_function_dst_alpha_local);

                cache_ptr->blend_function_dst_alpha_context = cache_ptr->blend_function_dst_alpha_local;
                cache_ptr->blend_function_dst_rgb_context   = cache_ptr->blend_function_dst_rgb_local;
                cache_ptr->blend_function_src_alpha_context = cache_ptr->blend_function_src_alpha_local;
                cache_ptr->blend_function_src_rgb_context   = cache_ptr->blend_function_src_rgb_local;
            }

            if (cache_ptr->blend_equation_rgb_context   != cache_ptr->blend_equation_rgb_local ||
                cache_ptr->blend_equation_alpha_context != cache_ptr->blend_equation_alpha_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLBlendEquationSeparate(cache_ptr->blend_equation_rgb_local,
                                                                             cache_ptr->blend_equation_alpha_local);

                cache_ptr->blend_equation_alpha_context = cache_ptr->blend_equation_alpha_local;
                cache_ptr->blend_equation_rgb_context   = cache_ptr->blend_equation_rgb_local;
            }

            if (memcmp(cache_ptr->blend_color_local,
                       cache_ptr->blend_color_context,
                       sizeof(cache_ptr->blend_color_local) ) != 0)
            {
                cache_ptr->entrypoints_private_ptr->pGLBlendColor(cache_ptr->blend_color_local[0],
                                                                  cache_ptr->blend_color_local[1],
                                                                  cache_ptr->blend_color_local[2],
                                                                  cache_ptr->blend_color_local[3]);
            }
        }

        /* Clear color */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_COLOR) &&
            (fabs(cache_ptr->active_clear_color_context[0] - cache_ptr->active_clear_color_local[0]) > 1e-5f ||
             fabs(cache_ptr->active_clear_color_context[1] - cache_ptr->active_clear_color_local[1]) > 1e-5f ||
             fabs(cache_ptr->active_clear_color_context[2] - cache_ptr->active_clear_color_local[2]) > 1e-5f ||
             fabs(cache_ptr->active_clear_color_context[3] - cache_ptr->active_clear_color_local[3]) > 1e-5f))
        {
            cache_ptr->entrypoints_private_ptr->pGLClearColor(cache_ptr->active_clear_color_local[0],
                                                              cache_ptr->active_clear_color_local[1],
                                                              cache_ptr->active_clear_color_local[2],
                                                              cache_ptr->active_clear_color_local[3]);

            memcpy(cache_ptr->active_clear_color_context,
                   cache_ptr->active_clear_color_local,
                   sizeof(cache_ptr->active_clear_color_context) );
        }

        /* Clear depth */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_DEPTH) &&
            fabs(cache_ptr->active_clear_depth_context - cache_ptr->active_clear_depth_local) > 1e-5f)
        {
            cache_ptr->entrypoints_private_ptr->pGLClearDepth(cache_ptr->active_clear_depth_local);

            cache_ptr->active_clear_depth_context = cache_ptr->active_clear_depth_local;
        }

        /* Color / depth mask */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK)
        {
            if (memcmp(cache_ptr->active_color_mask_context,
                       cache_ptr->active_color_mask_local,
                       sizeof(cache_ptr->active_color_mask_context)) != 0)
            {
                cache_ptr->entrypoints_private_ptr->pGLColorMask(cache_ptr->active_color_mask_local[0],
                                                                 cache_ptr->active_color_mask_local[1],
                                                                 cache_ptr->active_color_mask_local[2],
                                                                 cache_ptr->active_color_mask_local[3]);

                memcpy(cache_ptr->active_color_mask_context,
                       cache_ptr->active_color_mask_local,
                       sizeof(cache_ptr->active_color_mask_context) );
            }

            if (cache_ptr->active_depth_mask_context != cache_ptr->active_depth_mask_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLDepthMask(cache_ptr->active_depth_mask_local);

                cache_ptr->active_depth_mask_context = cache_ptr->active_depth_mask_local;
            }
        }

        /* Cull face */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE                        &&
            cache_ptr->active_cull_face_context != cache_ptr->active_cull_face_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLCullFace(cache_ptr->active_cull_face_local);

            cache_ptr->active_cull_face_context = cache_ptr->active_cull_face_local;
        }

        /* Depth func */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC &&
            cache_ptr->active_depth_func_context != cache_ptr->active_depth_func_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLDepthFunc(cache_ptr->active_depth_func_local);

            cache_ptr->active_depth_func_context = cache_ptr->active_depth_func_local;
        }

        /* Draw framebuffer */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER &&
            cache_ptr->active_draw_fbo_context != cache_ptr->active_draw_fbo_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                                   cache_ptr->active_draw_fbo_local);

            cache_ptr->active_draw_fbo_context = cache_ptr->active_draw_fbo_local;
        }

        /* Front face */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE &&
            cache_ptr->active_front_face_context != cache_ptr->active_front_face_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLFrontFace(cache_ptr->active_front_face_local);

            cache_ptr->active_front_face_context = cache_ptr->active_front_face_local;
        }

        /* Program object */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT)             &&
            cache_ptr->active_program_context != cache_ptr->active_program_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLUseProgram(cache_ptr->active_program_local);

            cache_ptr->active_program_context = cache_ptr->active_program_local;
        }

        /* Read framebuffer */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER &&
            cache_ptr->active_read_fbo_context != cache_ptr->active_read_fbo_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                                   cache_ptr->active_read_fbo_local);

            cache_ptr->active_read_fbo_context = cache_ptr->active_read_fbo_local;
        }

        /* Renderbuffer */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER)
        {
            cache_ptr->entrypoints_private_ptr->pGLBindRenderbuffer(GL_RENDERBUFFER,
                                                                    cache_ptr->active_rbo_local);

            cache_ptr->active_rbo_context = cache_ptr->active_rbo_local;
        }

        /* Rendering modes */
        #define UPDATE_STATE(context_state, local_state, gl_enum)       \
            if (local_state)                                            \
            {                                                           \
                cache_ptr->entrypoints_private_ptr->pGLEnable(gl_enum); \
            }                                                           \
            else                                                        \
            {                                                           \
                cache_ptr->entrypoints_private_ptr->pGLDisable(gl_enum);\
            }                                                           \
                                                                        \
            context_state = local_state;

        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES)
        {
            if (cache_ptr->active_rendering_mode_blend_context != cache_ptr->active_rendering_mode_blend_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_blend_context,
                             cache_ptr->active_rendering_mode_blend_local,
                             GL_BLEND);
            }

            if (cache_ptr->active_rendering_mode_color_logic_op_context != cache_ptr->active_rendering_mode_color_logic_op_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_color_logic_op_context,
                             cache_ptr->active_rendering_mode_color_logic_op_local,
                             GL_COLOR_LOGIC_OP);
            }

            if (cache_ptr->active_rendering_mode_cull_face_context != cache_ptr->active_rendering_mode_cull_face_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_cull_face_context,
                             cache_ptr->active_rendering_mode_cull_face_local,
                             GL_CULL_FACE);
            }

            if (cache_ptr->active_rendering_mode_depth_clamp_context != cache_ptr->active_rendering_mode_depth_clamp_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_depth_clamp_context,
                             cache_ptr->active_rendering_mode_depth_clamp_local,
                             GL_DEPTH_CLAMP);
            }

            if (cache_ptr->active_rendering_mode_depth_test_context != cache_ptr->active_rendering_mode_depth_test_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_depth_test_context,
                             cache_ptr->active_rendering_mode_depth_test_local,
                             GL_DEPTH_TEST);
            }

            if (cache_ptr->active_rendering_mode_dither_context != cache_ptr->active_rendering_mode_dither_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_dither_context,
                             cache_ptr->active_rendering_mode_dither_local,
                             GL_DITHER);
            }

            if (cache_ptr->active_rendering_mode_framebuffer_srgb_context != cache_ptr->active_rendering_mode_framebuffer_srgb_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_framebuffer_srgb_context,
                             cache_ptr->active_rendering_mode_framebuffer_srgb_local,
                             GL_FRAMEBUFFER_SRGB);
            }

            if (cache_ptr->active_rendering_mode_line_smooth_context != cache_ptr->active_rendering_mode_line_smooth_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_line_smooth_context,
                             cache_ptr->active_rendering_mode_line_smooth_local,
                             GL_LINE_SMOOTH);
            }

            if (cache_ptr->active_rendering_mode_multisample_context != cache_ptr->active_rendering_mode_multisample_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_multisample_context,
                             cache_ptr->active_rendering_mode_multisample_local,
                             GL_MULTISAMPLE);
            }

            if (cache_ptr->active_rendering_mode_polygon_offset_fill_context != cache_ptr->active_rendering_mode_polygon_offset_fill_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_polygon_offset_fill_context,
                             cache_ptr->active_rendering_mode_polygon_offset_fill_local,
                             GL_POLYGON_OFFSET_FILL);
            }

            if (cache_ptr->active_rendering_mode_polygon_offset_line_context != cache_ptr->active_rendering_mode_polygon_offset_line_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_polygon_offset_line_context,
                             cache_ptr->active_rendering_mode_polygon_offset_line_local,
                             GL_POLYGON_OFFSET_LINE);
            }

            if (cache_ptr->active_rendering_mode_polygon_smooth_context != cache_ptr->active_rendering_mode_polygon_smooth_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_polygon_smooth_context,
                             cache_ptr->active_rendering_mode_polygon_smooth_local,
                             GL_POLYGON_SMOOTH);
            }

            if (cache_ptr->active_rendering_mode_primitive_restart_context != cache_ptr->active_rendering_mode_primitive_restart_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_primitive_restart_context,
                             cache_ptr->active_rendering_mode_primitive_restart_local,
                             GL_PRIMITIVE_RESTART);
            }

            if (cache_ptr->active_rendering_mode_primitive_restart_fixed_index_context != cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_primitive_restart_fixed_index_context,
                             cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local,
                             GL_PRIMITIVE_RESTART_FIXED_INDEX);
            }

            if (cache_ptr->active_rendering_mode_program_point_size_context != cache_ptr->active_rendering_mode_program_point_size_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_program_point_size_context,
                             cache_ptr->active_rendering_mode_program_point_size_local,
                             GL_PROGRAM_POINT_SIZE);
            }

            if (cache_ptr->active_rendering_mode_rasterizer_discard_context != cache_ptr->active_rendering_mode_rasterizer_discard_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_rasterizer_discard_context,
                             cache_ptr->active_rendering_mode_rasterizer_discard_local,
                             GL_RASTERIZER_DISCARD);
            }

            if (cache_ptr->active_rendering_mode_sample_alpha_to_coverage_context != cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_sample_alpha_to_coverage_context,
                             cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local,
                             GL_SAMPLE_ALPHA_TO_COVERAGE);
            }

            if (cache_ptr->active_rendering_mode_sample_alpha_to_one_context != cache_ptr->active_rendering_mode_sample_alpha_to_one_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_sample_alpha_to_one_context,
                             cache_ptr->active_rendering_mode_sample_alpha_to_one_local,
                             GL_SAMPLE_ALPHA_TO_ONE);
            }

            if (cache_ptr->active_rendering_mode_sample_coverage_context != cache_ptr->active_rendering_mode_sample_coverage_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_sample_coverage_context,
                             cache_ptr->active_rendering_mode_sample_coverage_local,
                             GL_SAMPLE_COVERAGE);
            }

            if (cache_ptr->active_rendering_mode_sample_shading_context != cache_ptr->active_rendering_mode_sample_shading_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_sample_shading_context,
                             cache_ptr->active_rendering_mode_sample_shading_local,
                             GL_SAMPLE_SHADING);
            }

            if (cache_ptr->active_rendering_mode_sample_mask_context != cache_ptr->active_rendering_mode_sample_mask_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_sample_mask_context,
                             cache_ptr->active_rendering_mode_sample_mask_local,
                             GL_SAMPLE_MASK);
            }

            if (cache_ptr->active_rendering_mode_scissor_test_context != cache_ptr->active_rendering_mode_scissor_test_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_scissor_test_context,
                             cache_ptr->active_rendering_mode_scissor_test_local,
                             GL_SCISSOR_TEST);
            }

            if (cache_ptr->active_rendering_mode_stencil_test_context != cache_ptr->active_rendering_mode_stencil_test_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_stencil_test_context,
                             cache_ptr->active_rendering_mode_stencil_test_local,
                             GL_STENCIL_TEST);
            }

            if (cache_ptr->active_rendering_mode_texture_cube_map_seamless_context != cache_ptr->active_rendering_mode_texture_cube_map_seamless_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_texture_cube_map_seamless_context,
                             cache_ptr->active_rendering_mode_texture_cube_map_seamless_local,
                             GL_TEXTURE_CUBE_MAP_SEAMLESS);
            }
        } /* if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES) */

        /* Scissor box */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX) &&
            (cache_ptr->active_scissor_box_context[0] != cache_ptr->active_scissor_box_local[0] ||
             cache_ptr->active_scissor_box_context[1] != cache_ptr->active_scissor_box_local[1] ||
             cache_ptr->active_scissor_box_context[2] != cache_ptr->active_scissor_box_local[2] ||
             cache_ptr->active_scissor_box_context[3] != cache_ptr->active_scissor_box_local[3]))
        {
            cache_ptr->entrypoints_private_ptr->pGLScissor(cache_ptr->active_scissor_box_local[0],
                                                           cache_ptr->active_scissor_box_local[1],
                                                           cache_ptr->active_scissor_box_local[2],
                                                           cache_ptr->active_scissor_box_local[3]);

            memcpy(cache_ptr->active_scissor_box_context,
                   cache_ptr->active_scissor_box_local,
                   sizeof(cache_ptr->active_scissor_box_context) );
        }

        /* Texture unit */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT)                         &&
            cache_ptr->active_texture_unit_context != cache_ptr->active_texture_unit_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLActiveTexture(GL_TEXTURE0 + cache_ptr->active_texture_unit_local);

            cache_ptr->active_texture_unit_context = cache_ptr->active_texture_unit_local;
        }

        /* Vertex array object */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT)                                 &&
            cache_ptr->active_vertex_array_object_context != cache_ptr->active_vertex_array_object_local)
        {
            /* Bind the VAO */
            cache_ptr->entrypoints_private_ptr->pGLBindVertexArray(cache_ptr->active_vertex_array_object_local);

            cache_ptr->active_vertex_array_object_context = cache_ptr->active_vertex_array_object_local;

            /* Make sure the indexed binding is up-to-date */
            ogl_vao          current_vao             = NULL;
            GLuint           indexed_binding_context = -1;
            GLuint           indexed_binding_local   = -1;
            ogl_context_vaos vaos                    = NULL;

            ogl_context_get_property(cache_ptr->context,
                                     OGL_CONTEXT_PROPERTY_VAOS,
                                    &vaos);

            current_vao = ogl_context_vaos_get_vao(vaos,
                                                   cache_ptr->active_vertex_array_object_local);

            ogl_vao_get_property(current_vao,
                                 OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_CONTEXT,
                                &indexed_binding_context);
            ogl_vao_get_property(current_vao,
                                 OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_LOCAL,
                                &indexed_binding_local);

            if (indexed_binding_context != indexed_binding_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                                                  indexed_binding_local);

                ogl_vao_set_property(current_vao,
                                     OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_CONTEXT,
                                    &indexed_binding_local);
            } /* if (indexed_binding_context != indexed_binding_local) */
        }

        /* Viewport */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT)                            &&
            (cache_ptr->active_viewport_context[0] != cache_ptr->active_viewport_local[0] ||
             cache_ptr->active_viewport_context[1] != cache_ptr->active_viewport_local[1] ||
             cache_ptr->active_viewport_context[2] != cache_ptr->active_viewport_local[2] ||
             cache_ptr->active_viewport_context[3] != cache_ptr->active_viewport_local[3] ))
        {
            cache_ptr->entrypoints_private_ptr->pGLViewport(cache_ptr->active_viewport_local[0],
                                                            cache_ptr->active_viewport_local[1],
                                                            cache_ptr->active_viewport_local[2],
                                                            cache_ptr->active_viewport_local[3]);

            memcpy(cache_ptr->active_viewport_context,
                   cache_ptr->active_viewport_local,
                   sizeof(cache_ptr->active_viewport_local) );
        }
    } /* if (cache != NULL) */
}