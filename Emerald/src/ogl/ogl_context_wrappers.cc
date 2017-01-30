/**
 *
 * Emerald (kbi/elude @2013-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_bo_bindings.h"
#include "ogl/ogl_context_sampler_bindings.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_context_to_bindings.h"
#include "ogl/ogl_context_wrappers.h"
#include "ogl/ogl_context_vaos.h"
#include "ogl/ogl_vao.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_program.h"
#include "ral/ral_texture.h"
#include "system/system_log.h"
#include <math.h>

/* Injects glFinish() call after each GL call. Enable ONLY for debugging purposes */
//#define ENABLE_DEBUG_FINISH_CALLS

enum vap_worker_type
{
    VAP_WORKER_TYPE_NORMALIZED,
    VAP_WORKER_TYPE_INTEGER,
    VAP_WORKER_TYPE_DOUBLE
};

#ifdef _WIN32
    __declspec(thread) ogl_context_gl_entrypoints_private* _private_entrypoints_ptr = nullptr;
#else
    __thread ogl_context_gl_entrypoints_private* _private_entrypoints_ptr = nullptr;
#endif


/** TODO */
PRIVATE ogl_context_state_cache_property _ogl_context_wrappers_get_ogl_context_state_cache_property_for_glenum(GLenum property)
{
    ogl_context_state_cache_property result = OGL_CONTEXT_STATE_CACHE_PROPERTY_UNKNOWN;

    switch (property)
    {
        case GL_BLEND:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_BLEND;

            break;
        }

        case GL_COLOR_LOGIC_OP:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_COLOR_LOGIC_OP;

            break;
        }

        case GL_CULL_FACE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE;

            break;
        }

        case GL_DEPTH_CLAMP:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_CLAMP;

            break;
        }

        case GL_DEPTH_TEST:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_TEST;

            break;
        }

        case GL_DITHER:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DITHER;

            break;
        }

        case GL_FRAMEBUFFER_SRGB:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_FRAMEBUFFER_SRGB;

            break;
        }

        case GL_LINE_SMOOTH:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_LINE_SMOOTH;

            break;
        }

        case GL_MULTISAMPLE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_MULTISAMPLE;

            break;
        }

        case GL_POLYGON_OFFSET_FILL:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_FILL;

            break;
        }

        case GL_POLYGON_OFFSET_LINE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_LINE;

            break;
        }

        case GL_POLYGON_OFFSET_POINT:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_POINT;

            break;
        }

        case GL_POLYGON_SMOOTH:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_SMOOTH;

            break;
        }

        case GL_PRIMITIVE_RESTART:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART;

            break;
        }

        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART_FIXED_INDEX;

            break;
        }

        case GL_PROGRAM_POINT_SIZE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PROGRAM_POINT_SIZE;

            break;
        }

        case GL_RASTERIZER_DISCARD:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_RASTERIZER_DISCARD;

            break;
        }

        case GL_SAMPLE_ALPHA_TO_COVERAGE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_COVERAGE;

            break;
        }

        case GL_SAMPLE_ALPHA_TO_ONE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_ONE;

            break;
        }

        case GL_SAMPLE_COVERAGE:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_COVERAGE;

            break;
        }

        case GL_SAMPLE_SHADING:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_SHADING;

            break;
        }


        case GL_SAMPLE_MASK:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_MASK;

            break;
        }

        case GL_SCISSOR_TEST:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SCISSOR_TEST;

            break;
        }

        case GL_STENCIL_TEST:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_STENCIL_TEST;

            break;
        }

        case GL_TEXTURE_CUBE_MAP_SEAMLESS:
        {
            result = OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_TEXTURE_CUBE_MAP_SEAMLESS;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized property value");
        }
    }

    return result;
}

/** TODO */
PRIVATE void _ogl_context_wrappers_vertex_attrib_format_worker(vap_worker_type worker_type,
                                                               GLuint          attribindex,
                                                               GLint           size,
                                                               GLenum          type,
                                                               GLboolean       normalized,
                                                               GLuint          relativeoffset)
{
    ogl_context             context                = ogl_context_get_current_context();
    GLboolean               current_normalized     = GL_FALSE;
    GLuint                  current_relativeoffset = -1;
    GLint                   current_size           = -1;
    GLenum                  current_type           = -1;
    ogl_vao                 current_vao            = nullptr;
    GLuint                  current_vao_id         = -1;
    ogl_context_state_cache state_cache            = nullptr;
    ogl_context_vaos        vaos                   = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    /* Retrieve current VAO's VAA properties */
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    current_vao = ogl_context_vaos_get_vao(vaos,
                                           current_vao_id);

    ogl_vao_get_vaa_property(current_vao,
                             attribindex,
                             OGL_VAO_VAA_PROPERTY_NORMALIZED,
                            &current_normalized);
    ogl_vao_get_vaa_property(current_vao,
                             attribindex,
                             OGL_VAO_VAA_PROPERTY_RELATIVE_OFFSET,
                            &current_relativeoffset);
    ogl_vao_get_vaa_property(current_vao,
                             attribindex,
                             OGL_VAO_VAA_PROPERTY_SIZE,
                            &current_size);
    ogl_vao_get_vaa_property(current_vao,
                             attribindex,
                             OGL_VAO_VAA_PROPERTY_TYPE,
                            &current_type);

    if (current_normalized     != normalized     ||
        current_relativeoffset != relativeoffset ||
        current_size           != size           ||
        current_type           != type)
    {
        #ifdef ENABLE_DEBUG_FINISH_CALLS
        {
            _private_entrypoints_ptr->pGLFinish();
        }
        #endif

        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        switch (worker_type)
        {
            case VAP_WORKER_TYPE_DOUBLE:
            {
                _private_entrypoints_ptr->pGLVertexAttribLFormat(attribindex,
                                                                 size,
                                                                 type,
                                                                 relativeoffset);

                break;
            }

            case VAP_WORKER_TYPE_INTEGER:
            {
                _private_entrypoints_ptr->pGLVertexAttribIFormat(attribindex,
                                                                 size,
                                                                 type,
                                                                 relativeoffset);

                break;
            }

            case VAP_WORKER_TYPE_NORMALIZED:
            {
                _private_entrypoints_ptr->pGLVertexAttribFormat(attribindex,
                                                                size,
                                                                type,
                                                                normalized,
                                                                relativeoffset);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized VAP worker type");
            }
        }

        ogl_vao_set_vaa_property(current_vao,
                                 attribindex,
                                 OGL_VAO_VAA_PROPERTY_NORMALIZED,
                                &current_normalized);
        ogl_vao_set_vaa_property(current_vao,
                                 attribindex,
                                 OGL_VAO_VAA_PROPERTY_RELATIVE_OFFSET,
                                &current_relativeoffset);
        ogl_vao_set_vaa_property(current_vao,
                                 attribindex,
                                 OGL_VAO_VAA_PROPERTY_SIZE,
                                &current_size);
        ogl_vao_set_vaa_property(current_vao,
                                 attribindex,
                                 OGL_VAO_VAA_PROPERTY_TYPE,
                                &current_type);
    }
}

