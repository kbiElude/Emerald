/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef OGL_TYPES_H
#define OGL_TYPES_H

#include "ogl/gl3.h"
#include "system/system_time.h"
#include "system/system_types.h"

/* Primitive renderer */
DECLARE_HANDLE(ogl_primitive_renderer);

typedef unsigned int ogl_primitive_renderer_dataset_id;

typedef enum
{
    OGL_PRIMITIVE_TYPE_LINE_LOOP                = GL_LINE_LOOP,
    OGL_PRIMITIVE_TYPE_LINE_STRIP               = GL_LINE_STRIP,
    OGL_PRIMITIVE_TYPE_LINE_STRIP_ADJACENCY     = GL_LINE_STRIP_ADJACENCY,
    OGL_PRIMITIVE_TYPE_LINES                    = GL_LINES,
    OGL_PRIMITIVE_TYPE_LINES_ADJACENCY          = GL_LINES_ADJACENCY,
    OGL_PRIMITIVE_TYPE_POINTS                   = GL_POINTS,
    OGL_PRIMITIVE_TYPE_TRIANGLE_FAN             = GL_TRIANGLE_FAN,
    OGL_PRIMITIVE_TYPE_TRIANGLE_STRIP           = GL_TRIANGLE_STRIP,
    OGL_PRIMITIVE_TYPE_TRIANGLE_STRIP_ADJACENCY = GL_TRIANGLE_STRIP_ADJACENCY,
    OGL_PRIMITIVE_TYPE_TRIANGLES                = GL_TRIANGLES,
    OGL_PRIMITIVE_TYPE_TRIANGLES_ADJACENCY      = GL_TRIANGLES_ADJACENCY,

    OGL_PRIMITIVE_TYPE_UNDEFINED
} ogl_primitive_type;

/* Curve renderer */
DECLARE_HANDLE(ogl_curve_renderer);

typedef unsigned int ogl_curve_item_id;

/* Scene renderer */
DECLARE_HANDLE(ogl_scene_renderer);

/** Sampler manager handle */
DECLARE_HANDLE(ogl_sampler);
DECLARE_HANDLE(ogl_samplers);

/** Texture handle */
DECLARE_HANDLE(ogl_texture);
DECLARE_HANDLE(ogl_textures);

typedef                 PFNGLACTIVETEXTUREPROC                          PFNWRAPPEDGLACTIVETEXTUREPROC;
typedef                 PFNGLCOMPRESSEDTEXIMAGE1DPROC                   PFNWRAPPEDGLCOMPRESSEDTEXIMAGE1DPROC;
typedef                 PFNGLCOMPRESSEDTEXIMAGE2DPROC                   PFNWRAPPEDGLCOMPRESSEDTEXIMAGE2DPROC;
typedef                 PFNGLCOMPRESSEDTEXIMAGE3DPROC                   PFNWRAPPEDGLCOMPRESSEDTEXIMAGE3DPROC;
typedef                 PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC                PFNWRAPPEDGLCOMPRESSEDTEXSUBIMAGE1DPROC;
typedef                 PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC                PFNWRAPPEDGLCOMPRESSEDTEXSUBIMAGE2DPROC;
typedef                 PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC                PFNWRAPPEDGLCOMPRESSEDTEXSUBIMAGE3DPROC;
typedef                 PFNGLCOPYTEXIMAGE1DPROC                         PFNWRAPPEDGLCOPYTEXIMAGE1DPROC;
typedef                 PFNGLCOPYTEXIMAGE2DPROC                         PFNWRAPPEDGLCOPYTEXIMAGE2DPROC;
typedef                 PFNGLCOPYTEXSUBIMAGE1DPROC                      PFNWRAPPEDGLCOPYTEXSUBIMAGE1DPROC;
typedef                 PFNGLCOPYTEXSUBIMAGE2DPROC                      PFNWRAPPEDGLCOPYTEXSUBIMAGE2DPROC;
typedef                 PFNGLCOPYTEXSUBIMAGE3DPROC                      PFNWRAPPEDGLCOPYTEXSUBIMAGE3DPROC;
typedef void (APIENTRY *PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC)           (GLuint vaobj, GLuint index);
typedef void (APIENTRY *PFNGLENABLEVERTEXARRAYATTRIBEXTPROC)            (GLuint vaobj, GLuint index);
typedef                 PFNGLGETCOMPRESSEDTEXIMAGEPROC                  PFNWRAPPEDGLGETCOMPRESSEDTEXIMAGEPROC;
typedef                 PFNGLGETTEXIMAGEPROC                            PFNWRAPPEDGLGETTEXIMAGEPROC;
typedef                 PFNGLGETTEXLEVELPARAMETERFVPROC                 PFNWRAPPEDGLGETTEXLEVELPARAMETERFVPROC;
typedef                 PFNGLGETTEXLEVELPARAMETERIVPROC                 PFNWRAPPEDGLGETTEXLEVELPARAMETERIVPROC;
typedef                 PFNGLGETTEXPARAMETERFVPROC                      PFNWRAPPEDGLGETTEXPARAMETERFVPROC;
typedef                 PFNGLGETTEXPARAMETERIVPROC                      PFNWRAPPEDGLGETTEXPARAMETERIVPROC;
typedef                 PFNGLGETTEXPARAMETERIIVPROC                     PFNWRAPPEDGLGETTEXPARAMETERIIVPROC;
typedef                 PFNGLGETTEXPARAMETERIUIVPROC                    PFNWRAPPEDGLGETTEXPARAMETERIUIVPROC;
typedef                 PFNGLTEXBUFFERPROC                              PFNWRAPPEDGLTEXBUFFERPROC;
typedef                 PFNGLTEXIMAGE1DPROC                             PFNWRAPPEDGLTEXIMAGE1DPROC;
typedef                 PFNGLTEXIMAGE2DPROC                             PFNWRAPPEDGLTEXIMAGE2DPROC;
typedef                 PFNGLTEXIMAGE3DPROC                             PFNWRAPPEDGLTEXIMAGE3DPROC;
typedef                 PFNGLTEXPARAMETERFPROC                          PFNWRAPPEDGLTEXPARAMETERFPROC;
typedef                 PFNGLTEXPARAMETERFVPROC                         PFNWRAPPEDGLTEXPARAMETERFVPROC;
typedef                 PFNGLTEXPARAMETERIPROC                          PFNWRAPPEDGLTEXPARAMETERIPROC;
typedef                 PFNGLTEXPARAMETERIVPROC                         PFNWRAPPEDGLTEXPARAMETERIVPROC;
typedef                 PFNGLTEXPARAMETERIIVPROC                        PFNWRAPPEDGLTEXPARAMETERIIVPROC;
typedef                 PFNGLTEXPARAMETERIUIVPROC                       PFNWRAPPEDGLTEXPARAMETERIUIVPROC;
typedef                 PFNGLTEXSUBIMAGE1DPROC                          PFNWRAPPEDGLTEXSUBIMAGE1DPROC;
typedef                 PFNGLTEXSUBIMAGE2DPROC                          PFNWRAPPEDGLTEXSUBIMAGE2DPROC;
typedef                 PFNGLTEXSUBIMAGE3DPROC                          PFNWRAPPEDGLTEXSUBIMAGE3DPROC;
typedef void (APIENTRY *PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC)     (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride,       GLintptr offset);
typedef void (APIENTRY *PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC)      (GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset);
typedef void (APIENTRY* PFNWRAPPEDGLBINDMULTITEXTUREEXTPROC)            (GLenum texunit,      GLenum      target, ogl_texture texture);
typedef void (APIENTRY *PFNWRAPPEDGLBINDIMAGETEXTUREEXTPROC)            (GLuint index,        ogl_texture texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLint format);
typedef void (APIENTRY* PFNWRAPPEDGLBINDTEXTUREPROC)                    (GLenum target,       ogl_texture texture);
typedef void (APIENTRY* PFNWRAPPEDGLBINDTEXTURESPROC)                   (GLuint first,        GLsizei     count, ogl_texture* textures);
typedef void (APIENTRY* PFNWRAPPEDGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC)    (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *bits);
typedef void (APIENTRY* PFNWRAPPEDGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC)    (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *bits);
typedef void (APIENTRY* PFNWRAPPEDGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC)    (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *bits);
typedef void (APIENTRY* PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC) (ogl_texture texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *bits);
typedef void (APIENTRY* PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC) (ogl_texture texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *bits);
typedef void (APIENTRY* PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC) (ogl_texture texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *bits);
typedef void (APIENTRY *PFNWRAPPEDGLCOPYTEXTUREIMAGE1DEXTPROC)          (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (APIENTRY *PFNWRAPPEDGLCOPYTEXTUREIMAGE2DEXTPROC)          (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRY *PFNWRAPPEDGLCOPYTEXTURESUBIMAGE1DEXTPROC)       (ogl_texture texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRY *PFNWRAPPEDGLCOPYTEXTURESUBIMAGE2DEXTPROC)       (ogl_texture texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY *PFNWRAPPEDGLCOPYTEXTURESUBIMAGE3DEXTPROC)       (ogl_texture texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY* PFNWRAPPEDGLFRAMEBUFFERTEXTUREPROC)             (GLenum target, GLenum attachment, ogl_texture texture,   GLint       level);
typedef void (APIENTRY* PFNWRAPPEDGLFRAMEBUFFERTEXTURE1DPROC)           (GLenum target, GLenum attachment, GLenum      textarget, ogl_texture texture, GLint level);
typedef void (APIENTRY* PFNWRAPPEDGLFRAMEBUFFERTEXTURE2DPROC)           (GLenum target, GLenum attachment, GLenum      textarget, ogl_texture texture, GLint level);
typedef void (APIENTRY* PFNWRAPPEDGLFRAMEBUFFERTEXTURE3DPROC)           (GLenum target, GLenum attachment, GLenum      textarget, ogl_texture texture, GLint level, GLint layer);
typedef void (APIENTRY* PFNWRAPPEDGLFRAMEBUFFERTEXTURELAYERPROC)        (GLenum target, GLenum attachment, ogl_texture texture,   GLint       level,   GLint layer);
typedef void (APIENTRY* PFNWRAPPEDGLGENERATETEXTUREMIPMAPEXTPROC)       (ogl_texture texture, GLenum target);
typedef void (APIENTRY* PFNWRAPPEDGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC)   (ogl_texture texture, GLenum target, GLint lod, GLvoid *img);
typedef void (APIENTRY* PFNWRAPPEDGLGETTEXTUREIMAGEEXTPROC)             (ogl_texture texture, GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
typedef void (APIENTRY *PFNWRAPPEDGLGETTEXTUREPARAMETERFVEXTPROC)       (ogl_texture, GLenum target, GLint level, GLenum pname, GLfloat* params);
typedef void (APIENTRY *PFNWRAPPEDGLGETTEXTUREPARAMETERIVEXTPROC)       (ogl_texture, GLenum target, GLint level, GLenum pname, GLint* params);
typedef void (APIENTRY* PFNWRAPPEDGLGETTEXTUREPARAMETERIIVEXTPROC)      (ogl_texture texture, GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY* PFNWRAPPEDGLGETTEXTUREPARAMETERIUIVEXTPROC)     (ogl_texture texture, GLenum target, GLenum pname, GLuint *params);
typedef void (APIENTRY* PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTUREEXTPROC)     (GLuint framebuffer, GLenum attachment, ogl_texture texture, GLint level);
typedef void (APIENTRY* PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC) (GLuint framebuffer, GLenum attachment, ogl_texture texture, GLint level, GLenum face);
typedef void (APIENTRY* PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC)(GLuint framebuffer, GLenum attachment, ogl_texture texture, GLint level, GLint layer);
typedef void (APIENTRY* PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC)   (GLuint framebuffer, GLenum attachment, GLenum textarget, ogl_texture texture, GLint level);
typedef void (APIENTRY* PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC)   (GLuint framebuffer, GLenum attachment, GLenum textarget, ogl_texture texture, GLint level);
typedef void (APIENTRY* PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC)   (GLuint framebuffer, GLenum attachment, GLenum textarget, ogl_texture texture, GLint level, GLint zoffset);
typedef void (APIENTRY* PFNWRAPPEDGLTEXBUFFERRANGEPROC)                 (GLenum target,      GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRY* PFNWRAPPEDGLTEXSTORAGE1DPROC)                   (GLenum target,      GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (APIENTRY* PFNWRAPPEDGLTEXSTORAGE2DPROC)                   (GLenum target,      GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY* PFNWRAPPEDGLTEXSTORAGE2DMULTISAMPLEPROC)        (GLenum target,      GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRY* PFNWRAPPEDGLTEXSTORAGE3DPROC)                   (GLenum target,      GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY* PFNWRAPPEDGLTEXSTORAGE3DMULTISAMPLEPROC)        (GLenum target,      GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRY* PFNWRAPPEDGLTEXTUREBUFFEREXTPROC)               (ogl_texture texture, GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREIMAGE1DEXTPROC)              (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREIMAGE2DEXTPROC)              (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREIMAGE3DEXTPROC)              (ogl_texture texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREPARAMETERFEXTPROC)           (ogl_texture texture, GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREPARAMETERFVEXTPROC)          (ogl_texture texture, GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREPARAMETERIEXTPROC)           (ogl_texture texture, GLenum target, GLenum pname, GLint param);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREPARAMETERIIVEXTPROC)         (ogl_texture texture, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREPARAMETERIUIVEXTPROC)        (ogl_texture texture, GLenum target, GLenum pname, const GLuint *params);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTUREPARAMETERIVEXTPROC)          (ogl_texture texture, GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURERENDERBUFFEREXTPROC)         (ogl_texture texture, GLenum target, GLuint renderbuffer);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESTORAGE1DEXTPROC)            (ogl_texture texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESTORAGE2DEXTPROC)            (ogl_texture texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC) (ogl_texture texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESTORAGE3DEXTPROC)            (ogl_texture texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC) (ogl_texture texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESUBIMAGE1DEXTPROC)           (ogl_texture texture, GLenum target, GLint  level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESUBIMAGE2DEXTPROC)           (ogl_texture texture, GLenum target, GLint  level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *PFNWRAPPEDGLTEXTURESUBIMAGE3DEXTPROC)           (ogl_texture texture, GLenum target, GLint  level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY* PFNWRAPPEDGLTEXTUREBUFFERRANGEEXTPROC)          (ogl_texture texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);

