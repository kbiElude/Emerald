/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "ral/ral_buffer.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"


typedef struct _raGL_buffer
{
    ral_buffer  buffer;
    ogl_context context; /* NOT owned */
    GLint       id;      /* NOT owned */
    uint32_t    size;
    uint32_t    start_offset;

    ogl_context_gl_entrypoints* entrypoints_ptr;

    _raGL_buffer(ogl_context in_context,
                 GLint       in_id,
                 ral_buffer  in_buffer)
    {
        buffer       = in_buffer;
        context      = in_context;
        id           = in_id;
        size         = 0;
        start_offset = -1;

        /* NOTE: Only GL is supported at the moment. */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "TODO");
    }

    ~_raGL_buffer()
    {
        /* Do not release the buffer. Object life-time is handled by raGL_backend. */
    }
} _raGL_sampler;


/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_buffer_init_rendering_thread_callback(ogl_context context,
                                                                                void*       buffer)
{
    GLenum                init_bp            = GL_NONE;
    _raGL_sampler*        buffer_ptr         = (_raGL_buffer*) buffer;
    uint32_t              buffer_size        = 0;
    ral_buffer_usage_bits buffer_usage       = 0;
    ral_queue_bits        buffer_user_queues = 0;

    LOG_ERROR("Performance warning: raGL_buffer sync request.");

    /* Sanity checks */
    ASSERT_DEBUG_SYNC (buffer_ptr != NULL,
                       "Input raGL_buffer instance is NULL");
    ASSERT_DEBUG_SYNC (context == buffer_ptr->context,
                       "Rendering context mismatch");

    /* Retrieve RAL buffer properties */
    ral_buffer_get_property(buffer_ptr->buffer,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &buffer_size);
    ral_buffer_get_property(buffer_ptr->buffer,
                            RAL_BUFFER_PROPERTY_USAGE_BITS,
                           &buffer_usage);
    ral_buffer_get_property(buffer_ptr->buffer,
                            RAL_BUFFER_PROPERTY_USER_QUEUE_BITS,
                           &buffer_user_queues);

    /* Spawn an immutable buffer. If there's a specific single usage bit specific, use a corresponding
     * general binding point. If more bits are lit or RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT is the only bit set,
     * stick to the array buffer binding. */
    init_bp = GL_ARRAY_BUFFER;

    if ((buffer_usage &  RAL_BUFFER_USAGE_COPY_BIT) != 0 &&
        (buffer_usage & ~RAL_BUFFER_USAGE_COPY_BIT) == 0)
    {
        init_bp = GL_COPY_READ_BUFFER;
    }
    else
    if ((buffer_usage &  RAL_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0 &&
        (buffer_usage & ~RAL_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0)
    {
        init_bp = GL_ELEMENT_ARRAY_BUFFER;
    }
    else
    if ((buffer_usage &  RAL_BUFFER_USAGE_INDIRECT_DISPATCH_BUFFER_BIT) != 0 &&
        (buffer_usage & ~RAL_BUFFER_USAGE_INDIRECT_DISPATCH_BUFFER_BIT) == 0)
    {
        init_bp = GL_DISPATCH_INDIRECT_BUFFER;
    }
    else
    if ((buffer_usage &  RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT) != 0 &&
        (buffer_usage & ~RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT) == 0)
    {
        init_bp = GL_DRAW_INDIRECT_BUFFER;
    }
    else
    if ((buffer_usage &  RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT) != 0 &&
        (buffer_usage & ~RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT) == 0)
    {
        init_bp = GL_SHADER_STORAGE_BUFFER;
    }
    else
    if ((buffer_usage &  RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0 &&
        (buffer_usage & ~RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
    {
        init_bp = GL_UNIFORM_BUFFER;
    }

}


/** Please see header for specification */
PUBLIC raGL_buffer raGL_buffer_create(ogl_context context,
                                      GLint       bo_id,
                                      ral_buffer  buffer)
{
    _raGL_buffer* new_buffer_ptr = new (std::nothrow) _raGL_buffer(context,
                                                                   bo_id,
                                                                   buffer);

    ASSERT_ALWAYS_SYNC(new_buffer_ptr != NULL,
                       "Out of memory");

    if (new_buffer_ptr != NULL)
    {
        /* Request rendering thread call-back to set up the sampler object */
        ogl_context_request_callback_from_context_thread(context,
                                                         _raGL_buffer_init_rendering_thread_callback,
                                                         new_buffer_ptr);
    } /* if (new_sampler_ptr != NULL) */

    return (raGL_buffer) new_buffer_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_buffer_release(raGL_buffer buffer)
{
    ASSERT_DEBUG_SYNC(buffer != NULL,
                      "Input buffer is NULL");

    delete (_raGL_buffer*) buffer;
}
