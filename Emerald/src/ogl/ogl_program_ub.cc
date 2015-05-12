/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO: Double matrix uniform support.
 * TODO: Improved synchronization impl.
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"

#define DIRTY_OFFSET_UNUSED (-1)


/** Internal types */
typedef struct _ogl_program_ub
{
    ogl_buffers  buffers; /* NOT retained */
    ogl_context  context; /* NOT retained */
    unsigned int index;
    ogl_program  owner;

    /* block_data holds a local cache of uniform data. This helps us avoid all the complex logic
     * that would otherwise have to be used to store uniform values AND is pretty cache-friendly. */
    GLuint         block_bo_id;
    unsigned int   block_bo_start_offset;
    unsigned char* block_data;
    GLint          block_data_size;
    GLuint         indexed_ub_bp;

    system_hashed_ansi_string name;
    system_resizable_vector   members;                          /* pointers to const ogl_program_uniform_descriptor*. These are owned by owner - DO NOT release. */
    system_hash64map          offset_to_uniform_descriptor_map; /* uniform offset -> const ogl_program_uniform_descriptor*. Rules as above apply. */
    bool                      syncable;

    PFNGLBINDBUFFERPROC              pGLBindBuffer;
    PFNGLBUFFERSUBDATAPROC           pGLBufferSubData;
    PFNGLGETPROGRAMRESOURCEIVPROC    pGLGetProgramResourceiv;
    PFNGLNAMEDBUFFERSUBDATAEXTPROC   pGLNamedBufferSubDataEXT;
    PFNGLUNIFORMBLOCKBINDINGPROC     pGLUniformBlockBinding;

    /* Delimits a UB region which needs to be re-uploaded to the GPU upon next
     * synchronisation request.
     *
     * TODO: We could actually approach this task in a more intelligent manner:
     *
     * 1) (easier)   Issuing multiple GL update calls for disjoint memory regions.
     * 2) (trickier) Caching disjoint region data into a single temporary BO,
     *               uploading its contents just once, and then using gl*Sub*() calls
     *               to update relevant target buffer regions for ultra-fast performance.
     *
     * Sticking to brute-force implementation for testing purposes, for the time being.
     */
    unsigned int dirty_offset_end;
    unsigned int dirty_offset_start;

    explicit _ogl_program_ub(__in __notnull ogl_context               in_context,
                             __in __notnull ogl_program               in_owner,
                             __in __notnull unsigned int              in_index,
                             __in __notnull system_hashed_ansi_string in_name,
                             __in           bool                      in_syncable)
    {
        block_bo_id                      = 0;
        block_bo_start_offset            = -1;
        block_data                       = NULL;
        block_data_size                  = 0;
        buffers                          = NULL;
        context                          = in_context;
        dirty_offset_end                 = DIRTY_OFFSET_UNUSED;
        dirty_offset_start               = DIRTY_OFFSET_UNUSED;
        index                            = in_index;
        indexed_ub_bp                    = 0; /* default GL state value */
        name                             = in_name;
        members                          = system_resizable_vector_create(4, /* capacity */
                                                                          sizeof(ogl_program_uniform_descriptor*) );
        offset_to_uniform_descriptor_map = system_hash64map_create       (sizeof(ogl_program_uniform_descriptor*) );
        owner                            = in_owner;
        pGLBufferSubData                 = NULL;
        pGLGetProgramResourceiv          = NULL;
        pGLNamedBufferSubDataEXT         = NULL;
        pGLUniformBlockBinding           = NULL;
        syncable                         = in_syncable;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &buffers);

        ASSERT_DEBUG_SYNC(members                          != NULL &&
                          offset_to_uniform_descriptor_map != NULL,
                          "Out of memory");
    }

    ~_ogl_program_ub()
    {
        if (block_bo_id != NULL)
        {
            ogl_buffers_free_buffer_memory(buffers,
                                           block_bo_id,
                                           block_bo_start_offset);

            block_bo_id           = 0;
            block_bo_start_offset = -1;
        }

        if (block_data != NULL)
        {
            delete [] block_data;

            block_data = NULL;
        }

        if (members != NULL)
        {
            /* No need to release items - these are owned by ogl_program! */
            system_resizable_vector_release(members);

            members = NULL;
        }

        if (offset_to_uniform_descriptor_map != NULL)
        {
            /* No need to release the items, too */
            system_hash64map_release(offset_to_uniform_descriptor_map);

            offset_to_uniform_descriptor_map = NULL;
        }
    }
} _ogl_program_ub;


