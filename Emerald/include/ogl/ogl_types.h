/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef OGL_TYPES_H
#define OGL_TYPES_H

#include "ogl/gl3.h"
#include "ogl/glext.h"
#include "ral/ral_types.h"
#include "system/system_time.h"
#include "system/system_types.h"

#ifdef __linux
    #include <GL/glx.h>
#endif

#ifdef _WIN32
    #include "ogl/wglext.h"
#endif

typedef enum
{
    OGL_TEXTURE_DATA_FORMAT_DEPTH_STENCIL = GL_DEPTH_STENCIL,
    OGL_TEXTURE_DATA_FORMAT_RED           = GL_RED,
    OGL_TEXTURE_DATA_FORMAT_RED_INTEGER   = GL_RED_INTEGER,
    OGL_TEXTURE_DATA_FORMAT_RG            = GL_RG,
    OGL_TEXTURE_DATA_FORMAT_RG_INTEGER    = GL_RG_INTEGER,
    OGL_TEXTURE_DATA_FORMAT_RGB           = GL_RGB,
    OGL_TEXTURE_DATA_FORMAT_RGB_INTEGER   = GL_RGB_INTEGER,
    OGL_TEXTURE_DATA_FORMAT_RGBA          = GL_RGBA,
    OGL_TEXTURE_DATA_FORMAT_RGBA_INTEGER  = GL_RGBA_INTEGER,
    OGL_TEXTURE_DATA_FORMAT_RGBE          = GL_RGB, /* as per spec */

    OGL_TEXTURE_DATA_FORMAT_UNDEFINED = GL_NONE
} ogl_texture_data_format;

/* Vertex array object handles */
DECLARE_HANDLE(ogl_context_vaos);
DECLARE_HANDLE(ogl_vao);

typedef void (APIENTRY *PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC)      (GLuint vaobj, GLuint index);
typedef void (APIENTRY *PFNGLENABLEVERTEXARRAYATTRIBEXTPROC)       (GLuint vaobj, GLuint index);
typedef void (APIENTRY *PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC)(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride,       GLintptr offset);
typedef void (APIENTRY *PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC) (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset);


