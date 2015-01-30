/**
 *
 * Emerald (kbi/elude @2013-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_bo_bindings.h"
#include "ogl/ogl_context_sampler_bindings.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_context_to_bindings.h"
#include "ogl/ogl_context_wrappers.h"
#include "ogl/ogl_texture.h"
#include <math.h>

__declspec(thread) ogl_context_gl_entrypoints_private* _private_entrypoints_ptr = NULL;


/************************************************************* OTHER STATE ************************************************************/
PUBLIC void APIENTRY ogl_context_wrappers_glBlendColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha)
{
    const GLfloat           blend_color[] = {red, green, blue, alpha};
    ogl_context             context       = ogl_context_get_current_context();
    ogl_context_state_cache state_cache   = NULL;

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
    ogl_context_state_cache state_cache = NULL;

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
PUBLIC void APIENTRY ogl_context_wrappers_glBlendEquationSeparate(GLenum modeRGB,
                                                                  GLenum modeAlpha)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

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
    ogl_context_state_cache state_cache = NULL;

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
    ogl_context_state_cache state_cache = NULL;

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
PUBLIC void APIENTRY ogl_context_wrappers_glClear(GLbitfield mask)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_COLOR      |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX      |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);

    _private_entrypoints_ptr->pGLClear(mask);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glClearColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha)
{
    GLfloat                 color[]     = {red, green, blue, alpha};
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR,
                                         color);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha)
{
    GLboolean               color_mask[] = {red, green, blue, alpha};
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK,
                                         color_mask);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthMask(GLboolean flag)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;

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
    /* TODO: Add support for other modes */
    if (cap == GL_BLEND)
    {
        ogl_context             context     = ogl_context_get_current_context();
        const bool              new_state   = false;
        ogl_context_state_cache state_cache = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);

        ogl_context_state_cache_set_property(state_cache,
                                             OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED,
                                            &new_state);
    }
    else
    {
        _private_entrypoints_ptr->pGLDisable(cap);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDisablei(GLenum cap,
                                                     GLuint index)
{
    /* TODO: Add support for other modes */
    if (cap == GL_BLEND && index == 0)
    {
        ogl_context             context     = ogl_context_get_current_context();
        const bool              new_state   = false;
        ogl_context_state_cache state_cache = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);

        ogl_context_state_cache_set_property(state_cache,
                                             OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED,
                                            &new_state);
    }
    else
    {
        _private_entrypoints_ptr->pGLDisablei(cap,
                                              index);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glEnable(GLenum cap)
{
    /* TODO: Add support for other modes */
    if (cap == GL_BLEND)
    {
        ogl_context             context     = ogl_context_get_current_context();
        const bool              new_state   = true;
        ogl_context_state_cache state_cache = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);

        ogl_context_state_cache_set_property(state_cache,
                                             OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED,
                                            &new_state);
    }
    else
    {
        _private_entrypoints_ptr->pGLEnable(cap);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glEnablei(GLenum cap,
                                                    GLuint index)
{
    /* TODO: Add support for other modes */
    if (cap == GL_BLEND && index == 0)
    {
        ogl_context             context     = ogl_context_get_current_context();
        const bool              new_state   = true;
        ogl_context_state_cache state_cache = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);

        ogl_context_state_cache_set_property(state_cache,
                                             OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED,
                                            &new_state);
    }
    else
    {
        _private_entrypoints_ptr->pGLEnablei(cap,
                                             index);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glScissor(GLint   x,
                                                    GLint   y,
                                                    GLsizei width,
                                                    GLsizei height)
{
    ogl_context             context       = ogl_context_get_current_context();
    GLint                   scissor_box[] = {x, y, width, height};
    ogl_context_state_cache state_cache   = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX,
                                         scissor_box);
}

/******************************************************** VERTEX ARRAY OBJECTS *********************************************************/
/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindVertexArray(GLuint array)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &array);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteVertexArrays(GLsizei       n,
                                                               const GLuint* arrays)
{
    ogl_context             context     = ogl_context_get_current_context();
    GLuint                  current_vao = 0;
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                        &current_vao);

    _private_entrypoints_ptr->pGLDeleteVertexArrays(n, arrays);

    for (GLsizei current_n = 0;
                 current_n < n;
               ++current_n)
    {
        static GLuint zero_vao_id = 0;

        if (arrays[current_n] == current_vao)
        {
            ogl_context_state_cache_set_property(state_cache,
                                                 OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,
                                                &zero_vao_id);

            break;
        }
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDisableVertexAttribArray(GLuint index)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLDisableVertexAttribArray(index);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glEnableVertexAttribArray(GLuint index)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLEnableVertexAttribArray(index);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribdv(GLuint    index,
                                                              GLenum    pname,
                                                              GLdouble* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLGetVertexAttribdv(index,
                                                   pname,
                                                   params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribfv(GLuint   index,
                                                              GLenum   pname,
                                                              GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLGetVertexAttribfv(index,
                                                   pname,
                                                   params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLGetVertexAttribiv(index,
                                                   pname,
                                                   params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribPointerv(GLuint   index,
                                                                    GLenum   pname,
                                                                    GLvoid** pointer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLGetVertexAttribPointerv(index,
                                                         pname,
                                                         pointer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLGetVertexAttribIiv(index,
                                                    pname,
                                                    params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribIuiv(GLuint  index,
                                                                GLenum  pname,
                                                                GLuint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

    _private_entrypoints_ptr->pGLGetVertexAttribIuiv(index,
                                                     pname,
                                                     params);
}

/******************************************************** PROGRAM OBJECTS **************************************************************/
/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchCompute(GLuint num_groups_x,
                                                            GLuint num_groups_y,
                                                            GLuint num_groups_z)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);

    _private_entrypoints_ptr->pGLDispatchCompute(num_groups_x,
                                                 num_groups_y,
                                                 num_groups_z);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchComputeIndirect(GLintptr indirect)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT);

    _private_entrypoints_ptr->pGLDispatchComputeIndirect(indirect);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glUseProgram(GLuint program)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_set_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT,
                                        &program);
}

/******************************************************** SAMPLER OBJECTS **************************************************************/
/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSampler(GLuint unit,
                                                        GLuint sampler)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = NULL;

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
    ogl_context_sampler_bindings sampler_bindings = NULL;

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

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterfv(GLuint   sampler,
                                                                  GLenum   pname,
                                                                  GLfloat* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    _private_entrypoints_ptr->pGLGetSamplerParameterfv(sampler,
                                                       pname,
                                                       params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterIiv(GLuint sampler,
                                                                   GLenum pname,
                                                                   GLint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    _private_entrypoints_ptr->pGLGetSamplerParameterIiv(sampler,
                                                        pname,
                                                        params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterIuiv(GLuint  sampler,
                                                                    GLenum  pname,
                                                                    GLuint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    _private_entrypoints_ptr->pGLGetSamplerParameterIuiv(sampler,
                                                         pname,
                                                         params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameteriv(GLuint sampler,
                                                                  GLenum pname,
                                                                  GLint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_sampler_bindings sampler_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SAMPLER_BINDINGS,
                            &sampler_bindings);

    ogl_context_sampler_bindings_sync(sampler_bindings);

    _private_entrypoints_ptr->pGLGetSamplerParameteriv(sampler,
                                                       pname,
                                                       params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterf(GLuint  sampler,
                                                              GLenum  pname,
                                                              GLfloat param)
{
    ogl_context context = ogl_context_get_current_context ();

    _private_entrypoints_ptr->pGLSamplerParameterf(sampler,
                                                   pname,
                                                   param);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterfv(GLuint         sampler,
                                                               GLenum         pname,
                                                               const GLfloat* param)
{
    ogl_context context = ogl_context_get_current_context ();

    _private_entrypoints_ptr->pGLSamplerParameterfv(sampler,
                                                    pname,
                                                    param);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameteri(GLuint sampler,
                                                              GLenum pname,
                                                              GLint  param)
{
    ogl_context context = ogl_context_get_current_context ();

    _private_entrypoints_ptr->pGLSamplerParameteri(sampler,
                                                   pname,
                                                   param);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameteriv(GLuint       sampler,
                                                               GLenum       pname,
                                                               const GLint* param)
{
    ogl_context context = ogl_context_get_current_context ();

    _private_entrypoints_ptr->pGLSamplerParameteriv(sampler,
                                                    pname,
                                                    param);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterIiv(GLuint       sampler,
                                                                GLenum       pname,
                                                                const GLint* param)
{
    ogl_context context = ogl_context_get_current_context ();

    _private_entrypoints_ptr->pGLSamplerParameterIiv(sampler,
                                                     pname,
                                                     param);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterIuiv(GLuint        sampler,
                                                                 GLenum        pname,
                                                                 const GLuint* param)
{
    ogl_context context = ogl_context_get_current_context ();

    _private_entrypoints_ptr->pGLSamplerParameterIuiv(sampler,
                                                      pname,
                                                      param);
}

/********************************************************* BUFFER OBJECTS **************************************************************/
/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBeginTransformFeedback(GLenum primitiveMode)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;

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
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffer(GLenum target,
                                                       GLuint buffer)
{
    ogl_context context = ogl_context_get_current_context();

    if (target != GL_ELEMENT_ARRAY_BUFFER)
    {
        ogl_context_bo_bindings bo_bindings = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);

        ogl_context_bo_bindings_set_binding(bo_bindings,
                                            target,
                                            buffer);
    }
    else
    {
        ogl_context_state_cache state_cache = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                &state_cache);

        ogl_context_state_cache_sync(state_cache,
                                     STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);

        _private_entrypoints_ptr->pGLBindBuffer(target,
                                                buffer);
    }
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

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
    ogl_context_bo_bindings bo_bindings = NULL;

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
    ogl_context_bo_bindings bo_bindings = NULL;

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
    ogl_context_bo_bindings bo_bindings = NULL;

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
PUBLIC void APIENTRY ogl_context_wrappers_glBufferData(GLenum        target,
                                                       GLsizeiptr    size,
                                                       const GLvoid* data,
                                                       GLenum        usage)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    if (target != GL_ELEMENT_ARRAY_BUFFER)
    {
        ogl_context_bo_bindings_sync(bo_bindings,
                                     ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );
    }

    _private_entrypoints_ptr->pGLBufferData(target,
                                            size,
                                            data,
                                            usage);

    ogl_context_bo_bindings_set_bo_storage_size(bo_bindings,
                                                ogl_context_bo_bindings_get_general_binding(bo_bindings, target),
                                                size);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferStorage(GLenum        target,
                                                          GLsizeiptr    size,
                                                          const GLvoid* data,
                                                          GLbitfield    flags)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    if (target != GL_ELEMENT_ARRAY_BUFFER)
    {
        ogl_context_bo_bindings_sync(bo_bindings,
                                     ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );
    }

    _private_entrypoints_ptr->pGLBufferStorage(target,
                                               size,
                                               data,
                                               flags);

    ogl_context_bo_bindings_set_bo_storage_size(bo_bindings,
                                                ogl_context_bo_bindings_get_general_binding(bo_bindings, target),
                                                size);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferSubData(GLenum        target,
                                                          GLintptr      offset,
                                                          GLsizeiptr    size,
                                                          const GLvoid* data)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLBufferSubData(target,
                                               offset,
                                               size,
                                               data);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteBuffers(GLsizei       n,
                                                          const GLuint* buffers)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    _private_entrypoints_ptr->pGLDeleteBuffers(n,
                                               buffers);

    ogl_context_bo_bindings_reset_buffer_bindings(bo_bindings,
                                                  buffers,
                                                  n);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArrays(GLenum  mode,
                                                       GLint   first,
                                                       GLsizei count)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArrays(mode,
                                            first,
                                            count);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysInstanced(GLenum  mode,
                                                                GLint   first,
                                                                GLsizei count,
                                                                GLsizei primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArraysInstanced(mode,
                                                     first,
                                                     count,
                                                     primcount);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysInstancedBaseInstance(GLenum  mode,
                                                                            GLint   first,
                                                                            GLsizei count,
                                                                            GLsizei primcount,
                                                                            GLuint  baseinstance)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawArraysInstancedBaseInstance(mode,
                                                                 first,
                                                                 count,
                                                                 primcount,
                                                                 baseinstance);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElements(GLenum        mode,
                                                         GLsizei       count,
                                                         GLenum        type,
                                                         const GLvoid* indices)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawElements(mode,
                                              count,
                                              type,
                                              indices);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstanced(GLenum        mode,
                                                                  GLsizei       count,
                                                                  GLenum        type,
                                                                  const GLvoid* indices,
                                                                  GLsizei       primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawElementsInstanced(mode,
                                                       count,
                                                       type,
                                                       indices,
                                                       primcount);
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
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawRangeElements(mode,
                                                   start,
                                                   end,
                                                   count,
                                                   type,
                                                   indices);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedback(GLenum mode,
                                                                  GLuint id)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLDrawTransformFeedback(mode,
                                                       id);
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
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBooleani_v(GLenum     target,
                                                          GLuint     index,
                                                          GLboolean* data)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBooleanv(GLenum     pname,
                                                        GLboolean* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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

    _private_entrypoints_ptr->pGLGetBooleanv(pname,
                                             params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferParameteriv(GLenum target,
                                                                 GLenum pname,
                                                                 GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLGetBufferParameteriv(target,
                                                      pname,
                                                      params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferParameteri64v(GLenum   target,
                                                                   GLenum   pname,
                                                                   GLint64* params)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                         OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                        &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLGetBufferParameteri64v(target,
                                                        pname,
                                                        params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferPointerv(GLenum   target,
                                                              GLenum   pname,
                                                              GLvoid** params)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLGetBufferPointerv(target,
                                                   pname,
                                                   params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferSubData(GLenum     target,
                                                             GLintptr   offset,
                                                             GLsizeiptr size,
                                                             GLvoid*    data)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    _private_entrypoints_ptr->pGLGetBufferSubData(target,
                                                  offset,
                                                  size,
                                                  data);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetDoublev(GLenum    pname,
                                                       GLdouble* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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

    _private_entrypoints_ptr->pGLGetDoublev(pname,
                                            params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetFloatv(GLenum   pname,
                                                      GLfloat* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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

    _private_entrypoints_ptr->pGLGetFloatv(pname,
                                           params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetInteger64i_v(GLenum   target,
                                                            GLuint   index,
                                                            GLint64* data)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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

    _private_entrypoints_ptr->pGLGetInteger64i_v(target,
                                                 index,
                                                 data);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetIntegeri_v(GLenum target,
                                                          GLuint index,
                                                          GLint* data)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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

    _private_entrypoints_ptr->pGLGetIntegeri_v(target,
                                               index,
                                               data);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetIntegerv(GLenum pname,
                                                        GLint* params)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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

    _private_entrypoints_ptr->pGLGetIntegerv(pname,
                                             params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetNamedBufferParameterivEXT(GLuint buffer,
                                                                         GLenum pname,
                                                                         GLint* params)
{
    _private_entrypoints_ptr->pGLGetNamedBufferParameterivEXT(buffer,
                                                              pname,
                                                              params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetNamedBufferPointervEXT(GLuint buffer,
                                                                      GLenum pname,
                                                                      void** params)
{
    _private_entrypoints_ptr->pGLGetNamedBufferPointervEXT(buffer,
                                                           pname,
                                                           params);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glGetNamedBufferSubDataEXT(GLuint     buffer,
                                                                     GLintptr   offset,
                                                                     GLsizeiptr size,
                                                                     void*      data)
{
    _private_entrypoints_ptr->pGLGetNamedBufferSubDataEXT(buffer,
                                                          offset,
                                                          size,
                                                          data);
}

/** Please see header for spec */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBuffer(GLenum target,
                                                         GLenum access)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    GLvoid*                 result      = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    result = _private_entrypoints_ptr->pGLMapBuffer(target,
                                                    access);

    return result;
}

/** Please see header for spec */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBufferRange(GLenum     target,
                                                              GLintptr   offset,
                                                              GLsizeiptr length,
                                                              GLbitfield access)
{
    ogl_context             context     = ogl_context_get_current_context ();
    ogl_context_bo_bindings bo_bindings = NULL;
    GLvoid*                 result      = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    result = _private_entrypoints_ptr->pGLMapBufferRange(target,
                                                         offset,
                                                         length,
                                                         access);

    return result;
}

/** Please see header for spec */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawArrays(GLenum         mode,
                                                              const GLint*   first,
                                                              const GLsizei* count,
                                                              GLsizei        primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLMultiDrawArrays(mode,
                                                 first,
                                                 count,
                                                 primcount);
}

/** Please see header for spec */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawElements(GLenum               mode,
                                                                const GLsizei*       count,
                                                                GLenum               type,
                                                                const GLvoid* const* indices,
                                                                GLsizei              primcount)
{
    ogl_context                  context          = ogl_context_get_current_context ();
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX    | STATE_CACHE_SYNC_BIT_BLENDING                   |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLMultiDrawElements(mode,
                                                   count,
                                                   type,
                                                   indices,
                                                   primcount);
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
    ogl_context_bo_bindings      bo_bindings      = NULL;
    ogl_context_sampler_bindings sampler_bindings = NULL;
    ogl_context_state_cache      state_cache      = NULL;
    ogl_context_to_bindings      to_bindings      = NULL;

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
                                      STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT | STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT |
                                      STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_bo_bindings_sync     (bo_bindings,
                                      BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER     | BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER |
                                      BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER | BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER);
    ogl_context_sampler_bindings_sync(sampler_bindings);
    ogl_context_to_bindings_sync     (to_bindings,
                                      OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_ALL);

    _private_entrypoints_ptr->pGLMultiDrawElementsBaseVertex(mode,
                                                             count,
                                                             type,
                                                             indices,
                                                             drawcount,
                                                             basevertex);
}

/** Please see header for spec */
PUBLIC void* APIENTRY ogl_context_wrappers_glMapNamedBufferEXT(GLuint buffer,
                                                               GLenum access)
{
    ogl_context context = ogl_context_get_current_context();
    void*       result  = NULL;

    result = _private_entrypoints_ptr->pGLMapNamedBufferEXT(buffer,
                                                            access);

    return result;
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferDataEXT(GLuint      buffer,
                                                               GLsizeiptr  size,
                                                               const void* data,
                                                               GLenum      usage)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    _private_entrypoints_ptr->pGLNamedBufferDataEXT(buffer,
                                                    size,
                                                    data,
                                                    usage);

    ogl_context_bo_bindings_set_bo_storage_size(bo_bindings,
                                                buffer,
                                                size);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferStorageEXT(GLuint        buffer,
                                                                  GLsizeiptr    size,
                                                                  const GLvoid* data,
                                                                  GLbitfield    flags)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    _private_entrypoints_ptr->pGLNamedBufferStorageEXT(buffer,
                                                       size,
                                                       data,
                                                       flags);

    ogl_context_bo_bindings_set_bo_storage_size(bo_bindings,
                                                buffer,
                                                size);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferSubDataEXT(GLuint      buffer,
                                                                  GLintptr    offset,
                                                                  GLsizeiptr  size,
                                                                  const void* data)
{
    _private_entrypoints_ptr->pGLNamedBufferSubDataEXT(buffer,
                                                       offset,
                                                       size,
                                                       data);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedCopyBufferSubDataEXT(GLuint     readBuffer,
                                                                      GLuint     writeBuffer,
                                                                      GLintptr   readOffset,
                                                                      GLintptr   writeOffset,
                                                                      GLsizeiptr size)
{
    _private_entrypoints_ptr->pGLNamedCopyBufferSubDataEXT(readBuffer,
                                                           writeBuffer,
                                                           readOffset,
                                                           writeOffset,
                                                           size);
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
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);

    _private_entrypoints_ptr->pGLReadPixels(x,
                                            y,
                                            width,
                                            height,
                                            format,
                                            type,
                                            pixels);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glResumeTransformFeedback(void)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;

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

    _private_entrypoints_ptr->pGLResumeTransformFeedback();
}

/** Please see header for spec */
PUBLIC GLboolean APIENTRY ogl_context_wrappers_glUnmapBuffer(GLenum target)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    GLboolean               result      = GL_FALSE;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);

    ogl_context_bo_bindings_sync(bo_bindings,
                                 ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(target) );

    result = _private_entrypoints_ptr->pGLUnmapBuffer(target);

    return result;
}

/** Please see header for spec */
PUBLIC GLboolean APIENTRY ogl_context_wrappers_glUnmapNamedBufferEXT(GLuint buffer)
{
    GLboolean result = GL_FALSE;

    result = _private_entrypoints_ptr->pGLUnmapNamedBufferEXT(buffer);

    return result;
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
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER);

    _private_entrypoints_ptr->pGLVertexArrayVertexAttribIOffsetEXT(vaobj,
                                                                   buffer,
                                                                   index,
                                                                   size,
                                                                   type,
                                                                   stride,
                                                                   offset);
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
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER);

    _private_entrypoints_ptr->pGLVertexArrayVertexAttribOffsetEXT(vaobj,
                                                                  buffer,
                                                                  index,
                                                                  size,
                                                                  type,
                                                                  normalized,
                                                                  stride,
                                                                  offset);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribIPointer(GLuint        index,
                                                                 GLint         size,
                                                                 GLenum        type,
                                                                 GLsizei       stride,
                                                                 const GLvoid* pointer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER);

    _private_entrypoints_ptr->pGLVertexAttribIPointer(index,
                                                      size,
                                                      type,
                                                      stride,
                                                      pointer);
}

/** Please see header for spec */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribPointer(GLuint        index,
                                                                GLint         size,
                                                                GLenum        type,
                                                                GLboolean     normalized,
                                                                GLsizei       stride,
                                                                const GLvoid* pointer)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                            &bo_bindings);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER);

    _private_entrypoints_ptr->pGLVertexAttribPointer(index,
                                                     size,
                                                     type,
                                                     normalized,
                                                     stride,
                                                     pointer);
}

/********************************************************* TEXTURE OBJECTS *************************************************************/
/** TODO */
PRIVATE void _ogl_context_wrappers_update_mipmap_level_info(__in __notnull ogl_context context,
                                                            __in           GLenum      texture_target,
                                                            __in __notnull ogl_texture texture,
                                                            __in           GLuint      level)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints       = NULL;
    GLuint                                                    texture_gl_id         = 0;
    GLint                                                     mipmap_depth          = 0;
    GLint                                                     mipmap_height         = 0;
    GLint                                                     mipmap_internalformat = GL_NONE;
    GLint                                                     mipmap_width          = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_gl_id);

    dsa_entrypoints->pGLGetTextureLevelParameterivEXT(texture,
                                                      texture_target,
                                                      level,
                                                      GL_TEXTURE_DEPTH,
                                                     &mipmap_depth);
    dsa_entrypoints->pGLGetTextureLevelParameterivEXT(texture,
                                                      texture_target,
                                                      level,
                                                      GL_TEXTURE_HEIGHT,
                                                     &mipmap_height);
    dsa_entrypoints->pGLGetTextureLevelParameterivEXT(texture,
                                                      texture_target,
                                                      level,
                                                      GL_TEXTURE_INTERNAL_FORMAT,
                                                     &mipmap_internalformat);
    dsa_entrypoints->pGLGetTextureLevelParameterivEXT(texture,
                                                      texture_target,
                                                      level,
                                                      GL_TEXTURE_WIDTH,
                                                     &mipmap_width);

    if (mipmap_depth  != 0 ||
        mipmap_height != 0 ||
        mipmap_width  != 0)
    {
        ogl_texture_set_mipmap_property(texture,
                                        level,
                                        OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &mipmap_width);
        ogl_texture_set_mipmap_property(texture,
                                        level,
                                        OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &mipmap_height);
        ogl_texture_set_mipmap_property(texture,
                                        level,
                                        OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                        &mipmap_depth);
        ogl_texture_set_property       (texture,
                                        OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                        &mipmap_internalformat);
    }
}

/** TODO */
PRIVATE void _ogl_context_wrappers_update_mipmap_info(__in __notnull ogl_context context,
                                                      __in           GLenum      texture_target,
                                                      __in __notnull ogl_texture texture)
{
    const ogl_context_gl_limits* limits        = NULL;
    GLuint                       texture_gl_id = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_gl_id);

    /* Iterate through all allowed levels */
    unsigned int max_levels = (unsigned int) (log10( (double) limits->max_texture_size) / log10(2.0));

    for (GLuint level = 0; level < max_levels; ++level)
    {
        _ogl_context_wrappers_update_mipmap_level_info(context,
                                                       texture_target,
                                                       texture,
                                                       level);
    }
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glActiveTexture(GLenum texture)
{
    ogl_context_state_cache cache              = NULL;
    GLuint                  texture_unit_index = texture - GL_TEXTURE0;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &cache);

    ogl_context_state_cache_set_property(cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit_index);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glBindImageTextureEXT(GLuint      index,
                                                                ogl_texture texture,
                                                                GLint       level,
                                                                GLboolean   layered,
                                                                GLint       layer,
                                                                GLenum      access,
                                                                GLint       format)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLBindImageTexture(index,
                                                  texture_id,
                                                  level,
                                                  layered,
                                                  layer,
                                                  access,
                                                  format);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glBindMultiTextureEXT(GLenum      texunit,
                                                                GLenum      target,
                                                                ogl_texture texture)
{
    GLuint                  texture_id  = 0;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);
    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_to_bindings_set_binding(to_bindings,
                                        texunit - GL_TEXTURE0,
                                        target,
                                        texture);

}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTexture(GLenum      target,
                                                        ogl_texture texture)
{
    ogl_context             context      = ogl_context_get_current_context();
    GLuint                  texture_id   = 0;
    GLuint                  texture_unit = -1;
    ogl_context_state_cache state_cache  = NULL;
    ogl_context_to_bindings to_bindings  = NULL;

    if (texture != NULL)
    {
        ogl_texture_get_property(texture,
                                 OGL_TEXTURE_PROPERTY_ID,
                                &texture_id);
    }

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
PUBLIC void APIENTRY ogl_context_wrappers_glBindTextures(GLuint       first,
                                                         GLsizei      count,
                                                         ogl_texture* textures)
{
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    for (GLint n_texture = 0;
               n_texture < count;
             ++n_texture)
    {
        GLuint texture_id = 0;

        if (textures != NULL)
        {
            ogl_texture_get_property(textures[n_texture],
                                     OGL_TEXTURE_PROPERTY_ID,
                                    &texture_id);
        }

        if (texture_id == 0)
        {
            ogl_context_to_bindings_reset_all_bindings_for_texture_unit(to_bindings,
                                                                        first + n_texture);
        }
        else
        {
            GLenum texture_target;

            ogl_texture_get_property(textures[n_texture],
                                     OGL_TEXTURE_PROPERTY_TARGET,
                                    &texture_target);

            ogl_context_to_bindings_set_binding(to_bindings,
                                                first + n_texture, /* texture_unit */
                                                texture_target,
                                                textures[n_texture]);
        }
    }
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexImage3D(GLenum        target,
                                                                 GLint         level,
                                                                 GLenum        internalformat,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLsizei       depth,
                                                                 GLint         border,
                                                                 GLsizei       imageSize,
                                                                 const GLvoid* data)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTexImage3D(target,
                                                      level,
                                                      internalformat,
                                                      width,
                                                      height,
                                                      depth,
                                                      border,
                                                      imageSize,
                                                      data);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                    &depth);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexImage2D(GLenum        target,
                                                                 GLint         level,
                                                                 GLenum        internalformat,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLint         border,
                                                                 GLsizei       imageSize,
                                                                 const GLvoid* data)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTexImage2D(target,
                                                      level,
                                                      internalformat,
                                                      width,
                                                      height,
                                                      border,
                                                      imageSize,
                                                      data);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexImage1D(GLenum        target,
                                                                 GLint         level,
                                                                 GLenum        internalformat,
                                                                 GLsizei       width,
                                                                 GLint         border,
                                                                 GLsizei       imageSize,
                                                                 const GLvoid* data)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCompressedTexImage1D(target,
                                                      level,
                                                      internalformat,
                                                      width,
                                                      border,
                                                      imageSize,
                                                      data);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
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
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage1DEXT(ogl_texture   texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLsizei       width,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCompressedTextureSubImage1DEXT(texture_id,
                                                                target,
                                                                level,
                                                                xoffset,
                                                                width,
                                                                format,
                                                                imageSize,
                                                                bits);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage2DEXT(ogl_texture   texture,
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
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCompressedTextureSubImage2DEXT(texture_id,
                                                                target,
                                                                level,
                                                                xoffset,
                                                                yoffset,
                                                                width,
                                                                height,
                                                                format,
                                                                imageSize,
                                                                bits);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage3DEXT(ogl_texture   texture,
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
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCompressedTextureSubImage3DEXT(texture_id,
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
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureImage1DEXT(ogl_texture   texture,
                                                                        GLenum        target,
                                                                        GLint         level,
                                                                        GLenum        internalformat,
                                                                        GLsizei       width,
                                                                        GLint         border,
                                                                        GLsizei       imageSize,
                                                                        const GLvoid* bits)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCompressedTextureImage1DEXT(texture_id,
                                                             target,
                                                             level,
                                                             internalformat,
                                                             width,
                                                             border,
                                                             imageSize,
                                                             bits);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureImage2DEXT(ogl_texture   texture,
                                                                        GLenum        target,
                                                                        GLint         level,
                                                                        GLenum        internalformat,
                                                                        GLsizei       width,
                                                                        GLsizei       height,
                                                                        GLint         border,
                                                                        GLsizei       imageSize,
                                                                        const GLvoid* bits)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCompressedTextureImage2DEXT(texture_id,
                                                             target,
                                                             level,
                                                             internalformat,
                                                             width,
                                                             height,
                                                             border,
                                                             imageSize,
                                                             bits);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureImage3DEXT(ogl_texture   texture,
                                                                        GLenum        target,
                                                                        GLint         level,
                                                                        GLenum        internalformat,
                                                                        GLsizei       width,
                                                                        GLsizei       height,
                                                                        GLsizei       depth,
                                                                        GLint         border,
                                                                        GLsizei       imageSize,
                                                                        const GLvoid* bits)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCompressedTextureImage3DEXT(texture_id,
                                                             target,
                                                             level,
                                                             internalformat,
                                                             width,
                                                             height,
                                                             depth,
                                                             border,
                                                             imageSize,
                                                             bits);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                    &depth);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexImage1D(GLenum  target,
                                                           GLint   level,
                                                           GLenum  internalformat,
                                                           GLint   x,
                                                           GLint   y,
                                                           GLsizei width,
                                                           GLint   border)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCopyTexImage1D(target,
                                                level,
                                                internalformat,
                                                x,
                                                y,
                                                width,
                                                border);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexImage2D(GLenum  target,
                                                           GLint   level,
                                                           GLenum  internalformat,
                                                           GLint   x,
                                                           GLint   y,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLint   border)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCopyTexImage2D(target,
                                                level,
                                                internalformat,
                                                x,
                                                y,
                                                width,
                                                height,
                                                border);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
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
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT |
                                 STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLCopyTexSubImage1D(target,
                                                   level,
                                                   xoffset,
                                                   x,
                                                   y,
                                                   width);
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
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT |
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
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT |
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
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureImage1DEXT(ogl_texture texture,
                                                                  GLenum      target,
                                                                  GLint       level,
                                                                  GLenum      internalformat,
                                                                  GLint       x,
                                                                  GLint       y,
                                                                  GLsizei     width,
                                                                  GLint       border)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCopyTextureImage1DEXT(texture_id,
                                                       target,
                                                       level,
                                                       internalformat,
                                                       x,
                                                       y,
                                                       width,
                                                       border);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureImage2DEXT(ogl_texture texture,
                                                                  GLenum      target,
                                                                  GLint       level,
                                                                  GLenum      internalformat,
                                                                  GLint       x,
                                                                  GLint       y,
                                                                  GLsizei     width,
                                                                  GLsizei     height,
                                                                  GLint       border)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCopyTextureImage2DEXT(texture_id,
                                                       target,
                                                       level,
                                                       internalformat,
                                                       x,
                                                       y,
                                                       width,
                                                       height,
                                                       border);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage1DEXT(ogl_texture texture,
                                                                     GLenum      target,
                                                                     GLint       level,
                                                                     GLint       xoffset,
                                                                     GLint       x,
                                                                     GLint       y,
                                                                     GLsizei     width)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCopyTextureSubImage1DEXT(texture_id,
                                                          target,
                                                          level,
                                                          xoffset,
                                                          x,
                                                          y,
                                                          width);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage2DEXT(ogl_texture texture,
                                                                     GLenum      target,
                                                                     GLint       level,
                                                                     GLint       xoffset,
                                                                     GLint       yoffset,
                                                                     GLint       x,
                                                                     GLint       y,
                                                                     GLsizei     width,
                                                                     GLsizei     height)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCopyTextureSubImage2DEXT(texture_id,
                                                          target,
                                                          level,
                                                          xoffset,
                                                          yoffset,
                                                          x,
                                                          y,
                                                          width,
                                                          height);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage3DEXT(ogl_texture texture,
                                                                     GLenum      target,
                                                                     GLint       level,
                                                                     GLint       xoffset,
                                                                     GLint       yoffset,
                                                                     GLint       zoffset,
                                                                     GLint       x,
                                                                     GLint       y,
                                                                     GLsizei     width,
                                                                     GLsizei     height)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLCopyTextureSubImage3DEXT(texture_id,
                                                          target,
                                                          level,
                                                          xoffset,
                                                          yoffset,
                                                          zoffset,
                                                          x,
                                                          y,
                                                          width,
                                                          height);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture(GLenum      target,
                                                               GLenum      attachment,
                                                               ogl_texture texture,
                                                               GLint       level)
{
    GLuint                  texture_id  = 0;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLFramebufferTexture(target,
                                                    attachment,
                                                    texture_id,
                                                    level);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture1D(GLenum      target,
                                                                 GLenum      attachment,
                                                                 GLenum      textarget,
                                                                 ogl_texture texture,
                                                                 GLint       level)
{
    GLuint                  texture_id  = 0;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(textarget) );

    _private_entrypoints_ptr->pGLFramebufferTexture1D(target,
                                                      attachment,
                                                      textarget,
                                                      texture_id,
                                                      level);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture2D(GLenum      target,
                                                                 GLenum      attachment,
                                                                 GLenum      textarget,
                                                                 ogl_texture texture,
                                                                 GLint       level)
{
    GLuint                  texture_id  = 0;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(textarget) );

    _private_entrypoints_ptr->pGLFramebufferTexture2D(target,
                                                      attachment,
                                                      textarget,
                                                      texture_id,
                                                      level);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture3D(GLenum      target,
                                                                 GLenum      attachment,
                                                                 GLenum      textarget,
                                                                 ogl_texture texture,
                                                                 GLint       level,
                                                                 GLint       layer)
{
    GLuint                  texture_id  = 0;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(textarget) );

    _private_entrypoints_ptr->pGLFramebufferTexture3D(target,
                                                      attachment,
                                                      textarget,
                                                      texture_id,
                                                      level,
                                                      layer);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTextureLayer(GLenum      target,
                                                                    GLenum      attachment,
                                                                    ogl_texture texture,
                                                                    GLint       level,
                                                                    GLint       layer)
{
    GLuint                  texture_id  = 0;
    ogl_context_to_bindings to_bindings = NULL;

    ogl_context_get_property(ogl_context_get_current_context(),
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLFramebufferTextureLayer(target,
                                                         attachment,
                                                         texture_id,
                                                         level,
                                                         layer);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGenerateTextureMipmapEXT(ogl_texture texture,
                                                                     GLenum      target)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGenerateTextureMipmapEXT(texture_id,
                                                          target);

    _ogl_context_wrappers_update_mipmap_info(ogl_context_get_current_context(),
                                             target,
                                             texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTexImage(GLenum  target,
                                                                  GLint   level,
                                                                  GLvoid* img)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetCompressedTexImage(target,
                                                       level,
                                                       img);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTextureImageEXT(ogl_texture texture,
                                                                         GLenum      target,
                                                                         GLint       lod,
                                                                         GLvoid*     img)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGetCompressedTextureImageEXT(texture_id,
                                                              target,
                                                              lod,
                                                              img);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexImage(GLenum  target,
                                                        GLint   level,
                                                        GLenum  format,
                                                        GLenum  type,
                                                        GLvoid* pixels)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexImage(target,
                                             level,
                                             format,
                                             type,
                                             pixels);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexLevelParameterfv(GLenum   target,
                                                                   GLint    level,
                                                                   GLenum   pname,
                                                                   GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexLevelParameterfv(target,
                                                        level,
                                                        pname,
                                                        params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexLevelParameteriv(GLenum target,
                                                                   GLint  level,
                                                                   GLenum pname,
                                                                   GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexLevelParameteriv(target,
                                                        level,
                                                        pname,
                                                        params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureLevelParameterfvEXT(ogl_texture texture,
                                                                          GLenum      target,
                                                                          GLint       level,
                                                                          GLenum      pname,
                                                                          GLfloat*    params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGetTextureLevelParameterfvEXT(texture_id,
                                                               target,
                                                               level,
                                                               pname,
                                                               params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureLevelParameterivEXT(ogl_texture texture,
                                                                          GLenum      target,
                                                                          GLint       level,
                                                                          GLenum      pname,
                                                                          GLint*      params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGetTextureLevelParameterivEXT(texture_id,
                                                               target,
                                                               level,
                                                               pname,
                                                               params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterfv(GLenum   target,
                                                              GLenum   pname,
                                                              GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexParameterfv(target,
                                                   pname,
                                                   params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexParameteriv(target,
                                                   pname,
                                                   params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterIiv(GLenum target,
                                                               GLenum pname,
                                                               GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexParameterIiv(target,
                                                    pname,
                                                    params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterIuiv(GLenum  target,
                                                                GLenum  pname,
                                                                GLuint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLGetTexParameterIuiv(target,
                                                     pname,
                                                     params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureImageEXT(ogl_texture texture,
                                                               GLenum      target,
                                                               GLint       level,
                                                               GLenum      format,
                                                               GLenum      type,
                                                               GLvoid*     pixels)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGetTextureImageEXT(texture_id,
                                                    target,
                                                    level,
                                                    format,
                                                    type,
                                                    pixels);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureParameterIiv(ogl_texture texture,
                                                                   GLenum      target,
                                                                   GLenum      pname,
                                                                   GLint*      params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGetTextureParameterIiv(texture_id,
                                                        target,
                                                        pname,
                                                        params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureParameterIuiv(ogl_texture texture,
                                                                    GLenum      target,
                                                                    GLenum      pname,
                                                                    GLuint*     params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLGetTextureParameterIuiv(texture_id,
                                                         target,
                                                         pname,
                                                         params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTexture1DEXT(GLuint      framebuffer,
                                                                         GLenum      attachment,
                                                                         GLenum      textarget,
                                                                         ogl_texture texture,
                                                                         GLint       level)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLNamedFramebufferTexture1DEXT(framebuffer,
                                                              attachment,
                                                              textarget,
                                                              texture_id,
                                                              level);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTexture2DEXT(GLuint      framebuffer,
                                                                         GLenum      attachment,
                                                                         GLenum      textarget,
                                                                         ogl_texture texture,
                                                                         GLint       level)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLNamedFramebufferTexture2DEXT(framebuffer,
                                                              attachment,
                                                              textarget,
                                                              texture_id,
                                                              level);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTexture3DEXT(GLuint      framebuffer,
                                                                         GLenum      attachment,
                                                                         GLenum      textarget,
                                                                         ogl_texture texture,
                                                                         GLint       level,
                                                                         GLint       zoffset)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLNamedFramebufferTexture3DEXT(framebuffer,
                                                              attachment,
                                                              textarget,
                                                              texture_id,
                                                              level,
                                                              zoffset);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTextureEXT(GLuint      framebuffer,
                                                                       GLenum      attachment,
                                                                       ogl_texture texture,
                                                                       GLint       level)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLNamedFramebufferTextureEXT(framebuffer,
                                                            attachment,
                                                            texture_id,
                                                            level);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTextureFaceEXT(GLuint      framebuffer,
                                                                           GLenum      attachment,
                                                                           ogl_texture texture,
                                                                           GLint       level,
                                                                           GLenum      face)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLNamedFramebufferTextureFaceEXT(framebuffer,
                                                                attachment,
                                                                texture_id,
                                                                level,
                                                                face);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTextureLayerEXT(GLuint      framebuffer,
                                                                            GLenum      attachment,
                                                                            ogl_texture texture,
                                                                            GLint       level,
                                                                            GLint       layer)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(framebuffer,
                                                                 attachment,
                                                                 texture_id,
                                                                 level,
                                                                 layer);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexBuffer(GLenum target,
                                                      GLenum internalformat,
                                                      GLuint buffer)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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

    _private_entrypoints_ptr->pGLTexBuffer(target,
                                           internalformat,
                                           buffer);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_property(bound_texture,
                             OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                            &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexBufferRange(GLenum     target,
                                                           GLenum     internalformat,
                                                           GLuint     buffer,
                                                           GLintptr   offset,
                                                           GLsizeiptr size)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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

    _private_entrypoints_ptr->pGLTexBufferRange(target,
                                                internalformat,
                                                buffer,
                                                offset,
                                                size);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_property(bound_texture,
                             OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                            &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexImage1D(GLenum        target,
                                                       GLint         level,
                                                       GLint         internalformat,
                                                       GLsizei       width,
                                                       GLint         border,
                                                       GLenum        format,
                                                       GLenum        type,
                                                       const GLvoid* pixels)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexImage1D(target,
                                            level,
                                            internalformat,
                                            width,
                                            border,
                                            format,
                                            type,
                                            pixels);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexImage2D(GLenum        target,
                                                       GLint         level,
                                                       GLint         internalformat,
                                                       GLsizei       width,
                                                       GLsizei       height,
                                                       GLint         border,
                                                       GLenum        format,
                                                       GLenum        type,
                                                       const GLvoid* pixels)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexImage2D(target,
                                            level,
                                            internalformat,
                                            width,
                                            height,
                                            border,
                                            format,
                                            type,
                                            pixels);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexImage3D(GLenum        target,
                                                       GLint         level,
                                                       GLint         internalformat,
                                                       GLsizei       width,
                                                       GLsizei       height,
                                                       GLsizei       depth,
                                                       GLint         border,
                                                       GLenum        format,
                                                       GLenum        type,
                                                       const GLvoid* pixels)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings  = NULL;
    ogl_context_state_cache state_cache  = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

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
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);
    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexImage3D(target,
                                            level,
                                            internalformat,
                                            width,
                                            height,
                                            depth,
                                            border,
                                            format,
                                            type,
                                            pixels);

    ogl_texture bound_texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                                          texture_unit,
                                                                          target);

    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_mipmap_property(bound_texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                    &depth);
    ogl_texture_set_property       (bound_texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterf(GLenum  target,
                                                          GLenum  pname,
                                                          GLfloat param)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexParameterf(target,
                                               pname,
                                               param);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterfv(GLenum         target,
                                                           GLenum         pname,
                                                           const GLfloat* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexParameterfv(target,
                                                pname,
                                                params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameteri(GLenum target,
                                                          GLenum pname,
                                                          GLint  param)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexParameteri(target,
                                               pname,
                                               param);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameteriv(GLenum       target,
                                                           GLenum       pname,
                                                           const GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexParameteriv(target,
                                                pname,
                                                params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterIiv(GLenum       target,
                                                            GLenum       pname,
                                                            const GLint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexParameterIiv(target,
                                                 pname,
                                                 params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterIuiv(GLenum        target,
                                                             GLenum        pname,
                                                             const GLuint* params)
{
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexParameterIuiv(target,
                                                  pname,
                                                  params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage1D(GLenum  target,
                                                         GLsizei levels,
                                                         GLenum  internalformat,
                                                         GLsizei width)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    ogl_texture             texture      = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                        texture_unit,
                                                        target),

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexStorage1D(target,
                                              levels,
                                              internalformat,
                                              width);

    _ogl_context_wrappers_update_mipmap_info(context,
                                             target,
                                             texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage2D(GLenum  target,
                                                         GLsizei levels,
                                                         GLenum  internalformat,
                                                         GLsizei width,
                                                         GLsizei height)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    ogl_texture             texture      = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                        texture_unit,
                                                        target),

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexStorage2D(target,
                                              levels,
                                              internalformat,
                                              width,
                                              height);

    _ogl_context_wrappers_update_mipmap_info(context,
                                             target,
                                             texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage2DMultisample(GLenum    target,
                                                                    GLsizei   levels,
                                                                    GLenum    internalformat,
                                                                    GLsizei   width,
                                                                    GLsizei   height,
                                                                    GLboolean fixedsamplelocations)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    ogl_texture             texture      = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexStorage2DMultisample(target,
                                                         levels,
                                                         internalformat,
                                                         width,
                                                         height,
                                                         fixedsamplelocations);

    texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                        texture_unit,
                                                        target),

    _ogl_context_wrappers_update_mipmap_info(context,
                                             target,
                                             texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage3D(GLenum  target,
                                                         GLsizei levels,
                                                         GLenum  internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    ogl_texture             texture      = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                        texture_unit,
                                                        target);

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexStorage3D(target,
                                              levels,
                                              internalformat,
                                              width,
                                              height,
                                              depth);

    _ogl_context_wrappers_update_mipmap_info(context, target, texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage3DMultisample(GLenum    target,
                                                                    GLsizei   levels,
                                                                    GLenum    internalformat,
                                                                    GLsizei   width,
                                                                    GLsizei   height,
                                                                    GLsizei   depth,
                                                                    GLboolean fixedsamplelocations)
{
    ogl_context             context      = ogl_context_get_current_context();
    ogl_context_state_cache state_cache  = NULL;
    ogl_texture             texture      = NULL;
    uint32_t                texture_unit = -1;
    ogl_context_to_bindings to_bindings  = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_STATE_CACHE,
                            &state_cache);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TO_BINDINGS,
                            &to_bindings);

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,
                                        &texture_unit);

    texture = ogl_context_to_bindings_get_bound_texture(to_bindings,
                                                        texture_unit,
                                                        target),

    ogl_context_state_cache_sync(state_cache,
                                 STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT);
    ogl_context_to_bindings_sync(to_bindings,
                                 ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(target) );

    _private_entrypoints_ptr->pGLTexStorage3DMultisample(target,
                                                         levels,
                                                         internalformat,
                                                         width,
                                                         height,
                                                         depth,
                                                         fixedsamplelocations);

    _ogl_context_wrappers_update_mipmap_info(context,
                                             target,
                                             texture);
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
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexSubImage1D(target,
                                               level,
                                               xoffset,
                                               width,
                                               format,
                                               type,
                                               pixels);
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
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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

    _private_entrypoints_ptr->pGLTexSubImage2D(target,
                                               level,
                                               xoffset,
                                               yoffset,
                                               width,
                                               height,
                                               format,
                                               type,
                                               pixels);
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
    ogl_context_bo_bindings bo_bindings = NULL;
    ogl_context_state_cache state_cache = NULL;
    ogl_context_to_bindings to_bindings = NULL;

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
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferEXT(ogl_texture texture,
                                                             GLenum      target,
                                                             GLenum      internalformat,
                                                             GLuint      buffer)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureBufferEXT(texture_id,
                                                  target,
                                                  internalformat,
                                                  buffer);

    ogl_texture_set_property(texture,
                             OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                            &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferRangeEXT(ogl_texture texture,
                                                                  GLenum      target,
                                                                  GLenum      internalformat,
                                                                  GLuint      buffer,
                                                                  GLintptr    offset,
                                                                  GLsizeiptr  size)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureBufferRangeEXT(texture_id,
                                                       target,
                                                       internalformat,
                                                       buffer,
                                                       offset,
                                                       size);

    ogl_texture_set_property(texture,
                             OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                            &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureImage1DEXT(ogl_texture   texture,
                                                              GLenum        target,
                                                              GLint         level,
                                                              GLenum        internalformat,
                                                              GLsizei       width,
                                                              GLint         border,
                                                              GLenum        format,
                                                              GLenum        type,
                                                              const GLvoid* pixels)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureImage1DEXT(texture_id,
                                                   target,
                                                   level,
                                                   internalformat,
                                                   width,
                                                   border,
                                                   format,
                                                   type,
                                                   pixels);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureImage2DEXT(ogl_texture   texture,
                                                              GLenum        target,
                                                              GLint         level,
                                                              GLenum        internalformat,
                                                              GLsizei       width,
                                                              GLsizei       height,
                                                              GLint         border,
                                                              GLenum        format,
                                                              GLenum        type,
                                                              const GLvoid* pixels)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureImage2DEXT(texture_id,
                                                   target,
                                                   level,
                                                   internalformat,
                                                   width,
                                                   height,
                                                   border,
                                                   format,
                                                   type,
                                                   pixels);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureImage3DEXT(ogl_texture  texture,
                                                              GLenum        target,
                                                              GLint         level,
                                                              GLenum        internalformat,
                                                              GLsizei       width,
                                                              GLsizei       height,
                                                              GLsizei       depth,
                                                              GLint         border,
                                                              GLenum        format,
                                                              GLenum        type,
                                                              const GLvoid* pixels)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureImage3DEXT(texture_id,
                                                   target,
                                                   level,
                                                   internalformat,
                                                   width,
                                                   height,
                                                   depth,
                                                   border,
                                                   format,
                                                   type,
                                                   pixels);

    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    &width);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    &height);
    ogl_texture_set_mipmap_property(texture,
                                    level,
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                    &depth);
    ogl_texture_set_property       (texture,
                                    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                   &internalformat);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterfEXT(ogl_texture texture,
                                                                 GLenum      target,
                                                                 GLenum      pname,
                                                                 GLfloat     param)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureParameterfEXT(texture_id,
                                                      target,
                                                      pname,
                                                      param);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterfvEXT(ogl_texture    texture,
                                                                  GLenum         target,
                                                                  GLenum         pname,
                                                                  const GLfloat* params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureParameterfvEXT(texture_id,
                                                       target,
                                                       pname,
                                                       params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameteriEXT(ogl_texture texture,
                                                                 GLenum      target,
                                                                 GLenum      pname,
                                                                 GLint       param)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureParameteriEXT(texture_id,
                                                      target,
                                                      pname,
                                                      param);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterIivEXT(ogl_texture  texture,
                                                                   GLenum       target,
                                                                   GLenum       pname,
                                                                   const GLint* params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureParameterIivEXT(texture_id,
                                                        target,
                                                        pname,
                                                        params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterIuivEXT(ogl_texture   texture,
                                                                    GLenum        target,
                                                                    GLenum        pname,
                                                                    const GLuint* params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureParameterIuivEXT(texture_id,
                                                         target,
                                                         pname,
                                                         params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterivEXT(ogl_texture  texture,
                                                                  GLenum       target,
                                                                  GLenum       pname,
                                                                  const GLint* params)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureParameterivEXT(texture_id,
                                                       target,
                                                       pname,
                                                       params);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureRenderbufferEXT(ogl_texture texture,
                                                                   GLenum      target,
                                                                   GLuint      renderbuffer)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureRenderbufferEXT(texture_id,
                                                        target,
                                                        renderbuffer);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage1DEXT(ogl_texture texture,
                                                                GLenum      target,
                                                                GLsizei     levels,
                                                                GLenum      internalformat,
                                                                GLsizei     width)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureStorage1DEXT(texture_id,
                                                     target,
                                                     levels,
                                                     internalformat,
                                                     width);

    _ogl_context_wrappers_update_mipmap_info(ogl_context_get_current_context(),
                                             target,
                                             texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage2DEXT(ogl_texture texture,
                                                                GLenum      target,
                                                                GLsizei     levels,
                                                                GLenum      internalformat,
                                                                GLsizei     width,
                                                                GLsizei     height)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureStorage2DEXT(texture_id,
                                                     target,
                                                     levels,
                                                     internalformat,
                                                     width,
                                                     height);

    _ogl_context_wrappers_update_mipmap_info(ogl_context_get_current_context(), target, texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage2DMultisampleEXT(ogl_texture texture,
                                                                           GLenum      target,
                                                                           GLsizei     levels,
                                                                           GLenum      internalformat,
                                                                           GLsizei     width,
                                                                           GLsizei     height,
                                                                           GLboolean   fixedsamplelocations)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureStorage2DMultisampleEXT(texture_id,
                                                                target,
                                                                levels,
                                                                internalformat,
                                                                width,
                                                                height,
                                                                fixedsamplelocations);

    _ogl_context_wrappers_update_mipmap_info(ogl_context_get_current_context(), target, texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage3DEXT(ogl_texture texture,
                                                                GLenum      target,
                                                                GLsizei     levels,
                                                                GLenum      internalformat,
                                                                GLsizei     width,
                                                                GLsizei     height,
                                                                GLsizei     depth)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureStorage3DEXT(texture_id,
                                                     target,
                                                     levels,
                                                     internalformat,
                                                     width,
                                                     height,
                                                     depth);

    _ogl_context_wrappers_update_mipmap_info(ogl_context_get_current_context(), target, texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage3DMultisampleEXT(ogl_texture texture,
                                                                           GLenum      target,
                                                                           GLsizei     levels,
                                                                           GLenum      internalformat,
                                                                           GLsizei     width,
                                                                           GLsizei     height,
                                                                           GLsizei     depth,
                                                                           GLboolean   fixedsamplelocations)
{
    GLuint texture_id = 0;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureStorage2DMultisampleEXT(texture_id,
                                                                target,
                                                                levels,
                                                                internalformat,
                                                                width,
                                                                height,
                                                                fixedsamplelocations);

    _ogl_context_wrappers_update_mipmap_info(ogl_context_get_current_context(), target, texture);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage1DEXT(ogl_texture   texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLsizei       width,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels)
{
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureSubImage1DEXT(texture_id,
                                                      target,
                                                      level,
                                                      xoffset,
                                                      width,
                                                      format,
                                                      type,
                                                      pixels);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage2DEXT(ogl_texture   texture,
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
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureSubImage2DEXT(texture_id,
                                                      target,
                                                      level,
                                                      xoffset,
                                                      yoffset,
                                                      width,
                                                      height,
                                                      format,
                                                      type,
                                                      pixels);
}

/* Please see header for specification */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage3DEXT(ogl_texture   texture,
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
    GLuint                  texture_id  = 0;
    ogl_context             context     = ogl_context_get_current_context();
    ogl_context_bo_bindings bo_bindings = NULL;

    ogl_context_get_property    (context,
                                 OGL_CONTEXT_PROPERTY_BO_BINDINGS,
                                &bo_bindings);
    ogl_context_bo_bindings_sync(bo_bindings,
                                 BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER);

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    _private_entrypoints_ptr->pGLTextureSubImage3DEXT(texture_id,
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
}

/* Please see header for specification */
PUBLIC void ogl_context_wrappers_set_private_functions(__in __maybenull ogl_context_gl_entrypoints_private* ptrs)
{
    _private_entrypoints_ptr = ptrs;
}
