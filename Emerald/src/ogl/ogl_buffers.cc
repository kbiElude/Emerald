/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"


#define NONSPARSE_IBO_BUFFER_SIZE  (16  * 1024768)
#define NONSPARSE_MISC_BUFFER_SIZE (16  * 1024768)
#define NONSPARSE_UBO_BUFFER_SIZE  (4   * 1024768)
#define NONSPARSE_VBO_BUFFER_SIZE  (64  * 1024768)
#define SPARSE_BUFFER_SIZE         (512 * 1024768)

#define MAKE_BO_HASHMAP_KEY(bo_id, bo_offset)           \
    ((__int64(bo_id)     & ((1LL << 32) - 1))         | \
    ((__int64(bo_offset) & ((1LL << 32) - 1)) << 32))


typedef struct _ogl_buffers_buffer
{
    GLuint                   bo_id;
    struct _ogl_buffers*     buffers_ptr;
    _ogl_buffers_mappability mappability;
    GLvoid*                  mapped_gl_ptr;
    system_memory_manager    manager;

    ~_ogl_buffers_buffer();
} _ogl_buffers_buffer;

typedef struct _ogl_buffers
{
    bool                                           are_persistent_buffers_in;
    bool                                           are_sparse_buffers_in;
    system_hash64map                               bo_id_offset_hash_to_buffer_map;
    system_resource_pool                           buffer_descriptor_pool;
    ogl_context                                    context; /* do not retain - else face circular dependencies */

    ogl_context_es_entrypoints*                    entry_points_es;

    ogl_context_gl_entrypoints*                         entry_points_gl;
    ogl_context_gl_entrypoints_arb_buffer_storage*      entry_points_gl_bs;
    ogl_context_gl_entrypoints_ext_direct_state_access* entry_points_gl_dsa;
    ogl_context_gl_entrypoints_arb_sparse_buffer*       entry_points_gl_sb;

    system_hashed_ansi_string                      name;
    system_resizable_vector                        nonsparse_buffers[OGL_BUFFERS_USAGE_COUNT];
    unsigned int                                   page_size;
    system_resizable_vector                        sparse_buffers; /* sparse BOs are never mappable! */

    _ogl_buffers(__in __notnull ogl_context               in_context,
                 __in __notnull system_hashed_ansi_string in_name)
    {
        are_persistent_buffers_in       = false;
        are_sparse_buffers_in           = false;
        bo_id_offset_hash_to_buffer_map = system_hash64map_create    (sizeof(_ogl_buffers_buffer*) );
        buffer_descriptor_pool          = system_resource_pool_create(sizeof(_ogl_buffers_buffer),
                                                                      16,    /* n_elements_to_preallocate */
                                                                      NULL,  /* init_fn */
                                                                      NULL); /* deinit_fn */
        context                         = in_context;
        entry_points_es                 = NULL;
        entry_points_gl                 = NULL;
        entry_points_gl_bs              = NULL;
        entry_points_gl_dsa             = NULL;
        entry_points_gl_sb              = NULL;
        name                            = in_name;
        page_size                       = 0;
        sparse_buffers                  = system_resizable_vector_create(16, /* capacity */
                                                                         sizeof(_ogl_buffers_buffer*) );

        /* Initialize non-sparse buffer vectors */
        for (unsigned int n_buffer_usage = 0;
                          n_buffer_usage < OGL_BUFFERS_USAGE_COUNT;
                        ++n_buffer_usage)
        {
            nonsparse_buffers[n_buffer_usage] = system_resizable_vector_create(4, /* capacity */
                                                                               sizeof(_ogl_buffers_buffer*) );
        } /* for (all buffer usage types) */

        /* Cache the entry-points */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_es);
        } /* if (context_type == OGL_CONTEXT_TYPE_ES) */
        else
        {
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_gl);
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE,
                                    &entry_points_gl_bs);
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                    &entry_points_gl_dsa);

            /* Are persistent buffers supported? */
            #ifdef ENABLE_PERSISTENT_UB_STORAGE
            {
                ogl_context_get_property(context,
                                         OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE,
                                        &are_persistent_buffers_in);
            }
            #else
                are_persistent_buffers_in = false;
            #endif

            /* Are sparse buffers supported? */
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SPARSE_BUFFERS,
                                    &are_sparse_buffers_in);
        }

        if (are_sparse_buffers_in)
        {
            /* Retrieve entry-point & limits descriptors */
            const ogl_context_gl_limits_arb_sparse_buffer* limits_sb = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SPARSE_BUFFER,
                                    &entry_points_gl_sb);
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_LIMITS_ARB_SPARSE_BUFFER,
                                    &limits_sb);

            page_size = limits_sb->sparse_buffer_page_size;
        } /* if (are_sparse_buffers_in) */
    }

    ~_ogl_buffers()
    {
        if (bo_id_offset_hash_to_buffer_map != NULL)
        {
            system_hash64map_release(bo_id_offset_hash_to_buffer_map);

            bo_id_offset_hash_to_buffer_map = NULL;
        }

        if (nonsparse_buffers != NULL)
        {
            for (unsigned int n_buffer_usage = 0;
                              n_buffer_usage < OGL_BUFFERS_USAGE_COUNT;
                            ++n_buffer_usage)
            {
                _ogl_buffers_buffer* buffer_ptr = NULL;

                while (system_resizable_vector_pop(nonsparse_buffers[n_buffer_usage],
                                                  &buffer_ptr) )
                {
                    delete buffer_ptr;

                    buffer_ptr = NULL;
                } /* while (descriptors are available) */

                system_resizable_vector_release(nonsparse_buffers[n_buffer_usage]);

                nonsparse_buffers[n_buffer_usage] = NULL;
            } /* for (all buffer usage types) */
        } /* if (nonsparse_buffers != NULL) */

        if (sparse_buffers != NULL)
        {
            _ogl_buffers_buffer* buffer_ptr = NULL;

            while (system_resizable_vector_pop(sparse_buffers,
                                              &buffer_ptr) )
            {
                delete buffer_ptr;

                buffer_ptr = NULL;
            } /* while (descriptors are available) */

            system_resizable_vector_release(sparse_buffers);

            sparse_buffers = NULL;
        } /* if (sparse_buffers != NULL) */

        /* The descriptor pool needs to be released at the end */
        if (buffer_descriptor_pool != NULL)
        {
            system_resource_pool_release(buffer_descriptor_pool);

            buffer_descriptor_pool = NULL;
        } /* if (buffer_descriptor_pool != NULL) */
    }
} _ogl_buffers;


