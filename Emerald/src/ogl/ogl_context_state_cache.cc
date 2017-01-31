/**
 *
 * Emerald (kbi/elude @2014-2016)
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

    GLdouble* active_depth_ranges_context; /* each depth range = 2 consecutive GLdouble values */
    GLdouble* active_depth_ranges_local;   /* each depth range = 2 consecutive GLdouble values */

    GLuint active_draw_fbo_context;
    GLuint active_draw_fbo_local;

    GLenum active_front_face_context;
    GLenum active_front_face_local;

    GLfloat active_line_width_context;
    GLfloat active_line_width_local;

    GLfloat active_min_sample_shading_context;
    GLfloat active_min_sample_shading_local;

    GLint active_n_patch_vertices_context;
    GLint active_n_patch_vertices_local;

    GLenum active_polygon_mode_back_face_context;
    GLenum active_polygon_mode_back_face_local;
    GLenum active_polygon_mode_front_face_context;
    GLenum active_polygon_mode_front_face_local;

    GLfloat active_polygon_offset_factor_context;
    GLfloat active_polygon_offset_factor_local;
    GLfloat active_polygon_offset_units_context;
    GLfloat active_polygon_offset_units_local;

    GLuint active_program_context;
    GLuint active_program_local;

    GLuint active_read_fbo_context;
    GLuint active_read_fbo_local;

    GLuint active_rbo_context;
    GLuint active_rbo_local;

    GLint* active_scissor_boxes_context; /* each scissor box = 4 consecutive GLint values */
    GLint* active_scissor_boxes_local;   /* each scissor box = 4 consecutive GLint values */

    GLuint active_texture_unit_context;
    GLuint active_texture_unit_local;

    GLuint active_vertex_array_object_context;
    GLuint active_vertex_array_object_local;

    GLfloat* active_viewports_context; /* each viewport = 4 consecutive GLint values */
    GLfloat* active_viewports_local;   /* each viewport = 4 consecutive GLint values */

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
    GLboolean active_rendering_mode_polygon_offset_point_context;
    GLboolean active_rendering_mode_polygon_offset_point_local;
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
    const ogl_context_gl_limits*              limits_ptr;
} _ogl_context_state_cache;


