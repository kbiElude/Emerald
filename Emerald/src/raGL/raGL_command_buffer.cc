/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_state_cache.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_command_buffer.h"
#include "raGL/raGL_dep_tracker.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_framebuffers.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_utils.h"
#include "raGL/raGL_vaos.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_program.h"
#include "ral/ral_sampler.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_resource_pool.h"
#include "system/system_resizable_vector.h"

#define N_MAX_DRAW_BUFFERS      (8)
#define N_MAX_IMAGE_UNITS       (8)
#define N_MAX_RENDERTARGETS     (8)
#define N_MAX_SB_BINDINGS       (8)
#define N_MAX_TEXTURE_UNITS     (32) /* NOTE: GL 4.5 offers a min max of 80 */
#define N_MAX_UB_BINDINGS       (16)
#define N_MAX_VERTEX_ATTRIBUTES (16)
#define N_MAX_VIEWPORTS         (8)


typedef enum
{
    /* Command args stored in _raGL_command_active_texture_command_info */
    RAGL_COMMAND_TYPE_ACTIVE_TEXTURE,

    /* Command args stored in _raGL_command_bind_buffer_command_info */
    RAGL_COMMAND_TYPE_BIND_BUFFER,

    /* Command args stored in _raGL_command_bind_buffer_base_command_info */
    RAGL_COMMAND_TYPE_BIND_BUFFER_BASE,

    /* Command args stored in _raGL_command_bind_buffer_range_command_info */
    RAGL_COMMAND_TYPE_BIND_BUFFER_RANGE,

    /* Command args stored in _raGL_command_bind_framebuffer_command_info */
    RAGL_COMMAND_TYPE_BIND_FRAMEBUFFER,

    /* Command args stored in _raGL_command_bind_image_texture_command_info */
    RAGL_COMMAND_TYPE_BIND_IMAGE_TEXTURE,

    /* Command args stored in _raGL_command_bind_sampler_command_info */
    RAGL_COMMAND_TYPE_BIND_SAMPLER,

    /* Command args stored in _raGL_command_bind_texture_command_info */
    RAGL_COMMAND_TYPE_BIND_TEXTURE,

    /* Command args stored in _raGL_command_bind_vertex_array_command_info */
    RAGL_COMMAND_TYPE_BIND_VERTEX_ARRAY,

    /* Command arsg stored in _raGL_command_blend_color_command_info */
    RAGL_COMMAND_TYPE_BLEND_COLOR,

    /* Command args stored in _raGL_command_blend_equation_separate_command_info */
    RAGL_COMMAND_TYPE_BLEND_EQUATION_SEPARATE,

    /* Command args stored in _raGL_command_blend_func_separate_command_info */
    RAGL_COMMAND_TYPE_BLEND_FUNC_SEPARATE,

    /* Command args stored in _raGL_command_blit_framebuffer_command_info */
    RAGL_COMMAND_TYPE_BLIT_FRAMEBUFFER,

    /* Command args stored in _raGL_command_clear_command_info */
    RAGL_COMMAND_TYPE_CLEAR,

    /* Command args stored in _raGL_command_clear_buffer_sub_data_command_info */
    RAGL_COMMAND_TYPE_CLEAR_BUFFER_SUB_DATA,

    /* Command args stored in _raGL_command_clear_color_command_info */
    RAGL_COMMAND_TYPE_CLEAR_COLOR,

    /* Command args stored in _raGL_command_clear_depthf_command_info */
    RAGL_COMMAND_TYPE_CLEAR_DEPTHF,

    /* Command args stored in _raGL_command_clear_stencil_command_info */
    RAGL_COMMAND_TYPE_CLEAR_STENCIL,

    /* Command args stored in _raGL_command_copy_buffer_sub_data_command_info */
    RAGL_COMMAND_TYPE_COPY_BUFFER_SUB_DATA,

    /* Command args stored in _raGL_command_copy_image_sub_data_command_info */
    RAGL_COMMAND_TYPE_COPY_IMAGE_SUB_DATA,

    /* Command args stored in _raGL_command_cull_face_command_info */
    RAGL_COMMAND_TYPE_CULL_FACE,

    /* Command args stored in _raGL_command_depth_func_command_info */
    RAGL_COMMAND_TYPE_DEPTH_FUNC,

    /* Command args stored in _raGL_command_depth_mask_command_info */
    RAGL_COMMAND_TYPE_DEPTH_MASK,

    /* Command args stored in _raGL_command_depth_range_indexed_command_info */
    RAGL_COMMAND_TYPE_DEPTH_RANGE_INDEXED,

    /* Command args stored in _raGL_command_disable_command_info */
    RAGL_COMMAND_TYPE_DISABLE,

    /* Command args stored in _raGL_command_disable_vertex_attrib_array_command_info */
    RAGL_COMMAND_TYPE_DISABLE_VERTEX_ATTRIB_ARRAY,

    /* Command args stored in _raGL_command_dispatch_command_info */
    RAGL_COMMAND_TYPE_DISPATCH,

    /* Command args stored in _raGL_command_draw_arrays_command_info */
    RAGL_COMMAND_TYPE_DRAW_ARRAYS,

    /* Command args stored in _raGL_command_draw_arrays_indirect_command_info */
    RAGL_COMMAND_TYPE_DRAW_ARRAYS_INDIRECT,

    /* Command args stored in _raGL_command_draw_arrays_instanced_command_info */
    RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED,

    /* Command args stored in _raGL_command_draw_arrays_instanced_base_instance_command_info */
    RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE,

    /* Command args stored in _raGL_command_draw_buffers_command_info */
    RAGL_COMMAND_TYPE_DRAW_BUFFERS,

    /* Command args stored in _raGL_command_draw_elements_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS,

    /* Command args stored in _raGL_command_draw_elements_base_vertex_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS_BASE_VERTEX,

    /* Command args stored in _raGL_command_draw_elements_indirect_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INDIRECT,

    /* Command args stored in _raGL_command_draw_elements_instanced_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED,

    /* Command args stored in _raGL_command_draw_elements_instanced_base_instance_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_INSTANCE,

    /* Command args stored in _raGL_command_draw_elements_instanced_base_vertex_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX,

    /* Command args stored in _raGL_command_draw_elements_instanced_base_vertex_base_instance_command_info */
    RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE,

    /* Command args stored in _raGL_command_enable_command_info */
    RAGL_COMMAND_TYPE_ENABLE,

    /* Command args stored in _raGL_command_enable_vertex_attrib_array_command_info */
    RAGL_COMMAND_TYPE_ENABLE_VERTEX_ATTRIB_ARRAY,

    /* Command args stored in _raGL_command_execute_command_buffer_command_info */
    RAGL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER,

    /* Command args stored in _raGL_command_front_face_command_info */
    RAGL_COMMAND_TYPE_FRONT_FACE,

    /* Command args stored in _raGL_command_invalidate_tex_image_command_info */
    RAGL_COMMAND_TYPE_INVALIDATE_TEX_IMAGE,

    /* Command args stored in _raGL_command_line_width_command_info */
    RAGL_COMMAND_TYPE_LINE_WIDTH,

    /* Command args stored in _raGL_command_logic_op_command_info */
    RAGL_COMMAND_TYPE_LOGIC_OP,

    /* Command args stored in _raGL_command_memory_barrier_command_info */
    RAGL_COMMAND_TYPE_MEMORY_BARRIER,

    /* Command args stored in _raGL_command_min_sample_shading_command_info */
    RAGL_COMMAND_TYPE_MIN_SAMPLE_SHADING,

    /* Command args stored in _raGL_command_multi_draw_array_indirect_command_info */
    RAGL_COMMAND_TYPE_MULTI_DRAW_ARRAYS_INDIRECT,

    /* Command args stored in _raGL_command_multi_draw_elements_indirect_command_info */
    RAGL_COMMAND_TYPE_MULTI_DRAW_ELEMENTS_INDIRECT,

    /* Command args stored in _raGL_buffer_named_buffer_sub_data_command_info */
    RAGL_COMMAND_TYPE_NAMED_BUFFER_SUB_DATA,

        /* Command args stored in _raGL_command_patch_parameteri_command_info */
    RAGL_COMMAND_TYPE_PATCH_PARAMETERI,

    /* Command args stored in _raGL_command_polygon_mode_command_info */
    RAGL_COMMAND_TYPE_POLYGON_MODE,

    /* Command args stored in _raGL_command_polygon_offset_command_info */
    RAGL_COMMAND_TYPE_POLYGON_OFFSET,

    /* Command args stored in _raGL_command_scissor_indexedv_command_info */
    RAGL_COMMAND_TYPE_SCISSOR_INDEXEDV,

    /* Command args stored in _raGL_command_stencil_func_separate_command_info */
    RAGL_COMMAND_TYPE_STENCIL_FUNC_SEPARATE,

    /* Command args stored in _raGL_command_stencil_mask_separate_command_info */
    RAGL_COMMAND_TYPE_STENCIL_MASK_SEPARATE,

    /* Command args stored in _raGL_command_stencil_op_separate_command_info */
    RAGL_COMMAND_TYPE_STENCIL_OP_SEPARATE,

    /* Command args stored in _raGL_command_texture_parameterfv_command_info */
    RAGL_COMMAND_TYPE_TEXTURE_PARAMETERFV,

    /* Command args stored in _raGL_command_use_program_command_info */
    RAGL_COMMAND_TYPE_USE_PROGRAM,

    /* Command args stored in _raGL_command_viewport_indexedfv_command_info */
    RAGL_COMMAND_TYPE_VIEWPORT_INDEXEDFV
} raGL_command_type;


typedef struct
{
    GLenum target;

} _raGL_command_active_texture_command_info;

typedef struct
{
    GLuint bo_id;
    GLuint bp_index;
    GLenum target;

} _raGL_command_bind_buffer_base_command_info;

typedef struct
{
    GLuint bo_id;
    GLenum target;

} _raGL_command_bind_buffer_command_info;

typedef struct
{
    GLuint     bo_id;
    GLuint     bp_index;
    GLintptr   offset;
    GLsizeiptr size;
    GLenum     target;

} _raGL_command_bind_buffer_range_command_info;

typedef struct
{
    GLuint framebuffer;
    GLenum target;

} _raGL_command_bind_framebuffer_command_info;

typedef struct
{
    GLenum access;
    GLenum format;
    bool   is_layered;
    GLint  layer;
    GLint  level;
    GLuint to_id;
    GLuint tu_index;

} _raGL_command_bind_image_texture_command_info;

typedef struct
{
    GLuint sampler_id;
    GLuint unit;

} _raGL_command_bind_sampler_command_info;

typedef struct
{
    GLenum target;
    GLuint to_id;

} _raGL_command_bind_texture_command_info;

typedef struct
{
    GLuint vao_id;

} _raGL_command_bind_vertex_array_command_info;

typedef struct
{
    GLfloat rgba[4];

} _raGL_command_blend_color_command_info;

typedef struct
{
    GLenum modeRGB;
    GLenum modeAlpha;

} _raGL_command_blend_equation_separate_command_info;

typedef struct
{
    GLenum srcRGB;
    GLenum dstRGB;
    GLenum srcAlpha;
    GLenum dstAlpha;

} _raGL_command_blend_func_separate_command_info;

typedef struct
{
    GLuint     draw_fbo_id;
    GLint      dst_x0y0x1y1[4];
    GLenum     filter;
    GLbitfield mask;
    GLuint     read_fbo_id;
    GLint      src_x0y0x1y1[4];

} _raGL_command_blit_framebuffer_command_info;

typedef struct
{
    GLuint     bo_id;
    char32_t   data[4];
    GLenum     format;
    GLenum     internalformat;
    GLsizeiptr size;
    GLintptr   start_offset;
    GLenum     target;
    GLenum     type;

} _raGL_command_clear_buffer_sub_data_command_info;

typedef struct
{
    GLbitfield mask;

} _raGL_command_clear_command_info;

typedef struct
{
    GLfloat rgba[4];

} _raGL_command_clear_color_command_info;

typedef struct
{
    GLfloat depth;

} _raGL_command_clear_depthf_command_info;

typedef struct
{
    GLint stencil;

} _raGL_command_clear_stencil_command_info;

typedef struct
{
    GLintptr   read_offset;
    GLenum     read_target;
    GLsizeiptr size;
    GLintptr   write_offset;
    GLenum     write_target;

} _raGL_command_copy_buffer_sub_data_command_info;

typedef struct
{
    GLint   dst_level;
    GLenum  dst_object_id;
    GLenum  dst_target;
    GLint   dst_xyz[3];
    GLsizei size[3];
    GLint   src_level;
    GLenum  src_object_id;
    GLenum  src_target;
    GLint   src_xyz[3];

} _raGL_command_copy_image_sub_data_command_info;

typedef struct
{
    GLenum mode;

} _raGL_command_cull_face_command_info;

typedef struct
{
    GLenum func;

} _raGL_command_depth_func_command_info;

typedef struct
{
    GLboolean flag;

} _raGL_command_depth_mask_command_info;

typedef struct
{
    GLuint   index;
    GLdouble nearVal;
    GLdouble farVal;

} _raGL_command_depth_range_indexed_command_info;

typedef struct
{
    GLenum capability;

} _raGL_command_disable_command_info;

typedef struct
{
    GLuint index;

} _raGL_command_disable_vertex_attrib_array_command_info;

typedef struct
{
    GLuint x;
    GLuint y;
    GLuint z;

} _raGL_command_dispatch_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLsizei count;
    GLint   first;

} _raGL_command_draw_arrays_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLvoid* indirect;

} _raGL_command_draw_arrays_indirect_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLsizei count;
    GLint   first;
    GLsizei primcount;

} _raGL_command_draw_arrays_instanced_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLuint  base_instance;
    GLsizei count;
    GLint   first;
    GLsizei primcount;

} _raGL_command_draw_arrays_instanced_base_instance_command_info;

typedef struct
{
    GLenum  bufs[N_MAX_DRAW_BUFFERS];
    GLsizei n;

} _raGL_command_draw_buffers_command_info;

typedef struct
{
    GLint    base_vertex;
    GLsizei  count;
    GLvoid*  indices;
    GLenum   type;
} _raGL_command_draw_elements_base_vertex_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLsizei count;
    GLvoid* indices;
    GLenum  type;
} _raGL_command_draw_elements_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLenum  type;
} _raGL_command_draw_elements_indirect_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLsizei count;
    GLvoid* indices;
    GLsizei primcount;
    GLenum  type;
} _raGL_command_draw_elements_instanced_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLuint  base_instance;
    GLsizei count;
    GLvoid* indices;
    GLsizei primcount;
    GLenum  type;
} _raGL_command_draw_elements_instanced_base_instance_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLuint  base_instance;
    GLint   base_vertex;
    GLsizei count;
    GLvoid* indices;
    GLsizei primcount;
    GLenum  type;
} _raGL_command_draw_elements_instanced_base_vertex_base_instance_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLint   base_vertex;
    GLsizei count;
    GLvoid* indices;
    GLsizei primcount;
    GLenum  type;
} _raGL_command_draw_elements_instanced_base_vertex_command_info;

typedef struct
{
    GLenum capability;

} _raGL_command_enable_command_info;

typedef struct
{
    GLuint index;

} _raGL_command_enable_vertex_attrib_array_command_info;

typedef struct
{
    raGL_command_buffer command_buffer_raGL;

} _raGL_command_execute_command_buffer_command_info;

typedef struct
{
    GLenum mode;

} _raGL_command_front_face_command_info;

typedef struct
{
    GLint  level;
    GLuint texture;

} _raGL_command_invalidate_tex_image_command_info;

typedef struct
{
    float width;

} _raGL_command_line_width_command_info;

typedef struct
{
    GLenum opcode;

} _raGL_command_logic_op_command_info;

typedef struct
{
    GLbitfield barriers;

} _raGL_command_memory_barrier_command_info;

typedef struct
{
    GLfloat value;

} _raGL_command_min_sample_shading_command_info;

typedef struct
{
    // mode - defined by bound gfx state
    GLsizei drawcount;
    GLvoid* indirect;
    GLsizei stride;

} _raGL_command_multi_draw_arrays_indirect_command_info;

typedef struct
{
    GLsizei drawcount;
    GLvoid* indirect;
    GLsizei stride;
    GLenum  type;

} _raGL_command_multi_draw_elements_indirect_command_info;

typedef struct
{
    GLuint      bo_id;
    GLintptr    offset;
    GLsizeiptr  size;
    const void* data;

} _raGL_command_named_buffer_sub_data_command_info;

typedef struct
{
    GLenum pname;
    GLint  value;

} _raGL_command_patch_parameteri_command_info;

typedef struct
{
    GLenum face;
    GLenum mode;

} _raGL_command_polygon_mode_command_info;

typedef struct
{
    GLfloat factor;
    GLfloat units;

} _raGL_command_polygon_offset_command_info;

typedef struct
{
    GLuint index;
    GLint  v[4];

} _raGL_command_scissor_indexedv_command_info;

typedef struct
{
    GLenum face;
    GLenum func;
    GLint  ref;
    GLuint mask;

} _raGL_command_stencil_func_separate_command_info;

typedef struct
{
    GLenum face;
    GLuint mask;

} _raGL_command_stencil_mask_separate_command_info;

typedef struct
{
    GLenum face;
    GLenum sfail;
    GLenum dpfail;
    GLenum dppass;

} _raGL_command_stencil_op_separate_command_info;

typedef struct
{
    GLuint  texture;
    GLenum  target;
    GLenum  pname;
    GLfloat value[4];

} _raGL_command_texture_parameterfv_command_info;

typedef struct
{
    GLuint po_id;

} _raGL_command_use_program_command_info;

typedef struct
{
    GLuint  index;
    GLfloat v[4];

} _raGL_command_viewport_indexedfv_command_info;

typedef struct
{
    raGL_command_type type;

    union
    {
        _raGL_command_active_texture_command_info                                    active_texture_command_info;
        _raGL_command_bind_buffer_command_info                                       bind_buffer_command_info;
        _raGL_command_bind_buffer_base_command_info                                  bind_buffer_base_command_info;
        _raGL_command_bind_buffer_range_command_info                                 bind_buffer_range_command_info;
        _raGL_command_bind_framebuffer_command_info                                  bind_framebuffer_command_info;
        _raGL_command_bind_image_texture_command_info                                bind_image_texture_command_info;
        _raGL_command_bind_sampler_command_info                                      bind_sampler_command_info;
        _raGL_command_bind_texture_command_info                                      bind_texture_command_info;
        _raGL_command_bind_vertex_array_command_info                                 bind_vertex_array_command_info;
        _raGL_command_blend_color_command_info                                       blend_color_command_info;
        _raGL_command_blend_equation_separate_command_info                           blend_equation_separate_command_info;
        _raGL_command_blend_func_separate_command_info                               blend_func_separate_command_info;
        _raGL_command_blit_framebuffer_command_info                                  blit_framebuffer_command_info;
        _raGL_command_clear_command_info                                             clear_command_info;
        _raGL_command_clear_buffer_sub_data_command_info                             clear_buffer_sub_data_command_info;
        _raGL_command_clear_color_command_info                                       clear_color_command_info;
        _raGL_command_clear_depthf_command_info                                      clear_depthf_command_info;
        _raGL_command_clear_stencil_command_info                                     clear_stencil_command_info;
        _raGL_command_copy_buffer_sub_data_command_info                              copy_buffer_sub_data_command_info;
        _raGL_command_copy_image_sub_data_command_info                               copy_image_sub_data_command_info;
        _raGL_command_cull_face_command_info                                         cull_face_command_info;
        _raGL_command_depth_func_command_info                                        depth_func_command_info;
        _raGL_command_depth_mask_command_info                                        depth_mask_command_info;
        _raGL_command_depth_range_indexed_command_info                               depth_range_indexed_command_info;
        _raGL_command_disable_command_info                                           disable_command_info;
        _raGL_command_disable_vertex_attrib_array_command_info                       disable_vertex_attrib_array_command_info;
        _raGL_command_dispatch_command_info                                          dispatch_command_info;
        _raGL_command_draw_arrays_command_info                                       draw_arrays_command_info;
        _raGL_command_draw_arrays_indirect_command_info                              draw_arrays_indirect_command_info;
        _raGL_command_draw_arrays_instanced_base_instance_command_info               draw_arrays_instanced_base_instance_command_info;
        _raGL_command_draw_arrays_instanced_command_info                             draw_arrays_instanced_command_info;
        _raGL_command_draw_buffers_command_info                                      draw_buffers_command_info;
        _raGL_command_draw_elements_base_vertex_command_info                         draw_elements_base_vertex_command_info;
        _raGL_command_draw_elements_command_info                                     draw_elements_command_info;
        _raGL_command_draw_elements_indirect_command_info                            draw_elements_indirect_command_info;
        _raGL_command_draw_elements_instanced_base_instance_command_info             draw_elements_instanced_base_instance_command_info;
        _raGL_command_draw_elements_instanced_base_vertex_base_instance_command_info draw_elements_instanced_base_vertex_base_instance_command_info;
        _raGL_command_draw_elements_instanced_base_vertex_command_info               draw_elements_instanced_base_vertex_command_info;
        _raGL_command_draw_elements_instanced_command_info                           draw_elements_instanced_command_info;
        _raGL_command_enable_command_info                                            enable_command_info;
        _raGL_command_enable_vertex_attrib_array_command_info                        enable_vertex_attrib_array_command_info;
        _raGL_command_execute_command_buffer_command_info                            execute_command_buffer_command_info;
        _raGL_command_front_face_command_info                                        front_face_command_info;
        _raGL_command_invalidate_tex_image_command_info                              invalidate_tex_image_command_info;
        _raGL_command_line_width_command_info                                        line_width_command_info;
        _raGL_command_logic_op_command_info                                          logic_op_command_info;
        _raGL_command_memory_barrier_command_info                                    memory_barriers_command_info;
        _raGL_command_min_sample_shading_command_info                                min_sample_shading_command_info;
        _raGL_command_multi_draw_arrays_indirect_command_info                        multi_draw_arrays_indirect_command_info;
        _raGL_command_multi_draw_elements_indirect_command_info                      multi_draw_elements_indirect_command_info;
        _raGL_command_named_buffer_sub_data_command_info                             named_buffer_sub_data_command_info;
        _raGL_command_patch_parameteri_command_info                                  patch_parameteri_command_info;
        _raGL_command_polygon_mode_command_info                                      polygon_mode_command_info;
        _raGL_command_polygon_offset_command_info                                    polygon_offset_command_info;
        _raGL_command_scissor_indexedv_command_info                                  scissor_indexedv_command_info;
        _raGL_command_stencil_func_separate_command_info                             stencil_func_separate_command_info;
        _raGL_command_stencil_mask_separate_command_info                             stencil_mask_separate_command_info;
        _raGL_command_stencil_op_separate_command_info                               stencil_op_separate_command_info;
        _raGL_command_texture_parameterfv_command_info                               texture_parameterfv_command_info;
        _raGL_command_use_program_command_info                                       use_program_command_info;
        _raGL_command_viewport_indexedfv_command_info                                viewport_indexedfv_command_info;
    };

    void deinit()
    {
        /* Stub */
    }

} _raGL_command;

typedef struct _raGL_command_buffer_bake_state_buffer_binding
{
    raGL_buffer buffer;

    _raGL_command_buffer_bake_state_buffer_binding()
    {
        buffer = nullptr;
    }

} _raGL_command_buffer_bake_state_buffer_binding;