/** Forward declarations */
PRIVATE _ogl_buffers_buffer* _ogl_buffers_alloc_new_immutable_buffer    (__in __notnull _ogl_buffers*            buffers_ptr,
                                                                         __in           _ogl_buffers_mappability mappability,
                                                                         __in           _ogl_buffers_usage       usage,
                                                                         __in           unsigned int             required_size);
PRIVATE _ogl_buffers_buffer* _ogl_buffers_alloc_new_sparse_buffer       (__in __notnull _ogl_buffers*            buffers_ptr);
PRIVATE unsigned int         _ogl_buffers_get_nonsparse_buffer_size     (__in           _ogl_buffers_usage       usage);
PRIVATE void                 _ogl_buffers_on_sparse_memory_block_alloced(__in __notnull system_memory_manager    manager,
                                                                         __in           unsigned int             offset_aligned,
                                                                         __in           unsigned int             size,
                                                                         __in __notnull void*                    user_arg);
PRIVATE void                 _ogl_buffers_on_sparse_memory_block_freed  (__in __notnull system_memory_manager    manager,
                                                                         __in           unsigned int             offset_aligned,
                                                                         __in           unsigned int             size,
                                                                         __in __notnull void*                    user_arg);

/** TODO */
_ogl_buffers_buffer::~_ogl_buffers_buffer()
{
    if (bo_id != 0)
    {
        if (buffers_ptr->entry_points_gl != NULL)
        {
            buffers_ptr->entry_points_gl->pGLDeleteBuffers(1,
                                                          &bo_id);
        }
        else
        {
            ASSERT_DEBUG_SYNC(buffers_ptr->entry_points_es != NULL,
                              "ES and GL entry-point descriptors are unavailable.");

            buffers_ptr->entry_points_es->pGLDeleteBuffers(1,
                                                          &bo_id);
        }

        bo_id = 0;
    }

    if (manager != NULL)
    {
        system_memory_manager_release(manager);

        manager = NULL;
    }
}


