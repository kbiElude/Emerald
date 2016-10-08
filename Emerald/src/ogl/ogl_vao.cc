/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_vaos.h"
#include "ogl/ogl_vao.h"


/** TODO */
typedef struct _ogl_vao_vaa
{
    GLboolean enabled;
    GLboolean integer;
    GLboolean normalized;
    GLintptr  relative_offset;
    GLint     size;
    GLint     stride;
    GLenum    type;
    uint32_t  vb_binding;

    explicit _ogl_vao_vaa()
    {
        enabled         = GL_FALSE;
        integer         = GL_FALSE;
        normalized      = GL_FALSE;
        relative_offset = 0;
        size            = 4;
        stride          = 0;
        type            = GL_FLOAT;
        vb_binding      = -1;
    }
} _ogl_vao_vaa;

/** TODO */
typedef struct _ogl_vao_vb
{
    GLint    array_buffer_binding;
    GLuint   divisor;
    GLintptr offset;
    GLint    stride;

    _ogl_vao_vb()
    {
        array_buffer_binding = 0;
        divisor              = 0;
        offset               = 0;
        stride               = 16;
    }
} _ogl_vao_vb;

typedef struct _ogl_vao
{
    /* Do NOT retain */
    ogl_context context;

    GLuint        gl_id;
    GLuint        index_buffer_binding_context;
    GLuint        index_buffer_binding_local;
    unsigned int  n_vaas;
    unsigned int  n_vbs;
    _ogl_vao_vaa* vaas;
    _ogl_vao_vb*  vbs;

    explicit _ogl_vao(ogl_context  in_context,
                      unsigned int in_gl_id)
    {
        ral_backend_type backend_type                 = RAL_BACKEND_TYPE_UNKNOWN;
        unsigned int     n_max_vertex_attribs         = 0;
        unsigned int     n_max_vertex_attrib_bindings = 0;

        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_GL)
        {
            const ogl_context_gl_limits* limits_ptr = nullptr;

            ogl_context_get_property(in_context,
                                     OGL_CONTEXT_PROPERTY_LIMITS,
                                    &limits_ptr);

            n_max_vertex_attrib_bindings = limits_ptr->max_vertex_attrib_bindings;
            n_max_vertex_attribs         = limits_ptr->max_vertex_attribs;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_ES,
                              "Unrecognized rendering backend type.");

            ASSERT_DEBUG_SYNC(false,
                              "TODO: ES limits");
        }

        ASSERT_DEBUG_SYNC(n_max_vertex_attribs != 0,
                          "GL_MAX_VERTEX_ATTRIBS GL constant value is 0");
        ASSERT_DEBUG_SYNC(n_max_vertex_attrib_bindings != 0,
                          "GL_MAX_VERTEX_ATTRIB_BINDINGS GL constant value is 0");

        context                      = in_context;
        gl_id                        = in_gl_id;
        index_buffer_binding_context = 0;
        index_buffer_binding_local   = 0;
        n_vaas                       = n_max_vertex_attribs;
        n_vbs                        = n_max_vertex_attrib_bindings;
        vaas                         = new (std::nothrow) _ogl_vao_vaa[n_vaas];
        vbs                          = new (std::nothrow) _ogl_vao_vb [n_vbs];

        ASSERT_DEBUG_SYNC(vaas != nullptr && vbs != nullptr,
                          "Out of memory");

        for (uint32_t n_vaa = 0;
                      n_vaa < n_vaas;
                    ++n_vaa)
        {
            vaas[n_vaa].vb_binding = n_vaa;
        }
    }

    ~_ogl_vao()
    {
        /* Try to unregister the VAO from the VAO cache */
        ogl_context_vaos vaos = nullptr;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_VAOS,
                                &vaos);

        ogl_context_vaos_delete_vao(vaos,
                                    gl_id);

        /* Release other stuff */
        if (vaas != nullptr)
        {
            delete [] vaas;

            vaas = nullptr;
        }

        if (vbs != nullptr)
        {
            delete [] vbs;

            vbs = nullptr;
        }
    }
} _ogl_vao;