/* ES entry-points. */
typedef struct
{
    PFNGLACTIVESHADERPROGRAMPROC                 pGLActiveShaderProgram;
    PFNGLACTIVETEXTUREPROC                       pGLActiveTexture;
    PFNGLATTACHSHADERPROC                        pGLAttachShader;
    PFNGLBEGINQUERYPROC                          pGLBeginQuery;
    PFNGLBEGINTRANSFORMFEEDBACKPROC              pGLBeginTransformFeedback;
    PFNGLBINDATTRIBLOCATIONPROC                  pGLBindAttribLocation;
    PFNGLBINDBUFFERPROC                          pGLBindBuffer;
    PFNGLBINDBUFFERBASEPROC                      pGLBindBufferBase;
    PFNGLBINDBUFFERRANGEPROC                     pGLBindBufferRange;
    PFNGLBINDFRAMEBUFFERPROC                     pGLBindFramebuffer;
    PFNGLBINDIMAGETEXTUREPROC                    pGLBindImageTexture;
    PFNGLBINDPROGRAMPIPELINEPROC                 pGLBindProgramPipeline;
    PFNGLBINDRENDERBUFFERPROC                    pGLBindRenderbuffer;
    PFNGLBINDSAMPLERPROC                         pGLBindSampler;
    PFNGLBINDTEXTUREPROC                         pGLBindTexture;
    PFNGLBINDTRANSFORMFEEDBACKPROC               pGLBindTransformFeedback;
    PFNGLBINDVERTEXARRAYPROC                     pGLBindVertexArray;
    PFNGLBINDVERTEXBUFFERPROC                    pGLBindVertexBuffer;
    PFNGLBLENDCOLORPROC                          pGLBlendColor;
    PFNGLBLENDEQUATIONPROC                       pGLBlendEquation;
    PFNGLBLENDEQUATIONSEPARATEPROC               pGLBlendEquationSeparate;
    PFNGLBLENDFUNCPROC                           pGLBlendFunc;
    PFNGLBLENDFUNCSEPARATEPROC                   pGLBlendFuncSeparate;
    PFNGLBLITFRAMEBUFFERPROC                     pGLBlitFramebuffer;
    PFNGLBUFFERDATAPROC                          pGLBufferData;
    PFNGLBUFFERSUBDATAPROC                       pGLBufferSubData;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC              pGLCheckFramebufferStatus;
    PFNGLCLEARPROC                               pGLClear;
    PFNGLCLEARBUFFERFIPROC                       pGLClearBufferfi;
    PFNGLCLEARBUFFERFVPROC                       pGLClearBufferfv;
    PFNGLCLEARBUFFERIVPROC                       pGLClearBufferiv;
    PFNGLCLEARBUFFERUIVPROC                      pGLClearBufferuiv;
    PFNGLCLEARCOLORPROC                          pGLClearColor;
    PFNGLCLEARDEPTHFPROC                         pGLClearDepthf;
    PFNGLCLEARSTENCILPROC                        pGLClearStencil;
    PFNGLCLIENTWAITSYNCPROC                      pGLClientWaitSync;
    PFNGLCOLORMASKPROC                           pGLColorMask;
    PFNGLCOMPILESHADERPROC                       pGLCompileShader;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC             pGLCompressedTexSubImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC             pGLCompressedTexSubImage3D;
    PFNGLCOPYBUFFERSUBDATAPROC                   pGLCopyBufferSubData;
    PFNGLCOPYTEXSUBIMAGE2DPROC                   pGLCopyTexSubImage2D;
    PFNGLCOPYTEXSUBIMAGE3DPROC                   pGLCopyTexSubImage3D;
    PFNGLCREATEPROGRAMPROC                       pGLCreateProgram;
    PFNGLCREATESHADERPROC                        pGLCreateShader;
    PFNGLCREATESHADERPROGRAMVPROC                pGLCreateShaderProgramv;
    PFNGLCULLFACEPROC                            pGLCullFace;
    PFNGLDELETEBUFFERSPROC                       pGLDeleteBuffers;
    PFNGLDELETEFRAMEBUFFERSPROC                  pGLDeleteFramebuffers;
    PFNGLDELETEPROGRAMPROC                       pGLDeleteProgram;
    PFNGLDELETEPROGRAMPIPELINESPROC              pGLDeleteProgramPipelines;
    PFNGLDELETEQUERIESPROC                       pGLDeleteQueries;
    PFNGLDELETERENDERBUFFERSPROC                 pGLDeleteRenderbuffers;
    PFNGLDELETESAMPLERSPROC                      pGLDeleteSamplers;
    PFNGLDELETESHADERPROC                        pGLDeleteShader;
    PFNGLDELETESYNCPROC                          pGLDeleteSync;
    PFNGLDELETETEXTURESPROC                      pGLDeleteTextures;
    PFNGLDELETETRANSFORMFEEDBACKSPROC            pGLDeleteTransformFeedbacks;
    PFNGLDELETEVERTEXARRAYSPROC                  pGLDeleteVertexArrays;
    PFNGLDEPTHFUNCPROC                           pGLDepthFunc;
    PFNGLDEPTHMASKPROC                           pGLDepthMask;
    PFNGLDEPTHRANGEFPROC                         pGLDepthRangef;
    PFNGLDETACHSHADERPROC                        pGLDetachShader;
    PFNGLDISABLEPROC                             pGLDisable;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC            pGLDisableVertexAttribArray;
    PFNGLDISPATCHCOMPUTEPROC                     pGLDispatchCompute;
    PFNGLDISPATCHCOMPUTEINDIRECTPROC             pGLDispatchComputeIndirect;
    PFNGLDRAWARRAYSPROC                          pGLDrawArrays;
    PFNGLDRAWARRAYSINDIRECTPROC                  pGLDrawArraysIndirect;
    PFNGLDRAWARRAYSINSTANCEDPROC                 pGLDrawArraysInstanced;
    PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC     pGLDrawArraysInstancedBaseInstance;
    PFNGLDRAWBUFFERSPROC                         pGLDrawBuffers;
    PFNGLDRAWELEMENTSPROC                        pGLDrawElements;
    PFNGLDRAWELEMENTSINDIRECTPROC                pGLDrawElementsIndirect;
    PFNGLDRAWELEMENTSINSTANCEDPROC               pGLDrawElementsInstanced;
    PFNGLDRAWRANGEELEMENTSPROC                   pGLDrawRangeElements;
    PFNGLENABLEPROC                              pGLEnable;
    PFNGLENABLEVERTEXATTRIBARRAYPROC             pGLEnableVertexAttribArray;
    PFNGLENDQUERYPROC                            pGLEndQuery;
    PFNGLENDTRANSFORMFEEDBACKPROC                pGLEndTransformFeedback;
    PFNGLFENCESYNCPROC                           pGLFenceSync;
    PFNGLFINISHPROC                              pGLFinish;
    PFNGLFLUSHPROC                               pGLFlush;
    PFNGLFLUSHMAPPEDBUFFERRANGEPROC              pGLFlushMappedBufferRange;
    PFNGLFRAMEBUFFERPARAMETERIPROC               pGLFramebufferParameteri;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC             pGLFramebufferRenderbuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC                pGLFramebufferTexture2D;
    PFNGLFRAMEBUFFERTEXTURELAYERPROC             pGLFramebufferTextureLayer;
    PFNGLFRONTFACEPROC                           pGLFrontFace;
    PFNGLGENBUFFERSPROC                          pGLGenBuffers;
    PFNGLGENERATEMIPMAPPROC                      pGLGenerateMipmap;
    PFNGLGENFRAMEBUFFERSPROC                     pGLGenFramebuffers;
    PFNGLGENPROGRAMPIPELINESPROC                 pGLGenProgramPipelines;
    PFNGLGENQUERIESPROC                          pGLGenQueries;
    PFNGLGENRENDERBUFFERSPROC                    pGLGenRenderbuffers;
    PFNGLGENSAMPLERSPROC                         pGLGenSamplers;
    PFNGLGENTEXTURESPROC                         pGLGenTextures;
    PFNGLGENTRANSFORMFEEDBACKSPROC               pGLGenTransformFeedbacks;
    PFNGLGENVERTEXARRAYSPROC                     pGLGenVertexArrays;
    PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC      pGLGetActiveAtomicCounterBufferiv;
    PFNGLGETACTIVEATTRIBPROC                     pGLGetActiveAttrib;
    PFNGLGETACTIVEUNIFORMPROC                    pGLGetActiveUniform;
    PFNGLGETACTIVEUNIFORMBLOCKIVPROC             pGLGetActiveUniformBlockiv;
    PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC           pGLGetActiveUniformBlockName;
    PFNGLGETACTIVEUNIFORMSIVPROC                 pGLGetActiveUniformsiv;
    PFNGLGETATTACHEDSHADERSPROC                  pGLGetAttachedShaders;
    PFNGLGETATTRIBLOCATIONPROC                   pGLGetAttribLocation;
    PFNGLGETBOOLEANI_VPROC                       pGLGetBooleani_v;
    PFNGLGETBOOLEANVPROC                         pGLGetBooleanv;
    PFNGLGETBUFFERPARAMETERI64VPROC              pGLGetBufferParameteri64v;
    PFNGLGETBUFFERPARAMETERIVPROC                pGLGetBufferParameteriv;
    PFNGLGETBUFFERPOINTERVPROC                   pGLGetBufferPointerv;
    PFNGLGETERRORPROC                            pGLGetError;
    PFNGLGETFLOATVPROC                           pGLGetFloatv;
    PFNGLGETFRAGDATALOCATIONPROC                 pGLGetFragDataLocation;
    PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC pGLGetFramebufferAttachmentParameteriv;
    PFNGLGETFRAMEBUFFERPARAMETERIVPROC           pGLGetFramebufferParameteriv;
    PFNGLGETINTEGER64I_VPROC                     pGLGetInteger64i_v;
    PFNGLGETINTEGER64VPROC                       pGLGetInteger64v;
    PFNGLGETINTEGERI_VPROC                       pGLGetIntegeri_v;
    PFNGLGETINTEGERVPROC                         pGLGetIntegerv;
    PFNGLGETINTERNALFORMATIVPROC                 pGLGetInternalformativ;
    PFNGLGETMULTISAMPLEFVPROC                    pGLGetMultisamplefv;
    PFNGLGETPROGRAMBINARYPROC                    pGLGetProgramBinary;
    PFNGLGETPROGRAMINFOLOGPROC                   pGLGetProgramInfoLog;
    PFNGLGETPROGRAMINTERFACEIVPROC               pGLGetProgramInterfaceiv;
    PFNGLGETPROGRAMIVPROC                        pGLGetProgramiv;
    PFNGLGETPROGRAMPIPELINEINFOLOGPROC           pGLGetProgramPipelineInfoLog;
    PFNGLGETPROGRAMPIPELINEIVPROC                pGLGetProgramPipelineiv;
    PFNGLGETPROGRAMRESOURCEINDEXPROC             pGLGetProgramResourceIndex;
    PFNGLGETPROGRAMRESOURCEIVPROC                pGLGetProgramResourceiv;
    PFNGLGETPROGRAMRESOURCELOCATIONPROC          pGLGetProgramResourceLocation;
    PFNGLGETPROGRAMRESOURCENAMEPROC              pGLGetProgramResourceName;
    PFNGLGETQUERYIVPROC                          pGLGetQueryiv;
    PFNGLGETQUERYOBJECTUIVPROC                   pGLGetQueryObjectuiv;
    PFNGLGETRENDERBUFFERPARAMETERIVPROC          pGLGetRenderbufferParameteriv;
    PFNGLGETSAMPLERPARAMETERFVPROC               pGLGetSamplerParameterfv;
    PFNGLGETSAMPLERPARAMETERIVPROC               pGLGetSamplerParameteriv;
    PFNGLGETSHADERINFOLOGPROC                    pGLGetShaderInfoLog;
    PFNGLGETSHADERIVPROC                         pGLGetShaderiv;
    PFNGLGETSHADERPRECISIONFORMATPROC            pGLGetShaderPrecisionFormat;
    PFNGLGETSHADERSOURCEPROC                     pGLGetShaderSource;
    PFNGLGETSTRINGPROC                           pGLGetString;
    PFNGLGETSTRINGIPROC                          pGLGetStringi;
    PFNGLGETSYNCIVPROC                           pGLGetSynciv;
    PFNGLGETTEXLEVELPARAMETERFVPROC              pGLGetTexLevelParameterfv;
    PFNGLGETTEXLEVELPARAMETERIVPROC              pGLGetTexLevelParameteriv;
    PFNGLGETTEXPARAMETERFVPROC                   pGLGetTexParameterfv;
    PFNGLGETTEXPARAMETERIVPROC                   pGLGetTexParameteriv;
    PFNGLGETTRANSFORMFEEDBACKVARYINGPROC         pGLGetTransformFeedbackVarying;
    PFNGLGETUNIFORMBLOCKINDEXPROC                pGLGetUniformBlockIndex;
    PFNGLGETUNIFORMFVPROC                        pGLGetUniformfv;
    PFNGLGETUNIFORMINDICESPROC                   pGLGetUniformIndices;
    PFNGLGETUNIFORMIVPROC                        pGLGetUniformiv;
    PFNGLGETUNIFORMLOCATIONPROC                  pGLGetUniformLocation;
    PFNGLGETUNIFORMUIVPROC                       pGLGetUniformuiv;
    PFNGLGETVERTEXATTRIBFVPROC                   pGLGetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIIVPROC                  pGLGetVertexAttribIiv;
    PFNGLGETVERTEXATTRIBIUIVPROC                 pGLGetVertexAttribIuiv;
    PFNGLGETVERTEXATTRIBIVPROC                   pGLGetVertexAttribiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC             pGLGetVertexAttribPointerv;
    PFNGLHINTPROC                                pGLHint;
    PFNGLINVALIDATEFRAMEBUFFERPROC               pGLInvalidateFramebuffer;
    PFNGLINVALIDATESUBFRAMEBUFFERPROC            pGLInvalidateSubFramebuffer;
    PFNGLISBUFFERPROC                            pGLIsBuffer;
    PFNGLISENABLEDPROC                           pGLIsEnabled;
    PFNGLISFRAMEBUFFERPROC                       pGLIsFramebuffer;
    PFNGLISPROGRAMPROC                           pGLIsProgram;
    PFNGLISPROGRAMPIPELINEPROC                   pGLIsProgramPipeline;
    PFNGLISQUERYPROC                             pGLIsQuery;
    PFNGLISRENDERBUFFERPROC                      pGLIsRenderbuffer;
    PFNGLISSAMPLERPROC                           pGLIsSampler;
    PFNGLISSHADERPROC                            pGLIsShader;
    PFNGLISSYNCPROC                              pGLIsSync;
    PFNGLISTEXTUREPROC                           pGLIsTexture;
    PFNGLISTRANSFORMFEEDBACKPROC                 pGLIsTransformFeedback;
    PFNGLISVERTEXARRAYPROC                       pGLIsVertexArray;
    PFNGLLINEWIDTHPROC                           pGLLineWidth;
    PFNGLLINKPROGRAMPROC                         pGLLinkProgram;
    PFNGLMAPBUFFERRANGEPROC                      pGLMapBufferRange;
    PFNGLMEMORYBARRIERPROC                       pGLMemoryBarrier;
    PFNGLMEMORYBARRIERBYREGIONPROC               pGLMemoryBarrierByRegion;
    PFNGLPAUSETRANSFORMFEEDBACKPROC              pGLPauseTransformFeedback;
    PFNGLPIXELSTOREIPROC                         pGLPixelStorei;
    PFNGLPOLYGONOFFSETPROC                       pGLPolygonOffset;
    PFNGLPROGRAMBINARYPROC                       pGLProgramBinary;
    PFNGLPROGRAMPARAMETERIPROC                   pGLProgramParameteri;
    PFNGLPROGRAMUNIFORM1FPROC                    pGLProgramUniform1f;
    PFNGLPROGRAMUNIFORM1FVPROC                   pGLProgramUniform1fv;
    PFNGLPROGRAMUNIFORM1IPROC                    pGLProgramUniform1i;
    PFNGLPROGRAMUNIFORM1IVPROC                   pGLProgramUniform1iv;
    PFNGLPROGRAMUNIFORM1UIPROC                   pGLProgramUniform1ui;
    PFNGLPROGRAMUNIFORM1UIVPROC                  pGLProgramUniform1uiv;
    PFNGLPROGRAMUNIFORM2FPROC                    pGLProgramUniform2f;
    PFNGLPROGRAMUNIFORM2FVPROC                   pGLProgramUniform2fv;
    PFNGLPROGRAMUNIFORM2IPROC                    pGLProgramUniform2i;
    PFNGLPROGRAMUNIFORM2IVPROC                   pGLProgramUniform2iv;
    PFNGLPROGRAMUNIFORM2UIPROC                   pGLProgramUniform2ui;
    PFNGLPROGRAMUNIFORM2UIVPROC                  pGLProgramUniform2uiv;
    PFNGLPROGRAMUNIFORM3FPROC                    pGLProgramUniform3f;
    PFNGLPROGRAMUNIFORM3FVPROC                   pGLProgramUniform3fv;
    PFNGLPROGRAMUNIFORM3IPROC                    pGLProgramUniform3i;
    PFNGLPROGRAMUNIFORM3IVPROC                   pGLProgramUniform3iv;
    PFNGLPROGRAMUNIFORM3UIPROC                   pGLProgramUniform3ui;
    PFNGLPROGRAMUNIFORM3UIVPROC                  pGLProgramUniform3uiv;
    PFNGLPROGRAMUNIFORM4FPROC                    pGLProgramUniform4f;
    PFNGLPROGRAMUNIFORM4FVPROC                   pGLProgramUniform4fv;
    PFNGLPROGRAMUNIFORM4IPROC                    pGLProgramUniform4i;
    PFNGLPROGRAMUNIFORM4IVPROC                   pGLProgramUniform4iv;
    PFNGLPROGRAMUNIFORM4UIPROC                   pGLProgramUniform4ui;
    PFNGLPROGRAMUNIFORM4UIVPROC                  pGLProgramUniform4uiv;
    PFNGLPROGRAMUNIFORMMATRIX2FVPROC             pGLProgramUniformMatrix2fv;
    PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC           pGLProgramUniformMatrix2x3fv;
    PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC           pGLProgramUniformMatrix2x4fv;
    PFNGLPROGRAMUNIFORMMATRIX3FVPROC             pGLProgramUniformMatrix3fv;
    PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC           pGLProgramUniformMatrix3x2fv;
    PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC           pGLProgramUniformMatrix3x4fv;
    PFNGLPROGRAMUNIFORMMATRIX4FVPROC             pGLProgramUniformMatrix4fv;
    PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC           pGLProgramUniformMatrix4x2fv;
    PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC           pGLProgramUniformMatrix4x3fv;
    PFNGLREADBUFFERPROC                          pGLReadBuffer;
    PFNGLREADPIXELSPROC                          pGLReadPixels;
    PFNGLRELEASESHADERCOMPILERPROC               pGLReleaseShaderCompiler;
    PFNGLRENDERBUFFERSTORAGEPROC                 pGLRenderbufferStorage;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC      pGLRenderbufferStorageMultisample;
    PFNGLRESUMETRANSFORMFEEDBACKPROC             pGLResumeTransformFeedback;
    PFNGLSAMPLECOVERAGEPROC                      pGLSampleCoverage;
    PFNGLSAMPLEMASKIPROC                         pGLSampleMaski;
    PFNGLSAMPLERPARAMETERFPROC                   pGLSamplerParameterf;
    PFNGLSAMPLERPARAMETERFVPROC                  pGLSamplerParameterfv;
    PFNGLSAMPLERPARAMETERIPROC                   pGLSamplerParameteri;
    PFNGLSAMPLERPARAMETERIVPROC                  pGLSamplerParameteriv;
    PFNGLSCISSORPROC                             pGLScissor;
    PFNGLSHADERBINARYPROC                        pGLShaderBinary;
    PFNGLSHADERSOURCEPROC                        pGLShaderSource;
    PFNGLSHADERSTORAGEBLOCKBINDINGPROC           pGLShaderStorageBlockBinding;
    PFNGLSTENCILFUNCPROC                         pGLStencilFunc;
    PFNGLSTENCILFUNCSEPARATEPROC                 pGLStencilFuncSeparate;
    PFNGLSTENCILMASKPROC                         pGLStencilMask;
    PFNGLSTENCILMASKSEPARATEPROC                 pGLStencilMaskSeparate;
    PFNGLSTENCILOPPROC                           pGLStencilOp;
    PFNGLSTENCILOPSEPARATEPROC                   pGLStencilOpSeparate;
    PFNGLTEXPARAMETERFPROC                       pGLTexParameterf;
    PFNGLTEXPARAMETERFVPROC                      pGLTexParameterfv;
    PFNGLTEXPARAMETERIPROC                       pGLTexParameteri;
    PFNGLTEXPARAMETERIVPROC                      pGLTexParameteriv;
    PFNGLTEXSTORAGE2DPROC                        pGLTexStorage2D;
    PFNGLTEXSTORAGE2DMULTISAMPLEPROC             pGLTexStorage2DMultisample;
    PFNGLTEXSTORAGE3DPROC                        pGLTexStorage3D;
    PFNGLTEXSUBIMAGE2DPROC                       pGLTexSubImage2D;
    PFNGLTEXSUBIMAGE3DPROC                       pGLTexSubImage3D;
    PFNGLTRANSFORMFEEDBACKVARYINGSPROC           pGLTransformFeedbackVaryings;
    PFNGLUNIFORM1FPROC                           pGLUniform1f;
    PFNGLUNIFORM1FVPROC                          pGLUniform1fv;
    PFNGLUNIFORM1IPROC                           pGLUniform1i;
    PFNGLUNIFORM1IVPROC                          pGLUniform1iv;
    PFNGLUNIFORM1UIPROC                          pGLUniform1ui;
    PFNGLUNIFORM1UIVPROC                         pGLUniform1uiv;
    PFNGLUNIFORM2FPROC                           pGLUniform2f;
    PFNGLUNIFORM2FVPROC                          pGLUniform2fv;
    PFNGLUNIFORM2IPROC                           pGLUniform2i;
    PFNGLUNIFORM2IVPROC                          pGLUniform2iv;
    PFNGLUNIFORM2UIPROC                          pGLUniform2ui;
    PFNGLUNIFORM2UIVPROC                         pGLUniform2uiv;
    PFNGLUNIFORM3FPROC                           pGLUniform3f;
    PFNGLUNIFORM3FVPROC                          pGLUniform3fv;
    PFNGLUNIFORM3IPROC                           pGLUniform3i;
    PFNGLUNIFORM3IVPROC                          pGLUniform3iv;
    PFNGLUNIFORM3UIPROC                          pGLUniform3ui;
    PFNGLUNIFORM3UIVPROC                         pGLUniform3uiv;
    PFNGLUNIFORM4FPROC                           pGLUniform4f;
    PFNGLUNIFORM4FVPROC                          pGLUniform4fv;
    PFNGLUNIFORM4IPROC                           pGLUniform4i;
    PFNGLUNIFORM4IVPROC                          pGLUniform4iv;
    PFNGLUNIFORM4UIPROC                          pGLUniform4ui;
    PFNGLUNIFORM4UIVPROC                         pGLUniform4uiv;
    PFNGLUNIFORMBLOCKBINDINGPROC                 pGLUniformBlockBinding;
    PFNGLUNIFORMMATRIX2FVPROC                    pGLUniformMatrix2fv;
    PFNGLUNIFORMMATRIX2X3FVPROC                  pGLUniformMatrix2x3fv;
    PFNGLUNIFORMMATRIX2X4FVPROC                  pGLUniformMatrix2x4fv;
    PFNGLUNIFORMMATRIX3FVPROC                    pGLUniformMatrix3fv;
    PFNGLUNIFORMMATRIX3X2FVPROC                  pGLUniformMatrix3x2fv;
    PFNGLUNIFORMMATRIX3X4FVPROC                  pGLUniformMatrix3x4fv;
    PFNGLUNIFORMMATRIX4FVPROC                    pGLUniformMatrix4fv;
    PFNGLUNIFORMMATRIX4X2FVPROC                  pGLUniformMatrix4x2fv;
    PFNGLUNIFORMMATRIX4X3FVPROC                  pGLUniformMatrix4x3fv;
    PFNGLUNMAPBUFFERPROC                         pGLUnmapBuffer;
    PFNGLUSEPROGRAMPROC                          pGLUseProgram;
    PFNGLUSEPROGRAMSTAGESPROC                    pGLUseProgramStages;
    PFNGLVALIDATEPROGRAMPROC                     pGLValidateProgram;
    PFNGLVALIDATEPROGRAMPIPELINEPROC             pGLValidateProgramPipeline;
    PFNGLVERTEXATTRIB1FPROC                      pGLVertexAttrib1f;
    PFNGLVERTEXATTRIB1FVPROC                     pGLVertexAttrib1fv;
    PFNGLVERTEXATTRIB2FPROC                      pGLVertexAttrib2f;
    PFNGLVERTEXATTRIB2FVPROC                     pGLVertexAttrib2fv;
    PFNGLVERTEXATTRIB3FPROC                      pGLVertexAttrib3f;
    PFNGLVERTEXATTRIB3FVPROC                     pGLVertexAttrib3fv;
    PFNGLVERTEXATTRIB4FPROC                      pGLVertexAttrib4f;
    PFNGLVERTEXATTRIB4FVPROC                     pGLVertexAttrib4fv;
    PFNGLVERTEXATTRIBBINDINGPROC                 pGLVertexAttribBinding;
    PFNGLVERTEXATTRIBDIVISORPROC                 pGLVertexAttribDivisor;
    PFNGLVERTEXATTRIBFORMATPROC                  pGLVertexAttribFormat;
    PFNGLVERTEXATTRIBI4IPROC                     pGLVertexAttribI4i;
    PFNGLVERTEXATTRIBI4IVPROC                    pGLVertexAttribI4iv;
    PFNGLVERTEXATTRIBI4UIPROC                    pGLVertexAttribI4ui;
    PFNGLVERTEXATTRIBI4UIVPROC                   pGLVertexAttribI4uiv;
    PFNGLVERTEXATTRIBIFORMATPROC                 pGLVertexAttribIFormat;
    PFNGLVERTEXATTRIBIPOINTERPROC                pGLVertexAttribIPointer;
    PFNGLVERTEXATTRIBPOINTERPROC                 pGLVertexAttribPointer;
    PFNGLVERTEXBINDINGDIVISORPROC                pGLVertexBindingDivisor;
    PFNGLVIEWPORTPROC                            pGLViewport;
    PFNGLWAITSYNCPROC                            pGLWaitSync;

} ogl_context_es_entrypoints;