/** Enumerator that tells type of the rendering context */
typedef enum
{
    OGL_CONTEXT_TYPE_ES,
    OGL_CONTEXT_TYPE_GL,

    OGL_CONTEXT_TYPE_UNDEFINED
} ogl_context_type;

/** Enumerator that describes allowed types for a program attribute */
typedef enum
{
    PROGRAM_ATTRIBUTE_TYPE_FLOAT             = GL_FLOAT,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_VEC2        = GL_FLOAT_VEC2,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_VEC3        = GL_FLOAT_VEC3,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_VEC4        = GL_FLOAT_VEC4,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT2        = GL_FLOAT_MAT2,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT3        = GL_FLOAT_MAT3,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT4        = GL_FLOAT_MAT4,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT2x3      = GL_FLOAT_MAT2x3,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT2x4      = GL_FLOAT_MAT2x4,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT3x2      = GL_FLOAT_MAT3x2,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT3x4      = GL_FLOAT_MAT3x4,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT4x2      = GL_FLOAT_MAT4x2,
    PROGRAM_ATTRIBUTE_TYPE_FLOAT_MAT4x3      = GL_FLOAT_MAT4x3,
    PROGRAM_ATTRIBUTE_TYPE_INT               = GL_INT, 
    PROGRAM_ATTRIBUTE_TYPE_INT_VEC2          = GL_INT_VEC2,
    PROGRAM_ATTRIBUTE_TYPE_INT_VEC3          = GL_INT_VEC3,
    PROGRAM_ATTRIBUTE_TYPE_INT_VEC4          = GL_INT_VEC4,
    PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT      = GL_UNSIGNED_INT,
    PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT_VEC2 = GL_UNSIGNED_INT_VEC2,
    PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT_VEC3 = GL_UNSIGNED_INT_VEC3,
    PROGRAM_ATTRIBUTE_TYPE_UNSIGNED_INT_VEC4 = GL_UNSIGNED_INT_VEC4,

    PROGRAM_ATTRIBUTE_TYPE_UNDEFINED
} ogl_program_attribute_type;

/** Enumerator that describes allowed types for a program uniform */
typedef enum
{
    PROGRAM_UNIFORM_TYPE_FLOAT                                     = GL_FLOAT,
    PROGRAM_UNIFORM_TYPE_FLOAT_VEC2                                = GL_FLOAT_VEC2,
    PROGRAM_UNIFORM_TYPE_FLOAT_VEC3                                = GL_FLOAT_VEC3,
    PROGRAM_UNIFORM_TYPE_FLOAT_VEC4                                = GL_FLOAT_VEC4,
    PROGRAM_UNIFORM_TYPE_INT                                       = GL_INT,
    PROGRAM_UNIFORM_TYPE_INT_VEC2                                  = GL_INT_VEC2,
    PROGRAM_UNIFORM_TYPE_INT_VEC3                                  = GL_INT_VEC3,
    PROGRAM_UNIFORM_TYPE_INT_VEC4                                  = GL_INT_VEC4,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT                              = GL_UNSIGNED_INT,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_VEC2                         = GL_UNSIGNED_INT_VEC2,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_VEC3                         = GL_UNSIGNED_INT_VEC3,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_VEC4                         = GL_UNSIGNED_INT_VEC4,
    PROGRAM_UNIFORM_TYPE_BOOL                                      = GL_BOOL,
    PROGRAM_UNIFORM_TYPE_BOOL_VEC2                                 = GL_BOOL_VEC2,
    PROGRAM_UNIFORM_TYPE_BOOL_VEC3                                 = GL_BOOL_VEC3,
    PROGRAM_UNIFORM_TYPE_BOOL_VEC4                                 = GL_BOOL_VEC4,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT2                                = GL_FLOAT_MAT2,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT3                                = GL_FLOAT_MAT3,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT4                                = GL_FLOAT_MAT4,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x3                              = GL_FLOAT_MAT2x3,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x4                              = GL_FLOAT_MAT2x4,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x2                              = GL_FLOAT_MAT3x2,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x4                              = GL_FLOAT_MAT3x4,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x2                              = GL_FLOAT_MAT4x2,
    PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x3                              = GL_FLOAT_MAT4x3,
    PROGRAM_UNIFORM_TYPE_SAMPLER_1D                                = GL_SAMPLER_1D,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D                                = GL_SAMPLER_2D,
    PROGRAM_UNIFORM_TYPE_SAMPLER_3D                                = GL_SAMPLER_3D,
    PROGRAM_UNIFORM_TYPE_SAMPLER_CUBE                              = GL_SAMPLER_CUBE,
    PROGRAM_UNIFORM_TYPE_SAMPLER_1D_SHADOW                         = GL_SAMPLER_1D_SHADOW,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_SHADOW                         = GL_SAMPLER_2D_SHADOW,
    PROGRAM_UNIFORM_TYPE_SAMPLER_1D_ARRAY                          = GL_SAMPLER_1D_ARRAY,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_ARRAY                          = GL_SAMPLER_2D_ARRAY,
    PROGRAM_UNIFORM_TYPE_SAMPLER_1D_ARRAY_SHADOW                   = GL_SAMPLER_1D_ARRAY_SHADOW,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_ARRAY_SHADOW                   = GL_SAMPLER_2D_ARRAY_SHADOW,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_MULTISAMPLE                    = GL_SAMPLER_2D_MULTISAMPLE,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY              = GL_SAMPLER_2D_MULTISAMPLE_ARRAY,
    PROGRAM_UNIFORM_TYPE_SAMPLER_CUBE_SHADOW                       = GL_SAMPLER_CUBE_SHADOW,
    PROGRAM_UNIFORM_TYPE_SAMPLER_BUFFER                            = GL_SAMPLER_BUFFER,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_RECT                           = GL_SAMPLER_2D_RECT,
    PROGRAM_UNIFORM_TYPE_SAMPLER_2D_RECT_SHADOW                    = GL_SAMPLER_2D_RECT_SHADOW,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_1D                            = GL_INT_SAMPLER_1D,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_2D                            = GL_INT_SAMPLER_2D,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_3D                            = GL_INT_SAMPLER_3D,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_CUBE                          = GL_INT_SAMPLER_CUBE,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_1D_ARRAY                      = GL_INT_SAMPLER_1D_ARRAY,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_2D_ARRAY                      = GL_INT_SAMPLER_2D_ARRAY,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_2D_MULTISAMPLE                = GL_INT_SAMPLER_2D_MULTISAMPLE,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY          = GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_BUFFER                        = GL_INT_SAMPLER_BUFFER,
    PROGRAM_UNIFORM_TYPE_INT_SAMPLER_2D_RECT                       = GL_INT_SAMPLER_2D_RECT,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_1D                   = GL_UNSIGNED_INT_SAMPLER_1D,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_2D                   = GL_UNSIGNED_INT_SAMPLER_2D,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_3D                   = GL_UNSIGNED_INT_SAMPLER_3D,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_CUBE                 = GL_UNSIGNED_INT_SAMPLER_CUBE,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_1D_ARRAY             = GL_UNSIGNED_INT_SAMPLER_1D_ARRAY,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_2D_ARRAY             = GL_UNSIGNED_INT_SAMPLER_2D_ARRAY,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE       = GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY = GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_BUFFER               = GL_UNSIGNED_INT_SAMPLER_BUFFER,
    PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_SAMPLER_2D_RECT              = GL_UNSIGNED_INT_SAMPLER_2D_RECT,

    PROGRAM_UNIFORM_TYPE_UNDEFINED

} ogl_program_uniform_type;

/** Structure that describes properties of a program attribute */
typedef struct 
{
    system_hashed_ansi_string  name;
    GLsizei                    length;
    GLint                      location;
    GLint                      size;
    ogl_program_attribute_type type;
} ogl_program_attribute_descriptor;

/** Structure that describes properties of a program uniform */
typedef struct 
{
    system_hashed_ansi_string  name;
    GLsizei                    length;
    GLint                      location;
    GLint                      size;
    ogl_program_uniform_type   type;
    GLint                      ub_id;
    GLint                      ub_offset;
} ogl_program_uniform_descriptor;

/** Enumerator that describes type of a given shader */
typedef enum
{
    SHADER_TYPE_COMPUTE  = GL_COMPUTE_SHADER,
    SHADER_TYPE_FRAGMENT = GL_FRAGMENT_SHADER,
    SHADER_TYPE_GEOMETRY = GL_GEOMETRY_SHADER,
    SHADER_TYPE_VERTEX   = GL_VERTEX_SHADER,

    /* Always last */
    SHADER_TYPE_UNKNOWN
} ogl_shader_type;

/** Enumerator that describes current rendering handler's playback status */
typedef enum
{
    RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED,
    RENDERING_HANDLER_PLAYBACK_STATUS_PAUSED,
    RENDERING_HANDLER_PLAYBACK_STATUS_STARTED
} ogl_rendering_handler_playback_status;

/** Enumerator that describes rendering handler policy. */
typedef enum
{
    /* Rendering handler should limit the number of window swaps as defined by user-provided FPS rate. */
    RENDERING_HANDLER_POLICY_FPS,
    /* Rendering handler should not limit the number of window swaps */
    RENDERING_HANDLER_POLICY_MAX_PERFORMANCE,
    /* Rendering handler will render only ONE frame per Play() call */
    RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST
} ogl_rendering_handler_policy;

/* Enumerator that describes OpenGL texture binding points */
typedef enum
{
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_1D                   = GL_TEXTURE_1D,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_1D_ARRAY             = GL_TEXTURE_1D_ARRAY,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_2D                   = GL_TEXTURE_2D,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_2D_ARRAY             = GL_TEXTURE_2D_ARRAY,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_2D_MULTISAMPLE       = GL_TEXTURE_2D_MULTISAMPLE,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_2D_MULTISAMPLE_ARRAY = GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_3D                   = GL_TEXTURE_3D,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_BUFFER               = GL_TEXTURE_BUFFER,
    OGL_TEXTURE_BINDING_POINT_GL_TEXTURE_RECTANGLE            = GL_TEXTURE_RECTANGLE,

    /* Always last */
    OGL_TEXTURE_BINDING_POINT_COUNT
} ogl_texture_binding_point;

