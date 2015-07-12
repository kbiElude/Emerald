
/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_sampler.h"
#include "system/system_log.h"


/* Private declarations */
typedef struct _ogl_sampler
{
    ogl_context               context;
    GLuint                    gl_id;
    bool                      is_locked;
    system_hashed_ansi_string name;

    GLfloat border_color[4];
    GLenum  mag_filter;
    GLfloat max_lod;
    GLenum  min_filter;
    GLfloat min_lod;
    GLenum  texture_compare_func;
    GLenum  texture_compare_mode;
    GLenum  wrap_r;
    GLenum  wrap_s;
    GLenum  wrap_t;

    explicit _ogl_sampler(ogl_context in_context)
    {
        memset(border_color,
               0,
               sizeof(border_color) );

        context              = in_context;
        gl_id                = 0;
        is_locked            = false;
        mag_filter           = GL_LINEAR;
        max_lod              = 1000;
        min_filter           = GL_NEAREST_MIPMAP_LINEAR;
        min_lod              = -1000;
        texture_compare_func = GL_LEQUAL;
        texture_compare_mode = GL_NONE;
        wrap_r               = GL_REPEAT;
        wrap_s               = GL_REPEAT;
        wrap_t               = GL_REPEAT;
    }

    REFCOUNT_INSERT_VARIABLES
} _ogl_sampler;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_sampler,
                               ogl_sampler,
                               _ogl_sampler);

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_sampler_verify_context_type(ogl_context);
#else
    #define _ogl_sampler_verify_context_type(x)
#endif