/** TODO */
PRIVATE _ogl_buffers_buffer* _ogl_buffers_alloc_new_immutable_buffer(__in __notnull _ogl_buffers*            buffers_ptr,
                                                                     __in           _ogl_buffers_mappability mappability,
                                                                     __in           _ogl_buffers_usage       usage,
                                                                     __in           unsigned int             required_size)
{
    unsigned int         bo_flags       = GL_DYNAMIC_STORAGE_BIT;
    unsigned int         bo_size        = _ogl_buffers_get_nonsparse_buffer_size(usage);
    GLenum               bo_target      = 0;
    _ogl_buffers_buffer* new_buffer_ptr = (_ogl_buffers_buffer*) system_resource_pool_get_from_pool(buffers_ptr->buffer_descriptor_pool);

    switch (usage)
    {
        /* non-mappable */
        case OGL_BUFFERS_USAGE_IBO:
        {
            bo_target = GL_ELEMENT_ARRAY_BUFFER;

            break;
        }

        case OGL_BUFFERS_USAGE_UBO:
        {
            /* Assume the caller will want to update the UBO contents in the future. */
            if (buffers_ptr->are_persistent_buffers_in)
            {
                bo_flags  |= GL_MAP_COHERENT_BIT   |
                             GL_MAP_PERSISTENT_BIT |
                             GL_MAP_WRITE_BIT;
            }

            bo_target  = GL_UNIFORM_BUFFER;

            break;
        }

        case OGL_BUFFERS_USAGE_VBO:
        {
            bo_target = GL_ARRAY_BUFFER;

            break;
        }

        /* can either be mappable or non-mappable */
        case OGL_BUFFERS_USAGE_MISCELLANEOUS:
        {
            bo_flags |= (mappability == OGL_BUFFERS_MAPPABILITY_READ_AND_WRITE_OPS) ? (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT) :
                        (mappability == OGL_BUFFERS_MAPPABILITY_READ_OPS_ONLY)      ? (GL_MAP_READ_BIT)                    :
                        (mappability == OGL_BUFFERS_MAPPABILITY_WRITE_OPS_ONLY)     ? (GL_MAP_WRITE_BIT)                   :
                        0;

            bo_target = GL_ARRAY_BUFFER;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_buffers_usage value.");
        }
    } /* switch (usage) */

    /* If the caller demands a larger buffer, scale up the planned BO size */
    if (bo_size < required_size)
    {
        bo_size = required_size;
    }

    if (buffers_ptr->entry_points_gl != NULL)
    {
        ASSERT_DEBUG_SYNC(buffers_ptr->entry_points_gl_bs != NULL,
                          "Immutable buffer support is missing. Crash ahead.");

        buffers_ptr->entry_points_gl->pGLGenBuffers      (1,
                                                         &new_buffer_ptr->bo_id);
        buffers_ptr->entry_points_gl->pGLBindBuffer      (bo_target,
                                                          new_buffer_ptr->bo_id);
        buffers_ptr->entry_points_gl_bs->pGLBufferStorage(bo_target,
                                                          bo_size,
                                                          NULL, /* data */
                                                          bo_flags);
    }
    else
    {
        buffers_ptr->entry_points_es->pGLGenBuffers(1,
                                                   &new_buffer_ptr->bo_id);
        buffers_ptr->entry_points_es->pGLBindBuffer(bo_target,
                                                    new_buffer_ptr->bo_id);
        buffers_ptr->entry_points_es->pGLBufferData(bo_target,
                                                    bo_size,
                                                    NULL,           /* data */
                                                    GL_STATIC_DRAW);
    }

    /* Check if the request was successful */
    GLint64 reported_bo_size = 0;

    if (buffers_ptr->entry_points_gl != NULL)
    {
        buffers_ptr->entry_points_gl->pGLGetBufferParameteri64v(bo_target,
                                                                GL_BUFFER_SIZE,
                                                               &reported_bo_size);
    }
    else
    {
        buffers_ptr->entry_points_es->pGLGetBufferParameteri64v(bo_target,
                                                                GL_BUFFER_SIZE,
                                                               &reported_bo_size);
    }

    if (reported_bo_size >= bo_size)
    {
        /* Persistent buffer memory should be mapped at all times, so that the block owners
         * can modify it any time they wish. */
        if ((bo_flags & GL_MAP_PERSISTENT_BIT) != 0)
        {
            new_buffer_ptr->mapped_gl_ptr = buffers_ptr->entry_points_gl->pGLMapBufferRange(bo_target,
                                                                                            0, /* offset */
                                                                                            bo_size,
                                                                                            (bo_flags & ~GL_DYNAMIC_STORAGE_BIT) | GL_MAP_UNSYNCHRONIZED_BIT);
        }
        else
        {
            new_buffer_ptr->mapped_gl_ptr = NULL;
        }

        new_buffer_ptr->buffers_ptr = buffers_ptr;
        new_buffer_ptr->mappability = mappability;
        new_buffer_ptr->manager     = system_memory_manager_create(bo_size,
                                                                   1,             /* page_size - immutable buffers have no special requirements */
                                                                   NULL,          /* pfn_on_memory_block_alloced */
                                                                   NULL,          /* pfn_on_memory_block_freed */
                                                                   new_buffer_ptr,
                                                                   false);        /* should_be_thread_safe */

        system_resizable_vector_push(new_buffer_ptr->buffers_ptr->nonsparse_buffers[usage],
                                     new_buffer_ptr);
    } /* if (reported_bo_size >= bo_size) */
    else
    {
        /* BO allocation failure! */
        ASSERT_DEBUG_SYNC(false,
                          "Failed to allocate storage for an immutable buffer object.");

        if (buffers_ptr->entry_points_gl != NULL)
        {
            if (new_buffer_ptr->mapped_gl_ptr != NULL)
            {
                buffers_ptr->entry_points_gl->pGLUnmapBuffer(bo_target);
            }

            buffers_ptr->entry_points_gl->pGLDeleteBuffers(1,
                                                          &new_buffer_ptr->bo_id);
        }
        else
        {
            buffers_ptr->entry_points_es->pGLDeleteBuffers(1,
                                                          &new_buffer_ptr->bo_id);
        }

        system_resource_pool_return_to_pool(buffers_ptr->buffer_descriptor_pool,
                                            (system_resource_pool_block) new_buffer_ptr);

        new_buffer_ptr = NULL;
    }

    return new_buffer_ptr;
}