/* Enumerator that describes OpenGL texture dimensionality */
typedef enum
{
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D                   = GL_TEXTURE_1D,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D_ARRAY             = GL_TEXTURE_1D_ARRAY,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D                   = GL_TEXTURE_2D,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_ARRAY             = GL_TEXTURE_2D_ARRAY,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE       = GL_TEXTURE_2D_MULTISAMPLE,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE_ARRAY = GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_3D                   = GL_TEXTURE_3D,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_BUFFER               = GL_TEXTURE_BUFFER,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP             = GL_TEXTURE_CUBE_MAP,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP_ARRAY       = GL_TEXTURE_CUBE_MAP_ARRAY,
    OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_RECTANGLE            = GL_TEXTURE_RECTANGLE,
    OGL_TEXTURE_DIMENSIONALITY_UNKNOWN                         = GL_NONE
} ogl_texture_dimensionality;

/* Enumerator that describes OpenGL formats. */
typedef enum
{
    OGL_TEXTURE_FORMAT_BGR             = GL_BGR,
    OGL_TEXTURE_FORMAT_BGRA            = GL_BGRA,
    OGL_TEXTURE_FORMAT_DEPTH_COMPONENT = GL_DEPTH_COMPONENT,
    OGL_TEXTURE_FORMAT_DEPTH_STENCIL   = GL_DEPTH_STENCIL,
    OGL_TEXTURE_FORMAT_RED             = GL_RED,
    OGL_TEXTURE_FORMAT_RG              = GL_RG,
    OGL_TEXTURE_FORMAT_RGB             = GL_RGB,
    OGL_TEXTURE_FORMAT_RGBA            = GL_RGBA
} ogl_texture_format;

/* Enumerator that describes OpenGL internalformats */
typedef enum
{
    OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RED_RGTC1        = GL_COMPRESSED_RED_RGTC1,
    OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RG_RGTC2         = GL_COMPRESSED_RG_RGTC2,
    OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RED_RGTC1 = GL_COMPRESSED_SIGNED_RED_RGTC1,
    OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RG_RGTC2  = GL_COMPRESSED_SIGNED_RG_RGTC2,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16           = GL_DEPTH_COMPONENT16,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT24           = GL_DEPTH_COMPONENT24,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32           = GL_DEPTH_COMPONENT32,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32F          = GL_DEPTH_COMPONENT32F,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH24_STENCIL8            = GL_DEPTH24_STENCIL8,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH32F_STENCIL8           = GL_DEPTH32F_STENCIL8,
    OGL_TEXTURE_INTERNALFORMAT_GL_R11F_G11F_B10F              = GL_R11F_G11F_B10F,
    OGL_TEXTURE_INTERNALFORMAT_GL_R16                         = GL_R16,
    OGL_TEXTURE_INTERNALFORMAT_GL_R16F                        = GL_R16F,
    OGL_TEXTURE_INTERNALFORMAT_GL_R16I                        = GL_R16I,
    OGL_TEXTURE_INTERNALFORMAT_GL_R16UI                       = GL_R16UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_R16_SNORM                   = GL_R16_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_R3_G3_B2                    = GL_R3_G3_B2,
    OGL_TEXTURE_INTERNALFORMAT_GL_R32F                        = GL_R32F,
    OGL_TEXTURE_INTERNALFORMAT_GL_R32I                        = GL_R32I,
    OGL_TEXTURE_INTERNALFORMAT_GL_R32UI                       = GL_R32UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_R8                          = GL_R8,
    OGL_TEXTURE_INTERNALFORMAT_GL_R8_SNORM                    = GL_R8_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_R8I                         = GL_R8I,
    OGL_TEXTURE_INTERNALFORMAT_GL_R8UI                        = GL_R8UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG16                        = GL_RG16,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG16F                       = GL_RG16F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG16I                       = GL_RG16I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG16UI                      = GL_RG16UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG16_SNORM                  = GL_RG16_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG32F                       = GL_RG32F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG32I                       = GL_RG32I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG32UI                      = GL_RG32UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG8                         = GL_RG8,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG8_SNORM                   = GL_RG8_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG8I                        = GL_RG8I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG8UI                       = GL_RG8UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB10                       = GL_RGB10,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2                    = GL_RGB10_A2,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2UI                  = GL_RGB10_A2UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB12                       = GL_RGB12,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB16                       = GL_RGB16,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB16F                      = GL_RGB16F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB16I                      = GL_RGB16I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB16UI                     = GL_RGB16UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB16_SNORM                 = GL_RGB16_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB32F                      = GL_RGB32F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB32I                      = GL_RGB32I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB32UI                     = GL_RGB32UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB4                        = GL_RGB4,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB5                        = GL_RGB5,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB5_A1                     = GL_RGB5_A1,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB8                        = GL_RGB8,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB8_SNORM                  = GL_RGB8_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB8I                       = GL_RGB8I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB8UI                      = GL_RGB8UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGB9_E5                     = GL_RGB9_E5,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA12                      = GL_RGBA12,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16                      = GL_RGBA16,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16_SNORM                = GL_RGBA16_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16F                     = GL_RGBA16F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16I                     = GL_RGBA16I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16UI                    = GL_RGBA16UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA2                       = GL_RGBA2,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32F                     = GL_RGBA32F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32I                     = GL_RGBA32I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32UI                    = GL_RGBA32UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA4                       = GL_RGBA4,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8                       = GL_RGBA8,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8_SNORM                 = GL_RGBA8_SNORM,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8I                      = GL_RGBA8I,
    OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8UI                     = GL_RGBA8UI,
    OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8                       = GL_SRGB8,
    OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8_ALPHA8                = GL_SRGB8_ALPHA8,

    OGL_TEXTURE_INTERNALFORMAT_UNKNOWN = 0xFFFF
} ogl_texture_internalformat;

/* Enumerator that desribes OpenGL types */
typedef enum
{
    OGL_TEXTURE_TYPE_GL_BYTE                        = GL_BYTE,
    OGL_TEXTURE_TYPE_GL_FLOAT                       = GL_FLOAT,
    OGL_TEXTURE_TYPE_GL_INT                         = GL_INT,
    OGL_TEXTURE_TYPE_GL_SHORT                       = GL_SHORT,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_INT                = GL_UNSIGNED_INT,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_BYTE               = GL_UNSIGNED_BYTE,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_BYTE_3_3_2         = GL_UNSIGNED_BYTE_3_3_2,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_BYTE_2_3_3_REV     = GL_UNSIGNED_BYTE_2_3_3_REV,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT              = GL_UNSIGNED_SHORT,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT_5_6_5        = GL_UNSIGNED_SHORT_5_6_5,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT_5_6_5_REV    = GL_UNSIGNED_SHORT_5_6_5_REV,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT_4_4_4_4      = GL_UNSIGNED_SHORT_4_4_4_4,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT_4_4_4_4_REV  = GL_UNSIGNED_SHORT_4_4_4_4_REV,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT_5_5_5_1      = GL_UNSIGNED_SHORT_5_5_5_1,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_SHORT_1_5_5_5_REV  = GL_UNSIGNED_SHORT_1_5_5_5_REV,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_INT_8_8_8_8        = GL_UNSIGNED_INT_8_8_8_8,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_INT_8_8_8_8_REV    = GL_UNSIGNED_INT_8_8_8_8_REV,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_INT_10_10_10_2     = GL_UNSIGNED_INT_10_10_10_2,
    OGL_TEXTURE_TYPE_GL_UNSIGNED_INT_2_10_10_10_REV = GL_UNSIGNED_INT_2_10_10_10_REV
} ogl_texture_type;

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
    PFNGLCOMPRESSEDTEXIMAGE2DPROC                pGLCompressedTexImage2D;
    PFNGLCOMPRESSEDTEXIMAGE3DPROC                pGLCompressedTexImage3D;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC             pGLCompressedTexSubImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC             pGLCompressedTexSubImage3D;
    PFNGLCOPYBUFFERSUBDATAPROC                   pGLCopyBufferSubData;
    PFNGLCOPYTEXIMAGE2DPROC                      pGLCopyTexImage2D;
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
    PFNGLTEXIMAGE2DPROC                          pGLTexImage2D;
    PFNGLTEXIMAGE3DPROC                          pGLTexImage3D;
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

/** Structure that cotnains context-specific strings of GL3.3 */
typedef struct
{
    const GLubyte** extensions;
    const GLubyte*  renderer;
    const GLubyte*  shading_language_version;
    const GLubyte*  vendor;
    const GLubyte*  version;
} ogl_context_gl_info;

/** Structure that contains context-specific maximum parameter values of GL4.2 */
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
    GLint   max_color_texture_samples;
    GLint   max_combined_atomic_counter_buffers;
    GLint   max_combined_atomic_counters;
    GLint   max_combined_fragment_uniform_components;
    GLint   max_combined_image_units_and_fragment_outputs;
    GLint   max_combined_texture_image_units;
    GLint   max_combined_vertex_uniform_components;
    GLint   max_combined_geometry_uniform_components;
    GLint   max_combined_uniform_blocks;
    GLint   max_cube_map_texture_size;
    GLint   max_depth_texture_samples;
    GLint   max_draw_buffers;
    GLint   max_elements_indices;
    GLint   max_elements_vertices;
    GLint   max_fragment_atomic_counter_buffers;
    GLint   max_fragment_atomic_counters;
    GLint   max_fragment_uniform_components;
    GLint   max_fragment_uniform_blocks;
    GLint   max_fragment_input_components;
    GLint   max_geometry_atomic_counter_buffers;
    GLint   max_geometry_atomic_counters;
    GLint   max_geometry_input_components;
    GLint   max_geometry_output_vertices;
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
    GLint   max_shader_storage_buffer_bindings;
    GLint   max_tess_control_atomic_counter_buffers;
    GLint   max_tess_control_atomic_counters;
    GLint   max_tess_evaluation_atomic_counter_buffers;
    GLint   max_tess_evaluation_atomic_counters;
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
    GLint   max_vertex_attribs;
    GLint   max_vertex_texture_image_units;
    GLint   max_vertex_uniform_components;
    GLint   max_vertex_output_components;
    GLint   max_vertex_uniform_blocks;
    GLint   min_program_texel_offset;
    GLint   minor_version;
    GLint   num_compressed_texture_formats;
    GLint   num_extensions;
    GLint   num_program_binary_formats;
    GLfloat point_fade_threshold_size;
    GLint   point_size_range[2];
    GLint*  program_binary_formats;
    GLint   subpixel_bits;
    GLint   texture_buffer_offset_alignment;
    GLint   uniform_buffer_offset_alignment;
} ogl_context_gl_limits;

