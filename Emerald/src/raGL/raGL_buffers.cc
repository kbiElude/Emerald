/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_types.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_buffers.h"
#include "raGL/raGL_sync.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"

#define NONSPARSE_IBO_BUFFER_SIZE  (16  * 1024768)
#define NONSPARSE_MISC_BUFFER_SIZE (16  * 1024768)
#define NONSPARSE_UBO_BUFFER_SIZE  (4   * 1024768)
#define NONSPARSE_VBO_BUFFER_SIZE  (64  * 1024768)
#define SPARSE_BUFFER_SIZE         (512 * 1024768)


typedef enum
{
    RAGL_BUFFERS_HEAP_IBO,
    RAGL_BUFFERS_HEAP_MISCELLANEOUS,
    RAGL_BUFFERS_HEAP_UBO,
    RAGL_BUFFERS_HEAP_VBO,

    /* Always last */
    RAGL_BUFFERS_HEAP_COUNT
} _raGL_buffers_heap;


typedef struct _raGL_buffers_buffer
{
    GLuint                      bo_id;
    uint32_t                    bo_size;
    struct _raGL_buffers*       buffers_ptr;
    ral_buffer_mappability_bits mappability;
    system_memory_manager       manager;
} _raGL_buffers_buffer;

typedef struct _raGL_buffers
{
    bool                                           are_sparse_buffers_in;
    raGL_backend                                   backend;
    system_resource_pool                           buffer_descriptor_pool;
    ogl_context                                    context; /* do not retain - else face circular dependencies */
    ral_context                                    context_ral;
    ogl_context_es_entrypoints*                    entry_points_es;
    system_hash64map                               id_to_buffer_map;         /* maps GLuint to _raGL_buffers_buffer instance; does NOT own the instance */
    system_hash64map                               ral_buffer_to_buffer_map; /* maps ral_buffer to _raGL_buffers_buffer instances; owns the instances */

    ogl_context_gl_entrypoints*                    entry_points_gl;
    ogl_context_gl_entrypoints_arb_buffer_storage* entry_points_gl_bs;
    ogl_context_gl_entrypoints_arb_sparse_buffer*  entry_points_gl_sb;

    system_resizable_vector                        nonsparse_heaps[RAGL_BUFFERS_HEAP_COUNT]; /* owns ogl_buffer instances */
    unsigned int                                   page_size;
    system_resizable_vector                        sparse_buffers; /* sparse BOs are never mappable! */

    uint32_t shader_storage_buffer_alignment;
    uint32_t texture_buffer_alignment;
    uint32_t uniform_buffer_alignment;

    REFCOUNT_INSERT_VARIABLES


    _raGL_buffers(raGL_backend in_backend,
                  ral_context  in_context_ral,
                  ogl_context  in_context);

    ~_raGL_buffers()
    {
        if (id_to_buffer_map != nullptr)
        {
            system_hash64map_release(id_to_buffer_map);

            id_to_buffer_map = nullptr;
        }

        if (nonsparse_heaps != nullptr)
        {
            for (unsigned int n_heap = 0;
                              n_heap < RAGL_BUFFERS_HEAP_COUNT;
                            ++n_heap)
            {
                _raGL_buffers_buffer* buffer_ptr = nullptr;

                /* Note: nonsparse buffer descriptors are carved out of the buffer descriptor pool.
                 *       The pool takes care of releasing the embedded GL objects at the release time.
                 *
                 *       Calling a destructor here would be a unforgivable mistake, but we still need
                 *       to return the descriptor to the pool.
                 */
                while (system_resizable_vector_pop(nonsparse_heaps[n_heap],
                                                  &buffer_ptr) )
                {
                    system_resource_pool_return_to_pool(buffer_descriptor_pool,
                                                        (system_resource_pool_block) buffer_ptr);
                }

                system_resizable_vector_release(nonsparse_heaps[n_heap]);

                nonsparse_heaps[n_heap] = nullptr;
            }
        }

        if (ral_buffer_to_buffer_map != nullptr)
        {
            system_hash64map_release(ral_buffer_to_buffer_map);

            ral_buffer_to_buffer_map = nullptr;
        }

        if (sparse_buffers != nullptr)
        {
            _raGL_buffers_buffer* buffer_ptr = nullptr;

            /* The note above regarding the nonsparse buffer descriptors applies here, too. */
            while (system_resizable_vector_pop(sparse_buffers,
                                              &buffer_ptr) )
            {
                system_resource_pool_return_to_pool(buffer_descriptor_pool,
                                                    (system_resource_pool_block) buffer_ptr);
            }

            system_resizable_vector_release(sparse_buffers);

            sparse_buffers = nullptr;
        }

        /* The descriptor pool needs to be released at the end */
        if (buffer_descriptor_pool != nullptr)
        {
            system_resource_pool_release(buffer_descriptor_pool);

            buffer_descriptor_pool = nullptr;
        }
    }
} _raGL_buffers;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(raGL_buffers,
                               raGL_buffers,
                              _raGL_buffers);


