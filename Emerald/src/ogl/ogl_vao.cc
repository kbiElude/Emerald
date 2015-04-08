/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_vao.h"
#include "ogl/ogl_vaos.h"


/** TODO */
typedef struct _ogl_vao_vaa
{
    GLint     array_buffer_binding;
    GLint     divisor;
    GLboolean enabled;
    GLboolean integer;
    GLboolean normalized;
    GLvoid*   pointer;
    GLint     size;
    GLint     stride;
    GLenum    type;

    _ogl_vao_vaa()
    {
        array_buffer_binding = 0;
        divisor              = 0;
        enabled              = GL_FALSE;
        normalized           = GL_FALSE;
        pointer              = NULL;
        size                 = 4;
        stride               = 0;
        type                 = GL_FLOAT;
    }
} _ogl_vao_vaa;

typedef struct _ogl_vao
{
    /* Do NOT retain */
    ogl_context context;

    GLuint        gl_id;
    GLuint        index_buffer_binding;
    unsigned int  n_vaas;
    _ogl_vao_vaa* vaas;

    explicit _ogl_vao(__in __notnull ogl_context  in_context,
                      __in           unsigned int in_gl_id)
    {
        ogl_context_type context_type         = OGL_CONTEXT_TYPE_UNDEFINED;
        unsigned int     n_max_vertex_attribs = 0;

        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_GL)
        {
            const ogl_context_gl_limits* limits_ptr = NULL;

            ogl_context_get_property(in_context,
                                     OGL_CONTEXT_PROPERTY_LIMITS,
                                    &limits_ptr);

            n_max_vertex_attribs = limits_ptr->max_vertex_attribs;
        } /* if (context_type == OGL_CONTEXT_TYPE_GL) */
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_ES,
                              "Unrecognized rendering context type.");

            ASSERT_DEBUG_SYNC(false,
                              "TODO: ES limits");
        }

        ASSERT_DEBUG_SYNC(n_max_vertex_attribs != 0,
                          "GL_MAX_VERTEX_ATTRIBS GL constant value is 0");

        context = in_context;
        gl_id   = in_gl_id;
        n_vaas  = n_max_vertex_attribs;
        vaas    = new (std::nothrow) _ogl_vao_vaa[n_vaas];

        ASSERT_DEBUG_SYNC(vaas != NULL,
                          "Out of memory");
    }

    ~_ogl_vao()
    {
        /* Try to unregister the VAO from the VAO cache */
        ogl_vaos vaos = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_VAOS,
                                &vaos);

        ogl_vaos_delete_vao(vaos,
                            gl_id);

        /* Release other stuff */
        if (vaas != NULL)
        {
            delete [] vaas;

            vaas = NULL;
        }
    }
} _ogl_vao;


/** Please see header for spec */
PUBLIC ogl_vao ogl_vao_create(__in __notnull ogl_context  context,
                              __in           unsigned int gl_id)
{
    _ogl_vao* new_instance = new (std::nothrow) _ogl_vao(context,
                                                         gl_id);

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    return (ogl_vao) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_vao_get_property(__in  __notnull const ogl_vao    vao,
                                 __in            ogl_vao_property property,
                                 __out __notnull void*            out_result)
{
    const _ogl_vao* vao_ptr = (const _ogl_vao*) vao;

    switch (property)
    {
        case OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING:
        {
            *(GLuint*) out_result = vao_ptr->index_buffer_binding;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_vao_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_vao_get_vaa_property(__in  __notnull const ogl_vao        vao,
                                     __in            unsigned int         n_vaa,
                                     __in            ogl_vao_vaa_property property,
                                     __out __notnull void*                out_result)
{
    const _ogl_vao* vao_ptr = (const _ogl_vao*) vao;

    ASSERT_DEBUG_SYNC(n_vaa < vao_ptr->n_vaas,
                      "Invalid VAA index requested.");

    if (n_vaa < vao_ptr->n_vaas)
    {
        const _ogl_vao_vaa& vaa = vao_ptr->vaas[n_vaa];

        switch (property)
        {
            case OGL_VAO_VAA_PROPERTY_ARRAY_BUFFER_BINDING:
            {
                *(GLuint*) out_result = vaa.array_buffer_binding;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_DIVISOR:
            {
                *(GLuint*) out_result = vaa.divisor;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_ENABLED:
            {
                *(GLboolean*) out_result = vaa.enabled;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_NORMALIZED:
            {
                *(GLboolean*) out_result = vaa.normalized;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_POINTER:
            {
                *(GLvoid**) out_result = vaa.pointer;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_SIZE:
            {
                *(GLuint*) out_result = vaa.size;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_STRIDE:
            {
                *(GLuint*) out_result = vaa.stride;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_TYPE:
            {
                *(GLenum*) out_result = vaa.type;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized VAO VAA property");
            }
        } /* switch (property) */
    } /* if (n_vaa < vao_ptr->n_vaas) */
}

/** Please see header for spec */
PUBLIC void ogl_vao_release(__in __notnull __post_invalid ogl_vao vao)
{
    _ogl_vao* vao_ptr = (_ogl_vao*) vao;

    /* Done */
    delete vao_ptr;

    vao_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_vao_set_property(__in __notnull ogl_vao          vao,
                                 __in           ogl_vao_property property,
                                 __in __notnull const void*      data)
{
    _ogl_vao* vao_ptr = (_ogl_vao*) vao;

    switch (property)
    {
        case OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING:
        {
            vao_ptr->index_buffer_binding = *(GLuint*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_vao_property value.");
        }
    } /* switch (property) */
}

/** TODO */
PUBLIC void ogl_vao_set_vaa_property(__in __notnull ogl_vao              vao,
                                     __in           unsigned int         n_vaa,
                                     __in           ogl_vao_vaa_property property,
                                     __in __notnull const void*          data)
{
    _ogl_vao* vao_ptr = (_ogl_vao*) vao;

    ASSERT_DEBUG_SYNC(n_vaa < vao_ptr->n_vaas,
                      "Invalid VAA index requested.");

    if (n_vaa < vao_ptr->n_vaas)
    {
        _ogl_vao_vaa& vaa = vao_ptr->vaas[n_vaa];

        switch (property)
        {
            case OGL_VAO_VAA_PROPERTY_ARRAY_BUFFER_BINDING:
            {
                vaa.array_buffer_binding = *(GLuint*) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_DIVISOR:
            {
                vaa.divisor = *(GLuint*) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_ENABLED:
            {
                vaa.enabled = *(GLboolean*) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_NORMALIZED:
            {
                vaa.normalized = *(GLboolean*) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_POINTER:
            {
                vaa.pointer = *(GLvoid**) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_SIZE:
            {
                vaa.size = *(GLuint*) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_STRIDE:
            {
                vaa.stride = *(GLuint*) data;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_TYPE:
            {
                vaa.type = *(GLenum*) data;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized VAO VAA property");
            }
        } /* switch (property) */
    } /* if (n_vaa < vao_ptr->n_vaas) */
}