/* Encapsulates entry-points introduced by ES' GL_EXT_texture_buffer */
typedef struct
{

    PFNGLTEXBUFFEREXTPROC   pGLTexBufferEXT;
    PFNGLTEXBUFFERRANGEPROC pGLTexBufferRangeEXT;

} ogl_context_es_entrypoints_ext_texture_buffer;

/** Structure that cotnains context-specific strings of GL */
typedef struct
{
    const GLubyte** extensions;
    const GLubyte*  renderer;
    const GLubyte*  shading_language_version;
    const GLubyte*  vendor;
    const GLubyte*  version;
} ogl_context_gl_info;

/** Structure that contains context-specific maximum parameter values of GL4.3 */
typedef struct
{
    GLfloat aliased_line_width_range[2];
    GLfloat smooth_line_width_range [2];
    GLfloat line_width_granularity;
    GLint   major_version;
    GLint   max_3d_texture_size;
    GLint   max_array_texture_layers;
    GLint   max_atomic_counter_buffer_bindings;
    GLint   max_atomic_counter_buffer_size;
    GLint   max_color_attachments;
    GLint   max_color_texture_samples;
    GLint   max_combined_atomic_counter_buffers;
    GLint   max_combined_atomic_counters;
    GLint   max_combined_compute_uniform_components;
    GLint   max_combined_fragment_uniform_components;
    GLint   max_combined_geometry_uniform_components;
    GLint   max_combined_image_units_and_fragment_outputs;
    GLint   max_combined_shader_storage_blocks;
    GLint   max_combined_texture_image_units;
    GLint   max_combined_vertex_uniform_components;
    GLint   max_combined_uniform_blocks;
    GLint   max_compute_atomic_counter_buffers;
    GLint   max_compute_atomic_counters;
    GLint   max_compute_image_uniforms;
    GLint   max_compute_shader_storage_blocks;
    GLint   max_compute_shared_memory_size;
    GLint   max_compute_texture_image_units;
    GLint   max_compute_uniform_blocks;
    GLint   max_compute_uniform_components;
    GLint   max_compute_work_group_count[3];
    GLint   max_compute_work_group_invocations;
    GLint   max_compute_work_group_size[3];
    GLint   max_cube_map_texture_size;
    GLint   max_depth_texture_samples;
    GLint   max_draw_buffers;
    GLint   max_elements_indices;
    GLint   max_elements_vertices;
    GLint   max_fragment_atomic_counter_buffers;
    GLint   max_fragment_atomic_counters;
    GLint   max_fragment_input_components;
    GLint   max_fragment_shader_storage_blocks;
    GLint   max_fragment_uniform_components;
    GLint   max_fragment_uniform_blocks;
    GLint   max_framebuffer_height;
    GLint   max_framebuffer_layers;
    GLint   max_framebuffer_samples;
    GLint   max_framebuffer_width;
    GLint   max_geometry_atomic_counter_buffers;
    GLint   max_geometry_atomic_counters;
    GLint   max_geometry_input_components;
    GLint   max_geometry_output_vertices;
    GLint   max_geometry_shader_storage_blocks;
    GLint   max_geometry_texture_image_units;
    GLint   max_geometry_uniform_blocks;
    GLint   max_geometry_uniform_components;
    GLint   max_image_samples;
    GLint   max_image_units;
    GLint   max_integer_samples;
    GLint   max_program_texel_offset;
    GLint   max_rectangle_texture_size;
    GLint   max_renderbuffer_size;
    GLint   max_sample_mask_words;
    GLint   max_server_wait_timeout;
    GLint   max_shader_storage_block_size;
    GLint   max_shader_storage_buffer_bindings;
    GLint   max_tess_control_atomic_counter_buffers;
    GLint   max_tess_control_atomic_counters;
    GLint   max_tess_control_shader_storage_blocks;
    GLint   max_tess_evaluation_atomic_counter_buffers;
    GLint   max_tess_evaluation_atomic_counters;
    GLint   max_tess_evaluation_shader_storage_blocks;
    GLint   max_texture_buffer_size;
    GLint   max_texture_image_units;
    GLfloat max_texture_lod_bias;
    GLint   max_texture_size;
    GLint   max_transform_feedback_buffers;
    GLint   max_transform_feedback_interleaved_components;
    GLint   max_transform_feedback_separate_components;
    GLint   max_uniform_block_size;
    GLint   max_uniform_buffer_bindings;
    GLint   max_vertex_atomic_counter_buffers;
    GLint   max_vertex_atomic_counters;
    GLint   max_vertex_attrib_bindings;
    GLint   max_vertex_attribs;
    GLint   max_vertex_output_components;
    GLint   max_vertex_shader_storage_blocks;
    GLint   max_vertex_texture_image_units;
    GLint   max_vertex_uniform_blocks;
    GLint   max_vertex_uniform_components;
    GLint   max_viewports;
    GLint   min_max_uniform_block_size;
    GLint   min_program_texel_offset;
    GLint   minor_version;
    GLint   num_compressed_texture_formats;
    GLint   num_extensions;
    GLint   num_program_binary_formats;
    GLfloat point_fade_threshold_size;
    GLint   point_size_range[2];
    GLint*  program_binary_formats;
    GLint   shader_storage_buffer_offset_alignment;
    GLint   subpixel_bits;
    GLint   texture_buffer_offset_alignment;
    GLint   uniform_buffer_offset_alignment;
} ogl_context_gl_limits;