/** TODO */
PRIVATE _ogl_buffers_buffer* _ogl_buffers_alloc_new_sparse_buffer(__in __notnull _ogl_buffers* buffers_ptr)
{
    _ogl_buffers_buffer* new_buffer_ptr = (_ogl_buffers_buffer*) system_resource_pool_get_from_pool(buffers_ptr->buffer_descriptor_pool);

    ASSERT_DEBUG_SYNC(buffers_ptr->entry_points_gl    != NULL &&
                      buffers_ptr->entry_points_gl_bs != NULL,
                      "GL_ARB_buffer_storage and/or GL entry-points are unavailable.");

    buffers_ptr->entry_points_gl->pGLGenBuffers      (1,
                                                      &new_buffer_ptr->bo_id);
    buffers_ptr->entry_points_gl->pGLBindBuffer      (GL_ARRAY_BUFFER,
                                                      new_buffer_ptr->bo_id);
    buffers_ptr->entry_points_gl_bs->pGLBufferStorage(GL_ARRAY_BUFFER,
                                                      SPARSE_BUFFER_SIZE,
                                                      NULL, /* data */
                                                      GL_SPARSE_STORAGE_BIT_ARB |
                                                      GL_DYNAMIC_STORAGE_BIT);

    /* Check if the request was successful */
    GLint64 reported_bo_size = 0;

    buffers_ptr->entry_points_gl->pGLGetBufferParameteri64v(GL_ARRAY_BUFFER,
                                                            GL_BUFFER_SIZE,
                                                           &reported_bo_size);

    if (reported_bo_size >= SPARSE_BUFFER_SIZE)
    {
        new_buffer_ptr->buffers_ptr   = buffers_ptr;
        new_buffer_ptr->mappability   = OGL_BUFFERS_MAPPABILITY_NONE;
        new_buffer_ptr->mapped_gl_ptr = NULL; /* mapping sparse buffers = no no! */
        new_buffer_ptr->manager       = system_memory_manager_create(SPARSE_BUFFER_SIZE,
                                                                     buffers_ptr->page_size,
                                                                     _ogl_buffers_on_sparse_memory_block_alloced,
                                                                     _ogl_buffers_on_sparse_memory_block_freed,
                                                                     new_buffer_ptr,
                                                                     false); /* should_be_thread_safe */

        system_resizable_vector_push(new_buffer_ptr->buffers_ptr->sparse_buffers,
                                     new_buffer_ptr);
    }
    else
    {
        /* BO allocation failure! */
        ASSERT_DEBUG_SYNC(false,
                          "Failed to allocate storage for an immutable buffer object.");

        buffers_ptr->entry_points_gl->pGLDeleteBuffers(1,
                                                      &new_buffer_ptr->bo_id);

        system_resource_pool_return_to_pool(buffers_ptr->buffer_descriptor_pool,
                                            (system_resource_pool_block) new_buffer_ptr);

        new_buffer_ptr = NULL;
    }

    return new_buffer_ptr;
}

