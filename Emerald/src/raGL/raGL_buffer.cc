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
#include "system/system_resizable_vector.h"


typedef struct _raGL_buffer
{
    system_resizable_vector child_buffers; /* DOES NOT own _raGL_buffer* instances */
    ogl_context             context;       /* NOT owned */
    GLint                   id;            /* NOT owned */
    system_memory_manager   memory_manager;
    _raGL_buffer*           parent_buffer_ptr;
    uint32_t                size;
    uint32_t                start_offset;

    ogl_context_gl_entrypoints* entrypoints_ptr;

    _raGL_buffer(_raGL_buffer*         in_parent_buffer_ptr,
                 ogl_context           in_context,
                 GLint                 in_id,
                 system_memory_manager in_memory_manager,
                 uint32_t              in_start_offset,
                 uint32_t              in_size)
    {
        child_buffers     = system_resizable_vector_create(sizeof(_raGL_buffer*) );
        context           = in_context;
        id                = in_id;
        memory_manager    = in_memory_manager;
        parent_buffer_ptr = in_parent_buffer_ptr;
        size              = in_size;
        start_offset      = in_start_offset;

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
        if (child_buffers != NULL)
        {
            system_resizable_vector_release(child_buffers);

            child_buffers = NULL;
        } /* if (child_buffers != NULL) */

        if (parent_buffer_ptr != NULL)
        {
            /* Detach this instance from the parent */
            size_t child_index = system_resizable_vector_find(parent_buffer_ptr->child_buffers,
                                                              this);

            ASSERT_DEBUG_SYNC(child_index != ITEM_NOT_FOUND,
                              "Could not detach a child raGL_buffer instance from the parent.");
            if (child_index != ITEM_NOT_FOUND)
            {
                system_resizable_vector_delete_element_at(parent_buffer_ptr->child_buffers,
                                                          child_index);
            }
        }

        /* Do not release the buffer. Object life-time is handled by raGL_backend. */
    }
} _raGL_sampler;


/** Please see header for specification */
PUBLIC raGL_buffer raGL_buffer_create(ogl_context           context,
                                      GLint                 id,
                                      system_memory_manager memory_manager,
                                      uint32_t              start_offset,
                                      uint32_t              size)
{
    _raGL_buffer* new_buffer_ptr = new (std::nothrow) _raGL_buffer(NULL,    /* in_parent_buffer_ptr */
                                                                   context,
                                                                   id,
                                                                   memory_manager,
                                                                   start_offset,
                                                                   size);

    ASSERT_ALWAYS_SYNC(new_buffer_ptr != NULL,
                       "Out of memory");

    return (raGL_buffer) new_buffer_ptr;
}

/** Please see header for specification */
PUBLIC raGL_buffer raGL_buffer_create_raGL_buffer_subregion(raGL_buffer parent_buffer,
                                                            uint32_t    start_offset,
                                                            uint32_t    size)
{
    _raGL_buffer* parent_buffer_ptr = (_raGL_buffer*) parent_buffer;
    _raGL_buffer* result_ptr        = NULL;

    uint32_t new_size         = size;
    uint32_t new_start_offset = 0;

    /* Sanity checks */
    if (parent_buffer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Parent raGL_buffer instance is NULL");

        goto end;
    }

    new_start_offset = parent_buffer_ptr->start_offset + start_offset;

    if (new_size == 0)
    {
        ASSERT_DEBUG_SYNC(parent_buffer_ptr->size > new_start_offset,
                          "Zero-sized buffer region creation attempt");

        new_size = parent_buffer_ptr->size - new_start_offset;
    }

    if (new_start_offset >= parent_buffer_ptr->size)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Start offset exceeds available storage size.");

        goto end;
    }

    if (new_start_offset + new_size > parent_buffer_ptr->size)
    {
        ASSERT_DEBUG_SYNC(false,
                          "End offset exceeds available storage size.");

        goto end;
    }

    /* Create a new descriptor */
    result_ptr = new (std::nothrow) _raGL_buffer(parent_buffer_ptr,
                                                 parent_buffer_ptr->context,
                                                 parent_buffer_ptr->id,
                                                 parent_buffer_ptr->memory_manager,
                                                 new_start_offset,
                                                 new_size);

    if (result_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    /* Register the descriptor as the parent buffer's child */
    system_resizable_vector_push(parent_buffer_ptr->child_buffers,
                                 result_ptr);

end:
    return (raGL_buffer) result_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void raGL_buffer_get_property(raGL_buffer          buffer,
                                                 raGL_buffer_property property,
                                                 void*                out_result_ptr)
{
    _raGL_buffer* buffer_ptr = (_raGL_buffer*) buffer;

    /* Sanity checks */
    if (buffer_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_buffer instance is NULL");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAGL_BUFFER_PROPERTY_ID:
        {
            *(GLuint*) out_result_ptr = buffer_ptr->id;

            break;
        }

        case RAGL_BUFFER_PROPERTY_MEMORY_MANAGER:
        {
            *(system_memory_manager*) out_result_ptr = buffer_ptr->memory_manager;

            break;
        }

        case RAGL_BUFFER_PROPERTY_START_OFFSET:
        {
            *(uint32_t*) out_result_ptr = buffer_ptr->start_offset;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_buffer_property value.");
        }
    } /* switch (property) */
end:
    ;
}
/** Please see header for specification */
PUBLIC void raGL_buffer_release(raGL_buffer buffer)
{
    _raGL_buffer* buffer_ptr      = (_raGL_buffer*) buffer;
    uint32_t      n_child_buffers = 0;

    ASSERT_DEBUG_SYNC(buffer != NULL,
                      "Input buffer is NULL");

    system_resizable_vector_get_property(buffer_ptr->child_buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_child_buffers);

    ASSERT_DEBUG_SYNC(n_child_buffers == 0,
                      "Releasing a buffer object with children buffers still living!");

    delete (_raGL_buffer*) buffer;
}