/** Structure that contains context-specific GL4.2 function entry points */
typedef struct
{
    PFNGLACTIVESHADERPROGRAMPROC                 pGLActiveShaderProgram;
    PFNWRAPPEDGLACTIVETEXTUREPROC                pGLActiveTexture;
    PFNGLATTACHSHADERPROC                        pGLAttachShader;
    PFNGLBEGINCONDITIONALRENDERPROC              pGLBeginConditionalRender;
    PFNGLBEGINQUERYPROC                          pGLBeginQuery;
    PFNGLBEGINTRANSFORMFEEDBACKPROC              pGLBeginTransformFeedback;
    PFNGLBINDATTRIBLOCATIONPROC                  pGLBindAttribLocation;
    PFNGLBINDBUFFERPROC                          pGLBindBuffer;
    PFNGLBINDBUFFERBASEPROC                      pGLBindBufferBase;
    PFNGLBINDBUFFERRANGEPROC                     pGLBindBufferRange;
    PFNGLBINDFRAGDATALOCATIONPROC                pGLBindFragDataLocation;
    PFNGLBINDFRAMEBUFFERPROC                     pGLBindFramebuffer;
    PFNWRAPPEDGLBINDIMAGETEXTUREEXTPROC          pGLBindImageTexture;
    PFNGLBINDPROGRAMPIPELINEPROC                 pGLBindProgramPipeline;
    PFNGLBINDSAMPLERPROC                         pGLBindSampler;
    PFNGLBINDTRANSFORMFEEDBACKPROC               pGLBindTransformFeedback;
    PFNWRAPPEDGLBINDTEXTUREPROC                  pGLBindTexture;
    PFNGLBINDVERTEXARRAYPROC                     pGLBindVertexArray;
    PFNGLBLENDCOLORPROC                          pGLBlendColor;
    PFNGLBLENDEQUATIONPROC                       pGLBlendEquation;
    PFNGLBLENDEQUATIONSEPARATEPROC               pGLBlendEquationSeparate;
    PFNGLBLENDFUNCPROC                           pGLBlendFunc;
    PFNGLBLENDFUNCSEPARATEPROC                   pGLBlendFuncSeparate;
    PFNGLBLITFRAMEBUFFERPROC                     pGLBlitFramebuffer;
    PFNGLBUFFERDATAPROC                          pGLBufferData;
    PFNGLBUFFERSUBDATAPROC                       pGLBufferSubData;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC              pGLCheckFramebufferStatus;
    PFNGLCLAMPCOLORPROC                          pGLClampColor;
    PFNGLCLEARPROC                               pGLClear;
    PFNGLCLEARBUFFERFVPROC                       pGLClearBufferfv;
    PFNGLCLEARBUFFERFIPROC                       pGLClearBufferfi;
    PFNGLCLEARBUFFERIVPROC                       pGLClearBufferiv;
    PFNGLCLEARBUFFERUIVPROC                      pGLClearBufferuiv;
    PFNGLCLEARCOLORPROC                          pGLClearColor;
    PFNGLCLEARDEPTHPROC                          pGLClearDepth;
    PFNGLCLEARSTENCILPROC                        pGLClearStencil;
    PFNGLCOLORMASKPROC                           pGLColorMask;
    PFNGLCOLORMASKIPROC                          pGLColorMaski;
    PFNGLCOMPILESHADERPROC                       pGLCompileShader;
    PFNWRAPPEDGLCOMPRESSEDTEXIMAGE1DPROC         pGLCompressedTexImage1D;
    PFNWRAPPEDGLCOMPRESSEDTEXIMAGE2DPROC         pGLCompressedTexImage2D;
    PFNWRAPPEDGLCOMPRESSEDTEXIMAGE3DPROC         pGLCompressedTexImage3D;
    PFNWRAPPEDGLCOMPRESSEDTEXSUBIMAGE1DPROC      pGLCompressedTexSubImage1D;
    PFNWRAPPEDGLCOMPRESSEDTEXSUBIMAGE2DPROC      pGLCompressedTexSubImage2D;
    PFNWRAPPEDGLCOMPRESSEDTEXSUBIMAGE3DPROC      pGLCompressedTexSubImage3D;
    PFNWRAPPEDGLCOPYTEXIMAGE1DPROC               pGLCopyTexImage1D;
    PFNWRAPPEDGLCOPYTEXIMAGE2DPROC               pGLCopyTexImage2D;
    PFNWRAPPEDGLCOPYTEXSUBIMAGE1DPROC            pGLCopyTexSubImage1D;
    PFNWRAPPEDGLCOPYTEXSUBIMAGE2DPROC            pGLCopyTexSubImage2D;
    PFNWRAPPEDGLCOPYTEXSUBIMAGE3DPROC            pGLCopyTexSubImage3D;
    PFNGLCREATEPROGRAMPROC                       pGLCreateProgram;
    PFNGLCREATESHADERPROC                        pGLCreateShader;
    PFNGLCREATESHADERPROGRAMVPROC                pGLCreateShaderProgramv;
    PFNGLCULLFACEPROC                            pGLCullFace;
    PFNGLDELETEBUFFERSPROC                       pGLDeleteBuffers;
    PFNGLDELETEFRAMEBUFFERSPROC                  pGLDeleteFramebuffers;
    PFNGLDELETEPROGRAMPROC                       pGLDeleteProgram;
    PFNGLDELETEPROGRAMPIPELINESPROC              pGLDeleteProgramPipelines;
    PFNGLDELETEQUERIESPROC                       pGLDeleteQueries;
    PFNGLDELETESAMPLERSPROC                      pGLDeleteSamplers;
    PFNGLDELETESHADERPROC                        pGLDeleteShader;
    PFNGLDELETETEXTURESPROC                      pGLDeleteTextures;
    PFNGLDELETETRANSFORMFEEDBACKSPROC            pGLDeleteTransformFeedbacks;
    PFNGLDELETEVERTEXARRAYSPROC                  pGLDeleteVertexArrays;
    PFNGLDEPTHFUNCPROC                           pGLDepthFunc;
    PFNGLDEPTHMASKPROC                           pGLDepthMask;
    PFNGLDEPTHRANGEPROC                          pGLDepthRange;
    PFNGLDETACHSHADERPROC                        pGLDetachShader;
    PFNGLDISABLEPROC                             pGLDisable;
    PFNGLDISABLEIPROC                            pGLDisablei;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC            pGLDisableVertexAttribArray;
    PFNGLDRAWARRAYSPROC                          pGLDrawArrays;
    PFNGLDRAWARRAYSINSTANCEDPROC                 pGLDrawArraysInstanced;
    PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC     pGLDrawArraysInstancedBaseInstance;
    PFNGLDRAWBUFFERPROC                          pGLDrawBuffer;
    PFNGLDRAWBUFFERSPROC                         pGLDrawBuffers;
    PFNGLDRAWELEMENTSPROC                        pGLDrawElements;
    PFNGLDRAWELEMENTSINSTANCEDPROC               pGLDrawElementsInstanced;
    PFNGLDRAWRANGEELEMENTSPROC                   pGLDrawRangeElements;
    PFNGLDRAWTRANSFORMFEEDBACKPROC               pGLDrawTransformFeedback;
    PFNGLENABLEPROC                              pGLEnable;
    PFNGLENABLEIPROC                             pGLEnablei;
    PFNGLENABLEVERTEXATTRIBARRAYPROC             pGLEnableVertexAttribArray;
    PFNGLENDCONDITIONALRENDERPROC                pGLEndConditionalRender;
    PFNGLENDQUERYPROC                            pGLEndQuery;
    PFNGLENDTRANSFORMFEEDBACKPROC                pGLEndTransformFeedback;
    PFNGLFINISHPROC                              pGLFinish;
    PFNGLFLUSHPROC                               pGLFlush;
    PFNWRAPPEDGLFRAMEBUFFERTEXTUREPROC           pGLFramebufferTexture;
    PFNWRAPPEDGLFRAMEBUFFERTEXTURE1DPROC         pGLFramebufferTexture1D;
    PFNWRAPPEDGLFRAMEBUFFERTEXTURE2DPROC         pGLFramebufferTexture2D;
    PFNWRAPPEDGLFRAMEBUFFERTEXTURE3DPROC         pGLFramebufferTexture3D;
    PFNWRAPPEDGLFRAMEBUFFERTEXTURELAYERPROC      pGLFramebufferTextureLayer;
    PFNGLFRONTFACEPROC                           pGLFrontFace;
    PFNGLGENBUFFERSPROC                          pGLGenBuffers;
    PFNGLGENERATEMIPMAPPROC                      pGLGenerateMipmap;
    PFNGLGENFRAMEBUFFERSPROC                     pGLGenFramebuffers;
    PFNGLGENPROGRAMPIPELINESPROC                 pGLGenProgramPipelines;
    PFNGLGENQUERIESPROC                          pGLGenQueries;
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
    PFNGLGETBOOLEANVPROC                         pGLGetBooleanv;
    PFNGLGETBUFFERPARAMETERI64VPROC              pGLGetBufferParameteri64v;
    PFNGLGETBUFFERPARAMETERIVPROC                pGLGetBufferParameteriv;
    PFNGLGETBUFFERPOINTERVPROC                   pGLGetBufferPointerv;
    PFNGLGETBUFFERSUBDATAPROC                    pGLGetBufferSubData;
    PFNGLGETBOOLEANI_VPROC                       pGLGetBooleani_v;
    PFNWRAPPEDGLGETCOMPRESSEDTEXIMAGEPROC        pGLGetCompressedTexImage;
    PFNGLGETDOUBLEVPROC                          pGLGetDoublev;
    PFNGLGETERRORPROC                            pGLGetError;
    PFNGLGETFLOATVPROC                           pGLGetFloatv;
    PFNGLGETFRAGDATALOCATIONPROC                 pGLGetFragDataLocation;
    PFNGLGETINTEGER64I_VPROC                     pGLGetInteger64i_v;
    PFNGLGETINTEGERI_VPROC                       pGLGetIntegeri_v;
    PFNGLGETINTEGERVPROC                         pGLGetIntegerv;
    PFNGLGETPROGRAMBINARYPROC                    pGLGetProgramBinary;
    PFNGLGETPROGRAMIVPROC                        pGLGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC                   pGLGetProgramInfoLog;
    PFNGLGETSAMPLERPARAMETERFVPROC               pGLGetSamplerParameterfv;
    PFNGLGETSAMPLERPARAMETERIVPROC               pGLGetSamplerParameteriv;
    PFNGLGETSAMPLERPARAMETERIIVPROC              pGLGetSamplerParameterIiv;
    PFNGLGETSAMPLERPARAMETERIUIVPROC             pGLGetSamplerParameterIuiv;
    PFNGLGETSHADERIVPROC                         pGLGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC                    pGLGetShaderInfoLog;
    PFNGLGETSHADERSOURCEPROC                     pGLGetShaderSource;
    PFNGLGETSTRINGPROC                           pGLGetString;
    PFNGLGETSTRINGIPROC                          pGLGetStringi;
    PFNWRAPPEDGLGETTEXIMAGEPROC                  pGLGetTexImage;
    PFNWRAPPEDGLGETTEXLEVELPARAMETERFVPROC       pGLGetTexLevelParameterfv;
    PFNWRAPPEDGLGETTEXLEVELPARAMETERIVPROC       pGLGetTexLevelParameteriv;
    PFNWRAPPEDGLGETTEXPARAMETERFVPROC            pGLGetTexParameterfv;
    PFNWRAPPEDGLGETTEXPARAMETERIIVPROC           pGLGetTexParameterIiv;
    PFNWRAPPEDGLGETTEXPARAMETERIUIVPROC          pGLGetTexParameterIuiv;
    PFNWRAPPEDGLGETTEXPARAMETERIVPROC            pGLGetTexParameteriv;
    PFNGLGETTRANSFORMFEEDBACKVARYINGPROC         pGLGetTransformFeedbackVarying;
    PFNGLGETUNIFORMBLOCKINDEXPROC                pGLGetUniformBlockIndex;
    PFNGLGETUNIFORMFVPROC                        pGLGetUniformfv;
    PFNGLGETUNIFORMIVPROC                        pGLGetUniformiv;
    PFNGLGETUNIFORMLOCATIONPROC                  pGLGetUniformLocation;
    PFNGLGETQUERYIVPROC                          pGLGetQueryiv;
    PFNGLGETQUERYOBJECTIVPROC                    pGLGetQueryObjectiv;
    PFNGLGETQUERYOBJECTUIVPROC                   pGLGetQueryObjectuiv;
    PFNGLGETQUERYOBJECTUI64VPROC                 pGLGetQueryObjectui64v;
    PFNGLGETUNIFORMUIVPROC                       pGLGetUniformuiv;
    PFNGLGETVERTEXATTRIBDVPROC                   pGLGetVertexAttribdv;
    PFNGLGETVERTEXATTRIBFVPROC                   pGLGetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIIVPROC                  pGLGetVertexAttribIiv;
    PFNGLGETVERTEXATTRIBIUIVPROC                 pGLGetVertexAttribIuiv;
    PFNGLGETVERTEXATTRIBIVPROC                   pGLGetVertexAttribiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC             pGLGetVertexAttribPointerv;
    PFNGLHINTPROC                                pGLHint;
    PFNGLISBUFFERPROC                            pGLIsBuffer;
    PFNGLISENABLEDPROC                           pGLIsEnabled;
    PFNGLISENABLEDIPROC                          pGLIsEnabledi;
    PFNGLISPROGRAMPROC                           pGLIsProgram;
    PFNGLISSHADERPROC                            pGLIsShader;
    PFNGLISTEXTUREPROC                           pGLIsTexture;
    PFNGLISTRANSFORMFEEDBACKPROC                 pGLIsTransformFeedback;
    PFNGLISQUERYPROC                             pGLIsQuery;
    PFNGLLINEWIDTHPROC                           pGLLineWidth;
    PFNGLLINKPROGRAMPROC                         pGLLinkProgram;
    PFNGLLOGICOPPROC                             pGLLogicOp;
    PFNGLMAPBUFFERPROC                           pGLMapBuffer;
    PFNGLMAPBUFFERRANGEPROC                      pGLMapBufferRange;
    PFNGLMEMORYBARRIEREXTPROC                    pGLMemoryBarrier;
    PFNGLMULTIDRAWARRAYSPROC                     pGLMultiDrawArrays;
    PFNGLMULTIDRAWELEMENTSPROC                   pGLMultiDrawElements;
    PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC         pGLMultiDrawElementsBaseVertex;
    PFNGLPAUSETRANSFORMFEEDBACKPROC              pGLPauseTransformFeedback;
    PFNGLPIXELSTOREFPROC                         pGLPixelStoref;
    PFNGLPIXELSTOREIPROC                         pGLPixelStorei;
    PFNGLPOINTPARAMETERFPROC                     pGLPointParameterf;
    PFNGLPOINTPARAMETERFVPROC                    pGLPointParameterfv;
    PFNGLPOINTPARAMETERIPROC                     pGLPointParameteri;
    PFNGLPOINTPARAMETERIVPROC                    pGLPointParameteriv;
    PFNGLPOINTSIZEPROC                           pGLPointSize;
    PFNGLPOLYGONMODEPROC                         pGLPolygonMode;
    PFNGLPOLYGONOFFSETPROC                       pGLPolygonOffset;
    PFNGLPRIMITIVERESTARTINDEXPROC               pGLPrimitiveRestartIndex;
    PFNGLPROGRAMBINARYPROC                       pGLProgramBinary;
    PFNGLPROGRAMPARAMETERIPROC                   pGLProgramParameteri;
    PFNGLPROGRAMUNIFORM1DPROC                    pGLProgramUniform1d;
    PFNGLPROGRAMUNIFORM1DVPROC                   pGLProgramUniform1dv;
    PFNGLPROGRAMUNIFORM1FPROC                    pGLProgramUniform1f;
    PFNGLPROGRAMUNIFORM1FVPROC                   pGLProgramUniform1fv;
    PFNGLPROGRAMUNIFORM1IPROC                    pGLProgramUniform1i;
    PFNGLPROGRAMUNIFORM1IVPROC                   pGLProgramUniform1iv;
    PFNGLPROGRAMUNIFORM1UIPROC                   pGLProgramUniform1ui;
    PFNGLPROGRAMUNIFORM1UIVPROC                  pGLProgramUniform1uiv;
    PFNGLPROGRAMUNIFORM2DPROC                    pGLProgramUniform2d;
    PFNGLPROGRAMUNIFORM2DVPROC                   pGLProgramUniform2dv;
    PFNGLPROGRAMUNIFORM2FPROC                    pGLProgramUniform2f;
    PFNGLPROGRAMUNIFORM2FVPROC                   pGLProgramUniform2fv;
    PFNGLPROGRAMUNIFORM2IPROC                    pGLProgramUniform2i;
    PFNGLPROGRAMUNIFORM2IVPROC                   pGLProgramUniform2iv;
    PFNGLPROGRAMUNIFORM2UIPROC                   pGLProgramUniform2ui;
    PFNGLPROGRAMUNIFORM2UIVPROC                  pGLProgramUniform2uiv;
    PFNGLPROGRAMUNIFORM3DPROC                    pGLProgramUniform3d;
    PFNGLPROGRAMUNIFORM3DVPROC                   pGLProgramUniform3dv;
    PFNGLPROGRAMUNIFORM3FPROC                    pGLProgramUniform3f;
    PFNGLPROGRAMUNIFORM3FVPROC                   pGLProgramUniform3fv;
    PFNGLPROGRAMUNIFORM3IPROC                    pGLProgramUniform3i;
    PFNGLPROGRAMUNIFORM3IVPROC                   pGLProgramUniform3iv;
    PFNGLPROGRAMUNIFORM3UIPROC                   pGLProgramUniform3ui;
    PFNGLPROGRAMUNIFORM3UIVPROC                  pGLProgramUniform3uiv;
    PFNGLPROGRAMUNIFORM4DPROC                    pGLProgramUniform4d;
    PFNGLPROGRAMUNIFORM4DVPROC                   pGLProgramUniform4dv;
    PFNGLPROGRAMUNIFORM4FPROC                    pGLProgramUniform4f;
    PFNGLPROGRAMUNIFORM4FVPROC                   pGLProgramUniform4fv;
    PFNGLPROGRAMUNIFORM4IPROC                    pGLProgramUniform4i;
    PFNGLPROGRAMUNIFORM4IVPROC                   pGLProgramUniform4iv;
    PFNGLPROGRAMUNIFORM4UIPROC                   pGLProgramUniform4ui;
    PFNGLPROGRAMUNIFORM4UIVPROC                  pGLProgramUniform4uiv;
    PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC          pGLProgramUniformMatrix2fv;
    PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC        pGLProgramUniformMatrix2x3fv;
    PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC        pGLProgramUniformMatrix2x4fv;
    PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC          pGLProgramUniformMatrix3fv;
    PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC        pGLProgramUniformMatrix3x2fv;
    PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC        pGLProgramUniformMatrix3x4fv;
    PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC          pGLProgramUniformMatrix4fv;
    PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC        pGLProgramUniformMatrix4x2fv;
    PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC        pGLProgramUniformMatrix4x3fv;
    PFNGLREADBUFFERPROC                          pGLReadBuffer;
    PFNGLREADPIXELSPROC                          pGLReadPixels;
    PFNGLRESUMETRANSFORMFEEDBACKPROC             pGLResumeTransformFeedback;
    PFNGLSAMPLECOVERAGEPROC                      pGLSampleCoverage;
    PFNGLSAMPLERPARAMETERFPROC                   pGLSamplerParameterf;
    PFNGLSAMPLERPARAMETERFVPROC                  pGLSamplerParameterfv;
    PFNGLSAMPLERPARAMETERIIVPROC                 pGLSamplerParameterIiv;
    PFNGLSAMPLERPARAMETERIUIVPROC                pGLSamplerParameterIuiv;
    PFNGLSAMPLERPARAMETERIPROC                   pGLSamplerParameteri;
    PFNGLSAMPLERPARAMETERIVPROC                  pGLSamplerParameteriv;
    PFNGLSCISSORPROC                             pGLScissor;
    PFNGLSHADERSOURCEPROC                        pGLShaderSource;
    PFNGLSTENCILFUNCPROC                         pGLStencilFunc;
    PFNGLSTENCILFUNCSEPARATEPROC                 pGLStencilFuncSeparate;
    PFNGLSTENCILMASKPROC                         pGLStencilMask;
    PFNGLSTENCILMASKSEPARATEPROC                 pGLStencilMaskSeparate;
    PFNGLSTENCILOPPROC                           pGLStencilOp;
    PFNGLSTENCILOPSEPARATEPROC                   pGLStencilOpSeparate;
    PFNWRAPPEDGLTEXBUFFERPROC                    pGLTexBuffer;
    PFNGLTEXBUFFERRANGEPROC                      pGLTexBufferRange;
    PFNWRAPPEDGLTEXIMAGE1DPROC                   pGLTexImage1D;
    PFNWRAPPEDGLTEXIMAGE2DPROC                   pGLTexImage2D;
    PFNWRAPPEDGLTEXIMAGE3DPROC                   pGLTexImage3D;
    PFNWRAPPEDGLTEXPARAMETERFPROC                pGLTexParameterf;
    PFNWRAPPEDGLTEXPARAMETERFVPROC               pGLTexParameterfv;
    PFNWRAPPEDGLTEXPARAMETERIIVPROC              pGLTexParameterIiv;
    PFNWRAPPEDGLTEXPARAMETERIUIVPROC             pGLTexParameterIuiv;
    PFNWRAPPEDGLTEXPARAMETERIPROC                pGLTexParameteri;
    PFNWRAPPEDGLTEXPARAMETERIVPROC               pGLTexParameteriv;
    PFNWRAPPEDGLTEXSTORAGE1DPROC                 pGLTexStorage1D;
    PFNWRAPPEDGLTEXSTORAGE2DPROC                 pGLTexStorage2D;
    PFNWRAPPEDGLTEXSTORAGE3DPROC                 pGLTexStorage3D;
    PFNWRAPPEDGLTEXSUBIMAGE1DPROC                pGLTexSubImage1D;
    PFNWRAPPEDGLTEXSUBIMAGE2DPROC                pGLTexSubImage2D;
    PFNWRAPPEDGLTEXSUBIMAGE3DPROC                pGLTexSubImage3D;
    PFNGLTRANSFORMFEEDBACKVARYINGSPROC           pGLTransformFeedbackVaryings;
    PFNGLUNIFORMBLOCKBINDINGPROC                 pGLUniformBlockBinding;
    PFNGLUNMAPBUFFERPROC                         pGLUnmapBuffer;
    PFNGLUSEPROGRAMPROC                          pGLUseProgram;
    PFNGLUSEPROGRAMSTAGESPROC                    pGLUseProgramStages;
    PFNGLVALIDATEPROGRAMPROC                     pGLValidateProgram;
    PFNGLVERTEXATTRIB1DPROC                      pGLVertexAttrib1d;
    PFNGLVERTEXATTRIB1DVPROC                     pGLVertexAttrib1dv;
    PFNGLVERTEXATTRIB1FPROC                      pGLVertexAttrib1f;
    PFNGLVERTEXATTRIB1FVPROC                     pGLVertexAttrib1fv;
    PFNGLVERTEXATTRIB1SPROC                      pGLVertexAttrib1s;
    PFNGLVERTEXATTRIB1SVPROC                     pGLVertexAttrib1sv;
    PFNGLVERTEXATTRIB2DPROC                      pGLVertexAttrib2d;
    PFNGLVERTEXATTRIB2DVPROC                     pGLVertexAttrib2dv;
    PFNGLVERTEXATTRIB2FPROC                      pGLVertexAttrib2f;
    PFNGLVERTEXATTRIB2FVPROC                     pGLVertexAttrib2fv;
    PFNGLVERTEXATTRIB2SPROC                      pGLVertexAttrib2s;
    PFNGLVERTEXATTRIB2SVPROC                     pGLVertexAttrib2sv;
    PFNGLVERTEXATTRIB3DPROC                      pGLVertexAttrib3d;
    PFNGLVERTEXATTRIB3DVPROC                     pGLVertexAttrib3dv;
    PFNGLVERTEXATTRIB3FPROC                      pGLVertexAttrib3f;
    PFNGLVERTEXATTRIB3FVPROC                     pGLVertexAttrib3fv;
    PFNGLVERTEXATTRIB3SPROC                      pGLVertexAttrib3s;
    PFNGLVERTEXATTRIB3SVPROC                     pGLVertexAttrib3sv;
    PFNGLVERTEXATTRIB4NBVPROC                    pGLVertexAttrib4Nbv;
    PFNGLVERTEXATTRIB4NIVPROC                    pGLVertexAttrib4Niv;
    PFNGLVERTEXATTRIB4NSVPROC                    pGLVertexAttrib4Nsv;
    PFNGLVERTEXATTRIB4NUBPROC                    pGLVertexAttrib4Nub;
    PFNGLVERTEXATTRIB4NUBVPROC                   pGLVertexAttrib4Nubv;
    PFNGLVERTEXATTRIB4NUIVPROC                   pGLVertexAttrib4Nuiv;
    PFNGLVERTEXATTRIB4NUSVPROC                   pGLVertexAttrib4Nusv;
    PFNGLVERTEXATTRIB4BVPROC                     pGLVertexAttrib4bv;
    PFNGLVERTEXATTRIB4DPROC                      pGLVertexAttrib4d;
    PFNGLVERTEXATTRIB4DVPROC                     pGLVertexAttrib4dv;
    PFNGLVERTEXATTRIB4FPROC                      pGLVertexAttrib4f;
    PFNGLVERTEXATTRIB4FVPROC                     pGLVertexAttrib4fv;
    PFNGLVERTEXATTRIB4IVPROC                     pGLVertexAttrib4iv;
    PFNGLVERTEXATTRIB4SPROC                      pGLVertexAttrib4s;
    PFNGLVERTEXATTRIB4SVPROC                     pGLVertexAttrib4sv;
    PFNGLVERTEXATTRIB4UBVPROC                    pGLVertexAttrib4ubv;
    PFNGLVERTEXATTRIB4UIVPROC                    pGLVertexAttrib4uiv;
    PFNGLVERTEXATTRIB4USVPROC                    pGLVertexAttrib4usv;
    PFNGLVERTEXATTRIBDIVISORPROC                 pGLVertexAttribDivisor;
    PFNGLVERTEXATTRIBI1IPROC                     pGLVertexAttribI1i;
    PFNGLVERTEXATTRIBI1UIPROC                    pGLVertexAttribI1ui;
    PFNGLVERTEXATTRIBI1IVPROC                    pGLVertexAttribI1iv;
    PFNGLVERTEXATTRIBI1UIVPROC                   pGLVertexAttribI1uiv;
    PFNGLVERTEXATTRIBI2IPROC                     pGLVertexAttribI2i;
    PFNGLVERTEXATTRIBI2IVPROC                    pGLVertexAttribI2iv;
    PFNGLVERTEXATTRIBI2UIPROC                    pGLVertexAttribI2ui;
    PFNGLVERTEXATTRIBI2UIVPROC                   pGLVertexAttribI2uiv;
    PFNGLVERTEXATTRIBI3IPROC                     pGLVertexAttribI3i;
    PFNGLVERTEXATTRIBI3IVPROC                    pGLVertexAttribI3iv;
    PFNGLVERTEXATTRIBI3UIPROC                    pGLVertexAttribI3ui;
    PFNGLVERTEXATTRIBI3UIVPROC                   pGLVertexAttribI3uiv;
    PFNGLVERTEXATTRIBI4BVPROC                    pGLVertexAttribI4bv;
    PFNGLVERTEXATTRIBI4IPROC                     pGLVertexAttribI4i;
    PFNGLVERTEXATTRIBI4IVPROC                    pGLVertexAttribI4iv;
    PFNGLVERTEXATTRIBI4SVPROC                    pGLVertexAttribI4sv;
    PFNGLVERTEXATTRIBI4UBVPROC                   pGLVertexAttribI4ubv;
    PFNGLVERTEXATTRIBI4UIPROC                    pGLVertexAttribI4ui;
    PFNGLVERTEXATTRIBI4UIVPROC                   pGLVertexAttribI4uiv;
    PFNGLVERTEXATTRIBI4USVPROC                   pGLVertexAttribI4usv;
    PFNGLVERTEXATTRIBIPOINTERPROC                pGLVertexAttribIPointer;
    PFNGLVERTEXATTRIBPOINTERPROC                 pGLVertexAttribPointer;
    PFNGLVIEWPORTPROC                            pGLViewport;
} ogl_context_gl_entrypoints;