/** Please see header for spec */
PUBLIC ogl_context_state_cache ogl_context_state_cache_create(ogl_context context)
{
    _ogl_context_state_cache* new_cache_ptr = new (std::nothrow) _ogl_context_state_cache;

    ASSERT_ALWAYS_SYNC(new_cache_ptr != nullptr,
                       "Out of memory");

    if (new_cache_ptr != nullptr)
    {
        new_cache_ptr->context = context;
    }

    return reinterpret_cast<ogl_context_state_cache>(new_cache_ptr);
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_get_indexed_property(const ogl_context_state_cache            cache,
                                                         ogl_context_state_cache_indexed_property property,
                                                         uint32_t                                 index,
                                                         void*                                    data)
{
    const _ogl_context_state_cache* cache_ptr = reinterpret_cast<const _ogl_context_state_cache*>(cache);

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_DEPTH_RANGE:
        {
            memcpy(data,
                   cache_ptr->active_depth_ranges_local + 2 * index,
                   sizeof(GLdouble) * 2);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_SCISSOR_BOX:
        {
            memcpy(data,
                   cache_ptr->active_scissor_boxes_local + 4 * index,
                   sizeof(GLint) * 4);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_VIEWPORT:
        {
            memcpy(data,
                   cache_ptr->active_viewports_local + 4 * index,
                   sizeof(GLint) * 4);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized indexed property was specified.");
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_get_property(const ogl_context_state_cache    cache,
                                                 ogl_context_state_cache_property property,
                                                 void*                            out_result_ptr)
{
    const _ogl_context_state_cache* cache_ptr = reinterpret_cast<const _ogl_context_state_cache*>(cache);

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR:
        {
            memcpy(out_result_ptr,
                   cache_ptr->blend_color_local,
                   sizeof(cache_ptr->blend_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->blend_equation_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->blend_equation_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->blend_function_dst_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->blend_function_dst_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->blend_function_src_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->blend_function_src_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK:
        {
            memcpy(out_result_ptr,
                   cache_ptr->active_color_mask_local,
                   sizeof(cache_ptr->active_color_mask_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->active_cull_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->active_depth_func_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_depth_mask_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_draw_fbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_front_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_LINE_WIDTH:
        {
            *reinterpret_cast<GLfloat*>(out_result_ptr) = cache_ptr->active_line_width_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_MIN_SAMPLE_SHADING:
        {
            *reinterpret_cast<GLfloat*>(out_result_ptr) = cache_ptr->active_min_sample_shading_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_N_PATCH_VERTICES:
        {
            *reinterpret_cast<GLint*>(out_result_ptr) = cache_ptr->active_n_patch_vertices_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_MODE_BACK_FACE:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->active_polygon_mode_back_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_MODE_FRONT_FACE:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = cache_ptr->active_polygon_mode_front_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_FACTOR:
        {
            *reinterpret_cast<GLfloat*>(out_result_ptr) = cache_ptr->active_polygon_offset_factor_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_UNITS:
        {
            *reinterpret_cast<GLfloat*>(out_result_ptr) = cache_ptr->active_polygon_offset_units_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_program_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_read_fbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_rbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_BLEND:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_blend_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_COLOR_LOGIC_OP:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_color_logic_op_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_cull_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_CLAMP:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_depth_clamp_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_TEST:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_depth_test_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DITHER:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_dither_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_FRAMEBUFFER_SRGB:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_framebuffer_srgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_LINE_SMOOTH:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_line_smooth_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_MULTISAMPLE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_multisample_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_FILL:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_polygon_offset_fill_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_LINE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_polygon_offset_line_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_POINT:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_polygon_offset_point_local;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_SMOOTH:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_polygon_smooth_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_primitive_restart_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART_FIXED_INDEX:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PROGRAM_POINT_SIZE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_program_point_size_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_RASTERIZER_DISCARD:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_rasterizer_discard_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_COVERAGE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_ONE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_sample_alpha_to_one_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_COVERAGE:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_sample_coverage_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_SHADING:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_sample_shading_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_MASK:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_sample_mask_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SCISSOR_TEST:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_scissor_test_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_STENCIL_TEST:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_stencil_test_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_TEXTURE_CUBE_MAP_SEAMLESS:
        {
            *reinterpret_cast<GLboolean*>(out_result_ptr) = cache_ptr->active_rendering_mode_texture_cube_map_seamless_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_texture_unit_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = cache_ptr->active_vertex_array_object_local;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_state_cache property");
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_init(ogl_context_state_cache                   cache,
                                         const ogl_context_gl_entrypoints_private* entrypoints_private_ptr,
                                         const ogl_context_gl_limits*              limits_ptr)
{
    _ogl_context_state_cache* cache_ptr = reinterpret_cast<_ogl_context_state_cache*>(cache);

    /* Cache info in private descriptor */
    cache_ptr->entrypoints_private_ptr = entrypoints_private_ptr;
    cache_ptr->limits_ptr              = limits_ptr;

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

    /* Set default state: line width */
    cache_ptr->active_line_width_context = 1.0f;
    cache_ptr->active_line_width_local   = 1.0f;

    /* Set default state: min sample shading */
    cache_ptr->active_min_sample_shading_context = 0.0f;
    cache_ptr->active_min_sample_shading_local   = 0.0f;

    /* Set default state: n of patch vertices */
    cache_ptr->active_n_patch_vertices_context = 3;
    cache_ptr->active_n_patch_vertices_local   = 3;

    /* Set default state: poylgon mode state */
    cache_ptr->active_polygon_mode_back_face_context  = GL_FILL;
    cache_ptr->active_polygon_mode_back_face_local    = GL_FILL;
    cache_ptr->active_polygon_mode_front_face_context = GL_FILL;
    cache_ptr->active_polygon_mode_front_face_local   = GL_FILL;

    /* Set default state: polygon offset state */
    cache_ptr->active_polygon_offset_factor_context = 0.0f;
    cache_ptr->active_polygon_offset_factor_local   = 0.0f;
    cache_ptr->active_polygon_offset_units_context  = 0.0f;
    cache_ptr->active_polygon_offset_units_local    = 0.0f;

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
    cache_ptr->active_rendering_mode_polygon_offset_point_context          = false;
    cache_ptr->active_rendering_mode_polygon_offset_point_local            = false;
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
    ASSERT_DEBUG_SYNC(limits_ptr->max_viewports >= 1,
                      "Invalid maximum number of viewports reported by the GL implementation");

    cache_ptr->active_scissor_boxes_context = new GLint[4 * limits_ptr->max_viewports];
    cache_ptr->active_scissor_boxes_local   = new GLint[4 * limits_ptr->max_viewports];

    entrypoints_private_ptr->pGLGetIntegerv(GL_SCISSOR_BOX,
                                            cache_ptr->active_scissor_boxes_context);

    for (int32_t n_scissor_box = 1;
                 n_scissor_box < limits_ptr->max_viewports;
               ++n_scissor_box)
    {
        memcpy(cache_ptr->active_scissor_boxes_context + 4 * n_scissor_box,
               cache_ptr->active_scissor_boxes_context,
               sizeof(GLint) * 4);
    }

    memcpy(cache_ptr->active_scissor_boxes_local,
           cache_ptr->active_scissor_boxes_context,
           sizeof(GLint) * 4 * limits_ptr->max_viewports);

    /* Set default state: viewport */
    cache_ptr->active_depth_ranges_context = new GLdouble[2 * limits_ptr->max_viewports];
    cache_ptr->active_depth_ranges_local   = new GLdouble[2 * limits_ptr->max_viewports];
    cache_ptr->active_viewports_context    = new GLfloat[4 * limits_ptr->max_viewports];
    cache_ptr->active_viewports_local      = new GLfloat[4 * limits_ptr->max_viewports];

    ASSERT_DEBUG_SYNC(cache_ptr->active_depth_ranges_context != nullptr &&
                      cache_ptr->active_depth_ranges_local   != nullptr &&
                      cache_ptr->active_viewports_context    != nullptr &&
                      cache_ptr->active_viewports_local      != nullptr,
                      "Out of memory");

    entrypoints_private_ptr->pGLGetFloatv(GL_SCISSOR_BOX,
                                          cache_ptr->active_viewports_context);

    cache_ptr->active_depth_ranges_context[0] = 0.0;
    cache_ptr->active_depth_ranges_context[1] = 1.0;
    cache_ptr->active_depth_ranges_local[0]   = 0.0;
    cache_ptr->active_depth_ranges_local[1]   = 1.0;

    for (int32_t n_viewport = 1;
                 n_viewport < limits_ptr->max_viewports;
               ++n_viewport)
    {
        memcpy(cache_ptr->active_depth_ranges_context + 2 * n_viewport,
               cache_ptr->active_depth_ranges_context,
               sizeof(GLdouble) * 2);
        memcpy(cache_ptr->active_depth_ranges_local + 2 * n_viewport,
               cache_ptr->active_depth_ranges_context,
               sizeof(GLdouble) * 2);
        memcpy(cache_ptr->active_viewports_context + 4 * n_viewport,
               cache_ptr->active_viewports_context,
               sizeof(GLfloat) * 4);
    }

    memcpy(cache_ptr->active_viewports_local,
           cache_ptr->active_viewports_context,
           sizeof(GLfloat) * 4 * limits_ptr->max_viewports);
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_release(ogl_context_state_cache cache)
{
    _ogl_context_state_cache* cache_ptr = reinterpret_cast<_ogl_context_state_cache*>(cache);

    if (cache_ptr->active_depth_ranges_context != nullptr)
    {
        delete [] cache_ptr->active_depth_ranges_context;

        cache_ptr->active_depth_ranges_context = nullptr;
    }

    if (cache_ptr->active_depth_ranges_local != nullptr)
    {
        delete [] cache_ptr->active_depth_ranges_local;

        cache_ptr->active_depth_ranges_local = nullptr;
    }

    if (cache_ptr->active_scissor_boxes_context != nullptr)
    {
        delete [] cache_ptr->active_scissor_boxes_context;

        cache_ptr->active_scissor_boxes_context = nullptr;
    }

    if (cache_ptr->active_scissor_boxes_local != nullptr)
    {
        delete [] cache_ptr->active_scissor_boxes_local;

        cache_ptr->active_scissor_boxes_local = nullptr;
    }

    if (cache_ptr->active_viewports_context != nullptr)
    {
        delete [] cache_ptr->active_viewports_context;

        cache_ptr->active_viewports_context = nullptr;
    }

    if (cache_ptr->active_viewports_local != nullptr)
    {
        delete [] cache_ptr->active_viewports_local;

        cache_ptr->active_viewports_local = nullptr;
    }

    /* Done */
    delete cache_ptr;

    cache_ptr = nullptr;
}

/* Please see header for spec */
PUBLIC void ogl_context_state_cache_set_indexed_property(ogl_context_state_cache                  cache,
                                                         ogl_context_state_cache_indexed_property property,
                                                         uint32_t                                 index,
                                                         const void*                              data)
{
    _ogl_context_state_cache* cache_ptr = reinterpret_cast<_ogl_context_state_cache*>(cache);

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_DEPTH_RANGE:
        {
            memcpy(cache_ptr->active_depth_ranges_local + 2 * index,
                   data,
                   sizeof(GLdouble) * 2);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_SCISSOR_BOX:
        {
            memcpy(cache_ptr->active_scissor_boxes_local + 4 * index,
                   data,
                   sizeof(GLint) * 4);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_VIEWPORT:
        {
            memcpy(cache_ptr->active_viewports_local + 4 * index,
                   data,
                   sizeof(GLint) * 4);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized indexed property was specified.");
        }
    }
}

/* Please see header for spec */
PUBLIC void ogl_context_state_cache_set_property(ogl_context_state_cache          cache,
                                                 ogl_context_state_cache_property property,
                                                 const void*                      data)
{
    _ogl_context_state_cache* cache_ptr = reinterpret_cast<_ogl_context_state_cache*>(cache);

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
            cache_ptr->blend_equation_alpha_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB:
        {
            cache_ptr->blend_equation_rgb_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA:
        {
            cache_ptr->blend_function_dst_alpha_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB:
        {
            cache_ptr->blend_function_dst_rgb_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA:
        {
            cache_ptr->blend_function_src_alpha_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB:
        {
            cache_ptr->blend_function_src_rgb_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR:
        {
            memcpy(cache_ptr->active_clear_color_local,
                   data,
                   sizeof(cache_ptr->active_clear_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH_DOUBLE:
        {
            cache_ptr->active_clear_depth_local = *reinterpret_cast<const GLdouble*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH_FLOAT:
        {
            cache_ptr->active_clear_depth_local = *reinterpret_cast<const GLfloat*>(data);

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
            cache_ptr->active_cull_face_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC:
        {
            cache_ptr->active_depth_func_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK:
        {
            cache_ptr->active_depth_mask_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER:
        {
            cache_ptr->active_draw_fbo_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE:
        {
            cache_ptr->active_front_face_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_LINE_WIDTH:
        {
            cache_ptr->active_line_width_local = *reinterpret_cast<const GLfloat*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_MIN_SAMPLE_SHADING:
        {
            cache_ptr->active_min_sample_shading_local = *reinterpret_cast<const GLfloat*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_N_PATCH_VERTICES:
        {
            cache_ptr->active_n_patch_vertices_local = *reinterpret_cast<const GLint*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_MODE_BACK_FACE:
        {
            cache_ptr->active_polygon_mode_back_face_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_MODE_FRONT_FACE:
        {
            cache_ptr->active_polygon_mode_front_face_local = *reinterpret_cast<const GLenum*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_FACTOR:
        {
            cache_ptr->active_polygon_offset_factor_local = *reinterpret_cast<const GLfloat*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_UNITS:
        {
            cache_ptr->active_polygon_offset_units_local = *reinterpret_cast<const GLfloat*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            cache_ptr->active_program_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER:
        {
            cache_ptr->active_read_fbo_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER:
        {
            cache_ptr->active_rbo_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_BLEND:
        {
            cache_ptr->active_rendering_mode_blend_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_COLOR_LOGIC_OP:
        {
            cache_ptr->active_rendering_mode_color_logic_op_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE:
        {
            cache_ptr->active_rendering_mode_cull_face_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_CLAMP:
        {
            cache_ptr->active_rendering_mode_depth_clamp_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_TEST:
        {
            cache_ptr->active_rendering_mode_depth_test_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DITHER:
        {
            cache_ptr->active_rendering_mode_dither_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_FRAMEBUFFER_SRGB:
        {
            cache_ptr->active_rendering_mode_framebuffer_srgb_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_LINE_SMOOTH:
        {
            cache_ptr->active_rendering_mode_line_smooth_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_MULTISAMPLE:
        {
            cache_ptr->active_rendering_mode_multisample_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_FILL:
        {
            cache_ptr->active_rendering_mode_polygon_offset_fill_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_LINE:
        {
            cache_ptr->active_rendering_mode_polygon_offset_line_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_POINT:
        {
            cache_ptr->active_rendering_mode_polygon_offset_point_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_SMOOTH:
        {
            cache_ptr->active_rendering_mode_polygon_smooth_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART:
        {
            cache_ptr->active_rendering_mode_primitive_restart_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART_FIXED_INDEX:
        {
            cache_ptr->active_rendering_mode_primitive_restart_fixed_index_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PROGRAM_POINT_SIZE:
        {
            cache_ptr->active_rendering_mode_program_point_size_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_RASTERIZER_DISCARD:
        {
            cache_ptr->active_rendering_mode_rasterizer_discard_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_COVERAGE:
        {
            cache_ptr->active_rendering_mode_sample_alpha_to_coverage_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_ONE:
        {
            cache_ptr->active_rendering_mode_sample_alpha_to_one_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_COVERAGE:
        {
            cache_ptr->active_rendering_mode_sample_coverage_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_SHADING:
        {
            cache_ptr->active_rendering_mode_sample_shading_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_MASK:
        {
            cache_ptr->active_rendering_mode_sample_mask_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SCISSOR_TEST:
        {
            cache_ptr->active_rendering_mode_scissor_test_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_STENCIL_TEST:
        {
            cache_ptr->active_rendering_mode_stencil_test_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_TEXTURE_CUBE_MAP_SEAMLESS:
        {
            cache_ptr->active_rendering_mode_texture_cube_map_seamless_local = *reinterpret_cast<const GLboolean*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT:
        {
            cache_ptr->active_texture_unit_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT:
        {
            cache_ptr->active_vertex_array_object_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_state_cache property");
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_sync(ogl_context_state_cache cache,
                                         uint32_t                sync_bits)
{
    /* NOTE: cache is NULL during rendering context initialization */
    if (cache != nullptr)
    {
        _ogl_context_state_cache* cache_ptr = reinterpret_cast<_ogl_context_state_cache*>(cache);

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

        /* Line Width */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_LINE_WIDTH) &&
            fabs(cache_ptr->active_line_width_context - cache_ptr->active_line_width_local) > 1e-5f)
        {
            cache_ptr->entrypoints_private_ptr->pGLLineWidth(cache_ptr->active_line_width_local);

            cache_ptr->active_line_width_context = cache_ptr->active_line_width_local;
        }

        /* Minimum fraction of samples to calculate per-fragment */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING &&
            cache_ptr->active_min_sample_shading_context != cache_ptr->active_min_sample_shading_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLMinSampleShading(cache_ptr->active_min_sample_shading_local);

            cache_ptr->active_min_sample_shading_context = cache_ptr->active_min_sample_shading_local;
        }

        /* Number of patch vertices */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES &&
            cache_ptr->active_n_patch_vertices_context != cache_ptr->active_n_patch_vertices_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLPatchParameteri(GL_PATCH_VERTICES,
                                                                   cache_ptr->active_n_patch_vertices_local);

            cache_ptr->active_n_patch_vertices_context = cache_ptr->active_n_patch_vertices_local;
        }

        /* Polygon mode state */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_MODE)
        {
            if ( cache_ptr->active_polygon_mode_back_face_local  == cache_ptr->active_polygon_mode_front_face_local  &&
                (cache_ptr->active_polygon_mode_back_face_local  != cache_ptr->active_polygon_mode_back_face_context ||
                 cache_ptr->active_polygon_mode_front_face_local != cache_ptr->active_polygon_mode_front_face_context) )
            {
                cache_ptr->entrypoints_private_ptr->pGLPolygonMode(GL_FRONT_AND_BACK,
                                                                   cache_ptr->active_polygon_mode_back_face_local);

                cache_ptr->active_polygon_mode_back_face_context  = cache_ptr->active_polygon_mode_back_face_local;
                cache_ptr->active_polygon_mode_front_face_context = cache_ptr->active_polygon_mode_back_face_local;
            }
            else
            {
                if (cache_ptr->active_polygon_mode_back_face_local != cache_ptr->active_polygon_mode_back_face_context)
                {
                    cache_ptr->entrypoints_private_ptr->pGLPolygonMode(GL_BACK,
                                                                       cache_ptr->active_polygon_mode_back_face_local);

                    cache_ptr->active_polygon_mode_back_face_context = cache_ptr->active_polygon_mode_back_face_local;
                }

                if (cache_ptr->active_polygon_mode_front_face_local != cache_ptr->active_polygon_mode_front_face_context)
                {
                    cache_ptr->entrypoints_private_ptr->pGLPolygonMode(GL_FRONT,
                                                                       cache_ptr->active_polygon_mode_front_face_local);

                    cache_ptr->active_polygon_mode_front_face_context = cache_ptr->active_polygon_mode_front_face_local;
                }
            }
        }

        /* Polygon offset state */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE)                                                 &&
            (fabs(cache_ptr->active_polygon_offset_factor_context - cache_ptr->active_polygon_offset_factor_local) > 1e-5f ||
             fabs(cache_ptr->active_polygon_offset_units_context  - cache_ptr->active_polygon_offset_units_local)  > 1e-5f) )
        {
            cache_ptr->entrypoints_private_ptr->pGLPolygonOffset(cache_ptr->active_polygon_offset_factor_local,
                                                                 cache_ptr->active_polygon_offset_units_local);

            cache_ptr->active_polygon_offset_factor_context = cache_ptr->active_polygon_offset_factor_local;
            cache_ptr->active_polygon_offset_units_context  = cache_ptr->active_polygon_offset_units_local;
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

            if (cache_ptr->active_rendering_mode_polygon_offset_point_context != cache_ptr->active_rendering_mode_polygon_offset_point_local)
            {
                UPDATE_STATE(cache_ptr->active_rendering_mode_polygon_offset_point_context,
                             cache_ptr->active_rendering_mode_polygon_offset_point_local,
                             GL_POLYGON_OFFSET_POINT);
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
        }

        /* Scissor box */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX)
        {
            for (int32_t n_scissor_box = 0;
                         n_scissor_box < cache_ptr->limits_ptr->max_viewports;
                       ++n_scissor_box)
            {
                if (memcmp(cache_ptr->active_scissor_boxes_context + n_scissor_box * 4,
                           cache_ptr->active_scissor_boxes_local   + n_scissor_box * 4,
                           4 * sizeof(GLint) ) != 0)
                {
                    cache_ptr->entrypoints_private_ptr->pGLScissorIndexedv(n_scissor_box,
                                                                           cache_ptr->active_scissor_boxes_local + (n_scissor_box * 4) );

                    memcpy(cache_ptr->active_scissor_boxes_context + n_scissor_box * 4,
                           cache_ptr->active_scissor_boxes_local   + n_scissor_box * 4,
                           sizeof(GLint) * 4);
                }
            }
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
            ogl_vao          current_vao             = nullptr;
            GLuint           indexed_binding_context = -1;
            GLuint           indexed_binding_local   = -1;
            ogl_context_vaos vaos                    = nullptr;

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
            }
        }

        /* Viewport */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT)
        {
            for (int32_t n_viewport = 0;
                         n_viewport < cache_ptr->limits_ptr->max_viewports;
                       ++n_viewport)
            {
                if (memcmp(cache_ptr->active_depth_ranges_context + n_viewport * 2,
                           cache_ptr->active_depth_ranges_local   + n_viewport * 2,
                           2 * sizeof(GLdouble) ) != 0)
                {
                    cache_ptr->entrypoints_private_ptr->pGLDepthRangeIndexed(n_viewport,
                                                                             cache_ptr->active_depth_ranges_local[n_viewport * 2],
                                                                             cache_ptr->active_depth_ranges_local[n_viewport * 2 + 1]);

                    memcpy(cache_ptr->active_depth_ranges_context + n_viewport * 2,
                           cache_ptr->active_depth_ranges_local   + n_viewport * 2,
                           sizeof(GLdouble) * 2);
                }

                if (memcmp(cache_ptr->active_viewports_context + n_viewport * 4,
                           cache_ptr->active_viewports_local   + n_viewport * 4,
                           4 * sizeof(GLint) ) != 0)
                {
                    cache_ptr->entrypoints_private_ptr->pGLViewportIndexedfv(n_viewport,
                                                                             cache_ptr->active_viewports_local + (n_viewport * 4) );

                    memcpy(cache_ptr->active_viewports_context + n_viewport * 4,
                           cache_ptr->active_viewports_local   + n_viewport * 4,
                           sizeof(GLfloat) * 4);
                }
            }
        }
    }
}