/** TODO */
PRIVATE unsigned int _ogl_buffers_get_nonsparse_buffer_size(__in _ogl_buffers_usage usage)
{
    unsigned int result = 0;

    switch (usage)
    {
        /* non-mappable */
        case OGL_BUFFERS_USAGE_IBO:
        {
            result = NONSPARSE_IBO_BUFFER_SIZE;

            break;
        }

        case OGL_BUFFERS_USAGE_UBO:
        {
            result = NONSPARSE_UBO_BUFFER_SIZE;

            break;
        }

        case OGL_BUFFERS_USAGE_VBO:
        {
            result = NONSPARSE_VBO_BUFFER_SIZE;

            break;
        }

        case OGL_BUFFERS_USAGE_MISCELLANEOUS:
        {
            result = NONSPARSE_MISC_BUFFER_SIZE;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_buffers_usage value.");
        }
    } /* switch (usage) */

    return result;
}

/** TODO */
PRIVATE void _ogl_buffers_on_sparse_memory_block_alloced(__in __notnull system_memory_manager manager,
                                                         __in           unsigned int          offset_aligned,
                                                         __in           unsigned int          size,
                                                         __in __notnull void*                 user_arg)
{
    _ogl_buffers_buffer* buffer_ptr = (_ogl_buffers_buffer*) user_arg;

    /* Commit the pages the caller will later fill with data. */
    ASSERT_DEBUG_SYNC(buffer_ptr->buffers_ptr->entry_points_gl_sb != NULL,
                      "GL_ARB_sparse_buffer entry-points are unavailable.");

    buffer_ptr->buffers_ptr->entry_points_gl_sb->pGLNamedBufferPageCommitmentEXT(buffer_ptr->bo_id,
                                                                                 offset_aligned,
                                                                                 size,
                                                                                 GL_TRUE); /* commit */
}

/** TODO */
PRIVATE void _ogl_buffers_on_sparse_memory_block_freed(__in __notnull system_memory_manager manager,
                                                       __in           unsigned int          offset_aligned,
                                                       __in           unsigned int          size,
                                                       __in __notnull void*                 user_arg)
{
    _ogl_buffers_buffer* buffer_ptr = (_ogl_buffers_buffer*) user_arg;

    /* Release the pages we no longer need.
     *
     * Note: system_memory_manager ensures the pages that we are told to release no longer hold
     *       any other data, so we need not worry about damaging data owned by other allocations.
     */
    ASSERT_DEBUG_SYNC(buffer_ptr->buffers_ptr->entry_points_gl_sb != NULL,
                      "GL_ARB_sparse_buffer entry-points are unavailable.");

    buffer_ptr->buffers_ptr->entry_points_gl_sb->pGLNamedBufferPageCommitmentEXT(buffer_ptr->bo_id,
                                                                                 offset_aligned,
                                                                                 size,
                                                                                 GL_FALSE); /* commit */
}