/** TODO */
PRIVATE void _ogl_context_wrappers_vertex_attrib_pointer_worker(vap_worker_type worker_type,
                                                                GLuint          index,
                                                                GLint           size,
                                                                GLenum          type,
                                                                GLboolean       normalized,
                                                                GLsizei         stride,
                                                                const GLvoid*   pointer)
{
    ogl_context             context                  = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings              = nullptr;
    GLuint                  current_array_buffer     = 0;
    ogl_vao                 current_vao              = nullptr;
    GLuint                  current_vao_id           = -1;
    ogl_context_state_cache state_cache              = nullptr;
    ogl_context_vaos        vaos                     = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    /* Retrieve current GL_ARRAY_BUFFER binding */
    current_array_buffer = ogl_context_bo_bindings_get_general_binding(bo_bindings,
                                                                       GL_ARRAY_BUFFER);

    /* Retrieve current VAO's VAA properties */
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    current_vao = ogl_context_vaos_get_vao(vaos,
                                           current_vao_id);

    /* As per spec .. */
    GLuint effective_stride = 0;

    switch (worker_type)
    {
        case VAP_WORKER_TYPE_DOUBLE:
        {
            ogl_context_wrappers_glVertexAttribLFormat(index,
                                                       size,
                                                       type,
                                                       0); /* relativeoffset */

            break;
        }

        case VAP_WORKER_TYPE_INTEGER:
        {
            ogl_context_wrappers_glVertexAttribIFormat(index,
                                                       size,
                                                       type,
                                                       0); /* relativeoffset */

            break;
        }

        case VAP_WORKER_TYPE_NORMALIZED:
        {
            ogl_context_wrappers_glVertexAttribFormat(index,
                                                      size,
                                                      type,
                                                      normalized,
                                                      0); /* relativeoffset */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized VAP worker type");
        }
    }


    if (stride != 0)
    {
        effective_stride = stride;
    }
    else
    {
        ASSERT_DEBUG_SYNC(stride == 0,
                          "TODO: Compute effectiveStride based on size and type");
    }

    ogl_context_wrappers_glBindVertexBuffer(index,
                                            current_array_buffer,
                                            reinterpret_cast<GLintptr>(pointer),
                                            effective_stride);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glActiveTexture(GLenum texture)
{
    ogl_context_state_cache cache              = nullptr;
    GLuint                  texture_unit_index = texture - GL_TEXTURE0;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &cache);

    ogl_context_state_cache_set_property(cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                         &texture_unit_index);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBeginTransformFeedback(GLenum primitiveMode)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);

    _private_entrypoints_ptr->pGLBeginTransformFeedback(primitiveMode);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffer(GLenum target,
                                                       GLuint buffer)
{
    ogl_context context = ogl_context_get_current_context();

    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_set_binding(bo_bindings,
                                        target,
                                        buffer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_set_binding_base(bo_bindings,
                                             target,
                                             index,
                                             buffer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBufferRange(GLenum     target,
                                                            GLuint     index,
                                                            GLuint     buffer,
                                                            GLintptr   offset,
                                                            GLsizeiptr size)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_set_binding_range(bo_bindings,
                                              target,
                                              index,
                                              offset,
                                              size,
                                              buffer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffersBase(GLenum        target,
                                                            GLuint        first,
                                                            GLsizei       count,
                                                            const GLuint* buffers)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    for (unsigned int n = first;
                      n < first + count;
                    ++n)
    {
        ogl_context_bo_bindings_set_binding_base(bo_bindings,
                                                 target,
                                                 n,
                                                 buffers[n]);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffersRange(GLenum            target,
                                                             GLuint            first,
                                                             GLsizei           count,
                                                             const GLuint*     buffers,
                                                             const GLintptr*   offsets,
                                                             const GLsizeiptr* sizes)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    for (unsigned int n = first;
                      n < first + count;
                    ++n)
    {
        ogl_context_bo_bindings_set_binding_range(bo_bindings,
                                                  target,
                                                  n,
                                                  offsets[n],
                                                  sizes[n],
                                                  buffers[n]);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindFramebuffer(GLenum target,
                                                            GLuint fbo_id)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER,
                                                &fbo_id);

            break;
        }

        case GL_FRAMEBUFFER:
        {
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER,
                                                &fbo_id);
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER,
                                                &fbo_id);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER,
                                                &fbo_id);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glBindMultiTextureEXT(GLenum texunit,
                                                                GLenum target,
                                                                GLuint texture)
{
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_to_bindings_set_binding(to_bindings,
                                        texunit - GL_TEXTURE0,
                                        target,
                                        texture);

}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindRenderbuffer(GLenum target,
                                                             GLuint renderbuffer)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_state_cache state_cache = nullptr;

    ASSERT_DEBUG_SYNC(target == GL_RENDERBUFFER,
                      "Unsupported renderbuffer target");

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER,
                                        &renderbuffer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSampler(GLuint unit,
                                                        GLuint sampler)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_set_binding(sampler_bindings,
                                             unit,
                                             sampler);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSamplers(GLuint        first,
                                                         GLsizei       count,
                                                         const GLuint* samplers)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    for (int n = 0;
             n < count;
           ++n)
    {
        ogl_context_sampler_bindings_set_binding(sampler_bindings,
                                                 first + n,
                                                 samplers[n]);
    }
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTexture(GLenum target,
                                                        GLuint texture)
{
    ogl_context             context      = ogl_context_get_current_context();
    GLuint                  texture_unit = -1;
    ogl_context_state_cache state_cache  = nullptr;
    ogl_context_to_bindings to_bindings  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    ogl_context_to_bindings_set_binding(to_bindings,
                                        texture_unit,
                                        target,
                                        texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTextures(GLuint        first,
                                                         GLsizei       count,
                                                         const GLuint* textures)
{
    raGL_backend            backend         = nullptr;
    ogl_context             current_context = ogl_context_get_current_context();
    raGL_texture            texture_raGL    = nullptr;
    ral_texture             texture_ral     = nullptr;
    ogl_context_to_bindings to_bindings     = nullptr;

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    for (GLint n_texture = 0;
               n_texture < count;
             ++n_texture)
    {
        if (textures[n_texture] == 0)
        {
            ogl_context_to_bindings_reset_all_bindings_for_texture_unit(to_bindings,
                                                                        first + n_texture);
        }
        else
        {
            GLenum           texture_target;
            ral_texture_type texture_type_ral = RAL_TEXTURE_TYPE_UNKNOWN;

            ogl_context_get_property      (current_context,
                                           OGL_CONTEXT_PROPERTY_BACKEND,
                                          &backend);
            raGL_backend_get_texture_by_id(backend,
                                           textures[n_texture],
                                          &texture_raGL);
            raGL_texture_get_property     (texture_raGL,
                                           RAGL_TEXTURE_PROPERTY_RAL_TEXTURE,
                                           (void**) &texture_ral);
            ral_texture_get_property      (texture_ral,
                                           RAL_TEXTURE_PROPERTY_TYPE,
                                          &texture_type_ral);

            texture_target = raGL_utils_get_ogl_enum_for_ral_texture_type(texture_type_ral);

            ogl_context_to_bindings_set_binding(to_bindings,
                                                first + n_texture, /* texture_unit */
                                                texture_target,
                                                textures[n_texture]);
        }
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindVertexArray(GLuint array)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &array);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha)
{
    const GLfloat           blend_color[] = {red, green, blue, alpha};
    ogl_context             context       = ogl_context_get_current_context();
    ogl_context_state_cache state_cache   = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR,
                                         blend_color);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendEquation(GLenum mode)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA,
                                        &mode);
    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB,
                                        &mode);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindVertexBuffer(GLuint   bindingindex,
                                                             GLuint   buffer,
                                                             GLintptr offset,
                                                             GLsizei  stride)
{
    ogl_context             context           = ogl_context_get_current_context();
    ogl_vao                 current_vao       = nullptr;
    GLuint                  current_vao_id    = -1;
    GLuint                  current_vb_buffer = -1;
    GLintptr                current_vb_offset = -1;
    GLsizei                 current_vb_stride = -1;
    ogl_context_state_cache state_cache       = nullptr;
    GLboolean               vaa_enabled       = GL_FALSE;
    ogl_context_vaos        vaos              = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    current_vao = ogl_context_vaos_get_vao(vaos,
                                           current_vao_id);

    ogl_vao_get_vb_property(current_vao,
                            bindingindex,
                            OGL_VAO_VB_PROPERTY_BUFFER,
                           &current_vb_buffer);
    ogl_vao_get_vb_property(current_vao,
                            bindingindex,
                            OGL_VAO_VB_PROPERTY_OFFSET,
                           &current_vb_offset);
    ogl_vao_get_vb_property(current_vao,
                            bindingindex,
                            OGL_VAO_VB_PROPERTY_STRIDE,
                           &current_vb_stride);

    if (current_vb_buffer != buffer ||
        current_vb_offset != offset ||
        current_vb_stride != stride)
    {
        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        _private_entrypoints_ptr->pGLBindVertexBuffer(bindingindex,
                                                      buffer,
                                                      offset,
                                                      stride);

        ogl_vao_set_vb_property(current_vao,
                                bindingindex,
                                OGL_VAO_VB_PROPERTY_BUFFER,
                               &buffer);
        ogl_vao_set_vb_property(current_vao,
                                bindingindex,
                                OGL_VAO_VB_PROPERTY_OFFSET,
                               &offset);
        ogl_vao_set_vb_property(current_vao,
                                bindingindex,
                                OGL_VAO_VB_PROPERTY_STRIDE,
                               &stride);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendEquationSeparate(GLenum modeRGB,
                                                                  GLenum modeAlpha)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA,
                                        &modeRGB);
    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB,
                                        &modeAlpha);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendFunc(GLenum sfactor,
                                                      GLenum dfactor)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA,
                                        &dfactor);
    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB,
                                        &dfactor);
    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA,
                                        &sfactor);
    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB,
                                        &sfactor);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendFuncSeparate(GLenum srcRGB,
                                                              GLenum dstRGB,
                                                              GLenum srcAlpha,
                                                              GLenum dstAlpha)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA,
                                        &dstAlpha);
    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB,
                                        &dstRGB);
    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA,
                                        &srcAlpha);
    ogl_context_state_cache_set_property(state_cache,
                                        OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB,
                                        &srcRGB);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBlitFramebuffer(GLint      srcX0,
                                                            GLint      srcY0,
                                                            GLint      srcX1,
                                                            GLint      srcY1,
                                                            GLint      dstX0,
                                                            GLint      dstY0,
                                                            GLint      dstX1,
                                                            GLint      dstY1,
                                                            GLbitfield mask,
                                                            GLenum     filter)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES  |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);

    _private_entrypoints_ptr->pGLBlitFramebuffer(srcX0,
                                                 srcY0,
                                                 srcX1,
                                                 srcY1,
                                                 dstX0,
                                                 dstY0,
                                                 dstX1,
                                                 dstY1,
                                                 mask,
                                                 filter);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferData(GLenum        target,
                                                       GLsizeiptr    size,
                                                       const GLvoid* data,
                                                       GLenum        usage)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLBufferData(target,
                                            size,
                                            data,
                                            usage);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferStorage(GLenum        target,
                                                          GLsizeiptr    size,
                                                          const GLvoid* data,
                                                          GLbitfield    flags)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLBufferStorage(target,
                                               size,
                                               data,
                                               flags);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferSubData(GLenum        target,
                                                          GLintptr      offset,
                                                          GLsizeiptr    size,
                                                          const GLvoid* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLBufferSubData(target,
                                               offset,
                                               size,
                                               data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC GLenum APIENTRY ogl_context_wrappers_glCheckNamedFramebufferStatusEXT(GLuint framebuffer,
                                                                             GLenum target)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    return _private_entrypoints_ptr->pGLCheckNamedFramebufferStatusEXT(framebuffer,
                                                                       target);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClear(GLbitfield mask)
{
    int                     clear_mask  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    clear_mask = ((mask & GL_COLOR_BUFFER_BIT) ? STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_COLOR : 0) |
                 ((mask & GL_DEPTH_BUFFER_BIT) ? STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_DEPTH : 0);

    ogl_context_state_cache_sync(state_cache,
                                 clear_mask                                   |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES  |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX      |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT);

    _private_entrypoints_ptr->pGLClear(mask);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClearBufferData(GLenum      target,
                                                            GLenum      internalformat,
                                                            GLenum      format,
                                                            GLenum      type,
                                                            const void* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLClearBufferData(target,
                                                 internalformat,
                                                 format,
                                                 type,
                                                 data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClearBufferSubData(GLenum      target,
                                                               GLenum      internalformat,
                                                               GLintptr    offset,
                                                               GLsizeiptr  size,
                                                               GLenum      format,
                                                               GLenum      type,
                                                               const void* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLClearBufferSubData(target,
                                                    internalformat,
                                                    offset,
                                                    size,
                                                    format,
                                                    type,
                                                    data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClearColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha)
{
    GLfloat                 color[]     = {red, green, blue, alpha};
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR,
                                         color);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClearDepth(GLdouble value)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH_DOUBLE,
                                        &value);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClearDepthf(GLfloat d)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH_FLOAT,
                                        &d);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha)
{
    GLboolean               color_mask[] = {red, green, blue, alpha};
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK,
                                         color_mask);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexSubImage1D(GLenum        target,
                                                                    GLint         level,
                                                                    GLint         xoffset,
                                                                    GLsizei       width,
                                                                    GLenum        format,
                                                                    GLsizei       imageSize,
                                                                    const GLvoid* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTexSubImage1D(target,
                                                         level,
                                                         xoffset,
                                                         width,
                                                         format,
                                                         imageSize,
                                                         data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexSubImage2D(GLenum        target,
                                                                    GLint         level,
                                                                    GLint         xoffset,
                                                                    GLint         yoffset,
                                                                    GLsizei       width,
                                                                    GLsizei       height,
                                                                    GLenum        format,
                                                                    GLsizei       imageSize,
                                                                    const GLvoid* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTexSubImage2D(target,
                                                         level,
                                                         xoffset,
                                                         yoffset,
                                                         width,
                                                         height,
                                                         format,
                                                         imageSize,
                                                         data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexSubImage3D(GLenum        target,
                                                                    GLint         level,
                                                                    GLint         xoffset,
                                                                    GLint         yoffset,
                                                                    GLint         zoffset,
                                                                    GLsizei       width,
                                                                    GLsizei       height,
                                                                    GLsizei       depth,
                                                                    GLenum        format,
                                                                    GLsizei       imageSize,
                                                                    const GLvoid* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTexSubImage3D(target,
                                                         level,
                                                         xoffset,
                                                         yoffset,
                                                         zoffset,
                                                         width,
                                                         height,
                                                         depth,
                                                         format,
                                                         imageSize,
                                                         data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage1DEXT(GLuint        texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLsizei       width,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTextureSubImage1DEXT(texture,
                                                                target,
                                                                level,
                                                                xoffset,
                                                                width,
                                                                format,
                                                                imageSize,
                                                                bits);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage2DEXT(GLuint        texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLint         yoffset,
                                                                           GLsizei       width,
                                                                           GLsizei       height,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTextureSubImage2DEXT(texture,
                                                                target,
                                                                level,
                                                                xoffset,
                                                                yoffset,
                                                                width,
                                                                height,
                                                                format,
                                                                imageSize,
                                                                bits);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage3DEXT(GLuint        texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLint         yoffset,
                                                                           GLint         zoffset,
                                                                           GLsizei       width,
                                                                           GLsizei       height,
                                                                           GLsizei       depth,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTextureSubImage3DEXT(texture,
                                                                target,
                                                                level,
                                                                xoffset,
                                                                yoffset,
                                                                zoffset,
                                                                width,
                                                                height,
                                                                depth,
                                                                format,
                                                                imageSize,
                                                                bits);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyBufferSubData(GLenum     readTarget,
                                                              GLenum     writeTarget,
                                                              GLintptr   readOffset,
                                                              GLintptr   writeOffset,
                                                              GLsizeiptr size)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(readTarget)  |
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(writeTarget) );

    _private_entrypoints_ptr->pGLCopyBufferSubData(readTarget,
                                                   writeTarget,
                                                   readOffset,
                                                   writeOffset,
                                                   size);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexSubImage1D(GLenum  target,
                                                              GLint   level,
                                                              GLint   xoffset,
                                                              GLint   x,
                                                              GLint   y,
                                                              GLsizei width)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT     |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCopyTexSubImage1D(target,
                                                   level,
                                                   xoffset,
                                                   x,
                                                   y,
                                                   width);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexSubImage2D(GLenum  target,
                                                              GLint   level,
                                                              GLint   xoffset,
                                                              GLint   yoffset,
                                                              GLint   x,
                                                              GLint   y,
                                                              GLsizei width,
                                                              GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT     |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCopyTexSubImage2D(target,
                                                   level,
                                                   xoffset,
                                                   yoffset,
                                                   x,
                                                   y,
                                                   width,
                                                   height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexSubImage3D(GLenum  target,
                                                              GLint   level,
                                                              GLint   xoffset,
                                                              GLint   yoffset,
                                                              GLint   zoffset,
                                                              GLint   x,
                                                              GLint   y,
                                                              GLsizei width,
                                                              GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT     |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCopyTexSubImage3D(target,
                                                   level,
                                                   xoffset,
                                                   yoffset,
                                                   zoffset,
                                                   x,
                                                   y,
                                                   width,
                                                   height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage1DEXT(GLuint  texture,
                                                                     GLenum  target,
                                                                     GLint   level,
                                                                     GLint   xoffset,
                                                                     GLint   x,
                                                                     GLint   y,
                                                                     GLsizei width)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

    _private_entrypoints_ptr->pGLCopyTextureSubImage1DEXT(texture,
                                                          target,
                                                          level,
                                                          xoffset,
                                                          x,
                                                          y,
                                                          width);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage2DEXT(GLuint  texture,
                                                                     GLenum  target,
                                                                     GLint   level,
                                                                     GLint   xoffset,
                                                                     GLint   yoffset,
                                                                     GLint   x,
                                                                     GLint   y,
                                                                     GLsizei width,
                                                                     GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

    _private_entrypoints_ptr->pGLCopyTextureSubImage2DEXT(texture,
                                                          target,
                                                          level,
                                                          xoffset,
                                                          yoffset,
                                                          x,
                                                          y,
                                                          width,
                                                          height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage3DEXT(GLuint  texture,
                                                                     GLenum  target,
                                                                     GLint   level,
                                                                     GLint   xoffset,
                                                                     GLint   yoffset,
                                                                     GLint   zoffset,
                                                                     GLint   x,
                                                                     GLint   y,
                                                                     GLsizei width,
                                                                     GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

    _private_entrypoints_ptr->pGLCopyTextureSubImage3DEXT(texture,
                                                          target,
                                                          level,
                                                          xoffset,
                                                          yoffset,
                                                          zoffset,
                                                          x,
                                                          y,
                                                          width,
                                                          height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glCullFace(GLenum mode)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE,
                                        &mode);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteBuffers(GLsizei       n,
                                                          const GLuint* buffers)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    _private_entrypoints_ptr->pGLDeleteBuffers(n,
                                               buffers);

    ogl_context_bo_bindings_reset_buffer_bindings(bo_bindings,
                                                  buffers,
                                                  n);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteRenderbuffers(GLsizei       n,
                                                                const GLuint* renderbuffers)
{
    ogl_context             context        = ogl_context_get_current_context();
    GLuint                  current_rbo_id = 0;
    ogl_context_state_cache state_cache    = nullptr;
    static const GLuint     zero_rbo_id    = 0;

    ogl_context_get_property            (context,
                                         OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                        &state_cache);
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER,
                                        &current_rbo_id);

    for (unsigned n_renderbuffer = 0;
                  n_renderbuffer < static_cast<uint32_t>(n);
                ++n_renderbuffer)
    {
        if (renderbuffers[n] == current_rbo_id)
        {
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER,
                                                &zero_rbo_id);

            break;
        }
    }

    _private_entrypoints_ptr->pGLDeleteRenderbuffers(n,
                                                     renderbuffers);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteTextures(GLsizei       n,
                                                           const GLuint* textures)
{
    ogl_context             context             = ogl_context_get_current_context();
    ogl_context_to_bindings context_to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &context_to_bindings);

    ogl_context_to_bindings_on_textures_deleted(context_to_bindings,
                                                n,
                                                textures);

    _private_entrypoints_ptr->pGLDeleteTextures(n,
                                                textures);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteVertexArrays(GLsizei       n,
                                                               const GLuint* arrays)
{
    ogl_context             context     = ogl_context_get_current_context();
    GLuint                  current_vao = 0;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_vaos        vaos        = nullptr;

    ogl_context_get_property            (context,
                                         OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                        &state_cache);
    ogl_context_get_property            (context,
                                         OGL_CONTEXT_PROPERTY_VAOS,
                                        &vaos);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao);

    _private_entrypoints_ptr->pGLDeleteVertexArrays(n, arrays);

    for (GLsizei current_n = 0;
                 current_n < n;
               ++current_n)
    {
        static GLuint zero_vao_id = 0;

        /* Unbind the deleted VAO */
        if (arrays[current_n] == current_vao)
        {
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                                &zero_vao_id);
        }

        /* Remove the VAO from the VAO registry */
        ogl_context_vaos_delete_vao(vaos,
                                    arrays[current_n]);
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthFunc(GLenum func)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC,
                                        &func);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthMask(GLboolean flag)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK,
                                        &flag);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDisable(GLenum cap)
{
    ogl_context_state_cache_property cap_property = _ogl_context_wrappers_get_ogl_context_state_cache_property_for_glenum(cap);
    ogl_context                      context      = ogl_context_get_current_context                                      ();
    const bool                       new_state    = false;
    ogl_context_state_cache          state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         cap_property,
                                        &new_state);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDisablei(GLenum cap,
                                                     GLuint index)
{
    ASSERT_DEBUG_SYNC(index == 0,
                      "TODO");

    ogl_context_wrappers_glDisable(cap);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDisableVertexAttribArray(GLuint index)
{
    ogl_context             context        = ogl_context_get_current_context();
    GLuint                  current_vao_id = -1;
    ogl_context_state_cache state_cache    = nullptr;
    GLboolean               vaa_enabled    = GL_FALSE;
    ogl_context_vaos        vaos           = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    ogl_vao_get_vaa_property(ogl_context_vaos_get_vao(vaos,
                                                      current_vao_id),
                             index,
                             OGL_VAO_VAA_PROPERTY_ENABLED,
                            &vaa_enabled);

    if (vaa_enabled == GL_TRUE)
    {
        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        _private_entrypoints_ptr->pGLDisableVertexAttribArray(index);
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchCompute(GLuint num_groups_x,
                                                            GLuint num_groups_y,
                                                            GLuint num_groups_z)
{
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context                  context          = ogl_context_get_current_context();
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDispatchCompute(num_groups_x,
                                                 num_groups_y,
                                                 num_groups_z);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchComputeIndirect(GLintptr indirect)
{
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context                  context          = ogl_context_get_current_context();
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDispatchComputeIndirect(indirect);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArrays(GLenum  mode,
                                                       GLint   first,
                                                       GLsizei count)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArrays(mode,
                                            first,
                                            count);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysIndirect(GLenum      mode,
                                                               const void* indirect)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_DRAW_INDIRECT_BUFFER      |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArraysIndirect(mode,
                                                    indirect);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysInstanced(GLenum  mode,
                                                                GLint   first,
                                                                GLsizei count,
                                                                GLsizei primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArraysInstanced(mode,
                                                     first,
                                                     count,
                                                     primcount);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysInstancedBaseInstance(GLenum  mode,
                                                                            GLint   first,
                                                                            GLsizei count,
                                                                            GLsizei primcount,
                                                                            GLuint  baseinstance)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArraysInstancedBaseInstance(mode,
                                                                 first,
                                                                 count,
                                                                 primcount,
                                                                 baseinstance);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawBuffer(GLenum mode)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

    _private_entrypoints_ptr->pGLDrawBuffer(mode);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawBuffers(      GLsizei n,
                                                        const GLenum* bufs)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

    _private_entrypoints_ptr->pGLDrawBuffers(n, bufs);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElements(GLenum        mode,
                                                         GLsizei       count,
                                                         GLenum        type,
                                                         const GLvoid* indices)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawElements(mode,
                                              count,
                                              type,
                                              indices);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstancedBaseInstance(GLenum      mode,
                                                                              GLsizei     count,
                                                                              GLenum      type,
                                                                              const void* indices,
                                                                              GLsizei     instancecount,
                                                                              GLuint      baseinstance)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawElementsInstancedBaseInstance(mode,
                                                                   count,
                                                                   type,
                                                                   indices,
                                                                   instancecount,
                                                                   baseinstance);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstancedBaseVertex(GLenum      mode,
                                                                            GLsizei     count,
                                                                            GLenum      type,
                                                                            const void* indices,
                                                                            GLsizei     instancecount,
                                                                            GLint       basevertex)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawElementsInstancedBaseVertex(mode,
                                                                 count,
                                                                 type,
                                                                 indices,
                                                                 instancecount,
                                                                 basevertex);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstancedBaseVertexBaseInstance(GLenum      mode,
                                                                                        GLsizei     count,
                                                                                        GLenum      type,
                                                                                        const void* indices,
                                                                                        GLsizei     instancecount,
                                                                                        GLint       basevertex,
                                                                                        GLuint      baseinstance)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawElementsInstancedBaseVertexBaseInstance(mode,
                                                                             count,
                                                                             type,
                                                                             indices,
                                                                             instancecount,
                                                                             basevertex,
                                                                             baseinstance);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstanced(GLenum        mode,
                                                                  GLsizei       count,
                                                                  GLenum        type,
                                                                  const GLvoid* indices,
                                                                  GLsizei       primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawElementsInstanced(mode,
                                                       count,
                                                       type,
                                                       indices,
                                                       primcount);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawRangeElements(GLenum        mode,
                                                              GLuint        start,
                                                              GLuint        end,
                                                              GLsizei       count,
                                                              GLenum        type,
                                                              const GLvoid* indices)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawRangeElements(mode,
                                                   start,
                                                   end,
                                                   count,
                                                   type,
                                                   indices);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedback(GLenum mode,
                                                                  GLuint id)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawTransformFeedback(mode,
                                                       id);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedbackInstanced(GLenum  mode,
                                                                           GLuint  id,
                                                                           GLsizei instancecount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawTransformFeedbackInstanced(mode,
                                                                id,
                                                                instancecount);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedbackStreamInstanced(GLenum  mode,
                                                                                 GLuint  id,
                                                                                 GLuint  stream,
                                                                                 GLsizei instancecount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLDrawTransformFeedbackStreamInstanced(mode,
                                                                      id,
                                                                      stream,
                                                                      instancecount);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glEnable(GLenum cap)
{
    ogl_context_state_cache_property cap_property = _ogl_context_wrappers_get_ogl_context_state_cache_property_for_glenum(cap);
    ogl_context                      context      = ogl_context_get_current_context                                      ();
    const bool                       new_state    = true;
    ogl_context_state_cache          state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         cap_property,
                                        &new_state);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glEnablei(GLenum cap,
                                                    GLuint index)
{
    ASSERT_DEBUG_SYNC(index == 0,
                      "TODO");

    ogl_context_wrappers_glEnable(cap);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glEnableVertexAttribArray(GLuint index)
{
    ogl_context             context        = ogl_context_get_current_context();
    GLuint                  current_vao_id = -1;
    ogl_context_state_cache state_cache    = nullptr;
    GLboolean               vaa_enabled    = GL_FALSE;
    ogl_context_vaos        vaos           = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    ogl_vao_get_vaa_property(ogl_context_vaos_get_vao(vaos,
                                                      current_vao_id),
                             index,
                             OGL_VAO_VAA_PROPERTY_ENABLED,
                            &vaa_enabled);

    if (vaa_enabled == GL_FALSE)
    {
        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        _private_entrypoints_ptr->pGLEnableVertexAttribArray(index);
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}


/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferParameteri(GLenum target,
                                                                  GLenum pname,
                                                                  GLint  param)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferParameteri(target,
                                                       pname,
                                                       param);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferReadBufferEXT(GLuint framebuffer,
                                                                     GLenum mode)
{
    _private_entrypoints_ptr->pGLFramebufferReadBufferEXT(framebuffer,
                                                          mode);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferRenderbuffer(GLenum target,
                                                                    GLenum attachment,
                                                                    GLenum renderbuffertarget,
                                                                    GLuint renderbuffer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER | STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER | STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferRenderbuffer(target,
                                                         attachment,
                                                         renderbuffertarget,
                                                         renderbuffer);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture(GLenum target,
                                                               GLenum attachment,
                                                               GLuint texture,
                                                               GLint  level)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferTexture(target,
                                                    attachment,
                                                    texture,
                                                    level);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture1D(GLenum target,
                                                                 GLenum attachment,
                                                                 GLenum textarget,
                                                                 GLuint texture,
                                                                 GLint  level)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(textarget) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferTexture1D(target,
                                                      attachment,
                                                      textarget,
                                                      texture,
                                                      level);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture2D(GLenum target,
                                                                 GLenum attachment,
                                                                 GLenum textarget,
                                                                 GLuint texture,
                                                                 GLint  level)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(textarget) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferTexture2D(target,
                                                      attachment,
                                                      textarget,
                                                      texture,
                                                      level);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture3D(GLenum target,
                                                                 GLenum attachment,
                                                                 GLenum textarget,
                                                                 GLuint texture,
                                                                 GLint  level,
                                                                 GLint  layer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(textarget) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferTexture3D(target,
                                                      attachment,
                                                      textarget,
                                                      texture,
                                                      level,
                                                      layer);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTextureLayer(GLenum fb_target,
                                                                    GLenum attachment,
                                                                    GLuint texture,
                                                                    GLint  level,
                                                                    GLint  layer)
{
    raGL_backend            backend          = nullptr;
    ogl_context             current_context  = ogl_context_get_current_context();
    ogl_context_state_cache state_cache      = nullptr;
    raGL_texture            texture_raGL     = nullptr;
    ral_texture             texture_ral      = nullptr;
    GLenum                  texture_target   = GL_ZERO;
    ral_texture_type        texture_type_ral = RAL_TEXTURE_TYPE_UNKNOWN;
    ogl_context_to_bindings to_bindings      = nullptr;

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_get_property      (current_context,
                                   OGL_CONTEXT_PROPERTY_BACKEND,
                                  &backend);
    raGL_backend_get_texture_by_id(backend,
                                   texture,
                                  &texture_raGL);
    raGL_texture_get_property     (texture_raGL,
                                   RAGL_TEXTURE_PROPERTY_RAL_TEXTURE,
                                   (void**) &texture_ral);
    ral_texture_get_property      (texture_ral,
                                   RAL_TEXTURE_PROPERTY_TYPE,
                                  &texture_type_ral);

    texture_target = raGL_utils_get_ogl_enum_for_ral_texture_type(texture_type_ral);

    switch (fb_target)
    {
        case GL_DRAW_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(texture_target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLFramebufferTextureLayer(fb_target,
                                                         attachment,
                                                         texture,
                                                         level,
                                                         layer);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glFrontFace(GLenum mode)
{
    ogl_context             context       = ogl_context_get_current_context();
    ogl_context_state_cache state_cache   = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE,
                                        &mode);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGenerateMipmap(GLenum target)
{
    ogl_context             context               = ogl_context_get_current_context();
    GLuint                  current_texture_unit = 0;
    const static bool       mipmaps_generated    = true;
    ogl_context_state_cache state_cache          = nullptr;
    ogl_context_to_bindings to_bindings          = nullptr;

    ogl_context_get_property            (context,
                                         OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                        &state_cache);
    ogl_context_get_property            (context,
                                         OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                                        &to_bindings);
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &current_texture_unit);

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGenerateMipmap(target);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGenVertexArrays(GLsizei n,
                                                            GLuint* arrays)
{
    ogl_context      context = ogl_context_get_current_context();
    ogl_context_vaos vaos    = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    _private_entrypoints_ptr->pGLGenVertexArrays(n,
                                                 arrays);

    for (GLsizei current_n = 0;
                 current_n < n;
               ++current_n)
    {
        /* Add the VAO to the VAO registry */
        ogl_context_vaos_add_vao(vaos,
                                 arrays[current_n],
                                 ogl_vao_create(context,
                                                arrays[current_n])
                                );
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGenerateTextureMipmapEXT(GLuint texture,
                                                                     GLenum target)
{
    ogl_context             context               = ogl_context_get_current_context();
    const static bool       mipmaps_generated    = true;
    ogl_context_to_bindings to_bindings          = nullptr;

    _private_entrypoints_ptr->pGLGenerateTextureMipmapEXT(texture,
                                                          target);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGenTextures(GLsizei n,
                                                        GLuint* textures)
{
    ogl_context             context             = ogl_context_get_current_context();
    ogl_context_to_bindings context_to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &context_to_bindings);

    _private_entrypoints_ptr->pGLGenTextures(n,
                                             textures);

    ogl_context_to_bindings_on_textures_created(context_to_bindings,
                                                n,
                                                textures);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetActiveAtomicCounterBufferiv(GLuint program,
                                                                           GLuint bufferIndex,
                                                                           GLenum pname,
                                                                           GLint* params)
{
    ogl_context context = ogl_context_get_current_context();

    _private_entrypoints_ptr->pGLGetActiveAtomicCounterBufferiv(program,
                                                                bufferIndex,
                                                                pname,
                                                                params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBooleani_v(GLenum     target,
                                                          GLuint     index,
                                                          GLboolean* data)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                                &sampler_bindings);
    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);
    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                                &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync(to_bindings,
                                 OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    /* TODO: Pick target-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);

    _private_entrypoints_ptr->pGLGetBooleani_v(target,
                                               index,
                                               data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBooleanv(GLenum     pname,
                                                        GLboolean* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    /* TODO: Pick target-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetBooleanv(pname,
                                             params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferParameteriv(GLenum target,
                                                                 GLenum pname,
                                                                 GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetBufferParameteriv(target,
                                                      pname,
                                                      params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferParameteri64v(GLenum   target,
                                                                   GLenum   pname,
                                                                   GLint64* params)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                         OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                        &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetBufferParameteri64v(target,
                                                        pname,
                                                        params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferPointerv(GLenum   target,
                                                              GLenum   pname,
                                                              GLvoid** params)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetBufferPointerv(target,
                                                   pname,
                                                   params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferSubData(GLenum     target,
                                                             GLintptr   offset,
                                                             GLsizeiptr size,
                                                             GLvoid*    data)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetBufferSubData(target,
                                                  offset,
                                                  size,
                                                  data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTexImage(GLenum  target,
                                                                  GLint   level,
                                                                  GLvoid* img)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetCompressedTexImage(target,
                                                       level,
                                                       img);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTextureImageEXT(GLuint  texture,
                                                                         GLenum  target,
                                                                         GLint   lod,
                                                                         GLvoid* img)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetCompressedTextureImageEXT(texture,
                                                              target,
                                                              lod,
                                                              img);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetRenderbufferParameteriv(GLenum target,
                                                                       GLenum pname,
                                                                       GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetRenderbufferParameteriv(target,
                                                            pname,
                                                            params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetDoublev(GLenum    pname,
                                                       GLdouble* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    /* TODO: Pick pname-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetDoublev(pname,
                                            params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetFloatv(GLenum   pname,
                                                      GLfloat* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    /* TODO: Pick pname-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetFloatv(pname,
                                           params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetFramebufferParameteriv(GLenum target,
                                                                      GLenum pname,
                                                                      GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetFramebufferParameteriv(target,
                                                           pname,
                                                           params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetInteger64i_v(GLenum   target,
                                                            GLuint   index,
                                                            GLint64* data)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    /* TODO: Pick target-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetInteger64i_v(target,
                                                 index,
                                                 data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetIntegeri_v(GLenum target,
                                                          GLuint index,
                                                          GLint* data)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    /* TODO: Pick target-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetIntegeri_v(target,
                                               index,
                                               data);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetIntegerv(GLenum pname,
                                                        GLint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    switch (pname)
    {
        case GL_CURRENT_PROGRAM:
        {
            ogl_context_state_cache_get_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT,
                                                 params);

            goto end;
        }

        case GL_VIEWPORT:
        {
            ogl_context_state_cache_get_indexed_property(state_cache,
                                                         OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_VIEWPORT,
                                                         0, /* index */
                                                         params);

            goto end;
        }

        default:
        {
            /* Fall-through */
        }
    }

    /* TODO: Pick target-specific bit for improved efficiency */
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ALL);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ALL);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetIntegerv(pname,
                                             params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

end:
    ;
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterfv(GLuint   sampler,
                                                                  GLenum   pname,
                                                                  GLfloat* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetSamplerParameterfv(sampler,
                                                       pname,
                                                       params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterIiv(GLuint sampler,
                                                                   GLenum pname,
                                                                   GLint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetSamplerParameterIiv(sampler,
                                                        pname,
                                                        params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterIuiv(GLuint  sampler,
                                                                    GLenum  pname,
                                                                    GLuint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetSamplerParameterIuiv(sampler,
                                                         pname,
                                                         params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameteriv(GLuint sampler,
                                                                  GLenum pname,
                                                                  GLint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetSamplerParameteriv(sampler,
                                                       pname,
                                                       params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexImage(GLenum  target,
                                                        GLint   level,
                                                        GLenum  format,
                                                        GLenum  type,
                                                        GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexImage(target,
                                             level,
                                             format,
                                             type,
                                             pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexLevelParameterfv(GLenum   target,
                                                                   GLint    level,
                                                                   GLenum   pname,
                                                                   GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexLevelParameterfv(target,
                                                        level,
                                                        pname,
                                                        params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexLevelParameteriv(GLenum target,
                                                                   GLint  level,
                                                                   GLenum pname,
                                                                   GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexLevelParameteriv(target,
                                                        level,
                                                        pname,
                                                        params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterfv(GLenum   target,
                                                              GLenum   pname,
                                                              GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexParameterfv(target,
                                                   pname,
                                                   params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexParameteriv(target,
                                                   pname,
                                                   params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterIiv(GLenum target,
                                                               GLenum pname,
                                                               GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexParameterIiv(target,
                                                    pname,
                                                    params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterIuiv(GLenum  target,
                                                                GLenum  pname,
                                                                GLuint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTexParameterIuiv(target,
                                                     pname,
                                                     params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureImageEXT(GLuint  texture,
                                                               GLenum  target,
                                                               GLint   level,
                                                               GLenum  format,
                                                               GLenum  type,
                                                               GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLGetTextureImageEXT(texture,
                                                    target,
                                                    level,
                                                    format,
                                                    type,
                                                    pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribdv(GLuint    index,
                                                              GLenum    pname,
                                                              GLdouble* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* TODO: We could use VAO state cache values here */
    _private_entrypoints_ptr->pGLGetVertexAttribdv(index,
                                                   pname,
                                                   params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribfv(GLuint   index,
                                                              GLenum   pname,
                                                              GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* TODO: We could use VAO state cache values here */
    _private_entrypoints_ptr->pGLGetVertexAttribfv(index,
                                                   pname,
                                                   params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* TODO: We could use VAO state cache values here */
    _private_entrypoints_ptr->pGLGetVertexAttribiv(index,
                                                   pname,
                                                   params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribPointerv(GLuint   index,
                                                                    GLenum   pname,
                                                                    GLvoid** pointer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* TODO: We could use VAO state cache values here */
    _private_entrypoints_ptr->pGLGetVertexAttribPointerv(index,
                                                         pname,
                                                         pointer);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* TODO: We could use VAO state cache values here */
    _private_entrypoints_ptr->pGLGetVertexAttribIiv(index,
                                                    pname,
                                                    params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribIuiv(GLuint  index,
                                                                GLenum  pname,
                                                                GLuint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* TODO: We could use VAO state cache values here */
    _private_entrypoints_ptr->pGLGetVertexAttribIuiv(index,
                                                     pname,
                                                     params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glInvalidateFramebuffer(GLenum        target,
                                                                  GLsizei       numAttachments,
                                                                  const GLenum* attachments)
{
    ogl_context             context        = ogl_context_get_current_context();
    ogl_context_state_cache state_cache    = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLInvalidateFramebuffer(target,
                                                       numAttachments,
                                                       attachments);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glInvalidateSubFramebuffer(GLenum        target,
                                                                     GLsizei       numAttachments,
                                                                     const GLenum* attachments,
                                                                     GLint         x,
                                                                     GLint         y,
                                                                     GLsizei       width,
                                                                     GLsizei       height)
{
    ogl_context             context        = ogl_context_get_current_context();
    ogl_context_state_cache state_cache    = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER);

            break;
        }

        case GL_READ_FRAMEBUFFER:
        {
            ogl_context_state_cache_sync(state_cache,
                                         STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized FB BP");
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLInvalidateSubFramebuffer(target,
                                                          numAttachments,
                                                          attachments,
                                                          x,
                                                          y,
                                                          width,
                                                          height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBuffer(GLenum target,
                                                         GLenum access)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    GLvoid*                 result      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    result = _private_entrypoints_ptr->pGLMapBuffer(target,
                                                    access);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    return result;
}

/** Please see header for spec */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBufferRange(GLenum     target,
                                                              GLintptr   offset,
                                                              GLsizeiptr length,
                                                              GLbitfield access)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = nullptr;
    GLvoid*                 result      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    result = _private_entrypoints_ptr->pGLMapBufferRange(target,
                                                         offset,
                                                         length,
                                                         access);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    return result;
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glMemoryBarrier(GLbitfield barriers)
{
    uint32_t                bo_bindings_sync_bits = 0;
    uint32_t                cache_sync_bits       = 0;
    ogl_context             context               = ogl_context_get_current_context();
    ogl_context_bo_bindings context_bo_bindings   = nullptr;
    ogl_context_state_cache state_cache           = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &context_bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    if ((barriers & GL_COMMAND_BARRIER_BIT) != 0)
    {
        bo_bindings_sync_bits |= BO_BINDINGS_SYNC_BIT_DRAW_INDIRECT_BUFFER;
    }

    if ((barriers & GL_ELEMENT_ARRAY_BARRIER_BIT)       != 0 ||
        (barriers & GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT) != 0)
    {
        cache_sync_bits |= STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT;
    }

    if ((barriers & GL_PIXEL_BUFFER_BARRIER_BIT) != 0)
    {
        bo_bindings_sync_bits |= BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER    |
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER;
    }

    if (bo_bindings_sync_bits != 0)
    {
        ogl_context_bo_bindings_sync(context_bo_bindings,
                                     bo_bindings_sync_bits);
    }

    if (cache_sync_bits != 0)
    {
        ogl_context_state_cache_sync(state_cache,
                                     cache_sync_bits);
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLMemoryBarrier(barriers);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glMinSampleShading(GLfloat value)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_MIN_SAMPLE_SHADING,
                                        &value);
}

/** Please see header for spec */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawArrays(GLenum         mode,
                                                              const GLint*   first,
                                                              const GLsizei* count,
                                                              GLsizei        primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLMultiDrawArrays(mode,
                                                 first,
                                                 count,
                                                 primcount);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glMultiDrawArraysIndirect(GLenum      mode,
                                                                    const void* indirect,
                                                                    GLsizei     drawcount,
                                                                    GLsizei     stride)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLMultiDrawArraysIndirect(mode,
                                                         indirect,
                                                         drawcount,
                                                         stride);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawElements(GLenum               mode,
                                                                const GLsizei*       count,
                                                                GLenum               type,
                                                                const GLvoid* const* indices,
                                                                GLsizei              primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLMultiDrawElements(mode,
                                                   count,
                                                   type,
                                                   indices,
                                                   primcount);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawElementsBaseVertex(GLenum               mode,
                                                                          const GLsizei*       count,
                                                                          GLenum               type,
                                                                          const GLvoid* const* indices,
                                                                          GLsizei              drawcount,
                                                                          const GLint*         basevertex)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLMultiDrawElementsBaseVertex(mode,
                                                             count,
                                                             type,
                                                             indices,
                                                             drawcount,
                                                             basevertex);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glMultiDrawElementsIndirect(GLenum      mode,
                                                                      GLenum      type,
                                                                      const void* indirect,
                                                                      GLsizei     drawcount,
                                                                      GLsizei     stride)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = nullptr;
    ogl_context_sampler_bindings sampler_bindings = nullptr;
    ogl_context_state_cache      state_cache      = nullptr;
    ogl_context_to_bindings      to_bindings      = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync     (state_cache,
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             |
                                      STATE_CACHE_SYNC_BIT_BLENDING);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER     |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLMultiDrawElementsIndirect(mode,
                                                           type,
                                                           indirect,
                                                           drawcount,
                                                           stride);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferDataEXT(GLuint      buffer,
                                                               GLsizeiptr  size,
                                                               const void* data,
                                                               GLenum      usage)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    _private_entrypoints_ptr->pGLNamedBufferDataEXT(buffer,
                                                    size,
                                                    data,
                                                    usage);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferStorageEXT(GLuint        buffer,
                                                                  GLsizeiptr    size,
                                                                  const GLvoid* data,
                                                                  GLbitfield    flags)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    _private_entrypoints_ptr->pGLNamedBufferStorageEXT(buffer,
                                                       size,
                                                       data,
                                                       flags);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glPatchParameteri(GLenum pname,
                                                            GLint  value)
{

    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    /* TODO: Ignore GL_PATCH_DEFAULT_OUTER_LEVEL and GL_PATCH_DEFAULT_INNER_LEVEL for now. Implement
     *       if needed. */
    ASSERT_DEBUG_SYNC(pname == GL_PATCH_VERTICES,
                      "Invalid pname specified for a glPatchParameteri() call.");

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_N_PATCH_VERTICES,
                                        &value);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glPolygonOffset(GLfloat factor,
                                                          GLfloat units)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_FACTOR,
                                        &factor);
    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_UNITS,
                                        &units);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glReadBuffer(GLenum mode)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLReadBuffer(mode);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glReadPixels(GLint   x,
                                                       GLint   y,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum  format,
                                                       GLenum  type,
                                                       GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLReadPixels(x,
                                            y,
                                            width,
                                            height,
                                            format,
                                            type,
                                            pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glRenderbufferStorage(GLenum  target,
                                                                GLenum  internalformat,
                                                                GLsizei width,
                                                                GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLRenderbufferStorage(target,
                                                     internalformat,
                                                     width,
                                                     height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glRenderbufferStorageMultisample(GLenum  target,
                                                                           GLsizei samples,
                                                                           GLenum  internalformat,
                                                                           GLsizei width,
                                                                           GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLRenderbufferStorageMultisample(target,
                                                                samples,
                                                                internalformat,
                                                                width,
                                                                height);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glResumeTransformFeedback(void)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLResumeTransformFeedback();

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glScissor(GLint   x,
                                                    GLint   y,
                                                    GLsizei width,
                                                    GLsizei height)
{
    ogl_context             context       = ogl_context_get_current_context();
    GLint                   scissor_box[] = {x, y, width, height};
    ogl_context_state_cache state_cache   = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_indexed_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_SCISSOR_BOX,
                                                 0, /* index */
                                                 scissor_box);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glScissorIndexedv(GLuint       index,
                                                            const GLint *v)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_indexed_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_SCISSOR_BOX,
                                                 index,
                                                 v);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glShaderStorageBlockBinding(GLuint program,
                                                                      GLuint shaderStorageBlockIndex,
                                                                      GLuint shaderStorageBlockBinding)
{
    raGL_backend              context_backend = nullptr;
    ogl_context               context         = ogl_context_get_current_context();
    raGL_program              program_raGL    = nullptr;
    ogl_context_state_cache   state_cache     = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BACKEND,
                                &context_backend);
    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);

    /* Does it make sense to try to make the call? */
    raGL_backend_get_program_by_id(context_backend,
                                   program,
                                  &program_raGL);

    ASSERT_DEBUG_SYNC(program_raGL != nullptr,
                      "The raGL_program instance for requested PO was not found.");

    if (program_raGL != nullptr)
    {
        GLuint current_indexed_ssb_bp = -1;

        raGL_program_get_block_property(program_raGL,
                                        RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER,
                                        shaderStorageBlockIndex,
                                        RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                       &current_indexed_ssb_bp);

        //if (current_indexed_ssb_bp != shaderStorageBlockBinding)
        {
            /* Update the internal cache */
            _private_entrypoints_ptr->pGLShaderStorageBlockBinding(program,
                                                                   shaderStorageBlockIndex,
                                                                   shaderStorageBlockBinding);

            raGL_program_set_block_property(program_raGL,
                                            RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER,
                                            shaderStorageBlockIndex,
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &shaderStorageBlockBinding);
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexBuffer(GLenum target,
                                                      GLenum internalformat,
                                                      GLuint buffer)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = nullptr;
    ral_format              format_ral   = raGL_utils_get_ral_format_for_ogl_enum(internalformat);
    ogl_context_state_cache state_cache  = nullptr;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = nullptr;
    ral_texture_type        type_ral     = raGL_utils_get_ral_texture_type_for_ogl_enum(target);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexBuffer(target,
                                           internalformat,
                                           buffer);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexBufferRange(GLenum     target,
                                                           GLenum     internalformat,
                                                           GLuint     buffer,
                                                           GLintptr   offset,
                                                           GLsizeiptr size)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = nullptr;
    ral_format              format_ral   = raGL_utils_get_ral_format_for_ogl_enum(internalformat);
    ogl_context_state_cache state_cache  = nullptr;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = nullptr;
    ral_texture_type        type_ral     = raGL_utils_get_ral_texture_type_for_ogl_enum(target);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexBufferRange(target,
                                                internalformat,
                                                buffer,
                                                offset,
                                                size);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterf(GLenum  target,
                                                          GLenum  pname,
                                                          GLfloat param)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexParameterf(target,
                                               pname,
                                               param);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterfv(GLenum         target,
                                                           GLenum         pname,
                                                           const GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexParameterfv(target,
                                                pname,
                                                params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameteri(GLenum target,
                                                          GLenum pname,
                                                          GLint  param)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexParameteri(target,
                                               pname,
                                               param);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameteriv(GLenum       target,
                                                           GLenum       pname,
                                                           const GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexParameteriv(target,
                                                pname,
                                                params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterIiv(GLenum       target,
                                                            GLenum       pname,
                                                            const GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexParameterIiv(target,
                                                 pname,
                                                 params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterIuiv(GLenum        target,
                                                             GLenum        pname,
                                                             const GLuint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexParameterIuiv(target,
                                                  pname,
                                                  params);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexSubImage1D(GLenum        target,
                                                          GLint         level,
                                                          GLint         xoffset,
                                                          GLsizei       width,
                                                          GLenum        format,
                                                          GLenum        type,
                                                          const GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexSubImage1D(target,
                                               level,
                                               xoffset,
                                               width,
                                               format,
                                               type,
                                               pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexSubImage2D(GLenum        target,
                                                          GLint         level,
                                                          GLint         xoffset,
                                                          GLint         yoffset,
                                                          GLsizei       width,
                                                          GLsizei       height,
                                                          GLenum        format,
                                                          GLenum        type,
                                                          const GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexSubImage2D(target,
                                               level,
                                               xoffset,
                                               yoffset,
                                               width,
                                               height,
                                               format,
                                               type,
                                               pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexSubImage3D(GLenum        target,
                                                          GLint         level,
                                                          GLint         xoffset,
                                                          GLint         yoffset,
                                                          GLint         zoffset,
                                                          GLsizei       width,
                                                          GLsizei       height,
                                                          GLsizei       depth,
                                                          GLenum        format,
                                                          GLenum        type,
                                                          const GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ogl_context_state_cache state_cache = nullptr;
    ogl_context_to_bindings to_bindings = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTexSubImage3D(target,
                                               level,
                                               xoffset,
                                               yoffset,
                                               zoffset,
                                               width,
                                               height,
                                               depth,
                                               format,
                                               type,
                                               pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferEXT(GLuint texture,
                                                             GLenum target,
                                                             GLenum internalformat,
                                                             GLuint buffer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ral_format              format_ral   = raGL_utils_get_ral_format_for_ogl_enum      (internalformat);
    ral_texture_type        type_ral     = raGL_utils_get_ral_texture_type_for_ogl_enum(target);

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTextureBufferEXT(texture,
                                                  target,
                                                  internalformat,
                                                  buffer);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferRangeEXT(GLuint     texture,
                                                                  GLenum     target,
                                                                  GLenum     internalformat,
                                                                  GLuint     buffer,
                                                                  GLintptr   offset,
                                                                  GLsizeiptr size)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    ral_format              format_ral  = raGL_utils_get_ral_format_for_ogl_enum      (internalformat);
    ral_texture_type        type_ral    = raGL_utils_get_ral_texture_type_for_ogl_enum(target);

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTextureBufferRangeEXT(texture,
                                                       target,
                                                       internalformat,
                                                       buffer,
                                                       offset,
                                                       size);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage1DEXT(GLuint        texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLsizei       width,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTextureSubImage1DEXT(texture,
                                                      target,
                                                      level,
                                                      xoffset,
                                                      width,
                                                      format,
                                                      type,
                                                      pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage2DEXT(GLuint        texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLint         yoffset,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTextureSubImage2DEXT(texture,
                                                      target,
                                                      level,
                                                      xoffset,
                                                      yoffset,
                                                      width,
                                                      height,
                                                      format,
                                                      type,
                                                      pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage3DEXT(GLuint        texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLint         yoffset,
                                                                 GLint         zoffset,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLsizei       depth,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    _private_entrypoints_ptr->pGLTextureSubImage3DEXT(texture,
                                                      target,
                                                      level,
                                                      xoffset,
                                                      yoffset,
                                                      zoffset,
                                                      width,
                                                      height,
                                                      depth,
                                                      format,
                                                      type,
                                                      pixels);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glUniformBlockBinding(GLuint program,
                                                                GLuint uniformBlockIndex,
                                                                GLuint uniformBlockBinding)
{
    raGL_backend              context_backend = nullptr;
    ogl_context               context         = ogl_context_get_current_context();
    raGL_program              program_raGL    = nullptr;
    ogl_context_state_cache   state_cache     = nullptr;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BACKEND,
                                &context_backend);
    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    /* Does it make sense to try to make the call? */
    raGL_backend_get_program_by_id(context_backend,
                                   program,
                                  &program_raGL);

    ASSERT_DEBUG_SYNC(program_raGL != nullptr,
                      "The raGL_program instance for requested PO was not found.");

    if (program_raGL != nullptr)
    {
        GLuint current_indexed_ssb_bp = -1;

        raGL_program_get_block_property(program_raGL,
                                        RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                        uniformBlockIndex,
                                        RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                       &current_indexed_ssb_bp);

        //if (current_indexed_ssb_bp != uniformBlockBinding)
        {
            /* Update the internal cache */
            _private_entrypoints_ptr->pGLUniformBlockBinding(program,
                                                             uniformBlockIndex,
                                                             uniformBlockBinding);

            raGL_program_set_block_property(program_raGL,
                                            RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                            uniformBlockIndex,
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &uniformBlockBinding);
        }
    }

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif
}

/** Please see header for spec */
PUBLIC GLboolean APIENTRY ogl_context_wrappers_glUnmapBuffer(GLenum target)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = nullptr;
    GLboolean               result      = GL_FALSE;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    result = _private_entrypoints_ptr->pGLUnmapBuffer(target);

    #ifdef ENABLE_DEBUG_FINISH_CALLS
    {
        _private_entrypoints_ptr->pGLFinish();
    }
    #endif

    return result;
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glUseProgram(GLuint program)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT,
                                        &program);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexArrayVertexAttribIOffsetEXT(GLuint   vaobj,
                                                                              GLuint   buffer,
                                                                              GLuint   index,
                                                                              GLint    size,
                                                                              GLenum   type,
                                                                              GLsizei  stride,
                                                                              GLintptr offset)
{
    /* ..and having ARB_DSA entrypoints would greatly simplify that task. */
    ASSERT_DEBUG_SYNC(false,
                      "TODO. _ogl_context_wrappers_vertex_attrib_format_worker() requires modifications to support this entrypoint.");
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexArrayVertexAttribOffsetEXT(GLuint    vaobj,
                                                                             GLuint    buffer,
                                                                             GLuint    index,
                                                                             GLint     size,
                                                                             GLenum    type,
                                                                             GLboolean normalized,
                                                                             GLsizei   stride,
                                                                             GLintptr  offset)
{
    /* ..and having ARB_DSA entrypoints would greatly simplify that task. */
    ASSERT_DEBUG_SYNC(false,
                      "TODO. _ogl_context_wrappers_vertex_attrib_format_worker() requires modifications to support this entrypoint.");
}

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribBinding(GLuint attribindex,
                                                                GLuint bindingindex)
{
    ogl_context             context                = ogl_context_get_current_context();
    ogl_vao                 current_vao            = nullptr;
    GLuint                  current_vao_id         = -1;
    uint32_t                current_vao_vb_binding = -1;
    ogl_context_state_cache state_cache            = nullptr;
    ogl_context_vaos        vaos                   = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    /* Retrieve current VAO's VAA properties */
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    current_vao = ogl_context_vaos_get_vao(vaos,
                                           current_vao_id);

    ogl_vao_get_vaa_property(current_vao,
                             attribindex,
                             OGL_VAO_VAA_PROPERTY_VB_BINDING,
                            &current_vao_vb_binding);

    /* Update the binding only if necessary. */
    if (current_vao_vb_binding != bindingindex)
    {
        #ifdef ENABLE_DEBUG_FINISH_CALLS
        {
            _private_entrypoints_ptr->pGLFinish();
        }
        #endif

        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        _private_entrypoints_ptr->pGLVertexAttribBinding(attribindex,
                                                         bindingindex);

        ogl_vao_set_vaa_property(current_vao,
                                 attribindex,
                                 OGL_VAO_VAA_PROPERTY_VB_BINDING,
                                &bindingindex);
    }
}

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribFormat(GLuint    attribindex,
                                                               GLint     size,
                                                               GLenum    type,
                                                               GLboolean normalized,
                                                               GLuint    relativeoffset)
{
    _ogl_context_wrappers_vertex_attrib_format_worker(VAP_WORKER_TYPE_NORMALIZED,
                                                      attribindex,
                                                      size,
                                                      type,
                                                      normalized,
                                                      relativeoffset);
}

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribIFormat(GLuint attribindex,
                                                                GLint  size,
                                                                GLenum type,
                                                                GLuint relativeoffset)
{
    _ogl_context_wrappers_vertex_attrib_format_worker(VAP_WORKER_TYPE_INTEGER,
                                                      attribindex,
                                                      size,
                                                      type,
                                                      GL_FALSE, /* normalized - irrelevant */
                                                      relativeoffset);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribIPointer(GLuint        index,
                                                                 GLint         size,
                                                                 GLenum        type,
                                                                 GLsizei       stride,
                                                                 const GLvoid* pointer)
{
    _ogl_context_wrappers_vertex_attrib_pointer_worker(VAP_WORKER_TYPE_INTEGER,
                                                       index,
                                                       size,
                                                       type,
                                                       GL_FALSE, /* normalized - irrelevant */
                                                       stride,
                                                       pointer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribLFormat(GLuint attribindex,
                                                                GLint  size,
                                                                GLenum type,
                                                                GLuint relativeoffset)
{
    _ogl_context_wrappers_vertex_attrib_format_worker(VAP_WORKER_TYPE_DOUBLE,
                                                      attribindex,
                                                      size,
                                                      type,
                                                      GL_FALSE, /* normalized - irrelevant */
                                                      relativeoffset);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribPointer(GLuint        index,
                                                                GLint         size,
                                                                GLenum        type,
                                                                GLboolean     normalized,
                                                                GLsizei       stride,
                                                                const GLvoid* pointer)
{
    _ogl_context_wrappers_vertex_attrib_pointer_worker(VAP_WORKER_TYPE_NORMALIZED,
                                                       index,
                                                       size,
                                                       type,
                                                       normalized,
                                                       stride,
                                                       pointer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexBindingDivisor(GLuint bindingindex,
                                                                 GLuint divisor)
{
    ogl_context             context         = ogl_context_get_current_context();
    GLuint                  current_divisor = -1;
    ogl_vao                 current_vao     = nullptr;
    GLuint                  current_vao_id  = -1;
    ogl_context_state_cache state_cache     = nullptr;
    ogl_context_vaos        vaos            = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAOS,
                            &vaos);

    /* Retrieve current VAO's VAA properties */
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao_id);

    current_vao = ogl_context_vaos_get_vao(vaos,
                                           current_vao_id);

    ogl_vao_get_vb_property(current_vao,
                            bindingindex,
                            OGL_VAO_VB_PROPERTY_DIVISOR,
                           &current_divisor);

    /* Update the binding only if necessary. */
    if (current_divisor != divisor)
    {
        #ifdef ENABLE_DEBUG_FINISH_CALLS
        {
            _private_entrypoints_ptr->pGLFinish();
        }
        #endif

        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        _private_entrypoints_ptr->pGLVertexBindingDivisor(bindingindex,
                                                          divisor);

        ogl_vao_set_vb_property(current_vao,
                                bindingindex,
                                OGL_VAO_VB_PROPERTY_DIVISOR,
                               &divisor);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glViewport(GLint   x,
                                                     GLint   y,
                                                     GLsizei width,
                                                     GLsizei height)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;
    GLint                   viewport[]  = {x, y, width, height};

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_indexed_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_VIEWPORT,
                                                 0, /* index */
                                                 viewport);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glViewportIndexedfv(GLuint         index,
                                                              const GLfloat *v)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_indexed_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_VIEWPORT,
                                                 index,
                                                 v);
}


/* Please see header for specification */
PUBLIC void ogl_context_wrappers_set_private_functions(ogl_context_gl_entrypoints_private* ptrs)
{
    _private_entrypoints_ptr = ptrs;
}