/** Forward declarations */
PRIVATE _raGL_buffers_buffer* _raGL_buffers_alloc_new_immutable_buffer        (_raGL_buffers*              buffers_ptr,
                                                                               ral_buffer_mappability_bits mappability,
                                                                               ral_buffer_usage_bits       usage,
                                                                               unsigned int                required_size);
PRIVATE _raGL_buffers_buffer* _raGL_buffers_alloc_new_sparse_buffer           (_raGL_buffers*              buffers_ptr);
PRIVATE void                  _raGL_buffers_dealloc_sparse_buffer             (system_resource_pool_block  buffer_block);
PRIVATE _raGL_buffers_heap    _raGL_buffers_get_heap_for_ral_buffer_usage_bits(ral_buffer_usage_bits       usage);
PRIVATE unsigned int          _raGL_buffers_get_nonsparse_buffer_size         (_raGL_buffers_heap          heap);
PRIVATE void                  _raGL_buffers_on_sparse_memory_block_alloced    (system_memory_manager       manager,
                                                                               unsigned int                offset_aligned,
                                                                               unsigned int                size,
                                                                               void*                       user_arg);
PRIVATE void                  _raGL_buffers_on_sparse_memory_block_freed      (system_memory_manager       manager,
                                                                               unsigned int                offset_aligned,
                                                                               unsigned int                size,
                                                                               void*                       user_arg);
PRIVATE void                  _raGL_buffers_release                            (void*                      buffers);


/** TODO */
_raGL_buffers::_raGL_buffers(raGL_backend in_backend,
                             ral_context  in_context_ral,
                             ogl_context  in_context)
{
    are_sparse_buffers_in    = false;
    backend                  = in_backend;
    buffer_descriptor_pool   = system_resource_pool_create(sizeof(_raGL_buffers_buffer),
                                                           16,                                   /* n_elements_to_preallocate */
                                                           nullptr,                                 /* init_fn */
                                                           _raGL_buffers_dealloc_sparse_buffer); /* deinit_fn */
    context                  = in_context;
    context_ral              = in_context_ral;
    entry_points_es          = nullptr;
    entry_points_gl          = nullptr;
    entry_points_gl_bs       = nullptr;
    entry_points_gl_sb       = nullptr;
    id_to_buffer_map         = system_hash64map_create(sizeof(_raGL_buffers_buffer*) );
    page_size                = 0;
    ral_buffer_to_buffer_map = system_hash64map_create       (sizeof(_raGL_buffers_buffer*) );
    sparse_buffers           = system_resizable_vector_create(16); /* capacity */

    /* Retrieve alignment requirements */
    ral_backend_type             backend_type;
    const ogl_context_gl_limits* limits_ptr = nullptr;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                      "TODO");

    shader_storage_buffer_alignment = limits_ptr->shader_storage_buffer_offset_alignment;
    texture_buffer_alignment        = limits_ptr->texture_buffer_offset_alignment;
    uniform_buffer_alignment        = limits_ptr->uniform_buffer_offset_alignment;

    /* Initialize non-sparse heap vectors */
    for (unsigned int n_heap = 0;
                      n_heap < RAGL_BUFFERS_HEAP_COUNT;
                    ++n_heap)
    {
        nonsparse_heaps[n_heap] = system_resizable_vector_create(4); /* capacity */
    }

    /* Cache the entry-points */
    if (backend_type == RAL_BACKEND_TYPE_ES)
    {
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points_es);
    }
    else
    {
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points_gl);

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_BUFFER_STORAGE,
                                &entry_points_gl_bs);

        /* Are sparse buffers supported? */
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_SPARSE_BUFFERS,
                                &are_sparse_buffers_in);
    }

    if (are_sparse_buffers_in)
    {
        /* Retrieve entry-point & limits descriptors */
        const ogl_context_gl_limits_arb_sparse_buffer* limits_sb = nullptr;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SPARSE_BUFFER,
                                &entry_points_gl_sb);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS_ARB_SPARSE_BUFFER,
                                &limits_sb);

        page_size = limits_sb->sparse_buffer_page_size;
    }
}