/** Please see header for spec */
PUBLIC ogl_vao ogl_vao_create(ogl_context  context,
                              unsigned int gl_id)
{
    _ogl_vao* new_instance = new (std::nothrow) _ogl_vao(context,
                                                         gl_id);

    ASSERT_ALWAYS_SYNC(new_instance != nullptr,
                       "Out of memory");

    return (ogl_vao) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_vao_get_property(const ogl_vao    vao,
                                 ogl_vao_property property,
                                 void*            out_result_ptr)
{
    const _ogl_vao* vao_ptr = reinterpret_cast<const _ogl_vao*>(vao);

    switch (property)
    {
        case OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_CONTEXT:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = vao_ptr->index_buffer_binding_context;

            break;
        }

        case OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_LOCAL:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = vao_ptr->index_buffer_binding_local;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_vao_property value.");
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_vao_get_vaa_property(const ogl_vao        vao,
                                     unsigned int         n_vaa,
                                     ogl_vao_vaa_property property,
                                     void*                out_result_ptr)
{
    const _ogl_vao* vao_ptr = (const _ogl_vao*) vao;

    ASSERT_DEBUG_SYNC(n_vaa < vao_ptr->n_vaas,
                      "Invalid VAA index requested.");

    if (n_vaa < vao_ptr->n_vaas)
    {
        const _ogl_vao_vaa& vaa = vao_ptr->vaas[n_vaa];

        switch (property)
        {
            case OGL_VAO_VAA_PROPERTY_ENABLED:
            {
                *reinterpret_cast<GLboolean*>(out_result_ptr) = vaa.enabled;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_NORMALIZED:
            {
                *reinterpret_cast<GLboolean*>(out_result_ptr) = vaa.normalized;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_RELATIVE_OFFSET:
            {
                *reinterpret_cast<GLintptr*>(out_result_ptr) = vaa.relative_offset;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_SIZE:
            {
                *reinterpret_cast<GLuint*>(out_result_ptr) = vaa.size;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_STRIDE:
            {
                *reinterpret_cast<GLuint*>(out_result_ptr) = vaa.stride;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_TYPE:
            {
                *reinterpret_cast<GLenum*>(out_result_ptr) = vaa.type;

                break;
            }

            case OGL_VAO_VAA_PROPERTY_VB_BINDING:
            {
                *reinterpret_cast<uint32_t*>(out_result_ptr) = vaa.vb_binding;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized VAO VAA property");
            }
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_vao_get_vb_property(const ogl_vao       vao,
                                    unsigned int        n_vb,
                                    ogl_vao_vb_property property,
                                    void*               out_result_ptr)
{
    const _ogl_vao* vao_ptr = reinterpret_cast<const _ogl_vao*>(vao);

    ASSERT_DEBUG_SYNC(n_vb < vao_ptr->n_vbs,
                      "Invalid VB index requested.");

    if (n_vb < vao_ptr->n_vbs)
    {
        _ogl_vao_vb* vb_ptr = vao_ptr->vbs + n_vb;

        switch (property)
        {
            case OGL_VAO_VB_PROPERTY_BUFFER:
            {
                *reinterpret_cast<GLuint*>(out_result_ptr) = vb_ptr->array_buffer_binding;

                break;
            }

            case OGL_VAO_VB_PROPERTY_DIVISOR:
            {
                *reinterpret_cast<GLuint*>(out_result_ptr) = vb_ptr->divisor;

                break;
            }

            case OGL_VAO_VB_PROPERTY_OFFSET:
            {
                *reinterpret_cast<GLintptr*>(out_result_ptr) = vb_ptr->offset;

                break;
            }

            case OGL_VAO_VB_PROPERTY_STRIDE:
            {
                *reinterpret_cast<GLuint*>(out_result_ptr) = vb_ptr->stride;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized VAO VB property");
            }
        }
    }
}

/** Please see header for spec */
PUBLIC void ogl_vao_release(ogl_vao vao)
{
    _ogl_vao* vao_ptr = reinterpret_cast<_ogl_vao*>(vao);

    /* Done */
    delete vao_ptr;

    vao_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC void ogl_vao_set_property(ogl_vao          vao,
                                 ogl_vao_property property,
                                 const void*      data)
{
    _ogl_vao* vao_ptr = reinterpret_cast<_ogl_vao*>(vao);

    switch (property)
    {
        case OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_CONTEXT:
        {
            vao_ptr->index_buffer_binding_context = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        case OGL_VAO_PROPERTY_INDEX_BUFFER_BINDING_LOCAL:
        {
            vao_ptr->index_buffer_binding_local = *reinterpret_cast<const GLuint*>(data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_vao_property value.");
        }
    }
}

/** TODO */
PUBLIC void ogl_vao_set_vaa_property(ogl_vao              vao,
                                     unsigned int         n_vaa,
                                     ogl_vao_vaa_property property,
                                     const void*          data)
{
    _ogl_vao* vao_ptr = reinterpret_cast<_ogl_vao*>(vao);

    ASSERT_DEBUG_SYNC(n_vaa < vao_ptr->n_vaas,
                      "Invalid VAA index requested.");

    if (n_vaa < vao_ptr->n_vaas)
    {
        _ogl_vao_vaa& vaa = vao_ptr->vaas[n_vaa];

        switch (property)
        {
            case OGL_VAO_VAA_PROPERTY_ENABLED:
            {
                vaa.enabled = *reinterpret_cast<const GLboolean*>(data);

                break;
            }

            case OGL_VAO_VAA_PROPERTY_NORMALIZED:
            {
                vaa.normalized = *reinterpret_cast<const GLboolean*>(data);

                break;
            }

            case OGL_VAO_VAA_PROPERTY_RELATIVE_OFFSET:
            {
                vaa.relative_offset = *reinterpret_cast<const GLintptr*>(data);

                break;
            }

            case OGL_VAO_VAA_PROPERTY_SIZE:
            {
                vaa.size = *reinterpret_cast<const GLuint*>(data);

                break;
            }

            case OGL_VAO_VAA_PROPERTY_STRIDE:
            {
                vaa.stride = *reinterpret_cast<const GLuint*>(data);

                break;
            }

            case OGL_VAO_VAA_PROPERTY_TYPE:
            {
                vaa.type = *reinterpret_cast<const GLenum*>(data);

                break;
            }

            case OGL_VAO_VAA_PROPERTY_VB_BINDING:
            {
                vaa.vb_binding = *reinterpret_cast<const uint32_t*>(data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized VAO VAA property");
            }
        }
    }
}

/** TODO */
PUBLIC void ogl_vao_set_vb_property(ogl_vao             vao,
                                    unsigned int        n_vb,
                                    ogl_vao_vb_property property,
                                    const void*         data)
{
    _ogl_vao* vao_ptr = reinterpret_cast<_ogl_vao*>(vao);

    ASSERT_DEBUG_SYNC(n_vb < vao_ptr->n_vbs,
                      "Invalid VB index requested.");

    if (n_vb < vao_ptr->n_vbs)
    {
        _ogl_vao_vb& vb = vao_ptr->vbs[n_vb];

        switch (property)
        {
            case OGL_VAO_VB_PROPERTY_BUFFER:
            {
                vb.array_buffer_binding = *reinterpret_cast<const GLuint*>(data);

                break;
            }

            /* GLuint */
            case OGL_VAO_VB_PROPERTY_DIVISOR:
            {
                vb.divisor = *reinterpret_cast<const GLuint*>(data);

                break;
            }

            /* GLintptr */
            case OGL_VAO_VB_PROPERTY_OFFSET:
            {
                vb.offset = *reinterpret_cast<const GLintptr*>(data);

                break;
            }

            /* GLsizei */
            case OGL_VAO_VB_PROPERTY_STRIDE:
            {
                vb.stride = *reinterpret_cast<const GLsizei*>(data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ogl_vao_vb_property enum value specified.");
            }
        }
    }
}