/** Structure that contains context-specific GL4.3 function entry points */
typedef struct
{
    PFNGLACTIVESHADERPROGRAMPROC                         pGLActiveShaderProgram;
    PFNGLACTIVETEXTUREPROC                               pGLActiveTexture;
    PFNGLATTACHSHADERPROC                                pGLAttachShader;
    PFNGLBEGINCONDITIONALRENDERPROC                      pGLBeginConditionalRender;
    PFNGLBEGINQUERYPROC                                  pGLBeginQuery;
    PFNGLBEGINTRANSFORMFEEDBACKPROC                      pGLBeginTransformFeedback;
    PFNGLBINDATTRIBLOCATIONPROC                          pGLBindAttribLocation;
    PFNGLBINDBUFFERPROC                                  pGLBindBuffer;
    PFNGLBINDBUFFERBASEPROC                              pGLBindBufferBase;
    PFNGLBINDBUFFERRANGEPROC                             pGLBindBufferRange;
    PFNGLBINDFRAGDATALOCATIONPROC                        pGLBindFragDataLocation;
    PFNGLBINDFRAMEBUFFERPROC                             pGLBindFramebuffer;
    PFNGLBINDIMAGETEXTUREEXTPROC                         pGLBindImageTexture;
    PFNGLBINDPROGRAMPIPELINEPROC                         pGLBindProgramPipeline;
    PFNGLBINDRENDERBUFFERPROC                            pGLBindRenderbuffer;
    PFNGLBINDSAMPLERPROC                                 pGLBindSampler;
    PFNGLBINDTRANSFORMFEEDBACKPROC                       pGLBindTransformFeedback;
    PFNGLBINDTEXTUREPROC                                 pGLBindTexture;
    PFNGLBINDVERTEXARRAYPROC                             pGLBindVertexArray;
    PFNGLBINDVERTEXBUFFERPROC                            pGLBindVertexBuffer;
    PFNGLBLENDCOLORPROC                                  pGLBlendColor;
    PFNGLBLENDEQUATIONPROC                               pGLBlendEquation;
    PFNGLBLENDEQUATIONSEPARATEPROC                       pGLBlendEquationSeparate;
    PFNGLBLENDFUNCPROC                                   pGLBlendFunc;
    PFNGLBLENDFUNCSEPARATEPROC                           pGLBlendFuncSeparate;
    PFNGLBLITFRAMEBUFFERPROC                             pGLBlitFramebuffer;
    PFNGLBUFFERDATAPROC                                  pGLBufferData;
    PFNGLBUFFERSUBDATAPROC                               pGLBufferSubData;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC                      pGLCheckFramebufferStatus;
    PFNGLCLAMPCOLORPROC                                  pGLClampColor;
    PFNGLCLEARPROC                                       pGLClear;
    PFNGLCLEARBUFFERDATAPROC                             pGLClearBufferData;
    PFNGLCLEARBUFFERSUBDATAPROC                          pGLClearBufferSubData;
    PFNGLCLEARBUFFERFVPROC                               pGLClearBufferfv;
    PFNGLCLEARBUFFERFIPROC                               pGLClearBufferfi;
    PFNGLCLEARBUFFERIVPROC                               pGLClearBufferiv;
    PFNGLCLEARBUFFERUIVPROC                              pGLClearBufferuiv;
    PFNGLCLEARCOLORPROC                                  pGLClearColor;
    PFNGLCLEARDEPTHPROC                                  pGLClearDepth;
    PFNGLCLEARDEPTHFPROC                                 pGLClearDepthf;
    PFNGLCLEARSTENCILPROC                                pGLClearStencil;
    PFNGLCLIENTWAITSYNCPROC                              pGLClientWaitSync;
    PFNGLCOLORMASKPROC                                   pGLColorMask;
    PFNGLCOLORMASKIPROC                                  pGLColorMaski;
    PFNGLCOMPILESHADERPROC                               pGLCompileShader;
    PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC                     pGLCompressedTexSubImage1D;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC                     pGLCompressedTexSubImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC                     pGLCompressedTexSubImage3D;
    PFNGLCOPYBUFFERSUBDATAPROC                           pGLCopyBufferSubData;
    PFNGLCOPYIMAGESUBDATAPROC                            pGLCopyImageSubData;
    PFNGLCOPYTEXSUBIMAGE1DPROC                           pGLCopyTexSubImage1D;
    PFNGLCOPYTEXSUBIMAGE2DPROC                           pGLCopyTexSubImage2D;
    PFNGLCOPYTEXSUBIMAGE3DPROC                           pGLCopyTexSubImage3D;
    PFNGLCREATEPROGRAMPROC                               pGLCreateProgram;
    PFNGLCREATESHADERPROC                                pGLCreateShader;
    PFNGLCREATESHADERPROGRAMVPROC                        pGLCreateShaderProgramv;
    PFNGLCULLFACEPROC                                    pGLCullFace;
    PFNGLDEBUGMESSAGECALLBACKPROC                        pGLDebugMessageCallback;
    PFNGLDEBUGMESSAGECONTROLPROC                         pGLDebugMessageControl;
    PFNGLDEBUGMESSAGEINSERTPROC                          pGLDebugMessageInsert;
    PFNGLDELETEBUFFERSPROC                               pGLDeleteBuffers;
    PFNGLDELETEFRAMEBUFFERSPROC                          pGLDeleteFramebuffers;
    PFNGLDELETEPROGRAMPROC                               pGLDeleteProgram;
    PFNGLDELETEPROGRAMPIPELINESPROC                      pGLDeleteProgramPipelines;
    PFNGLDELETEQUERIESPROC                               pGLDeleteQueries;
    PFNGLDELETERENDERBUFFERSPROC                         pGLDeleteRenderbuffers;
    PFNGLDELETESAMPLERSPROC                              pGLDeleteSamplers;
    PFNGLDELETESHADERPROC                                pGLDeleteShader;
    PFNGLDELETESYNCPROC                                  pGLDeleteSync;
    PFNGLDELETETEXTURESPROC                              pGLDeleteTextures;
    PFNGLDELETETRANSFORMFEEDBACKSPROC                    pGLDeleteTransformFeedbacks;
    PFNGLDELETEVERTEXARRAYSPROC                          pGLDeleteVertexArrays;
    PFNGLDEPTHFUNCPROC                                   pGLDepthFunc;
    PFNGLDEPTHMASKPROC                                   pGLDepthMask;
    PFNGLDEPTHRANGEPROC                                  pGLDepthRange;
    PFNGLDEPTHRANGEINDEXEDPROC                           pGLDepthRangeIndexed;
    PFNGLDETACHSHADERPROC                                pGLDetachShader;
    PFNGLDISABLEPROC                                     pGLDisable;
    PFNGLDISABLEIPROC                                    pGLDisablei;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC                    pGLDisableVertexAttribArray;
    PFNGLDISPATCHCOMPUTEPROC                             pGLDispatchCompute;
    PFNGLDISPATCHCOMPUTEINDIRECTPROC                     pGLDispatchComputeIndirect;
    PFNGLDRAWARRAYSPROC                                  pGLDrawArrays;
    PFNGLDRAWARRAYSINDIRECTPROC                          pGLDrawArraysIndirect;
    PFNGLDRAWARRAYSINSTANCEDPROC                         pGLDrawArraysInstanced;
    PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC             pGLDrawArraysInstancedBaseInstance;
    PFNGLDRAWBUFFERPROC                                  pGLDrawBuffer;
    PFNGLDRAWBUFFERSPROC                                 pGLDrawBuffers;
    PFNGLDRAWELEMENTSPROC                                pGLDrawElements;
    PFNGLDRAWELEMENTSINSTANCEDPROC                       pGLDrawElementsInstanced;
    PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC           pGLDrawElementsInstancedBaseInstance;
    PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC             pGLDrawElementsInstancedBaseVertex;
    PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC pGLDrawElementsInstancedBaseVertexBaseInstance;
    PFNGLDRAWRANGEELEMENTSPROC                           pGLDrawRangeElements;
    PFNGLDRAWTRANSFORMFEEDBACKPROC                       pGLDrawTransformFeedback;
    PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC              pGLDrawTransformFeedbackInstanced;
    PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC        pGLDrawTransformFeedbackStreamInstanced;
    PFNGLENABLEPROC                                      pGLEnable;
    PFNGLENABLEIPROC                                     pGLEnablei;
    PFNGLENABLEVERTEXATTRIBARRAYPROC                     pGLEnableVertexAttribArray;
    PFNGLENDCONDITIONALRENDERPROC                        pGLEndConditionalRender;
    PFNGLENDQUERYPROC                                    pGLEndQuery;
    PFNGLENDTRANSFORMFEEDBACKPROC                        pGLEndTransformFeedback;
    PFNGLFENCESYNCPROC                                   pGLFenceSync;
    PFNGLFINISHPROC                                      pGLFinish;
    PFNGLFLUSHPROC                                       pGLFlush;
    PFNGLFRAMEBUFFERPARAMETERIPROC                       pGLFramebufferParameteri;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC                     pGLFramebufferRenderbuffer;
    PFNGLFRAMEBUFFERTEXTUREPROC                          pGLFramebufferTexture;
    PFNGLFRAMEBUFFERTEXTURE1DPROC                        pGLFramebufferTexture1D;
    PFNGLFRAMEBUFFERTEXTURE2DPROC                        pGLFramebufferTexture2D;
    PFNGLFRAMEBUFFERTEXTURE3DPROC                        pGLFramebufferTexture3D;
    PFNGLFRAMEBUFFERTEXTURELAYERPROC                     pGLFramebufferTextureLayer;
    PFNGLFRONTFACEPROC                                   pGLFrontFace;
    PFNGLGENBUFFERSPROC                                  pGLGenBuffers;
    PFNGLGENERATEMIPMAPPROC                              pGLGenerateMipmap;
    PFNGLGENFRAMEBUFFERSPROC                             pGLGenFramebuffers;
    PFNGLGENPROGRAMPIPELINESPROC                         pGLGenProgramPipelines;
    PFNGLGENQUERIESPROC                                  pGLGenQueries;
    PFNGLGENRENDERBUFFERSPROC                            pGLGenRenderbuffers;
    PFNGLGENSAMPLERSPROC                                 pGLGenSamplers;
    PFNGLGENTEXTURESPROC                                 pGLGenTextures;
    PFNGLGENTRANSFORMFEEDBACKSPROC                       pGLGenTransformFeedbacks;
    PFNGLGENVERTEXARRAYSPROC                             pGLGenVertexArrays;
    PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC              pGLGetActiveAtomicCounterBufferiv;
    PFNGLGETACTIVEATTRIBPROC                             pGLGetActiveAttrib;
    PFNGLGETACTIVEUNIFORMPROC                            pGLGetActiveUniform;
    PFNGLGETACTIVEUNIFORMBLOCKIVPROC                     pGLGetActiveUniformBlockiv;
    PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC                   pGLGetActiveUniformBlockName;
    PFNGLGETACTIVEUNIFORMSIVPROC                         pGLGetActiveUniformsiv;
    PFNGLGETATTACHEDSHADERSPROC                          pGLGetAttachedShaders;
    PFNGLGETATTRIBLOCATIONPROC                           pGLGetAttribLocation;
    PFNGLGETBOOLEANVPROC                                 pGLGetBooleanv;
    PFNGLGETBUFFERPARAMETERI64VPROC                      pGLGetBufferParameteri64v;
    PFNGLGETBUFFERPARAMETERIVPROC                        pGLGetBufferParameteriv;
    PFNGLGETBUFFERPOINTERVPROC                           pGLGetBufferPointerv;
    PFNGLGETBUFFERSUBDATAPROC                            pGLGetBufferSubData;
    PFNGLGETBOOLEANI_VPROC                               pGLGetBooleani_v;
    PFNGLGETCOMPRESSEDTEXIMAGEPROC                       pGLGetCompressedTexImage;
    PFNGLGETDEBUGMESSAGELOGPROC                          pGLGetDebugMessageLog;
    PFNGLGETFRAMEBUFFERPARAMETERIVPROC                   pGLGetFramebufferParameteriv;
    PFNGLGETDOUBLEVPROC                                  pGLGetDoublev;
    PFNGLGETERRORPROC                                    pGLGetError;
    PFNGLGETFLOATVPROC                                   pGLGetFloatv;
    PFNGLGETFRAGDATALOCATIONPROC                         pGLGetFragDataLocation;
    PFNGLGETINTEGER64I_VPROC                             pGLGetInteger64i_v;
    PFNGLGETINTEGERI_VPROC                               pGLGetIntegeri_v;
    PFNGLGETINTEGERVPROC                                 pGLGetIntegerv;
    PFNGLGETINTERNALFORMATIVPROC                         pGLGetInternalformativ;
    PFNGLGETOBJECTLABELPROC                              pGLGetObjectLabel;
    PFNGLGETOBJECTPTRLABELPROC                           pGLGetObjectPtrLabel;
    PFNGLGETPROGRAMBINARYPROC                            pGLGetProgramBinary;
    PFNGLGETPROGRAMIVPROC                                pGLGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC                           pGLGetProgramInfoLog;
    PFNGLGETPROGRAMINTERFACEIVPROC                       pGLGetProgramInterfaceiv;
    PFNGLGETPROGRAMRESOURCEINDEXPROC                     pGLGetProgramResourceIndex;
    PFNGLGETPROGRAMRESOURCEIVPROC                        pGLGetProgramResourceiv;
    PFNGLGETPROGRAMRESOURCELOCATIONPROC                  pGLGetProgramResourceLocation;
    PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC             pGLGetProgramResourceLocationIndex;
    PFNGLGETPROGRAMRESOURCENAMEPROC                      pGLGetProgramResourceName;
    PFNGLGETRENDERBUFFERPARAMETERIVPROC                  pGLGetRenderbufferParameteriv;
    PFNGLGETSAMPLERPARAMETERFVPROC                       pGLGetSamplerParameterfv;
    PFNGLGETSAMPLERPARAMETERIVPROC                       pGLGetSamplerParameteriv;
    PFNGLGETSAMPLERPARAMETERIIVPROC                      pGLGetSamplerParameterIiv;
    PFNGLGETSAMPLERPARAMETERIUIVPROC                     pGLGetSamplerParameterIuiv;
    PFNGLGETSHADERIVPROC                                 pGLGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC                            pGLGetShaderInfoLog;
    PFNGLGETSHADERSOURCEPROC                             pGLGetShaderSource;
    PFNGLGETSTRINGPROC                                   pGLGetString;
    PFNGLGETSTRINGIPROC                                  pGLGetStringi;
    PFNGLGETTEXIMAGEPROC                                 pGLGetTexImage;
    PFNGLGETTEXLEVELPARAMETERFVPROC                      pGLGetTexLevelParameterfv;
    PFNGLGETTEXLEVELPARAMETERIVPROC                      pGLGetTexLevelParameteriv;
    PFNGLGETTEXPARAMETERFVPROC                           pGLGetTexParameterfv;
    PFNGLGETTEXPARAMETERIIVPROC                          pGLGetTexParameterIiv;
    PFNGLGETTEXPARAMETERIUIVPROC                         pGLGetTexParameterIuiv;
    PFNGLGETTEXPARAMETERIVPROC                           pGLGetTexParameteriv;
    PFNGLGETTRANSFORMFEEDBACKVARYINGPROC                 pGLGetTransformFeedbackVarying;
    PFNGLGETUNIFORMBLOCKINDEXPROC                        pGLGetUniformBlockIndex;
    PFNGLGETUNIFORMFVPROC                                pGLGetUniformfv;
    PFNGLGETUNIFORMIVPROC                                pGLGetUniformiv;
    PFNGLGETUNIFORMLOCATIONPROC                          pGLGetUniformLocation;
    PFNGLGETQUERYIVPROC                                  pGLGetQueryiv;
    PFNGLGETQUERYOBJECTIVPROC                            pGLGetQueryObjectiv;
    PFNGLGETQUERYOBJECTUIVPROC                           pGLGetQueryObjectuiv;
    PFNGLGETQUERYOBJECTUI64VPROC                         pGLGetQueryObjectui64v;
    PFNGLGETUNIFORMUIVPROC                               pGLGetUniformuiv;
    PFNGLGETVERTEXATTRIBDVPROC                           pGLGetVertexAttribdv;
    PFNGLGETVERTEXATTRIBFVPROC                           pGLGetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIIVPROC                          pGLGetVertexAttribIiv;
    PFNGLGETVERTEXATTRIBIUIVPROC                         pGLGetVertexAttribIuiv;
    PFNGLGETVERTEXATTRIBIVPROC                           pGLGetVertexAttribiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC                     pGLGetVertexAttribPointerv;
    PFNGLHINTPROC                                        pGLHint;
    PFNGLINVALIDATEBUFFERDATAPROC                        pGLInvalidateBufferData;
    PFNGLINVALIDATEBUFFERSUBDATAPROC                     pGLInvalidateBufferSubData;
    PFNGLINVALIDATEFRAMEBUFFERPROC                       pGLInvalidateFramebuffer;
    PFNGLINVALIDATESUBFRAMEBUFFERPROC                    pGLInvalidateSubFramebuffer;
    PFNGLINVALIDATETEXIMAGEPROC                          pGLInvalidateTexImage;
    PFNGLINVALIDATETEXSUBIMAGEPROC                       pGLInvalidateTexSubImage;
    PFNGLISBUFFERPROC                                    pGLIsBuffer;
    PFNGLISENABLEDPROC                                   pGLIsEnabled;
    PFNGLISENABLEDIPROC                                  pGLIsEnabledi;
    PFNGLISPROGRAMPROC                                   pGLIsProgram;
    PFNGLISSHADERPROC                                    pGLIsShader;
    PFNGLISTEXTUREPROC                                   pGLIsTexture;
    PFNGLISTRANSFORMFEEDBACKPROC                         pGLIsTransformFeedback;
    PFNGLISQUERYPROC                                     pGLIsQuery;
    PFNGLLINEWIDTHPROC                                   pGLLineWidth;
    PFNGLLINKPROGRAMPROC                                 pGLLinkProgram;
    PFNGLLOGICOPPROC                                     pGLLogicOp;
    PFNGLMAPBUFFERPROC                                   pGLMapBuffer;
    PFNGLMAPBUFFERRANGEPROC                              pGLMapBufferRange;
    PFNGLMEMORYBARRIEREXTPROC                            pGLMemoryBarrier;
    PFNGLMINSAMPLESHADINGPROC                            pGLMinSampleShading;
    PFNGLMULTIDRAWARRAYSPROC                             pGLMultiDrawArrays;
    PFNGLMULTIDRAWARRAYSINDIRECTPROC                     pGLMultiDrawArraysIndirect;
    PFNGLMULTIDRAWELEMENTSPROC                           pGLMultiDrawElements;
    PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC                 pGLMultiDrawElementsBaseVertex;
    PFNGLMULTIDRAWELEMENTSINDIRECTPROC                   pGLMultiDrawElementsIndirect;
    PFNGLOBJECTLABELPROC                                 pGLObjectLabel;
    PFNGLOBJECTPTRLABELPROC                              pGLObjectPtrLabel;
    PFNGLPATCHPARAMETERIPROC                             pGLPatchParameteri;
    PFNGLPAUSETRANSFORMFEEDBACKPROC                      pGLPauseTransformFeedback;
    PFNGLPIXELSTOREFPROC                                 pGLPixelStoref;
    PFNGLPIXELSTOREIPROC                                 pGLPixelStorei;
    PFNGLPOINTPARAMETERFPROC                             pGLPointParameterf;
    PFNGLPOINTPARAMETERFVPROC                            pGLPointParameterfv;
    PFNGLPOINTPARAMETERIPROC                             pGLPointParameteri;
    PFNGLPOINTPARAMETERIVPROC                            pGLPointParameteriv;
    PFNGLPOINTSIZEPROC                                   pGLPointSize;
    PFNGLPOLYGONMODEPROC                                 pGLPolygonMode;
    PFNGLPOLYGONOFFSETPROC                               pGLPolygonOffset;
    PFNGLPOPDEBUGGROUPPROC                               pGLPopDebugGroup;
    PFNGLPRIMITIVERESTARTINDEXPROC                       pGLPrimitiveRestartIndex;
    PFNGLPROGRAMBINARYPROC                               pGLProgramBinary;
    PFNGLPROGRAMPARAMETERIPROC                           pGLProgramParameteri;
    PFNGLPROGRAMUNIFORM1DPROC                            pGLProgramUniform1d;
    PFNGLPROGRAMUNIFORM1DVPROC                           pGLProgramUniform1dv;
    PFNGLPROGRAMUNIFORM1FPROC                            pGLProgramUniform1f;
    PFNGLPROGRAMUNIFORM1FVPROC                           pGLProgramUniform1fv;
    PFNGLPROGRAMUNIFORM1IPROC                            pGLProgramUniform1i;
    PFNGLPROGRAMUNIFORM1IVPROC                           pGLProgramUniform1iv;
    PFNGLPROGRAMUNIFORM1UIPROC                           pGLProgramUniform1ui;
    PFNGLPROGRAMUNIFORM1UIVPROC                          pGLProgramUniform1uiv;
    PFNGLPROGRAMUNIFORM2DPROC                            pGLProgramUniform2d;
    PFNGLPROGRAMUNIFORM2DVPROC                           pGLProgramUniform2dv;
    PFNGLPROGRAMUNIFORM2FPROC                            pGLProgramUniform2f;
    PFNGLPROGRAMUNIFORM2FVPROC                           pGLProgramUniform2fv;
    PFNGLPROGRAMUNIFORM2IPROC                            pGLProgramUniform2i;
    PFNGLPROGRAMUNIFORM2IVPROC                           pGLProgramUniform2iv;
    PFNGLPROGRAMUNIFORM2UIPROC                           pGLProgramUniform2ui;
    PFNGLPROGRAMUNIFORM2UIVPROC                          pGLProgramUniform2uiv;
    PFNGLPROGRAMUNIFORM3DPROC                            pGLProgramUniform3d;
    PFNGLPROGRAMUNIFORM3DVPROC                           pGLProgramUniform3dv;
    PFNGLPROGRAMUNIFORM3FPROC                            pGLProgramUniform3f;
    PFNGLPROGRAMUNIFORM3FVPROC                           pGLProgramUniform3fv;
    PFNGLPROGRAMUNIFORM3IPROC                            pGLProgramUniform3i;
    PFNGLPROGRAMUNIFORM3IVPROC                           pGLProgramUniform3iv;
    PFNGLPROGRAMUNIFORM3UIPROC                           pGLProgramUniform3ui;
    PFNGLPROGRAMUNIFORM3UIVPROC                          pGLProgramUniform3uiv;
    PFNGLPROGRAMUNIFORM4DPROC                            pGLProgramUniform4d;
    PFNGLPROGRAMUNIFORM4DVPROC                           pGLProgramUniform4dv;
    PFNGLPROGRAMUNIFORM4FPROC                            pGLProgramUniform4f;
    PFNGLPROGRAMUNIFORM4FVPROC                           pGLProgramUniform4fv;
    PFNGLPROGRAMUNIFORM4IPROC                            pGLProgramUniform4i;
    PFNGLPROGRAMUNIFORM4IVPROC                           pGLProgramUniform4iv;
    PFNGLPROGRAMUNIFORM4UIPROC                           pGLProgramUniform4ui;
    PFNGLPROGRAMUNIFORM4UIVPROC                          pGLProgramUniform4uiv;
    PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC                  pGLProgramUniformMatrix2fv;
    PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC                pGLProgramUniformMatrix2x3fv;
    PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC                pGLProgramUniformMatrix2x4fv;
    PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC                  pGLProgramUniformMatrix3fv;
    PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC                pGLProgramUniformMatrix3x2fv;
    PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC                pGLProgramUniformMatrix3x4fv;
    PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC                  pGLProgramUniformMatrix4fv;
    PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC                pGLProgramUniformMatrix4x2fv;
    PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC                pGLProgramUniformMatrix4x3fv;
    PFNGLPUSHDEBUGGROUPPROC                              pGLPushDebugGroup;
    PFNGLREADBUFFERPROC                                  pGLReadBuffer;
    PFNGLREADPIXELSPROC                                  pGLReadPixels;
    PFNGLRENDERBUFFERSTORAGEPROC                         pGLRenderbufferStorage;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC              pGLRenderbufferStorageMultisample;
    PFNGLRESUMETRANSFORMFEEDBACKPROC                     pGLResumeTransformFeedback;
    PFNGLSAMPLECOVERAGEPROC                              pGLSampleCoverage;
    PFNGLSAMPLERPARAMETERFPROC                           pGLSamplerParameterf;
    PFNGLSAMPLERPARAMETERFVPROC                          pGLSamplerParameterfv;
    PFNGLSAMPLERPARAMETERIIVPROC                         pGLSamplerParameterIiv;
    PFNGLSAMPLERPARAMETERIUIVPROC                        pGLSamplerParameterIuiv;
    PFNGLSAMPLERPARAMETERIPROC                           pGLSamplerParameteri;
    PFNGLSAMPLERPARAMETERIVPROC                          pGLSamplerParameteriv;
    PFNGLSCISSORPROC                                     pGLScissor;
    PFNGLSCISSORINDEXEDVPROC                             pGLScissorIndexedv;
    PFNGLSHADERSTORAGEBLOCKBINDINGPROC                   pGLShaderStorageBlockBinding;
    PFNGLSHADERSOURCEPROC                                pGLShaderSource;
    PFNGLSTENCILFUNCPROC                                 pGLStencilFunc;
    PFNGLSTENCILFUNCSEPARATEPROC                         pGLStencilFuncSeparate;
    PFNGLSTENCILMASKPROC                                 pGLStencilMask;
    PFNGLSTENCILMASKSEPARATEPROC                         pGLStencilMaskSeparate;
    PFNGLSTENCILOPPROC                                   pGLStencilOp;
    PFNGLSTENCILOPSEPARATEPROC                           pGLStencilOpSeparate;
    PFNGLTEXBUFFERPROC                                   pGLTexBuffer;
    PFNGLTEXBUFFERRANGEPROC                              pGLTexBufferRange;
    PFNGLTEXIMAGE1DPROC                                  pGLTexImage1D;
    PFNGLTEXIMAGE2DPROC                                  pGLTexImage2D;
    PFNGLTEXIMAGE3DPROC                                  pGLTexImage3D;
    PFNGLTEXPARAMETERFPROC                               pGLTexParameterf;
    PFNGLTEXPARAMETERFVPROC                              pGLTexParameterfv;
    PFNGLTEXPARAMETERIIVPROC                             pGLTexParameterIiv;
    PFNGLTEXPARAMETERIUIVPROC                            pGLTexParameterIuiv;
    PFNGLTEXPARAMETERIPROC                               pGLTexParameteri;
    PFNGLTEXPARAMETERIVPROC                              pGLTexParameteriv;
    PFNGLTEXSUBIMAGE1DPROC                               pGLTexSubImage1D;
    PFNGLTEXSUBIMAGE2DPROC                               pGLTexSubImage2D;
    PFNGLTEXSUBIMAGE3DPROC                               pGLTexSubImage3D;
    PFNGLTEXTUREVIEWPROC                                 pGLTextureView;
    PFNGLTRANSFORMFEEDBACKVARYINGSPROC                   pGLTransformFeedbackVaryings;
    PFNGLUNIFORMBLOCKBINDINGPROC                         pGLUniformBlockBinding;
    PFNGLUNMAPBUFFERPROC                                 pGLUnmapBuffer;
    PFNGLUSEPROGRAMPROC                                  pGLUseProgram;
    PFNGLUSEPROGRAMSTAGESPROC                            pGLUseProgramStages;
    PFNGLWAITSYNCPROC                                    pGLWaitSync;
    PFNGLVALIDATEPROGRAMPROC                             pGLValidateProgram;
    PFNGLVERTEXATTRIB1DPROC                              pGLVertexAttrib1d;
    PFNGLVERTEXATTRIB1DVPROC                             pGLVertexAttrib1dv;
    PFNGLVERTEXATTRIB1FPROC                              pGLVertexAttrib1f;
    PFNGLVERTEXATTRIB1FVPROC                             pGLVertexAttrib1fv;
    PFNGLVERTEXATTRIB1SPROC                              pGLVertexAttrib1s;
    PFNGLVERTEXATTRIB1SVPROC                             pGLVertexAttrib1sv;
    PFNGLVERTEXATTRIB2DPROC                              pGLVertexAttrib2d;
    PFNGLVERTEXATTRIB2DVPROC                             pGLVertexAttrib2dv;
    PFNGLVERTEXATTRIB2FPROC                              pGLVertexAttrib2f;
    PFNGLVERTEXATTRIB2FVPROC                             pGLVertexAttrib2fv;
    PFNGLVERTEXATTRIB2SPROC                              pGLVertexAttrib2s;
    PFNGLVERTEXATTRIB2SVPROC                             pGLVertexAttrib2sv;
    PFNGLVERTEXATTRIB3DPROC                              pGLVertexAttrib3d;
    PFNGLVERTEXATTRIB3DVPROC                             pGLVertexAttrib3dv;
    PFNGLVERTEXATTRIB3FPROC                              pGLVertexAttrib3f;
    PFNGLVERTEXATTRIB3FVPROC                             pGLVertexAttrib3fv;
    PFNGLVERTEXATTRIB3SPROC                              pGLVertexAttrib3s;
    PFNGLVERTEXATTRIB3SVPROC                             pGLVertexAttrib3sv;
    PFNGLVERTEXATTRIB4NBVPROC                            pGLVertexAttrib4Nbv;
    PFNGLVERTEXATTRIB4NIVPROC                            pGLVertexAttrib4Niv;
    PFNGLVERTEXATTRIB4NSVPROC                            pGLVertexAttrib4Nsv;
    PFNGLVERTEXATTRIB4NUBPROC                            pGLVertexAttrib4Nub;
    PFNGLVERTEXATTRIB4NUBVPROC                           pGLVertexAttrib4Nubv;
    PFNGLVERTEXATTRIB4NUIVPROC                           pGLVertexAttrib4Nuiv;
    PFNGLVERTEXATTRIB4NUSVPROC                           pGLVertexAttrib4Nusv;
    PFNGLVERTEXATTRIB4BVPROC                             pGLVertexAttrib4bv;
    PFNGLVERTEXATTRIB4DPROC                              pGLVertexAttrib4d;
    PFNGLVERTEXATTRIB4DVPROC                             pGLVertexAttrib4dv;
    PFNGLVERTEXATTRIB4FPROC                              pGLVertexAttrib4f;
    PFNGLVERTEXATTRIB4FVPROC                             pGLVertexAttrib4fv;
    PFNGLVERTEXATTRIB4IVPROC                             pGLVertexAttrib4iv;
    PFNGLVERTEXATTRIB4SPROC                              pGLVertexAttrib4s;
    PFNGLVERTEXATTRIB4SVPROC                             pGLVertexAttrib4sv;
    PFNGLVERTEXATTRIB4UBVPROC                            pGLVertexAttrib4ubv;
    PFNGLVERTEXATTRIB4UIVPROC                            pGLVertexAttrib4uiv;
    PFNGLVERTEXATTRIB4USVPROC                            pGLVertexAttrib4usv;
    PFNGLVERTEXATTRIBDIVISORPROC                         pGLVertexAttribDivisor;
    PFNGLVERTEXATTRIBBINDINGPROC                         pGLVertexAttribBinding;
    PFNGLVERTEXATTRIBFORMATPROC                          pGLVertexAttribFormat;
    PFNGLVERTEXATTRIBIFORMATPROC                         pGLVertexAttribIFormat;
    PFNGLVERTEXATTRIBLFORMATPROC                         pGLVertexAttribLFormat;
    PFNGLVERTEXATTRIBI1IPROC                             pGLVertexAttribI1i;
    PFNGLVERTEXATTRIBI1UIPROC                            pGLVertexAttribI1ui;
    PFNGLVERTEXATTRIBI1IVPROC                            pGLVertexAttribI1iv;
    PFNGLVERTEXATTRIBI1UIVPROC                           pGLVertexAttribI1uiv;
    PFNGLVERTEXATTRIBI2IPROC                             pGLVertexAttribI2i;
    PFNGLVERTEXATTRIBI2IVPROC                            pGLVertexAttribI2iv;
    PFNGLVERTEXATTRIBI2UIPROC                            pGLVertexAttribI2ui;
    PFNGLVERTEXATTRIBI2UIVPROC                           pGLVertexAttribI2uiv;
    PFNGLVERTEXATTRIBI3IPROC                             pGLVertexAttribI3i;
    PFNGLVERTEXATTRIBI3IVPROC                            pGLVertexAttribI3iv;
    PFNGLVERTEXATTRIBI3UIPROC                            pGLVertexAttribI3ui;
    PFNGLVERTEXATTRIBI3UIVPROC                           pGLVertexAttribI3uiv;
    PFNGLVERTEXATTRIBI4BVPROC                            pGLVertexAttribI4bv;
    PFNGLVERTEXATTRIBI4IPROC                             pGLVertexAttribI4i;
    PFNGLVERTEXATTRIBI4IVPROC                            pGLVertexAttribI4iv;
    PFNGLVERTEXATTRIBI4SVPROC                            pGLVertexAttribI4sv;
    PFNGLVERTEXATTRIBI4UBVPROC                           pGLVertexAttribI4ubv;
    PFNGLVERTEXATTRIBI4UIPROC                            pGLVertexAttribI4ui;
    PFNGLVERTEXATTRIBI4UIVPROC                           pGLVertexAttribI4uiv;
    PFNGLVERTEXATTRIBI4USVPROC                           pGLVertexAttribI4usv;
    PFNGLVERTEXATTRIBIPOINTERPROC                        pGLVertexAttribIPointer;
    PFNGLVERTEXATTRIBPOINTERPROC                         pGLVertexAttribPointer;
    PFNGLVERTEXBINDINGDIVISORPROC                        pGLVertexBindingDivisor;
    PFNGLVIEWPORTPROC                                    pGLViewport;
    PFNGLVIEWPORTINDEXEDFVPROC                           pGLViewportIndexedfv;
} ogl_context_gl_entrypoints;