/** TODO */
PRIVATE unsigned int _ogl_program_ub_get_expected_src_data_size(__in __notnull const ogl_program_uniform_descriptor* uniform_ptr,
                                                                __in           unsigned int                          n_array_items)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        case PROGRAM_UNIFORM_TYPE_FLOAT:             result = sizeof(float)         * 1;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_VEC2:        result = sizeof(float)         * 2;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_VEC3:        result = sizeof(float)         * 3;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_VEC4:        result = sizeof(float)         * 4;  break;
        case PROGRAM_UNIFORM_TYPE_INT:               result = sizeof(int)           * 1;  break;
        case PROGRAM_UNIFORM_TYPE_INT_VEC2:          result = sizeof(int)           * 2;  break;
        case PROGRAM_UNIFORM_TYPE_INT_VEC3:          result = sizeof(int)           * 3;  break;
        case PROGRAM_UNIFORM_TYPE_INT_VEC4:          result = sizeof(int)           * 4;  break;
        case PROGRAM_UNIFORM_TYPE_UNSIGNED_INT:      result = sizeof(int)           * 1;  break;
        case PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_VEC2: result = sizeof(int)           * 2;  break;
        case PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_VEC3: result = sizeof(int)           * 3;  break;
        case PROGRAM_UNIFORM_TYPE_UNSIGNED_INT_VEC4: result = sizeof(int)           * 4;  break;
        case PROGRAM_UNIFORM_TYPE_BOOL:              result = sizeof(unsigned char) * 1;  break;
        case PROGRAM_UNIFORM_TYPE_BOOL_VEC2:         result = sizeof(unsigned char) * 2;  break;
        case PROGRAM_UNIFORM_TYPE_BOOL_VEC3:         result = sizeof(unsigned char) * 3;  break;
        case PROGRAM_UNIFORM_TYPE_BOOL_VEC4:         result = sizeof(unsigned char) * 4;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2:        result = sizeof(float)         * 4;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3:        result = sizeof(float)         * 9;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4:        result = sizeof(float)         * 16; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x3:      result = sizeof(float)         * 6;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x4:      result = sizeof(float)         * 8;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x2:      result = sizeof(float)         * 6;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x4:      result = sizeof(float)         * 12; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x2:      result = sizeof(float)         * 8;  break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x3:      result = sizeof(float)         * 12; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized uniform type [%d]",
                              uniform_ptr->type);
        }
    } /* switch (uniform_ptr->type) */

    result *= n_array_items;

    /* All done */
    return result;
}

/** TODO */
PRIVATE unsigned int _ogl_program_ub_get_memcpy_friendly_matrix_stride(__in __notnull const ogl_program_uniform_descriptor* uniform_ptr)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        /* Square matrices */
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2: result = sizeof(float) * 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3: result = sizeof(float) * 3; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4: result = sizeof(float) * 4; break;

        /* Non-square matrices */
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x3:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 3
                                                        : sizeof(float) * 2;

            break;
        }

        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x4:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 4
                                                        : sizeof(float) * 2;

            break;
        }

        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x2:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 2
                                                        : sizeof(float) * 3;

            break;
        }

        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x4:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 4
                                                        : sizeof(float) * 3;

            break;
        }

        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x2:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 2
                                                        : sizeof(float) * 4;

            break;
        }

        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x3:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 3
                                                        : sizeof(float) * 4;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized uniform type [%d]",
                              uniform_ptr->type);
        }
    } /* switch (uniform_ptr->type) */

    return result;
}

/** TODO */
PRIVATE unsigned int _ogl_program_ub_get_n_matrix_columns(__in __notnull const ogl_program_uniform_descriptor* uniform_ptr)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2:   result = 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3:   result = 3; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4:   result = 4; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x3: result = 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x4: result = 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x2: result = 3; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x4: result = 3; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x2: result = 4; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x3: result = 4; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized uniform type [%d]",
                              uniform_ptr->type);
        }
    } /* switch (uniform_ptr->type) */

    return result;
}