/* TODO */
PRIVATE void _ogl_sampler_release(void* arg)
{
    _ogl_sampler*                     sampler_ptr = (_ogl_sampler*) arg;
    const ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(sampler_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    if (sampler_ptr->gl_id != 0)
    {
        entrypoints->pGLDeleteSamplers(1 /* n */,
                                      &sampler_ptr->gl_id);

        sampler_ptr->gl_id = 0;
    }
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_sampler_verify_context_type(ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_sampler is only supported under GL contexts")
    }
#endif

/* Please see header for specification */
PUBLIC ogl_sampler ogl_sampler_create(ogl_context               context,
                                      system_hashed_ansi_string name)
{
    _ogl_sampler* new_sampler = new (std::nothrow) _ogl_sampler(context);

    _ogl_sampler_verify_context_type(context);

    ASSERT_ALWAYS_SYNC(new_sampler != NULL,
                       "Out of memory");

    if (new_sampler != NULL)
    {
        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                               &entry_points);

        memset(new_sampler,
               0,
               sizeof(_ogl_sampler) );

        new_sampler->context = context;
        new_sampler->name    = name;

        entry_points->pGLGenSamplers(1,
                                    &new_sampler->gl_id);

        /* Initialize reference counting */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_sampler,
                                                       _ogl_sampler_release,
                                                       OBJECT_TYPE_OGL_SAMPLER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Samplers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_sampler) new_sampler;
}

/* Please see header for spec */
PUBLIC EMERALD_API GLuint ogl_sampler_get_id(ogl_sampler sampler)
{
    return ((_ogl_sampler*) sampler)->gl_id;
}

/* Please see header for spec */
PUBLIC void ogl_sampler_get_property(const ogl_sampler    sampler,
                                     ogl_sampler_property property,
                                     void*                out_result)
{
    const _ogl_sampler* sampler_ptr = (const _ogl_sampler*) sampler;

    if (sampler_ptr == NULL)
    {
        goto end;
    }

    switch (property)
    {
        case OGL_SAMPLER_PROPERTY_BORDER_COLOR:
        {
            *((const float**) out_result) = sampler_ptr->border_color;

            break;
        }

        case OGL_SAMPLER_PROPERTY_LOCKED:
        {
            *(bool*) out_result = sampler_ptr->is_locked;

            break;
        }

        case OGL_SAMPLER_PROPERTY_MAG_FILTER:
        {
            *((GLenum*) out_result) = sampler_ptr->mag_filter;

            break;
        }

        case OGL_SAMPLER_PROPERTY_MAX_LOD:
        {
            *((GLfloat*) out_result) = sampler_ptr->max_lod;

            break;
        }

        case OGL_SAMPLER_PROPERTY_MIN_FILTER:
        {
            *((GLenum*) out_result) = sampler_ptr->min_filter;

            break;
        }

        case OGL_SAMPLER_PROPERTY_MIN_LOD:
        {
            *((GLfloat*) out_result) = sampler_ptr->min_lod;

            break;
        }

        case OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_FUNC:
        {
            *((GLenum*) out_result) = sampler_ptr->texture_compare_func;

            break;
        }

        case OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_MODE:
        {
            *((GLenum*) out_result) = sampler_ptr->texture_compare_mode;

            break;
        }

        case OGL_SAMPLER_PROPERTY_WRAP_R:
        {
            *((GLenum*) out_result) = sampler_ptr->wrap_r;

            break;
        }

        case OGL_SAMPLER_PROPERTY_WRAP_S:
        {
            *((GLenum*) out_result) = sampler_ptr->wrap_s;

            break;
        }

        case OGL_SAMPLER_PROPERTY_WRAP_T:
        {
            *((GLenum*) out_result) = sampler_ptr->wrap_t;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_sampler property requested");
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for spec */
PUBLIC void ogl_sampler_set_property(ogl_sampler          sampler,
                                     ogl_sampler_property property,
                                     const void*          in_data)
{
    _ogl_sampler*                     sampler_ptr = (_ogl_sampler*) sampler;
    const ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(sampler_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    if (sampler_ptr == NULL)
    {
        goto end;
    }

    if (property != OGL_SAMPLER_PROPERTY_LOCKED)
    {
        ASSERT_ALWAYS_SYNC(!sampler_ptr->is_locked,
                           "Cannot change ogl_sampler property - sampler locked.");

        if (sampler_ptr->is_locked)
        {
            goto end;
        }
    }

    switch (property)
    {
        case OGL_SAMPLER_PROPERTY_BORDER_COLOR:
        {
            memcpy(sampler_ptr->border_color,
                   (float*) in_data,
                   sizeof(sampler_ptr->border_color) );

            entrypoints->pGLSamplerParameterfv(sampler_ptr->gl_id,
                                               GL_TEXTURE_BORDER_COLOR,
                                               (const GLfloat*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_LOCKED:
        {
            sampler_ptr->is_locked = *(bool*) in_data;

            break;
        }

        case OGL_SAMPLER_PROPERTY_MAG_FILTER:
        {
            sampler_ptr->mag_filter = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_MAG_FILTER,
                                               (const GLint*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_MAX_LOD:
        {
            sampler_ptr->max_lod = *((GLfloat*) in_data);

            entrypoints->pGLSamplerParameterfv(sampler_ptr->gl_id,
                                               GL_TEXTURE_MAX_LOD,
                                               (const GLfloat*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_MIN_FILTER:
        {
            sampler_ptr->min_filter = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_MIN_FILTER,
                                               (const GLint*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_MIN_LOD:
        {
            sampler_ptr->min_lod = *((GLfloat*) in_data);

            entrypoints->pGLSamplerParameterfv(sampler_ptr->gl_id,
                                               GL_TEXTURE_MIN_LOD,
                                               (const GLfloat*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_FUNC:
        {
            sampler_ptr->texture_compare_func = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_COMPARE_FUNC,
                                               (const GLint*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_MODE:
        {
            sampler_ptr->texture_compare_mode = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_COMPARE_MODE,
                                               (const GLint*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_WRAP_R:
        {
            sampler_ptr->wrap_r = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_WRAP_R,
                                               (const GLint*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_WRAP_S:
        {
            sampler_ptr->wrap_s = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_WRAP_S,
                                               (const GLint*) in_data);

            break;
        }

        case OGL_SAMPLER_PROPERTY_WRAP_T:
        {
            sampler_ptr->wrap_t = *((GLenum*) in_data);

            entrypoints->pGLSamplerParameteriv(sampler_ptr->gl_id,
                                               GL_TEXTURE_WRAP_T,
                                               (const GLint*) in_data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_sampler property requested");
        }
    } /* switch (property) */

end:
    ;
}
