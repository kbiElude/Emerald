/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"

#define NONSPARSE_BUFFER_SIZE (32  * 1024768)
#define SPARSE_BUFFER_SIZE    (512 * 1024768)

typedef struct _ogl_buffers_buffer
{
    GLuint                   bo_id;
    struct _ogl_buffers*     buffers_ptr;
    _ogl_buffers_mappability mappability;
    system_memory_manager    manager;

    ~_ogl_buffers_buffer()
    {
        ASSERT_DEBUG_SYNC(bo_id != 0,
                          "Buffer Object not deallocated but the BO descriptor destructor was called");

        if (manager != NULL)
        {
            system_memory_manager_release(manager);

            manager = NULL;
        }
    }
} _ogl_buffers_buffer;

typedef struct _ogl_buffers
{
    bool                                           are_sparse_buffers_in;
    system_resource_pool                           buffer_descriptor_pool;
    ogl_context                                    context; /* do not retain - else face circular dependencies */
    ogl_context_gl_entrypoints*                    entry_points;
    ogl_context_gl_entrypoints_arb_buffer_storage* entry_points_bs;
    ogl_context_gl_entrypoints_arb_sparse_buffer*  entry_points_sb;
    system_hashed_ansi_string                      name;
    system_resizable_vector                        nonsparse_buffers;
    unsigned int                                   page_size;
    system_resizable_vector                        sparse_buffers; /* sparse BOs are not mappable! */

    _ogl_buffers(__in __notnull ogl_context               in_context,
                 __in __notnull system_hashed_ansi_string in_name)
    {
        are_sparse_buffers_in  = false;
        buffer_descriptor_pool = system_resource_pool_create(sizeof(_ogl_buffers_buffer),
                                                             16,    /* n_elements_to_preallocate */
                                                             NULL,  /* init_fn */
                                                             NULL); /* deinit_fn */
        context                = in_context;
        entry_points           = NULL;
        entry_points_bs        = NULL;
        entry_points_sb        = NULL;
        name                   = in_name;
        nonsparse_buffers      = system_resizable_vector_create(16, /* capacity */
                                                                sizeof(_ogl_buffers_buffer*) );
        page_size              = 0;
        sparse_buffers         = system_resizable_vector_create(16, /* capacity */
                                                                sizeof(_ogl_buffers_buffer*) );

        /* Cache the entry-points */
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE,
                                &entry_points_bs);

        /* Are sparse buffers supported? */
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SPARSE_BUFFERS,
                                &are_sparse_buffers_in);

        if (are_sparse_buffers_in)
        {
            /* Retrieve entry-point & limits descriptors */
            const ogl_context_gl_limits_arb_sparse_buffer* limits_sb = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SPARSE_BUFFER,
                                    &entry_points_sb);
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_LIMITS_ARB_SPARSE_BUFFER,
                                    &limits_sb);

            page_size = limits_sb->sparse_buffer_page_size;
        } /* if (are_sparse_buffers_in) */
    }

    ~_ogl_buffers()
    {
        if (buffer_descriptor_pool != NULL)
        {
            system_resource_pool_release(buffer_descriptor_pool);

            buffer_descriptor_pool = NULL;
        }

        if (nonsparse_buffers != NULL)
        {
            system_resizable_vector_release(nonsparse_buffers);

            nonsparse_buffers = NULL;
        }

        if (sparse_buffers != NULL)
        {
            system_resizable_vector_release(sparse_buffers);

            sparse_buffers = NULL;
        }
    }
} _ogl_buffers;


/** Forward declarations */
PRIVATE void _ogl_buffers_alloc_new_sparse_buffer(__in __notnull _ogl_buffers*         buffers_ptr);
PRIVATE void _ogl_buffers_on_memory_block_alloced(__in __notnull system_memory_manager manager,
                                                  __in           unsigned int          offset_aligned,
                                                  __in           unsigned int          size,
                                                  __in __notnull void*                 user_arg);
PRIVATE void _ogl_buffers_on_memory_block_freed  (__in __notnull system_memory_manager manager,
                                                  __in           unsigned int          offset_aligned,
                                                  __in           unsigned int          size,
                                                  __in __notnull void*                 user_arg);