typedef struct
{
    PFNGLACTIVETEXTUREPROC                               pGLActiveTexture;
    PFNGLBEGINTRANSFORMFEEDBACKPROC                      pGLBeginTransformFeedback;
    PFNGLBINDBUFFERPROC                                  pGLBindBuffer;
    PFNGLBINDBUFFERBASEPROC                              pGLBindBufferBase;
    PFNGLBINDBUFFERRANGEPROC                             pGLBindBufferRange;
    PFNGLBINDFRAMEBUFFERPROC                             pGLBindFramebuffer;
    PFNGLBINDRENDERBUFFERPROC                            pGLBindRenderbuffer;
    PFNGLBINDSAMPLERPROC                                 pGLBindSampler;
    PFNGLBINDTEXTUREPROC                                 pGLBindTexture;
    PFNGLBINDVERTEXARRAYPROC                             pGLBindVertexArray;
    PFNGLBINDVERTEXBUFFERPROC                            pGLBindVertexBuffer;
    PFNGLBLENDCOLORPROC                                  pGLBlendColor;
    PFNGLBLENDEQUATIONPROC                               pGLBlendEquation;
    PFNGLBLENDEQUATIONSEPARATEPROC                       pGLBlendEquationSeparate;
    PFNGLBLENDFUNCPROC                                   pGLBlendFunc;
    PFNGLBLENDFUNCSEPARATEPROC                           pGLBlendFuncSeparate;
    PFNGLBLITFRAMEBUFFERPROC                             pGLBlitFramebuffer;
    PFNGLBUFFERDATAPROC                                  pGLBufferData;
    PFNGLBUFFERSUBDATAPROC                               pGLBufferSubData;
    PFNGLCLEARPROC                                       pGLClear;
    PFNGLCLEARBUFFERDATAPROC                             pGLClearBufferData;
    PFNGLCLEARBUFFERSUBDATAPROC                          pGLClearBufferSubData;
    PFNGLCLEARCOLORPROC                                  pGLClearColor;
    PFNGLCLEARDEPTHPROC                                  pGLClearDepth;
    PFNGLCLEARDEPTHFPROC                                 pGLClearDepthf;
    PFNGLCOLORMASKPROC                                   pGLColorMask;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC                     pGLCompressedTexSubImage3D;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC                     pGLCompressedTexSubImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC                     pGLCompressedTexSubImage1D;
    PFNGLCOPYBUFFERSUBDATAPROC                           pGLCopyBufferSubData;
    PFNGLCOPYTEXSUBIMAGE1DPROC                           pGLCopyTexSubImage1D;
    PFNGLCOPYTEXSUBIMAGE2DPROC                           pGLCopyTexSubImage2D;
    PFNGLCOPYTEXSUBIMAGE3DPROC                           pGLCopyTexSubImage3D;
    PFNGLCULLFACEPROC                                    pGLCullFace;
    PFNGLDELETEBUFFERSPROC                               pGLDeleteBuffers;
    PFNGLDELETERENDERBUFFERSPROC                         pGLDeleteRenderbuffers;
    PFNGLDELETETEXTURESPROC                              pGLDeleteTextures;
    PFNGLDELETEVERTEXARRAYSPROC                          pGLDeleteVertexArrays;
    PFNGLDEPTHFUNCPROC                                   pGLDepthFunc;
    PFNGLDEPTHMASKPROC                                   pGLDepthMask;
    PFNGLDISABLEPROC                                     pGLDisable;
    PFNGLDISABLEIPROC                                    pGLDisablei;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC                    pGLDisableVertexAttribArray;
    PFNGLDISPATCHCOMPUTEPROC                             pGLDispatchCompute;
    PFNGLDISPATCHCOMPUTEINDIRECTPROC                     pGLDispatchComputeIndirect;
    PFNGLDRAWARRAYSPROC                                  pGLDrawArrays;
    PFNGLDRAWARRAYSINDIRECTPROC                          pGLDrawArraysIndirect;
    PFNGLDRAWARRAYSINSTANCEDPROC                         pGLDrawArraysInstanced;
    PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC             pGLDrawArraysInstancedBaseInstance;
    PFNGLDRAWBUFFERPROC                                  pGLDrawBuffer;
    PFNGLDRAWBUFFERSPROC                                 pGLDrawBuffers;
    PFNGLDRAWELEMENTSPROC                                pGLDrawElements;
    PFNGLDRAWELEMENTSINSTANCEDPROC                       pGLDrawElementsInstanced;
    PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC           pGLDrawElementsInstancedBaseInstance;
    PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC             pGLDrawElementsInstancedBaseVertex;
    PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC pGLDrawElementsInstancedBaseVertexBaseInstance;
    PFNGLDRAWRANGEELEMENTSPROC                           pGLDrawRangeElements;
    PFNGLDRAWTRANSFORMFEEDBACKPROC                       pGLDrawTransformFeedback;
    PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC              pGLDrawTransformFeedbackInstanced;
    PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC        pGLDrawTransformFeedbackStreamInstanced;
    PFNGLENABLEPROC                                      pGLEnable;
    PFNGLENABLEIPROC                                     pGLEnablei;
    PFNGLENABLEVERTEXATTRIBARRAYPROC                     pGLEnableVertexAttribArray;
    PFNGLFINISHPROC                                      pGLFinish;
    PFNGLFRAMEBUFFERPARAMETERIPROC                       pGLFramebufferParameteri;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC                     pGLFramebufferRenderbuffer;
    PFNGLFRAMEBUFFERTEXTUREPROC                          pGLFramebufferTexture;
    PFNGLFRAMEBUFFERTEXTURE1DPROC                        pGLFramebufferTexture1D;
    PFNGLFRAMEBUFFERTEXTURE2DPROC                        pGLFramebufferTexture2D;
    PFNGLFRAMEBUFFERTEXTURE3DPROC                        pGLFramebufferTexture3D;
    PFNGLFRAMEBUFFERTEXTURELAYERPROC                     pGLFramebufferTextureLayer;
    PFNGLFRONTFACEPROC                                   pGLFrontFace;
    PFNGLGENERATEMIPMAPPROC                              pGLGenerateMipmap;
    PFNGLGENTEXTURESPROC                                 pGLGenTextures;
    PFNGLGENVERTEXARRAYSPROC                             pGLGenVertexArrays;
    PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC              pGLGetActiveAtomicCounterBufferiv;
    PFNGLGETBOOLEANI_VPROC                               pGLGetBooleani_v;
    PFNGLGETBOOLEANVPROC                                 pGLGetBooleanv;
    PFNGLGETBUFFERPARAMETERI64VPROC                      pGLGetBufferParameteri64v;
    PFNGLGETBUFFERPARAMETERIVPROC                        pGLGetBufferParameteriv;
    PFNGLGETBUFFERPOINTERVPROC                           pGLGetBufferPointerv;
    PFNGLGETBUFFERSUBDATAPROC                            pGLGetBufferSubData;
    PFNGLGETCOMPRESSEDTEXIMAGEPROC                       pGLGetCompressedTexImage;
    PFNGLGETDOUBLEVPROC                                  pGLGetDoublev;
    PFNGLGETFLOATVPROC                                   pGLGetFloatv;
    PFNGLGETFRAMEBUFFERPARAMETERIVPROC                   pGLGetFramebufferParameteriv;
    PFNGLGETINTEGER64I_VPROC                             pGLGetInteger64i_v;
    PFNGLGETINTEGERI_VPROC                               pGLGetIntegeri_v;
    PFNGLGETINTEGERVPROC                                 pGLGetIntegerv;
    PFNGLGETINTERNALFORMATIVPROC                         pGLGetInternalformativ;
    PFNGLGETRENDERBUFFERPARAMETERIVPROC                  pGLGetRenderbufferParameteriv;
    PFNGLGETSAMPLERPARAMETERFVPROC                       pGLGetSamplerParameterfv;
    PFNGLGETSAMPLERPARAMETERIVPROC                       pGLGetSamplerParameteriv;
    PFNGLGETSAMPLERPARAMETERIIVPROC                      pGLGetSamplerParameterIiv;
    PFNGLGETSAMPLERPARAMETERIUIVPROC                     pGLGetSamplerParameterIuiv;
    PFNGLGETTEXPARAMETERIIVPROC                          pGLGetTexParameterIiv;
    PFNGLGETTEXPARAMETERIUIVPROC                         pGLGetTexParameterIuiv;
    PFNGLGETVERTEXATTRIBDVPROC                           pGLGetVertexAttribdv;
    PFNGLGETVERTEXATTRIBFVPROC                           pGLGetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIVPROC                           pGLGetVertexAttribiv;
    PFNGLGETVERTEXATTRIBIIVPROC                          pGLGetVertexAttribIiv;
    PFNGLGETVERTEXATTRIBIUIVPROC                         pGLGetVertexAttribIuiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC                     pGLGetVertexAttribPointerv;
    PFNGLINVALIDATEFRAMEBUFFERPROC                       pGLInvalidateFramebuffer;
    PFNGLINVALIDATESUBFRAMEBUFFERPROC                    pGLInvalidateSubFramebuffer;
    PFNGLLINEWIDTHPROC                                   pGLLineWidth;
    PFNGLMAPBUFFERPROC                                   pGLMapBuffer;
    PFNGLMAPBUFFERRANGEPROC                              pGLMapBufferRange;
    PFNGLMEMORYBARRIERPROC                               pGLMemoryBarrier;
    PFNGLMINSAMPLESHADINGPROC                            pGLMinSampleShading;
    PFNGLMULTIDRAWARRAYSPROC                             pGLMultiDrawArrays;
    PFNGLMULTIDRAWARRAYSINDIRECTPROC                     pGLMultiDrawArraysIndirect;
    PFNGLMULTIDRAWELEMENTSPROC                           pGLMultiDrawElements;
    PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC                 pGLMultiDrawElementsBaseVertex;
    PFNGLMULTIDRAWELEMENTSINDIRECTPROC                   pGLMultiDrawElementsIndirect;
    PFNGLPATCHPARAMETERIPROC                             pGLPatchParameteri;
    PFNGLPOLYGONMODEPROC                                 pGLPolygonMode;
    PFNGLPOLYGONOFFSETPROC                               pGLPolygonOffset;
    PFNGLREADBUFFERPROC                                  pGLReadBuffer;
    PFNGLREADPIXELSPROC                                  pGLReadPixels;
    PFNGLRENDERBUFFERSTORAGEPROC                         pGLRenderbufferStorage;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC              pGLRenderbufferStorageMultisample;
    PFNGLRESUMETRANSFORMFEEDBACKPROC                     pGLResumeTransformFeedback;
    PFNGLSCISSORPROC                                     pGLScissor;
    PFNGLSCISSORINDEXEDVPROC                             pGLScissorIndexedv;
    PFNGLSHADERSTORAGEBLOCKBINDINGPROC                   pGLShaderStorageBlockBinding;
    PFNGLTEXPARAMETERIIVPROC                             pGLTexParameterIiv;
    PFNGLTEXPARAMETERIUIVPROC                            pGLTexParameterIuiv;
    PFNGLTEXSTORAGE1DPROC                                pGLTexStorage1D;
    PFNGLTEXSTORAGE2DPROC                                pGLTexStorage2D;
    PFNGLTEXSTORAGE2DMULTISAMPLEPROC                     pGLTexStorage2DMultisample;
    PFNGLTEXSTORAGE3DPROC                                pGLTexStorage3D;
    PFNGLTEXSTORAGE3DMULTISAMPLEPROC                     pGLTexStorage3DMultisample;
    PFNGLTEXSUBIMAGE1DPROC                               pGLTexSubImage1D;
    PFNGLTEXSUBIMAGE2DPROC                               pGLTexSubImage2D;
    PFNGLTEXSUBIMAGE3DPROC                               pGLTexSubImage3D;
    PFNGLTEXTUREVIEWPROC                                 pGLTextureView;
    PFNGLUNIFORMBLOCKBINDINGPROC                         pGLUniformBlockBinding;
    PFNGLUNMAPBUFFERPROC                                 pGLUnmapBuffer;
    PFNGLUSEPROGRAMPROC                                  pGLUseProgram;
    PFNGLVERTEXATTRIBBINDINGPROC                         pGLVertexAttribBinding;
    PFNGLVERTEXATTRIBFORMATPROC                          pGLVertexAttribFormat;
    PFNGLVERTEXATTRIBIFORMATPROC                         pGLVertexAttribIFormat;
    PFNGLVERTEXATTRIBIPOINTERPROC                        pGLVertexAttribIPointer;
    PFNGLVERTEXATTRIBLFORMATPROC                         pGLVertexAttribLFormat;
    PFNGLVERTEXATTRIBPOINTERPROC                         pGLVertexAttribPointer;
    PFNGLVERTEXBINDINGDIVISORPROC                        pGLVertexBindingDivisor;
    PFNGLVIEWPORTPROC                                    pGLViewport;
    PFNGLVIEWPORTINDEXEDFVPROC                           pGLViewportIndexedfv;

    /* DSA */
    PFNGLBINDMULTITEXTUREEXTPROC             pGLBindMultiTextureEXT;
    PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC  pGLCheckNamedFramebufferStatusEXT;
    PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC     pGLCompressedTextureImage3DEXT;
    PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC     pGLCompressedTextureImage2DEXT;
    PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC     pGLCompressedTextureImage1DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC  pGLCompressedTextureSubImage3DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC  pGLCompressedTextureSubImage2DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC  pGLCompressedTextureSubImage1DEXT;
    PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC        pGLCopyTextureSubImage1DEXT;
    PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC        pGLCopyTextureSubImage2DEXT;
    PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC        pGLCopyTextureSubImage3DEXT;
    PFNGLFRAMEBUFFERREADBUFFEREXTPROC        pGLFramebufferReadBufferEXT;
    PFNGLGENERATETEXTUREMIPMAPEXTPROC        pGLGenerateTextureMipmapEXT;
    PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC    pGLGetCompressedTextureImageEXT;
    PFNGLGETTEXTUREIMAGEEXTPROC              pGLGetTextureImageEXT;
    PFNGLGETTEXTUREPARAMETERIIVEXTPROC       pGLGetTextureParameterIiv;
    PFNGLGETTEXTUREPARAMETERIUIVEXTPROC      pGLGetTextureParameterIuiv;

    PFNGLTEXTUREBUFFEREXTPROC               pGLTextureBufferEXT;
    PFNGLTEXTUREIMAGE1DEXTPROC              pGLTextureImage1DEXT;
    PFNGLTEXTUREIMAGE2DEXTPROC              pGLTextureImage2DEXT;
    PFNGLTEXTUREIMAGE3DEXTPROC              pGLTextureImage3DEXT;
    PFNGLTEXTURESUBIMAGE1DEXTPROC           pGLTextureSubImage1DEXT;
    PFNGLTEXTURESUBIMAGE2DEXTPROC           pGLTextureSubImage2DEXT;
    PFNGLTEXTURESUBIMAGE3DEXTPROC           pGLTextureSubImage3DEXT;

    PFNGLMAPNAMEDBUFFEREXTPROC            pGLMapNamedBufferEXT;
    PFNGLNAMEDBUFFERDATAEXTPROC           pGLNamedBufferDataEXT;

    PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC  pGLVertexArrayVertexAttribOffsetEXT;
    PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC pGLVertexArrayVertexAttribIOffsetEXT;

    /* GL_ARB_buffer_storage */
    PFNGLBUFFERSTORAGEPROC         pGLBufferStorage;
    PFNGLNAMEDBUFFERSTORAGEEXTPROC pGLNamedBufferStorageEXT;

    /* GL_ARB_multi_bind */
    PFNGLBINDBUFFERSBASEPROC  pGLBindBuffersBase;
    PFNGLBINDBUFFERSRANGEPROC pGLBindBuffersRange;
    PFNGLBINDSAMPLERSPROC     pGLBindSamplers;
    PFNGLBINDTEXTURESPROC     pGLBindTextures;

    /* GL_ARB_texture_buffer_range */
    PFNGLTEXTUREBUFFERRANGEEXTPROC pGLTextureBufferRangeEXT;

    PFNGLGETTEXIMAGEPROC            pGLGetTexImage;
    PFNGLGETTEXLEVELPARAMETERFVPROC pGLGetTexLevelParameterfv;
    PFNGLGETTEXLEVELPARAMETERIVPROC pGLGetTexLevelParameteriv;
    PFNGLGETTEXPARAMETERFVPROC      pGLGetTexParameterfv;
    PFNGLGETTEXPARAMETERIVPROC      pGLGetTexParameteriv;
    PFNGLTEXBUFFERPROC              pGLTexBuffer;
    PFNGLTEXBUFFERRANGEPROC         pGLTexBufferRange;
    PFNGLTEXPARAMETERFPROC          pGLTexParameterf;
    PFNGLTEXPARAMETERFVPROC         pGLTexParameterfv;
    PFNGLTEXPARAMETERIPROC          pGLTexParameteri;
    PFNGLTEXPARAMETERIVPROC         pGLTexParameteriv;
} ogl_context_gl_entrypoints_private;