/** TODO */
PRIVATE _raGL_buffers_buffer* _raGL_buffers_alloc_new_immutable_buffer(_raGL_buffers*              buffers_ptr,
                                                                       ral_buffer_mappability_bits mappability_bits,
                                                                       ral_buffer_usage_bits       usage,
                                                                       unsigned int                required_size)
{
    _raGL_buffers_heap    heap_index     = _raGL_buffers_get_heap_for_ral_buffer_usage_bits(usage);
    unsigned int          bo_flags       = GL_DYNAMIC_STORAGE_BIT;
    GLuint                bo_id          = 0;
    unsigned int          bo_size        = _raGL_buffers_get_nonsparse_buffer_size(heap_index);
    GLenum                bo_target      = 0;
    _raGL_buffers_buffer* new_buffer_ptr = reinterpret_cast<_raGL_buffers_buffer*>(system_resource_pool_get_from_pool(buffers_ptr->buffer_descriptor_pool));

    /** TODO */
    ASSERT_DEBUG_SYNC( (mappability_bits & RAL_BUFFER_MAPPABILITY_COHERENT_BIT) == 0,
                      "TODO");

    /** TODO: This is a temporary copy-paste of the code used in raGL_buffer. Ultimately, ogl_buffers and ogl_buffer will be merged */
    bo_target = GL_ARRAY_BUFFER;

    if ((usage &  RAL_BUFFER_USAGE_COPY_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_COPY_BIT) == 0)
    {
        bo_target = GL_COPY_READ_BUFFER;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0)
    {
        bo_target = GL_ELEMENT_ARRAY_BUFFER;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_INDIRECT_DISPATCH_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_INDIRECT_DISPATCH_BUFFER_BIT) == 0)
    {
        bo_target = GL_DISPATCH_INDIRECT_BUFFER;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT) == 0)
    {
        bo_target = GL_DRAW_INDIRECT_BUFFER;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT) == 0)
    {
        bo_target = GL_SHADER_STORAGE_BUFFER;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_RO_TEXTURE_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_RO_TEXTURE_BUFFER_BIT) == 0)
    {
        bo_target = GL_TEXTURE_BUFFER;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0)
    {
        bo_target = GL_UNIFORM_BUFFER;
    }
    else
    {
        if ((mappability_bits & RAL_BUFFER_MAPPABILITY_READ_OP_BIT) != 0)
        {
            bo_flags |= GL_MAP_READ_BIT;
        }

        if ((mappability_bits & RAL_BUFFER_MAPPABILITY_WRITE_OP_BIT) != 0)
        {
            bo_flags |= GL_MAP_WRITE_BIT;
        }

        bo_target = GL_ARRAY_BUFFER;
    }

    if (bo_target != GL_ARRAY_BUFFER)
    {
        ASSERT_DEBUG_SYNC(mappability_bits == RAL_BUFFER_MAPPABILITY_NONE,
                          "Invalid mappability setting.");
    }

    /* If the caller demands a larger buffer, scale up the planned BO size */
    if (bo_size < required_size)
    {
        bo_size = required_size;
    }

    if (buffers_ptr->entry_points_gl != nullptr)
    {
        ASSERT_DEBUG_SYNC(buffers_ptr->entry_points_gl_bs != nullptr,
                          "Immutable buffer support is missing. Crash ahead.");

        buffers_ptr->entry_points_gl->pGLGenBuffers      (1,
                                                         &bo_id);
        buffers_ptr->entry_points_gl->pGLBindBuffer      (bo_target,
                                                          bo_id);
        buffers_ptr->entry_points_gl_bs->pGLBufferStorage(bo_target,
                                                          bo_size,
                                                          nullptr,
                                                          bo_flags);
    }
    else
    {
        buffers_ptr->entry_points_es->pGLGenBuffers(1,
                                                   &bo_id);
        buffers_ptr->entry_points_es->pGLBindBuffer(bo_target,
                                                    bo_id);
        buffers_ptr->entry_points_es->pGLBufferData(bo_target,
                                                    bo_size,
                                                    nullptr,           /* data */
                                                    GL_STATIC_DRAW);
    }

    /* Check if the request was successful */
    GLint64 reported_bo_size = 0;

    if (buffers_ptr->entry_points_gl != nullptr)
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
        int heap_type = _raGL_buffers_get_heap_for_ral_buffer_usage_bits(usage);

        new_buffer_ptr->bo_id       = bo_id;
        new_buffer_ptr->bo_size     = bo_size;
        new_buffer_ptr->buffers_ptr = buffers_ptr;
        new_buffer_ptr->mappability = mappability_bits;
        new_buffer_ptr->manager     = system_memory_manager_create(bo_size,
                                                                   1,             /* page_size - immutable buffers have no special requirements */
                                                                   nullptr,          /* pfn_on_memory_block_alloced */
                                                                   nullptr,          /* pfn_on_memory_block_freed */
                                                                   new_buffer_ptr,
                                                                   true);         /* should_be_thread_safe */

        system_resizable_vector_push(new_buffer_ptr->buffers_ptr->nonsparse_heaps[heap_type],
                                     new_buffer_ptr);

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(buffers_ptr->id_to_buffer_map,
                                                     (system_hash64) bo_id),
                          "Buffer info descriptor already stored for BO GL id %d",
                          bo_id);

        system_hash64map_insert(buffers_ptr->id_to_buffer_map,
                                bo_id,
                                new_buffer_ptr,
                                nullptr,  /* callback          */
                                nullptr); /* callback_argument */
    }
    else
    {
        /* BO allocation failure! */
        ASSERT_DEBUG_SYNC(false,
                          "Failed to allocate storage for an immutable buffer object.");

        if (buffers_ptr->entry_points_gl != nullptr)
        {
            buffers_ptr->entry_points_gl->pGLDeleteBuffers(1,
                                                          &bo_id);
        }
        else
        {
            buffers_ptr->entry_points_es->pGLDeleteBuffers(1,
                                                          &bo_id);
        }

        system_resource_pool_return_to_pool(buffers_ptr->buffer_descriptor_pool,
                                            (system_resource_pool_block) new_buffer_ptr);

        new_buffer_ptr = nullptr;
    }

    {
        raGL_backend_enqueue_sync();
    }

    return new_buffer_ptr;
}