typedef struct
{
    PFNGLACTIVETEXTUREPROC                   pGLActiveTexture;
    PFNGLBEGINTRANSFORMFEEDBACKPROC          pGLBeginTransformFeedback;
    PFNGLBINDBUFFERPROC                      pGLBindBuffer;
    PFNGLBINDBUFFERBASEPROC                  pGLBindBufferBase;
    PFNGLBINDBUFFERRANGEPROC                 pGLBindBufferRange;
    PFNGLBINDIMAGETEXTUREPROC                pGLBindImageTexture;
    PFNGLBINDSAMPLERPROC                     pGLBindSampler;
    PFNGLBINDTEXTUREPROC                     pGLBindTexture;
    PFNGLBINDVERTEXARRAYPROC                 pGLBindVertexArray;
    PFNGLBLENDCOLORPROC                      pGLBlendColor;
    PFNGLBLENDEQUATIONPROC                   pGLBlendEquation;
    PFNGLBLENDEQUATIONSEPARATEPROC           pGLBlendEquationSeparate;
    PFNGLBLENDFUNCPROC                       pGLBlendFunc;
    PFNGLBLENDFUNCSEPARATEPROC               pGLBlendFuncSeparate;
    PFNGLBUFFERDATAPROC                      pGLBufferData;
    PFNGLBUFFERSUBDATAPROC                   pGLBufferSubData;
    PFNGLCLEARPROC                           pGLClear;
    PFNGLCLEARCOLORPROC                      pGLClearColor;
    PFNGLCOLORMASKPROC                       pGLColorMask;
    PFNGLCOMPRESSEDTEXIMAGE3DPROC            pGLCompressedTexImage3D;
    PFNGLCOMPRESSEDTEXIMAGE2DPROC            pGLCompressedTexImage2D;
    PFNGLCOMPRESSEDTEXIMAGE1DPROC            pGLCompressedTexImage1D;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC         pGLCompressedTexSubImage3D;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC         pGLCompressedTexSubImage2D;
    PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC         pGLCompressedTexSubImage1D;
    PFNGLCOPYTEXIMAGE1DPROC                  pGLCopyTexImage1D;
    PFNGLCOPYTEXIMAGE2DPROC                  pGLCopyTexImage2D;
    PFNGLCOPYTEXSUBIMAGE1DPROC               pGLCopyTexSubImage1D;
    PFNGLCOPYTEXSUBIMAGE2DPROC               pGLCopyTexSubImage2D;
    PFNGLCOPYTEXSUBIMAGE3DPROC               pGLCopyTexSubImage3D;
    PFNGLCULLFACEPROC                        pGLCullFace;
    PFNGLDELETEBUFFERSPROC                   pGLDeleteBuffers;
    PFNGLDELETEVERTEXARRAYSPROC              pGLDeleteVertexArrays;
    PFNGLDEPTHFUNCPROC                       pGLDepthFunc;
    PFNGLDEPTHMASKPROC                       pGLDepthMask;
    PFNGLDISABLEPROC                         pGLDisable;
    PFNGLDISABLEIPROC                        pGLDisablei;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC        pGLDisableVertexAttribArray;
    PFNGLDRAWARRAYSPROC                      pGLDrawArrays;
    PFNGLDRAWARRAYSINSTANCEDPROC             pGLDrawArraysInstanced;
    PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC pGLDrawArraysInstancedBaseInstance;
    PFNGLDRAWELEMENTSPROC                    pGLDrawElements;
    PFNGLDRAWELEMENTSINSTANCEDPROC           pGLDrawElementsInstanced;
    PFNGLDRAWRANGEELEMENTSPROC               pGLDrawRangeElements;
    PFNGLDRAWTRANSFORMFEEDBACKPROC           pGLDrawTransformFeedback;
    PFNGLENABLEPROC                          pGLEnable;
    PFNGLENABLEIPROC                         pGLEnablei;
    PFNGLENABLEVERTEXATTRIBARRAYPROC         pGLEnableVertexAttribArray;
    PFNGLFRAMEBUFFERTEXTUREPROC              pGLFramebufferTexture;
    PFNGLFRAMEBUFFERTEXTURE1DPROC            pGLFramebufferTexture1D;
    PFNGLFRAMEBUFFERTEXTURE2DPROC            pGLFramebufferTexture2D;
    PFNGLFRAMEBUFFERTEXTURE3DPROC            pGLFramebufferTexture3D;
    PFNGLFRAMEBUFFERTEXTURELAYERPROC         pGLFramebufferTextureLayer;
    PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC  pGLGetActiveAtomicCounterBufferiv;
    PFNGLGETBOOLEANI_VPROC                   pGLGetBooleani_v;
    PFNGLGETBOOLEANVPROC                     pGLGetBooleanv;
    PFNGLGETBUFFERPARAMETERI64VPROC          pGLGetBufferParameteri64v;
    PFNGLGETBUFFERPARAMETERIVPROC            pGLGetBufferParameteriv;
    PFNGLGETBUFFERPOINTERVPROC               pGLGetBufferPointerv;
    PFNGLGETBUFFERSUBDATAPROC                pGLGetBufferSubData;
    PFNGLGETCOMPRESSEDTEXIMAGEPROC           pGLGetCompressedTexImage;
    PFNGLGETDOUBLEVPROC                      pGLGetDoublev;
    PFNGLGETFLOATVPROC                       pGLGetFloatv;
    PFNGLGETINTEGER64I_VPROC                 pGLGetInteger64i_v;
    PFNGLGETINTEGERI_VPROC                   pGLGetIntegeri_v;
    PFNGLGETINTEGERVPROC                     pGLGetIntegerv;
    PFNGLGETSAMPLERPARAMETERFVPROC           pGLGetSamplerParameterfv;
    PFNGLGETSAMPLERPARAMETERIVPROC           pGLGetSamplerParameteriv;
    PFNGLGETSAMPLERPARAMETERIIVPROC          pGLGetSamplerParameterIiv;
    PFNGLGETSAMPLERPARAMETERIUIVPROC         pGLGetSamplerParameterIuiv;
    PFNGLGETTEXPARAMETERIIVPROC              pGLGetTexParameterIiv;
    PFNGLGETTEXPARAMETERIUIVPROC             pGLGetTexParameterIuiv;
    PFNGLGETVERTEXATTRIBDVPROC               pGLGetVertexAttribdv;
    PFNGLGETVERTEXATTRIBFVPROC               pGLGetVertexAttribfv;
    PFNGLGETVERTEXATTRIBIVPROC               pGLGetVertexAttribiv;
    PFNGLGETVERTEXATTRIBIIVPROC              pGLGetVertexAttribIiv;
    PFNGLGETVERTEXATTRIBIUIVPROC             pGLGetVertexAttribIuiv;
    PFNGLGETVERTEXATTRIBPOINTERVPROC         pGLGetVertexAttribPointerv;
    PFNGLMAPBUFFERPROC                       pGLMapBuffer;
    PFNGLMAPBUFFERRANGEPROC                  pGLMapBufferRange;
    PFNGLMULTIDRAWARRAYSPROC                 pGLMultiDrawArrays;
    PFNGLMULTIDRAWELEMENTSPROC               pGLMultiDrawElements;
    PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC     pGLMultiDrawElementsBaseVertex;
    PFNGLREADPIXELSPROC                      pGLReadPixels;
    PFNGLRESUMETRANSFORMFEEDBACKPROC         pGLResumeTransformFeedback;
    PFNGLSAMPLERPARAMETERFPROC               pGLSamplerParameterf;
    PFNGLSAMPLERPARAMETERFVPROC              pGLSamplerParameterfv;
    PFNGLSAMPLERPARAMETERIIVPROC             pGLSamplerParameterIiv;
    PFNGLSAMPLERPARAMETERIUIVPROC            pGLSamplerParameterIuiv;
    PFNGLSAMPLERPARAMETERIPROC               pGLSamplerParameteri;
    PFNGLSAMPLERPARAMETERIVPROC              pGLSamplerParameteriv;
    PFNGLSCISSORPROC                         pGLScissor;
    PFNGLTEXIMAGE1DPROC                      pGLTexImage1D;
    PFNGLTEXIMAGE2DPROC                      pGLTexImage2D;
    PFNGLTEXIMAGE3DPROC                      pGLTexImage3D;
    PFNGLTEXPARAMETERIIVPROC                 pGLTexParameterIiv;
    PFNGLTEXPARAMETERIUIVPROC                pGLTexParameterIuiv;
    PFNGLTEXSTORAGE1DPROC                    pGLTexStorage1D;
    PFNGLTEXSTORAGE2DPROC                    pGLTexStorage2D;
    PFNGLTEXSTORAGE3DPROC                    pGLTexStorage3D;
    PFNGLTEXSUBIMAGE1DPROC                   pGLTexSubImage1D;
    PFNGLTEXSUBIMAGE2DPROC                   pGLTexSubImage2D;
    PFNGLTEXSUBIMAGE3DPROC                   pGLTexSubImage3D;
    PFNGLUNMAPBUFFERPROC                     pGLUnmapBuffer;
    PFNGLUSEPROGRAMPROC                      pGLUseProgram;
    PFNGLVERTEXATTRIBIPOINTERPROC            pGLVertexAttribIPointer;
    PFNGLVERTEXATTRIBPOINTERPROC             pGLVertexAttribPointer;
    PFNGLVIEWPORTPROC                        pGLViewport;

    /* DSA */
    PFNGLBINDMULTITEXTUREEXTPROC             pGLBindMultiTextureEXT;
    PFNGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC     pGLCompressedTextureImage3DEXT;
    PFNGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC     pGLCompressedTextureImage2DEXT;
    PFNGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC     pGLCompressedTextureImage1DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC  pGLCompressedTextureSubImage3DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC  pGLCompressedTextureSubImage2DEXT;
    PFNGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC  pGLCompressedTextureSubImage1DEXT;
    PFNGLCOPYTEXTUREIMAGE1DEXTPROC           pGLCopyTextureImage1DEXT;
    PFNGLCOPYTEXTUREIMAGE2DEXTPROC           pGLCopyTextureImage2DEXT;
    PFNGLCOPYTEXTURESUBIMAGE1DEXTPROC        pGLCopyTextureSubImage1DEXT;
    PFNGLCOPYTEXTURESUBIMAGE2DEXTPROC        pGLCopyTextureSubImage2DEXT;
    PFNGLCOPYTEXTURESUBIMAGE3DEXTPROC        pGLCopyTextureSubImage3DEXT;
    PFNGLGENERATETEXTUREMIPMAPEXTPROC        pGLGenerateTextureMipmapEXT;
    PFNGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC    pGLGetCompressedTextureImageEXT;
    PFNGLGETTEXTUREIMAGEEXTPROC              pGLGetTextureImageEXT;
    PFNGLGETTEXTURELEVELPARAMETERFVEXTPROC   pGLGetTextureLevelParameterfvEXT;
    PFNGLGETTEXTURELEVELPARAMETERIVEXTPROC   pGLGetTextureLevelParameterivEXT;
    PFNGLGETTEXTUREPARAMETERIIVEXTPROC       pGLGetTextureParameterIiv;
    PFNGLGETTEXTUREPARAMETERIUIVEXTPROC      pGLGetTextureParameterIuiv;
    PFNGLNAMEDFRAMEBUFFERTEXTUREEXTPROC      pGLNamedFramebufferTextureEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC pGLNamedFramebufferTextureLayerEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC  pGLNamedFramebufferTextureFaceEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC    pGLNamedFramebufferTexture1DEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC    pGLNamedFramebufferTexture2DEXT;
    PFNGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC    pGLNamedFramebufferTexture3DEXT;

    PFNGLTEXTUREBUFFEREXTPROC               pGLTextureBufferEXT;
    PFNGLTEXTUREIMAGE1DEXTPROC              pGLTextureImage1DEXT;
    PFNGLTEXTUREIMAGE2DEXTPROC              pGLTextureImage2DEXT;
    PFNGLTEXTUREIMAGE3DEXTPROC              pGLTextureImage3DEXT;
    PFNGLTEXTUREPARAMETERFEXTPROC           pGLTextureParameterfEXT;
    PFNGLTEXTUREPARAMETERFVEXTPROC          pGLTextureParameterfvEXT;
    PFNGLTEXTUREPARAMETERIEXTPROC           pGLTextureParameteriEXT;
    PFNGLTEXTUREPARAMETERIIVEXTPROC         pGLTextureParameterIivEXT;
    PFNGLTEXTUREPARAMETERIUIVEXTPROC        pGLTextureParameterIuivEXT;
    PFNGLTEXTUREPARAMETERIVEXTPROC          pGLTextureParameterivEXT;
    PFNGLTEXTURERENDERBUFFEREXTPROC         pGLTextureRenderbufferEXT;
    PFNGLTEXTURESTORAGE1DEXTPROC            pGLTextureStorage1DEXT;
    PFNGLTEXTURESTORAGE2DEXTPROC            pGLTextureStorage2DEXT;
    PFNGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC pGLTextureStorage2DMultisampleEXT;
    PFNGLTEXTURESTORAGE3DEXTPROC            pGLTextureStorage3DEXT;
    PFNGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC pGLTextureStorage3DMultisampleEXT;
    PFNGLTEXTURESUBIMAGE1DEXTPROC           pGLTextureSubImage1DEXT;
    PFNGLTEXTURESUBIMAGE2DEXTPROC           pGLTextureSubImage2DEXT;
    PFNGLTEXTURESUBIMAGE3DEXTPROC           pGLTextureSubImage3DEXT;

    PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC pGLGetNamedBufferParameterivEXT;
    PFNGLGETNAMEDBUFFERPOINTERVEXTPROC    pGLGetNamedBufferPointervEXT;
    PFNGLGETNAMEDBUFFERSUBDATAEXTPROC     pGLGetNamedBufferSubDataEXT;
    PFNGLMAPNAMEDBUFFEREXTPROC            pGLMapNamedBufferEXT;
    PFNGLNAMEDBUFFERDATAEXTPROC           pGLNamedBufferDataEXT;
    PFNGLNAMEDBUFFERSUBDATAEXTPROC        pGLNamedBufferSubDataEXT;
    PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC    pGLNamedCopyBufferSubDataEXT;
    PFNGLUNMAPNAMEDBUFFEREXTPROC          pGLUnmapNamedBufferEXT;

    PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC  pGLVertexArrayVertexAttribOffsetEXT;
    PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC pGLVertexArrayVertexAttribIOffsetEXT;

    /* GL_ARB_buffer_storage */
    PFNGLBUFFERSTORAGEPROC         pGLBufferStorage;
    PFNGLNAMEDBUFFERSTORAGEEXTPROC pGLNamedBufferStorageEXT;

    /* GL_ARB_compute_shader */
    PFNGLDISPATCHCOMPUTEPROC         pGLDispatchCompute;
    PFNGLDISPATCHCOMPUTEINDIRECTPROC pGLDispatchComputeIndirect;

    /* GL_ARB_multi_bind */
    PFNGLBINDBUFFERSBASEPROC  pGLBindBuffersBase;
    PFNGLBINDBUFFERSRANGEPROC pGLBindBuffersRange;
    PFNGLBINDSAMPLERSPROC     pGLBindSamplers;
    PFNGLBINDTEXTURESPROC     pGLBindTextures;

    /* GL_ARB_texture_buffer_range */
    PFNGLTEXTUREBUFFERRANGEEXTPROC pGLTextureBufferRangeEXT;

    /* GL_ARB_texture_storage_multisample */
    PFNGLTEXSTORAGE2DMULTISAMPLEPROC pGLTexStorage2DMultisample;
    PFNGLTEXSTORAGE3DMULTISAMPLEPROC pGLTexStorage3DMultisample;

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
    PFNGLDISPATCHCOMPUTEPROC         pGLDispatchCompute;
    PFNGLDISPATCHCOMPUTEINDIRECTPROC pGLDispatchComputeIndirect;
} ogl_context_gl_entrypoints_arb_compute_shader;