typedef struct _raGL_command_buffer_bake_state_rt
{
    ral_color        blend_constant;
    bool             blend_enabled;
    ral_blend_op     blend_op_alpha;
    ral_blend_op     blend_op_color;
    bool             color_writes[4];
    ral_blend_factor dst_alpha_blend_factor;
    ral_blend_factor dst_color_blend_factor;
    ral_blend_factor src_alpha_blend_factor;
    ral_blend_factor src_color_blend_factor;
    ral_texture_view texture_view;

    explicit _raGL_command_buffer_bake_state_rt(const ral_command_buffer_set_color_rendertarget_command_info& set_rt_command)
    {
        blend_constant            = set_rt_command.blend_constant;
        blend_enabled             = set_rt_command.blend_enabled;
        blend_op_alpha            = set_rt_command.blend_op_alpha;
        blend_op_color            = set_rt_command.blend_op_color;
        color_writes[0]           = set_rt_command.channel_writes.color0;
        color_writes[1]           = set_rt_command.channel_writes.color1;
        color_writes[2]           = set_rt_command.channel_writes.color2;
        color_writes[3]           = set_rt_command.channel_writes.color3;
        dst_alpha_blend_factor    = set_rt_command.dst_alpha_blend_factor;
        dst_color_blend_factor    = set_rt_command.dst_color_blend_factor;
        src_alpha_blend_factor    = set_rt_command.src_alpha_blend_factor;
        src_color_blend_factor    = set_rt_command.src_color_blend_factor;
        texture_view              = set_rt_command.texture_view;
    }

    _raGL_command_buffer_bake_state_rt()
    {
        blend_enabled  = false;
        texture_view   = nullptr;
    }

} _raGL_command_buffer_bake_state_rt;

typedef struct _raGL_command_buffer_bake_state_scissor_box
{
    GLsizei size[2];
    GLint   xy[2];

    explicit _raGL_command_buffer_bake_state_scissor_box(const GLsizei* in_size,
                                                         const GLint*   in_xy)
    {
        memcpy(size,
               in_size,
               sizeof(size) );
        memcpy(xy,
               in_xy,
               sizeof(xy) );
    }

    _raGL_command_buffer_bake_state_scissor_box()
    {
        memset(size,
               0,
               sizeof(size) );
        memset(xy,
               0,
               sizeof(xy) );
    }
} _raGL_command_buffer_bake_state_scissor_box;

typedef struct
{
    raGL_program active_program;

    ral_gfx_state active_gfx_state;
    bool          active_gfx_state_dirty;

    GLenum                                         active_fbo_draw_buffers[N_MAX_DRAW_BUFFERS];
    bool                                           active_fbo_draw_buffers_dirty;
    raGL_framebuffer                               active_fbo_draw;
    raGL_texture                                   active_image_bindings      [N_MAX_IMAGE_UNITS];
    bool                                           active_rt_attachments_dirty;
    _raGL_command_buffer_bake_state_rt             active_rt_color_attachments[N_MAX_RENDERTARGETS];
    ral_texture_view                               active_rt_ds_attachment;
    _raGL_command_buffer_bake_state_buffer_binding active_sb_bindings             [N_MAX_SB_BINDINGS];
    _raGL_command_buffer_bake_state_scissor_box    active_scissor_boxes           [N_MAX_VIEWPORTS];
    raGL_texture                                   active_texture_sampler_bindings[N_MAX_TEXTURE_UNITS];
    _raGL_command_buffer_bake_state_buffer_binding active_ub_bindings             [N_MAX_UB_BINDINGS];

    bool                    vao_dirty;
    raGL_buffer             vao_index_buffer;
    raGL_vaos_vertex_buffer vbs[N_MAX_VERTEX_ATTRIBUTES];

    void reset()
    {
        memset(active_image_bindings,
               0,
               sizeof(active_image_bindings) );
        memset(active_sb_bindings,
               0,
               sizeof(active_sb_bindings) );
        memset(active_texture_sampler_bindings,
               0,
               sizeof(active_texture_sampler_bindings) );
        memset(active_ub_bindings,
               0,
               sizeof(active_ub_bindings) );
        memset(vbs,
               0,
               sizeof(vbs) );

        for (uint32_t n_draw_buffer = 0;
                      n_draw_buffer < N_MAX_DRAW_BUFFERS;
                    ++n_draw_buffer)
        {
            /* As per spec */
            active_fbo_draw_buffers[n_draw_buffer] = (n_draw_buffer == 0) ? GL_COLOR_ATTACHMENT0
                                                                          : GL_NONE;
        }

        for (uint32_t n_rt = 0;
                      n_rt < sizeof(active_rt_color_attachments) / sizeof(active_rt_color_attachments[0]);
                    ++n_rt)
        {
            active_rt_color_attachments[n_rt] = _raGL_command_buffer_bake_state_rt();
        }

        active_fbo_draw_buffers_dirty = true;
        active_fbo_draw               = nullptr;
        active_gfx_state              = nullptr;
        active_gfx_state_dirty        = false;
        active_program                = nullptr;
        active_rt_attachments_dirty   = true;
        active_rt_ds_attachment       = nullptr;
        vao_dirty                     = true; /* GL requires a VAO to be bound at all times */
        vao_index_buffer              = nullptr;
    }
} _raGL_command_buffer_bake_state;

typedef struct _raGL_command_buffer
{
    _raGL_command_buffer_bake_state                           bake_state;
    ral_command_buffer                                        command_buffer_ral;
    system_resizable_vector                                   commands;
    raGL_dep_tracker                                          dep_tracker;
    ogl_context                                               context; /* do NOT release */
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr;
    const ogl_context_gl_limits*                              limits_ptr;


    void init(ogl_context in_context)
    {
        raGL_backend backend = nullptr;

        if (commands == nullptr)
        {
            commands = system_resizable_vector_create(32); /* capacity */
        }
        else
        {
            uint32_t n_commands = 0;

            system_resizable_vector_get_property(commands,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_commands);

            ASSERT_DEBUG_SYNC(n_commands == 0,
                              "Existing command container is not empty");
        }

        command_buffer_ral = nullptr;
        context            = in_context;

        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_BACKEND,
                                &backend);
        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);
        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &entrypoints_dsa_ptr);
        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        raGL_backend_get_private_property(backend,
                                          RAGL_BACKEND_PRIVATE_PROPERTY_DEP_TRACKER,
                                         &dep_tracker);
    }

    void bake_and_bind_fbo                     ();
    void bake_and_bind_vao                     ();
    void bake_gfx_state                        ();
    void bake_pre_dispatch_draw_memory_barriers();
    void bake_rt_state                         ();

    void bake_commands();
    void clear_commands();

    void process_clear_rt_binding_command       (const ral_command_buffer_clear_rt_binding_command_info*        command_ral_ptr);
    void process_copy_buffer_to_buffer_command  (const ral_command_buffer_copy_buffer_to_buffer_command_info*   command_ral_ptr);
    void process_copy_texture_to_texture_command(const ral_command_buffer_copy_texture_to_texture_command_info* command_ral_ptr);
    void process_dispatch_command               (const ral_command_buffer_dispatch_command_info*                command_ral_ptr);
    void process_draw_call_indexed_command      (const ral_command_buffer_draw_call_indexed_command_info*       command_ral_ptr);
    void process_draw_call_indirect_command     (const ral_command_buffer_draw_call_indirect_command_info*      command_ral_ptr);
    void process_draw_call_regular_command      (const ral_command_buffer_draw_call_regular_command_info*       command_ral_ptr);
    void process_execute_command_buffer_command (const ral_command_buffer_execute_command_buffer_command_info*  command_ral_ptr);
    void process_fill_buffer_command            (const ral_command_buffer_fill_buffer_command_info*             command_ral_ptr);
    void process_invalidate_texture_command     (const ral_command_buffer_invalidate_texture_command_info*      command_ral_ptr);
    void process_set_binding_command            (const ral_command_buffer_set_binding_command_info*             command_ral_ptr);
    void process_set_gfx_state_command          (const ral_command_buffer_set_gfx_state_command_info*           command_ral_ptr);
    void process_set_program_command            (const ral_command_buffer_set_program_command_info*             command_ral_ptr);
    void process_set_color_rendertarget_command (const ral_command_buffer_set_color_rendertarget_command_info*  command_ral_ptr);
    void process_set_depth_rendertarget_command (const ral_command_buffer_set_depth_rendertarget_command_info*  command_ral_ptr);
    void process_set_scissor_box_command        (const ral_command_buffer_set_scissor_box_command_info*         command_ral_ptr);
    void process_set_vertex_buffer_command      (const ral_command_buffer_set_vertex_buffer_command_info*       command_ral_ptr);
    void process_set_viewport_command           (const ral_command_buffer_set_viewport_command_info*            command_ral_ptr);
    void process_update_buffer_command          (const ral_command_buffer_update_buffer_command_info*           command_ral_ptr);

} _raGL_command_buffer;

PRIVATE system_resource_pool command_buffer_pool = nullptr;
PRIVATE system_resource_pool command_pool        = nullptr;


/** TODO */
void _raGL_command_buffer::bake_and_bind_fbo()
{
    raGL_backend      backend_gl  = nullptr;
    raGL_framebuffer  fbo_raGL    = nullptr;
    GLuint            fbo_raGL_id = 0;
    raGL_framebuffers fbos        = nullptr;

    ogl_context_get_property         (context,
                                      OGL_CONTEXT_PROPERTY_BACKEND,
                                     &backend_gl);
    raGL_backend_get_private_property(backend_gl,
                                      RAGL_BACKEND_PRIVATE_PROPERTY_FBOS,
                                     &fbos);

    /* Retrieve a FBO. If none is available, this function will bake one right now,
     * or return a cached one otherwise. */
    ral_texture_view color_attachments[N_MAX_RENDERTARGETS];
    ral_texture_view ds_attachment                          = bake_state.active_rt_ds_attachment;

    for (uint32_t n_color_attachment = 0;
                  n_color_attachment < N_MAX_RENDERTARGETS;
                ++n_color_attachment)
    {
        color_attachments[n_color_attachment] = bake_state.active_rt_color_attachments[n_color_attachment].texture_view;
    }

    raGL_framebuffers_get_framebuffer(fbos,
                                      N_MAX_RENDERTARGETS,
                                      color_attachments,
                                      ds_attachment,
                                     &fbo_raGL);

    ASSERT_DEBUG_SYNC(fbo_raGL != nullptr,
                      "Could not retrieve a FBO with the specified attachment configuration");

    bake_state.active_fbo_draw = fbo_raGL;

    raGL_framebuffer_get_property(fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &fbo_raGL_id);

    /* Bind the framebuffer */
    {
        _raGL_command* bind_fb_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

        bind_fb_command_ptr->bind_framebuffer_command_info.framebuffer = fbo_raGL_id;
        bind_fb_command_ptr->bind_framebuffer_command_info.target      = GL_DRAW_FRAMEBUFFER;
        bind_fb_command_ptr->type                                      = RAGL_COMMAND_TYPE_BIND_FRAMEBUFFER;

        system_resizable_vector_push(commands,
                                     bind_fb_command_ptr);
    }

    /* Update the draw buffers if needed */
    if (bake_state.active_fbo_draw_buffers_dirty)
    {
        _raGL_command* draw_buffers_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

        memcpy(draw_buffers_command_ptr->draw_buffers_command_info.bufs,
               bake_state.active_fbo_draw_buffers,
               sizeof(bake_state.active_fbo_draw_buffers) );

        draw_buffers_command_ptr->draw_buffers_command_info.n = N_MAX_DRAW_BUFFERS;
        draw_buffers_command_ptr->type                        = RAGL_COMMAND_TYPE_DRAW_BUFFERS;

        system_resizable_vector_push(commands,
                                     draw_buffers_command_ptr);

        bake_state.active_fbo_draw_buffers_dirty = false;
    }

}

/** TODO */
void _raGL_command_buffer::bake_and_bind_vao()
{
    raGL_backend                    backend_gl = nullptr;
    uint32_t                        n_vas      = 0;
    uint32_t                        n_vbs      = 0;
    raGL_vao                        vao        = nullptr;
    GLuint                          vao_id     = 0;
    raGL_vaos                       vaos       = nullptr;
    ral_gfx_state_vertex_attribute* vas        = nullptr;

    ogl_context_get_property         (context,
                                      OGL_CONTEXT_PROPERTY_BACKEND,
                                     &backend_gl);
    raGL_backend_get_private_property(backend_gl,
                                      RAGL_BACKEND_PRIVATE_PROPERTY_VAOS,
                                     &vaos);

    ral_gfx_state_get_property(bake_state.active_gfx_state,
                               RAL_GFX_STATE_PROPERTY_N_VERTEX_ATTRIBUTES,
                              &n_vas);
    ral_gfx_state_get_property(bake_state.active_gfx_state,
                               RAL_GFX_STATE_PROPERTY_VERTEX_ATTRIBUTES,
                              &vas);

    /* Count the number of VBS used */
    bool null_vb_entry_found = false;

    for (uint32_t n_vb = 0;
                  n_vb < sizeof(bake_state.vbs) / sizeof(bake_state.vbs[0]);
                ++n_vb)
    {
        if (bake_state.vbs[n_vb].buffer_raGL != nullptr)
        {
            /* TODO: We need a "VB" reset command to reset all VB bindings */
            ASSERT_DEBUG_SYNC(!null_vb_entry_found,
                              "Vertex buffer bindings must be contiguous.");

            ++n_vbs;
        }
        else
        {
            null_vb_entry_found = true;
        }
    }

    ASSERT_DEBUG_SYNC(n_vas == n_vbs,
                      "n_VAS / n_VBS mismatch detected"); /* TODO */

    /* Retrieve the VAO. Note that raGL_vaos takes care of baking the VAO, if a new one
     * is needed. */
    vao = raGL_vaos_get_vao(vaos,
                            bake_state.vao_index_buffer,
                            n_vbs,
                            vas,
                            bake_state.vbs);

    raGL_vao_get_property(vao,
                          RAGL_VAO_PROPERTY_ID,
                          reinterpret_cast<void**>(&vao_id) );

    /* Enqueue a command to bind the VAO */
    _raGL_command* bind_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

    bind_command_ptr->bind_vertex_array_command_info.vao_id = vao_id;
    bind_command_ptr->type                                  = RAGL_COMMAND_TYPE_BIND_VERTEX_ARRAY;

    system_resizable_vector_push(commands,
                                 bind_command_ptr);

    /* All done */
    bake_state.vao_dirty = false;
}