/** TODO */
PRIVATE unsigned int _ogl_program_ub_get_n_matrix_rows(__in __notnull const ogl_program_uniform_descriptor* uniform_ptr)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2:   result = 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3:   result = 3; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4:   result = 4; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x3: result = 3; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x4: result = 4; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x2: result = 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x4: result = 4; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x2: result = 2; break;
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x3: result = 3; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized uniform type [%d]",
                              uniform_ptr->type);
        }
    } /* switch (uniform_ptr->type) */

    return result;
}

/** TODO */
PRIVATE bool _ogl_program_ub_init(__in __notnull _ogl_program_ub* ub_ptr)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(ub_ptr != NULL,
                      "Input argument is NULL");

    /* Retrieve entry-points & platform-specific UB alignment requirement */
    ogl_context_type context_type                    = OGL_CONTEXT_TYPE_UNDEFINED;
    GLint            uniform_buffer_offset_alignment = 0;

    ogl_context_get_property(ub_ptr->context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_GL)
    {
        const ogl_context_gl_entrypoints*                             entry_points     = NULL;
        const ogl_context_gl_entrypoints_ext_direct_state_access*     entry_points_dsa = NULL;
        const ogl_context_gl_entrypoints_arb_program_interface_query* entry_points_piq = NULL;
        const ogl_context_gl_limits*                                  limits_ptr       = NULL;

        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);
        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_PROGRAM_INTERFACE_QUERY,
                                &entry_points_piq);
        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &entry_points_dsa);
        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        ub_ptr->pGLBindBuffer           = entry_points->pGLBindBuffer;
        ub_ptr->pGLBufferSubData        = entry_points->pGLBufferSubData;
        ub_ptr->pGLGetProgramResourceiv = entry_points_piq->pGLGetProgramResourceiv;
        ub_ptr->pGLUniformBlockBinding  = entry_points->pGLUniformBlockBinding;
        uniform_buffer_offset_alignment = limits_ptr->uniform_buffer_offset_alignment;

        if (entry_points_dsa->pGLNamedBufferSubDataEXT != NULL)
        {
            ub_ptr->pGLNamedBufferSubDataEXT = entry_points_dsa->pGLNamedBufferSubDataEXT;
        } /* if (entry_points_dsa->pGLNamedBufferSubDataEXT != NULL) */
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_ES,
                          "Unrecognized rendering context type");

        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        /* TODO: Create a limits array for ES contexts. */
        entry_points->pGLGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                                    &uniform_buffer_offset_alignment);

        ub_ptr->pGLBindBuffer           = entry_points->pGLBindBuffer;
        ub_ptr->pGLBufferSubData        = entry_points->pGLBufferSubData;
        ub_ptr->pGLGetProgramResourceiv = entry_points->pGLGetProgramResourceiv;
        ub_ptr->pGLUniformBlockBinding  = entry_points->pGLUniformBlockBinding;
    }

    /* Retrieve UB uniform block properties */
    static const GLenum ub_property_active_variables      = GL_ACTIVE_VARIABLES;
    static const GLenum ub_property_buffer_data_size      = GL_BUFFER_DATA_SIZE;
    static const GLenum ub_property_num_active_variables  = GL_NUM_ACTIVE_VARIABLES;
                 GLint  n_active_uniform_blocks           = 0;
                 GLint  n_active_uniform_block_max_length = 0;
                 GLuint po_id                             = ogl_program_get_id(ub_ptr->owner);

    ub_ptr->pGLGetProgramResourceiv(po_id,
                                    GL_UNIFORM_BLOCK,
                                    ub_ptr->index,
                                    1, /* propCount */
                                   &ub_property_buffer_data_size,
                                    1,    /* bufSize */
                                    NULL, /* length */
                                   &ub_ptr->block_data_size);

    if (ub_ptr->block_data_size > 0 &&
        ub_ptr->syncable)
    {
        ub_ptr->block_data = new (std::nothrow) unsigned char[ub_ptr->block_data_size];

        if (ub_ptr->block_data == NULL)
        {
            ASSERT_DEBUG_SYNC(ub_ptr->block_data != NULL,
                              "Out of memory");

            result = false;

            goto end;
        }

        /* All uniforms are set to zeroes by default. */
        memset(ub_ptr->block_data,
               0,
               ub_ptr->block_data_size);

        ogl_buffers_allocate_buffer_memory(ub_ptr->buffers,
                                           ub_ptr->block_data_size,
                                           uniform_buffer_offset_alignment,
                                           OGL_BUFFERS_MAPPABILITY_NONE,
                                           OGL_BUFFERS_USAGE_UBO,
                                           OGL_BUFFERS_FLAGS_NONE,
                                          &ub_ptr->block_bo_id,
                                          &ub_ptr->block_bo_start_offset);

        /* Force a data sync */
        ub_ptr->dirty_offset_end   = ub_ptr->block_data_size;
        ub_ptr->dirty_offset_start = 0;

        ogl_program_ub_sync( (ogl_program_ub) ub_ptr);
    } /* if (ub_ptr->block_data_size > 0 && ub_ptr->syncable) */

    /* Determine all uniform members of the block */
    GLint* active_uniform_indices = NULL;
    GLint  n_active_ub_uniforms   = 0;

    ub_ptr->pGLGetProgramResourceiv(po_id,
                                    GL_UNIFORM_BLOCK,
                                    ub_ptr->index,
                                    1, /* propCount */
                                   &ub_property_num_active_variables,
                                    1,    /* bufSize */
                                    NULL, /* length */
                                   &n_active_ub_uniforms);

    if (n_active_ub_uniforms > 0)
    {
        active_uniform_indices = new (std::nothrow) GLint[n_active_ub_uniforms];

        if (active_uniform_indices == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            result = false;
            goto end;
        } /* if (active_uniform_indices == NULL) */

        if (active_uniform_indices != NULL)
        {
            ub_ptr->pGLGetProgramResourceiv(po_id,
                                    GL_UNIFORM_BLOCK,
                                    ub_ptr->index,
                                    1, /* propCount */
                                   &ub_property_active_variables,
                                    n_active_ub_uniforms,
                                    NULL, /* length */
                                    active_uniform_indices);

            for (int n_active_uniform = 0;
                     n_active_uniform < n_active_ub_uniforms;
                   ++n_active_uniform)
            {
                const ogl_program_uniform_descriptor* uniform_ptr = NULL;

                if (!ogl_program_get_uniform_by_index(ub_ptr->owner,
                                                      active_uniform_indices[n_active_uniform],
                                                     &uniform_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve uniform descriptor for index [%d]",
                                      active_uniform_indices[n_active_uniform]);

                    result = false;
                    goto end;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(uniform_ptr->ub_offset != -1,
                                      "Uniform block member offset is -1!");
                    ASSERT_DEBUG_SYNC(!system_hash64map_contains(ub_ptr->offset_to_uniform_descriptor_map,
                                                                 (system_hash64) uniform_ptr->ub_offset),
                                      "Uniform block offset [%d] is already occupied!",
                                      uniform_ptr->ub_offset);

                    system_hash64map_insert     (ub_ptr->offset_to_uniform_descriptor_map,
                                                 (system_hash64) uniform_ptr->ub_offset,
                                                 (void*) uniform_ptr,
                                                 NULL,  /* on_remove_callback */
                                                 NULL); /* on_remove_callback_user_arg */
                    system_resizable_vector_push(ub_ptr->members,
                                                 (void*) uniform_ptr);
                }
            } /* for (all uniform block members) */

            delete [] active_uniform_indices;
            active_uniform_indices = NULL;
        }
    } /* if (n_active_ub_uniforms > 0) */

    /* All done */