typedef struct
{

    GLint max_combined_compute_uniform_components;
    GLint max_compute_atomic_counter_buffers;
    GLint max_compute_atomic_counters;
    GLint max_compute_image_uniforms;
    GLint max_compute_shared_memory_size;
    GLint max_compute_texture_image_units;
    GLint max_compute_uniform_blocks;
    GLint max_compute_uniform_components;
    GLint max_compute_work_group_count[3];
    GLint max_compute_work_group_invocations;
    GLint max_compute_work_group_size[3];

} ogl_context_gl_limits_arb_compute_shader;

typedef struct
{

    PFNGLDEBUGMESSAGECALLBACKARBPROC pGLDebugMessageCallbackARB;
    PFNGLDEBUGMESSAGECONTROLARBPROC  pGLDebugMessageControlARB;
    PFNGLDEBUGMESSAGEINSERTARBPROC   pGLDebugMessageInsertARB;
    PFNGLGETDEBUGMESSAGELOGARBPROC   pGLGetDebugMessageLogARB;

} ogl_context_gl_entrypoints_arb_debug_output;

typedef struct
{
    PFNGLFRAMEBUFFERPARAMETERIPROC     pGLFramebufferParameteri;
    PFNGLGETFRAMEBUFFERPARAMETERIVPROC pGLGetFramebufferParameteriv;
} ogl_context_gl_entrypoints_arb_framebuffer_no_attachments;