typedef struct
{

    PFNGLBUFFERSTORAGEPROC pGLBufferStorage;

} ogl_context_gl_entrypoints_arb_buffer_storage;

typedef struct
{
    PFNGLCLEARTEXIMAGEPROC    pGLClearTexImage;
    PFNGLCLEARTEXSUBIMAGEPROC pGLClearTexSubImage;

} ogl_context_gl_entrypoints_arb_clear_texture;

typedef struct
{
    PFNGLDISPATCHCOMPUTEPROC         pGLDispatchCompute;
    PFNGLDISPATCHCOMPUTEINDIRECTPROC pGLDispatchComputeIndirect;
} ogl_context_gl_entrypoints_arb_compute_shader;

typedef struct
{
    PFNGLBINDBUFFERSBASEPROC   pGLBindBuffersBase;
    PFNGLBINDBUFFERSRANGEPROC  pGLBindBuffersRange;
    PFNGLBINDIMAGETEXTURESPROC pGLBindImageTextures;
    PFNGLBINDSAMPLERSPROC      pGLBindSamplers;
    PFNGLBINDTEXTURESPROC      pGLBindTextures;
} ogl_context_gl_entrypoints_arb_multi_bind;

typedef struct
{
    PFNGLBUFFERPAGECOMMITMENTARBPROC      pGLBufferPageCommitmentARB;
    PFNGLNAMEDBUFFERPAGECOMMITMENTEXTPROC pGLNamedBufferPageCommitmentEXT;
} ogl_context_gl_entrypoints_arb_sparse_buffer;

