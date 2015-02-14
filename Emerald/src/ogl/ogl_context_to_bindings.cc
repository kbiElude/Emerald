/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_to_bindings.h"
#include "ogl/ogl_texture.h"
#include "system/system_hash64map.h"

typedef enum
{
    /* NOTE: This ordering must match the one used for ogl_context_to_bindings_sync_bit */
    BINDING_TARGET_TEXTURE_1D,
    BINDING_TARGET_TEXTURE_1D_ARRAY,
    BINDING_TARGET_TEXTURE_2D,
    BINDING_TARGET_TEXTURE_2D_ARRAY,
    BINDING_TARGET_TEXTURE_2D_MULTISAMPLE,
    BINDING_TARGET_TEXTURE_2D_MULTISAMPLE_ARRAY,
    BINDING_TARGET_TEXTURE_3D,
    BINDING_TARGET_TEXTURE_BUFFER,
    BINDING_TARGET_TEXTURE_CUBE_MAP,
    BINDING_TARGET_TEXTURE_CUBE_MAP_ARRAY,
    BINDING_TARGET_TEXTURE_RECTANGLE,

    /* Always last */
    BINDING_TARGET_COUNT
} _ogl_context_to_binding_target;

/** TODO */
typedef struct _ogl_context_to_bindings_to_info
{
    ogl_texture texture_context;
    ogl_texture texture_local;

    _ogl_context_to_bindings_to_info()
    {
        texture_context = NULL;
        texture_local   = NULL;
    }
} _ogl_context_to_bindings_to_info;

/** TODO */
typedef struct _ogl_context_to_bindings_texture_unit
{
    _ogl_context_to_bindings_to_info bindings[BINDING_TARGET_COUNT];
    bool                             dirty   [BINDING_TARGET_COUNT];

    _ogl_context_to_bindings_texture_unit()
    {
        memset(dirty, 0, sizeof(dirty) );
    }
} _ogl_context_to_bindings_texture_unit;

/** TODO */
typedef struct _ogl_context_to_bindings
{
    /* Contains exactly GL_MAX_TEXTURE_IMAGE_UNITS items */
    _ogl_context_to_bindings_texture_unit* texture_units;

    /* Used by sync() */
    GLuint* sync_data_textures;

    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;
    uint32_t    gl_max_texture_image_units_value;
    bool        is_arb_multi_bind_supported;

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;
} _ogl_context_to_bindings;


/** TODO */
PRIVATE GLenum _ogl_context_to_bindings_get_glenum_from_ogl_context_to_binding_target(_ogl_context_to_binding_target binding_target)
{
    GLenum result = GL_NONE;

    switch (binding_target)
    {
        case BINDING_TARGET_TEXTURE_1D:                   result = GL_TEXTURE_1D;                   break;
        case BINDING_TARGET_TEXTURE_1D_ARRAY:             result = GL_TEXTURE_1D_ARRAY;             break;
        case BINDING_TARGET_TEXTURE_2D:                   result = GL_TEXTURE_2D;                   break;
        case BINDING_TARGET_TEXTURE_2D_ARRAY:             result = GL_TEXTURE_2D_ARRAY;             break;
        case BINDING_TARGET_TEXTURE_2D_MULTISAMPLE:       result = GL_TEXTURE_2D_MULTISAMPLE;       break;
        case BINDING_TARGET_TEXTURE_2D_MULTISAMPLE_ARRAY: result = GL_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
        case BINDING_TARGET_TEXTURE_3D:                   result = GL_TEXTURE_3D;                   break;
        case BINDING_TARGET_TEXTURE_BUFFER:               result = GL_TEXTURE_BUFFER;               break;
        case BINDING_TARGET_TEXTURE_CUBE_MAP:             result = GL_TEXTURE_CUBE_MAP;             break;
        case BINDING_TARGET_TEXTURE_CUBE_MAP_ARRAY:       result = GL_TEXTURE_CUBE_MAP_ARRAY;       break;
        case BINDING_TARGET_TEXTURE_RECTANGLE:            result = GL_TEXTURE_RECTANGLE;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture binding target");
        }
    } /* switch (gl_target) */

    return result;
}

