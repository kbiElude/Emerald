/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_bo_bindings.h"
#include "system/system_hash64map.h"

/** TODO.
 *
 *  NOTE: For house-keeping, make sure general + indiced binding targets are defined
  *       before BINDING_TARGET_COUNT_INDICED.
  **/
typedef enum
{
    BINDING_TARGET_ATOMIC_COUNTER_BUFFER,       /* general + indiced */
    BINDING_TARGET_SHADER_STORAGE_BUFFER,       /* general + indiced */
    BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER,   /* general + indiced */
    BINDING_TARGET_UNIFORM_BUFFER,              /* general + indiced */

    BINDING_TARGET_COUNT_INDICED,

    BINDING_TARGET_ARRAY_BUFFER = BINDING_TARGET_COUNT_INDICED, /* general */
    BINDING_TARGET_COPY_READ_BUFFER,                            /* general */
    BINDING_TARGET_COPY_WRITE_BUFFER,                           /* general */
    BINDING_TARGET_DISPATCH_INDIRECT_BUFFER,                    /* general */
    BINDING_TARGET_DRAW_INDIRECT_BUFFER,                        /* general */
    BINDING_TARGET_PIXEL_PACK_BUFFER,                           /* general */
    BINDING_TARGET_PIXEL_UNPACK_BUFFER,                         /* general */
    BINDING_TARGET_QUERY_BUFFER,                                /* general */
    BINDING_TARGET_TEXTURE_BUFFER,                              /* general */

    BINDING_TARGET_COUNT,

    /* Special binding that should be handled by ogl_context_vaos */
    BINDING_TARGET_ELEMENT_ARRAY_BUFFER,

    BINDING_TARGET_UNKNOWN = BINDING_TARGET_COUNT
} _ogl_context_bo_binding_target;

/** TODO */
typedef struct _ogl_context_bo_bindings_bo_info
{
    GLsizeiptr size;

    _ogl_context_bo_bindings_bo_info()
    {
        size = 0;
    }
} _ogl_context_bo_bindings_bo_info;

/** TODO */
typedef struct _ogl_context_bo_bindings_general_binding
{
    GLuint bo_id;
    GLenum gl_bo_binding_point;

    _ogl_context_bo_bindings_general_binding()
    {
        bo_id = 0;
    }
} _ogl_context_bo_bindings_general_binding;

/** TODO */
typedef struct _ogl_context_bo_bindings_indiced_binding
{
    GLuint     bo_id;
    bool       is_range_binding;
    GLintptr   offset;
    GLsizeiptr size;

    bool   dirty; /* only use for the "local" case */
    GLenum gl_bo_binding_point;

    _ogl_context_bo_bindings_indiced_binding()
    {
        bo_id            = 0;
        dirty            = false;
        is_range_binding = false;
        offset           = 0;
        size             = 0;
    }
} _ogl_context_bo_bindings_indiced_binding;

/** TODO */
typedef struct _ogl_context_bo_bindings
{
    _ogl_context_bo_bindings_general_binding  bindings_general_context[BINDING_TARGET_COUNT];
    _ogl_context_bo_bindings_general_binding  bindings_general_local  [BINDING_TARGET_COUNT];
    _ogl_context_bo_bindings_indiced_binding* bindings_indiced_context[BINDING_TARGET_COUNT_INDICED];
    _ogl_context_bo_bindings_indiced_binding* bindings_indiced_local  [BINDING_TARGET_COUNT_INDICED];

    /* Used by sync() */
    GLuint*     sync_indiced_bindings_data_buffers;
    GLintptr*   sync_indiced_bindings_data_offsets;
    GLsizeiptr* sync_indiced_bindings_data_sizes;

    /* For GL_ARB_multi_bind path, we need to cache buffer object storage size information, in order to
     * convert all glBindBufferBase() calls into ranged ones.
     */
    system_hash64map bo_id_to_bo_info_map;

    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;
    bool        is_arb_multi_bind_supported;

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;
    const ogl_context_gl_limits*              limits_ptr; /* used for sanity checks */
} _ogl_context_bo_bindings;


/** TODO */
PRIVATE GLenum _ogl_context_bo_bindings_get_glenum_from_ogl_context_bo_binding_target(__in GLenum binding_target)
{
    GLenum result = GL_NONE;

    switch (binding_target)
    {
        case BINDING_TARGET_ARRAY_BUFFER:              result = GL_ARRAY_BUFFER;              break;
        case BINDING_TARGET_ATOMIC_COUNTER_BUFFER:     result = GL_ATOMIC_COUNTER_BUFFER;     break;
        case BINDING_TARGET_COPY_READ_BUFFER:          result = GL_COPY_READ_BUFFER;          break;
        case BINDING_TARGET_COPY_WRITE_BUFFER:         result = GL_COPY_WRITE_BUFFER;         break;
        case BINDING_TARGET_DISPATCH_INDIRECT_BUFFER:  result = GL_DISPATCH_INDIRECT_BUFFER;  break;
        case BINDING_TARGET_DRAW_INDIRECT_BUFFER:      result = GL_DRAW_INDIRECT_BUFFER;      break;
        case BINDING_TARGET_ELEMENT_ARRAY_BUFFER:      result = GL_ELEMENT_ARRAY_BUFFER;      break;
        case BINDING_TARGET_PIXEL_PACK_BUFFER:         result = GL_PIXEL_PACK_BUFFER;         break;
        case BINDING_TARGET_PIXEL_UNPACK_BUFFER:       result = GL_PIXEL_UNPACK_BUFFER;       break;
        case BINDING_TARGET_QUERY_BUFFER:              result = GL_QUERY_BUFFER;              break;
        case BINDING_TARGET_SHADER_STORAGE_BUFFER:     result = GL_SHADER_STORAGE_BUFFER;     break;
        case BINDING_TARGET_TEXTURE_BUFFER:            result = GL_TEXTURE_BUFFER;            break;
        case BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER: result = GL_TRANSFORM_FEEDBACK_BUFFER; break;
        case BINDING_TARGET_UNIFORM_BUFFER:            result = GL_UNIFORM_BUFFER;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized buffer object binding target");
        }
    } /* switch (binding_target) */

    return result;
}