typedef struct
{
    GLint sparse_buffer_page_size;
} ogl_context_gl_limits_arb_sparse_buffer;

typedef struct
{
    float texture_max_anisotropy_ext;
} ogl_context_gl_limits_ext_texture_filter_anisotropic;

typedef struct
{
    /* NOTE: EXT_direct_state_access support will be superseded with ARB_direct_state_access
     *       in the near future.
     *
     *       If you need to add a new entry-point, please ensure these come from core GL 4.2
     *       feature-set and carry no vendor-specific dependencies.
     */
    PFNGLBINDMULTITEXTUREEXTPROC                         pGLBindMultiTextureEXT;
    PFNGLCHECKNAMEDFRAMEBUFFERSTATUSEXTPROC              pGLCheckNamedFramebufferStatusEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC              pGLCompressedTextureSubImage1DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC              pGLCompressedTextureSubImage2DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC              pGLCompressedTextureSubImage3DEXT;
    PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC                    pGLCopyTextureSubImage1DEXT;
    PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC                    pGLCopyTextureSubImage2DEXT;
    PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC                    pGLCopyTextureSubImage3DEXT;
    PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC                 pGLDisableVertexArrayAttribEXT;
    PFNGLENABLEVERTEXARRAYATTRIBEXTPROC                  pGLEnableVertexArrayAttribEXT;
    PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC                    pGLFramebufferDrawBufferEXT;
    PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC                   pGLFramebufferDrawBuffersEXT;
    PFNGLFRAMEBUFFERREADBUFFEREXTPROC                    pGLFramebufferReadBufferEXT;
    PFNGLGENERATETEXTUREMIPMAPEXTPROC                    pGLGenerateTextureMipmapEXT;
    PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC                pGLGetCompressedTextureImageEXT;
    PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC                pGLGetFramebufferParameterivEXT;
    PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC                pGLGetNamedBufferParameterivEXT;
    PFNGLGETNAMEDBUFFERPOINTERVEXTPROC                   pGLGetNamedBufferPointervEXT;
    PFNGLGETNAMEDBUFFERSUBDATAEXTPROC                    pGLGetNamedBufferSubDataEXT;
    PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC pGLGetNamedFramebufferAttachmentParameterivEXT;
    PFNGLGETNAMEDPROGRAMIVEXTPROC                        pGLGetNamedProgramivEXT;
    PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC          pGLGetNamedRenderbufferParameterivEXT;
    PFNGLGETTEXTUREIMAGEEXTPROC                          pGLGetTextureImageEXT;
    PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC               pGLGetTextureLevelParameterfvEXT;
    PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC               pGLGetTextureLevelParameterivEXT;
    PFNGLGETTEXTUREPARAMETERIVEXTPROC                    pGLGetTextureParameterivEXT;
    PFNGLNAMEDBUFFERDATAEXTPROC                          pGLNamedBufferDataEXT;
    PFNGLNAMEDBUFFERSUBDATAEXTPROC                       pGLNamedBufferSubDataEXT;
    PFNGLNAMEDBUFFERSTORAGEEXTPROC                       pGLNamedBufferStorageEXT;
    PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC                   pGLNamedCopyBufferSubDataEXT;
    PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC             pGLNamedFramebufferRenderbufferEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC                pGLNamedFramebufferTexture1DEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC                pGLNamedFramebufferTexture2DEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC                pGLNamedFramebufferTexture3DEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC                  pGLNamedFramebufferTextureEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC             pGLNamedFramebufferTextureLayerEXT;
    PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC                 pGLNamedRenderbufferStorageEXT;
    PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC      pGLNamedRenderbufferStorageMultisampleEXT;
    PFNGLTEXTUREBUFFEREXTPROC                            pGLTextureBufferEXT;
    PFNGLTEXTUREBUFFERRANGEEXTPROC                       pGLTextureBufferRangeEXT;
    PFNGLTEXTUREPARAMETERFEXTPROC                        pGLTextureParameterfEXT;
    PFNGLTEXTUREPARAMETERFVEXTPROC                       pGLTextureParameterfvEXT;
    PFNGLTEXTUREPARAMETERIEXTPROC                        pGLTextureParameteriEXT;
    PFNGLTEXTUREPARAMETERIVEXTPROC                       pGLTextureParameterivEXT;
    PFNGLTEXTUREPARAMETERIIVEXTPROC                      pGLTextureParameterIivEXT;
    PFNGLTEXTUREPARAMETERIUIVEXTPROC                     pGLTextureParameterIuivEXT;
    PFNGLTEXTURESTORAGE1DEXTPROC                         pGLTextureStorage1DEXT;
    PFNGLTEXTURESTORAGE2DEXTPROC                         pGLTextureStorage2DEXT;
    PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC              pGLTextureStorage2DMultisampleEXT;
    PFNGLTEXTURESTORAGE3DEXTPROC                         pGLTextureStorage3DEXT;
    PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC              pGLTextureStorage3DMultisampleEXT;
    PFNGLTEXTURESUBIMAGE1DEXTPROC                        pGLTextureSubImage1DEXT;
    PFNGLTEXTURESUBIMAGE2DEXTPROC                        pGLTextureSubImage2DEXT;
    PFNGLTEXTURESUBIMAGE3DEXTPROC                        pGLTextureSubImage3DEXT;
    PFNGLUNMAPNAMEDBUFFEREXTPROC                         pGLUnmapNamedBufferEXT;
    PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC            pGLVertexArrayVertexAttribOffsetEXT;
    PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC           pGLVertexArrayVertexAttribIOffsetEXT;
} ogl_context_gl_entrypoints_ext_direct_state_access;