typedef struct
{
    GLint max_framebuffer_height;
    GLint max_framebuffer_layers;
    GLint max_framebuffer_samples;
    GLint max_framebuffer_width;
} ogl_context_gl_limits_arb_framebuffer_no_attachments;

typedef struct
{
    PFNGLBINDBUFFERSBASEPROC     pGLBindBuffersBase;
    PFNGLBINDBUFFERSRANGEPROC    pGLBindBuffersRange;
    PFNGLBINDIMAGETEXTURESPROC   pGLBindImageTextures;
    PFNGLBINDSAMPLERSPROC        pGLBindSamplers;
    PFNWRAPPEDGLBINDTEXTURESPROC pGLBindTextures;
} ogl_context_gl_entrypoints_arb_multi_bind;

typedef struct
{
    PFNWRAPPEDGLTEXSTORAGE2DMULTISAMPLEPROC pGLTexStorage2DMultisample;
    PFNWRAPPEDGLTEXSTORAGE3DMULTISAMPLEPROC pGLTexStorage3DMultisample;
} ogl_context_gl_entrypoints_arb_texture_storage_multisample;

typedef struct
{
    PFNWRAPPEDGLBINDMULTITEXTUREEXTPROC                     pGLBindMultiTextureEXT;
    PFNWRAPPEDGLCOMPRESSEDTEXTUREIMAGE1DEXTPROC             pGLCompressedTextureImage1DEXT;
    PFNWRAPPEDGLCOMPRESSEDTEXTUREIMAGE2DEXTPROC             pGLCompressedTextureImage2DEXT;
    PFNWRAPPEDGLCOMPRESSEDTEXTUREIMAGE3DEXTPROC             pGLCompressedTextureImage3DEXT;
    PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE1DEXTPROC          pGLCompressedTextureSubImage1DEXT;
    PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC          pGLCompressedTextureSubImage2DEXT;
    PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE3DEXTPROC          pGLCompressedTextureSubImage3DEXT;
    PFNWRAPPEDGLCOPYTEXTUREIMAGE1DEXTPROC                   pGLCopyTextureImage1DEXT;
    PFNWRAPPEDGLCOPYTEXTUREIMAGE2DEXTPROC                   pGLCopyTextureImage2DEXT;
    PFNWRAPPEDGLCOPYTEXTURESUBIMAGE1DEXTPROC                pGLCopyTextureSubImage1DEXT;
    PFNWRAPPEDGLCOPYTEXTURESUBIMAGE2DEXTPROC                pGLCopyTextureSubImage2DEXT;
    PFNWRAPPEDGLCOPYTEXTURESUBIMAGE3DEXTPROC                pGLCopyTextureSubImage3DEXT;
    PFNGLDISABLEVERTEXARRAYATTRIBEXTPROC                    pGLDisableVertexArrayAttribEXT;
    PFNGLENABLEVERTEXARRAYATTRIBEXTPROC                     pGLEnableVertexArrayAttribEXT;
    PFNGLFRAMEBUFFERDRAWBUFFEREXTPROC                       pGLFramebufferDrawBufferEXT;
    PFNGLFRAMEBUFFERDRAWBUFFERSEXTPROC                      pGLFramebufferDrawBuffersEXT;
    PFNGLFRAMEBUFFERREADBUFFEREXTPROC                       pGLFramebufferReadBufferEXT;
    PFNWRAPPEDGLGENERATETEXTUREMIPMAPEXTPROC                pGLGenerateTextureMipmapEXT;
    PFNWRAPPEDGLGETCOMPRESSEDTEXTUREIMAGEEXTPROC            pGLGetCompressedTextureImageEXT;
    PFNGLGETFRAMEBUFFERPARAMETERIVEXTPROC                   pGLGetFramebufferParameterivEXT;
    PFNGLGETNAMEDBUFFERPARAMETERIVEXTPROC                   pGLGetNamedBufferParameterivEXT;
    PFNGLGETNAMEDBUFFERPOINTERVEXTPROC                      pGLGetNamedBufferPointervEXT;
    PFNGLGETNAMEDBUFFERSUBDATAEXTPROC                       pGLGetNamedBufferSubDataEXT;
    PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC    pGLGetNamedFramebufferAttachmentParameterivEXT;
    PFNGLGETNAMEDPROGRAMIVEXTPROC                           pGLGetNamedProgramivEXT;
    PFNGLGETNAMEDRENDERBUFFERPARAMETERIVEXTPROC             pGLGetNamedRenderbufferParameterivEXT;
    PFNWRAPPEDGLGETTEXTUREIMAGEEXTPROC                      pGLGetTextureImageEXT;
    PFNWRAPPEDGLGETTEXTUREPARAMETERFVEXTPROC                pGLGetTextureLevelParameterfvEXT;
    PFNWRAPPEDGLGETTEXTUREPARAMETERIVEXTPROC                pGLGetTextureLevelParameterivEXT;
    PFNGLMAPNAMEDBUFFEREXTPROC                              pGLMapNamedBufferEXT;
    PFNGLNAMEDBUFFERDATAEXTPROC                             pGLNamedBufferDataEXT;
    PFNGLNAMEDBUFFERSTORAGEEXTPROC                          pGLNamedBufferStorageEXT;
    PFNGLNAMEDBUFFERSUBDATAEXTPROC                          pGLNamedBufferSubDataEXT;
    PFNGLNAMEDCOPYBUFFERSUBDATAEXTPROC                      pGLNamedCopyBufferSubDataEXT;
    PFNGLNAMEDFRAMEBUFFERRENDERBUFFEREXTPROC                pGLNamedFramebufferRenderbufferEXT;
    PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTUREEXTPROC              pGLNamedFramebufferTextureEXT;
    PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURE1DEXTPROC            pGLNamedFramebufferTexture1DEXT;
    PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURE2DEXTPROC            pGLNamedFramebufferTexture2DEXT;
    PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURE3DEXTPROC            pGLNamedFramebufferTexture3DEXT;
    PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTUREFACEEXTPROC          pGLNamedFramebufferTextureFaceEXT;
    PFNWRAPPEDGLNAMEDFRAMEBUFFERTEXTURELAYEREXTPROC         pGLNamedFramebufferTextureLayerEXT;
    PFNGLNAMEDRENDERBUFFERSTORAGEEXTPROC                    pGLNamedRenderbufferStorageEXT;
    PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC         pGLNamedRenderbufferStorageMultisampleEXT;
    PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLECOVERAGEEXTPROC pGLNamedRenderbufferStorageMultisampleCoverageEXT;
    PFNWRAPPEDGLTEXTUREBUFFEREXTPROC                        pGLTextureBufferEXT;
    PFNWRAPPEDGLTEXTUREBUFFERRANGEEXTPROC                   pGLTextureBufferRangeEXT;
    PFNWRAPPEDGLTEXTUREIMAGE1DEXTPROC                       pGLTextureImage1DEXT;
    PFNWRAPPEDGLTEXTUREIMAGE2DEXTPROC                       pGLTextureImage2DEXT;
    PFNWRAPPEDGLTEXTUREIMAGE3DEXTPROC                       pGLTextureImage3DEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERIEXTPROC                    pGLTextureParameteriEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERIVEXTPROC                   pGLTextureParameterivEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERFEXTPROC                    pGLTextureParameterfEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERFVEXTPROC                   pGLTextureParameterfvEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERIIVEXTPROC                  pGLTextureParameterIivEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERIUIVEXTPROC                 pGLTextureParameterIuivEXT;
    PFNWRAPPEDGLTEXTURERENDERBUFFEREXTPROC                  pGLTextureRenderbufferEXT;
    PFNWRAPPEDGLTEXTURESTORAGE1DEXTPROC                     pGLTextureStorage1DEXT;
    PFNWRAPPEDGLTEXTURESTORAGE2DEXTPROC                     pGLTextureStorage2DEXT;
    PFNWRAPPEDGLTEXTURESTORAGE2DMULTISAMPLEEXTPROC          pGLTextureStorage2DMultisampleEXT;
    PFNWRAPPEDGLTEXTURESTORAGE3DEXTPROC                     pGLTextureStorage3DEXT;
    PFNWRAPPEDGLTEXTURESTORAGE3DMULTISAMPLEEXTPROC          pGLTextureStorage3DMultisampleEXT;
    PFNWRAPPEDGLTEXTURESUBIMAGE1DEXTPROC                    pGLTextureSubImage1DEXT;
    PFNWRAPPEDGLTEXTURESUBIMAGE2DEXTPROC                    pGLTextureSubImage2DEXT;
    PFNWRAPPEDGLTEXTURESUBIMAGE3DEXTPROC                    pGLTextureSubImage3DEXT;
    PFNGLUNMAPNAMEDBUFFEREXTPROC                            pGLUnmapNamedBufferEXT;
    PFNGLVERTEXARRAYVERTEXATTRIBOFFSETEXTPROC               pGLVertexArrayVertexAttribOffsetEXT;
    PFNGLVERTEXARRAYVERTEXATTRIBIOFFSETEXTPROC              pGLVertexArrayVertexAttribIOffsetEXT;

} ogl_context_gl_entrypoints_ext_direct_state_access;