end:
    return result;
}

/** TODO */
PRIVATE bool _ogl_program_ub_is_matrix_uniform(__in __notnull const ogl_program_uniform_descriptor* uniform_ptr)
{
    bool result = false;

    switch (uniform_ptr->type)
    {
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x3:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT2x4:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x2:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT3x4:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x2:
        case PROGRAM_UNIFORM_TYPE_FLOAT_MAT4x3:
        {
            result = true;
        }
    } /* switch (uniform_ptr->type) */

    return result;
}

/** TODO */
PRIVATE void _ogl_program_ub_set_uniform_value(__in                       __notnull ogl_program_ub ub,
                                               __in                                 GLuint         ub_uniform_offset,
                                               __in_ecount(src_data_size) __notnull const void*    src_data,
                                               __in                                 int            src_data_flags,
                                               __in                                 unsigned int   src_data_size,
                                               __in                                 unsigned int   dst_array_start_index,
                                               __in                                 unsigned int   dst_array_item_count)
{
    _ogl_program_ub* ub_ptr = (_ogl_program_ub*) ub;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(ub_ptr != NULL,
                      "Input UB is NULL - crash ahead.");

    ASSERT_DEBUG_SYNC(ub_uniform_offset < (GLuint) ub_ptr->block_data_size,
                      "Input UB uniform offset [%d] is larger than the preallocated UB data buffer! (size:%d)",
                      ub_uniform_offset,
                      ub_ptr->block_data_size);

    if (!ub_ptr->syncable)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Uniform block at index [%d] has not been initialized in synchronizable mode.",
                          ub_ptr->index);

        goto end;
    }

    /* Retrieve uniform descriptor */
    const ogl_program_uniform_descriptor* uniform_ptr = NULL;

    if (!system_hash64map_get(ub_ptr->offset_to_uniform_descriptor_map,
                              (system_hash64) ub_uniform_offset,
                             &uniform_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized uniform offset [%d].",
                          ub_uniform_offset);

        goto end;
    }

    /* Some further sanity checks.. */
    ASSERT_DEBUG_SYNC((dst_array_start_index == 0 || dst_array_item_count == 1) && uniform_ptr->size == 1                                             ||
                       dst_array_start_index >= 0 && dst_array_item_count >= 1  && (dst_array_start_index + dst_array_item_count <= uniform_ptr->size),
                      "Invalid dst array start index or dst array item count.");

    ASSERT_DEBUG_SYNC(uniform_ptr->ub_offset != -1,
                      "Uniform offset is -1");

    ASSERT_DEBUG_SYNC(uniform_ptr->size == 1                                       ||
                      uniform_ptr->size != 1 && uniform_ptr->ub_array_stride != -1,
                      "Uniform's array stride is -1 but the uniform is an array.");

    /* Check if src_data_size is correct */
    #ifdef _DEBUG
    {
        const unsigned int expected_src_data_size = _ogl_program_ub_get_expected_src_data_size(uniform_ptr,
                                                                                               dst_array_item_count);

        if (src_data_size != expected_src_data_size)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of bytes storing source data [%d] is not equal to the expected storage size [%d] for uniform at UB offset [%d]",
                              src_data_size,
                              expected_src_data_size,
                              ub_uniform_offset);

            goto end;
        } /* if (src_data_size != expected_src_data_size) */
    }
    #endif

    /* Proceed with updating the internal cache */
    const bool   is_uniform_matrix     = _ogl_program_ub_is_matrix_uniform(uniform_ptr);
    unsigned int modified_region_end   = DIRTY_OFFSET_UNUSED;
    unsigned int modified_region_start = DIRTY_OFFSET_UNUSED;

          unsigned char* dst_traveller_ptr = ub_ptr->block_data + uniform_ptr->ub_offset                                                       +
                                             ((uniform_ptr->ub_array_stride != -1) ? uniform_ptr->ub_array_stride : 0) * dst_array_start_index;
    const unsigned char* src_traveller_ptr = (const unsigned char*) src_data;

    if (is_uniform_matrix)
    {
        /* Matrix uniforms, yay */
        ASSERT_DEBUG_SYNC(src_data_flags == 0,
                          "TODO: transposed matrix data setting");

        if (_ogl_program_ub_get_memcpy_friendly_matrix_stride(uniform_ptr) != uniform_ptr->ub_matrix_stride)
        {
            /* NOTE: The following code may seem redundant but will be useful for code maintainability
             *       when we introduce transposed data support. */
            if (uniform_ptr->is_row_major_matrix)
            {
                /* Need to copy the data row-by-row */
                const unsigned int n_matrix_rows = _ogl_program_ub_get_n_matrix_rows(uniform_ptr);
                const unsigned int row_data_size = n_matrix_rows * sizeof(float);

                for (unsigned int n_row = 0;
                                  n_row < n_matrix_rows;
                                ++n_row)
                {
                    /* TODO: Consider doing epsilon-based comparisons here */
                    if (memcmp(dst_traveller_ptr,
                               src_traveller_ptr,
                               row_data_size) != 0)
                    {
                        memcpy(dst_traveller_ptr,
                               src_traveller_ptr,
                               row_data_size);

                        if (modified_region_start == DIRTY_OFFSET_UNUSED)
                        {
                            modified_region_start = dst_traveller_ptr - ub_ptr->block_data;
                        }

                        modified_region_end = dst_traveller_ptr - ub_ptr->block_data + row_data_size;
                    }

                    dst_traveller_ptr += uniform_ptr->ub_matrix_stride;
                    src_traveller_ptr += row_data_size;
                } /* for (all matrix rows) */
            }
            else
            {
                /* Need to copy the data column-by-column */
                const unsigned int n_matrix_columns = _ogl_program_ub_get_n_matrix_columns(uniform_ptr);
                const unsigned int column_data_size = sizeof(float) * n_matrix_columns;

                for (unsigned int n_column = 0;
                                  n_column < n_matrix_columns;
                                ++n_column)
                {
                    /* TODO: Consider doing epsilon-based comparisons here */
                    if (memcmp(dst_traveller_ptr,
                               src_traveller_ptr,
                               column_data_size) != 0)
                    {
                        memcpy(dst_traveller_ptr,
                               src_traveller_ptr,
                               column_data_size);

                        if (modified_region_start == DIRTY_OFFSET_UNUSED)
                        {
                            modified_region_start = dst_traveller_ptr - ub_ptr->block_data;
                        }

                        modified_region_end = dst_traveller_ptr - ub_ptr->block_data + column_data_size;
                    }

                    dst_traveller_ptr += uniform_ptr->ub_matrix_stride;
                    src_traveller_ptr += column_data_size;
                } /* for (all matrix columns) */
            }
        } /* if (_ogl_program_ub_get_memcpy_friendly_matrix_stride(uniform_ptr) != uniform_ptr->ub_matrix_stride) */
        else
        {
            /* Good to use the good old memcpy(), since the data is laid out linearly
             * without any padding in-between.
             *
             * TODO: Consider doing epsilon-based comparisons here */
            if (memcmp(dst_traveller_ptr,
                       src_traveller_ptr,
                       src_data_size) != 0)
            {
                memcpy(dst_traveller_ptr,
                       src_traveller_ptr,
                       src_data_size);

                modified_region_start = dst_traveller_ptr     - ub_ptr->block_data;
                modified_region_end   = modified_region_start + src_data_size;
            }
        }
    } /* if (is_uniform_matrix) */
    else
    {
        /* Non-matrix uniforms */
        ASSERT_DEBUG_SYNC(src_data_flags == 0,
                          "Flags != 0 but the target uniform is not a matrix.");

        if (uniform_ptr->ub_array_stride != 0  && /* no padding             */
            uniform_ptr->ub_array_stride != -1)   /* not an arrayed uniform */
        {
            const unsigned int src_single_item_size = _ogl_program_ub_get_expected_src_data_size(uniform_ptr,
                                                                                                 1);         /* n_array_items */

            /* Not good, need to take the padding into account.. */
            for (unsigned int n_item = 0;
                              n_item < dst_array_item_count;
                            ++n_item)
            {
                if (memcmp(dst_traveller_ptr,
                           src_traveller_ptr,
                           src_single_item_size) != 0)
                {
                    memcpy(dst_traveller_ptr,
                           src_traveller_ptr,
                           src_single_item_size);

                    if (modified_region_start == DIRTY_OFFSET_UNUSED)
                    {
                        modified_region_start = dst_traveller_ptr - ub_ptr->block_data;
                    }

                    modified_region_end = dst_traveller_ptr - ub_ptr->block_data + src_single_item_size;
                }

                dst_traveller_ptr += uniform_ptr->ub_array_stride;
                src_traveller_ptr += src_single_item_size;
            } /* for (all array items) */
        }
        else
        {
            /* Not an array! We can again go away with a single memcpy */
            ASSERT_DEBUG_SYNC(uniform_ptr->size == 1,
                              "Sanity check failed");

            if (memcmp(dst_traveller_ptr,
                       src_traveller_ptr,
                       src_data_size) != 0)
            {
                memcpy(dst_traveller_ptr,
                       src_traveller_ptr,
                       src_data_size);

                modified_region_start = dst_traveller_ptr     - ub_ptr->block_data;
                modified_region_end   = modified_region_start + src_data_size;
            }
        }
    }

    /* Update the dirty region delimiters if needed */
    ASSERT_DEBUG_SYNC(modified_region_start == DIRTY_OFFSET_UNUSED && modified_region_end == DIRTY_OFFSET_UNUSED ||
                      modified_region_start != DIRTY_OFFSET_UNUSED && modified_region_end != DIRTY_OFFSET_UNUSED,
                      "Sanity check failed.");

    if (modified_region_start != DIRTY_OFFSET_UNUSED &&
        modified_region_end   != DIRTY_OFFSET_UNUSED)
    {
        ASSERT_DEBUG_SYNC(modified_region_start    < ub_ptr->block_data_size &&
                          (modified_region_end - 1) < ub_ptr->block_data_size,
                          "Sanity check failed");

        if (ub_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED        ||
            modified_region_start      <  ub_ptr->dirty_offset_start)
        {
            ub_ptr->dirty_offset_start = modified_region_start;
        }

        if (ub_ptr->dirty_offset_end == DIRTY_OFFSET_UNUSED ||
            modified_region_end      >  ub_ptr->dirty_offset_end)
        {
            ub_ptr->dirty_offset_end = modified_region_end;
        }
    } /* if (modified_region_start != DIRTY_OFFSET_UNUSED && modified_region_end != DIRTY_OFFSET_UNUSED) */

    /* All done */