/** Various predefined shader generators. */
DECLARE_HANDLE(shaders_fragment_convolution3x3);
DECLARE_HANDLE(shaders_fragment_laplacian);
DECLARE_HANDLE(shaders_fragment_rgb_to_Yxy);
DECLARE_HANDLE(shaders_fragment_saturate);
DECLARE_HANDLE(shaders_fragment_sobel);
DECLARE_HANDLE(shaders_fragment_static);
DECLARE_HANDLE(shaders_fragment_texture2D_filmic);
DECLARE_HANDLE(shaders_fragment_texture2D_filmic_customizable);
DECLARE_HANDLE(shaders_fragment_texture2D_linear);
DECLARE_HANDLE(shaders_fragment_texture2D_plain);
DECLARE_HANDLE(shaders_fragment_texture2D_reinhardt);
DECLARE_HANDLE(shaders_fragment_uber);
DECLARE_HANDLE(shaders_fragment_Yxy_to_rgb);
DECLARE_HANDLE(shaders_vertex_combinedmvp_generic);
DECLARE_HANDLE(shaders_vertex_combinedmvp_simplified_twopoint);
DECLARE_HANDLE(shaders_vertex_fullscreen);
DECLARE_HANDLE(shaders_vertex_uber);

/** OpenGL context handle */
DECLARE_HANDLE(ogl_context);

#ifdef _WIN32
    typedef HGLRC ogl_context_handle;

    DECLARE_HANDLE(ogl_context_win32);
#else
    typedef GLXContext ogl_context_handle;

    DECLARE_HANDLE(ogl_context_linux);
#endif

/** TODO.
 *
 *  @param context           TODO
 *  @param frame_time        TODO
 *  @param rendering_area_px_topdown [0]: x1 of the rendering area (in pixels)
 *                                   [1]: y1 of the rendering area (in pixels)
 *                                   [2]: x2 of the rendering area (in pixels)
 *                                   [3]: y2 of the rendering area (in pixels)
 *  @param callback_user_arg TODO
 */
typedef void (*PFNOGLPIPELINECALLBACKPROC)(ral_context context,
                                           uint32_t    n_frame,
                                           system_time frame_time,
                                           const int*  rendering_area_px_topdown,
                                           void*       callback_user_arg);

#endif /* OGL_TYPES_H */