/** TODO */
void _raGL_command_buffer::bake_commands()
{
    uint32_t n_ral_commands = 0;

    ral_command_buffer_get_property(command_buffer_ral,
                                    RAL_COMMAND_BUFFER_PROPERTY_N_RECORDED_COMMANDS,
                                   &n_ral_commands);

    for (uint32_t n_ral_command = 0;
                  n_ral_command < n_ral_commands;
                ++n_ral_command)
    {
        const void*      command_ral_raw_ptr = nullptr;
        ral_command_type command_ral_type    = RAL_COMMAND_TYPE_UNKNOWN;
        bool             result;

        result = ral_command_buffer_get_recorded_command(command_buffer_ral,
                                                         n_ral_command,
                                                        &command_ral_type,
                                                        &command_ral_raw_ptr);
        ASSERT_DEBUG_SYNC(result,
                          "ral_command_buffer_get_recorded_command() failed.");

        switch (command_ral_type)
        {
            case RAL_COMMAND_TYPE_CLEAR_RT_BINDING:
            {
                process_clear_rt_binding_command(reinterpret_cast<const ral_command_buffer_clear_rt_binding_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE:
            {
                process_copy_texture_to_texture_command(reinterpret_cast<const ral_command_buffer_copy_texture_to_texture_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_DISPATCH:
            {
                process_dispatch_command(reinterpret_cast<const ral_command_buffer_dispatch_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDEXED:
            {
                process_draw_call_indexed_command(reinterpret_cast<const ral_command_buffer_draw_call_indexed_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT:
            {
                process_draw_call_indirect_command(reinterpret_cast<const ral_command_buffer_draw_call_indirect_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_REGULAR:
            {
                process_draw_call_regular_command(reinterpret_cast<const ral_command_buffer_draw_call_regular_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER:
            {
                process_execute_command_buffer_command(reinterpret_cast<const ral_command_buffer_execute_command_buffer_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_FILL_BUFFER:
            {
                process_fill_buffer_command(reinterpret_cast<const ral_command_buffer_fill_buffer_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_INVALIDATE_TEXTURE:
            {
                process_invalidate_texture_command(reinterpret_cast<const ral_command_buffer_invalidate_texture_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_BINDING:
            {
                process_set_binding_command(reinterpret_cast<const ral_command_buffer_set_binding_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_GFX_STATE:
            {
                process_set_gfx_state_command(reinterpret_cast<const ral_command_buffer_set_gfx_state_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_PROGRAM:
            {
                process_set_program_command(reinterpret_cast<const ral_command_buffer_set_program_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_COLOR_RENDERTARGET:
            {
                process_set_color_rendertarget_command(reinterpret_cast<const ral_command_buffer_set_color_rendertarget_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_DEPTH_RENDERTARGET:
            {
                process_set_depth_rendertarget_command(reinterpret_cast<const ral_command_buffer_set_depth_rendertarget_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_SCISSOR_BOX:
            {
                process_set_scissor_box_command(reinterpret_cast<const ral_command_buffer_set_scissor_box_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_VERTEX_BUFFER:
            {
                process_set_vertex_buffer_command(reinterpret_cast<const ral_command_buffer_set_vertex_buffer_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_VIEWPORT:
            {
                process_set_viewport_command(reinterpret_cast<const ral_command_buffer_set_viewport_command_info*>(command_ral_raw_ptr) );

                break;
            }

            case RAL_COMMAND_TYPE_UPDATE_BUFFER:
            {
                process_update_buffer_command(reinterpret_cast<const ral_command_buffer_update_buffer_command_info*>(command_ral_raw_ptr) );

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL command type");
            }
        }
    }
}

/** TODO */
void _raGL_command_buffer::bake_gfx_state()
{
    bool                 alpha_to_coverage_enabled    = false;
    bool                 alpha_to_one_enabled         = false;
    bool                 culling_enabled              = false;
    ral_cull_mode        cull_mode                    = RAL_CULL_MODE_NONE;
    float                depth_bias_constant_factor   = 0.0f;
    bool                 depth_bias_enabled           = false;
    float                depth_bias_slope_factor      = 0.0f;
    float                depth_bounds_max             = 0.0f;
    float                depth_bounds_min             = 0.0f;
    bool                 depth_bounds_test_enabled    = false;
    bool                 depth_clamp_enabled          = false;
    ral_compare_op       depth_compare_func           = RAL_COMPARE_OP_UNKNOWN;
    bool                 depth_test_enabled           = false;
    bool                 depth_writes_enabled         = false;
    ral_front_face       front_face                   = RAL_FRONT_FACE_UNKNOWN;
    float                line_width                   = 0.0f;
    ral_logic_op         logic_op                     = RAL_LOGIC_OP_UNKNOWN;
    bool                 logic_op_enabled             = false;
    uint32_t             n_patch_control_points       = 0;
    ral_polygon_mode     polygon_mode_back            = RAL_POLYGON_MODE_UNKNOWN;
    ral_polygon_mode     polygon_mode_front           = RAL_POLYGON_MODE_UNKNOWN;
    bool                 primitive_restart_enabled    = false;
    ral_primitive_type   primitive_type               = RAL_PRIMITIVE_TYPE_UNKNOWN;
    bool                 rasterizer_discard_enabled   = false;
    bool                 sample_shading_enabled       = false;
    float                sample_shading_min           = 0.0f;
    bool                 scissor_test_enabled         = false;
    bool                 static_scissor_boxes_enabled = false;
    bool                 static_viewports_enabled     = false;
    ral_stencil_op_state stencil_test_back;
    bool                 stencil_test_enabled         = false;
    ral_stencil_op_state stencil_test_front;

    struct
    {
        ral_gfx_state_property property;
        void*                  out_result_ptr;
    } gfx_state_prop_to_var_mappings[] =
    {
        {RAL_GFX_STATE_PROPERTY_ALPHA_TO_COVERAGE_ENABLED,                  &alpha_to_coverage_enabled},
        {RAL_GFX_STATE_PROPERTY_ALPHA_TO_ONE_ENABLED,                       &alpha_to_one_enabled},
        {RAL_GFX_STATE_PROPERTY_CULLING_ENABLED,                            &culling_enabled},
        {RAL_GFX_STATE_PROPERTY_CULL_MODE,                                  &cull_mode},
        {RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_CONSTANT_FACTOR,                 &depth_bias_constant_factor},
        {RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_ENABLED,                         &depth_bias_enabled},
        {RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_SLOPE_FACTOR,                    &depth_bias_slope_factor},
        {RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MAX,                           &depth_bounds_max},
        {RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MIN,                           &depth_bounds_min},
        {RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_TEST_ENABLED,                  &depth_bounds_test_enabled},    // <- not available as core func in gl
        {RAL_GFX_STATE_PROPERTY_DEPTH_CLAMP_ENABLED,                        &depth_clamp_enabled},
        {RAL_GFX_STATE_PROPERTY_DEPTH_COMPARE_FUNC,                         &depth_compare_func},
        {RAL_GFX_STATE_PROPERTY_DEPTH_TEST_ENABLED,                         &depth_test_enabled},
        {RAL_GFX_STATE_PROPERTY_DEPTH_WRITES_ENABLED,                       &depth_writes_enabled},
        {RAL_GFX_STATE_PROPERTY_FRONT_FACE,                                 &front_face},
        {RAL_GFX_STATE_PROPERTY_LINE_WIDTH,                                 &line_width},
        {RAL_GFX_STATE_PROPERTY_LOGIC_OP,                                   &logic_op},
        {RAL_GFX_STATE_PROPERTY_LOGIC_OP_ENABLED,                           &logic_op_enabled},
        {RAL_GFX_STATE_PROPERTY_N_PATCH_CONTROL_POINTS,                     &n_patch_control_points},
        {RAL_GFX_STATE_PROPERTY_POLYGON_MODE_BACK,                          &polygon_mode_back},
        {RAL_GFX_STATE_PROPERTY_POLYGON_MODE_FRONT,                         &polygon_mode_front},
        {RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,                             &primitive_type},
        {RAL_GFX_STATE_PROPERTY_PRIMITIVE_RESTART_ENABLED,                  &primitive_restart_enabled},
        {RAL_GFX_STATE_PROPERTY_RASTERIZER_DISCARD_ENABLED,                 &rasterizer_discard_enabled},
        {RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_ENABLED,                     &sample_shading_enabled},
        {RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_MIN,                         &sample_shading_min},
        {RAL_GFX_STATE_PROPERTY_SCISSOR_TEST_ENABLED,                       &scissor_test_enabled},
        {RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES_ENABLED,               &static_scissor_boxes_enabled},
        {RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS_ENABLED,                   &static_viewports_enabled},
        {RAL_GFX_STATE_PROPERTY_STENCIL_TEST_BACK,                          &stencil_test_back},
        {RAL_GFX_STATE_PROPERTY_STENCIL_TEST_ENABLED,                       &stencil_test_enabled},
        {RAL_GFX_STATE_PROPERTY_STENCIL_TEST_FRONT,                         &stencil_test_front}
    };
    const uint32_t n_gfx_state_prop_to_var_mappings = sizeof(gfx_state_prop_to_var_mappings) / sizeof(gfx_state_prop_to_var_mappings[0]);

    for (uint32_t n_mapping = 0;
                  n_mapping < n_gfx_state_prop_to_var_mappings;
                ++n_mapping)
    {
        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   gfx_state_prop_to_var_mappings[n_mapping].property,
                                   gfx_state_prop_to_var_mappings[n_mapping].out_result_ptr);
    }

    ASSERT_DEBUG_SYNC(!depth_bounds_test_enabled,
                      "TODO");

    struct _mode_state
    {
        GLenum mode;
        bool   state;
    } mode_states[] =
    {
        /* NOTE: Depth bias is handled separately */

        {GL_SAMPLE_ALPHA_TO_COVERAGE,  alpha_to_coverage_enabled},
        {GL_SAMPLE_ALPHA_TO_ONE,       alpha_to_one_enabled},
        {GL_CULL_FACE,                 culling_enabled},
        {GL_DEPTH_CLAMP,               depth_clamp_enabled},
        {GL_DEPTH_TEST,                depth_test_enabled},
        {GL_FRAMEBUFFER_SRGB,          false}, /* TODO: This should be enabled if there's at least one sRGB color attachment !! */
        {GL_COLOR_LOGIC_OP,            logic_op_enabled},
        {GL_PRIMITIVE_RESTART,         primitive_restart_enabled},
        {GL_PROGRAM_POINT_SIZE,        true},
        {GL_RASTERIZER_DISCARD,        rasterizer_discard_enabled},
        {GL_SAMPLE_SHADING,            sample_shading_enabled},
        {GL_SCISSOR_TEST,              scissor_test_enabled},
        {GL_STENCIL_TEST,              stencil_test_enabled},
        {GL_TEXTURE_CUBE_MAP_SEAMLESS, true}
    };
    const uint32_t n_mode_states = sizeof(mode_states) / sizeof(mode_states[0]);

    for (uint32_t n_mode = 0;
                  n_mode < n_mode_states;
                ++n_mode)
    {
        const _mode_state& current_mode_state = mode_states[n_mode];

        /* Enqueue a GL command */
        _raGL_command* new_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

        if (current_mode_state.state)
        {
            new_command_ptr->enable_command_info.capability = current_mode_state.mode;
            new_command_ptr->type                           = RAGL_COMMAND_TYPE_ENABLE;
        }
        else
        {
            new_command_ptr->disable_command_info.capability = current_mode_state.mode;
            new_command_ptr->type                            = RAGL_COMMAND_TYPE_DISABLE;
        }

        system_resizable_vector_push(commands,
                                     new_command_ptr);
    }

    /* Cull mode */
    if (culling_enabled)
    {
        _raGL_command* new_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

        switch (cull_mode)
        {
            case RAL_CULL_MODE_BACK:           new_command_ptr->cull_face_command_info.mode = GL_BACK;           break;
            case RAL_CULL_MODE_FRONT:          new_command_ptr->cull_face_command_info.mode = GL_FRONT;          break;
            case RAL_CULL_MODE_FRONT_AND_BACK: new_command_ptr->cull_face_command_info.mode = GL_FRONT_AND_BACK; break;

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL cull mode requested.");
            }
        }

        new_command_ptr->type = RAGL_COMMAND_TYPE_CULL_FACE;

        system_resizable_vector_push(commands,
                                     new_command_ptr);
    }

    /* Depth bias */
    bool polygon_offset_fill_mode_enabled  = false;
    bool polygon_offset_line_mode_enabled  = false;
    bool polygon_offset_point_mode_enabled = false;

    for (uint32_t n_polygon_mode = 0;
                  n_polygon_mode < 2; /* back, front */
                ++n_polygon_mode)
    {
        const ral_polygon_mode current_polygon_mode = (n_polygon_mode == 0) ? polygon_mode_back
                                                                            : polygon_mode_front;

        if (fabs(depth_bias_constant_factor) > 1e-5f ||
            fabs(depth_bias_slope_factor)    > 1e-5f)
        {
            switch (current_polygon_mode)
            {
                case RAL_POLYGON_MODE_FILL:   polygon_offset_fill_mode_enabled  = true; break;
                case RAL_POLYGON_MODE_LINES:  polygon_offset_line_mode_enabled  = true; break;
                case RAL_POLYGON_MODE_POINTS: polygon_offset_point_mode_enabled = true; break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized RAL polygon mode value encountered.");
                }
            }
        }
    }

    /* Depth writes + depth test related states. */
    if (depth_writes_enabled)
    {
        _raGL_command* depth_func_command_ptr          = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));
        _raGL_command* enable_depth_writes_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

        enable_depth_writes_command_ptr->depth_mask_command_info.flag = GL_FALSE;
        enable_depth_writes_command_ptr->type                         = RAGL_COMMAND_TYPE_DEPTH_MASK;

        depth_func_command_ptr->depth_func_command_info.func = raGL_utils_get_ogl_enum_for_ral_compare_op(depth_compare_func);
        depth_func_command_ptr->type                         = RAGL_COMMAND_TYPE_DEPTH_FUNC;

        system_resizable_vector_push(commands,
                                     enable_depth_writes_command_ptr);
        system_resizable_vector_push(commands,
                                     depth_func_command_ptr);
    }
    else
    {
        _raGL_command* disable_depth_writes_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        disable_depth_writes_command_ptr->depth_mask_command_info.flag = GL_FALSE;
        disable_depth_writes_command_ptr->type                         = RAGL_COMMAND_TYPE_DEPTH_MASK;

        system_resizable_vector_push(commands,
                                     disable_depth_writes_command_ptr);
    }

    /* Polygon offsets */
    struct _polygon_offset_mode
    {
        bool   status;
        GLenum mode;
    } polygon_offset_modes[] =
    {
        { polygon_offset_fill_mode_enabled,  GL_POLYGON_OFFSET_FILL },
        { polygon_offset_line_mode_enabled,  GL_POLYGON_OFFSET_LINE },
        { polygon_offset_point_mode_enabled, GL_POLYGON_OFFSET_POINT}
    };
    const uint32_t n_polygon_offset_modes = sizeof(polygon_offset_modes) / sizeof(polygon_offset_modes[0]);
    bool           has_set_polygon_offset = false;

    for (uint32_t n_mode = 0;
                  n_mode < 3;
                ++n_mode)
    {
        const _polygon_offset_mode& current_mode = polygon_offset_modes[n_mode];

        if (current_mode.status)
        {
            _raGL_command* new_enable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            new_enable_command_ptr->enable_command_info.capability = current_mode.mode;
            new_enable_command_ptr->type                           = RAGL_COMMAND_TYPE_ENABLE;

            if (!has_set_polygon_offset)
            {
                _raGL_command* new_polygon_offset_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

                new_polygon_offset_command_ptr->polygon_offset_command_info.factor = depth_bias_slope_factor;
                new_polygon_offset_command_ptr->polygon_offset_command_info.units  = depth_bias_constant_factor;
                new_polygon_offset_command_ptr->type                               = RAGL_COMMAND_TYPE_POLYGON_OFFSET;

                system_resizable_vector_push(commands,
                                             new_polygon_offset_command_ptr);

                has_set_polygon_offset = true;
            }

            system_resizable_vector_push(commands,
                                         new_enable_command_ptr);
        }
        else
        {
            _raGL_command* new_disable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool));

            new_disable_command_ptr->disable_command_info.capability = current_mode.mode;
            new_disable_command_ptr->type                            = RAGL_COMMAND_TYPE_DISABLE;

            system_resizable_vector_push(commands,
                                         new_disable_command_ptr);

        }
    }

    /* Front face */
    {
        _raGL_command* new_front_face_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        switch (front_face)
        {
            case RAL_FRONT_FACE_CCW: new_front_face_command_ptr->front_face_command_info.mode = GL_CCW; break;
            case RAL_FRONT_FACE_CW:  new_front_face_command_ptr->front_face_command_info.mode = GL_CW;  break;

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL front face requested");
            }
        }

        new_front_face_command_ptr->type = RAGL_COMMAND_TYPE_FRONT_FACE;

        system_resizable_vector_push(commands,
                                     new_front_face_command_ptr);
    }

    /* Line width. */
    {
        _raGL_command* new_line_width_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        ASSERT_DEBUG_SYNC(line_width >= limits_ptr->aliased_line_width_range[0] &&
                          line_width <= limits_ptr->aliased_line_width_range[1],
                          "Unsupported line width requested.");

        new_line_width_command_ptr->line_width_command_info.width = line_width;
        new_line_width_command_ptr->type                          = RAGL_COMMAND_TYPE_LINE_WIDTH;

        system_resizable_vector_push(commands,
                                     new_line_width_command_ptr);
    }

    /* Logic op */
    if (logic_op_enabled)
    {
        _raGL_command* new_logic_op_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        switch (logic_op)
        {
            case RAL_LOGIC_OP_AND:           new_logic_op_command_ptr->logic_op_command_info.opcode = GL_AND;           break;
            case RAL_LOGIC_OP_AND_INVERTED:  new_logic_op_command_ptr->logic_op_command_info.opcode = GL_AND_INVERTED;  break;
            case RAL_LOGIC_OP_AND_REVERSE:   new_logic_op_command_ptr->logic_op_command_info.opcode = GL_AND_REVERSE;   break;
            case RAL_LOGIC_OP_CLEAR:         new_logic_op_command_ptr->logic_op_command_info.opcode = GL_CLEAR;         break;
            case RAL_LOGIC_OP_COPY:          new_logic_op_command_ptr->logic_op_command_info.opcode = GL_COPY;          break;
            case RAL_LOGIC_OP_COPY_INVERTED: new_logic_op_command_ptr->logic_op_command_info.opcode = GL_COPY_INVERTED; break;
            case RAL_LOGIC_OP_EQUIVALENT:    new_logic_op_command_ptr->logic_op_command_info.opcode = GL_EQUIV;         break;
            case RAL_LOGIC_OP_INVERT:        new_logic_op_command_ptr->logic_op_command_info.opcode = GL_INVERT;        break;
            case RAL_LOGIC_OP_NAND:          new_logic_op_command_ptr->logic_op_command_info.opcode = GL_NAND;          break;
            case RAL_LOGIC_OP_NOOP:          new_logic_op_command_ptr->logic_op_command_info.opcode = GL_NOOP;          break;
            case RAL_LOGIC_OP_NOR:           new_logic_op_command_ptr->logic_op_command_info.opcode = GL_NOR;           break;
            case RAL_LOGIC_OP_OR:            new_logic_op_command_ptr->logic_op_command_info.opcode = GL_OR;            break;
            case RAL_LOGIC_OP_OR_INVERTED:   new_logic_op_command_ptr->logic_op_command_info.opcode = GL_OR_INVERTED;   break;
            case RAL_LOGIC_OP_OR_REVERSE:    new_logic_op_command_ptr->logic_op_command_info.opcode = GL_OR_REVERSE;    break;
            case RAL_LOGIC_OP_SET:           new_logic_op_command_ptr->logic_op_command_info.opcode = GL_SET;           break;
            case RAL_LOGIC_OP_XOR:           new_logic_op_command_ptr->logic_op_command_info.opcode = GL_XOR;           break;

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL logic op requested.");
            }

            new_logic_op_command_ptr->type = RAGL_COMMAND_TYPE_LOGIC_OP;

            system_resizable_vector_push(commands,
                                         new_logic_op_command_ptr);
        }
    }

    /* Number of patch control points.
     *
     * This call is actually only necessary if we're using tessellation, but we want to avoid
     * tying GFX state with the active program for now.
     **/
    {
        _raGL_command* new_patch_parameteri_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        new_patch_parameteri_command_ptr->patch_parameteri_command_info.pname = GL_PATCH_VERTICES;
        new_patch_parameteri_command_ptr->patch_parameteri_command_info.value = n_patch_control_points;
        new_patch_parameteri_command_ptr->type                                = RAGL_COMMAND_TYPE_PATCH_PARAMETERI;

        system_resizable_vector_push(commands,
                                     new_patch_parameteri_command_ptr);
        
    }

    /* Polygon mode */
    {
        if (polygon_mode_back == polygon_mode_front)
        {
            _raGL_command* new_polygon_mode_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            new_polygon_mode_command_ptr->polygon_mode_command_info.face = GL_FRONT_AND_BACK;
            new_polygon_mode_command_ptr->polygon_mode_command_info.mode = raGL_utils_get_ogl_enum_for_ral_polygon_mode(polygon_mode_back);
            new_polygon_mode_command_ptr->type                           = RAGL_COMMAND_TYPE_POLYGON_MODE;

            system_resizable_vector_push(commands,
                                         new_polygon_mode_command_ptr);
        }
        else
        {
            _raGL_command* new_polygon_mode_back_command_ptr  = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* new_polygon_mode_front_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            new_polygon_mode_back_command_ptr->polygon_mode_command_info.face = GL_BACK;
            new_polygon_mode_back_command_ptr->polygon_mode_command_info.mode = raGL_utils_get_ogl_enum_for_ral_polygon_mode(polygon_mode_back);
            new_polygon_mode_back_command_ptr->type                           = RAGL_COMMAND_TYPE_POLYGON_MODE;

            new_polygon_mode_front_command_ptr->polygon_mode_command_info.face = GL_FRONT;
            new_polygon_mode_front_command_ptr->polygon_mode_command_info.mode = raGL_utils_get_ogl_enum_for_ral_polygon_mode(polygon_mode_front);
            new_polygon_mode_front_command_ptr->type                           = RAGL_COMMAND_TYPE_POLYGON_MODE;

            system_resizable_vector_push(commands,
                                         new_polygon_mode_back_command_ptr);
            system_resizable_vector_push(commands,
                                         new_polygon_mode_front_command_ptr);

        }
    }

    /* Sample shading */
    if (!sample_shading_enabled)
    {
        _raGL_command* disable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        disable_command_ptr->disable_command_info.capability = GL_SAMPLE_SHADING;
        disable_command_ptr->type                            = RAGL_COMMAND_TYPE_DISABLE;

        system_resizable_vector_push(commands,
                                     disable_command_ptr);
    }
    else
    {
        _raGL_command* enable_command_ptr             = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
        _raGL_command* min_sample_shading_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        enable_command_ptr->disable_command_info.capability = GL_SAMPLE_SHADING;
        enable_command_ptr->type                            = RAGL_COMMAND_TYPE_ENABLE;

        min_sample_shading_command_ptr->min_sample_shading_command_info.value = sample_shading_min;
        min_sample_shading_command_ptr->type                                  = RAGL_COMMAND_TYPE_MIN_SAMPLE_SHADING;

        system_resizable_vector_push(commands,
                                     enable_command_ptr);
        system_resizable_vector_push(commands,
                                     min_sample_shading_command_ptr);
    }

    /* Static scissor boxes & viewports
     *
     * This is not supported in core GL, so we need to emulate this by adjusting scissor box &
     * viewport states with GL calls.
     */
    if (static_scissor_boxes_enabled)
    {
        uint32_t                                         n_scissor_boxes              = 0;
        ral_command_buffer_set_scissor_box_command_info* set_scissor_box_commands_ral = nullptr;

        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_N_STATIC_SCISSOR_BOXES_AND_VIEWPORTS,
                                  &n_scissor_boxes);
        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES,
                                  &set_scissor_box_commands_ral);

        /* TODO: We could use glScissorArrayv() here instead. */
        if (scissor_test_enabled)
        {
            for (uint32_t n_scissor_box = 0;
                          n_scissor_box < n_scissor_boxes;
                        ++n_scissor_box)
            {
                _raGL_command* scissor_indexedv_command_raGL_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

                scissor_indexedv_command_raGL_ptr->scissor_indexedv_command_info.index = set_scissor_box_commands_ral[n_scissor_box].index;
                scissor_indexedv_command_raGL_ptr->scissor_indexedv_command_info.v[0]  = set_scissor_box_commands_ral[n_scissor_box].xy[0];
                scissor_indexedv_command_raGL_ptr->scissor_indexedv_command_info.v[1]  = set_scissor_box_commands_ral[n_scissor_box].xy[1];
                scissor_indexedv_command_raGL_ptr->scissor_indexedv_command_info.v[2]  = set_scissor_box_commands_ral[n_scissor_box].size[0];
                scissor_indexedv_command_raGL_ptr->scissor_indexedv_command_info.v[3]  = set_scissor_box_commands_ral[n_scissor_box].size[1];
                scissor_indexedv_command_raGL_ptr->type                                = RAGL_COMMAND_TYPE_SCISSOR_INDEXEDV;

                system_resizable_vector_push(commands,
                                             scissor_indexedv_command_raGL_ptr);
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Static scissor boxes have been redundantly defined - scissor test is disabled.");
        }
    }

    if (static_viewports_enabled)
    {
        uint32_t                                      n_viewports               = 0;
        ral_command_buffer_set_viewport_command_info* set_viewport_commands_ral = nullptr;

        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_N_STATIC_SCISSOR_BOXES_AND_VIEWPORTS,
                                  &n_viewports);
        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                  &set_viewport_commands_ral);

        /* TODO: We could use glDepthRangeArrayv() here instead. */
        /* TODO: We could use glViewportArrayv() here instead.   */
        for (uint32_t n_viewport = 0;
                      n_viewport < n_viewports;
                    ++n_viewport)
        {
            _raGL_command* depth_range_indexed_command_raGL_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* set_viewport_command_raGL_ptr        = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            depth_range_indexed_command_raGL_ptr->depth_range_indexed_command_info.farVal  = set_viewport_commands_ral[n_viewport].depth_range[1];
            depth_range_indexed_command_raGL_ptr->depth_range_indexed_command_info.index   = set_viewport_commands_ral[n_viewport].index;
            depth_range_indexed_command_raGL_ptr->depth_range_indexed_command_info.nearVal = set_viewport_commands_ral[n_viewport].depth_range[0];
            depth_range_indexed_command_raGL_ptr->type = RAGL_COMMAND_TYPE_DEPTH_RANGE_INDEXED;

            set_viewport_command_raGL_ptr->viewport_indexedfv_command_info.index = set_viewport_commands_ral[n_viewport].index;
            set_viewport_command_raGL_ptr->viewport_indexedfv_command_info.v[0]  = set_viewport_commands_ral[n_viewport].xy[0];
            set_viewport_command_raGL_ptr->viewport_indexedfv_command_info.v[1]  = set_viewport_commands_ral[n_viewport].xy[1];
            set_viewport_command_raGL_ptr->viewport_indexedfv_command_info.v[2]  = set_viewport_commands_ral[n_viewport].size[0];
            set_viewport_command_raGL_ptr->viewport_indexedfv_command_info.v[3]  = set_viewport_commands_ral[n_viewport].size[1];
            set_viewport_command_raGL_ptr->type                                  = RAGL_COMMAND_TYPE_VIEWPORT_INDEXEDFV;

            system_resizable_vector_push(commands,
                                         depth_range_indexed_command_raGL_ptr);
            system_resizable_vector_push(commands,
                                         set_viewport_command_raGL_ptr);
        }
    }

    /* Stencil test */
    if (stencil_test_enabled)
    {
        for (uint32_t n_iteration = 0;
                      n_iteration < 2; /* back + front stencil test state */
                    ++n_iteration)
        {
            const bool                  is_back_state_iteration = (n_iteration == 0);
            const GLenum                current_face            = (is_back_state_iteration) ? GL_BACK
                                                                                            : GL_FRONT;
            const ral_stencil_op_state& state                   = (is_back_state_iteration) ? stencil_test_back
                                                                                            : stencil_test_front;

            _raGL_command* stencil_func_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* stencil_mask_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* stencil_op_command_ptr   = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            stencil_func_command_ptr->stencil_func_separate_command_info.face = current_face;
            stencil_func_command_ptr->stencil_func_separate_command_info.func = raGL_utils_get_ogl_enum_for_ral_compare_op(state.compare_op);
            stencil_func_command_ptr->stencil_func_separate_command_info.mask = state.compare_mask;
            stencil_func_command_ptr->stencil_func_separate_command_info.ref  = state.reference_value;
            stencil_func_command_ptr->type                                    = RAGL_COMMAND_TYPE_STENCIL_FUNC_SEPARATE;

            stencil_mask_command_ptr->stencil_mask_separate_command_info.face = current_face;
            stencil_mask_command_ptr->stencil_mask_separate_command_info.mask = state.write_mask;
            stencil_mask_command_ptr->type                                    = RAGL_COMMAND_TYPE_STENCIL_MASK_SEPARATE;

            stencil_op_command_ptr->stencil_op_separate_command_info.dpfail = raGL_utils_get_ogl_enum_for_ral_stencil_op(state.depth_fail);
            stencil_op_command_ptr->stencil_op_separate_command_info.dppass = raGL_utils_get_ogl_enum_for_ral_stencil_op(state.pass);
            stencil_op_command_ptr->stencil_op_separate_command_info.face   = current_face;
            stencil_op_command_ptr->stencil_op_separate_command_info.sfail  = raGL_utils_get_ogl_enum_for_ral_stencil_op(state.fail);
            stencil_op_command_ptr->type                                    = RAGL_COMMAND_TYPE_STENCIL_OP_SEPARATE;

            system_resizable_vector_push(commands,
                                         stencil_func_command_ptr);
            system_resizable_vector_push(commands,
                                         stencil_mask_command_ptr);
            system_resizable_vector_push(commands,
                                         stencil_op_command_ptr);
        }
    }

    bake_state.active_gfx_state_dirty = false;
}

/** TODO */
void _raGL_command_buffer::bake_pre_dispatch_draw_memory_barriers()
{
    ral_program active_program_ral = nullptr;

    if (bake_state.active_program == nullptr)
    {
        ASSERT_DEBUG_SYNC(bake_state.active_program != nullptr,
                          "A draw call is about to be executed without an active program.");

        goto end;
    }

    raGL_program_get_property(bake_state.active_program,
                              RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                             &active_program_ral);

    if (active_program_ral == nullptr)
    {
        ASSERT_DEBUG_SYNC(active_program_ral != nullptr,
                          "No RAL program instance found for active program defined by bake state.");

        goto end;
    }

    /* If there are any images / storage buffers in use, check their status. If they are marked as dirty,
     * chances are a relevant memory barrier is needed. Throw it in "just in case".
     *
     * TODO: The memory barriers are only needed if the updated image / buffer regions are going to be read
     *       by the upcoming draw call. We currently do not check this, always assuming the worst-case scenario.
     */
    GLenum   memory_barriers_to_schedule = 0;
    uint32_t n_sampler_variables         = 0;
    uint32_t n_ssb_bindings              = 0;
    uint32_t n_storage_image_variables   = 0;
    uint32_t n_ub_bindings               = 0;

    raGL_program_get_property     (bake_state.active_program,
                                   RAGL_PROGRAM_PROPERTY_N_SSB_BINDINGS,
                                  &n_ssb_bindings);
    raGL_program_get_property     (bake_state.active_program,
                                   RAGL_PROGRAM_PROPERTY_N_UB_BINDINGS,
                                  &n_ub_bindings);
    ral_program_get_block_property(active_program_ral,
                                   system_hashed_ansi_string_get_default_empty_string(),
                                   RAL_PROGRAM_BLOCK_PROPERTY_N_SAMPLER_VARIABLES,
                                  &n_sampler_variables);
    ral_program_get_block_property(active_program_ral,
                                   system_hashed_ansi_string_get_default_empty_string(),
                                   RAL_PROGRAM_BLOCK_PROPERTY_N_STORAGE_IMAGE_VARIABLES,
                                  &n_storage_image_variables);

    if (n_ssb_bindings > 0)
    {
        bool any_sb_dirty = false;

        for (uint32_t n_sb = 0;
                      n_sb < n_ssb_bindings;
                    ++n_sb)
        {
            uint32_t bp = -1;

            raGL_program_get_block_property(bake_state.active_program,
                                            RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER,
                                            n_sb,
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &bp);

            ASSERT_DEBUG_SYNC(bp != -1,
                              "Could not retrieve storage buffer binding point");
            ASSERT_DEBUG_SYNC(bp < N_MAX_SB_BINDINGS,
                              "Invalid storage buffer binding point retrieved.");

            ASSERT_DEBUG_SYNC(bake_state.active_sb_bindings[bp].buffer != nullptr,
                              "No raGL_buffer instance associated with storage buffer binding point [%d]",
                              bp);

            any_sb_dirty |= raGL_dep_tracker_is_dirty(dep_tracker,
                                                      bake_state.active_sb_bindings[bp].buffer,
                                                      GL_SHADER_STORAGE_BARRIER_BIT);

            /* Since we have no way of determining whether the buffer is used for writes (at the moment),
             * assume the worst-case scenario */
            raGL_dep_tracker_mark_as_dirty(dep_tracker,
                                           bake_state.active_sb_bindings[bp].buffer);
        }

        if (any_sb_dirty)
        {
            memory_barriers_to_schedule |= GL_SHADER_STORAGE_BARRIER_BIT;
        }
    }

    if (n_storage_image_variables > 0)
    {
        bool any_si_dirty = false;

        for (uint32_t n_si = 0;
                      n_si < n_storage_image_variables;
                    ++n_si)
        {
            const ral_program_variable*  si_variable_ptr      = nullptr;
            const raGL_program_variable* si_variable_raGL_ptr = nullptr;

            ral_program_get_block_variable_by_class(active_program_ral,
                                                    system_hashed_ansi_string_get_default_empty_string(),
                                                    RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE,
                                                    n_si,
                                                   &si_variable_ptr);

            ASSERT_DEBUG_SYNC(si_variable_ptr != nullptr,
                              "No storage image defined at index [%d]",
                              n_si);
            ASSERT_DEBUG_SYNC(si_variable_ptr->location != -1,
                              "Storage image uniform [%s] has a location of -1.",
                              system_hashed_ansi_string_get_buffer(si_variable_ptr->name) );

            raGL_program_get_uniform_by_location(bake_state.active_program,
                                                 si_variable_ptr->location,
                                                &si_variable_raGL_ptr);

            ASSERT_DEBUG_SYNC(si_variable_raGL_ptr != nullptr,
                              "No raGL variable instance defined for location [%d]",
                              si_variable_ptr->location);
            ASSERT_DEBUG_SYNC(si_variable_raGL_ptr->image_unit < N_MAX_IMAGE_UNITS,
                              "Image unit [%d] exceeds maximum allowed texture image unit index.",
                              si_variable_raGL_ptr->image_unit);

            ASSERT_DEBUG_SYNC(bake_state.active_image_bindings[si_variable_raGL_ptr->image_unit] != nullptr,
                              "No texture bound to texture image unit [%d]",
                              si_variable_raGL_ptr->image_unit);

            any_si_dirty |= raGL_dep_tracker_is_dirty(dep_tracker,
                                                      bake_state.active_image_bindings[si_variable_raGL_ptr->image_unit],
                                                      GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            /* Since we have no way of determining whether the image is used for writes (at the moment),
             * assume the worst-case scenario */
            raGL_dep_tracker_mark_as_dirty(dep_tracker,
                                           bake_state.active_image_bindings[si_variable_raGL_ptr->image_unit]);
        }

        if (any_si_dirty)
        {
            memory_barriers_to_schedule |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
        }
    }

    if (n_ub_bindings > 0)
    {
        bool any_ub_dirty = false;

        for (uint32_t n_ub = 0;
                      n_ub < n_ub_bindings;
                    ++n_ub)
        {
            uint32_t bp = -1;

            raGL_program_get_block_property(bake_state.active_program,
                                            RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                            n_ub,
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &bp);

            ASSERT_DEBUG_SYNC(bp != -1,
                              "Could not retrieve uniform buffer binding point");
            ASSERT_DEBUG_SYNC(bp < N_MAX_UB_BINDINGS,
                              "Invalid uniform buffer binding point retrieved.");

            ASSERT_DEBUG_SYNC(bake_state.active_ub_bindings[bp].buffer != nullptr,
                              "No raGL_buffer instance associated with uniform buffer binding point [%d]",
                              bp);

            any_ub_dirty |= raGL_dep_tracker_is_dirty(dep_tracker,
                                                      bake_state.active_ub_bindings[bp].buffer,
                                                      GL_UNIFORM_BARRIER_BIT);
        }

        if (any_ub_dirty)
        {
            memory_barriers_to_schedule |= GL_UNIFORM_BARRIER_BIT;
        }
    }

    /* Is any of the buffers backing up active VAs dirty? */
    bool any_vb_dirty = false;

    for (uint32_t n_vb = 0;
                  n_vb < sizeof(bake_state.vbs) / sizeof(bake_state.vbs[0]);
                ++n_vb)
    {
        any_vb_dirty = raGL_dep_tracker_is_dirty(dep_tracker,
                                                 bake_state.vbs[n_vb].buffer_raGL,
                                                 GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }

    if (any_vb_dirty)
    {
        memory_barriers_to_schedule |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    }

    /* Have any of the textures the shader may attempt to sample from been updated via image store ops? */
    bool any_sampler_variable_dirty = false;

    for (uint32_t n_sampler_variable = 0;
                  n_sampler_variable < n_sampler_variables;
                ++n_sampler_variable)
    {
        const ral_program_variable*  sampler_variable_ptr      = nullptr;
        const raGL_program_variable* sampler_variable_raGL_ptr = nullptr;

        ral_program_get_block_variable_by_class(active_program_ral,
                                                system_hashed_ansi_string_get_default_empty_string(),
                                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER,
                                                n_sampler_variable,
                                               &sampler_variable_ptr);

        ASSERT_DEBUG_SYNC(sampler_variable_ptr != nullptr,
                          "No sampler defined at index [%d]",
                          n_sampler_variable);
        ASSERT_DEBUG_SYNC(sampler_variable_ptr->location != -1,
                          "Sampler uniform [%s] has a location of -1.",
                          system_hashed_ansi_string_get_buffer(sampler_variable_ptr->name) );

        raGL_program_get_uniform_by_location(bake_state.active_program,
                                             sampler_variable_ptr->location,
                                            &sampler_variable_raGL_ptr);

        ASSERT_DEBUG_SYNC(sampler_variable_raGL_ptr->texture_unit != -1,
                          "No texture unit assigned to a sampler uniform [%s]",
                          system_hashed_ansi_string_get_buffer(sampler_variable_ptr->name) );
        ASSERT_DEBUG_SYNC(sampler_variable_raGL_ptr->texture_unit < N_MAX_TEXTURE_UNITS,
                          "Texture unit assigned to a sampler uniform [%s] (d) exceeds N_MAX_TEXTURE_UNITS (%d)",
                          system_hashed_ansi_string_get_buffer(sampler_variable_ptr->name),
                          sampler_variable_raGL_ptr->texture_unit);

        any_sampler_variable_dirty |= raGL_dep_tracker_is_dirty(dep_tracker,
                                                                bake_state.active_texture_sampler_bindings[sampler_variable_raGL_ptr->texture_unit],
                                                                GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    if (any_sampler_variable_dirty)
    {
        memory_barriers_to_schedule |= GL_TEXTURE_FETCH_BARRIER_BIT;
    }

    /* Schedule the memory barrier command */
    if (memory_barriers_to_schedule != 0)
    {
        _raGL_command* memory_barrier_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        memory_barrier_command_ptr->memory_barriers_command_info.barriers = memory_barriers_to_schedule;
        memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

        system_resizable_vector_push(commands,
                                     memory_barrier_command_ptr);
    }
end:
    ;
}

/** TODO */
void _raGL_command_buffer::bake_rt_state()
{
    /* Configure RT-related states */
    bool all_rt_attachments_use_identical_blend_settings = true;

    for (uint32_t n_rt = 1;
                  n_rt < sizeof(bake_state.active_rt_color_attachments) / sizeof(bake_state.active_rt_color_attachments[0]) && all_rt_attachments_use_identical_blend_settings;
                ++n_rt)
    {
        const _raGL_command_buffer_bake_state_rt& base_rt    = bake_state.active_rt_color_attachments[0];
        const _raGL_command_buffer_bake_state_rt& current_rt = bake_state.active_rt_color_attachments[n_rt];

        if (current_rt.texture_view == nullptr)
        {
            continue;
        }

        all_rt_attachments_use_identical_blend_settings &= (current_rt.blend_constant         == base_rt.blend_constant) &&
                                                           (current_rt.blend_enabled          == base_rt.blend_enabled)  &&
                                                           (current_rt.blend_op_alpha         == base_rt.blend_op_alpha) &&
                                                           (current_rt.blend_op_color         == base_rt.blend_op_color)         &&
                                                           (current_rt.dst_alpha_blend_factor == base_rt.dst_alpha_blend_factor) &&
                                                           (current_rt.dst_color_blend_factor == base_rt.dst_color_blend_factor) &&
                                                           (current_rt.src_alpha_blend_factor == base_rt.src_alpha_blend_factor) &&
                                                           (current_rt.src_color_blend_factor == base_rt.src_color_blend_factor);
    }

    ASSERT_DEBUG_SYNC(all_rt_attachments_use_identical_blend_settings,
                      "GL back-end does not support per-draw buffer blending settings.");

    /* NOTE: Color/depth/stencil writes are configured at rendertarget binding update time. */

    /* Below we can safely assume all draw buffers are going to use the same blend settings */
    const _raGL_command_buffer_bake_state_rt& current_rt = bake_state.active_rt_color_attachments[0];

    if (!current_rt.blend_enabled)
    {
        _raGL_command* disable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        disable_command_ptr->disable_command_info.capability = GL_BLEND;
        disable_command_ptr->type                            = RAGL_COMMAND_TYPE_DISABLE;

        system_resizable_vector_push(commands,
                                     disable_command_ptr);
    }
    else
    {
        {
            /* Enable blending */
            _raGL_command* enable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            enable_command_ptr->enable_command_info.capability = GL_BLEND;
            enable_command_ptr->type                           = RAGL_COMMAND_TYPE_ENABLE;

            system_resizable_vector_push(commands,
                                         enable_command_ptr);
        }

        if (current_rt.dst_alpha_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.dst_alpha_blend_factor == RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            current_rt.dst_alpha_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.dst_alpha_blend_factor == RAL_BLEND_FACTOR_CONSTANT_COLOR           ||
            current_rt.dst_color_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.dst_color_blend_factor == RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            current_rt.dst_color_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.dst_color_blend_factor == RAL_BLEND_FACTOR_CONSTANT_COLOR           ||
            current_rt.src_alpha_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.src_alpha_blend_factor == RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            current_rt.src_alpha_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.src_alpha_blend_factor == RAL_BLEND_FACTOR_CONSTANT_COLOR           ||
            current_rt.src_color_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.src_color_blend_factor == RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA ||
            current_rt.src_color_blend_factor == RAL_BLEND_FACTOR_CONSTANT_ALPHA           ||
            current_rt.src_color_blend_factor == RAL_BLEND_FACTOR_CONSTANT_COLOR)
        {
            /* Blend constant */
            _raGL_command* blend_color_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            ASSERT_DEBUG_SYNC(current_rt.blend_constant.data_type == RAL_COLOR_DATA_TYPE_FLOAT,
                              "Invalid blend constant color data type");

            static_assert(sizeof(blend_color_command_ptr->blend_color_command_info.rgba) == sizeof(current_rt.blend_constant.f32), "");

            memcpy(blend_color_command_ptr->blend_color_command_info.rgba,
                   current_rt.blend_constant.f32,
                   sizeof(blend_color_command_ptr->blend_color_command_info.rgba) );

            blend_color_command_ptr->type = RAGL_COMMAND_TYPE_BLEND_COLOR;

            system_resizable_vector_push(commands,
                                         blend_color_command_ptr);
        }

        {
            /* Blend ops */
            _raGL_command* blend_equation_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            blend_equation_command_ptr->blend_equation_separate_command_info.modeAlpha = raGL_utils_get_ogl_enum_for_ral_blend_op(current_rt.blend_op_alpha);
            blend_equation_command_ptr->blend_equation_separate_command_info.modeRGB   = raGL_utils_get_ogl_enum_for_ral_blend_op(current_rt.blend_op_color);
            blend_equation_command_ptr->type                                           = RAGL_COMMAND_TYPE_BLEND_EQUATION_SEPARATE;

            system_resizable_vector_push(commands,
                                         blend_equation_command_ptr);
        }

        {
            /* Blend functions */
            _raGL_command* blend_func_separate_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            blend_func_separate_command_ptr->blend_func_separate_command_info.dstAlpha = raGL_utils_get_ogl_enum_for_ral_blend_factor(current_rt.dst_alpha_blend_factor);
            blend_func_separate_command_ptr->blend_func_separate_command_info.dstRGB   = raGL_utils_get_ogl_enum_for_ral_blend_factor(current_rt.dst_color_blend_factor);
            blend_func_separate_command_ptr->blend_func_separate_command_info.srcAlpha = raGL_utils_get_ogl_enum_for_ral_blend_factor(current_rt.src_alpha_blend_factor);
            blend_func_separate_command_ptr->blend_func_separate_command_info.srcRGB   = raGL_utils_get_ogl_enum_for_ral_blend_factor(current_rt.src_color_blend_factor);
            blend_func_separate_command_ptr->type                                      = RAGL_COMMAND_TYPE_BLEND_FUNC_SEPARATE;

            system_resizable_vector_push(commands,
                                         blend_func_separate_command_ptr);
        }
    }

    /* All done */
    bake_state.active_rt_attachments_dirty = false;
}

/** TODO */
void _raGL_command_buffer::clear_commands()
{
    _raGL_command* current_command_ptr = nullptr;

    while (system_resizable_vector_pop(commands,
                                      &current_command_ptr) )
    {
        current_command_ptr->deinit();

        system_resource_pool_return_to_pool(command_pool,
                                            (system_resource_pool_block) current_command_ptr);
    }

    bake_state.reset();
}

/** TODO */
void _raGL_command_buffer::process_clear_rt_binding_command(const ral_command_buffer_clear_rt_binding_command_info* command_ral_ptr)
{
    raGL_backend backend_raGL = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);

    if (bake_state.active_fbo_draw_buffers_dirty)
    {
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }

    bool scissor_test_pre_enabled          = false;
    bool should_restore_draw_buffers       = false;
    bool should_restore_scissor_test_state = false;

    if ( bake_state.active_gfx_state != nullptr &&
        !bake_state.active_gfx_state_dirty)
    {
        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_SCISSOR_TEST_ENABLED,
                                  &scissor_test_pre_enabled);

        should_restore_scissor_test_state = true;
    }
    else
    {
        should_restore_scissor_test_state = false;
    }

    {
        /* In OpenGL, clear regions can be controlled with a scissor test. Enable one now. */
        _raGL_command* enable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        enable_command_ptr->enable_command_info.capability = GL_SCISSOR_TEST;
        enable_command_ptr->type                           = RAGL_COMMAND_TYPE_ENABLE;

        system_resizable_vector_push(commands,
                                     enable_command_ptr);
    }

    for (uint32_t n_rt = 0;
                  n_rt < command_ral_ptr->n_rendertargets;
                ++n_rt)
    {
        const ral_command_buffer_clear_rt_binding_rendertarget& current_rt     = command_ral_ptr->rendertargets[n_rt];
        ral_texture_view                                        current_rt_ral = nullptr;

        switch (current_rt.aspect)
        {
            case RAL_TEXTURE_ASPECT_COLOR_BIT:
            {
                current_rt_ral = bake_state.active_rt_color_attachments[current_rt.rt_index].texture_view;

                break;
            }

            case RAL_TEXTURE_ASPECT_DEPTH_BIT:
            case (RAL_TEXTURE_ASPECT_DEPTH_BIT | RAL_TEXTURE_ASPECT_STENCIL_BIT):
            case RAL_TEXTURE_ASPECT_STENCIL_BIT:
            {
                current_rt_ral = bake_state.active_rt_ds_attachment;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                 "Unrecognized texture aspect specified");
            }
        }

        uint32_t        current_rt_depth;
        ral_format      current_rt_format;
        bool            current_rt_format_has_color_comps;
        bool            current_rt_format_has_depth_comps;
        bool            current_rt_format_has_stencil_comps;
        ral_format_type current_rt_format_type;
        uint32_t        current_rt_height;
        uint32_t        current_rt_n_base_layer;
        uint32_t        current_rt_n_base_mip;
        uint32_t        current_rt_n_layers;
        uint32_t        current_rt_n_mips;
        ral_texture     current_rt_parent_texture;
        uint32_t        current_rt_width;

        if (current_rt_ral == nullptr)
        {
            ASSERT_DEBUG_SYNC(current_rt_ral != nullptr,
                              "No texture view assigned to the specified rendertarget");

            continue;
        }

        ral_texture_view_get_property(current_rt_ral,
                                      RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                     &current_rt_format);
        ral_texture_view_get_property(current_rt_ral,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_BASE_LAYER,
                                     &current_rt_n_base_layer);
        ral_texture_view_get_property(current_rt_ral,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_BASE_MIPMAP,
                                     &current_rt_n_base_mip);
        ral_texture_view_get_property(current_rt_ral,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                     &current_rt_n_layers);
        ral_texture_view_get_property(current_rt_ral,
                                      RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                     &current_rt_n_mips);
        ral_texture_view_get_property(current_rt_ral,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &current_rt_parent_texture);

        ral_utils_get_format_property(current_rt_format,
                                      RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                     &current_rt_format_has_color_comps);
        ral_utils_get_format_property(current_rt_format,
                                      RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                     &current_rt_format_has_depth_comps);
        ral_utils_get_format_property(current_rt_format,
                                      RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                     &current_rt_format_has_stencil_comps);
        ral_utils_get_format_property(current_rt_format,
                                      RAL_FORMAT_PROPERTY_FORMAT_TYPE,
                                     &current_rt_format_type);

        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_COLOR_BIT) != 0)
        {
            /* Color attachment-specific checks */
            if (!current_rt_format_has_color_comps)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Cannot clear color channel of a a non-color rendertarget");

                continue;
            }

            if (current_rt.rt_index >= N_MAX_RENDERTARGETS)
            {
                ASSERT_DEBUG_SYNC(current_rt.rt_index < N_MAX_RENDERTARGETS,
                                  "Invalid rendertarget index specified.");

                continue;
            }
        }

        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) != 0)
        {
            /* Depth attachment-specific checks */
            ral_texture_aspect ds_aspect;

            if (!current_rt_format_has_depth_comps)
            {
                ASSERT_DEBUG_SYNC(!((current_rt.aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) != 0 && !current_rt_format_has_depth_comps),
                                  "Cannot clear depth channel of a a non-depth rendertarget");

                continue;
            }

            if (bake_state.active_rt_ds_attachment == nullptr)
            {
                ASSERT_DEBUG_SYNC(bake_state.active_rt_ds_attachment != nullptr,
                                  "Cannot clear depth attachment - no depth/stencil attachment is defined.");

                continue;
            }

            ral_texture_view_get_property(bake_state.active_rt_ds_attachment,
                                          RAL_TEXTURE_VIEW_PROPERTY_ASPECT,
                                         &ds_aspect);

            if ((ds_aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) == 0)
            {
                ASSERT_DEBUG_SYNC((ds_aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) != 0,
                                  "Cannot clear depth attachment - current DS attachment does not hold depth data.");

                continue;
            }
        }

        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0 && !current_rt_format_has_stencil_comps)
        {
            ASSERT_DEBUG_SYNC(!((current_rt.aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0 && !current_rt_format_has_stencil_comps),
                              "Cannot clear stencil channel of a a non-stencil rendertarget");

            continue;
        }

        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0)
        {
            /* Stencil attachment-specific checks */
            ral_texture_aspect ds_aspect;

            if (bake_state.active_rt_ds_attachment == nullptr)
            {
                ASSERT_DEBUG_SYNC(bake_state.active_rt_ds_attachment != nullptr,
                                  "Cannot clear stencil attachment - no depth/stencil attachment is defined.");

                continue;
            }

            ral_texture_view_get_property(bake_state.active_rt_ds_attachment,
                                          RAL_TEXTURE_VIEW_PROPERTY_ASPECT,
                                         &ds_aspect);

            if ((ds_aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) == 0)
            {
                ASSERT_DEBUG_SYNC((ds_aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) != 0,
                                  "Cannot clear depth attachment - current DS attachment does not hold depth data.");

                continue;
            }
        }

        if (current_rt_n_mips != 1)
        {
            ASSERT_DEBUG_SYNC(current_rt_n_mips == 1,
                              "Rendertarget must not be assigned more than one mip");

            continue;
        }

        ral_texture_get_mipmap_property(current_rt_parent_texture,
                                        current_rt_n_base_layer,
                                        current_rt_n_base_mip,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                       &current_rt_depth);
        ral_texture_get_mipmap_property(current_rt_parent_texture,
                                        current_rt_n_base_layer,
                                        current_rt_n_base_mip,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                       &current_rt_height);
        ral_texture_get_mipmap_property(current_rt_parent_texture,
                                        current_rt_n_base_layer,
                                        current_rt_n_base_mip,
                                        RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                       &current_rt_width);

        /* Adjust the draw buffer configuration, so that subsequent clear ops only modify contents of the specified
         * rendertarget */
        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_COLOR_BIT) != 0)
        {
            _raGL_command* draw_buffers_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            draw_buffers_command_ptr->draw_buffers_command_info.bufs[0] = GL_COLOR_ATTACHMENT0 + current_rt.rt_index;
            draw_buffers_command_ptr->draw_buffers_command_info.n       = 1;
            draw_buffers_command_ptr->type                              = RAGL_COMMAND_TYPE_DRAW_BUFFERS;

            should_restore_draw_buffers = true;

            system_resizable_vector_push(commands,
                                         draw_buffers_command_ptr);
        }

        /* Adjust clear color values */
        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_COLOR_BIT) != 0)
        {
            _raGL_command* clear_color_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            /* TODO: We should handle sint/uint clear requests differently */
            static_assert(sizeof(clear_color_command_ptr->clear_color_command_info.rgba) == sizeof(current_rt.clear_value.color.f32), "");

            switch (current_rt_format_type)
            {
                case RAL_FORMAT_TYPE_SFLOAT:
                case RAL_FORMAT_TYPE_UFLOAT:
                {
                    memcpy(clear_color_command_ptr->clear_color_command_info.rgba,
                           current_rt.clear_value.color.f32,
                           sizeof(clear_color_command_ptr->clear_color_command_info.rgba) );

                    break;
                }

                case RAL_FORMAT_TYPE_UNORM:
                {
                    for (uint32_t n_component = 0;
                                  n_component < 4;
                                ++n_component)
                    {
                        clear_color_command_ptr->clear_color_command_info.rgba[n_component] = float(current_rt.clear_value.color.ui8[n_component]) / 255.0f;
                    }

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unsupported rendertarget format type");
                }
            }

            clear_color_command_ptr->type = RAGL_COMMAND_TYPE_CLEAR_COLOR;

            system_resizable_vector_push(commands,
                                         clear_color_command_ptr);
        }

        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) != 0)
        {
            _raGL_command* clear_depth_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            clear_depth_command_ptr->clear_depthf_command_info.depth = current_rt.clear_value.depth;
            clear_depth_command_ptr->type                            = RAGL_COMMAND_TYPE_CLEAR_DEPTHF;

            system_resizable_vector_push(commands,
                                         clear_depth_command_ptr);
        }

        if ((current_rt.aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0)
        {
            _raGL_command* clear_stencil_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            clear_stencil_command_ptr->clear_stencil_command_info.stencil = current_rt.clear_value.stencil;
            clear_stencil_command_ptr->type                               = RAGL_COMMAND_TYPE_CLEAR_STENCIL;

            system_resizable_vector_push(commands,
                                         clear_stencil_command_ptr);
        }

        for (uint32_t n_region = 0;
                      n_region < command_ral_ptr->n_clear_regions;
                    ++n_region)
        {
            _raGL_command*                                          clear_command_ptr;
            const ral_command_buffer_clear_rt_binding_clear_region& current_region = command_ral_ptr->clear_regions[n_region];
            _raGL_command*                                          scissor_command_ptr;

            if (current_region.n_base_layer + current_region.n_layers > current_rt_n_layers)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid layers specified.");

                continue;
            }

            if (current_region.xy[0] + current_region.size[0] > current_rt_width ||
                current_region.xy[1] + current_region.size[1] > current_rt_height)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid clear region specified");

                continue;
            }

            /* Adjust the scissor box */
            scissor_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            scissor_command_ptr->scissor_indexedv_command_info.index = 0;
            scissor_command_ptr->scissor_indexedv_command_info.v[0]  = current_region.xy  [0];
            scissor_command_ptr->scissor_indexedv_command_info.v[1]  = current_region.xy  [1];
            scissor_command_ptr->scissor_indexedv_command_info.v[2]  = current_region.size[0];
            scissor_command_ptr->scissor_indexedv_command_info.v[3]  = current_region.size[1];
            scissor_command_ptr->type                                = RAGL_COMMAND_TYPE_SCISSOR_INDEXEDV;

            system_resizable_vector_push(commands,
                                         scissor_command_ptr);

            /* Enqueue clear command */
            clear_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            clear_command_ptr->clear_command_info.mask = (((current_rt.aspect & RAL_TEXTURE_ASPECT_COLOR_BIT)   != 0) ? GL_COLOR_BUFFER_BIT   : 0) |
                                                         (((current_rt.aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT)   != 0) ? GL_DEPTH_BUFFER_BIT   : 0) |
                                                         (((current_rt.aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0) ? GL_STENCIL_BUFFER_BIT : 0);
            clear_command_ptr->type                    = RAGL_COMMAND_TYPE_CLEAR;

            system_resizable_vector_push(commands,
                                         clear_command_ptr);
        }
    }

    if (should_restore_draw_buffers)
    {
        _raGL_command* draw_buffers_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        static_assert(sizeof(draw_buffers_command_ptr->draw_buffers_command_info.bufs) == sizeof(bake_state.active_fbo_draw_buffers), "");

        memcpy(draw_buffers_command_ptr->draw_buffers_command_info.bufs,
               bake_state.active_fbo_draw_buffers,
               sizeof(bake_state.active_fbo_draw_buffers) );

        draw_buffers_command_ptr->draw_buffers_command_info.n = N_MAX_DRAW_BUFFERS;
        draw_buffers_command_ptr->type                        = RAGL_COMMAND_TYPE_DRAW_BUFFERS;

        system_resizable_vector_push(commands,
                                     draw_buffers_command_ptr);
    }

    if (should_restore_scissor_test_state)
    {
        _raGL_command* scissor_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        scissor_command_ptr->scissor_indexedv_command_info.index = 0;
        scissor_command_ptr->scissor_indexedv_command_info.v[0]  = bake_state.active_scissor_boxes[0].xy  [0];
        scissor_command_ptr->scissor_indexedv_command_info.v[1]  = bake_state.active_scissor_boxes[0].xy  [1];
        scissor_command_ptr->scissor_indexedv_command_info.v[2]  = bake_state.active_scissor_boxes[0].size[0];
        scissor_command_ptr->scissor_indexedv_command_info.v[3]  = bake_state.active_scissor_boxes[0].size[1];
        scissor_command_ptr->type                                = RAGL_COMMAND_TYPE_SCISSOR_INDEXEDV;

        system_resizable_vector_push(commands,
                                     scissor_command_ptr);


        if (!scissor_test_pre_enabled)
        {
            _raGL_command* disable_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            disable_command_ptr->enable_command_info.capability = GL_SCISSOR_TEST;
            disable_command_ptr->type                           = RAGL_COMMAND_TYPE_DISABLE;

            system_resizable_vector_push(commands,
                                         disable_command_ptr);
        }
    }
}

/** TODO */
void _raGL_command_buffer::process_copy_buffer_to_buffer_command(const ral_command_buffer_copy_buffer_to_buffer_command_info* command_ral_ptr)
{
    raGL_backend   backend_raGL                 = nullptr;
    _raGL_command* bind_commands_ptr[2]         = {nullptr, nullptr};
    ral_context    context_ral                  = nullptr;
    raGL_buffer    dst_buffer_raGL              = nullptr;
    GLuint         dst_buffer_raGL_id           = 0;
    uint32_t       dst_buffer_raGL_start_offset = -1;
    uint32_t       dst_buffer_ral_start_offset  = -1;
    _raGL_command* copy_command_ptr             = nullptr;
    bool           result                       = true;
    raGL_buffer    src_buffer_raGL              = nullptr;
    GLuint         src_buffer_raGL_id           = 0;
    uint32_t       src_buffer_raGL_start_offset = -1;
    uint32_t       src_buffer_ral_start_offset  = -1;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);

    result &= raGL_backend_get_buffer(backend_raGL,
                                      command_ral_ptr->dst_buffer,
                                      &dst_buffer_raGL);
    result &= raGL_backend_get_buffer(backend_raGL,
                                      command_ral_ptr->src_buffer,
                                      &src_buffer_raGL);

    ASSERT_DEBUG_SYNC(result,
                      "Could not retrieve raGL_buffer instances");

    raGL_buffer_get_property(dst_buffer_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &dst_buffer_raGL_id);
    raGL_buffer_get_property(dst_buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &dst_buffer_raGL_start_offset);
    raGL_buffer_get_property(src_buffer_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &src_buffer_raGL_id);
    raGL_buffer_get_property(src_buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &src_buffer_raGL_start_offset);

    ral_buffer_get_property(command_ral_ptr->dst_buffer,
                            RAL_BUFFER_PROPERTY_START_OFFSET,
                           &dst_buffer_ral_start_offset);
    ral_buffer_get_property(command_ral_ptr->src_buffer,
                            RAL_BUFFER_PROPERTY_START_OFFSET,
                           &src_buffer_ral_start_offset);

    bind_commands_ptr[0] = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
    bind_commands_ptr[1] = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
    copy_command_ptr     = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    /* Ensure source & destination buffer objects are flushed */
    if (raGL_dep_tracker_is_dirty(dep_tracker,
                                  src_buffer_raGL,
                                  GL_BUFFER_UPDATE_BARRIER_BIT) ||
        raGL_dep_tracker_is_dirty(dep_tracker,
                                  dst_buffer_raGL,
                                  GL_BUFFER_UPDATE_BARRIER_BIT) )
    {
        _raGL_command* memory_barrier_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        memory_barrier_command_ptr->memory_barriers_command_info.barriers = GL_BUFFER_UPDATE_BARRIER_BIT;
        memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

        system_resizable_vector_push(commands,
                                     memory_barrier_command_ptr);
    }

    /* Issue the copy command, after the buffers are bound to relevant targets */
    _raGL_command_bind_buffer_command_info&          bind1_command_args = bind_commands_ptr[0]->bind_buffer_command_info;
    _raGL_command_bind_buffer_command_info&          bind2_command_args = bind_commands_ptr[1]->bind_buffer_command_info;
    _raGL_command_copy_buffer_sub_data_command_info& copy_command_args  = copy_command_ptr->copy_buffer_sub_data_command_info;

    bind1_command_args.bo_id   = dst_buffer_raGL_id;
    bind1_command_args.target  = GL_COPY_WRITE_BUFFER;
    bind_commands_ptr[0]->type = RAGL_COMMAND_TYPE_BIND_BUFFER;

    bind2_command_args.bo_id   = src_buffer_raGL_id;
    bind2_command_args.target  = GL_COPY_READ_BUFFER;
    bind_commands_ptr[1]->type = RAGL_COMMAND_TYPE_BIND_BUFFER;

    copy_command_args.read_offset  = command_ral_ptr->src_buffer_start_offset + src_buffer_raGL_start_offset + src_buffer_ral_start_offset;
    copy_command_args.read_target  = GL_COPY_READ_BUFFER;
    copy_command_args.size         = command_ral_ptr->size;
    copy_command_args.write_offset = command_ral_ptr->dst_buffer_start_offset + dst_buffer_raGL_start_offset + dst_buffer_ral_start_offset;
    copy_command_args.write_target = GL_COPY_WRITE_BUFFER;
    copy_command_ptr->type         = RAGL_COMMAND_TYPE_COPY_BUFFER_SUB_DATA;

    system_resizable_vector_push(commands,
                                 bind_commands_ptr[0]);
    system_resizable_vector_push(commands,
                                 bind_commands_ptr[1]);
    system_resizable_vector_push(commands,
                                 copy_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_copy_texture_to_texture_command(const ral_command_buffer_copy_texture_to_texture_command_info* command_ral_ptr)
{
    raGL_framebuffers  backend_fbos      = nullptr;
    raGL_backend       backend_raGL      = nullptr;
    ral_context        context_ral       = nullptr;
    ral_format         dst_texture_format_ral;
    GLuint             dst_texture_id    = 0;
    bool               dst_texture_is_rb = false;
    raGL_texture       dst_texture_raGL  = nullptr;
    ral_texture_type   dst_texture_type;
    _raGL_command*     new_command_ptr   = nullptr;
    bool               result            = true;
    ral_format         src_texture_format_ral;
    GLuint             src_texture_id    = 0;
    bool               src_texture_is_rb = false;
    raGL_texture       src_texture_raGL  = nullptr;
    ral_texture_type   src_texture_type;
    bool               use_copy_op       = false;

    ogl_context_get_property         (context,
                                      OGL_CONTEXT_PROPERTY_BACKEND,
                                     &backend_raGL);
    ogl_context_get_property         (context,
                                      OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                                     &context_ral);
    raGL_backend_get_private_property(backend_raGL,
                                      RAGL_BACKEND_PRIVATE_PROPERTY_FBOS,
                                     &backend_fbos);

    result &= raGL_backend_get_texture(backend_raGL,
                                       command_ral_ptr->dst_texture,
                                       &dst_texture_raGL);
    result &= raGL_backend_get_texture(backend_raGL,
                                       command_ral_ptr->src_texture,
                                       &src_texture_raGL);

    ASSERT_DEBUG_SYNC(result,
                      "Could not retrieve raGL_texture instances");

    ral_texture_get_property(command_ral_ptr->dst_texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &dst_texture_format_ral);
    ral_texture_get_property(command_ral_ptr->dst_texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &dst_texture_type);
    ral_texture_get_property(command_ral_ptr->src_texture,
                             RAL_TEXTURE_PROPERTY_FORMAT,
                            &src_texture_format_ral);
    ral_texture_get_property(command_ral_ptr->src_texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &src_texture_type);

    raGL_texture_get_property(dst_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                              (void**) &dst_texture_id);
    raGL_texture_get_property(dst_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                              (void**) &dst_texture_is_rb);
    raGL_texture_get_property(src_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                              (void**) &src_texture_id);
    raGL_texture_get_property(src_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                              (void**) &src_texture_is_rb);

    new_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    if (command_ral_ptr->dst_size[0] == command_ral_ptr->src_size[0] &&
        command_ral_ptr->dst_size[1] == command_ral_ptr->src_size[1] &&
        command_ral_ptr->dst_size[2] == command_ral_ptr->src_size[2])
    {
        /* This is a copy op, as long as all aspects supported by the format have been requested .. */
        bool dst_format_aspects[3];
        bool src_format_aspects[3];

        for (uint32_t n_texture = 0;
                      n_texture < 2;
                    ++n_texture)
        {
            ral_format current_texture_format = (n_texture == 0) ? dst_texture_format_ral : src_texture_format_ral;
            bool*      out_result_ptr         = (n_texture == 0) ? dst_format_aspects     : src_format_aspects;

            ral_utils_get_format_property(current_texture_format,
                                          RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                          out_result_ptr + 0);
            ral_utils_get_format_property(current_texture_format,
                                          RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                          out_result_ptr + 1);
            ral_utils_get_format_property(current_texture_format,
                                          RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                          out_result_ptr + 2);
        }

        use_copy_op = (dst_format_aspects[0] == src_format_aspects[0] &&
                       dst_format_aspects[1] == src_format_aspects[1] &&
                       dst_format_aspects[2] == src_format_aspects[2]);
    }

    if (use_copy_op)
    {
        _raGL_command_copy_image_sub_data_command_info& command_args = new_command_ptr->copy_image_sub_data_command_info;

        ASSERT_DEBUG_SYNC(dst_texture_format_ral == src_texture_format_ral,
                          "TODO"); /* glCopyImageSubData() will only work for compatible formats. */

        /* Ensure source & destination texture objects are flushed */
        if (raGL_dep_tracker_is_dirty(dep_tracker,
                                      src_texture_raGL,
                                      GL_TEXTURE_UPDATE_BARRIER_BIT) ||
            raGL_dep_tracker_is_dirty(dep_tracker,
                                      dst_texture_raGL,
                                      GL_TEXTURE_UPDATE_BARRIER_BIT) )
        {
            _raGL_command* memory_barrier_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            memory_barrier_command_ptr->memory_barriers_command_info.barriers = GL_TEXTURE_UPDATE_BARRIER_BIT;
            memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

            system_resizable_vector_push(commands,
                                         memory_barrier_command_ptr);
        }

        /* Issue the command */
        command_args.dst_level     = command_ral_ptr->n_dst_texture_mipmap;
        command_args.dst_object_id = dst_texture_id;
        command_args.dst_target    = (dst_texture_is_rb) ? GL_RENDERBUFFER : raGL_utils_get_ogl_enum_for_ral_texture_type(dst_texture_type);
        command_args.src_level     = command_ral_ptr->n_src_texture_mipmap;
        command_args.src_object_id = src_texture_id;
        command_args.src_target    = (src_texture_is_rb) ? GL_RENDERBUFFER : raGL_utils_get_ogl_enum_for_ral_texture_type(src_texture_type);

        static_assert(sizeof(command_args.dst_xyz) == sizeof(command_ral_ptr->dst_start_xyz), "");
        static_assert(sizeof(command_args.size)    == sizeof(command_ral_ptr->dst_size),      "");
        static_assert(sizeof(command_args.src_xyz) == sizeof(command_ral_ptr->src_start_xyz), "");

        memcpy(command_args.size,
               command_ral_ptr->dst_size,
               sizeof(command_args.size) );
        memcpy(command_args.dst_xyz,
               command_ral_ptr->dst_start_xyz,
               sizeof(command_args.dst_xyz) );
        memcpy(command_args.src_xyz,
               command_ral_ptr->src_start_xyz,
               sizeof(command_args.src_xyz) );

        new_command_ptr->type = RAGL_COMMAND_TYPE_COPY_IMAGE_SUB_DATA;

        system_resizable_vector_push(commands,
                                     new_command_ptr);
    }
    else
    {
        /* This is a blit op */
        _raGL_command_blit_framebuffer_command_info& command_args                      = new_command_ptr->blit_framebuffer_command_info;
        raGL_framebuffer                             draw_fb_raGL                      = nullptr;
        raGL_framebuffer                             read_fb_raGL                      = nullptr;
        ral_texture_view_create_info                 texture_view_create_info_items[2];
        ral_texture_view                             texture_views                 [2];

        texture_view_create_info_items[0].aspect       = static_cast<ral_texture_aspect>(command_ral_ptr->aspect);
        texture_view_create_info_items[0].format       = dst_texture_format_ral;
        texture_view_create_info_items[0].n_base_layer = command_ral_ptr->n_dst_texture_layer;
        texture_view_create_info_items[0].n_base_mip   = command_ral_ptr->n_dst_texture_mipmap;
        texture_view_create_info_items[0].n_layers     = 1;
        texture_view_create_info_items[0].n_mips       = 1;
        texture_view_create_info_items[0].texture      = command_ral_ptr->dst_texture;
        texture_view_create_info_items[0].type         = dst_texture_type;

        texture_view_create_info_items[1].aspect       = static_cast<ral_texture_aspect>(command_ral_ptr->aspect);
        texture_view_create_info_items[1].format       = src_texture_format_ral;
        texture_view_create_info_items[1].n_base_layer = command_ral_ptr->n_src_texture_layer;
        texture_view_create_info_items[1].n_base_mip   = command_ral_ptr->n_src_texture_mipmap;
        texture_view_create_info_items[1].n_layers     = 1;
        texture_view_create_info_items[1].n_mips       = 1;
        texture_view_create_info_items[1].texture      = command_ral_ptr->src_texture;
        texture_view_create_info_items[1].type         = src_texture_type;

        /* Note: texture views will automatically be released when parent texture is destroyed. */
        ral_context_create_texture_views(context_ral,
                                         sizeof(texture_view_create_info_items) / sizeof(texture_view_create_info_items[0]),
                                         texture_view_create_info_items,
                                         texture_views);

        ASSERT_DEBUG_SYNC(texture_views[0] != nullptr && texture_views[1] != nullptr,
                          "Could not create RAL texture views");

        raGL_framebuffers_get_framebuffer(backend_fbos,
                                          1, /* n_attachments */
                                          texture_views + 0,
                                          nullptr, /* in_opt_ds_attachment */
                                         &draw_fb_raGL);
        raGL_framebuffers_get_framebuffer(backend_fbos,
                                          1, /* n_attachments */
                                          texture_views + 1,
                                          nullptr, /* in_opt_ds_attachment */
                                         &read_fb_raGL);

        raGL_framebuffer_get_property(draw_fb_raGL,
                                      RAGL_FRAMEBUFFER_PROPERTY_ID,
                                     &command_args.draw_fbo_id);
        raGL_framebuffer_get_property(read_fb_raGL,
                                      RAGL_FRAMEBUFFER_PROPERTY_ID,
                                     &command_args.read_fbo_id);

        ASSERT_DEBUG_SYNC(command_ral_ptr->dst_start_xyz[2] == 0 &&
                          command_ral_ptr->src_start_xyz[2] == 0,
                          "TODO");

        /* Before blitting, ensure both source and destination textures are flushed */
        if (raGL_dep_tracker_is_dirty(dep_tracker,
                                      command_ral_ptr->dst_texture,
                                      GL_FRAMEBUFFER_BARRIER_BIT) ||
            raGL_dep_tracker_is_dirty(dep_tracker,
                                      command_ral_ptr->src_texture,
                                      GL_FRAMEBUFFER_BARRIER_BIT) )
        {
            _raGL_command* bind_back_draw_fb_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* bind_draw_fb_command_ptr      = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* bind_read_fb_command_ptr      = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            _raGL_command* memory_barrier_command_ptr    = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            /* Bind the draw & read FBs */
            bind_draw_fb_command_ptr->bind_framebuffer_command_info.framebuffer = command_args.draw_fbo_id;
            bind_draw_fb_command_ptr->bind_framebuffer_command_info.target      = GL_DRAW_FRAMEBUFFER;
            bind_draw_fb_command_ptr->type                                      = RAGL_COMMAND_TYPE_BIND_FRAMEBUFFER;

            bind_read_fb_command_ptr->bind_framebuffer_command_info.framebuffer = command_args.read_fbo_id;
            bind_read_fb_command_ptr->bind_framebuffer_command_info.target      = GL_READ_FRAMEBUFFER;
            bind_read_fb_command_ptr->type                                      = RAGL_COMMAND_TYPE_BIND_FRAMEBUFFER;

            system_resizable_vector_push(commands,
                                         bind_draw_fb_command_ptr);
            system_resizable_vector_push(commands,
                                         bind_read_fb_command_ptr);

            /* Issue the memory barrier */
            memory_barrier_command_ptr->memory_barriers_command_info.barriers = GL_FRAMEBUFFER_BARRIER_BIT;
            memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

            system_resizable_vector_push(commands,
                                         memory_barrier_command_ptr);

            /* Restore the draw FB. We don't care about the read FBO. */
            raGL_framebuffer_get_property(bake_state.active_fbo_draw,
                                          RAGL_FRAMEBUFFER_PROPERTY_ID,
                                         &bind_back_draw_fb_command_ptr->bind_framebuffer_command_info.framebuffer);

            bind_back_draw_fb_command_ptr->bind_framebuffer_command_info.target = GL_DRAW_FRAMEBUFFER;
            bind_back_draw_fb_command_ptr->type                                 = RAGL_COMMAND_TYPE_BIND_FRAMEBUFFER;

            system_resizable_vector_push(commands,
                                         bind_back_draw_fb_command_ptr);
        }

        /* Carry on with the blit */
        command_args.dst_x0y0x1y1[0] = command_ral_ptr->dst_start_xyz[0];
        command_args.dst_x0y0x1y1[1] = command_ral_ptr->dst_start_xyz[1];
        command_args.dst_x0y0x1y1[2] = command_args.dst_x0y0x1y1[0] + command_ral_ptr->dst_size[0];
        command_args.dst_x0y0x1y1[3] = command_args.dst_x0y0x1y1[1] + command_ral_ptr->dst_size[1];
        command_args.filter          = raGL_utils_get_ogl_enum_for_ral_texture_filter_mag(command_ral_ptr->scaling_filter);
        command_args.mask            = ((command_ral_ptr->aspect & RAL_TEXTURE_ASPECT_COLOR_BIT)   ? GL_COLOR_BUFFER_BIT   : 0) |
                                       ((command_ral_ptr->aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT)   ? GL_DEPTH_BUFFER_BIT   : 0) |
                                       ((command_ral_ptr->aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) ? GL_STENCIL_BUFFER_BIT : 0);
        command_args.src_x0y0x1y1[0] = command_ral_ptr->src_start_xyz[0];
        command_args.src_x0y0x1y1[1] = command_ral_ptr->src_start_xyz[1];
        command_args.src_x0y0x1y1[2] = command_args.src_x0y0x1y1[0] + command_ral_ptr->src_size[0];
        command_args.src_x0y0x1y1[3] = command_args.src_x0y0x1y1[1] + command_ral_ptr->src_size[1];

        new_command_ptr->type = RAGL_COMMAND_TYPE_BLIT_FRAMEBUFFER;


        system_resizable_vector_push(commands,
                                     new_command_ptr);
    }
}

/** TODO */
void _raGL_command_buffer::process_dispatch_command(const ral_command_buffer_dispatch_command_info* command_ral_ptr)
{
    _raGL_command* dispatch_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    /* Sanity checks */
    #ifdef _DEBUG
    {
        ral_program active_program_ral = nullptr;

        ASSERT_DEBUG_SYNC(bake_state.active_program != nullptr,
                          "No active program");

        raGL_program_get_property(bake_state.active_program,
                                  RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                                 &active_program_ral);

        ASSERT_DEBUG_SYNC(ral_program_is_shader_stage_defined(active_program_ral,
                                                              RAL_SHADER_TYPE_COMPUTE),
                          "Active program does not define compute shader stage");
    }
    #endif

    bake_pre_dispatch_draw_memory_barriers();

    dispatch_command_ptr->dispatch_command_info.x = command_ral_ptr->x;
    dispatch_command_ptr->dispatch_command_info.y = command_ral_ptr->y;
    dispatch_command_ptr->dispatch_command_info.z = command_ral_ptr->z;
    dispatch_command_ptr->type                    = RAGL_COMMAND_TYPE_DISPATCH;

    system_resizable_vector_push(commands,
                                 dispatch_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_draw_call_indexed_command(const ral_command_buffer_draw_call_indexed_command_info* command_ral_ptr)
{
    /* TODO: Coalesce multiple indexed draw calls into a single multi-draw call. */
    raGL_backend backend_raGL                   = nullptr;
    raGL_buffer  index_buffer_raGL              = nullptr;
    uint32_t     index_buffer_raGL_start_offset = -1;
    uint32_t     n_bytes_per_index              = 0;

    switch (command_ral_ptr->index_type)
    {
        case RAL_INDEX_TYPE_16BIT: n_bytes_per_index = 2; break;
        case RAL_INDEX_TYPE_32BIT: n_bytes_per_index = 4; break;
        case RAL_INDEX_TYPE_8BIT:  n_bytes_per_index = 1; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized index type requested");
        }
    }

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);

    /* Bind the index buffer */
    raGL_backend_get_buffer(backend_raGL,
                            command_ral_ptr->index_buffer,
                           &index_buffer_raGL);

    raGL_buffer_get_property(index_buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &index_buffer_raGL_start_offset);

    if (index_buffer_raGL != bake_state.vao_index_buffer)
    {
        bake_state.vao_dirty        = true;
        bake_state.vao_index_buffer = index_buffer_raGL;
    }

    /* If no VAO is currently bound, or current VAO configuration does not match bake state,
     * we need to bind a different vertex array object. */
    if (bake_state.vao_dirty)
    {
        bake_and_bind_vao();

        ASSERT_DEBUG_SYNC(!bake_state.vao_dirty,
                          "VAO state still marked as dirty.");
    }

    /* If the index buffer is dirty, flush it now */
    if (raGL_dep_tracker_is_dirty(dep_tracker,
                                  index_buffer_raGL,
                                  GL_ELEMENT_ARRAY_BARRIER_BIT) )
    {
        _raGL_command* memory_barrier_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        memory_barrier_command_ptr->memory_barriers_command_info.barriers = GL_ELEMENT_ARRAY_BARRIER_BIT;
        memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

        system_resizable_vector_push(commands,
                                     memory_barrier_command_ptr);
    }

    /* Update context state if a new GFX state has been bound */
    if (bake_state.active_gfx_state_dirty)
    {
        bake_gfx_state();

        ASSERT_DEBUG_SYNC(!bake_state.active_gfx_state_dirty,
                          "GFX state still marked as dirty.");
    }

    /* Update rendertarget-related states as well. */
    if (bake_state.active_rt_attachments_dirty)
    {
        bake_rt_state    ();
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_rt_attachments_dirty,
                          "Rendertarget state still marked as dirty.");
        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }
    else
    if (bake_state.active_fbo_draw_buffers_dirty)
    {
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }

    bake_pre_dispatch_draw_memory_barriers();

    /* Issue the draw call */
    _raGL_command* draw_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    raGL_buffer_get_property(index_buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &index_buffer_raGL_start_offset);

    if (command_ral_ptr->base_instance == 0)
    {
        if (command_ral_ptr->base_vertex == 0)
        {
            if (command_ral_ptr->n_instances == 1)
            {
                draw_command_ptr->draw_elements_command_info.count   = command_ral_ptr->n_indices;
                draw_command_ptr->draw_elements_command_info.indices = reinterpret_cast<GLvoid*>(index_buffer_raGL_start_offset + command_ral_ptr->first_index * n_bytes_per_index);
                draw_command_ptr->draw_elements_command_info.type    = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);
                draw_command_ptr->type                               = RAGL_COMMAND_TYPE_DRAW_ELEMENTS;
            }
            else
            {
                draw_command_ptr->draw_elements_instanced_command_info.count     = command_ral_ptr->n_indices;
                draw_command_ptr->draw_elements_instanced_command_info.indices   = reinterpret_cast<GLvoid*>(index_buffer_raGL_start_offset + command_ral_ptr->first_index * n_bytes_per_index);
                draw_command_ptr->draw_elements_instanced_command_info.primcount = command_ral_ptr->n_instances;
                draw_command_ptr->draw_elements_instanced_command_info.type      = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);
                draw_command_ptr->type                                           = RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED;
            }
        }
        else
        {
            if (command_ral_ptr->n_instances == 1)
            {
                draw_command_ptr->draw_elements_base_vertex_command_info.base_vertex = command_ral_ptr->base_vertex;
                draw_command_ptr->draw_elements_base_vertex_command_info.count       = command_ral_ptr->n_indices;
                draw_command_ptr->draw_elements_base_vertex_command_info.indices     = reinterpret_cast<GLvoid*>(index_buffer_raGL_start_offset + command_ral_ptr->first_index * n_bytes_per_index);
                draw_command_ptr->draw_elements_base_vertex_command_info.type        = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);
                draw_command_ptr->type                                               = RAGL_COMMAND_TYPE_DRAW_ELEMENTS_BASE_VERTEX;
            }
            else
            {
                draw_command_ptr->draw_elements_instanced_base_vertex_command_info.base_vertex = command_ral_ptr->base_vertex;
                draw_command_ptr->draw_elements_instanced_base_vertex_command_info.count       = command_ral_ptr->n_indices;
                draw_command_ptr->draw_elements_instanced_base_vertex_command_info.indices     = reinterpret_cast<GLvoid*>(index_buffer_raGL_start_offset + command_ral_ptr->first_index * n_bytes_per_index);
                draw_command_ptr->draw_elements_instanced_base_vertex_command_info.primcount   = command_ral_ptr->n_instances;
                draw_command_ptr->draw_elements_instanced_base_vertex_command_info.type        = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);
                draw_command_ptr->type                                                         = RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX;
            }
        }
    }
    else
    {
        if (command_ral_ptr->base_vertex == 0)
        {
            draw_command_ptr->draw_elements_instanced_base_instance_command_info.base_instance = command_ral_ptr->base_instance;
            draw_command_ptr->draw_elements_instanced_base_instance_command_info.count         = command_ral_ptr->n_indices;
            draw_command_ptr->draw_elements_instanced_base_instance_command_info.indices       = reinterpret_cast<GLvoid*>(index_buffer_raGL_start_offset + command_ral_ptr->first_index * n_bytes_per_index);
            draw_command_ptr->draw_elements_instanced_base_instance_command_info.primcount     = command_ral_ptr->n_instances;
            draw_command_ptr->draw_elements_instanced_base_instance_command_info.type          = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);
            draw_command_ptr->type                                                             = RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_INSTANCE;
        }
        else
        {
            draw_command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info.base_instance = command_ral_ptr->base_instance;
            draw_command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info.base_vertex   = command_ral_ptr->base_vertex;
            draw_command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info.count         = command_ral_ptr->n_indices;
            draw_command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info.indices       = reinterpret_cast<GLvoid*>(index_buffer_raGL_start_offset + command_ral_ptr->first_index * n_bytes_per_index);
            draw_command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info.primcount     = command_ral_ptr->n_instances;
            draw_command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info.type          = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);
            draw_command_ptr->type                                                                         = RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE;
        }
    }

    system_resizable_vector_push(commands,
                                 draw_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_draw_call_indirect_command(const ral_command_buffer_draw_call_indirect_command_info* command_ral_ptr)
{
    /* TODO: Coalesce multiple indirect draw calls into a single multi-draw call. */
    raGL_backend   backend_raGL     = nullptr;
    _raGL_command* draw_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    ASSERT_DEBUG_SYNC(command_ral_ptr->indirect_buffer != nullptr,
                      "Indirect buffer is nullptr");

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);

    /* Bind the indirect buffer */
    {
        _raGL_command* bind_command_ptr        = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
        raGL_buffer    indirect_buffer_raGL    = nullptr;
        GLuint         indirect_buffer_raGL_id = 0;

        ASSERT_DEBUG_SYNC(command_ral_ptr->indirect_buffer != nullptr,
                          "Indirect buffer is null");

        raGL_backend_get_buffer (backend_raGL,
                                 command_ral_ptr->indirect_buffer,
                                &indirect_buffer_raGL);
        raGL_buffer_get_property(indirect_buffer_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &indirect_buffer_raGL_id);

        bind_command_ptr->bind_buffer_command_info.bo_id  = indirect_buffer_raGL_id;
        bind_command_ptr->bind_buffer_command_info.target = GL_DRAW_INDIRECT_BUFFER;
        bind_command_ptr->type                            = RAGL_COMMAND_TYPE_BIND_BUFFER;

        system_resizable_vector_push(commands,
                                     bind_command_ptr);

        /* Flush the indirect buffer if needed */
        if (raGL_dep_tracker_is_dirty(dep_tracker,
                                      indirect_buffer_raGL,
                                      GL_COMMAND_BARRIER_BIT) )
        {
            _raGL_command* memory_barrier_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            memory_barrier_command_ptr->memory_barriers_command_info.barriers = GL_COMMAND_BARRIER_BIT;
            memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

            system_resizable_vector_push(commands,
                                         memory_barrier_command_ptr);
        }
    }

    /* Update context state if a new GFX state has been bound */
    if (bake_state.active_gfx_state_dirty)
    {
        bake_gfx_state();

        ASSERT_DEBUG_SYNC(!bake_state.active_gfx_state_dirty,
                          "GFX state still marked as dirty.");
    }

    /* Update rendertarget-related states as well. */
    if (bake_state.active_rt_attachments_dirty)
    {
        bake_rt_state    ();
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_rt_attachments_dirty,
                          "Rendertarget state still marked as dirty.");
        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }
    else
    if (bake_state.active_fbo_draw_buffers_dirty)
    {
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }

    if (command_ral_ptr->index_buffer == nullptr)
    {
        bake_pre_dispatch_draw_memory_barriers();

        /* Issue the draw call */
        _raGL_command_multi_draw_arrays_indirect_command_info& command_args = draw_command_ptr->multi_draw_arrays_indirect_command_info;

        command_args.drawcount = 1;
        command_args.indirect  = reinterpret_cast<GLvoid*>(command_ral_ptr->offset);
        command_args.stride    = command_ral_ptr->stride;

        draw_command_ptr->type = RAGL_COMMAND_TYPE_MULTI_DRAW_ARRAYS_INDIRECT;
    }
    else
    {
        /* Bind the index buffer */
        {
            raGL_buffer index_buffer_raGL = nullptr;

            raGL_backend_get_buffer(backend_raGL,
                                    command_ral_ptr->index_buffer,
                                   &index_buffer_raGL);

            /* Bind the index buffer */
            if (index_buffer_raGL != bake_state.vao_index_buffer)
            {
                bake_state.vao_dirty        = true;
                bake_state.vao_index_buffer = index_buffer_raGL;
            }

            /* If no VAO is currently bound, or current VAO configuration does not match bake state,
            * we need to bind a different vertex array object. */
            if (bake_state.vao_dirty)
            {
                bake_and_bind_vao();

                ASSERT_DEBUG_SYNC(!bake_state.vao_dirty,
                    "VA state still marked as dirty.");
            }

            /* If the index buffer is dirty, flush it now */
            if (raGL_dep_tracker_is_dirty(dep_tracker,
                                          index_buffer_raGL,
                                          GL_ELEMENT_ARRAY_BARRIER_BIT) )
            {
                _raGL_command* memory_barrier_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

                memory_barrier_command_ptr->memory_barriers_command_info.barriers = GL_ELEMENT_ARRAY_BARRIER_BIT;
                memory_barrier_command_ptr->type                                  = RAGL_COMMAND_TYPE_MEMORY_BARRIER;

                system_resizable_vector_push(commands,
                                             memory_barrier_command_ptr);
            }
        }

        bake_pre_dispatch_draw_memory_barriers();

        /* Issue the draw call */
        _raGL_command_multi_draw_elements_indirect_command_info& command_args = draw_command_ptr->multi_draw_elements_indirect_command_info;

        command_args.drawcount = 1;
        command_args.indirect  = reinterpret_cast<GLvoid*>(command_ral_ptr->offset);
        command_args.stride    = command_ral_ptr->stride;
        command_args.type      = raGL_utils_get_ogl_enum_for_ral_index_type(command_ral_ptr->index_type);

        draw_command_ptr->type = RAGL_COMMAND_TYPE_MULTI_DRAW_ELEMENTS_INDIRECT;
    }

    system_resizable_vector_push(commands,
                                 draw_command_ptr);
}

void _raGL_command_buffer::process_draw_call_regular_command(const ral_command_buffer_draw_call_regular_command_info* command_ral_ptr)
{
    /* TODO: Coalesce multiple draw calls into a single multi-draw call. */
    _raGL_command* draw_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    /* Update context state if a new GFX state has been bound */
    if (bake_state.active_gfx_state_dirty)
    {
        bake_gfx_state();

        ASSERT_DEBUG_SYNC(!bake_state.active_gfx_state_dirty,
            "GFX state still marked as dirty.");
    }

    /* Update rendertarget-related states as well. */
    if (bake_state.active_rt_attachments_dirty)
    {
        bake_rt_state    ();
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_rt_attachments_dirty,
                          "Rendertarget state still marked as dirty.");
        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }
    else
    if (bake_state.active_fbo_draw_buffers_dirty)
    {
        bake_and_bind_fbo();

        ASSERT_DEBUG_SYNC(!bake_state.active_fbo_draw_buffers_dirty,
                          "Could not update draw buffer configuration");
    }

    /* If no VAO is currently bound, or current VAO configuration does not match bake state,
     * we need to bind a different vertex array object. */
    if (bake_state.vao_dirty)
    {
        bake_and_bind_vao();

        ASSERT_DEBUG_SYNC(!bake_state.vao_dirty,
                          "VAO state still marked as dirty.");
    }

    bake_pre_dispatch_draw_memory_barriers();

    if (command_ral_ptr->n_instances == 1)
    {
        _raGL_command_draw_arrays_command_info& command_args = draw_command_ptr->draw_arrays_command_info;

        ASSERT_DEBUG_SYNC(command_ral_ptr->base_instance == 0,
                          "Base instance must be 0 for single-instanced draw calls");

        command_args.count = command_ral_ptr->n_vertices;
        command_args.first = command_ral_ptr->base_vertex;

        draw_command_ptr->type = RAGL_COMMAND_TYPE_DRAW_ARRAYS;
    }
    else
    {
        if (command_ral_ptr->base_instance == 0)
        {
            _raGL_command_draw_arrays_instanced_command_info& command_args = draw_command_ptr->draw_arrays_instanced_command_info;

            command_args.count     = command_ral_ptr->n_vertices;
            command_args.first     = command_ral_ptr->base_vertex;
            command_args.primcount = command_ral_ptr->n_instances;

            draw_command_ptr->type = RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED;
        }
        else
        {
            _raGL_command_draw_arrays_instanced_base_instance_command_info& command_args = draw_command_ptr->draw_arrays_instanced_base_instance_command_info;

            command_args.base_instance = command_ral_ptr->base_instance;
            command_args.count         = command_ral_ptr->n_vertices;
            command_args.first         = command_ral_ptr->base_vertex;
            command_args.primcount     = command_ral_ptr->n_instances;

            draw_command_ptr->type = RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE;
        }
    }

    system_resizable_vector_push(commands,
                                 draw_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_execute_command_buffer_command(const ral_command_buffer_execute_command_buffer_command_info* command_ral_ptr)
{
    raGL_backend        backend_raGL        = nullptr;
    raGL_command_buffer command_buffer_raGL = nullptr;
    _raGL_command*      execute_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);

    raGL_backend_get_command_buffer(backend_raGL,
                                    command_ral_ptr->command_buffer,
                                   &command_buffer_raGL);

    ASSERT_DEBUG_SYNC(command_buffer_raGL != nullptr,
                      "raGL command buffer returned by GL backend is null");

    execute_command_ptr->execute_command_buffer_command_info.command_buffer_raGL = command_buffer_raGL;
    execute_command_ptr->type                                                    = RAGL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER;

    system_resizable_vector_push(commands,
                                 execute_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_fill_buffer_command(const ral_command_buffer_fill_buffer_command_info* command_ral_ptr)
{
    raGL_backend   backend_raGL             = nullptr;
    raGL_buffer    buffer_raGL              = nullptr;
    GLuint         buffer_raGL_id           = 0;
    uint32_t       buffer_raGL_start_offset = 0;
    _raGL_command* bind_command_ptr         = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
    _raGL_command* clear_command_ptr        = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);
    raGL_backend_get_buffer (backend_raGL,
                             command_ral_ptr->buffer,
                            &buffer_raGL);
    raGL_buffer_get_property(buffer_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &buffer_raGL_id);
    raGL_buffer_get_property(buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &buffer_raGL_start_offset);

    bind_command_ptr->type                            = RAGL_COMMAND_TYPE_BIND_BUFFER;
    bind_command_ptr->bind_buffer_command_info.bo_id  = buffer_raGL_id;
    bind_command_ptr->bind_buffer_command_info.target = GL_COPY_WRITE_BUFFER;

    clear_command_ptr->clear_buffer_sub_data_command_info.bo_id          = buffer_raGL_id;
    clear_command_ptr->clear_buffer_sub_data_command_info.format         = GL_RED_INTEGER;
    clear_command_ptr->clear_buffer_sub_data_command_info.internalformat = GL_R32I;
    clear_command_ptr->clear_buffer_sub_data_command_info.size           = sizeof(uint32_t) * command_ral_ptr->n_dwords;
    clear_command_ptr->clear_buffer_sub_data_command_info.start_offset   = command_ral_ptr->start_offset + buffer_raGL_start_offset;
    clear_command_ptr->clear_buffer_sub_data_command_info.target         = GL_COPY_WRITE_BUFFER;
    clear_command_ptr->clear_buffer_sub_data_command_info.type           = GL_INT;

    memcpy(clear_command_ptr->clear_buffer_sub_data_command_info.data,
          &command_ral_ptr->dword_value,
           sizeof(int32_t) );

    clear_command_ptr->type = RAGL_COMMAND_TYPE_CLEAR_BUFFER_SUB_DATA;

    system_resizable_vector_push(commands,
                                 bind_command_ptr);
    system_resizable_vector_push(commands,
                                 clear_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_invalidate_texture_command(const ral_command_buffer_invalidate_texture_command_info* command_ral_ptr)
{
    raGL_backend backend_raGL       = nullptr;
    raGL_texture texture_raGL       = nullptr;
    GLuint       texture_raGL_id    = 0;
    bool         texture_raGL_is_rb = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);

    raGL_backend_get_texture(backend_raGL,
                             command_ral_ptr->texture,
                            &texture_raGL);

    raGL_texture_get_property(texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                              (void**) &texture_raGL_id);
    raGL_texture_get_property(texture_raGL,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                              (void**) &texture_raGL_is_rb);

    ASSERT_DEBUG_SYNC(!texture_raGL_is_rb,
                      "TODO"); /* NOTE: To invalidate a RB, we'd need to set up a FBO; probably not worth the effort. */

    for (uint32_t n_mip = command_ral_ptr->n_start_mip;
                  n_mip < command_ral_ptr->n_start_mip + command_ral_ptr->n_mips;
                ++n_mip)
    {
        _raGL_command* new_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

        new_command_ptr->invalidate_tex_image_command_info.level   = n_mip;
        new_command_ptr->invalidate_tex_image_command_info.texture = texture_raGL_id;
        new_command_ptr->type                                      = RAGL_COMMAND_TYPE_INVALIDATE_TEX_IMAGE;

        system_resizable_vector_push(commands,
                                     new_command_ptr);
    }
}

/** TODO */
void _raGL_command_buffer::process_set_binding_command(const ral_command_buffer_set_binding_command_info* command_ral_ptr)
{
    raGL_backend backend_gl = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_gl);

    ASSERT_DEBUG_SYNC(bake_state.active_program != nullptr,
                      "No active raGL_program assigned.");

    switch (command_ral_ptr->binding_type)
    {
        case RAL_BINDING_TYPE_RENDERTARGET:
        {
            ral_program                 active_program_ral = nullptr;
            const ral_program_variable* variable_ral_ptr   = nullptr;

            raGL_program_get_property(bake_state.active_program,
                                      RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                                     &active_program_ral);

            ral_program_get_output_variable_by_name(active_program_ral,
                                                    command_ral_ptr->name,
                                                   &variable_ral_ptr);

            ASSERT_DEBUG_SYNC(variable_ral_ptr != nullptr,
                              "No ral_program_variable instance exposed for output variable [%s]",
                              system_hashed_ansi_string_get_buffer(command_ral_ptr->name) );
            ASSERT_DEBUG_SYNC(variable_ral_ptr->location < N_MAX_RENDERTARGETS,
                              "Too many output variables defined in the shader.");

            ASSERT_DEBUG_SYNC(command_ral_ptr->rendertarget_binding.rt_index < N_MAX_RENDERTARGETS,
                              "Invalid rendertarget index specified");

            if (bake_state.active_fbo_draw_buffers[variable_ral_ptr->location] != (GL_COLOR_ATTACHMENT0 + command_ral_ptr->rendertarget_binding.rt_index))
            {
                bake_state.active_fbo_draw_buffers[variable_ral_ptr->location] = GL_COLOR_ATTACHMENT0 + command_ral_ptr->rendertarget_binding.rt_index;
                bake_state.active_fbo_draw_buffers_dirty                       = true;
            }

            break;
        }

        case RAL_BINDING_TYPE_SAMPLED_IMAGE:
        {
            _raGL_command*               active_texture_command_ptr      = nullptr;
            _raGL_command*               bind_sampler_command_ptr        = nullptr;
            _raGL_command*               bind_texture_command_ptr        = nullptr;
            raGL_sampler                 sampler_raGL                    = nullptr;
            GLuint                       sampler_raGL_id                 = 0;
            _raGL_command*               texture_parameterfv_command_ptr = nullptr;
            raGL_texture                 texture_raGL                    = nullptr;
            GLuint                       texture_raGL_id                 = 0;
            bool                         texture_raGL_is_rb;
            ral_texture                  texture_ral                     = nullptr;
            ral_texture_type             texture_ral_type;
            const raGL_program_variable* variable_raGL_ptr               = nullptr;

            raGL_program_get_uniform_by_name(bake_state.active_program,
                                             command_ral_ptr->name,
                                            &variable_raGL_ptr);

            ASSERT_DEBUG_SYNC(variable_raGL_ptr != nullptr,
                              "No _raGL_program_variable instance exposed for uniform [%s]",
                              system_hashed_ansi_string_get_buffer(command_ral_ptr->name) );

            ral_texture_view_get_property(command_ral_ptr->sampled_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                         &texture_ral);
            ral_texture_view_get_property(command_ral_ptr->sampled_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                         &texture_ral_type);

            raGL_backend_get_sampler(backend_gl,
                                     command_ral_ptr->sampled_image_binding.sampler,
                                    &sampler_raGL);
            raGL_backend_get_texture(backend_gl,
                                     texture_ral,
                                    &texture_raGL);

            raGL_sampler_get_property(sampler_raGL,
                                      RAGL_SAMPLER_PROPERTY_ID,
                                      (void**) &sampler_raGL_id);
            raGL_texture_get_property(texture_raGL,
                                      RAGL_TEXTURE_PROPERTY_ID,
                                      (void**) &texture_raGL_id);
            raGL_texture_get_property(texture_raGL,
                                      RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                      (void**) &texture_raGL_is_rb);

            ASSERT_DEBUG_SYNC(!texture_raGL_is_rb,
                              "Cannot use a renderbuffer for sampling purposes");

            /* Configure & enqueue relevant GL commands.
             *
             * NOTE: RAL specifies LOD bias as a sampler state, whereas in OpenGL this is a texture state.
             *       Hence, at binding time, we need to ensure LOD bias is kept up-to-date.
             **/
            GLfloat      sampler_lod_bias;
            const GLenum texture_target_gl = raGL_utils_get_ogl_enum_for_ral_texture_type(texture_ral_type);

            ral_sampler_get_property(command_ral_ptr->sampled_image_binding.sampler,
                                     RAL_SAMPLER_PROPERTY_LOD_BIAS,
                                    &sampler_lod_bias);

            active_texture_command_ptr      = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            bind_sampler_command_ptr        = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            bind_texture_command_ptr        = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
            texture_parameterfv_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            active_texture_command_ptr->active_texture_command_info.target = GL_TEXTURE0 + variable_raGL_ptr->texture_unit;
            active_texture_command_ptr->type                               = RAGL_COMMAND_TYPE_ACTIVE_TEXTURE;

            bind_sampler_command_ptr->bind_sampler_command_info.sampler_id = sampler_raGL_id;
            bind_sampler_command_ptr->bind_sampler_command_info.unit       = variable_raGL_ptr->texture_unit;
            bind_sampler_command_ptr->type                                 = RAGL_COMMAND_TYPE_BIND_SAMPLER;

            bind_texture_command_ptr->bind_texture_command_info.target = texture_target_gl;
            bind_texture_command_ptr->bind_texture_command_info.to_id  = texture_raGL_id;
            bind_texture_command_ptr->type                             = RAGL_COMMAND_TYPE_BIND_TEXTURE;

            texture_parameterfv_command_ptr->texture_parameterfv_command_info.pname    = GL_TEXTURE_LOD_BIAS;
            texture_parameterfv_command_ptr->texture_parameterfv_command_info.target   = texture_target_gl;
            texture_parameterfv_command_ptr->texture_parameterfv_command_info.texture  = texture_raGL_id;
            texture_parameterfv_command_ptr->texture_parameterfv_command_info.value[0] = sampler_lod_bias;
            texture_parameterfv_command_ptr->type                                      = RAGL_COMMAND_TYPE_TEXTURE_PARAMETERFV;

            system_resizable_vector_push(commands,
                                         active_texture_command_ptr);
            system_resizable_vector_push(commands,
                                         bind_sampler_command_ptr);
            system_resizable_vector_push(commands,
                                         bind_texture_command_ptr);
            system_resizable_vector_push(commands,
                                         texture_parameterfv_command_ptr);

            /* Update bake state */
            ASSERT_DEBUG_SYNC(variable_raGL_ptr->texture_unit < N_MAX_TEXTURE_UNITS,
                              "Requested texture unit (%d) exceeds N_MAX_TEXTURE_UNITS (%d)",
                              variable_raGL_ptr->texture_unit,
                              N_MAX_TEXTURE_UNITS);

            bake_state.active_texture_sampler_bindings[variable_raGL_ptr->texture_unit] = texture_raGL;

            break;
        }

        case RAL_BINDING_TYPE_STORAGE_BUFFER:
        case RAL_BINDING_TYPE_UNIFORM_BUFFER:
        {
            GLuint                                          bp                       = -1;
            const ral_command_buffer_buffer_binding_info&   buffer_binding_info      = (command_ral_ptr->binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? command_ral_ptr->storage_buffer_binding : command_ral_ptr->uniform_buffer_binding;
            const GLenum                                    buffer_binding_target    = (command_ral_ptr->binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? GL_SHADER_STORAGE_BUFFER                : GL_UNIFORM_BUFFER;
            raGL_buffer                                     buffer_raGL              = nullptr;
            GLuint                                          buffer_raGL_id           = 0;
            uint32_t                                        buffer_raGL_start_offset = -1;
            uint32_t                                        buffer_ral_size          = -1;
            uint32_t                                        buffer_ral_start_offset  = -1;
            const uint32_t                                  n_bps_supported          = (command_ral_ptr->binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? N_MAX_SB_BINDINGS : N_MAX_UB_BINDINGS;

            ral_buffer_get_property(buffer_binding_info.buffer,
                                    RAL_BUFFER_PROPERTY_START_OFFSET,
                                   &buffer_ral_start_offset);
            ral_buffer_get_property(buffer_binding_info.buffer,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &buffer_ral_size);

            raGL_backend_get_buffer(backend_gl,
                                    buffer_binding_info.buffer,
                                   &buffer_raGL);

            ASSERT_DEBUG_SYNC(buffer_raGL != nullptr,
                              "No raGL_buffer instance associated with the specified RAL buffer");

            raGL_buffer_get_property               (buffer_raGL,
                                                    RAGL_BUFFER_PROPERTY_ID,
                                                   &buffer_raGL_id);
            raGL_buffer_get_property               (buffer_raGL,
                                                    RAGL_BUFFER_PROPERTY_START_OFFSET,
                                                   &buffer_raGL_start_offset);

            raGL_program_get_block_property_by_name(bake_state.active_program,
                                                    command_ral_ptr->name,
                                                    RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                                   &bp);

            ASSERT_DEBUG_SYNC(bp != -1,
                              "Buffer name [%s] was not recognized by raGL_program.",
                              system_hashed_ansi_string_get_buffer(command_ral_ptr->name) );
            ASSERT_DEBUG_SYNC(bp < n_bps_supported,
                              "Too large binding point was requested.");

            /* Enqueue a GL command */
            _raGL_command* bind_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            bind_command_ptr->bind_buffer_range_command_info.bo_id    = buffer_raGL_id;
            bind_command_ptr->bind_buffer_range_command_info.bp_index = bp;
            bind_command_ptr->bind_buffer_range_command_info.offset   = buffer_binding_info.offset +
                                                                        buffer_raGL_start_offset   +
                                                                        buffer_ral_start_offset;
            bind_command_ptr->bind_buffer_range_command_info.size     = (buffer_binding_info.size == 0) ? buffer_ral_size :
                                                                                                          buffer_binding_info.size;
            bind_command_ptr->bind_buffer_range_command_info.target   = buffer_binding_target;
            bind_command_ptr->type                                    = RAGL_COMMAND_TYPE_BIND_BUFFER_RANGE;

            system_resizable_vector_push(commands,
                                         bind_command_ptr);

            /* Update internal bake state */
            _raGL_command_buffer_bake_state_buffer_binding& bake_state_bp = (command_ral_ptr->binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? bake_state.active_sb_bindings[bp]
                                                                                                                                               : bake_state.active_ub_bindings[bp];

            bake_state_bp.buffer = buffer_raGL;

            break;
        }

        case RAL_BINDING_TYPE_STORAGE_IMAGE:
        {
            raGL_texture                 parent_texture_raGL       = nullptr;
            GLuint                       parent_texture_raGL_id    = 0;
            bool                         parent_texture_raGL_is_rb = false;
            ral_texture                  parent_texture_ral        = nullptr;
            ral_format                   texture_view_format;
            GLuint                       texture_view_n_base_layer = -1;
            GLuint                       texture_view_n_base_level = -1;
            GLuint                       texture_view_n_layers     = 0;
            const raGL_program_variable* variable_ptr              = nullptr;

            raGL_program_get_uniform_by_name(bake_state.active_program,
                                             command_ral_ptr->name,
                                            &variable_ptr);

            ral_texture_view_get_property(command_ral_ptr->storage_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                         &texture_view_format);
            ral_texture_view_get_property(command_ral_ptr->storage_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_BASE_LAYER,
                                         &texture_view_n_base_layer);
            ral_texture_view_get_property(command_ral_ptr->storage_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_BASE_MIPMAP,
                                         &texture_view_n_base_level);
            ral_texture_view_get_property(command_ral_ptr->storage_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                         &texture_view_n_layers);
            ral_texture_view_get_property(command_ral_ptr->storage_image_binding.texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                         &parent_texture_ral);

            raGL_backend_get_texture(backend_gl,
                                     parent_texture_ral,
                                    &parent_texture_raGL);

            ASSERT_DEBUG_SYNC(parent_texture_raGL != nullptr,
                              "Null raGL_texture reported for a RAL texture");


            raGL_texture_get_property(parent_texture_raGL,
                                      RAGL_TEXTURE_PROPERTY_ID,
                                      (void**) &parent_texture_raGL_id);
            raGL_texture_get_property(parent_texture_raGL,
                                      RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                      (void**) &parent_texture_raGL_is_rb);

            ASSERT_DEBUG_SYNC(!parent_texture_raGL_is_rb,
                              "Cannot bind a renderbuffer to a storage image binding.");

            /* Bind the image */
            _raGL_command* bind_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

            bind_command_ptr->bind_image_texture_command_info.access     = ((command_ral_ptr->storage_image_binding.access_bits & RAL_IMAGE_ACCESS_READ) != 0 && (command_ral_ptr->storage_image_binding.access_bits & RAL_IMAGE_ACCESS_WRITE) == 0) ? GL_READ_ONLY
                                                                         : ((command_ral_ptr->storage_image_binding.access_bits & RAL_IMAGE_ACCESS_READ) != 0 && (command_ral_ptr->storage_image_binding.access_bits & RAL_IMAGE_ACCESS_WRITE) != 0) ? GL_READ_WRITE
                                                                                                                                                                                                                                                     : GL_WRITE_ONLY;
            bind_command_ptr->bind_image_texture_command_info.format     = raGL_utils_get_ogl_enum_for_ral_format(texture_view_format);
            bind_command_ptr->bind_image_texture_command_info.is_layered = (texture_view_n_layers > 1) ? GL_TRUE : GL_FALSE;
            bind_command_ptr->bind_image_texture_command_info.layer      = texture_view_n_base_layer;
            bind_command_ptr->bind_image_texture_command_info.level      = texture_view_n_base_level;
            bind_command_ptr->bind_image_texture_command_info.to_id      = parent_texture_raGL_id;
            bind_command_ptr->bind_image_texture_command_info.tu_index   = variable_ptr->image_unit;
            bind_command_ptr->type                                       = RAGL_COMMAND_TYPE_BIND_IMAGE_TEXTURE;

            system_resizable_vector_push(commands,
                                         bind_command_ptr);

            /* Update internal bake state */
            bake_state.active_image_bindings[variable_ptr->image_unit] = parent_texture_raGL;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL binding type");
        }
    }
}

/** TODO */
void _raGL_command_buffer::process_set_color_rendertarget_command(const ral_command_buffer_set_color_rendertarget_command_info* command_ral_ptr)
{
    ASSERT_DEBUG_SYNC(command_ral_ptr->rendertarget_index < N_MAX_RENDERTARGETS,
                      "Too many rendertargets requested.");

    bake_state.active_rt_attachments_dirty                                      = true;
    bake_state.active_rt_color_attachments[command_ral_ptr->rendertarget_index] = _raGL_command_buffer_bake_state_rt(*command_ral_ptr);
}

/** TODO */
void _raGL_command_buffer::process_set_depth_rendertarget_command(const ral_command_buffer_set_depth_rendertarget_command_info* command_ral_ptr)
{
    if (bake_state.active_rt_ds_attachment != command_ral_ptr->depth_rt)
    {
        bake_state.active_rt_attachments_dirty = true;
        bake_state.active_rt_ds_attachment     = command_ral_ptr->depth_rt;
    }
}

/** TODO */
void _raGL_command_buffer::process_set_gfx_state_command(const ral_command_buffer_set_gfx_state_command_info* command_ral_ptr)
{
    ASSERT_DEBUG_SYNC(command_ral_ptr->new_state != nullptr,
                      "NULL ral_gfx_state instance specified.");

    if (command_ral_ptr->new_state != bake_state.active_gfx_state)
    {
        /* Do not fire any GL calls yet. Cache the new gfx_state and wait until actual draw call. */
        bake_state.active_gfx_state       = command_ral_ptr->new_state;
        bake_state.active_gfx_state_dirty = true;
    }
}

/** TODO */
void _raGL_command_buffer::process_set_program_command(const ral_command_buffer_set_program_command_info* command_ral_ptr)
{
    raGL_backend backend_gl      = nullptr;
    raGL_program program_raGL    = nullptr;
    GLuint       program_raGL_id = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_gl);

    raGL_backend_get_program(backend_gl,
                             command_ral_ptr->new_program,
                            &program_raGL);

    ASSERT_DEBUG_SYNC(program_raGL != nullptr,
                      "No raGL_program instance associated with the specified RAL program");

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    /* Enqueue the GL command */
    _raGL_command* gl_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    gl_command_ptr->use_program_command_info.po_id = program_raGL_id;
    gl_command_ptr->type                           = RAGL_COMMAND_TYPE_USE_PROGRAM;

    system_resizable_vector_push(commands,
                                 gl_command_ptr);

    bake_state.active_program = program_raGL;
}

/** TODO */
void _raGL_command_buffer::process_set_scissor_box_command(const ral_command_buffer_set_scissor_box_command_info* command_ral_ptr)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(command_ral_ptr->index < static_cast<uint32_t>(limits_ptr->max_viewports),
                      "Invalid scissor box index");

    if (bake_state.active_gfx_state != nullptr)
    {
        bool static_scissor_boxes_enabled;

        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES_ENABLED,
                                  &static_scissor_boxes_enabled);

        ASSERT_DEBUG_SYNC(!static_scissor_boxes_enabled,
                          "Illegal \"set scissor box\" command request - bound gfx_state has the \"static scissor boxes\" mode enabled.");
    }

    /* Enqueue the GL command */
    _raGL_command* gl_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    gl_command_ptr->scissor_indexedv_command_info.index = command_ral_ptr->index;
    gl_command_ptr->scissor_indexedv_command_info.v[0]  = command_ral_ptr->xy  [0];
    gl_command_ptr->scissor_indexedv_command_info.v[1]  = command_ral_ptr->xy  [1];
    gl_command_ptr->scissor_indexedv_command_info.v[2]  = command_ral_ptr->size[0];
    gl_command_ptr->scissor_indexedv_command_info.v[3]  = command_ral_ptr->size[1];
    gl_command_ptr->type                                = RAGL_COMMAND_TYPE_SCISSOR_INDEXEDV;

    memcpy(bake_state.active_scissor_boxes[command_ral_ptr->index].size,
           command_ral_ptr->size,
           sizeof(bake_state.active_scissor_boxes[command_ral_ptr->index].size) );
    memcpy(bake_state.active_scissor_boxes[command_ral_ptr->index].xy,
           command_ral_ptr->xy,
           sizeof(bake_state.active_scissor_boxes[command_ral_ptr->index].xy) );

    system_resizable_vector_push(commands,
                                 gl_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_set_vertex_buffer_command(const ral_command_buffer_set_vertex_buffer_command_info* command_ral_ptr)
{
    /* RAL holds most of the VAA state in gfx_state container. Furthermore, it doesn't directly
     * map to vertex arrays & vertex buffers we have in GL.
     *
     * In order to avoid doing insensible bind calls all the time, we cache configured VA state
     * and bind corresponding VAOs at draw call time. */
    const raGL_program_attribute* attribute_ptr            = nullptr;
    raGL_backend                  backend_raGL             = nullptr;
    raGL_buffer                   buffer_raGL              = nullptr;
    uint32_t                      buffer_raGL_start_offset = 0;
    uint32_t                      buffer_ral_start_offset  = 0;

    raGL_program_get_vertex_attribute_by_name(bake_state.active_program,
                                              command_ral_ptr->name,
                                             &attribute_ptr);

    ASSERT_DEBUG_SYNC(attribute_ptr != nullptr,
                      "Invalid vertex attribute requested.");

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_raGL);

    raGL_backend_get_buffer(backend_raGL,
                            command_ral_ptr->buffer,
                           &buffer_raGL);

    ASSERT_DEBUG_SYNC(buffer_raGL != nullptr,
                      "No raGL buffer instance found for the specified RAL buffer instance.");

    raGL_buffer_get_property(buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &buffer_raGL_start_offset);
    ral_buffer_get_property (command_ral_ptr->buffer,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &buffer_ral_start_offset);

    if (bake_state.vbs[attribute_ptr->location].buffer_raGL  != buffer_raGL                                                                        ||
        bake_state.vbs[attribute_ptr->location].start_offset != command_ral_ptr->start_offset + buffer_raGL_start_offset + buffer_ral_start_offset)
    {
        /* Need to update the VA configuration */
        bake_state.vbs[attribute_ptr->location].buffer_raGL  = buffer_raGL;
        bake_state.vbs[attribute_ptr->location].start_offset = command_ral_ptr->start_offset + buffer_raGL_start_offset + buffer_ral_start_offset;
        bake_state.vao_dirty                                 = true;
    }
}

/** TODO */
void _raGL_command_buffer::process_set_viewport_command(const ral_command_buffer_set_viewport_command_info* command_ral_ptr)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(command_ral_ptr->index < static_cast<uint32_t>(limits_ptr->max_viewports),
                      "Invalid viewport index");

    if (bake_state.active_gfx_state != nullptr)
    {
        bool static_viewports_enabled;

        ral_gfx_state_get_property(bake_state.active_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS_ENABLED,
                                  &static_viewports_enabled);

        ASSERT_DEBUG_SYNC(!static_viewports_enabled,
                          "Illegal \"set viewport\" command request - bound gfx_state has the \"static viewports\" mode enabled.");
    }

    /* Enqueue the GL command */
    _raGL_command* depth_range_indexed_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );
    _raGL_command* viewport_indexedfv_command_ptr  = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    depth_range_indexed_command_ptr->depth_range_indexed_command_info.index   = command_ral_ptr->index;
    depth_range_indexed_command_ptr->depth_range_indexed_command_info.nearVal = command_ral_ptr->depth_range[0];
    depth_range_indexed_command_ptr->depth_range_indexed_command_info.farVal  = command_ral_ptr->depth_range[1];
    depth_range_indexed_command_ptr->type                                     = RAGL_COMMAND_TYPE_DEPTH_RANGE_INDEXED;

    viewport_indexedfv_command_ptr->viewport_indexedfv_command_info.index = command_ral_ptr->index;
    viewport_indexedfv_command_ptr->viewport_indexedfv_command_info.v[0]  = command_ral_ptr->xy  [0];
    viewport_indexedfv_command_ptr->viewport_indexedfv_command_info.v[1]  = command_ral_ptr->xy  [1];
    viewport_indexedfv_command_ptr->viewport_indexedfv_command_info.v[2]  = command_ral_ptr->size[0];
    viewport_indexedfv_command_ptr->viewport_indexedfv_command_info.v[3]  = command_ral_ptr->size[1];
    viewport_indexedfv_command_ptr->type                                  = RAGL_COMMAND_TYPE_VIEWPORT_INDEXEDFV;

    system_resizable_vector_push(commands,
                                 depth_range_indexed_command_ptr);
    system_resizable_vector_push(commands,
                                 viewport_indexedfv_command_ptr);
}

/** TODO */
void _raGL_command_buffer::process_update_buffer_command(const ral_command_buffer_update_buffer_command_info* command_ral_ptr)
{
    raGL_backend backend_gl               = nullptr;
    raGL_buffer  buffer_raGL              = nullptr;
    GLuint       buffer_raGL_id           = 0;
    uint32_t     buffer_raGL_start_offset = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &backend_gl);

    raGL_backend_get_buffer(backend_gl,
                            command_ral_ptr->buffer,
                           &buffer_raGL);

    ASSERT_DEBUG_SYNC(buffer_raGL != nullptr,
                      "No raGL_buffer instance associated with the specified RAL buffer");

    raGL_buffer_get_property(buffer_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &buffer_raGL_id);
    raGL_buffer_get_property(buffer_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &buffer_raGL_start_offset);

    /* Enqueue a corresponding GL command */
    _raGL_command* named_buffer_sub_data_command_ptr = reinterpret_cast<_raGL_command*>(system_resource_pool_get_from_pool(command_pool) );

    named_buffer_sub_data_command_ptr->named_buffer_sub_data_command_info.bo_id  = buffer_raGL_id ;
    named_buffer_sub_data_command_ptr->named_buffer_sub_data_command_info.data   = (command_ral_ptr->alloced_data != nullptr) ? command_ral_ptr->alloced_data
                                                                                                                              : command_ral_ptr->preallocated_data;
    named_buffer_sub_data_command_ptr->named_buffer_sub_data_command_info.offset = command_ral_ptr->start_offset + buffer_raGL_start_offset;
    named_buffer_sub_data_command_ptr->named_buffer_sub_data_command_info.size   = command_ral_ptr->size;
    named_buffer_sub_data_command_ptr->type                                      = RAGL_COMMAND_TYPE_NAMED_BUFFER_SUB_DATA;

    system_resizable_vector_push(commands,
                                 named_buffer_sub_data_command_ptr);
}

/** TODO */
PRIVATE void _raGL_command_buffer_deinit_command(system_resource_pool_block block)
{
    _raGL_command* command_ptr = reinterpret_cast<_raGL_command*>(block);

    command_ptr->deinit();
}

/** TODO */
PRIVATE void _raGL_command_buffer_deinit_command_buffer(system_resource_pool_block block)
{
    _raGL_command_buffer* cmd_buffer_ptr = reinterpret_cast<_raGL_command_buffer*>(block); 

    if (cmd_buffer_ptr->commands != nullptr)
    {
        system_resizable_vector_release(cmd_buffer_ptr->commands);

        cmd_buffer_ptr->commands = nullptr;
    }
}

/** TODO */
PRIVATE void _raGL_command_buffer_init_command_buffer(system_resource_pool_block block)
{
    _raGL_command_buffer* cmd_buffer_ptr = reinterpret_cast<_raGL_command_buffer*>(block);

    cmd_buffer_ptr->commands = system_resizable_vector_create(32);
    cmd_buffer_ptr->context  = nullptr;
}


/** Please see header for specification */
PUBLIC raGL_command_buffer raGL_command_buffer_create(ral_command_buffer command_buffer_ral,
                                                      ogl_context        context)
{
    _raGL_command_buffer* new_command_buffer_ptr = nullptr;

    new_command_buffer_ptr = reinterpret_cast<_raGL_command_buffer*>(system_resource_pool_get_from_pool(command_buffer_pool) );

    if (new_command_buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(new_command_buffer_ptr != nullptr,
                          "Could not retrieve a command buffer from the command buffer pool");

        goto end;
    }

    new_command_buffer_ptr->init          (context);
    new_command_buffer_ptr->clear_commands();

    new_command_buffer_ptr->command_buffer_ral = command_buffer_ral;
    new_command_buffer_ptr->context            = context;
end:
    return reinterpret_cast<raGL_command_buffer>(new_command_buffer_ptr);
}

/** Please see header for specification */
PUBLIC void raGL_command_buffer_deinit()
{
    system_resource_pool_release(command_buffer_pool);
    system_resource_pool_release(command_pool);

    command_buffer_pool = nullptr;
    command_pool        = nullptr;
}

/** Please see header for specification */
PUBLIC void raGL_command_buffer_execute(raGL_command_buffer command_buffer,
                                        raGL_dep_tracker    dep_tracker)
{
    _raGL_command_buffer* command_buffer_ptr = reinterpret_cast<_raGL_command_buffer*>(command_buffer);
    uint32_t              n_commands         = 0;

    system_resizable_vector_get_property(command_buffer_ptr->commands,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_commands);

    for (uint32_t n_command = 0;
                  n_command < n_commands;
                ++n_command)
    {
        _raGL_command* command_ptr = nullptr;

        system_resizable_vector_get_element_at(command_buffer_ptr->commands,
                                               n_command,
                                              &command_ptr);

        /* Retrieve current GFX state, if needs be */
        switch (command_ptr->type)
        {
            case RAGL_COMMAND_TYPE_DRAW_ARRAYS:
            case RAGL_COMMAND_TYPE_DRAW_ARRAYS_INDIRECT:
            case RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED:
            case RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_BASE_VERTEX:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INDIRECT:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_INSTANCE:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX:
            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE:
            case RAGL_COMMAND_TYPE_MULTI_DRAW_ARRAYS_INDIRECT:
            case RAGL_COMMAND_TYPE_MULTI_DRAW_ELEMENTS_INDIRECT:
            {
                raGL_backend backend = nullptr;

                ogl_context_get_property(command_buffer_ptr->context,
                                         OGL_CONTEXT_PROPERTY_BACKEND,
                                        &backend);

                ASSERT_DEBUG_SYNC(command_buffer_ptr->bake_state.active_gfx_state != nullptr,
                                  "No GFX state assigned to the command buffer");
            }
        }

        /* Execute GL command(s) for the RAL command */
        switch (command_ptr->type)
        {
            case RAGL_COMMAND_TYPE_ACTIVE_TEXTURE:
            {
                const _raGL_command_active_texture_command_info& command_args = command_ptr->active_texture_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLActiveTexture(command_args.target);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_BUFFER:
            {
                const _raGL_command_bind_buffer_command_info& command_args = command_ptr->bind_buffer_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindBuffer(command_args.target,
                                                                   command_args.bo_id);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_BUFFER_BASE:
            {
                const _raGL_command_bind_buffer_base_command_info& command_args = command_ptr->bind_buffer_base_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindBufferBase(command_args.target,
                                                                       command_args.bp_index,
                                                                       command_args.bo_id);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_BUFFER_RANGE:
            {
                const _raGL_command_bind_buffer_range_command_info& command_args = command_ptr->bind_buffer_range_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindBufferRange(command_args.target,
                                                                        command_args.bp_index,
                                                                        command_args.bo_id,
                                                                        command_args.offset,
                                                                        command_args.size);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_FRAMEBUFFER:
            {
                const _raGL_command_bind_framebuffer_command_info& command_args = command_ptr->bind_framebuffer_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindFramebuffer(command_args.target,
                                                                        command_args.framebuffer);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_IMAGE_TEXTURE:
            {
                const _raGL_command_bind_image_texture_command_info& command_args = command_ptr->bind_image_texture_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindImageTexture(command_args.tu_index,
                                                                         command_args.to_id,
                                                                         command_args.level,
                                                                         command_args.is_layered,
                                                                         command_args.layer,
                                                                         command_args.access,
                                                                         command_args.format);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_SAMPLER:
            {
                const _raGL_command_bind_sampler_command_info& command_args = command_ptr->bind_sampler_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindSampler(command_args.unit,
                                                                    command_args.sampler_id);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_TEXTURE:
            {
                const _raGL_command_bind_texture_command_info& command_args = command_ptr->bind_texture_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindTexture(command_args.target,
                                                                    command_args.to_id);

                break;
            }

            case RAGL_COMMAND_TYPE_BIND_VERTEX_ARRAY:
            {
                const _raGL_command_bind_vertex_array_command_info& command_args = command_ptr->bind_vertex_array_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindVertexArray(command_args.vao_id);

                break;
            }

            case RAGL_COMMAND_TYPE_BLEND_COLOR:
            {
                const _raGL_command_blend_color_command_info& command_args = command_ptr->blend_color_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBlendColor(command_args.rgba[0],
                                                                   command_args.rgba[1],
                                                                   command_args.rgba[2],
                                                                   command_args.rgba[3]);

                break;
            }

            case RAGL_COMMAND_TYPE_BLEND_EQUATION_SEPARATE:
            {
                const _raGL_command_blend_equation_separate_command_info& command_args = command_ptr->blend_equation_separate_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBlendEquationSeparate(command_args.modeRGB,
                                                                              command_args.modeAlpha);

                break;
            }

            case RAGL_COMMAND_TYPE_BLEND_FUNC_SEPARATE:
            {
                const _raGL_command_blend_func_separate_command_info& command_args = command_ptr->blend_func_separate_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBlendFuncSeparate(command_args.srcRGB,
                                                                          command_args.dstRGB,
                                                                          command_args.srcAlpha,
                                                                          command_args.dstAlpha);

                break;
            }

            case RAGL_COMMAND_TYPE_BLIT_FRAMEBUFFER:
            {
                const _raGL_command_blit_framebuffer_command_info& command_args = command_ptr->blit_framebuffer_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                                        command_args.draw_fbo_id);
                command_buffer_ptr->entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                                        command_args.read_fbo_id);

                command_buffer_ptr->entrypoints_ptr->pGLBlitFramebuffer(command_args.src_x0y0x1y1[0],
                                                                        command_args.src_x0y0x1y1[1],
                                                                        command_args.src_x0y0x1y1[2],
                                                                        command_args.src_x0y0x1y1[3],
                                                                        command_args.dst_x0y0x1y1[0],
                                                                        command_args.dst_x0y0x1y1[1],
                                                                        command_args.dst_x0y0x1y1[2],
                                                                        command_args.dst_x0y0x1y1[3],
                                                                        command_args.mask,
                                                                        command_args.filter);

                break;
            }

            case RAGL_COMMAND_TYPE_CLEAR:
            {
                const _raGL_command_clear_command_info& command_args = command_ptr->clear_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLClear(command_args.mask);

                break;
            }

            case RAGL_COMMAND_TYPE_CLEAR_BUFFER_SUB_DATA:
            {
                const _raGL_command_clear_buffer_sub_data_command_info& command_args = command_ptr->clear_buffer_sub_data_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLClearBufferSubData(command_args.target,
                                                                           command_args.internalformat,
                                                                           command_args.start_offset,
                                                                           command_args.size,
                                                                           command_args.format,
                                                                           command_args.type,
                                                                           command_args.data);

                break;
            }

            case RAGL_COMMAND_TYPE_CLEAR_COLOR:
            {
                const _raGL_command_clear_color_command_info& command_args = command_ptr->clear_color_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLClearColor(command_args.rgba[0],
                                                                   command_args.rgba[1],
                                                                   command_args.rgba[2],
                                                                   command_args.rgba[3]);

                break;
            }

            case RAGL_COMMAND_TYPE_CLEAR_DEPTHF:
            {
                const _raGL_command_clear_depthf_command_info& command_args = command_ptr->clear_depthf_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLClearDepthf(command_args.depth);

                break;
            }

            case RAGL_COMMAND_TYPE_CLEAR_STENCIL:
            {
                const _raGL_command_clear_stencil_command_info& command_args = command_ptr->clear_stencil_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLClearStencil(command_args.stencil);

                break;
            }

            case RAGL_COMMAND_TYPE_COPY_BUFFER_SUB_DATA:
            {
                const _raGL_command_copy_buffer_sub_data_command_info& command_args = command_ptr->copy_buffer_sub_data_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLCopyBufferSubData(command_args.read_target,
                                                                          command_args.write_target,
                                                                          command_args.read_offset,
                                                                          command_args.write_offset,
                                                                          command_args.size);

                break;
            }

            case RAGL_COMMAND_TYPE_COPY_IMAGE_SUB_DATA:
            {
                const _raGL_command_copy_image_sub_data_command_info& command_args = command_ptr->copy_image_sub_data_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLCopyImageSubData(command_args.src_object_id,
                                                                         command_args.src_target,
                                                                         command_args.src_level,
                                                                         command_args.src_xyz[0],
                                                                         command_args.src_xyz[1],
                                                                         command_args.src_xyz[2],
                                                                         command_args.dst_object_id,
                                                                         command_args.dst_target,
                                                                         command_args.dst_level,
                                                                         command_args.dst_xyz[0],
                                                                         command_args.dst_xyz[1],
                                                                         command_args.dst_xyz[2],
                                                                         command_args.size[0],
                                                                         command_args.size[1],
                                                                         command_args.size[2]);

                break;
            }

            case RAGL_COMMAND_TYPE_CULL_FACE:
            {
                const _raGL_command_cull_face_command_info& command_args = command_ptr->cull_face_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLCullFace(command_args.mode);

                break;
            }

            case RAGL_COMMAND_TYPE_DEPTH_FUNC:
            {
                const _raGL_command_depth_func_command_info& command_args = command_ptr->depth_func_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDepthFunc(command_args.func);

                break;
            }

            case RAGL_COMMAND_TYPE_DEPTH_MASK:
            {
                const _raGL_command_depth_mask_command_info& command_args = command_ptr->depth_mask_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDepthMask(command_args.flag);

                break;
            }

            case RAGL_COMMAND_TYPE_DEPTH_RANGE_INDEXED:
            {
                const _raGL_command_depth_range_indexed_command_info& command_args = command_ptr->depth_range_indexed_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDepthRangeIndexed(command_args.index,
                                                                          command_args.nearVal,
                                                                          command_args.farVal);

                break;
            }

            case RAGL_COMMAND_TYPE_DISABLE:
            {
                const _raGL_command_disable_command_info& command_args = command_ptr->disable_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDisable(command_args.capability);

                break;
            }

            case RAGL_COMMAND_TYPE_DISABLE_VERTEX_ATTRIB_ARRAY:
            {
                const _raGL_command_disable_vertex_attrib_array_command_info& command_args = command_ptr->disable_vertex_attrib_array_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDisableVertexAttribArray(command_args.index);

                break;
            }

            case RAGL_COMMAND_TYPE_DISPATCH:
            {
                const _raGL_command_dispatch_command_info& command_args = command_ptr->dispatch_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDispatchCompute(command_args.x,
                                                                        command_args.y,
                                                                        command_args.z);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ARRAYS:
            {
                const _raGL_command_draw_arrays_command_info& command_args = command_ptr->draw_arrays_command_info;
                ral_primitive_type                            primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawArrays(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                   command_args.first,
                                                                   command_args.count);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ARRAYS_INDIRECT:
            {
                const _raGL_command_draw_arrays_indirect_command_info& command_args = command_ptr->draw_arrays_indirect_command_info;
                ral_primitive_type                                     primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawArraysIndirect(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                           command_args.indirect);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED:
            {
                const _raGL_command_draw_arrays_instanced_command_info& command_args = command_ptr->draw_arrays_instanced_command_info;
                ral_primitive_type                                      primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawArraysInstanced(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                            command_args.first,
                                                                            command_args.count,
                                                                            command_args.primcount);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ARRAYS_INSTANCED_BASE_INSTANCE:
            {
                const _raGL_command_draw_arrays_instanced_base_instance_command_info& command_args = command_ptr->draw_arrays_instanced_base_instance_command_info;
                ral_primitive_type                                                    primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawArraysInstancedBaseInstance(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                        command_args.first,
                                                                                        command_args.count,
                                                                                        command_args.primcount,
                                                                                        command_args.base_instance);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_BUFFERS:
            {
                const _raGL_command_draw_buffers_command_info& command_args = command_ptr->draw_buffers_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLDrawBuffers(command_args.n,
                                                                    command_args.bufs);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS:
            {
                const _raGL_command_draw_elements_command_info& command_args = command_ptr->draw_elements_command_info;
                ral_primitive_type                              primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLMultiDrawElements(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                         &command_args.count,
                                                                          command_args.type,
                                                                         &command_args.indices,
                                                                          1); /* primcount */

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_BASE_VERTEX:
            {
                const _raGL_command_draw_elements_base_vertex_command_info& command_args = command_ptr->draw_elements_base_vertex_command_info;
                ral_primitive_type                                          primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLMultiDrawElementsBaseVertex(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                   &command_args.count,
                                                                                    command_args.type,
                                                                                   &command_args.indices,
                                                                                    1, /* primcount */
                                                                                   &command_args.base_vertex);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INDIRECT:
            {
                const _raGL_command_draw_elements_indirect_command_info& command_args = command_ptr->draw_elements_indirect_command_info;
                ral_primitive_type                                       primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLMultiDrawElementsIndirect(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                  command_args.type,
                                                                                  nullptr, /* indirect  */
                                                                                  1,       /* drawcount */
                                                                                  0);      /* stride    */

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED:
            {
                const _raGL_command_draw_elements_instanced_command_info command_args = command_ptr->draw_elements_instanced_command_info;
                ral_primitive_type                                       primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawElementsInstanced(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                              command_args.count,
                                                                              command_args.type,
                                                                              command_args.indices,
                                                                              command_args.primcount);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_INSTANCE:
            {
                const _raGL_command_draw_elements_instanced_base_instance_command_info& command_args = command_ptr->draw_elements_instanced_base_instance_command_info;
                ral_primitive_type                                                      primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawElementsInstancedBaseInstance(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                          command_args.count,
                                                                                          command_args.type,
                                                                                          command_args.indices,
                                                                                          command_args.primcount,
                                                                                          command_args.base_instance);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX:
            {
                const _raGL_command_draw_elements_instanced_base_vertex_command_info& command_args = command_ptr->draw_elements_instanced_base_vertex_command_info;
                ral_primitive_type                                                    primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawElementsInstancedBaseVertex(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                        command_args.count,
                                                                                        command_args.type,
                                                                                        command_args.indices,
                                                                                        command_args.primcount,
                                                                                        command_args.base_vertex);

                break;
            }

            case RAGL_COMMAND_TYPE_DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE:
            {
                const _raGL_command_draw_elements_instanced_base_vertex_base_instance_command_info& command_args = command_ptr->draw_elements_instanced_base_vertex_base_instance_command_info;
                ral_primitive_type                                                                  primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLDrawElementsInstancedBaseVertexBaseInstance(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                                    command_args.count,
                                                                                                    command_args.type,
                                                                                                    command_args.indices,
                                                                                                    command_args.primcount,
                                                                                                    command_args.base_vertex,
                                                                                                    command_args.base_instance);

                break;
            }

            case RAGL_COMMAND_TYPE_ENABLE:
            {
                const _raGL_command_enable_command_info& command_args = command_ptr->enable_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLEnable(command_args.capability);

                break;
            }

            case RAGL_COMMAND_TYPE_ENABLE_VERTEX_ATTRIB_ARRAY:
            {
                const _raGL_command_enable_vertex_attrib_array_command_info& command_args = command_ptr->enable_vertex_attrib_array_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLEnableVertexAttribArray(command_args.index);

                break;
            }

            case RAGL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER:
            {
                raGL_command_buffer_execute(command_ptr->execute_command_buffer_command_info.command_buffer_raGL,
                                            dep_tracker);

                break;
            }

            case RAGL_COMMAND_TYPE_FRONT_FACE:
            {
                const _raGL_command_front_face_command_info& command_args = command_ptr->front_face_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLFrontFace(command_args.mode);

                break;
            }

            case RAGL_COMMAND_TYPE_INVALIDATE_TEX_IMAGE:
            {
                const _raGL_command_invalidate_tex_image_command_info& command_args = command_ptr->invalidate_tex_image_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLInvalidateTexImage(command_args.texture,
                                                                           command_args.level);

                break;
            }

            case RAGL_COMMAND_TYPE_LINE_WIDTH:
            {
                const _raGL_command_line_width_command_info& command_args = command_ptr->line_width_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLLineWidth(command_args.width);

                break;
            }

            case RAGL_COMMAND_TYPE_LOGIC_OP:
            {
                const _raGL_command_logic_op_command_info& command_args = command_ptr->logic_op_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLLogicOp(command_args.opcode);

                break;
            }

            case RAGL_COMMAND_TYPE_MEMORY_BARRIER:
            {
                const _raGL_command_memory_barrier_command_info& command_args = command_ptr->memory_barriers_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLMemoryBarrier(command_args.barriers);

                break;
            }

            case RAGL_COMMAND_TYPE_MIN_SAMPLE_SHADING:
            {
                const _raGL_command_min_sample_shading_command_info& command_args = command_ptr->min_sample_shading_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLMinSampleShading(command_args.value);

                break;
            }

            case RAGL_COMMAND_TYPE_MULTI_DRAW_ARRAYS_INDIRECT:
            {
                const _raGL_command_multi_draw_arrays_indirect_command_info& command_args = command_ptr->multi_draw_arrays_indirect_command_info;
                ral_primitive_type                                           primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLMultiDrawArraysIndirect(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                command_args.indirect,
                                                                                command_args.drawcount,
                                                                                command_args.stride);

                break;
            }

            case RAGL_COMMAND_TYPE_MULTI_DRAW_ELEMENTS_INDIRECT:
            {
                const _raGL_command_multi_draw_elements_indirect_command_info& command_args = command_ptr->multi_draw_elements_indirect_command_info;
                ral_primitive_type                                             primitive_type_ral;

                ral_gfx_state_get_property(command_buffer_ptr->bake_state.active_gfx_state,
                                           RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,
                                          &primitive_type_ral);

                command_buffer_ptr->entrypoints_ptr->pGLMultiDrawElementsIndirect(raGL_utils_get_ogl_enum_for_ral_primitive_type(primitive_type_ral),
                                                                                  command_args.type,
                                                                                  command_args.indirect,
                                                                                  command_args.drawcount,
                                                                                  command_args.stride);

                break;
            }

            case RAGL_COMMAND_TYPE_NAMED_BUFFER_SUB_DATA:
            {
                const _raGL_command_named_buffer_sub_data_command_info& command_args = command_ptr->named_buffer_sub_data_command_info;

                command_buffer_ptr->entrypoints_dsa_ptr->pGLNamedBufferSubDataEXT(command_args.bo_id,
                                                                                  command_args.offset,
                                                                                  command_args.size,
                                                                                  command_args.data);

                break;
            }

            case RAGL_COMMAND_TYPE_PATCH_PARAMETERI:
            {
                const _raGL_command_patch_parameteri_command_info& command_args = command_ptr->patch_parameteri_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLPatchParameteri(command_args.pname,
                                                                        command_args.value);

                break;
            }

            case RAGL_COMMAND_TYPE_POLYGON_MODE:
            {
                const _raGL_command_polygon_mode_command_info& command_args = command_ptr->polygon_mode_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLPolygonMode(command_args.face,
                                                                    command_args.mode);

                break;
            }

            case RAGL_COMMAND_TYPE_POLYGON_OFFSET:
            {
                const _raGL_command_polygon_offset_command_info& command_args = command_ptr->polygon_offset_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLPolygonOffset(command_args.factor,
                                                                      command_args.units);

                break;
            }

            case RAGL_COMMAND_TYPE_SCISSOR_INDEXEDV:
            {
                const _raGL_command_scissor_indexedv_command_info& command_args = command_ptr->scissor_indexedv_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLScissorIndexedv(command_args.index,
                                                                        command_args.v);

                break;
            }

            case RAGL_COMMAND_TYPE_STENCIL_FUNC_SEPARATE:
            {
                const _raGL_command_stencil_func_separate_command_info& command_args = command_ptr->stencil_func_separate_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLStencilFuncSeparate(command_args.face,
                                                                            command_args.func,
                                                                            command_args.ref,
                                                                            command_args.mask);

                break;
            }

            case RAGL_COMMAND_TYPE_STENCIL_MASK_SEPARATE:
            {
                const _raGL_command_stencil_mask_separate_command_info& command_args = command_ptr->stencil_mask_separate_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLStencilMaskSeparate(command_args.face,
                                                                            command_args.mask);

                break;
            }

            case RAGL_COMMAND_TYPE_STENCIL_OP_SEPARATE:
            {
                const _raGL_command_stencil_op_separate_command_info& command_args = command_ptr->stencil_op_separate_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLStencilOpSeparate(command_args.face,
                                                                          command_args.sfail,
                                                                          command_args.dpfail,
                                                                          command_args.dppass);

                break;
            }

            case RAGL_COMMAND_TYPE_TEXTURE_PARAMETERFV:
            {
                const _raGL_command_texture_parameterfv_command_info& command_args = command_ptr->texture_parameterfv_command_info;

                command_buffer_ptr->entrypoints_dsa_ptr->pGLTextureParameterfvEXT(command_args.texture,
                                                                                  command_args.target,
                                                                                  command_args.pname,
                                                                                  command_args.value);

                break;
            }

            case RAGL_COMMAND_TYPE_USE_PROGRAM:
            {
                const _raGL_command_use_program_command_info& command_args = command_ptr->use_program_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLUseProgram(command_args.po_id);

                break;
            }

            case RAGL_COMMAND_TYPE_VIEWPORT_INDEXEDFV:
            {
                const _raGL_command_viewport_indexedfv_command_info& command_args = command_ptr->viewport_indexedfv_command_info;

                command_buffer_ptr->entrypoints_ptr->pGLViewportIndexedfv(command_args.index,
                                                                          command_args.v);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized raGL command type");
            }
        }
    }
}

/** Please see header for specification */
PUBLIC void raGL_command_buffer_init()
{
    command_buffer_pool = system_resource_pool_create(sizeof(_raGL_command_buffer),
                                                      32 /* n_elements_to_preallocate */,
                                                      _raGL_command_buffer_init_command_buffer,
                                                      _raGL_command_buffer_deinit_command_buffer);
    command_pool        = system_resource_pool_create(sizeof(_raGL_command),
                                                      32,                                   /* n_elements_to_preallocate */
                                                      nullptr,                              /* init_fn                   */
                                                      _raGL_command_buffer_deinit_command); /* deinit_fn                 */

    ASSERT_DEBUG_SYNC(command_buffer_pool != nullptr,
                      "Could not create a raGL command buffer pool");
    ASSERT_DEBUG_SYNC(command_pool != nullptr,
                      "Could not create a raGL command pool");
}

/** Please see header for specification */
PUBLIC void raGL_command_buffer_release(raGL_command_buffer command_buffer)
{
    _raGL_command_buffer* command_buffer_ptr = reinterpret_cast<_raGL_command_buffer*>(command_buffer);

    ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                      "Input raGL_command_buffer instance is NULL");

    command_buffer_ptr->clear_commands();

    system_resource_pool_return_to_pool(command_buffer_pool,
                                        (system_resource_pool_block) command_buffer);
}

/** Please see header for specification */
PUBLIC void raGL_command_buffer_start_recording(raGL_command_buffer command_buffer)
{
    _raGL_command_buffer* command_buffer_ptr  = reinterpret_cast<_raGL_command_buffer*>(command_buffer);
    _raGL_command*        current_command_ptr = nullptr;

    /* Reset any enqueued commands */
    command_buffer_ptr->clear_commands();
}

/** Please see header for specification */
PUBLIC void raGL_command_buffer_stop_recording(raGL_command_buffer command_buffer)
{
    _raGL_command_buffer* command_buffer_ptr = reinterpret_cast<_raGL_command_buffer*>(command_buffer);

    /* Process the RAL commands and convert them to a set of GL instructions */
    command_buffer_ptr->bake_commands();
}