/** TODO */
PRIVATE void _ogl_buffers_alloc_new_sparse_buffer(__in __notnull _ogl_buffers* buffers_ptr)
{
    _ogl_buffers_buffer* new_buffer_ptr = (_ogl_buffers_buffer*) system_resource_pool_get_from_pool(buffers_ptr->buffer_descriptor_pool);

    buffers_ptr->entry_points->pGLGenBuffers      (1,
                                                  &new_buffer_ptr->bo_id);
    buffers_ptr->entry_points->pGLBindBuffer      (GL_ARRAY_BUFFER,
                                                   new_buffer_ptr->bo_id);
    buffers_ptr->entry_points_bs->pGLBufferStorage(GL_ARRAY_BUFFER,
                                                   SPARSE_BUFFER_SIZE,
                                                   NULL, /* data */
                                                   GL_SPARSE_STORAGE_BIT_ARB |
                                                   GL_DYNAMIC_STORAGE_BIT);

    new_buffer_ptr->buffers_ptr = buffers_ptr;
    new_buffer_ptr->mappability = OGL_BUFFERS_MAPPABILITY_NONE;
    new_buffer_ptr->manager     = system_memory_manager_create(SPARSE_BUFFER_SIZE,
                                                               buffers_ptr->page_size,
                                                               _ogl_buffers_on_memory_block_alloced,
                                                               _ogl_buffers_on_memory_block_freed,
                                                               new_buffer_ptr,
                                                               false); /* should_be_thread_safe */
}

/** TODO */
PRIVATE void _ogl_buffers_on_memory_block_alloced(__in __notnull system_memory_manager manager,
                                                  __in           unsigned int          offset_aligned,
                                                  __in           unsigned int          size,
                                                  __in __notnull void*                 user_arg)
{
    _ogl_buffers_buffer* buffer_ptr = (_ogl_buffers_buffer*) user_arg;

    /* Commit the pages the caller will later fill with data. */
    buffer_ptr->buffers_ptr->entry_points_sb->pGLNamedBufferPageCommitmentEXT(buffer_ptr->bo_id,
                                                                              offset_aligned,
                                                                              size,
                                                                              GL_TRUE); /* commit */
}

/** TODO */
PRIVATE void _ogl_buffers_on_memory_block_freed(__in __notnull system_memory_manager manager,
                                                __in           unsigned int          offset_aligned,
                                                __in           unsigned int          size,
                                                __in __notnull void*                 user_arg)
{
    _ogl_buffers_buffer* buffer_ptr = (_ogl_buffers_buffer*) user_arg;

    /* Release the pages we no longer need.
     *
     * Note: system_memory_manager ensures the pages that we are told to release no longer hold
     *       any other data.
     */
    buffer_ptr->buffers_ptr->entry_points_sb->pGLNamedBufferPageCommitmentEXT(buffer_ptr->bo_id,
                                                                              offset_aligned,
                                                                              size,
                                                                              GL_FALSE); /* commit */
}


/** Please see header for spec */
PUBLIC ogl_buffers ogl_buffers_create(__in __notnull ogl_context               context,
                                      __in __notnull system_hashed_ansi_string name)
{
    _ogl_buffers* new_buffers = new (std::nothrow) _ogl_buffers(context,
                                                                name);

    ASSERT_DEBUG_SYNC(new_buffers != NULL,
                      "Out of memory");

    if (new_buffers != NULL)
    {
        /* If sparse buffers are supported, allocate a single sparse buffer */
        if (new_buffers->are_sparse_buffers_in)
        {
            _ogl_buffers_alloc_new_sparse_buffer(new_buffers);
        } /* if (new_buffers->are_sparse_buffers_in) */
        else
        {
            // _ogl_buffers_alloc_new_immutable_buffer(new_buffers);

            // todo..
        }
    }

    return (ogl_buffers) new_buffers;
}

/** Please see header for spec */
PUBLIC void ogl_buffers_release(__in __notnull ogl_buffers buffers)
{
    delete (_ogl_buffers*) buffers;

    buffers = NULL;
}