/** TODO */
PRIVATE _ogl_context_bo_binding_target _ogl_context_bo_bindings_get_ogl_context_bo_binding_target_from_glenum(__in GLenum binding_target)
{
    _ogl_context_bo_binding_target result = BINDING_TARGET_UNKNOWN;

    switch (binding_target)
    {
        case GL_ARRAY_BUFFER:              result = BINDING_TARGET_ARRAY_BUFFER;              break;
        case GL_ATOMIC_COUNTER_BUFFER:     result = BINDING_TARGET_ATOMIC_COUNTER_BUFFER;     break;
        case GL_COPY_READ_BUFFER:          result = BINDING_TARGET_COPY_READ_BUFFER;          break;
        case GL_COPY_WRITE_BUFFER:         result = BINDING_TARGET_COPY_WRITE_BUFFER;         break;
        case GL_DISPATCH_INDIRECT_BUFFER:  result = BINDING_TARGET_DISPATCH_INDIRECT_BUFFER;  break;
        case GL_DRAW_INDIRECT_BUFFER:      result = BINDING_TARGET_DRAW_INDIRECT_BUFFER;      break;
        case GL_ELEMENT_ARRAY_BUFFER:      result = BINDING_TARGET_ELEMENT_ARRAY_BUFFER;      break;
        case GL_PIXEL_PACK_BUFFER:         result = BINDING_TARGET_PIXEL_PACK_BUFFER;         break;
        case GL_PIXEL_UNPACK_BUFFER:       result = BINDING_TARGET_PIXEL_UNPACK_BUFFER;       break;
        case GL_QUERY_BUFFER:              result = BINDING_TARGET_QUERY_BUFFER;              break;
        case GL_SHADER_STORAGE_BUFFER:     result = BINDING_TARGET_SHADER_STORAGE_BUFFER;     break;
        case GL_TEXTURE_BUFFER:            result = BINDING_TARGET_TEXTURE_BUFFER;            break;
        case GL_TRANSFORM_FEEDBACK_BUFFER: result = BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER; break;
        case GL_UNIFORM_BUFFER:            result = BINDING_TARGET_UNIFORM_BUFFER;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized buffer object binding target");
        }
    } /* switch (binding_target) */

    return result;
}

/** TODO */
PRIVATE void _ogl_context_bo_bindings_sync_multi_bind_process_indiced_sync_bit(__in __notnull ogl_context_bo_bindings        bindings,
                                                                               __in           GLint                          n_gl_max_bindings,
                                                                               __in           _ogl_context_bo_binding_target internal_target,
                                                                               __in           GLenum                         gl_target)
{
    uint32_t dirty_start_index = 0xFFFFFFFF;
    uint32_t dirty_end_index   = 0;

    /* Determine how many bindings we need to update */
    _ogl_context_bo_bindings* bindings_ptr = (_ogl_context_bo_bindings*) bindings;

    ASSERT_DEBUG_SYNC(internal_target < BINDING_TARGET_COUNT_INDICED,
                      "Invalid internal target requested");

    for (int n_binding = 0;
             n_binding < n_gl_max_bindings;
           ++n_binding)
    {
        if (bindings_ptr->bindings_indiced_local[internal_target][n_binding].dirty)
        {
            if (dirty_start_index == 0xFFFFFFFF)
            {
                dirty_start_index = n_binding;
            }

            dirty_end_index = n_binding;
        }
    } /* for (all bindings) */

    /* Prepare the arguments */
    GLuint*     sync_data_buffers = bindings_ptr->sync_indiced_bindings_data_buffers;
    GLintptr*   sync_data_offsets = bindings_ptr->sync_indiced_bindings_data_offsets;
    GLsizeiptr* sync_data_sizes   = bindings_ptr->sync_indiced_bindings_data_sizes;

    const int n_bindings_to_update = dirty_end_index - dirty_start_index + 1;

    /* Configure the input arrays */
    if (dirty_start_index != 0xFFFFFFFF)
    {
        for (int n_binding = 0;
                 n_binding < n_bindings_to_update;
               ++n_binding)
        {
            const _ogl_context_bo_bindings_indiced_binding* local_binding_ptr = bindings_ptr->bindings_indiced_local[internal_target] + (dirty_start_index + n_binding);

            sync_data_buffers[n_binding] = local_binding_ptr->bo_id;

            if (local_binding_ptr->is_range_binding)
            {
                sync_data_offsets[n_binding] = local_binding_ptr->offset;
                sync_data_sizes  [n_binding] = local_binding_ptr->size;
            } /* if (binding_ptr->is_range_binding) */
            else
            {
                /* Retrieve BO size */
                const _ogl_context_bo_bindings_bo_info* bo_ptr = NULL;

                if (local_binding_ptr->bo_id != 0                            &&
                    system_hash64map_get(bindings_ptr->bo_id_to_bo_info_map,
                                         local_binding_ptr->bo_id,
                                        &bo_ptr) )
                {
                    /* Set the arguments */
                    sync_data_offsets[n_binding] = 0;
                    sync_data_sizes  [n_binding] = bo_ptr->size;
                }
                else
                {
                    /* This code-path is valid as long as we're unbinding a BO */
                    ASSERT_DEBUG_SYNC(local_binding_ptr->bo_id == 0,
                                      "Could not retrieve BO descriptor");

                    sync_data_offsets[n_binding] = 0;
                    sync_data_sizes  [n_binding] = 0;
                }
            }
        } /* for (all indiced bindings that we need to update) */

        /* Issue the GL call */
        bindings_ptr->entrypoints_private_ptr->pGLBindBuffersRange(gl_target,
                                                                   dirty_start_index,
                                                                   n_bindings_to_update,
                                                                   sync_data_buffers,
                                                                   sync_data_offsets,
                                                                   sync_data_sizes);

        /* Mark all bindings as updated */
        for (int n_binding = 0;
                 n_binding < n_bindings_to_update;
               ++n_binding)
        {
            _ogl_context_bo_bindings_indiced_binding* context_binding_ptr = bindings_ptr->bindings_indiced_context[internal_target] + (dirty_start_index + n_binding);
            _ogl_context_bo_bindings_indiced_binding* local_binding_ptr   = bindings_ptr->bindings_indiced_local  [internal_target] + (dirty_start_index + n_binding);

            local_binding_ptr->dirty = false;
            *context_binding_ptr     = *local_binding_ptr;
        }
    } /* if (dirty_start_index != 0xFFFFFFFF) */
}

