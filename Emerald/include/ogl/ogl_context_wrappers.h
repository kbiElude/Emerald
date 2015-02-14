/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef OGL_CONTEXT_WRAPPERS_H
#define OGL_CONTEXT_WRAPPERS_H

#include "ogl/ogl_types.h"

/*** OTHER CONTEXT-WIDE STATE */
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendEquation(GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendEquationSeparate(GLenum modeRGB,
                                                                  GLenum modeAlpha);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendFunc(GLenum sfactor,
                                                      GLenum dfactor);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBlendFuncSeparate(GLenum srcRGB,
                                                              GLenum dstRGB,
                                                              GLenum srcAlpha,
                                                              GLenum dstAlpha);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClear(GLbitfield mask);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClearColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCullFace(GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthFunc(GLenum func);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthMask(GLboolean flag);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDisable(GLenum cap);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDisablei(GLenum cap,
                                                     GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glEnable(GLenum cap);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glEnablei(GLenum cap,
                                                    GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glScissor(GLint   x,
                                                    GLint   y,
                                                    GLsizei width,
                                                    GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glViewport(GLint   x,
                                                     GLint   y,
                                                     GLsizei width,
                                                     GLsizei height);

/*** VERTEX ARRAY OBJECTS ***/
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindVertexArray(GLuint array);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteVertexArrays(GLsizei       n,
                                                               const GLuint* arrays);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDisableVertexAttribArray(GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glEnableVertexAttribArray(GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribdv(GLuint    index,
                                                              GLenum    pname,
                                                              GLdouble* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribfv(GLuint   index,
                                                              GLenum   pname,
                                                              GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribPointerv(GLuint   index,
                                                                    GLenum   pname,
                                                                    GLvoid** pointer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetVertexAttribIuiv(GLuint  index,
                                                                GLenum  pname,
                                                                GLuint* params);

/*** PROGRAM OBJECTS ***/
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchCompute(GLuint num_groups_x,
                                                            GLuint num_groups_y,
                                                            GLuint num_groups_z);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchComputeIndirect(GLintptr indirect);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glUseProgram(GLuint program);

/*** SAMPLERS ***/
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSampler(GLuint unit,
                                                        GLuint sampler);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSamplers(GLuint        first,
                                                         GLsizei       count,
                                                         const GLuint* samplers);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterfv(GLuint   sampler,
                                                                  GLenum   pname,
                                                                  GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterIiv(GLuint sampler,
                                                                   GLenum pname,
                                                                   GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameterIuiv(GLuint  sampler,
                                                                    GLenum  pname,
                                                                    GLuint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetSamplerParameteriv(GLuint sampler,
                                                                  GLenum pname,
                                                                  GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterf(GLuint sampler,
                                                             GLenum  pname,
                                                             GLfloat param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterfv(GLuint         sampler,
                                                               GLenum         pname,
                                                               const GLfloat* param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameteri(GLuint sampler,
                                                              GLenum pname,
                                                              GLint  param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameteriv(GLuint       sampler,
                                                               GLenum       pname,
                                                               const GLint* param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterIiv(GLuint       sampler,
                                                                GLenum       pname,
                                                                const GLint* param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glSamplerParameterIuiv(GLuint        sampler,
                                                                 GLenum        pname,
                                                                 const GLuint* param);

/**** BUFFERS ****/
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBeginTransformFeedback(GLenum primitiveMode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffer(GLenum target,
                                                       GLuint buffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBufferRange(GLenum target,
                                                            GLuint     index,
                                                            GLuint     buffer,
                                                            GLintptr   offset,
                                                            GLsizeiptr size);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffersBase(GLenum        target,
                                                            GLuint        first,
                                                            GLsizei       count,
                                                            const GLuint* buffers);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindBuffersRange(GLenum            target,
                                                             GLuint            first,
                                                             GLsizei           count,
                                                             const GLuint*     buffers,
                                                             const GLintptr*   offsets,
                                                             const GLsizeiptr* sizes);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferData(GLenum        target,
                                                       GLsizeiptr    size,
                                                       const GLvoid* data,
                                                       GLenum        usage);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferStorage(GLenum        target,
                                                          GLsizeiptr    size,
                                                          const GLvoid* data,
                                                          GLbitfield    flags);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBufferSubData(GLenum        target,
                                                          GLintptr      offset,
                                                          GLsizeiptr    size,
                                                          const GLvoid* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteBuffers(GLsizei       n,
                                                          const GLuint* buffers);

PUBLIC void APIENTRY ogl_context_wrappers_glDrawArrays(GLenum  mode,
                                                       GLint   first,
                                                       GLsizei count);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysInstanced(GLenum  mode,
                                                                GLint   first,
                                                                GLsizei count,
                                                                GLsizei primcount);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysInstancedBaseInstance(GLenum  mode,
                                                                            GLint   first,
                                                                            GLsizei count,
                                                                            GLsizei primcount,
                                                                            GLuint  baseinstance);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElements(GLenum        mode,
                                                         GLsizei       count,
                                                         GLenum        type,
                                                         const GLvoid* indices);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstanced(GLenum        mode,
                                                                  GLsizei       count,
                                                                  GLenum        type,
                                                                  const GLvoid* indices,
                                                                  GLsizei       primcount);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawRangeElements(GLenum        mode,
                                                              GLuint        start,
                                                              GLuint        end,
                                                              GLsizei       count,
                                                              GLenum        type,
                                                              const GLvoid* indices);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedback(GLenum mode,
                                                                  GLuint id);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetActiveAtomicCounterBufferiv(GLuint program,
                                                                           GLuint bufferIndex,
                                                                           GLenum pname,
                                                                           GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBooleani_v(GLenum target,
                                                          GLuint     index,
                                                          GLboolean* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBooleanv(GLenum     pname,
                                                        GLboolean* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferParameteriv(GLenum target,
                                                                 GLenum pname,
                                                                 GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferParameteri64v(GLenum   target,
                                                                   GLenum   pname,
                                                                   GLint64* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferPointerv(GLenum   target,
                                                              GLenum   pname,
                                                              GLvoid** params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetBufferSubData(GLenum     target,
                                                             GLintptr   offset,
                                                             GLsizeiptr size,
                                                             GLvoid*    data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetDoublev(GLenum    pname,
                                                       GLdouble* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetFloatv(GLenum   pname,
                                                      GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetInteger64i_v(GLenum   target,
                                                            GLuint   index,
                                                            GLint64* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetIntegeri_v(GLenum target,
                                                          GLuint index,
                                                          GLint* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetIntegerv(GLenum pname,
                                                        GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetNamedBufferParameterivEXT(GLuint buffer,
                                                                         GLenum pname,
                                                                         GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetNamedBufferPointervEXT(GLuint buffer,
                                                                      GLenum pname,
                                                                      void** params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetNamedBufferSubDataEXT(GLuint     buffer,
                                                                     GLintptr   offset,
                                                                     GLsizeiptr size,
                                                                     void*      data);

/** TODO */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBuffer(GLenum target,
                                                         GLenum access);

/** TODO */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBufferRange(GLenum     target,
                                                              GLintptr   offset,
                                                              GLsizeiptr length,
                                                              GLbitfield access);

/** TODO */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawArrays(GLenum         mode,
                                                              const GLint*   first,
                                                              const GLsizei* count,
                                                              GLsizei        primcount);

/** TODO */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawElements(GLenum               mode,
                                                                const GLsizei*       count,
                                                                GLenum               type,
                                                                const GLvoid* const* indices,
                                                                GLsizei              primcount);

/** TODO */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawElementsBaseVertex(GLenum               mode,
                                                                          const GLsizei*       count,
                                                                          GLenum               type,
                                                                          const GLvoid* const* indices,
                                                                          GLsizei              drawcount,
                                                                          const GLint*         basevertex);

/** TODO */
PUBLIC void* APIENTRY ogl_context_wrappers_glMapNamedBufferEXT(GLuint buffer,
                                                               GLenum access);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferDataEXT(GLuint      buffer,
                                                               GLsizeiptr  size,
                                                               const void* data,
                                                               GLenum      usage);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferStorageEXT(GLuint        buffer,
                                                                  GLsizeiptr    size,
                                                                  const GLvoid* data,
                                                                  GLbitfield    flags);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedBufferSubDataEXT(GLuint      buffer,
                                                                  GLintptr    offset,
                                                                  GLsizeiptr  size,
                                                                  const void* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedCopyBufferSubDataEXT(GLuint     readBuffer,
                                                                      GLuint     writeBuffer,
                                                                      GLintptr   readOffset,
                                                                      GLintptr   writeOffset,
                                                                      GLsizeiptr size);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glReadPixels(GLint   x,
                                                       GLint   y,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum  format,
                                                       GLenum  type,
                                                       GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glResumeTransformFeedback(void);

/** TODO */
PUBLIC GLboolean APIENTRY ogl_context_wrappers_glUnmapBuffer(GLenum target);

/** TODO */
PUBLIC GLboolean APIENTRY ogl_context_wrappers_glUnmapNamedBufferEXT(GLuint buffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexArrayVertexAttribIOffsetEXT(GLuint   vaobj,
                                                                              GLuint   buffer,
                                                                              GLuint   index,
                                                                              GLint    size,
                                                                              GLenum   type,
                                                                              GLsizei  stride,
                                                                              GLintptr offset);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexArrayVertexAttribOffsetEXT(GLuint    vaobj,
                                                                             GLuint    buffer,
                                                                             GLuint    index,
                                                                             GLint     size,
                                                                             GLenum    type,
                                                                             GLboolean normalized,
                                                                             GLsizei   stride,
                                                                             GLintptr  offset);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribIPointer(GLuint        index,
                                                                 GLint         size,
                                                                 GLenum        type,
                                                                 GLsizei       stride,
                                                                 const GLvoid* pointer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribPointer(GLuint index,
                                                                GLint         size,
                                                                GLenum        type,
                                                                GLboolean     normalized,
                                                                GLsizei       stride,
                                                                const GLvoid* pointer);

/**** TEXTURES ****/

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glActiveTexture(GLenum texture);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindImageTextureEXT(GLuint      index,
                                                                ogl_texture texture,
                                                                GLint       level,
                                                                GLboolean   layered,
                                                                GLint       layer,
                                                                GLenum      access,
                                                                GLint       format);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindMultiTextureEXT(GLenum      texunit,
                                                                GLenum      target,
                                                                ogl_texture texture);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTexture(GLenum      target,
                                                        ogl_texture texture);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTextures(GLuint       first,
                                                         GLsizei      count,
                                                         ogl_texture* textures);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexImage3D(GLenum        target,
                                                                 GLint         level,
                                                                 GLenum        internalformat,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLsizei       depth,
                                                                 GLint         border,
                                                                 GLsizei       imageSize,
                                                                 const GLvoid* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexImage2D(GLenum        target,
                                                                 GLint         level,
                                                                 GLenum        internalformat,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLint         border,
                                                                 GLsizei       imageSize,
                                                                 const GLvoid* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexImage1D(GLenum        target,
                                                                 GLint         level,
                                                                 GLenum        internalformat,
                                                                 GLsizei       width,
                                                                 GLint         border,
                                                                 GLsizei       imageSize,
                                                                 const GLvoid* data);

/** TODO */
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
                                                                    const GLvoid* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexSubImage2D(GLenum        target,
                                                                    GLint         level,
                                                                    GLint         xoffset,
                                                                    GLint         yoffset,
                                                                    GLsizei       width,
                                                                    GLsizei       height,
                                                                    GLenum        format,
                                                                    GLsizei       imageSize,
                                                                    const GLvoid* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTexSubImage1D(GLenum        target,
                                                                    GLint         level,
                                                                    GLint         xoffset,
                                                                    GLsizei       width,
                                                                    GLenum        format,
                                                                    GLsizei       imageSize,
                                                                    const GLvoid* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage1DEXT(ogl_texture   texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLsizei       width,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage2DEXT(ogl_texture   texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLint         yoffset,
                                                                           GLsizei       width,
                                                                           GLsizei       height,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits);

/** TODO */
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
                                                                           const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureImage1DEXT(ogl_texture   texture,
                                                                        GLenum        target,
                                                                        GLint         level,
                                                                        GLenum        internalformat,
                                                                        GLsizei       width,
                                                                        GLint         border,
                                                                        GLsizei       imageSize,
                                                                        const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureImage2DEXT(ogl_texture  texture,
                                                                        GLenum        target,
                                                                        GLint         level,
                                                                        GLenum        internalformat,
                                                                        GLsizei       width,
                                                                        GLsizei       height,
                                                                        GLint         border,
                                                                        GLsizei       imageSize,
                                                                        const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureImage3DEXT(ogl_texture   texture,
                                                                        GLenum        target,
                                                                        GLint         level,
                                                                        GLenum        internalformat,
                                                                        GLsizei       width,
                                                                        GLsizei       height,
                                                                        GLsizei       depth,
                                                                        GLint         border,
                                                                        GLsizei       imageSize,
                                                                        const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexImage1D(GLenum  target,
                                                           GLint   level,
                                                           GLenum  internalformat,
                                                           GLint   x,
                                                           GLint   y,
                                                           GLsizei width,
                                                           GLint   border);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexImage2D(GLenum target,
                                                           GLint   level,
                                                           GLenum  internalformat,
                                                           GLint   x,
                                                           GLint   y,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLint   border);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexSubImage1D(GLenum  target,
                                                              GLint   level,
                                                              GLint   xoffset,
                                                              GLint   x,
                                                              GLint   y,
                                                              GLsizei width);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexSubImage2D(GLenum  target,
                                                              GLint   level,
                                                              GLint   xoffset,
                                                              GLint   yoffset,
                                                              GLint   x,
                                                              GLint   y,
                                                              GLsizei width,
                                                              GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTexSubImage3D(GLenum target,
                                                              GLint   level,
                                                              GLint   xoffset,
                                                              GLint   yoffset,
                                                              GLint   zoffset,
                                                              GLint   x,
                                                              GLint   y,
                                                              GLsizei width,
                                                              GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureImage1DEXT(ogl_texture texture,
                                                                  GLenum      target,
                                                                  GLint       level,
                                                                  GLenum      internalformat,
                                                                  GLint       x,
                                                                  GLint       y,
                                                                  GLsizei     width,
                                                                  GLint       border);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureImage2DEXT(ogl_texture texture,
                                                                  GLenum      target,
                                                                  GLint       level,
                                                                  GLenum      internalformat,
                                                                  GLint       x,
                                                                  GLint       y,
                                                                  GLsizei     width,
                                                                  GLsizei     height,
                                                                  GLint       border);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage1DEXT(ogl_texture texture,
                                                                     GLenum      target,
                                                                     GLint       level,
                                                                     GLint       xoffset,
                                                                     GLint       x,
                                                                     GLint       y,
                                                                     GLsizei     width);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage2DEXT(ogl_texture texture,
                                                                     GLenum      target,
                                                                     GLint       level,
                                                                     GLint       xoffset,
                                                                     GLint       yoffset,
                                                                     GLint       x,
                                                                     GLint       y,
                                                                     GLsizei     width,
                                                                     GLsizei     height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage3DEXT(ogl_texture texture,
                                                                     GLenum      target,
                                                                     GLint       level,
                                                                     GLint       xoffset,
                                                                     GLint       yoffset,
                                                                     GLint       zoffset,
                                                                     GLint       x,
                                                                     GLint       y,
                                                                     GLsizei     width,
                                                                     GLsizei     height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture(GLenum      target,
                                                               GLenum      attachment,
                                                               ogl_texture texture,
                                                               GLint       level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture1D(GLenum      target,
                                                                 GLenum      attachment,
                                                                 GLenum      textarget,
                                                                 ogl_texture texture,
                                                                 GLint       level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture2D(GLenum      target,
                                                                 GLenum      attachment,
                                                                 GLenum      textarget,
                                                                 ogl_texture texture,
                                                                 GLint       level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture3D(GLenum      target,
                                                                 GLenum      attachment,
                                                                 GLenum      textarget,
                                                                 ogl_texture texture,
                                                                 GLint       level,
                                                                 GLint       layer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTextureLayer(GLenum      target,
                                                                    GLenum      attachment,
                                                                    ogl_texture texture,
                                                                    GLint       level,
                                                                    GLint       layer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGenerateTextureMipmapEXT(ogl_texture texture,
                                                                     GLenum      target);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTexImage(GLenum  target,
                                                                  GLint   level,
                                                                  GLvoid* img);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTextureImageEXT(ogl_texture texture,
                                                                         GLenum      target,
                                                                         GLint       lod,
                                                                         GLvoid*     img);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexImage(GLenum  target,
                                                        GLint   level,
                                                        GLenum  format,
                                                        GLenum  type,
                                                        GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexLevelParameterfv(GLenum   target,
                                                                   GLint    level,
                                                                   GLenum   pname,
                                                                   GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexLevelParameteriv(GLenum target,
                                                                   GLint  level,
                                                                   GLenum pname,
                                                                   GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterfv(GLenum   target,
                                                              GLenum   pname,
                                                              GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterIiv(GLenum target,
                                                               GLenum pname,
                                                               GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTexParameterIuiv(GLenum  target,
                                                                GLenum  pname,
                                                                GLuint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureImageEXT(ogl_texture texture,
                                                               GLenum      target,
                                                               GLint       level,
                                                               GLenum      format,
                                                               GLenum      type,
                                                               GLvoid*     pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureLevelParameterfvEXT(ogl_texture texture,
                                                                          GLenum      target,
                                                                          GLint       level,
                                                                          GLenum      pname,
                                                                          GLfloat*    params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureLevelParameterivEXT(ogl_texture texture,
                                                                          GLenum      target,
                                                                          GLint       level,
                                                                          GLenum      pname,
                                                                          GLint*      params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureParameterIiv(ogl_texture texture,
                                                                   GLenum      target,
                                                                   GLenum      pname,
                                                                   GLint*      params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureParameterIuiv(ogl_texture texture,
                                                                    GLenum      target,
                                                                    GLenum      pname,
                                                                    GLuint*     params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTexture1DEXT(GLuint      framebuffer,
                                                                         GLenum      attachment,
                                                                         GLenum      textarget,
                                                                         ogl_texture texture,
                                                                         GLint       level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTexture2DEXT(GLuint      framebuffer,
                                                                         GLenum      attachment,
                                                                         GLenum      textarget,
                                                                         ogl_texture texture,
                                                                         GLint       level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTexture3DEXT(GLuint      framebuffer,
                                                                         GLenum      attachment,
                                                                         GLenum      textarget,
                                                                         ogl_texture texture,
                                                                         GLint       level,
                                                                         GLint       zoffset);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTextureEXT(GLuint      framebuffer,
                                                                       GLenum      attachment,
                                                                       ogl_texture texture,
                                                                       GLint       level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTextureFaceEXT(GLuint      framebuffer,
                                                                           GLenum      attachment,
                                                                           ogl_texture texture,
                                                                           GLint       level,
                                                                           GLenum      face);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glNamedFramebufferTextureLayerEXT(GLuint      framebuffer,
                                                                            GLenum      attachment,
                                                                            ogl_texture texture,
                                                                            GLint       level,
                                                                            GLint       layer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexBuffer(GLenum target,
                                                      GLenum internalformat,
                                                      GLuint buffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexBufferRange(GLenum     target,
                                                           GLenum     internalformat,
                                                           GLuint     buffer,
                                                           GLintptr   offset,
                                                           GLsizeiptr size);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexImage1D(GLenum        target,
                                                       GLint         level,
                                                       GLint         internalformat,
                                                       GLsizei       width,
                                                       GLint         border,
                                                       GLenum        format,
                                                       GLenum        type,
                                                       const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexImage2D(GLenum        target,
                                                       GLint         level,
                                                       GLint         internalformat,
                                                       GLsizei       width,
                                                       GLsizei       height,
                                                       GLint         border,
                                                       GLenum        format,
                                                       GLenum        type,
                                                       const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexImage3D(GLenum        target,
                                                       GLint         level,
                                                       GLint         internalformat,
                                                       GLsizei       width,
                                                       GLsizei       height,
                                                       GLsizei       depth,
                                                       GLint         border,
                                                       GLenum        format,
                                                       GLenum        type,
                                                       const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterf(GLenum  target,
                                                          GLenum  pname,
                                                          GLfloat param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterfv(GLenum         target,
                                                           GLenum         pname,
                                                           const GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameteri(GLenum target,
                                                          GLenum pname,
                                                          GLint  param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameteriv(GLenum       target,
                                                           GLenum       pname,
                                                           const GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterIiv(GLenum       target,
                                                            GLenum       pname,
                                                            const GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexParameterIuiv(GLenum        target,
                                                             GLenum        pname,
                                                             const GLuint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage1D(GLenum  target,
                                                         GLsizei levels,
                                                         GLenum  internalformat,
                                                         GLsizei width);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage2D(GLenum  target,
                                                         GLsizei levels,
                                                         GLenum  internalformat,
                                                         GLsizei width,
                                                         GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage2DMultisample(GLenum    target,
                                                                    GLsizei   levels,
                                                                    GLenum    internalformat,
                                                                    GLsizei   width,
                                                                    GLsizei   height,
                                                                    GLboolean fixedsamplelocations);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage3D(GLenum  target,
                                                         GLsizei levels,
                                                         GLenum  internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexStorage3DMultisample(GLenum    target,
                                                                    GLsizei   levels,
                                                                    GLenum    internalformat,
                                                                    GLsizei   width,
                                                                    GLsizei   height,
                                                                    GLsizei   depth,
                                                                    GLboolean fixedsamplelocations);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexSubImage1D(GLenum        target,
                                                          GLint         level,
                                                          GLint         xoffset,
                                                          GLsizei       width,
                                                          GLenum        format,
                                                          GLenum        type,
                                                          const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTexSubImage2D(GLenum        target,
                                                          GLint         level,
                                                          GLint         xoffset,
                                                          GLint         yoffset,
                                                          GLsizei       width,
                                                          GLsizei       height,
                                                          GLenum        format,
                                                          GLenum        type,
                                                          const GLvoid* pixels);

/** TODO */
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
                                                          const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferEXT(ogl_texture texture,
                                                             GLenum      target,
                                                             GLenum      internalformat,
                                                             GLuint      buffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferRangeEXT(ogl_texture texture,
                                                                  GLenum      target,
                                                                  GLenum      internalformat,
                                                                  GLuint      buffer,
                                                                  GLintptr    offset,
                                                                  GLsizeiptr  size);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureImage1DEXT(ogl_texture  texture,
                                                              GLenum        target,
                                                              GLint         level,
                                                              GLenum        internalformat,
                                                              GLsizei       width,
                                                              GLint         border,
                                                              GLenum        format,
                                                              GLenum        type,
                                                              const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureImage2DEXT(ogl_texture   texture,
                                                              GLenum        target,
                                                              GLint         level,
                                                              GLenum        internalformat,
                                                              GLsizei       width,
                                                              GLsizei       height,
                                                              GLint         border,
                                                              GLenum        format,
                                                              GLenum        type,
                                                              const GLvoid* pixels);

/** TODO */
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
                                                              const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterfEXT(ogl_texture texture,
                                                                 GLenum      target,
                                                                 GLenum      pname,
                                                                 GLfloat     param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterfvEXT(ogl_texture   texture,
                                                                  GLenum         target,
                                                                  GLenum         pname,
                                                                  const GLfloat* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameteriEXT(ogl_texture texture,
                                                                 GLenum      target,
                                                                 GLenum      pname,
                                                                 GLint       param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterIivEXT(ogl_texture  texture,
                                                                   GLenum       target,
                                                                   GLenum       pname,
                                                                   const GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterIuivEXT(ogl_texture   texture,
                                                                    GLenum        target,
                                                                    GLenum        pname,
                                                                    const GLuint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureParameterivEXT(ogl_texture  texture,
                                                                  GLenum       target,
                                                                  GLenum       pname,
                                                                  const GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureRenderbufferEXT(ogl_texture texture,
                                                                   GLenum      target,
                                                                   GLuint      renderbuffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage1DEXT(ogl_texture texture,
                                                                GLenum      target,
                                                                GLsizei     levels,
                                                                GLenum      internalformat,
                                                                GLsizei     width);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage2DEXT(ogl_texture texture,
                                                                GLenum      target,
                                                                GLsizei     levels,
                                                                GLenum      internalformat,
                                                                GLsizei     width,
                                                                GLsizei     height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage2DMultisampleEXT(ogl_texture texture,
                                                                           GLenum      target,
                                                                           GLsizei     levels,
                                                                           GLenum      internalformat,
                                                                           GLsizei     width,
                                                                           GLsizei     height,
                                                                           GLboolean   fixedsamplelocations);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage3DEXT(ogl_texture texture,
                                                                GLenum      target,
                                                                GLsizei     levels,
                                                                GLenum      internalformat,
                                                                GLsizei     width,
                                                                GLsizei     height,
                                                                GLsizei     depth);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureStorage3DMultisampleEXT(ogl_texture texture,
                                                                           GLenum      target,
                                                                           GLsizei     levels,
                                                                           GLenum      internalformat,
                                                                           GLsizei     width,
                                                                           GLsizei     height,
                                                                           GLsizei     depth,
                                                                           GLboolean   fixedsamplelocations);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage1DEXT(ogl_texture   texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLsizei       width,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage2DEXT(ogl_texture  texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLint         yoffset,
                                                                 GLsizei       width,
                                                                 GLsizei       height,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels);

/** TODO */
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
                                                                 const GLvoid* pixels);

/** TODO */
PUBLIC void ogl_context_wrappers_set_private_functions(__in __maybenull ogl_context_gl_entrypoints_private*);

#endif /* OGL_CONTEXT_WRAPPERS_H */