/** TODO */
PRIVATE _ogl_context_to_binding_target _ogl_context_to_bindings_get_ogl_context_to_binding_target_from_glenum(GLenum gl_target)
{
    _ogl_context_to_binding_target result = BINDING_TARGET_COUNT;

    switch (gl_target)
    {
        case GL_TEXTURE_1D:                   result = BINDING_TARGET_TEXTURE_1D;                   break;
        case GL_TEXTURE_1D_ARRAY:             result = BINDING_TARGET_TEXTURE_1D_ARRAY;             break;
        case GL_TEXTURE_2D:                   result = BINDING_TARGET_TEXTURE_2D;                   break;
        case GL_TEXTURE_2D_ARRAY:             result = BINDING_TARGET_TEXTURE_2D_ARRAY;             break;
        case GL_TEXTURE_2D_MULTISAMPLE:       result = BINDING_TARGET_TEXTURE_2D_MULTISAMPLE;       break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: result = BINDING_TARGET_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
        case GL_TEXTURE_3D:                   result = BINDING_TARGET_TEXTURE_3D;                   break;
        case GL_TEXTURE_BUFFER:               result = BINDING_TARGET_TEXTURE_BUFFER;               break;
        case GL_TEXTURE_CUBE_MAP:             result = BINDING_TARGET_TEXTURE_CUBE_MAP;             break;
        case GL_TEXTURE_CUBE_MAP_ARRAY:       result = BINDING_TARGET_TEXTURE_CUBE_MAP_ARRAY;       break;
        case GL_TEXTURE_RECTANGLE:            result = BINDING_TARGET_TEXTURE_RECTANGLE;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture target");
        }
    } /* switch (gl_target) */

    return result;
}

/** TODO */
PRIVATE void _ogl_context_to_bindings_sync_multi_bind_process(__in __notnull ogl_context_to_bindings          bindings,
                                                              __in           ogl_context_to_bindings_sync_bit sync_bits)
{
    _ogl_context_to_bindings* bindings_ptr = (_ogl_context_to_bindings*) bindings;

    /* Iterate over all binding targets */
    for (_ogl_context_to_binding_target binding_target = (_ogl_context_to_binding_target) 0;
                                        binding_target < BINDING_TARGET_COUNT;
                                ++(int&)binding_target)
    {
        const GLenum binding_target_gl = _ogl_context_to_bindings_get_glenum_from_ogl_context_to_binding_target(binding_target);

        /* Since ordering for _ogl_context_to_binding_target and ogl_context_to_bindings_sync_bit is exactly the same,
         * we're OK with the following check..
         */
        if (!(sync_bits & (1 << binding_target) ))
        {
            /* Skip this iteration */
            continue;
        }

        /* Iterate over all texture units. Find the first binding that differs between client and
         * server sides (if any) */
        uint32_t n_texture_unit_first = 0xFFFFFFFF;
        uint32_t n_texture_unit_count = 0;

        for (unsigned int n_texture_unit = 0;
                          n_texture_unit < bindings_ptr->gl_max_texture_image_units_value;
                        ++n_texture_unit)
        {
            _ogl_context_to_bindings_texture_unit* texture_unit_ptr = bindings_ptr->texture_units + n_texture_unit;

            if (texture_unit_ptr->dirty[binding_target])
            {
                _ogl_context_to_bindings_to_info* binding_ptr = texture_unit_ptr->bindings + binding_target;

                if (binding_ptr->texture_context != binding_ptr->texture_local)
                {
                    if (n_texture_unit_first == 0xFFFFFFFF)
                    {
                        n_texture_unit_first = n_texture_unit;
                    }

                    n_texture_unit_count = n_texture_unit - n_texture_unit_first;

                    ogl_texture_get_property(binding_ptr->texture_local,
                                             OGL_TEXTURE_PROPERTY_ID,
                                             bindings_ptr->sync_data_textures + n_texture_unit_count);

                    binding_ptr->texture_context = binding_ptr->texture_local;
                }/* if (binding does not match on client & server side */

                /* Mark the binding targets of this texture unit as no longer dirty */
                texture_unit_ptr->dirty[binding_target] = false;
            } /* if (texture_unit_ptr->dirty) */
        } /* /* for (texture units) */

        /* We can now use multi-bind functionality if necessary */
        if (n_texture_unit_first != 0xFFFFFFFF)
        {
            bindings_ptr->entrypoints_private_ptr->pGLBindTextures(n_texture_unit_first,
                                                                   n_texture_unit_count + 1,
                                                                   bindings_ptr->sync_data_textures);
        }
    } /* for (all texture targets) */
}