/** TODO */
PRIVATE _raGL_buffers_buffer* _raGL_buffers_alloc_new_sparse_buffer(_raGL_buffers* buffers_ptr)
{
    _raGL_buffers_buffer* new_buffer_ptr = reinterpret_cast<_raGL_buffers_buffer*>(system_resource_pool_get_from_pool(buffers_ptr->buffer_descriptor_pool));

    ASSERT_DEBUG_SYNC(buffers_ptr->entry_points_gl    != nullptr &&
                      buffers_ptr->entry_points_gl_bs != nullptr,
                      "GL_ARB_buffer_storage and/or GL entry-points are unavailable.");

    buffers_ptr->entry_points_gl->pGLGenBuffers      (1,
                                                      &new_buffer_ptr->bo_id);
    buffers_ptr->entry_points_gl->pGLBindBuffer      (GL_ARRAY_BUFFER,
                                                      new_buffer_ptr->bo_id);
    buffers_ptr->entry_points_gl_bs->pGLBufferStorage(GL_ARRAY_BUFFER,
                                                      SPARSE_BUFFER_SIZE,
                                                      nullptr, /* data */
                                                      GL_SPARSE_STORAGE_BIT_ARB |
                                                      GL_DYNAMIC_STORAGE_BIT);

    /* Check if the request was successful */
    GLint64 reported_bo_size = 0;

    buffers_ptr->entry_points_gl->pGLGetBufferParameteri64v(GL_ARRAY_BUFFER,
                                                            GL_BUFFER_SIZE,
                                                           &reported_bo_size);

    if (reported_bo_size >= SPARSE_BUFFER_SIZE)
    {
        new_buffer_ptr->bo_size     = (uint32_t) reported_bo_size;
        new_buffer_ptr->buffers_ptr = buffers_ptr;
        new_buffer_ptr->mappability = RAL_BUFFER_MAPPABILITY_NONE;
        new_buffer_ptr->manager     = system_memory_manager_create(SPARSE_BUFFER_SIZE,
                                                                   buffers_ptr->page_size,
                                                                   _raGL_buffers_on_sparse_memory_block_alloced,
                                                                   _raGL_buffers_on_sparse_memory_block_freed,
                                                                   new_buffer_ptr,
                                                                   true); /* should_be_thread_safe */

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

        new_buffer_ptr = nullptr;
    }

    raGL_backend_enqueue_sync();

    return new_buffer_ptr;
}

/** TODO */
PRIVATE void _raGL_buffers_dealloc_sparse_buffer(system_resource_pool_block buffer_block)
{
    _raGL_buffers_buffer* buffer_ptr = reinterpret_cast<_raGL_buffers_buffer*>(buffer_block);

    if (buffer_ptr->bo_id != 0)
    {
        if (buffer_ptr->buffers_ptr->entry_points_gl != nullptr)
        {
            buffer_ptr->buffers_ptr->entry_points_gl->pGLDeleteBuffers(1,
                                                                      &buffer_ptr->bo_id);
        }
        else
        {
            ASSERT_DEBUG_SYNC(buffer_ptr->buffers_ptr->entry_points_es != nullptr,
                              "ES and GL entry-point descriptors are unavailable.");

            buffer_ptr->buffers_ptr->entry_points_es->pGLDeleteBuffers(1,
                                                                      &buffer_ptr->bo_id);
        }

        buffer_ptr->bo_id = 0;
    }

    if (buffer_ptr->manager != nullptr)
    {
        system_memory_manager_release(buffer_ptr->manager);

        buffer_ptr->manager = nullptr;
    }

    raGL_backend_enqueue_sync();
}