/** TODO */
PRIVATE void _ogl_context_bo_bindings_sync_non_multi_bind_process_indiced_sync_bit(__in __notnull ogl_context_bo_bindings        bindings,
                                                                                   __in           GLint                          n_gl_max_bindings,
                                                                                   __in           _ogl_context_bo_binding_target internal_target)
{
    _ogl_context_bo_bindings* bindings_ptr = (_ogl_context_bo_bindings*) bindings;

    for (int n_binding = 0;
             n_binding < n_gl_max_bindings;
           ++n_binding)
    {
        _ogl_context_bo_bindings_indiced_binding* context_binding_ptr = bindings_ptr->bindings_indiced_context[internal_target] + n_binding;
        _ogl_context_bo_bindings_indiced_binding* local_binding_ptr   = bindings_ptr->bindings_indiced_local  [internal_target] + n_binding;

        if (local_binding_ptr->dirty)
        {
            if (local_binding_ptr->is_range_binding)
            {
                bindings_ptr->entrypoints_private_ptr->pGLBindBufferRange(local_binding_ptr->gl_bo_binding_point,
                                                                          n_binding,
                                                                          local_binding_ptr->bo_id,
                                                                          local_binding_ptr->offset,
                                                                          local_binding_ptr->size);
            }
            else
            {
                bindings_ptr->entrypoints_private_ptr->pGLBindBufferBase(local_binding_ptr->gl_bo_binding_point,
                                                                         n_binding,
                                                                         local_binding_ptr->bo_id);
            }

            /* Update internal representation */
            local_binding_ptr->dirty = false;
            *context_binding_ptr     = *local_binding_ptr;
        } /* if (local_binding_ptr->dirty) */
    } /* for (all bindings) */
}

/** TODO */
PRIVATE void _ogl_context_bo_bindings_sync_process_general_sync_bit(__in __notnull ogl_context_bo_bindings        bindings,
                                                                    __in           _ogl_context_bo_binding_target internal_target)
{
    _ogl_context_bo_bindings* bindings_ptr = (_ogl_context_bo_bindings*) bindings;

    _ogl_context_bo_bindings_general_binding* context_binding_ptr = bindings_ptr->bindings_general_context + internal_target;
    _ogl_context_bo_bindings_general_binding* local_binding_ptr   = bindings_ptr->bindings_general_local   + internal_target;

    if (local_binding_ptr->bo_id != context_binding_ptr->bo_id)
    {
        bindings_ptr->entrypoints_private_ptr->pGLBindBuffer(local_binding_ptr->gl_bo_binding_point,
                                                             local_binding_ptr->bo_id);

        /* Update internal representation */
        *context_binding_ptr = *local_binding_ptr;
    } /* if (local_binding_ptr->bo_id != context_binding_ptr->bo_id) */
}

/** Please see header for spec */
PUBLIC ogl_context_bo_bindings ogl_context_bo_bindings_create(__in __notnull ogl_context context)
{
    _ogl_context_bo_bindings* new_bindings = new (std::nothrow) _ogl_context_bo_bindings;

    ASSERT_ALWAYS_SYNC(new_bindings != NULL,
                       "Out of memory");

    if (new_bindings != NULL)
    {
        new_bindings->context              = context;
        new_bindings->bo_id_to_bo_info_map = system_hash64map_create(sizeof(_ogl_context_bo_bindings_bo_info*));

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &new_bindings->limits_ptr);
    } /* if (new_bindings != NULL) */

    return (ogl_context_bo_bindings) new_bindings;
}

/** Please see header for spec */
PUBLIC GLuint ogl_context_bo_bindings_get_general_binding(__in __notnull const ogl_context_bo_bindings bo_bindings,
                                                          __in           GLenum                        target)
{
    const _ogl_context_bo_bindings* bindings_ptr    = (const _ogl_context_bo_bindings*) bo_bindings;
    _ogl_context_bo_binding_target  internal_target = _ogl_context_bo_bindings_get_ogl_context_bo_binding_target_from_glenum(target);
    GLuint                          result          = 0;

    ASSERT_DEBUG_SYNC(internal_target < BINDING_TARGET_COUNT,
                      "Invalid binding target requested");

    if (internal_target < BINDING_TARGET_COUNT)
    {
        result = bindings_ptr->bindings_general_local[internal_target].bo_id;
    }

    return result;
}

