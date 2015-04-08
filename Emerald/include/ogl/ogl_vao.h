/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO: Vertex attribute array buffer bindings support (GL_ARB_vertex_attrib_binding!)
 *
 * Internal use only.
 */
#ifndef OGL_VAO_H
#define OGL_VAO_H

#include "ogl/ogl_types.h"


typedef enum
{
    /* GLuint */
    OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING
} ogl_vao_property;

typedef enum
{
    /* GLuint */
    OGL_VAO_VAA_PROPERTY_ARRAY_BUFFER_BINDING,
    /* GLint */
    OGL_VAO_VAA_PROPERTY_DIVISOR,
    /* GLboolean */
    OGL_VAO_VAA_PROPERTY_ENABLED,
    /* GLboolean */
    OGL_VAO_VAA_PROPERTY_NORMALIZED,
    /* GLvoid* */
    OGL_VAO_VAA_PROPERTY_POINTER,
    /* GLint */
    OGL_VAO_VAA_PROPERTY_SIZE,
    /* GLint */
    OGL_VAO_VAA_PROPERTY_STRIDE,
    /* GLenum */
    OGL_VAO_VAA_PROPERTY_TYPE
} ogl_vao_vaa_property;

/* Declare private handle */
DECLARE_HANDLE(ogl_vao);


/** TODO */
PUBLIC ogl_vao ogl_vao_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_vao_get_property(__in  __notnull const ogl_vao    vao,
                                 __in            ogl_vao_property property,
                                 __out __notnull void*            out_result);

/** TODO */
PUBLIC void ogl_vao_get_vaa_property(__in  __notnull const ogl_vao        vao,
                                     __in            unsigned int         n_vaa,
                                     __in            ogl_vao_vaa_property property,
                                     __out __notnull void*                out_result);

/** TODO */
PUBLIC void ogl_vao_release(__in __notnull __post_invalid ogl_vao vao);

/** TODO */
PUBLIC void ogl_vao_set_property(__in __notnull ogl_vao          vao,
                                 __in           ogl_vao_property property,
                                 __in __notnull const void*      data);

/** TODO */
PUBLIC void ogl_vao_set_vaa_property(__in __notnull ogl_vao              vao,
                                     __in           unsigned int         n_vaa,
                                     __in           ogl_vao_vaa_property property,
                                     __in __notnull const void*          data);

#endif /* OGL_VAO_H */