/** TODO */
PRIVATE void _ogl_context_to_bindings_sync_non_multi_bind_process(__in __notnull ogl_context_to_bindings          bindings,
                                                                  __in           ogl_context_to_bindings_sync_bit sync_bits)
{
    _ogl_context_to_bindings* bindings_ptr = (_ogl_context_to_bindings*) bindings;

    /* Iterate over all binding targets */
    for (_ogl_context_to_binding_target binding_target = (_ogl_context_to_binding_target) 0;
                                        binding_target < BINDING_TARGET_COUNT;
                                ++(int&)binding_target)
    {
        const GLenum binding_target_gl = _ogl_context_to_bindings_get_glenum_from_ogl_context_to_binding_target(binding_target);

        /* Since ordering for _ogl_context_to_binding_target and ogl_context_to_bindings_sync_bit is exactly the same,
         * we're OK with the following check..
         */
        if (!(sync_bits & (1 << binding_target) ))
        {
            /* Skip this iteration */
            continue;
        }

        for (unsigned int n_texture_unit = 0;
                          n_texture_unit < bindings_ptr->gl_max_texture_image_units_value;
                        ++n_texture_unit)
        {
            _ogl_context_to_bindings_texture_unit* texture_unit_ptr = bindings_ptr->texture_units + n_texture_unit;

            if (texture_unit_ptr->dirty[binding_target])
            {
                _ogl_context_to_bindings_to_info* binding_ptr = texture_unit_ptr->bindings + binding_target;

                if (binding_ptr->texture_context != binding_ptr->texture_local)
                {
                    GLuint texture_id = 0;

                    ogl_texture_get_property(binding_ptr->texture_local,
                                             OGL_TEXTURE_PROPERTY_ID,
                                            &texture_id);

                    /** TODO: This will crash without DSA support */
                    bindings_ptr->entrypoints_private_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0 + n_texture_unit,
                                                                                  binding_target_gl,
                                                                                  texture_id);

                    /* Update internal representation */
                    binding_ptr->texture_context = binding_ptr->texture_local;
                }/* if (binding does not match on client & server side */

                /* Mark the binding targets of this texture unit as no longer dirty */
                texture_unit_ptr->dirty[binding_target] = false;
            } /* if (texture_unit_ptr->dirty) */
        } /* /* for (texture units) */
    } /* for (all texture targets) */
}

/** Please see header for spec */
PUBLIC ogl_context_to_bindings ogl_context_to_bindings_create(__in __notnull ogl_context context)
{
    _ogl_context_to_bindings* new_bindings = new (std::nothrow) _ogl_context_to_bindings;

    ASSERT_ALWAYS_SYNC(new_bindings != NULL, "Out of memory");
    if (new_bindings != NULL)
    {
        new_bindings->context = context;
    } /* if (new_bindings != NULL) */

    return (ogl_context_to_bindings) new_bindings;
}

/** Please see header for spec */
PUBLIC ogl_texture ogl_context_to_bindings_get_bound_texture(__in __notnull const ogl_context_to_bindings to_bindings,
                                                             __in           GLuint                        texture_unit,
                                                             __in           GLenum                        target)
{
    ogl_texture                     result          = NULL;
    const _ogl_context_to_bindings* to_bindings_ptr = (const _ogl_context_to_bindings*) to_bindings;

    ASSERT_DEBUG_SYNC(texture_unit < to_bindings_ptr->gl_max_texture_image_units_value,
                      "Invalid texture unit requested");
    if (texture_unit < to_bindings_ptr->gl_max_texture_image_units_value)
    {
        _ogl_context_to_binding_target internal_target = _ogl_context_to_bindings_get_ogl_context_to_binding_target_from_glenum(target);

        ASSERT_DEBUG_SYNC(internal_target < BINDING_TARGET_COUNT,
                          "Unrecognized texture target");
        if (internal_target < BINDING_TARGET_COUNT)
        {
            result = to_bindings_ptr->texture_units[texture_unit].bindings[internal_target].texture_local;
        }
    }

    return result;
}