/** TODO */
PRIVATE _raGL_buffers_heap _raGL_buffers_get_heap_for_ral_buffer_usage_bits(ral_buffer_usage_bits usage)
{
    _raGL_buffers_heap result = RAGL_BUFFERS_HEAP_MISCELLANEOUS;

    if ((usage & RAL_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
    {
        result = RAGL_BUFFERS_HEAP_IBO;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
    {
        result = RAGL_BUFFERS_HEAP_VBO;
    }
    else
    if ((usage &  RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0 &&
        (usage & ~RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
    {
        result = RAGL_BUFFERS_HEAP_VBO;
    }
    else
    {
        result = RAGL_BUFFERS_HEAP_MISCELLANEOUS;
    }

    return result;
}

/** TODO */
PRIVATE unsigned int _raGL_buffers_get_nonsparse_buffer_size(_raGL_buffers_heap heap)
{
    unsigned int result = 0;

    switch (heap)
    {
        /* non-mappable */
        case RAGL_BUFFERS_HEAP_IBO:
        {
            result = NONSPARSE_IBO_BUFFER_SIZE;

            break;
        }

        case RAGL_BUFFERS_HEAP_UBO:
        {
            result = NONSPARSE_UBO_BUFFER_SIZE;

            break;
        }

        case RAGL_BUFFERS_HEAP_VBO:
        {
            result = NONSPARSE_VBO_BUFFER_SIZE;

            break;
        }

        case RAGL_BUFFERS_HEAP_MISCELLANEOUS:
        {
            result = NONSPARSE_MISC_BUFFER_SIZE;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _raGL_buffers_heap value.");
        }
    }

    return result;
}

/** TODO */
PRIVATE ral_present_job _raGL_buffers_init_rendering_callback(ral_context                                                context,
                                                              void*                                                      buffers_raw_ptr,
                                                              const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_buffers* buffers_ptr = reinterpret_cast<_raGL_buffers*>(buffers_raw_ptr);

    /* If sparse buffers are supported, allocate a single sparse buffer */
    if (buffers_ptr->are_sparse_buffers_in)
    {
        _raGL_buffers_alloc_new_sparse_buffer(buffers_ptr);
    }
    else
    {
        for (unsigned int n_heap = 0;
                          n_heap < RAGL_BUFFERS_HEAP_COUNT;
                        ++n_heap)
        {
            ral_buffer_mappability_bits mappability = (n_heap == RAGL_BUFFERS_HEAP_MISCELLANEOUS) ? (RAL_BUFFER_MAPPABILITY_READ_OP_BIT | RAL_BUFFER_MAPPABILITY_WRITE_OP_BIT)
                                                                                                  :  RAL_BUFFER_MAPPABILITY_NONE;

            _raGL_buffers_alloc_new_immutable_buffer(buffers_ptr,
                                                     mappability,
                                                     (n_heap == RAGL_BUFFERS_HEAP_IBO) ? RAL_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                   : (n_heap == RAGL_BUFFERS_HEAP_UBO) ? RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                                   : (n_heap == RAGL_BUFFERS_HEAP_VBO) ? RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                                                       : 0xFFFFFFFF,
                                                     _raGL_buffers_get_nonsparse_buffer_size( (_raGL_buffers_heap) n_heap) );
        }
    }

    /* We issue GL calls directly from this entrypoint, so it's OK to return a null present job */
    return nullptr;
}

/** TODO */
PRIVATE void _raGL_buffers_on_sparse_memory_block_alloced(system_memory_manager manager,
                                                          unsigned int          offset_aligned,
                                                          unsigned int          size,
                                                          void*                 user_arg)
{
    _raGL_buffers_buffer* buffer_ptr = reinterpret_cast<_raGL_buffers_buffer*>(user_arg);

    /* Commit the pages the caller will later fill with data. */
    ASSERT_DEBUG_SYNC(buffer_ptr->buffers_ptr->entry_points_gl_sb != nullptr,
                      "GL_ARB_sparse_buffer entry-points are unavailable.");

    buffer_ptr->buffers_ptr->entry_points_gl_sb->pGLNamedBufferPageCommitmentEXT(buffer_ptr->bo_id,
                                                                                 offset_aligned,
                                                                                 size,
                                                                                 GL_TRUE); /* commit */

    raGL_backend_enqueue_sync();
}

/** TODO */
PRIVATE void _raGL_buffers_on_sparse_memory_block_freed(system_memory_manager manager,
                                                        unsigned int          offset_aligned,
                                                        unsigned int          size,
                                                        void*                 user_arg)
{
    _raGL_buffers_buffer* buffer_ptr = reinterpret_cast<_raGL_buffers_buffer*>(user_arg);

    /* Release the pages we no longer need.
     *
     * Note: system_memory_manager ensures the pages that we are told to release no longer hold
     *       any other data, so we need not worry about damaging data owned by other allocations.
     */
    ASSERT_DEBUG_SYNC(buffer_ptr->buffers_ptr->entry_points_gl_sb != nullptr,
                      "GL_ARB_sparse_buffer entry-points are unavailable.");

    buffer_ptr->buffers_ptr->entry_points_gl_sb->pGLNamedBufferPageCommitmentEXT(buffer_ptr->bo_id,
                                                                                 offset_aligned,
                                                                                 size,
                                                                                 GL_FALSE); /* commit */

    raGL_backend_enqueue_sync();
}

/** TODO */
PRIVATE void _raGL_buffers_release(void* buffers)
{
    /* Stub. The reference counter impl will call the actual destructor */
}

/** Please see header for spec */
PUBLIC bool raGL_buffers_allocate_buffer_memory_for_ral_buffer(raGL_buffers in_raGL_buffers,
                                                               ral_buffer   in_ral_buffer,
                                                               raGL_buffer* out_buffer_ptr)
{
    uint32_t                    alignment_requirement      = 1;
    ral_buffer_mappability_bits buffer_mappability         = 0;
    ral_buffer                  buffer_parent              = nullptr;
    raGL_buffer                 buffer_parent_raGL         = nullptr;
    ral_buffer_property_bits    buffer_properties          = 0;
    raGL_buffer                 buffer_raGL                = nullptr;
    uint32_t                    buffer_size                = 0;
    uint32_t                    buffer_start_offset        = -1;
    ral_buffer_usage_bits       buffer_usage               = 0;
    _raGL_buffers_buffer*       buffer_ptr                 = nullptr;
    _raGL_buffers*              buffers_ptr                = reinterpret_cast<_raGL_buffers*>(in_raGL_buffers);
    bool                        is_sparse                  = false;
    bool                        must_be_immutable_bo_based = false;
    bool                        result                     = false;
    unsigned int                result_id                  = 0;
    unsigned int                result_offset              = -1;

    /* Extract properties from the input RAL buffer */
    ral_buffer_get_property(in_ral_buffer,
                            RAL_BUFFER_PROPERTY_MAPPABILITY_BITS,
                           &buffer_mappability);
    ral_buffer_get_property(in_ral_buffer,
                            RAL_BUFFER_PROPERTY_PROPERTY_BITS,
                           &buffer_properties);
    ral_buffer_get_property(in_ral_buffer,
                            RAL_BUFFER_PROPERTY_PARENT_BUFFER,
                           &buffer_parent);
    ral_buffer_get_property(in_ral_buffer,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &buffer_size);
    ral_buffer_get_property(in_ral_buffer,
                            RAL_BUFFER_PROPERTY_START_OFFSET,
                           &buffer_start_offset);
    ral_buffer_get_property(in_ral_buffer,
                            RAL_BUFFER_PROPERTY_USAGE_BITS,
                           &buffer_usage);

    must_be_immutable_bo_based = (buffer_properties & RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT) == 0;

    /* Sync with other contexts before continuing */
    raGL_backend_sync();

    /* If this raGL_buffer instance is being created with aliasing the existing buffer memory storage in mind,
     * do not allocate any memory. */
    if (buffer_parent != nullptr)
    {
        raGL_buffer result_buffer_raGL = nullptr;

        if (!raGL_backend_get_buffer(buffers_ptr->backend,
                                     buffer_parent,
                           (void**) &buffer_parent_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find a raGL_buffer instance for the parent ral_buffer instance.");

            goto end;
        }

        if (buffer_size == 0)
        {
            uint32_t buffer_parent_size = 0;

            /* In this case, the subregion should use all available buffer memory space. */
            ral_buffer_get_property(buffer_parent,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &buffer_parent_size);

            ASSERT_DEBUG_SYNC(buffer_parent_size > buffer_start_offset,
                              "Requested subregion's start offset exceeds parent buffer's size");

            buffer_size = buffer_parent_size - buffer_start_offset;
        }

        result_buffer_raGL = raGL_buffer_create_raGL_buffer_subregion(buffer_parent_raGL,
                                                                      buffer_start_offset,
                                                                      buffer_size);

        if (result_buffer_raGL == nullptr)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to create a new raGL_buffer subregion instance");

            goto end;
        }

        *out_buffer_ptr = result_buffer_raGL;
        result          = true;

        goto end;
    }

    /* Determine the alignment requirement */
    if ((buffer_usage & RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT) != 0)
    {
        alignment_requirement = buffers_ptr->shader_storage_buffer_alignment;
    }

    if ((buffer_usage & RAL_BUFFER_USAGE_RO_TEXTURE_BUFFER_BIT) != 0)
    {
        alignment_requirement = system_math_other_lcm(alignment_requirement,
                                                      buffers_ptr->texture_buffer_alignment);
    }

    if ((buffer_usage & RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
    {
        alignment_requirement = system_math_other_lcm(alignment_requirement,
                                                      buffers_ptr->uniform_buffer_alignment);
    }

    /* Check the sparse buffers first, if these are supported and no mappability
     * was requested. */
    if (buffers_ptr->are_sparse_buffers_in                                &&
       !must_be_immutable_bo_based                                        &&
        buffer_mappability                 == RAL_BUFFER_MAPPABILITY_NONE)
    {
        /* Try to find a suitable sparse buffer */
        unsigned int n_sparse_buffers = 0;

        system_resizable_vector_get_property(buffers_ptr->sparse_buffers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_sparse_buffers);

        for (unsigned int n_sparse_buffer = 0;
                          n_sparse_buffer < n_sparse_buffers && !result;
                        ++n_sparse_buffer)
        {
            if (system_resizable_vector_get_element_at(buffers_ptr->sparse_buffers,
                                                       n_sparse_buffer,
                                                      &buffer_ptr) )
            {
                ASSERT_DEBUG_SYNC(buffer_ptr->mappability == RAL_BUFFER_MAPPABILITY_NONE,
                                  "Sanity check failed");

                result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                           buffer_size,
                                                           alignment_requirement,
                                                          &result_offset);

                if (result)
                {
                    is_sparse = true;
                    result_id = buffer_ptr->bo_id;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve sparse buffer descriptor");
            }
        }

        /* No free sparse buffer available? Create a new one IF the requested size does not
         * exceed the sparse buffer size for the requested buffer usage type. */
        if (!result)
        {
            if (buffer_size < SPARSE_BUFFER_SIZE)
            {
                buffer_ptr = _raGL_buffers_alloc_new_sparse_buffer(buffers_ptr);

                if (buffer_ptr != nullptr)
                {
                    result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                               buffer_size,
                                                               alignment_requirement,
                                                              &result_offset);

                    if (result)
                    {
                        is_sparse = true;
                        result_id = buffer_ptr->bo_id;
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Memory block allocation failed in a brand new sparse buffer.");
                    }
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Sparse buffer allocation failed.");
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Requested buffer size is larger than the permitted sparse buffer size! "
                                  "Falling back to immutable buffer objects");
            }
        }
    }

    if (!result)
    {
        /* Iterate over all non-sparse heap buffers we have cached so far. */
        int          heap_index          = _raGL_buffers_get_heap_for_ral_buffer_usage_bits(buffer_usage);
        unsigned int n_nonsparse_buffers = 0;

        system_resizable_vector_get_property(buffers_ptr->nonsparse_heaps[heap_index],
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_nonsparse_buffers);

        for (unsigned int n_nonsparse_buffer = 0;
                          n_nonsparse_buffer < n_nonsparse_buffers && !result;
                        ++n_nonsparse_buffer)
        {
            if (system_resizable_vector_get_element_at(buffers_ptr->nonsparse_heaps[heap_index],
                                                       n_nonsparse_buffer,
                                                      &buffer_ptr) )
            {
                /* Try to allocate the requested memory region */
                result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                           buffer_size,
                                                           alignment_requirement,
                                                          &result_offset);

                if (result)
                {
                    result_id = buffer_ptr->bo_id;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve a non-sparse buffer descriptor at index [%d]",
                                  n_nonsparse_buffer);
            }
        }

        /* No suitable BO found? Try instantiating a new one */
        if (!result)
        {
            buffer_ptr = _raGL_buffers_alloc_new_immutable_buffer(buffers_ptr,
                                                                  buffer_mappability,
                                                                  buffer_usage,
                                                                  buffer_size);

            if (buffer_ptr != nullptr)
            {
                result = system_memory_manager_alloc_block(buffer_ptr->manager,
                                                           buffer_size,
                                                           alignment_requirement,
                                                          &result_offset);

                if (result)
                {
                    result_id = buffer_ptr->bo_id;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not allocate a new immutable buffer");
                }
            }
        }
    }

    LOG_ERROR("raGL_buffers_allocate_buffer_memory(): size:[%u] alignment_requirement:[%u] => BO id:[%u] offset:[%u]",
              buffer_size,
              alignment_requirement,
              result_id,
              result_offset);

    if (result)
    {
        ASSERT_DEBUG_SYNC(result_id     != 0 &&
                          result_offset != -1,
                          "Sanity check failed");

        *out_buffer_ptr = raGL_buffer_create(buffers_ptr->context,
                                             result_id,
                                             buffer_ptr->manager,
                                             result_offset,
                                             buffer_size);

        ASSERT_ALWAYS_SYNC(*out_buffer_ptr != nullptr,
                           "Out of memory");
    }

end:
    return result;
}

/** Please see header for spec */
PUBLIC bool raGL_buffers_allocate_buffer_memory_for_ral_buffer_create_info(raGL_buffers                  buffers,
                                                                           const ral_buffer_create_info* buffer_create_info_ptr,
                                                                           raGL_buffer*                  out_buffer_ptr)
{
    _raGL_buffers* buffers_ptr = reinterpret_cast<_raGL_buffers*>(buffers);
    bool           result      = false;
    ral_buffer     temp_buffer = nullptr;

    temp_buffer = ral_buffer_create(buffers_ptr->context_ral,
                                    system_hashed_ansi_string_create("Temporary RAL buffer"),
                                    buffer_create_info_ptr);

    result = raGL_buffers_allocate_buffer_memory_for_ral_buffer(buffers,
                                                                temp_buffer,
                                                                out_buffer_ptr);

    ral_buffer_release(temp_buffer);

    return result;
}

/** Please see header for spec */
PUBLIC raGL_buffers raGL_buffers_create(raGL_backend backend,
                                        ral_context  context_ral,
                                        ogl_context  context)
{
    _raGL_buffers* new_buffers = new (std::nothrow) _raGL_buffers(backend,
                                                                  context_ral,
                                                                  context);

    ASSERT_DEBUG_SYNC(new_buffers != nullptr,
                      "Out of memory");

    if (new_buffers != nullptr)
    {
        ral_rendering_handler rendering_handler = nullptr;

        ral_context_get_property(context_ral,
                                 RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                &rendering_handler);

        ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                         _raGL_buffers_init_rendering_callback,
                                                         new_buffers,
                                                         false); /* present_after_executed */

        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_buffers,
                                                       _raGL_buffers_release,
                                                       OBJECT_TYPE_RAGL_BUFFERS,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\raGL Buffers\\",
                                                                                                               "Context-wide GL buffer object manager") );
    }

    return (raGL_buffers) new_buffers;
}

/** Please see header for spec */
PUBLIC void raGL_buffers_free_buffer_memory(raGL_buffers buffers,
                                            raGL_buffer  buffer)
{
    GLuint                bo_id             = -1;
    system_memory_manager bo_memory_manager = nullptr;
    uint32_t              bo_start_offset   = 0;
    _raGL_buffers*        buffers_ptr       = reinterpret_cast<_raGL_buffers*>(buffers);

    raGL_buffer_get_property(buffer,
                             RAGL_BUFFER_PROPERTY_ID,
                            &bo_id);
    raGL_buffer_get_property(buffer,
                             RAGL_BUFFER_PROPERTY_MEMORY_MANAGER,
                            &bo_memory_manager);
    raGL_buffer_get_property(buffer,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &bo_start_offset);

    LOG_ERROR("raGL_buffers_free_buffer_memory(): BO id:[%u] offset:[%u]",
              bo_id,
              bo_start_offset);

    system_memory_manager_free_block(bo_memory_manager,
                                     bo_start_offset);

    raGL_buffer_release(buffer);
    buffer = nullptr;
}

/** Please see header for spec */
PUBLIC void raGL_buffers_get_buffer_property(const raGL_buffers           buffers,
                                             GLuint                       bo_id,
                                             raGL_buffers_buffer_property property,
                                             void*                        out_result_ptr)
{
    _raGL_buffers_buffer* buffer_ptr  = nullptr;
    _raGL_buffers*        buffers_ptr = reinterpret_cast<_raGL_buffers*>(buffers);

    if (buffers_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(buffers_ptr != nullptr,
                          "Input raGL_buffers instance is nullptr");

        goto end;
    }

    if (!system_hash64map_get(buffers_ptr->id_to_buffer_map,
                              bo_id,
                             &buffer_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "GL BO id [%d] was not recognized.",
                          bo_id);

        goto end;
    }

    switch (property)
    {
        case RAGL_BUFFERS_BUFFER_PROPERTY_SIZE:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = buffer_ptr->bo_size;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_buffers_buffer_property value specified.");
        }
    }
end:
    ;
}