/** Please see header for spec */
PUBLIC EMERALD_API bool ogl_buffers_allocate_buffer_memory(__in  __notnull ogl_buffers              buffers,
                                                           __in            unsigned int             size,
                                                           __in            unsigned int             alignment_requirement,
                                                           __in            _ogl_buffers_mappability mappability,
                                                           __in            _ogl_buffers_usage       usage,
                                                           __in            int                      flags, /* bitfield of OGL_BUFFERS_FLAGS_ */
                                                           __out __notnull unsigned int*            out_bo_id_ptr,
                                                           __out __notnull unsigned int*            out_bo_offset_ptr,
                                                           __out_opt       GLvoid**                 out_mapped_gl_ptr)
{
    _ogl_buffers_buffer* buffer_ptr                 = NULL;
    _ogl_buffers*        buffers_ptr                = (_ogl_buffers*) buffers;
    const bool           must_be_immutable_bo_based = (flags & OGL_BUFFERS_FLAGS_IMMUTABLE_BUFFER_MEMORY_BIT) != 0;
    bool                 result                     = false;
    unsigned int         result_id                  = 0;
    unsigned int         result_offset              = -1;
    GLvoid*              result_mapped_gl_ptr       = NULL;

    /* Check the sparse buffers first. */
    bool is_mappability_compatible_with_sparse_buffers = (mappability == OGL_BUFFERS_MAPPABILITY_NONE);

    if ( buffers_ptr->are_sparse_buffers_in            &&
        !must_be_immutable_bo_based                    &&
         is_mappability_compatible_with_sparse_buffers)
    {
        /* Try to find a suitable sparse buffer */
        const unsigned int n_sparse_buffers = system_resizable_vector_get_amount_of_elements(buffers_ptr->sparse_buffers);

        for (unsigned int n_sparse_buffer = 0;
                          n_sparse_buffer < n_sparse_buffers && !result;
                        ++n_sparse_buffer)
        {
            if (system_resizable_vector_get_element_at(buffers_ptr->sparse_buffers,
                                                       n_sparse_buffer,
                                                      &buffer_ptr) )
            {
                ASSERT_DEBUG_SYNC(buffer_ptr->mappability == OGL_BUFFERS_MAPPABILITY_NONE,
                                  "Sanity check failed");

                result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                           size,
                                                           alignment_requirement,
                                                          &result_offset);
            } /* if (sparse descriptor found) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve sparse buffer descriptor");
            }
        } /* for (all sparse buffers) */

        /* No free sparse buffer available? Create a new one IF the requested size does not
         * exceed the sparse buffer size for the requested buffer usage type. */
        if (!result)
        {
            if (size < SPARSE_BUFFER_SIZE)
            {
                buffer_ptr = _ogl_buffers_alloc_new_sparse_buffer(buffers_ptr);

                if (buffer_ptr != NULL)
                {
                    result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                               size,
                                                               alignment_requirement,
                                                              &result_offset);

                    if (!result)
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Memory block allocation failed in a brand new sparse buffer.");
                    }
                } /* if (buffer_ptr != NULL) */
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Sparse buffer allocation failed.");
                }
            }/* if (size < SPARSE_BUFFER_SIZE) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Requested buffer size is larger than the permitted sparse buffer size! "
                                  "Falling back to immutable buffer objects");
            }
        } /* if (!result) */
    } /* if (can use a sparse buffer) */

    if (!result)
    {
        /* Iterate over all non-sparse buffers we have cached so far. */
        ASSERT_DEBUG_SYNC(usage < OGL_BUFFERS_USAGE_COUNT,
                          "Invalid usage enum supplied.");

        const unsigned int n_nonsparse_buffers = system_resizable_vector_get_amount_of_elements(buffers_ptr->nonsparse_buffers[usage]);

        for (unsigned int n_nonsparse_buffer = 0;
                          n_nonsparse_buffer < n_nonsparse_buffers && !result;
                        ++n_nonsparse_buffer)
        {
            if (system_resizable_vector_get_element_at(buffers_ptr->nonsparse_buffers[usage],
                                                       n_nonsparse_buffer,
                                                      &buffer_ptr) )
            {
                /* Try to allocate the requested memory region */
                result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                           size,
                                                           alignment_requirement,
                                                          &result_offset);
            } /* if (non-sparse descriptor was found) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve a non-sparse buffer descriptor at index [%d]",
                                  n_nonsparse_buffer);
            }
        } /* for (all non-sparse buffers defined) */

        /* No suitable BO found? Try instantiating a new one */
        if (!result)
        {
            buffer_ptr = _ogl_buffers_alloc_new_immutable_buffer(buffers_ptr,
                                                                 mappability,
                                                                 usage,
                                                                 size);

            if (buffer_ptr != NULL)
            {
                result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                           size,
                                                           alignment_requirement,
                                                          &result_offset);
            } /* if (buffer_ptr != NULL) */
        } /* if (!result) */
    } /* if (!result) */

    if (result)
    {
        result_id            = buffer_ptr->bo_id;
        result_mapped_gl_ptr = buffer_ptr->mapped_gl_ptr;
    } /* if (result) */
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_buffers_allocate_buffer_memory() failed.");
    }

    LOG_ERROR("ogl_buffers_allocate_buffer_memory(): size:[%d] alignment_requirement:[%d] => BO id:[%d] offset:[%d] mapped GL ptr:[%x]",
              size,
              alignment_requirement,
              result_id,
              result_offset,
              result_mapped_gl_ptr);

    if (result)
    {
        ASSERT_DEBUG_SYNC(result_id     != 0 &&
                          result_offset != -1,
                          "Sanity check failed");

        *out_bo_id_ptr     = result_id;
        *out_bo_offset_ptr = result_offset;

        if (out_mapped_gl_ptr != NULL)
        {
            *out_mapped_gl_ptr = result_mapped_gl_ptr;
        }

        system_hash64map_insert(buffers_ptr->bo_id_offset_hash_to_buffer_map,
                                MAKE_BO_HASHMAP_KEY(result_id,
                                                    result_offset),
                                buffer_ptr,
                                NULL,       /* on_remove_callback */
                                NULL);      /* on_remove_callback_user_arg */
    }

    return result;
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
            for (unsigned int n_buffer_usage = 0;
                              n_buffer_usage < OGL_BUFFERS_USAGE_COUNT;
                            ++n_buffer_usage)
            {
                _ogl_buffers_mappability mappability = (n_buffer_usage == OGL_BUFFERS_USAGE_MISCELLANEOUS) ? OGL_BUFFERS_MAPPABILITY_READ_AND_WRITE_OPS
                                                                                                           : OGL_BUFFERS_MAPPABILITY_NONE;

                 _ogl_buffers_alloc_new_immutable_buffer(new_buffers,
                                                         mappability,
                                                         (_ogl_buffers_usage) n_buffer_usage,
                                                         _ogl_buffers_get_nonsparse_buffer_size( (_ogl_buffers_usage) n_buffer_usage) );
            }
        }
    } /* if (new_buffers != NULL) */

    return (ogl_buffers) new_buffers;
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_buffers_free_buffer_memory(__in __notnull ogl_buffers  buffers,
                                                       __in           unsigned int bo_id,
                                                       __in           unsigned int bo_offset)
{
    _ogl_buffers_buffer* buffer_ptr  = NULL;
    _ogl_buffers*        buffers_ptr = (_ogl_buffers*) buffers;
    const system_hash64  bo_hash     = MAKE_BO_HASHMAP_KEY(bo_id, bo_offset);

    LOG_ERROR("ogl_buffers_free_buffer_memory(): BO id:[%d] offset:[%d]",
              bo_id,
              bo_offset);

    if (system_hash64map_get(buffers_ptr->bo_id_offset_hash_to_buffer_map,
                             bo_hash,
                            &buffer_ptr) )
    {
        system_memory_manager_free_block(buffer_ptr->manager,
                                         bo_offset);

        system_hash64map_remove(buffers_ptr->bo_id_offset_hash_to_buffer_map,
                                bo_hash);
    } /* if (allocation descriptor was found) */
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified memory block (BO id:[%d] offset:[%d]) could not have been freed.",
                          bo_id,
                          bo_offset);
    }

}

/** Please see header for spec */
PUBLIC void ogl_buffers_release(__in __notnull ogl_buffers buffers)
{
    delete (_ogl_buffers*) buffers;

    buffers = NULL;
}