/** Please see header for spec */
PUBLIC ogl_context_to_bindings_sync_bit ogl_context_to_bindings_get_ogl_context_to_bindings_sync_bit_from_gl_target(__in GLenum binding_target)
{
    ogl_context_to_bindings_sync_bit result = (ogl_context_to_bindings_sync_bit) 0;

    switch (binding_target)
    {
        case GL_TEXTURE_1D:                   result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_1D;                   break;
        case GL_TEXTURE_1D_ARRAY:             result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_1D_ARRAY;             break;
        case GL_TEXTURE_2D:                   result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D;                   break;
        case GL_TEXTURE_2D_ARRAY:             result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D_ARRAY;             break;
        case GL_TEXTURE_2D_MULTISAMPLE:       result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D_MULTISAMPLE;       break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
        case GL_TEXTURE_3D:                   result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_3D;                   break;
        case GL_TEXTURE_BUFFER:               result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER;               break;

        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        {
            result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_CUBE_MAP;

            break;
        }

        case GL_TEXTURE_CUBE_MAP_ARRAY:
        {
            /* NOTE: This will probably blow up, because the CM detailed texture targets
             *       are currently associated with CM texture target, not the CMA one.
             *       Investigate if needed.
             */
            ASSERT_DEBUG_SYNC(false,
                              "Doh");

            result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_CUBE_MAP_ARRAY;
            break;
        }

        case GL_TEXTURE_RECTANGLE:
        {
            result = OGL_CONTEXT_TO_BINDINGS_SYNC_BIT_TEXTURE_RECTANGLE;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture binding target");
        }
    } /* switch (binding_target) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_to_bindings_init(__in __notnull ogl_context_to_bindings                   bindings,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_to_bindings*    bindings_ptr = (_ogl_context_to_bindings*) bindings;
    const ogl_context_gl_limits* limits_ptr   = NULL;

    ogl_context_get_property(bindings_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* Cache info in private descriptor */
    bindings_ptr->entrypoints_private_ptr          = entrypoints_private_ptr;
    bindings_ptr->gl_max_texture_image_units_value = limits_ptr->max_texture_image_units;

    /* Determine if GL_ARB_multi_bind is supported */
    bindings_ptr->is_arb_multi_bind_supported = ogl_context_is_extension_supported(bindings_ptr->context,
                                                                                   system_hashed_ansi_string_create("GL_ARB_multi_bind") );

    /* Initialize texture unit descriptors */
    ASSERT_ALWAYS_SYNC(limits_ptr->max_texture_image_units != 0,
                       "GL_MAX_TEXTURE_IMAGE_UNITS is 0, crash imminent.");

    bindings_ptr->texture_units = new (std::nothrow) _ogl_context_to_bindings_texture_unit[limits_ptr->max_texture_image_units];

    ASSERT_ALWAYS_SYNC(bindings_ptr->texture_units != NULL,
                       "Out of memory");

    /* Allocate arrays used by sync() */
    bindings_ptr->sync_data_textures = new (std::nothrow) GLuint[limits_ptr->max_texture_image_units];

    ASSERT_ALWAYS_SYNC(bindings_ptr->sync_data_textures != NULL,
                       "Out of memory");
}