typedef struct
{

    PFNGLGETPROGRAMINTERFACEIVPROC           pGLGetProgramInterfaceiv;
    PFNGLGETPROGRAMRESOURCEINDEXPROC         pGLGetProgramResourceIndex;
    PFNGLGETPROGRAMRESOURCEIVPROC            pGLGetProgramResourceiv;
    PFNGLGETPROGRAMRESOURCELOCATIONPROC      pGLGetProgramResourceLocation;
    PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC pGLGetProgramResourceLocationIndex;
    PFNGLGETPROGRAMRESOURCENAMEPROC          pGLGetProgramResourceName;

} ogl_context_gl_entrypoints_arb_program_interface_query;

typedef struct
{

    PFNGLSHADERSTORAGEBLOCKBINDINGPROC pGLShaderStorageBlockBinding;

} ogl_context_gl_entrypoints_arb_shader_storage_buffer_object;


typedef struct
{

    GLint max_combined_shader_storage_blocks;
    GLint max_compute_shader_storage_blocks;
    GLint max_fragment_shader_storage_blocks;
    GLint max_geometry_shader_storage_blocks;
    GLint max_shader_storage_block_size;
    GLint max_shader_storage_buffer_bindings;
    GLint max_tess_control_shader_storage_blocks;
    GLint max_tess_evaluation_shader_storage_blocks;
    GLint max_vertex_shader_storage_blocks;
    GLint shader_storage_buffer_offset_alignment;

} ogl_context_gl_limits_arb_shader_storage_buffer_object;

/** Enumerator that can be used to define property caller wants to find out more details on. */
typedef enum
{
    OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_RED_BITS,
    OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_GREEN_BITS,
    OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_BLUE_BITS,
    OGL_PIXEL_FORMAT_DESCRIPTOR_COLOR_BUFFER_ALPHA_BITS,
    OGL_PIXEL_FORMAT_DESCRIPTOR_DEPTH_BITS
} ogl_pixel_format_descriptor_field;

/** Flyby support handle */
DECLARE_HANDLE(ogl_flyby);

/** Shader variable types */
typedef enum
{
    TYPE_BOOL,
    TYPE_BVEC2,
    TYPE_BVEC3,
    TYPE_BVEC4,
    TYPE_DMAT2,
    TYPE_DMAT2X3,
    TYPE_DMAT2X4,
    TYPE_DMAT3,
    TYPE_DMAT3X2,
    TYPE_DMAT3X4,
    TYPE_DMAT4,
    TYPE_DMAT4X2,
    TYPE_DMAT4X3,
    TYPE_DVEC2,
    TYPE_DVEC3,
    TYPE_DVEC4,
    TYPE_FLOAT,
    TYPE_INT,
    TYPE_ISAMPLER1D,
    TYPE_ISAMPLER1DARRAY,
    TYPE_ISAMPLER2D,
    TYPE_ISAMPLER2DARRAY,
    TYPE_ISAMPLER2DMS,
    TYPE_ISAMPLER2DMSARRAY,
    TYPE_ISAMPLER2DRECT,
    TYPE_ISAMPLER3D,
    TYPE_ISAMPLERBUFFER,
    TYPE_ISAMPLERCUBE,
    TYPE_ISAMPLERCUBEARRAY,
    TYPE_IVEC2,
    TYPE_IVEC3,
    TYPE_IVEC4,
    TYPE_MAT2,
    TYPE_MAT2X3,
    TYPE_MAT2X4,
    TYPE_MAT3,
    TYPE_MAT3X2,
    TYPE_MAT3X4,
    TYPE_MAT4,
    TYPE_MAT4X2,
    TYPE_MAT4X3,
    TYPE_SAMPLER1D,
    TYPE_SAMPLER1DARRAY,
    TYPE_SAMPLER1DARRAYSHADOW,
    TYPE_SAMPLER1DSHADOW,
    TYPE_SAMPLER2D,
    TYPE_SAMPLER2DARRAY,
    TYPE_SAMPLER2DARRAYSHADOW,
    TYPE_SAMPLER2DMS,
    TYPE_SAMPLER2DMSARRAY,
    TYPE_SAMPLER2DRECT,
    TYPE_SAMPLER2DRECTSHADOW,
    TYPE_SAMPLER2DSHADOW,
    TYPE_SAMPLER3D,
    TYPE_SAMPLERBUFFER,
    TYPE_SAMPLERCUBE,
    TYPE_SAMPLERCUBEARRAY,
    TYPE_SAMPLERCUBEARRAYSHADOW,
    TYPE_SAMPLERCUBESHADOW,
    TYPE_UINT,
    TYPE_USAMPLER1D,
    TYPE_USAMPLER1DARRAY,
    TYPE_USAMPLER2D,
    TYPE_USAMPLER2DARRAY,
    TYPE_USAMPLER2DMS,
    TYPE_USAMPLER2DMSARRAY,
    TYPE_USAMPLER2DRECT,
    TYPE_USAMPLER3D,
    TYPE_USAMPLERBUFFER,
    TYPE_USAMPLERCUBE,
    TYPE_USAMPLERCUBEARRAY,
    TYPE_UVEC2,
    TYPE_UVEC3,
    TYPE_UVEC4,
    TYPE_VEC2,
    TYPE_VEC3,
    TYPE_VEC4,
    TYPE_VOID,

    /* Always last */
    TYPE_UNKNOWN
} _shader_variable_type;

/** Program handle */
DECLARE_HANDLE(ogl_program);

typedef unsigned int ogl_program_uniform_block_id;

/** Shader handle & various related types from shaders module. */
DECLARE_HANDLE(ogl_shader);
DECLARE_HANDLE(ogl_shader_constructor);
DECLARE_HANDLE(ogl_uber);
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
DECLARE_HANDLE(shaders_vertex_combinedmvp_ubo);
DECLARE_HANDLE(shaders_vertex_fullscreen);
DECLARE_HANDLE(shaders_vertex_m_vp_generic);
DECLARE_HANDLE(shaders_vertex_model_mvp_combiner);
DECLARE_HANDLE(shaders_vertex_mvp_combiner);
DECLARE_HANDLE(shaders_vertex_uber);

/** Rendering handler handle */
DECLARE_HANDLE(ogl_rendering_handler);

/** Pixel format descriptor handle */
DECLARE_HANDLE(ogl_pixel_format_descriptor);

/** OpenGL context handle */
DECLARE_HANDLE(ogl_context);

/* Materials library handle */
DECLARE_HANDLE(ogl_materials);

/** Pipeline handle */
DECLARE_HANDLE(ogl_pipeline);

/** Skybox handle */
DECLARE_HANDLE(ogl_skybox);

/** Text handle */
DECLARE_HANDLE(ogl_text);

/* UI renderer and handler */
DECLARE_HANDLE(ogl_ui);

typedef enum
{
    OGL_UI_TEXTURE_PREVIEW_TYPE_ALPHA,
    OGL_UI_TEXTURE_PREVIEW_TYPE_DEPTH,
    OGL_UI_TEXTURE_PREVIEW_TYPE_RED,
    OGL_UI_TEXTURE_PREVIEW_TYPE_RGB,
    OGL_UI_TEXTURE_PREVIEW_TYPE_RGBA,
} ogl_ui_texture_preview_type;

/* UI control handle */
DECLARE_HANDLE(ogl_ui_control);

/** Rendering handler call-back */
typedef void (*PFNOGLRENDERINGHANDLERRENDERINGCALLBACK)   (ogl_context /* context */, uint32_t /* n_frame */, system_timeline_time /* frame time */, void* user_arg);
typedef void (*PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC)(ogl_context /* context */, void* /* user argument */);

/*************** Type handlers ***********************/
/** Releases all fields of an _ogl_context_gl_info structure without actually releasing
 *  the @param info ptr.
 *
 *  @param info Descriptor to deinitialize.
 */
PUBLIC void deinit_ogl_context_gl_info(__in __notnull __deallocate(mem) ogl_context_gl_info* info);

PUBLIC void init_ogl_context_gl_info(__in __notnull ogl_context_gl_info* info, const __in __notnull ogl_context_gl_limits* limits);

#endif /* OGL_TYPES_H */
