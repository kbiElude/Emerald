/**
 *
 * Emerald (kbi/elude @2015-2016)
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
    /* GLuint. Rendering context setting.
     *
     * This state must be synchronized with the local value before
     * a VAO-dependent ES/GL call is made. The synchronization is
     * handled by ogl_context_state_cache.
     */
    OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_CONTEXT,

    /* GLuint. Local setting. */
    OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_LOCAL,
} ogl_vao_property;

typedef enum
{
    /* GLboolean */
    OGL_VAO_VAA_PROPERTY_ENABLED,
    /* GLboolean */
    OGL_VAO_VAA_PROPERTY_NORMALIZED,
    /* GLintptr */
    OGL_VAO_VAA_PROPERTY_RELATIVE_OFFSET,
    /* GLint */
    OGL_VAO_VAA_PROPERTY_SIZE,
    /* GLint */
    OGL_VAO_VAA_PROPERTY_STRIDE,
    /* GLenum */
    OGL_VAO_VAA_PROPERTY_TYPE,

    /* uint32_t */
    OGL_VAO_VAA_PROPERTY_VB_BINDING,
} ogl_vao_vaa_property;

typedef enum
{
    /* GLuint */
    OGL_VAO_VB_PROPERTY_BUFFER,

    /* GLuint */
    OGL_VAO_VB_PROPERTY_DIVISOR,

    /* GLintptr */
    OGL_VAO_VB_PROPERTY_OFFSET,

    /* GLsizei */
    OGL_VAO_VB_PROPERTY_STRIDE,
} ogl_vao_vb_property;

/** TODO */
PUBLIC ogl_vao ogl_vao_create(ogl_context  context,
                              unsigned int gl_id);

/** TODO */
PUBLIC void ogl_vao_get_property(const ogl_vao    vao,
                                 ogl_vao_property property,
                                 void*            out_result_ptr);

/** TODO */
PUBLIC void ogl_vao_get_vaa_property(const ogl_vao        vao,
                                     unsigned int         n_vaa,
                                     ogl_vao_vaa_property property,
                                     void*                out_result_ptr);

/** TODO */
PUBLIC void ogl_vao_get_vb_property(const ogl_vao       vao,
                                    unsigned int        n_vb,
                                    ogl_vao_vb_property property,
                                    void*               out_result_ptr);

/** TODO */
PUBLIC void ogl_vao_release(ogl_vao vao);

/** TODO */
PUBLIC void ogl_vao_set_property(ogl_vao          vao,
                                 ogl_vao_property property,
                                 const void*      data);

/** TODO */
PUBLIC void ogl_vao_set_vaa_property(ogl_vao              vao,
                                     unsigned int         n_vaa,
                                     ogl_vao_vaa_property property,
                                     const void*          data);

/** TODO */
PUBLIC void ogl_vao_set_vb_property(ogl_vao             vao,
                                    unsigned int        n_vb,
                                    ogl_vao_vb_property property,
                                    const void*         data);

#endif /* OGL_VAO_H */