/** TODO */
PUBLIC ogl_context_bo_bindings_sync_bit ogl_context_bo_bindings_get_ogl_context_bo_bindings_sync_bit_for_gl_target(__in GLenum binding_target)
{
    ogl_context_bo_bindings_sync_bit result;

    switch (binding_target)
    {
        case GL_ARRAY_BUFFER:              result = BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER;              break;
        case GL_ATOMIC_COUNTER_BUFFER:     result = BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER;     break;
        case GL_COPY_READ_BUFFER:          result = BO_BINDINGS_SYNC_BIT_COPY_READ_BUFFER;          break;
        case GL_COPY_WRITE_BUFFER:         result = BO_BINDINGS_SYNC_BIT_COPY_WRITE_BUFFER;         break;
        case GL_DISPATCH_INDIRECT_BUFFER:  result = BO_BINDINGS_SYNC_BIT_DISPATCH_INDIRECT_BUFFER;  break;
        case GL_DRAW_INDIRECT_BUFFER:      result = BO_BINDINGS_SYNC_BIT_DRAW_INDIRECT_BUFFER;      break;
        case GL_PIXEL_PACK_BUFFER:         result = BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER;         break;
        case GL_PIXEL_UNPACK_BUFFER:       result = BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER;       break;
        case GL_QUERY_BUFFER:              result = BO_BINDINGS_SYNC_BIT_QUERY_BUFFER;              break;
        case GL_SHADER_STORAGE_BUFFER:     result = BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER;     break;
        case GL_TEXTURE_BUFFER:            result = BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER;            break;
        case GL_TRANSFORM_FEEDBACK_BUFFER: result = BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER; break;
        case GL_UNIFORM_BUFFER:            result = BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER;            break;

        case GL_ELEMENT_ARRAY_BUFFER:
        {
            /* No syncing should ever be done for ELEMENT_ARRAY_BUFFER */
            result = (ogl_context_bo_bindings_sync_bit) 0;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized buffer object binding target");
        }
    } /* switch (binding_target) */

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_init(__in __notnull ogl_context_bo_bindings                   bindings,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_bo_bindings* bindings_ptr = (_ogl_context_bo_bindings*) bindings;

    /* Cache private entrypoints descriptor */
    bindings_ptr->entrypoints_private_ptr = entrypoints_private_ptr;

    /* Determine if GL_ARB_multi_bind is supported */
    bindings_ptr->is_arb_multi_bind_supported = ogl_context_is_extension_supported(bindings_ptr->context,
                                                                                   system_hashed_ansi_string_create("GL_ARB_multi_bind") );

    /* Initialize general binding descriptors */
    for (unsigned int n_binding_target = 0;
                      n_binding_target < BINDING_TARGET_COUNT;
                    ++n_binding_target)
    {
        bindings_ptr->bindings_general_context[n_binding_target].bo_id = 0;
        bindings_ptr->bindings_general_context[n_binding_target].gl_bo_binding_point = _ogl_context_bo_bindings_get_glenum_from_ogl_context_bo_binding_target(n_binding_target);

        bindings_ptr->bindings_general_local[n_binding_target].bo_id = 0;
        bindings_ptr->bindings_general_local[n_binding_target].gl_bo_binding_point = _ogl_context_bo_bindings_get_glenum_from_ogl_context_bo_binding_target(n_binding_target);
    }

    /* Initialize indiced binding descriptors */
    ASSERT_ALWAYS_SYNC(bindings_ptr->limits_ptr->max_atomic_counter_buffer_bindings != 0,
                       "GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS is 0, crash imminent.");
    ASSERT_ALWAYS_SYNC(bindings_ptr->limits_ptr->max_shader_storage_buffer_bindings != 0,
                       "GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS is 0, crash imminent.");
    ASSERT_ALWAYS_SYNC(bindings_ptr->limits_ptr->max_transform_feedback_buffers != 0,
                       "GL_MAX_TRANSFORM_FEEDBACK_BUFFERS is 0, crash imminent.");
    ASSERT_ALWAYS_SYNC(bindings_ptr->limits_ptr->max_uniform_buffer_bindings != 0,
                       "GL_MAX_UNIFORM_BUFFERS is 0, crash imminent.");

    bindings_ptr->bindings_indiced_context[BINDING_TARGET_ATOMIC_COUNTER_BUFFER]     = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_atomic_counter_buffer_bindings];
    bindings_ptr->bindings_indiced_local  [BINDING_TARGET_ATOMIC_COUNTER_BUFFER]     = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_atomic_counter_buffer_bindings];
    bindings_ptr->bindings_indiced_context[BINDING_TARGET_SHADER_STORAGE_BUFFER]     = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_shader_storage_buffer_bindings];
    bindings_ptr->bindings_indiced_local  [BINDING_TARGET_SHADER_STORAGE_BUFFER]     = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_shader_storage_buffer_bindings];
    bindings_ptr->bindings_indiced_context[BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER] = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_transform_feedback_buffers];
    bindings_ptr->bindings_indiced_local  [BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER] = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_transform_feedback_buffers];
    bindings_ptr->bindings_indiced_context[BINDING_TARGET_UNIFORM_BUFFER]            = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_uniform_buffer_bindings];
    bindings_ptr->bindings_indiced_local  [BINDING_TARGET_UNIFORM_BUFFER]            = new (std::nothrow) _ogl_context_bo_bindings_indiced_binding[bindings_ptr->limits_ptr->max_uniform_buffer_bindings];

    /* Make sure all allocations succeeded */
    for (unsigned int n_binding_target = 0;
                      n_binding_target < BINDING_TARGET_COUNT_INDICED;
                    ++n_binding_target)
    {
        ASSERT_ALWAYS_SYNC(bindings_ptr->bindings_indiced_context[n_binding_target] != NULL,
                           "Out of memory");

        ASSERT_ALWAYS_SYNC(bindings_ptr->bindings_indiced_local[n_binding_target] != NULL,
                           "Out of memory");
    }

    /* Set up GL targets */
    for (int n_binding = 0;
             n_binding < bindings_ptr->limits_ptr->max_atomic_counter_buffer_bindings;
           ++n_binding)
    {
        bindings_ptr->bindings_indiced_context[BINDING_TARGET_ATOMIC_COUNTER_BUFFER][n_binding].gl_bo_binding_point = GL_ATOMIC_COUNTER_BUFFER;
        bindings_ptr->bindings_indiced_local  [BINDING_TARGET_ATOMIC_COUNTER_BUFFER][n_binding].gl_bo_binding_point = GL_ATOMIC_COUNTER_BUFFER;
    }

    for (int n_binding = 0;
             n_binding < bindings_ptr->limits_ptr->max_shader_storage_buffer_bindings;
           ++n_binding)
    {
        bindings_ptr->bindings_indiced_context[BINDING_TARGET_SHADER_STORAGE_BUFFER][n_binding].gl_bo_binding_point = GL_SHADER_STORAGE_BUFFER;
        bindings_ptr->bindings_indiced_local  [BINDING_TARGET_SHADER_STORAGE_BUFFER][n_binding].gl_bo_binding_point = GL_SHADER_STORAGE_BUFFER;
    }

    for (int n_binding = 0;
             n_binding < bindings_ptr->limits_ptr->max_transform_feedback_buffers;
           ++n_binding)
    {
        bindings_ptr->bindings_indiced_context[BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER][n_binding].gl_bo_binding_point = GL_TRANSFORM_FEEDBACK_BUFFER;
        bindings_ptr->bindings_indiced_local  [BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER][n_binding].gl_bo_binding_point = GL_TRANSFORM_FEEDBACK_BUFFER;
    }

    for (int n_binding = 0;
             n_binding < bindings_ptr->limits_ptr->max_uniform_buffer_bindings;
           ++n_binding)
    {
        bindings_ptr->bindings_indiced_context[BINDING_TARGET_UNIFORM_BUFFER][n_binding].gl_bo_binding_point = GL_UNIFORM_BUFFER;
        bindings_ptr->bindings_indiced_local  [BINDING_TARGET_UNIFORM_BUFFER][n_binding].gl_bo_binding_point = GL_UNIFORM_BUFFER;
    }

    /* Allocate arrays used by sync() */
    unsigned int n_indiced_bindings_max = bindings_ptr->limits_ptr->max_atomic_counter_buffer_bindings;

    if (n_indiced_bindings_max < (GLuint) bindings_ptr->limits_ptr->max_shader_storage_buffer_bindings)
    {
        n_indiced_bindings_max = (GLuint) bindings_ptr->limits_ptr->max_shader_storage_buffer_bindings;
    }

    if (n_indiced_bindings_max < (GLuint) bindings_ptr->limits_ptr->max_transform_feedback_buffers)
    {
        n_indiced_bindings_max = (GLuint) bindings_ptr->limits_ptr->max_transform_feedback_buffers;
    }

    if (n_indiced_bindings_max < (GLuint) bindings_ptr->limits_ptr->max_uniform_buffer_bindings)
    {
        n_indiced_bindings_max = (GLuint) bindings_ptr->limits_ptr->max_uniform_buffer_bindings;
    }

    bindings_ptr->sync_indiced_bindings_data_buffers = new (std::nothrow) GLuint    [n_indiced_bindings_max];
    bindings_ptr->sync_indiced_bindings_data_offsets = new (std::nothrow) GLintptr  [n_indiced_bindings_max];
    bindings_ptr->sync_indiced_bindings_data_sizes   = new (std::nothrow) GLsizeiptr[n_indiced_bindings_max];

    ASSERT_ALWAYS_SYNC(bindings_ptr->sync_indiced_bindings_data_buffers != NULL &&
                       bindings_ptr->sync_indiced_bindings_data_offsets != NULL &&
                       bindings_ptr->sync_indiced_bindings_data_sizes   != NULL,
                       "Out of memory");
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_release(__in __notnull __post_invalid ogl_context_bo_bindings bindings)
{
    _ogl_context_bo_bindings* bindings_ptr = (_ogl_context_bo_bindings*) bindings;

    /* Release indiced bindings */
    for (unsigned int n_binding = 0;
                      n_binding < BINDING_TARGET_COUNT_INDICED;
                    ++n_binding)
    {
        if (bindings_ptr->bindings_indiced_context[n_binding] != NULL)
        {
            delete bindings_ptr->bindings_indiced_context[n_binding];

            bindings_ptr->bindings_indiced_context[n_binding] = NULL;
        }

        if (bindings_ptr->bindings_indiced_local[n_binding] != NULL)
        {
            delete bindings_ptr->bindings_indiced_local[n_binding];

            bindings_ptr->bindings_indiced_local[n_binding] = NULL;
        }
    }

    /* Release helper buffers */
    if (bindings_ptr->sync_indiced_bindings_data_buffers != NULL)
    {
        delete [] bindings_ptr->sync_indiced_bindings_data_buffers;

        bindings_ptr->sync_indiced_bindings_data_buffers = NULL;
    }

    if (bindings_ptr->sync_indiced_bindings_data_offsets != NULL)
    {
        delete [] bindings_ptr->sync_indiced_bindings_data_offsets;

        bindings_ptr->sync_indiced_bindings_data_offsets = NULL;
    }

    if (bindings_ptr->sync_indiced_bindings_data_sizes != NULL)
    {
        delete [] bindings_ptr->sync_indiced_bindings_data_sizes;

        bindings_ptr->sync_indiced_bindings_data_sizes = NULL;
    }

    /* Release helper objects */
    if (bindings_ptr->bo_id_to_bo_info_map != NULL)
    {
        system_hash64                     bo_hash = 0;
        _ogl_context_bo_bindings_bo_info* bo_ptr  = NULL;

        while (system_hash64map_get_element_at(bindings_ptr->bo_id_to_bo_info_map,
                                               0,
                                              &bo_ptr,
                                              &bo_hash) )
        {
            delete bo_ptr;
            bo_ptr = NULL;

            system_hash64map_remove(bindings_ptr->bo_id_to_bo_info_map,
                                    bo_hash);
        }

        system_hash64map_release(bindings_ptr->bo_id_to_bo_info_map);

        bindings_ptr->bo_id_to_bo_info_map = NULL;
    }

    /* Done */
    delete bindings_ptr;

    bindings_ptr = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_reset_buffer_bindings(__in __notnull ogl_context_bo_bindings bindings,
                                                          __in __notnull const GLuint*           buffers,
                                                          __in           GLint                   n)
{
    _ogl_context_bo_bindings*    bindings_ptr = (_ogl_context_bo_bindings*) bindings;
    const ogl_context_gl_limits* limits_ptr   = NULL;

    ogl_context_get_property(bindings_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    _ogl_context_bo_bindings_general_binding* general_bindings[] =
    {
        bindings_ptr->bindings_general_context,
        bindings_ptr->bindings_general_local
    };
    const unsigned int n_general_bindings = sizeof(general_bindings) / sizeof(general_bindings[0]);

    struct _indiced_binding
    {
        _ogl_context_bo_binding_target target;
        GLint                          n_bindings;
    } indiced_bindings[] =
    {
        {BINDING_TARGET_ATOMIC_COUNTER_BUFFER,     limits_ptr->max_atomic_counter_buffer_bindings},
        {BINDING_TARGET_SHADER_STORAGE_BUFFER,     limits_ptr->max_shader_storage_buffer_bindings},
        {BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER, limits_ptr->max_transform_feedback_buffers},
        {BINDING_TARGET_UNIFORM_BUFFER,            limits_ptr->max_uniform_buffer_bindings},
    };
    const unsigned int n_indiced_bindings = sizeof(indiced_bindings) / sizeof(indiced_bindings[0]);

    /* Iterate over all input buffer objects */
    for (GLint n_buffer = 0;
               n_buffer < n;
            ++n_buffer)
    {
        /* Iterate over general bindings */
        for (unsigned int n_general_binding = 0;
                          n_general_binding < n_general_bindings;
                        ++n_general_binding)
        {
            _ogl_context_bo_bindings_general_binding* general_bindings_ptr = general_bindings[n_general_binding];

            for (unsigned int n_binding = 0;
                              n_binding < BINDING_TARGET_COUNT;
                            ++n_binding)
            {
                _ogl_context_bo_bindings_general_binding* binding_ptr = general_bindings_ptr + n_binding;

                if (binding_ptr->bo_id == buffers[n_buffer])
                {
                    /* Reset the binding */
                    binding_ptr->bo_id = 0;
                }
            }
        } /* for (both general binding descriptor types) */

        /* Iterate over indiced bindings */
        for (unsigned int n_indiced_binding = 0;
                          n_indiced_binding < n_indiced_bindings;
                        ++n_indiced_binding)
        {
            const _indiced_binding* indiced_binding_ptr = indiced_bindings + n_indiced_binding;

            for (unsigned int n_binding_type = 0;
                              n_binding_type < 2; /* context / local */
                            ++n_binding_type)
            {
                _ogl_context_bo_bindings_indiced_binding** indiced_bindings_ptr = (n_binding_type == 0) ? bindings_ptr->bindings_indiced_context
                                                                                                        : bindings_ptr->bindings_indiced_local;

                for (int n_binding = 0;
                         n_binding < indiced_binding_ptr->n_bindings;
                       ++n_binding)
                {
                    _ogl_context_bo_bindings_indiced_binding* current_binding_ptr = indiced_bindings_ptr[indiced_binding_ptr->target];

                    if (current_binding_ptr[n_binding].bo_id == buffers[n_buffer])
                    {
                        /* Reset the binding */
                        current_binding_ptr[n_binding].bo_id = 0;
                        current_binding_ptr[n_binding].dirty = true;
                    }
                } /* for (all indiced buffer object bindings) */
            } /* for (both binding types) */
        } /* for (all indiced binding descriptors) */
    } /* for (all input buffer object IDs) */
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_set_binding(__in __notnull ogl_context_bo_bindings bindings,
                                                __in           GLenum                  binding_point,
                                                __in           GLint                   bo_id)
{
    _ogl_context_bo_bindings*      bindings_ptr   = (_ogl_context_bo_bindings*) bindings;
    _ogl_context_bo_binding_target binding_target = _ogl_context_bo_bindings_get_ogl_context_bo_binding_target_from_glenum(binding_point);

    ASSERT_DEBUG_SYNC(binding_target < BINDING_TARGET_COUNT,
                      "Invalid binding target requested");

    if (binding_target  < BINDING_TARGET_COUNT                 &&
        binding_target != BINDING_TARGET_ELEMENT_ARRAY_BUFFER) /* do not cache IB BP binding! this is a VAO state! */
    {
        bindings_ptr->bindings_general_local[binding_target].bo_id = bo_id;
    } /* if (binding_target != BINDING_TARGET_UNKNOWN) */
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_set_binding_base(__in __notnull ogl_context_bo_bindings bindings,
                                                     __in           GLenum                  binding_point,
                                                     __in           GLuint                  binding_index,
                                                     __in           GLint                   bo_id)
{
    _ogl_context_bo_bindings*      bindings_ptr   = (_ogl_context_bo_bindings*) bindings;
    _ogl_context_bo_binding_target binding_target = _ogl_context_bo_bindings_get_ogl_context_bo_binding_target_from_glenum(binding_point);

    ASSERT_DEBUG_SYNC(binding_target < BINDING_TARGET_COUNT_INDICED,
                      "Invalid binding target requested");

    if (binding_target < BINDING_TARGET_COUNT_INDICED)
    {
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].bo_id            = bo_id;
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].is_range_binding = false;
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].offset           = 0;
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].size             = 0;

        /* Update dirty flag */
        if (bindings_ptr->bindings_indiced_context[binding_target][binding_index].bo_id            != bindings_ptr->bindings_indiced_local[binding_target][binding_index].bo_id            ||
            bindings_ptr->bindings_indiced_context[binding_target][binding_index].is_range_binding != bindings_ptr->bindings_indiced_local[binding_target][binding_index].is_range_binding ||
            bindings_ptr->bindings_indiced_context[binding_target][binding_index].offset           != bindings_ptr->bindings_indiced_local[binding_target][binding_index].offset           ||
            bindings_ptr->bindings_indiced_context[binding_target][binding_index].size             != bindings_ptr->bindings_indiced_local[binding_target][binding_index].size)
        {
            bindings_ptr->bindings_indiced_local[binding_target][binding_index].dirty = true;
        }
        else
        {
            bindings_ptr->bindings_indiced_local[binding_target][binding_index].dirty = false;
        }
    } /* if (binding_target < BINDING_TARGET_COUNT_INDICED) */
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_set_binding_range(__in __notnull ogl_context_bo_bindings bindings,
                                                      __in           GLenum                  binding_point,
                                                      __in           GLuint                  binding_index,
                                                      __in           GLintptr                offset,
                                                      __in           GLsizeiptr              size,
                                                      __in           GLint                   bo_id)
{
    _ogl_context_bo_bindings*      bindings_ptr   = (_ogl_context_bo_bindings*) bindings;
    _ogl_context_bo_binding_target binding_target = _ogl_context_bo_bindings_get_ogl_context_bo_binding_target_from_glenum(binding_point);

    ASSERT_DEBUG_SYNC(binding_target < BINDING_TARGET_COUNT_INDICED,
                      "Invalid binding target requested");
    if (binding_target < BINDING_TARGET_COUNT_INDICED)
    {
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].bo_id            = bo_id;
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].is_range_binding = true;
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].offset           = offset;
        bindings_ptr->bindings_indiced_local[binding_target][binding_index].size             = size;

        if (bindings_ptr->bindings_indiced_context[binding_target][binding_index].bo_id            != bindings_ptr->bindings_indiced_local[binding_target][binding_index].bo_id            ||
            bindings_ptr->bindings_indiced_context[binding_target][binding_index].is_range_binding != bindings_ptr->bindings_indiced_local[binding_target][binding_index].is_range_binding ||
            bindings_ptr->bindings_indiced_context[binding_target][binding_index].offset           != bindings_ptr->bindings_indiced_local[binding_target][binding_index].offset           ||
            bindings_ptr->bindings_indiced_context[binding_target][binding_index].size             != bindings_ptr->bindings_indiced_local[binding_target][binding_index].size)
        {
            bindings_ptr->bindings_indiced_local[binding_target][binding_index].dirty = true;
        }
        else
        {
            bindings_ptr->bindings_indiced_local[binding_target][binding_index].dirty = false;
        }
    } /* if (binding_target < BINDING_TARGET_COUNT_INDICED) */

    #ifdef _DEBUG
    {
        /* Perform alignment sanity checks.
         *
         * TODO: These checks should actually be executed upon a draw call as a part of the validation.
         *       However, that would be too costly. Since we're doing this specifically for early-debugging
         *       purposes, please improve when necessary.
         */
        switch (binding_point)
        {
            case GL_SHADER_STORAGE_BUFFER:
            {
                ASSERT_DEBUG_SYNC( (offset % bindings_ptr->limits_ptr->shader_storage_buffer_offset_alignment) == 0,
                                  "A misaligned SSBO ranged binding was detected.");

                break;
            }

            case GL_TEXTURE_BUFFER:
            {
                ASSERT_DEBUG_SYNC( (offset % bindings_ptr->limits_ptr->texture_buffer_offset_alignment) == 0,
                                  "A misaligned TBO ranged binding was detected.");

                break;
            }

            case GL_UNIFORM_BUFFER:
            {
                ASSERT_DEBUG_SYNC( (offset % bindings_ptr->limits_ptr->uniform_buffer_offset_alignment) == 0,
                                  "A misaligned UBO ranged binding was detected.");

                break;
            }
        } /* switch (binding_point) */
    }
    #endif
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_set_bo_storage_size(__in __notnull ogl_context_bo_bindings bindings,
                                                        __in           GLuint                  bo_id,
                                                        __in           GLsizeiptr              bo_size)
{
    _ogl_context_bo_bindings* bindings_ptr = (_ogl_context_bo_bindings*) bindings;

    /* Is the BO already recognized? */
    _ogl_context_bo_bindings_bo_info* bo_ptr = NULL;

    if (!system_hash64map_get(bindings_ptr->bo_id_to_bo_info_map,
                              bo_id,
                             &bo_ptr) )
    {
        /* Nope. Allocate a new descriptor */
        bo_ptr = new (std::nothrow) _ogl_context_bo_bindings_bo_info;

        ASSERT_ALWAYS_SYNC(bo_ptr != NULL,
                           "Out of memory");

        if (bo_ptr == NULL)
        {
            goto end;
        }

        system_hash64map_insert(bindings_ptr->bo_id_to_bo_info_map,
                                bo_id,
                                bo_ptr,
                                NULL,  /* on_remove_callback_proc */
                                NULL); /* on_remove_callback_proc_user_arg */
    }

    /* Update the size */
    bo_ptr->size = bo_size;

end:
    ;
}

