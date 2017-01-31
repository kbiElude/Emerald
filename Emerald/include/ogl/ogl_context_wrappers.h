/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef OGL_CONTEXT_WRAPPERS_H
#define OGL_CONTEXT_WRAPPERS_H

#include "ogl/ogl_types.h"

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glActiveTexture(GLenum texture);

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
PUBLIC void APIENTRY ogl_context_wrappers_glBindFramebuffer(GLenum target,
                                                            GLuint fbo_id);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindMultiTextureEXT(GLenum texunit,
                                                                GLenum target,
                                                                GLuint texture);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindRenderbuffer(GLenum target,
                                                             GLuint renderbuffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSampler(GLuint unit,
                                                        GLuint sampler);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindSamplers(GLuint        first,
                                                         GLsizei       count,
                                                         const GLuint* samplers);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTexture(GLenum target,
                                                        GLuint texture);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindTextures(GLuint        first,
                                                         GLsizei       count,
                                                         const GLuint* textures);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindVertexArray(GLuint array);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glBindVertexBuffer(GLuint   bindingindex,
                                                             GLuint   buffer,
                                                             GLintptr offset,
                                                             GLsizei  stride);

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
PUBLIC void APIENTRY ogl_context_wrappers_glBlitFramebuffer(GLint      srcX0,
                                                            GLint      srcY0,
                                                            GLint      srcX1,
                                                            GLint      srcY1,
                                                            GLint      dstX0,
                                                            GLint      dstY0,
                                                            GLint      dstX1,
                                                            GLint      dstY1,
                                                            GLbitfield mask,
                                                            GLenum     filter);

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
PUBLIC GLenum APIENTRY ogl_context_wrappers_glCheckNamedFramebufferStatusEXT(GLuint framebuffer,
                                                                             GLenum target);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClear(GLbitfield mask);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClearBufferData(GLenum      target,
                                                            GLenum      internalformat,
                                                            GLenum      format,
                                                            GLenum      type,
                                                            const void* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClearBufferSubData(GLenum      target,
                                                               GLenum      internalformat,
                                                               GLintptr    offset,
                                                               GLsizeiptr  size,
                                                               GLenum      format,
                                                               GLenum      type,
                                                               const void* data);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClearColor(GLfloat red,
                                                       GLfloat green,
                                                       GLfloat blue,
                                                       GLfloat alpha);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClearDepth(GLdouble value);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glClearDepthf(GLfloat d);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha);

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
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage1DEXT(GLuint        texture,
                                                                           GLenum        target,
                                                                           GLint         level,
                                                                           GLint         xoffset,
                                                                           GLsizei       width,
                                                                           GLenum        format,
                                                                           GLsizei       imageSize,
                                                                           const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCompressedTextureSubImage2DEXT(GLuint        texture,
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
                                                                           const GLvoid* bits);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyBufferSubData(GLenum     readTarget,
                                                              GLenum     writeTarget,
                                                              GLintptr   readOffset,
                                                              GLintptr   writeOffset,
                                                              GLsizeiptr size);

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
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage1DEXT(GLuint  texture,
                                                                     GLenum  target,
                                                                     GLint   level,
                                                                     GLint   xoffset,
                                                                     GLint   x,
                                                                     GLint   y,
                                                                     GLsizei width);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage2DEXT(GLuint  texture,
                                                                     GLenum  target,
                                                                     GLint   level,
                                                                     GLint   xoffset,
                                                                     GLint   yoffset,
                                                                     GLint   x,
                                                                     GLint   y,
                                                                     GLsizei width,
                                                                     GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCopyTextureSubImage3DEXT(GLuint  texture,
                                                                     GLenum  target,
                                                                     GLint   level,
                                                                     GLint   xoffset,
                                                                     GLint   yoffset,
                                                                     GLint   zoffset,
                                                                     GLint   x,
                                                                     GLint   y,
                                                                     GLsizei width,
                                                                     GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glCullFace(GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteRenderbuffers(GLsizei       n,
                                                                const GLuint* renderbuffers);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteTextures(GLsizei       n,
                                                           const GLuint* textures);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteVertexArrays(GLsizei       n,
                                                               const GLuint* arrays);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthFunc(GLenum func);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthMask(GLboolean flag);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDepthRangeIndexed(GLuint   index,
                                                              GLdouble nearVal,
                                                              GLdouble farVal);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDisable(GLenum cap);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDisablei(GLenum cap,
                                                     GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDisableVertexAttribArray(GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchCompute(GLuint num_groups_x,
                                                            GLuint num_groups_y,
                                                            GLuint num_groups_z);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDispatchComputeIndirect(GLintptr indirect);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDeleteBuffers(GLsizei       n,
                                                          const GLuint* buffers);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArrays(GLenum  mode,
                                                       GLint   first,
                                                       GLsizei count);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawArraysIndirect(GLenum      mode,
                                                               const void* indirect);

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
PUBLIC void APIENTRY ogl_context_wrappers_glDrawBuffer(GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawBuffers(      GLsizei n,
                                                        const GLenum* bufs);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElements(GLenum        mode,
                                                         GLsizei       count,
                                                         GLenum        type,
                                                         const GLvoid* indices);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstancedBaseInstance(GLenum      mode,
                                                                              GLsizei     count,
                                                                              GLenum      type,
                                                                              const void* indices,
                                                                              GLsizei     instancecount,
                                                                              GLuint      baseinstance);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstancedBaseVertex(GLenum        mode,
                                                                            GLsizei       count,
                                                                            GLenum        type,
                                                                            const GLvoid *indices,
                                                                            GLsizei       primcount,
                                                                            GLint         basevertex);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawElementsInstancedBaseVertexBaseInstance(GLenum      mode,
                                                                                        GLsizei     count,
                                                                                        GLenum      type,
                                                                                        const void* indices,
                                                                                        GLsizei     instancecount,
                                                                                        GLint       basevertex,
                                                                                        GLuint      baseinstance);

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
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedbackInstanced(GLenum  mode,
                                                                           GLuint  id,
                                                                           GLsizei instancecount);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glDrawTransformFeedbackStreamInstanced(GLenum  mode,
                                                                                 GLuint  id,
                                                                                 GLuint  stream,
                                                                                 GLsizei instancecount);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glEnable(GLenum cap);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glEnablei(GLenum cap,
                                                    GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glEnableVertexAttribArray(GLuint index);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferParameteri(GLenum target,
                                                                  GLenum pname,
                                                                  GLint  param);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetFramebufferParameteriv(GLenum target,
                                                                      GLenum pname,
                                                                      GLint* params);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferReadBufferEXT(GLuint framebuffer,
                                                                     GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferRenderbuffer(GLenum target,
                                                                    GLenum attachment,
                                                                    GLenum renderbuffertarget,
                                                                    GLuint renderbuffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture(GLenum target,
                                                               GLenum attachment,
                                                               GLuint texture,
                                                               GLint  level);
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture1D(GLenum target,
                                                                 GLenum attachment,
                                                                 GLenum textarget,
                                                                 GLuint texture,
                                                                 GLint  level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture2D(GLenum target,
                                                                 GLenum attachment,
                                                                 GLenum textarget,
                                                                 GLuint texture,
                                                                 GLint  level);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTexture3D(GLenum target,
                                                                 GLenum attachment,
                                                                 GLenum textarget,
                                                                 GLuint texture,
                                                                 GLint  level,
                                                                 GLint  layer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFramebufferTextureLayer(GLenum target,
                                                                    GLenum attachment,
                                                                    GLuint texture,
                                                                    GLint  level,
                                                                    GLint  layer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glFrontFace(GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGenerateMipmap(GLenum target);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGenTextures(GLsizei n,
                                                        GLuint* textures);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGenVertexArrays(GLsizei n,
                                                            GLuint* arrays);

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
PUBLIC void APIENTRY ogl_context_wrappers_glGetInternalformativ(GLenum  target,
                                                                GLenum  internalformat,
                                                                GLenum  pname,
                                                                GLsizei bufSize,
                                                                GLint*  params);

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
PUBLIC void APIENTRY ogl_context_wrappers_glGenerateTextureMipmapEXT(GLuint texture,
                                                                     GLenum target);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTexImage(GLenum  target,
                                                                  GLint   level,
                                                                  GLvoid* img);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetCompressedTextureImageEXT(GLuint  texture,
                                                                         GLenum  target,
                                                                         GLint   lod,
                                                                         GLvoid* img);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glGetRenderbufferParameteriv(GLenum target,
                                                                       GLenum pname,
                                                                       GLint* params);

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
PUBLIC void APIENTRY ogl_context_wrappers_glGetTextureImageEXT(GLuint  texture,
                                                               GLenum  target,
                                                               GLint   level,
                                                               GLenum  format,
                                                               GLenum  type,
                                                               GLvoid* pixels);

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

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glInvalidateFramebuffer(GLenum        target,
                                                                  GLsizei       numAttachments,
                                                                  const GLenum* attachments);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glInvalidateSubFramebuffer(GLenum        target,
                                                                     GLsizei       numAttachments,
                                                                     const GLenum* attachments,
                                                                     GLint         x,
                                                                     GLint         y,
                                                                     GLsizei       width,
                                                                     GLsizei       height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glLineWidth(GLfloat width);

/** TODO */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBuffer(GLenum target,
                                                         GLenum access);

/** TODO */
PUBLIC GLvoid* APIENTRY ogl_context_wrappers_glMapBufferRange(GLenum     target,
                                                              GLintptr   offset,
                                                              GLsizeiptr length,
                                                              GLbitfield access);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glMemoryBarrier(GLbitfield barriers);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glMinSampleShading(GLfloat value);

/** TODO */
PUBLIC GLvoid APIENTRY ogl_context_wrappers_glMultiDrawArrays(GLenum         mode,
                                                              const GLint*   first,
                                                              const GLsizei* count,
                                                              GLsizei        primcount);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glMultiDrawArraysIndirect(GLenum      mode,
                                                                    const void* indirect,
                                                                    GLsizei     drawcount,
                                                                    GLsizei     stride);

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
PUBLIC void APIENTRY ogl_context_wrappers_glMultiDrawElementsIndirect(GLenum      mode,
                                                                      GLenum      type,
                                                                      const void* indirect,
                                                                      GLsizei     drawcount,
                                                                      GLsizei     stride);

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
PUBLIC void APIENTRY ogl_context_wrappers_glPatchParameteri(GLenum pname,
                                                            GLint  value);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glPolygonMode(GLenum face,
                                                        GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glPolygonOffset(GLfloat factor,
                                                          GLfloat units);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glReadBuffer(GLenum mode);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glReadPixels(GLint   x,
                                                       GLint   y,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum  format,
                                                       GLenum  type,
                                                       GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glRenderbufferStorage(GLenum  target,
                                                                GLenum  internalformat,
                                                                GLsizei width,
                                                                GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glRenderbufferStorageMultisample(GLenum  target,
                                                                           GLsizei samples,
                                                                           GLenum  internalformat,
                                                                           GLsizei width,
                                                                           GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glResumeTransformFeedback(void);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glScissor(GLint   x,
                                                    GLint   y,
                                                    GLsizei width,
                                                    GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glScissorIndexedv(GLuint       index,
                                                            const GLint *v);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glShaderStorageBlockBinding(GLuint program,
                                                                      GLuint shaderStorageBlockIndex,
                                                                      GLuint shaderStorageBlockBinding);

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
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferEXT(GLuint texture,
                                                             GLenum target,
                                                             GLenum internalformat,
                                                             GLuint buffer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureBufferRangeEXT(GLuint     texture,
                                                                  GLenum     target,
                                                                  GLenum     internalformat,
                                                                  GLuint     buffer,
                                                                  GLintptr   offset,
                                                                  GLsizeiptr size);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage1DEXT(GLuint        texture,
                                                                 GLenum        target,
                                                                 GLint         level,
                                                                 GLint         xoffset,
                                                                 GLsizei       width,
                                                                 GLenum        format,
                                                                 GLenum        type,
                                                                 const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glTextureSubImage2DEXT(GLuint        texture,
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
                                                                 const GLvoid* pixels);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glUniformBlockBinding(GLuint program,
                                                                GLuint uniformBlockIndex,
                                                                GLuint uniformBlockBinding);

/** TODO */
PUBLIC GLboolean APIENTRY ogl_context_wrappers_glUnmapBuffer(GLenum target);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glUseProgram(GLuint program);

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
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribBinding(GLuint attribindex,
                                                                GLuint bindingindex);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribFormat(GLuint    attribindex,
                                                               GLint     size,
                                                               GLenum    type,
                                                               GLboolean normalized,
                                                               GLuint    relativeoffset);
/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribIFormat(GLuint attribindex,
                                                                GLint  size,
                                                                GLenum type,
                                                                GLuint relativeoffset);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribIPointer(GLuint        index,
                                                                 GLint         size,
                                                                 GLenum        type,
                                                                 GLsizei       stride,
                                                                 const GLvoid* pointer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribLFormat(GLuint attribindex,
                                                                GLint  size,
                                                                GLenum type,
                                                                GLuint relativeoffset);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexAttribPointer(GLuint        index,
                                                                GLint         size,
                                                                GLenum        type,
                                                                GLboolean     normalized,
                                                                GLsizei       stride,
                                                                const GLvoid* pointer);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glVertexBindingDivisor(GLuint bindingindex,
                                                                 GLuint divisor);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glViewport(GLint   x,
                                                     GLint   y,
                                                     GLsizei width,
                                                     GLsizei height);

/** TODO */
PUBLIC void APIENTRY ogl_context_wrappers_glViewportIndexedfv(GLuint         index,
                                                              const GLfloat *v);


/** TODO */
PUBLIC void ogl_context_wrappers_set_private_functions(ogl_context_gl_entrypoints_private*);

#endif /* OGL_CONTEXT_WRAPPERS_H */