/** Please see header for spec */
PUBLIC void ogl_context_to_bindings_release(__in __notnull __post_invalid ogl_context_to_bindings bindings)
{
    _ogl_context_to_bindings* bindings_ptr = (_ogl_context_to_bindings*) bindings;

    /* Release binding storage */
    if (bindings_ptr->texture_units != NULL)
    {
        delete [] bindings_ptr->texture_units;

        bindings_ptr->texture_units = NULL;
    }

    /* Release helper buffers */
    if (bindings_ptr->sync_data_textures != NULL)
    {
        delete [] bindings_ptr->sync_data_textures;

        bindings_ptr->sync_data_textures = NULL;
    }

    /* Done */
    delete bindings_ptr;

    bindings_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_context_to_bindings_reset_all_bindings_for_texture_unit(__in __notnull ogl_context_to_bindings to_bindings,
                                                                        __in           uint32_t                texture_unit)
{
    _ogl_context_to_bindings* to_bindings_ptr = (_ogl_context_to_bindings*) to_bindings;

    ASSERT_DEBUG_SYNC(texture_unit < to_bindings_ptr->gl_max_texture_image_units_value,
                      "Invalid texture unit index");

    if (texture_unit < to_bindings_ptr->gl_max_texture_image_units_value)
    {
        for (unsigned int n_target = 0;
                          n_target < BINDING_TARGET_COUNT;
                        ++n_target)
        {
            to_bindings_ptr->texture_units[texture_unit].bindings[n_target].texture_context = NULL;
            to_bindings_ptr->texture_units[texture_unit].bindings[n_target].texture_local   = NULL;
            to_bindings_ptr->texture_units[texture_unit].dirty[n_target]                    = false;
        } /* for (all texture targets) */
    }
}

/** Please see header for spec */
PUBLIC void ogl_context_to_bindings_set_binding(__in __notnull ogl_context_to_bindings bindings,
                                                __in           GLuint                  texture_unit,
                                                __in           GLenum                  target,
                                                __in_opt       ogl_texture             texture)
{
    _ogl_context_to_bindings* bindings_ptr = (_ogl_context_to_bindings*) bindings;

    ASSERT_DEBUG_SYNC(texture_unit < bindings_ptr->gl_max_texture_image_units_value,
                      "Invalid texture unit requested");
    if (texture_unit < bindings_ptr->gl_max_texture_image_units_value)
    {
        _ogl_context_to_binding_target    local_target = _ogl_context_to_bindings_get_ogl_context_to_binding_target_from_glenum(target);
        _ogl_context_to_bindings_to_info* to_ptr       = bindings_ptr->texture_units[texture_unit].bindings + local_target;

        if (to_ptr->texture_local != texture)
        {
            bindings_ptr->texture_units[texture_unit].dirty[local_target] = true;
            to_ptr->texture_local                                         = texture;

            /* NOTE: GL_ARB_multi_bind cannot be used against texture objects, to which data storage has
             *       not been configured. Verify the object has been bound to a texture target - if not,
             *       bind it *now*
             */
            if (texture != NULL)
            {
                bool texture_bound = false;

                ogl_texture_get_property(texture,
                                         OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND,
                                        &texture_bound);

                if (!texture_bound)
                {
                    GLuint texture_id = 0;

                    ogl_texture_get_property(texture,
                                             OGL_TEXTURE_PROPERTY_ID,
                                            &texture_id);

                    bindings_ptr->entrypoints_private_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0 + texture_unit,
                                                                                  target,
                                                                                  texture_id);

                    to_ptr->texture_context = texture;
                    texture_bound           = true;

                    ogl_texture_set_property(texture,
                                             OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND,
                                            &texture_bound);
                }
            } /* if (texture != NULL) */
        }
    } /* if (texture_unit < bindings_ptr->gl_max_texture_image_units_value) */
}

/** Please see header for spec */
PUBLIC void ogl_context_to_bindings_sync(__in __notnull ogl_context_to_bindings          bindings,
                                         __in           ogl_context_to_bindings_sync_bit sync_bits)
{
    /* NOTE: bindings is NULL during rendering context initialization */
    if (bindings != NULL)
    {
        _ogl_context_to_bindings* bindings_ptr = (_ogl_context_to_bindings*) bindings;

        if (bindings_ptr->is_arb_multi_bind_supported)
        {
            _ogl_context_to_bindings_sync_multi_bind_process(bindings, sync_bits);
        }
        else
        {
            _ogl_context_to_bindings_sync_non_multi_bind_process(bindings, sync_bits);
        }
    } /* if (bindings != NULL) */
}