end:
    ;
}


/** Please see header for spec */
PUBLIC ogl_program_ub ogl_program_ub_create(__in __notnull ogl_context               context,
                                            __in __notnull ogl_program               owner_program,
                                            __in __notnull unsigned int              ub_index,
                                            __in __notnull system_hashed_ansi_string ub_name,
                                            __in           bool                      support_sync_behavior)
{
    _ogl_program_ub* new_ub_ptr = new (std::nothrow) _ogl_program_ub(context,
                                                                     owner_program,
                                                                     ub_index,
                                                                     ub_name,
                                                                     support_sync_behavior);

    ASSERT_DEBUG_SYNC(new_ub_ptr != NULL,
                      "Out of memory");

    if (new_ub_ptr != NULL)
    {
        _ogl_program_ub_init(new_ub_ptr);
    } /* if (new_ub_ptr != NULL) */

    return (ogl_program_ub) new_ub_ptr;
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_get_property(__in  __notnull const ogl_program_ub    ub,
                                                    __in            ogl_program_ub_property property,
                                                    __out __notnull void*                   out_result)
{
    const _ogl_program_ub* ub_ptr = (const _ogl_program_ub*) ub;

    switch (property)
    {
        case OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE:
        {
            ASSERT_DEBUG_SYNC(ub_ptr->syncable,
                              "OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE property only available for syncable ogl_program_ub instances.");

            *(unsigned int*) out_result = ub_ptr->block_data_size;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_BO_ID:
        {
            ASSERT_DEBUG_SYNC(ub_ptr->syncable,
                              "OGL_PROGRAM_UB_PROPERTY_BO_ID property only available for syncable ogl_program_ub instances.");

            *(GLuint*) out_result = ub_ptr->block_bo_id;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET:
        {
            ASSERT_DEBUG_SYNC(ub_ptr->syncable,
                              "OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET property only available for syncable ogl_program_ub instances.");

            *(GLuint*) out_result = ub_ptr->block_bo_start_offset;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_INDEX:
        {
            *(GLuint*) out_result = ub_ptr->index;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_INDEXED_UB_BP:
        {
            *(GLuint*) out_result = ub_ptr->indexed_ub_bp;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = ub_ptr->name;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_N_MEMBERS:
        {
            *(unsigned int*) out_result = system_resizable_vector_get_amount_of_elements(ub_ptr->members);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_program_ub_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_program_ub_release(__in __notnull ogl_program_ub ub)
{
    delete (_ogl_program_ub*) ub;
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_set_arrayed_uniform_value(__in                       __notnull ogl_program_ub ub,
                                                                 __in                                 GLuint         ub_uniform_offset,
                                                                 __in_ecount(src_data_size) __notnull const void*    src_data,
                                                                 __in                                 int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                 __in                                 unsigned int   src_data_size,
                                                                 __in                                 unsigned int   dst_array_start_index,
                                                                 __in                                 unsigned int   dst_array_item_count)
{
    _ogl_program_ub_set_uniform_value(ub,
                                      ub_uniform_offset,
                                      src_data,
                                      src_data_flags,
                                      src_data_size,
                                      dst_array_start_index,
                                      dst_array_item_count);
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_set_nonarrayed_uniform_value(__in                       __notnull ogl_program_ub ub,
                                                                    __in                                 GLuint         ub_uniform_offset,
                                                                    __in_ecount(src_data_size) __notnull const void*    src_data,
                                                                    __in                                 int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                    __in                                 unsigned int   src_data_size)
{
    _ogl_program_ub_set_uniform_value(ub,
                                      ub_uniform_offset,
                                      src_data,
                                      src_data_flags,
                                      src_data_size,
                                      0,  /* dst_array_start */
                                      1); /* dst_array_item_count */
}

/* Please see header for spec */
PUBLIC void ogl_program_ub_set_property(__in  __notnull const ogl_program_ub    ub,
                                        __in            ogl_program_ub_property property,
                                        __out __notnull const void*             data)
{
    _ogl_program_ub* ub_ptr = (_ogl_program_ub*) ub;

    switch (property)
    {
        case OGL_PROGRAM_UB_PROPERTY_INDEXED_UB_BP:
        {
            ub_ptr->indexed_ub_bp = *(GLuint*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_program_ub_proeprty value.");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void ogl_program_ub_sync(__in __notnull ogl_program_ub ub)
{
    _ogl_program_ub* ub_ptr = (_ogl_program_ub*) ub;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(ub_ptr->dirty_offset_end != DIRTY_OFFSET_UNUSED && ub_ptr->dirty_offset_start != DIRTY_OFFSET_UNUSED ||
                      ub_ptr->dirty_offset_end == DIRTY_OFFSET_UNUSED && ub_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED,
                      "Sanity check failed");

    if (!ub_ptr->syncable)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Uniform block at index [%d] has not been initialized in synchronizable mode.",
                          ub_ptr->index);

        goto end;
    }

    /* Anything to refresh? */
    if (ub_ptr->dirty_offset_end   == DIRTY_OFFSET_UNUSED &&
        ub_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED)
    {
        /* Nothing to synchronize */
        goto end;
    }

    if (ub_ptr->pGLNamedBufferSubDataEXT != NULL)
    {
        ub_ptr->pGLNamedBufferSubDataEXT(ub_ptr->block_bo_id,
                                         ub_ptr->block_bo_start_offset + ub_ptr->dirty_offset_start,
                                         ub_ptr->dirty_offset_end - ub_ptr->dirty_offset_start,
                                         ub_ptr->block_data + ub_ptr->dirty_offset_start);
    }
    else
    {
        /* Use a rarely used binding point to avoid collisions with existing bindings */
        ub_ptr->pGLBindBuffer   (GL_TRANSFORM_FEEDBACK_BUFFER,
                                 ub_ptr->block_bo_id);
        ub_ptr->pGLBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,
                                 ub_ptr->block_bo_start_offset + ub_ptr->dirty_offset_start,
                                 ub_ptr->dirty_offset_end - ub_ptr->dirty_offset_start,
                                 ub_ptr->block_data + ub_ptr->dirty_offset_start);
    }

    /* Reset the offsets */
    ub_ptr->dirty_offset_end   = DIRTY_OFFSET_UNUSED;
    ub_ptr->dirty_offset_start = DIRTY_OFFSET_UNUSED;

    /* All done */
end:
    ;
}