/** Please see header for spec */
PUBLIC void ogl_context_bo_bindings_sync(__in __notnull ogl_context_bo_bindings bindings,
                                         __in           uint32_t                sync_bits)
{
    /* NOTE: bindings is NULL during rendering context initialization */
    if (bindings != NULL)
    {
        _ogl_context_bo_bindings*    bindings_ptr = (_ogl_context_bo_bindings*) bindings;
        const ogl_context_gl_limits* limits_ptr   = NULL;

        ogl_context_get_property(bindings_ptr->context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        /* For indiced bindings, use GL_ARB_multi_bind if available */
        if (sync_bits & BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER)
        {
            if (bindings_ptr->is_arb_multi_bind_supported)
            {
                _ogl_context_bo_bindings_sync_multi_bind_process_indiced_sync_bit(bindings,
                                                                                  limits_ptr->max_atomic_counter_buffer_bindings,
                                                                                  BINDING_TARGET_ATOMIC_COUNTER_BUFFER,
                                                                                  GL_ATOMIC_COUNTER_BUFFER);
            }
            else
            {
                _ogl_context_bo_bindings_sync_non_multi_bind_process_indiced_sync_bit(bindings,
                                                                                      limits_ptr->max_atomic_counter_buffer_bindings,
                                                                                      BINDING_TARGET_ATOMIC_COUNTER_BUFFER);
            }
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER)
        {
            if (bindings_ptr->is_arb_multi_bind_supported)
            {
                _ogl_context_bo_bindings_sync_multi_bind_process_indiced_sync_bit(bindings,
                                                                                  limits_ptr->max_shader_storage_buffer_bindings,
                                                                                  BINDING_TARGET_SHADER_STORAGE_BUFFER,
                                                                                  GL_SHADER_STORAGE_BUFFER);
            }
            else
            {
                _ogl_context_bo_bindings_sync_non_multi_bind_process_indiced_sync_bit(bindings,
                                                                                      limits_ptr->max_shader_storage_buffer_bindings,
                                                                                      BINDING_TARGET_SHADER_STORAGE_BUFFER);
            }
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER)
        {
            if (bindings_ptr->is_arb_multi_bind_supported)
            {
                _ogl_context_bo_bindings_sync_multi_bind_process_indiced_sync_bit(bindings,
                                                                                  limits_ptr->max_transform_feedback_buffers,
                                                                                  BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER,
                                                                                  GL_TRANSFORM_FEEDBACK_BUFFER);
            }
            else
            {
                _ogl_context_bo_bindings_sync_non_multi_bind_process_indiced_sync_bit(bindings,
                                                                                      limits_ptr->max_transform_feedback_buffers,
                                                                                      BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER);
            }
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER)
        {
            if (bindings_ptr->is_arb_multi_bind_supported)
            {
                _ogl_context_bo_bindings_sync_multi_bind_process_indiced_sync_bit(bindings,
                                                                                  limits_ptr->max_uniform_buffer_bindings,
                                                                                  BINDING_TARGET_UNIFORM_BUFFER,
                                                                                  GL_UNIFORM_BUFFER);
            }
            else
            {
                _ogl_context_bo_bindings_sync_non_multi_bind_process_indiced_sync_bit(bindings,
                                                                                      limits_ptr->max_uniform_buffer_bindings,
                                                                                      BINDING_TARGET_UNIFORM_BUFFER);
            }
        }

        /* Set up general binding arrays. */
        if (sync_bits & BO_BINDINGS_SYNC_BIT_ARRAY_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_ARRAY_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_ATOMIC_COUNTER_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_ATOMIC_COUNTER_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_COPY_READ_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_COPY_READ_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_COPY_WRITE_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_COPY_WRITE_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_DISPATCH_INDIRECT_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_DISPATCH_INDIRECT_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_DRAW_INDIRECT_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_DRAW_INDIRECT_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_PIXEL_PACK_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_PIXEL_PACK_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_PIXEL_UNPACK_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_PIXEL_UNPACK_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_QUERY_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_QUERY_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_SHADER_STORAGE_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_SHADER_STORAGE_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_TEXTURE_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_TEXTURE_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_TRANSFORM_FEEDBACK_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_TRANSFORM_FEEDBACK_BUFFER);
        }

        if (sync_bits & BO_BINDINGS_SYNC_BIT_UNIFORM_BUFFER)
        {
            _ogl_context_bo_bindings_sync_process_general_sync_bit(bindings,
                                                                   BINDING_TARGET_UNIFORM_BUFFER);
        }
    }
}