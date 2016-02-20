/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO: Double matrix uniform support.
 * TODO: Improved synchronization impl.
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_block.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"

#define DIRTY_OFFSET_UNUSED (-1)


/** Internal types */
typedef struct _ogl_program_block
{
    ral_context  context; /* NOT retained */
    unsigned int index;
    raGL_program owner;

    /* block_data holds a local cache of variable data. This helps us avoid all the complex logic
     * that would otherwise have to be used to store uniform values AND is pretty cache-friendly. */
    ral_buffer             block_bo;
    unsigned char*         block_data;
    GLint                  block_data_size;
    ogl_program_block_type block_type;
    GLint                  indexed_bp;

    bool                      is_intel_driver;
    system_hashed_ansi_string name;
    system_resizable_vector   members_raGL;         /* pointers to const _raGL_program_variable*. */
    system_hash64map          members_raGL_by_name; /* HAS hash -> const _raGL_program_variable*. DO NOT release the items. */
    bool                      syncable;

    PFNGLBINDBUFFERPROC            pGLBindBuffer;
    PFNGLBUFFERSUBDATAPROC         pGLBufferSubData;
    PFNGLFINISHPROC                pGLFinish;
    PFNGLGETPROGRAMRESOURCEIVPROC  pGLGetProgramResourceiv;
    PFNGLNAMEDBUFFERSUBDATAEXTPROC pGLNamedBufferSubDataEXT;

    /* Delimits a region which needs to be re-uploaded to the GPU upon next
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

    explicit _ogl_program_block(ral_context               in_context,
                                raGL_program              in_owner,
                                unsigned int              in_index,
                                system_hashed_ansi_string in_name,
                                bool                      in_syncable,
                                ogl_program_block_type    in_type)
    {
        /* Sanity checks.
         *
         * 1. Sync is not available yet for SSBs (todo?)
         */
        ASSERT_DEBUG_SYNC(in_type == OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER && !in_syncable || 
                          in_type == OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                          "Sanity check failed");

        /* Initialize fields with default values */
        block_bo                  = NULL;
        block_data                = NULL;
        block_data_size           = 0;
        block_type                = in_type;
        context                   = in_context;
        dirty_offset_end          = DIRTY_OFFSET_UNUSED;
        dirty_offset_start        = DIRTY_OFFSET_UNUSED;
        index                     = in_index;
        indexed_bp                = 0; /* default GL state value */
        is_intel_driver           = false; /* updated later */
        name                      = in_name;
        members_raGL              = system_resizable_vector_create(4 /* capacity */);
        members_raGL_by_name      = system_hash64map_create       (sizeof(const _raGL_program_variable*) );
        owner                     = in_owner;
        pGLBufferSubData          = NULL;
        pGLFinish                 = NULL;
        pGLGetProgramResourceiv   = NULL;
        pGLNamedBufferSubDataEXT  = NULL;
        syncable                  = in_syncable;
    }

    ~_ogl_program_block()
    {
        if (block_bo != NULL)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_buffers */
                                       (const void**) &block_bo);

            block_bo = NULL;
        }

        if (block_data != NULL)
        {
            delete [] block_data;

            block_data = NULL;
        }

        if (members_raGL != NULL)
        {
            _raGL_program_variable* variable_raGL_ptr = NULL;

            while (system_resizable_vector_pop(members_raGL,
                                              &variable_raGL_ptr) )
            {
                delete variable_raGL_ptr;
            }

            system_resizable_vector_release(members_raGL);
            members_raGL = NULL;
        }

        if (members_raGL_by_name != NULL)
        {
            /* No need to release items - these are owned by raGL_program! */
            system_hash64map_release(members_raGL_by_name);

            members_raGL_by_name = NULL;
        }
    }
} _ogl_program_block;


/** TODO */
PRIVATE unsigned int _ogl_program_block_get_expected_src_data_size(const ral_program_variable* uniform_ptr,
                                                                   unsigned int                n_array_items)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT:             result = sizeof(float)         * 1;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2:        result = sizeof(float)         * 2;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3:        result = sizeof(float)         * 3;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4:        result = sizeof(float)         * 4;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_INT:               result = sizeof(int)           * 1;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_INT_VEC2:          result = sizeof(int)           * 2;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_INT_VEC3:          result = sizeof(int)           * 3;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_INT_VEC4:          result = sizeof(int)           * 4;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT:      result = sizeof(int)           * 1;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC2: result = sizeof(int)           * 2;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC3: result = sizeof(int)           * 3;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC4: result = sizeof(int)           * 4;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_BOOL:              result = sizeof(unsigned char) * 1;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC2:         result = sizeof(unsigned char) * 2;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC3:         result = sizeof(unsigned char) * 3;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC4:         result = sizeof(unsigned char) * 4;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2:        result = sizeof(float)         * 4;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3:        result = sizeof(float)         * 9;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4:        result = sizeof(float)         * 16; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3:      result = sizeof(float)         * 6;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4:      result = sizeof(float)         * 8;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2:      result = sizeof(float)         * 6;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4:      result = sizeof(float)         * 12; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2:      result = sizeof(float)         * 8;  break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3:      result = sizeof(float)         * 12; break;

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
PRIVATE unsigned int _ogl_program_block_get_memcpy_friendly_matrix_stride(const ral_program_variable* uniform_ptr)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        /* Square matrices */
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2: result = sizeof(float) * 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3: result = sizeof(float) * 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4: result = sizeof(float) * 4; break;

        /* Non-square matrices */
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 3
                                                        : sizeof(float) * 2;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 4
                                                        : sizeof(float) * 2;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 2
                                                        : sizeof(float) * 3;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 4
                                                        : sizeof(float) * 3;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2:
        {
            result = (uniform_ptr->is_row_major_matrix) ? sizeof(float) * 2
                                                        : sizeof(float) * 4;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3:
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
PRIVATE unsigned int _ogl_program_block_get_n_matrix_columns(const ral_program_variable* uniform_ptr)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2:   result = 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3:   result = 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4:   result = 4; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3: result = 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4: result = 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2: result = 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4: result = 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2: result = 4; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3: result = 4; break;

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
PRIVATE unsigned int _ogl_program_block_get_n_matrix_rows(const ral_program_variable* uniform_ptr)
{
    unsigned int result = 0;

    switch (uniform_ptr->type)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2:   result = 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3:   result = 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4:   result = 4; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3: result = 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4: result = 4; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2: result = 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4: result = 4; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2: result = 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3: result = 3; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized uniform type [%d]",
                              uniform_ptr->type);
        }
    } /* switch (uniform_ptr->type) */

    return result;
}

/** TODO.
 *
 *  NOTE: This function does not initialize SSB/UB bindings for a reason. If we tried to do that,
 *        we would overflow the stack, as the context wrapper's glShaderStorageBlockBinding() and
 *        glUniformBlockBinding() implementations would eventually call this function again.
 *        Instead, we configure the bindings in the linking handler, after all blocks are
 *        enumerated.
 */
PRIVATE bool _ogl_program_block_init(_ogl_program_block* block_ptr)
{
    GLint*      active_variable_indices = NULL;
    GLint       n_active_variables      = 0;
    GLuint      po_id                   = 0;
    ral_program po_ral                  = NULL;
    bool        result                  = true;

    ASSERT_DEBUG_SYNC(block_ptr != NULL,
                      "Input argument is NULL");

    raGL_program_get_property(block_ptr->owner,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &po_id);

    /* Retrieve entry-points & platform-specific UB alignment requirement */
    ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

    ral_context_get_property(block_ptr->context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    if (backend_type == RAL_BACKEND_TYPE_GL)
    {
        const ogl_context_gl_entrypoints*                         entry_points     = NULL;
        const ogl_context_gl_entrypoints_ext_direct_state_access* entry_points_dsa = NULL;
        const ogl_context_gl_limits*                              limits_ptr       = NULL;

        ogl_context_get_property(ral_context_get_gl_context(block_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);
        ogl_context_get_property(ral_context_get_gl_context(block_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &entry_points_dsa);
        ogl_context_get_property(ral_context_get_gl_context(block_ptr->context),
                                 OGL_CONTEXT_PROPERTY_IS_INTEL_DRIVER,
                                &block_ptr->is_intel_driver);
        ogl_context_get_property(ral_context_get_gl_context(block_ptr->context),
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        block_ptr->pGLBindBuffer           = entry_points->pGLBindBuffer;
        block_ptr->pGLBufferSubData        = entry_points->pGLBufferSubData;
        block_ptr->pGLFinish               = entry_points->pGLFinish;
        block_ptr->pGLGetProgramResourceiv = entry_points->pGLGetProgramResourceiv;

        if (entry_points_dsa->pGLNamedBufferSubDataEXT != NULL)
        {
            block_ptr->pGLNamedBufferSubDataEXT = entry_points_dsa->pGLNamedBufferSubDataEXT;
        } /* if (entry_points_dsa->pGLNamedBufferSubDataEXT != NULL) */
    }
    else
    {
        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_ES,
                          "Unrecognized rendering backend type");

        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(ral_context_get_gl_context(block_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        block_ptr->pGLBindBuffer           = entry_points->pGLBindBuffer;
        block_ptr->pGLBufferSubData        = entry_points->pGLBufferSubData;
        block_ptr->pGLFinish               = entry_points->pGLFinish;
        block_ptr->pGLGetProgramResourceiv = entry_points->pGLGetProgramResourceiv;
    }

    /* Convert the block type to GL interface type */
    const GLenum block_type_gl = (block_ptr->block_type == OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER) ? GL_SHADER_STORAGE_BLOCK
                                                                                                         : GL_UNIFORM_BLOCK;

    ASSERT_DEBUG_SYNC(block_ptr->block_type == OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER ||
                      block_ptr->block_type == OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                      "Unrecognized ogl_program_block type.");

    /* Retrieve UB uniform block properties */
    static const GLenum block_property_active_variables     = GL_ACTIVE_VARIABLES;
    static const GLenum block_property_buffer_data_size     = GL_BUFFER_DATA_SIZE;
    static const GLenum block_property_num_active_variables = GL_NUM_ACTIVE_VARIABLES;
                 GLint  n_active_uniform_blocks             = 0;
                 GLint  n_active_uniform_block_max_length   = 0;

    block_ptr->pGLGetProgramResourceiv(po_id,
                                       block_type_gl,
                                       block_ptr->index,
                                       1, /* propCount */
                                      &block_property_buffer_data_size,
                                       1,    /* bufSize */
                                       NULL, /* length */
                                      &block_ptr->block_data_size);

    if (block_ptr->block_data_size > 0 &&
        block_ptr->syncable)
    {
        ral_buffer_create_info block_create_info;

        block_ptr->block_data = new (std::nothrow) unsigned char[block_ptr->block_data_size];

        if (block_ptr->block_data == NULL)
        {
            ASSERT_DEBUG_SYNC(block_ptr->block_data != NULL,
                              "Out of memory");

            result = false;

            goto end;
        }

        /* All uniforms need to be set to zeroes. */
        memset(block_ptr->block_data,
               0,
               block_ptr->block_data_size);

        block_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
        block_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
        block_create_info.size             = block_ptr->block_data_size;
        block_create_info.usage_bits       = RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        block_create_info.user_queue_bits  = 0xFFFFFFFF;

        ral_context_create_buffers(block_ptr->context,
                                   1,
                                  &block_create_info,
                                  &block_ptr->block_bo);

        /* Force a data sync */
        block_ptr->dirty_offset_end   = block_ptr->block_data_size;
        block_ptr->dirty_offset_start = 0;

        ogl_program_block_sync( (ogl_program_block) block_ptr);
    } /* if (ub_ptr->block_data_size > 0 && ub_ptr->syncable) */

    /* Register the block on RAL level */
    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(block_ptr->name) > 0,
                      "This path should not be executed for default UB.");

    raGL_program_get_property(block_ptr->owner,
                              RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                             &po_ral);

    /* Determine all variables for the block */
    block_ptr->pGLGetProgramResourceiv(po_id,
                                       block_type_gl,
                                       block_ptr->index,
                                       1, /* propCount */
                                      &block_property_num_active_variables,
                                       1,    /* bufSize */
                                       NULL, /* length */
                                      &n_active_variables);

    if (n_active_variables > 0)
    {
        active_variable_indices = new (std::nothrow) GLint[n_active_variables];

        if (active_variable_indices == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            result = false;
            goto end;
        } /* if (active_variable_indices == NULL) */

        if (active_variable_indices != NULL)
        {
            block_ptr->pGLGetProgramResourceiv(po_id,
                                               block_type_gl,
                                               block_ptr->index,
                                               1, /* propCount */
                                              &block_property_active_variables,
                                               n_active_variables,
                                               NULL, /* length */
                                               active_variable_indices);

            for (int n_active_variable = 0;
                     n_active_variable < n_active_variables;
                   ++n_active_variable)
            {
                _raGL_program_variable* variable_raGL_ptr = NULL;
                ral_program_variable*   variable_ral_ptr  = NULL;

                if (block_ptr->block_type == OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER)
                {
                    variable_raGL_ptr = new (std::nothrow) _raGL_program_variable;
                    variable_ral_ptr  = new (std::nothrow) ral_program_variable;

                    raGL_program_get_program_variable_details(block_ptr->owner,
                                                              0,    /* temp_variable_name_storage */
                                                              NULL, /* temp_variable_name */
                                                              variable_ral_ptr,
                                                              variable_raGL_ptr,
                                                              GL_BUFFER_VARIABLE,
                                                              active_variable_indices[n_active_variable]);

                    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(variable_raGL_ptr->name) > 0,
                                      "SSBO variable name length is 0");

                    system_resizable_vector_push(block_ptr->members_raGL,
                                                 (void*) variable_raGL_ptr);
                    system_hash64map_insert     (block_ptr->members_raGL_by_name,
                                                 system_hashed_ansi_string_get_hash(variable_raGL_ptr->name),
                                                 (void*) variable_raGL_ptr,
                                                 NULL,  /* on_remove_callback_proc */
                                                 NULL); /* callback_argument */
                }
                else
                {
                    ral_program             program_ral            = NULL;
                    _raGL_program_variable* temp_variable_raGL_ptr = NULL;

                    ASSERT_DEBUG_SYNC(block_ptr->block_type == OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                      "Sanity check failed");

                    raGL_program_get_property(block_ptr->owner,
                                              RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                                              &program_ral);

                    if (!raGL_program_get_uniform_by_index(block_ptr->owner,
                                                           active_variable_indices[n_active_variable],
                                                           (const _raGL_program_variable**) &temp_variable_raGL_ptr) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve variable descriptor for index [%d]",
                                          active_variable_indices[n_active_variable]);

                        result = false;
                        goto end;
                    }

                    variable_raGL_ptr = new (std::nothrow) _raGL_program_variable(*temp_variable_raGL_ptr);
                    variable_ral_ptr  = new (std::nothrow) ral_program_variable  ();

                    raGL_program_get_program_variable_details(block_ptr->owner,
                                                              0,    /* temp_variable_name_storage */
                                                              NULL, /* temp_variable_name */
                                                              variable_ral_ptr,
                                                              NULL,
                                                              GL_UNIFORM,
                                                              active_variable_indices[n_active_variable]);
                    /* Store the descriptor */
                    system_resizable_vector_push(block_ptr->members_raGL,
                                                 (void*) variable_raGL_ptr);
                    system_hash64map_insert     (block_ptr->members_raGL_by_name,
                                                 system_hashed_ansi_string_get_hash(variable_raGL_ptr->name),
                                                 (void*) variable_raGL_ptr,
                                                 NULL,  /* on_remove_callback_proc */
                                                 NULL); /* callback_argument */
                }

                ral_program_attach_variable_to_metadata_block(po_ral,
                                                              block_ptr->name,
                                                              variable_ral_ptr);
            } /* for (all active variables) */

            delete [] active_variable_indices;
            active_variable_indices = NULL;
        }
    } /* if (n_active_variable > 0) */

    /* All done */
end:
    return result;
}

/** TODO */
PRIVATE bool _ogl_program_block_is_matrix_uniform(const ral_program_variable* uniform_ptr)
{
    bool result = false;

    switch (uniform_ptr->type)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2:
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3:
        {
            result = true;
        }
    } /* switch (uniform_ptr->type) */

    return result;
}

/** TODO */
PRIVATE void _ogl_program_block_set_uniform_value(ogl_program_block block,
                                                  GLuint            block_variable_offset,
                                                  const void*       src_data,
                                                  unsigned int      src_data_size,
                                                  unsigned int      dst_array_start_index,
                                                  unsigned int      dst_array_item_count)
{
    _ogl_program_block*         block_ptr             = (_ogl_program_block*) block;
    unsigned char*              dst_traveller_ptr     = NULL;
    bool                        is_uniform_matrix     = false;
    unsigned int                modified_region_end   = DIRTY_OFFSET_UNUSED;
    unsigned int                modified_region_start = DIRTY_OFFSET_UNUSED;
    ral_program                 program_ral           = NULL;
    const unsigned char*        src_traveller_ptr     = NULL;
    const ral_program_variable* variable_ral_ptr      = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(block_ptr != NULL,
                      "Input UB is NULL - crash ahead.");

    ASSERT_DEBUG_SYNC(block_variable_offset < (GLuint) block_ptr->block_data_size,
                      "Input UB uniform offset [%d] is larger than the preallocated UB data buffer! (size:%d)",
                      block_variable_offset,
                      block_ptr->block_data_size);

    if (!block_ptr->syncable)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Uniform block at index [%d] has not been initialized in synchronizable mode.",
                          block_ptr->index);

        goto end;
    }

    /* Retrieve uniform descriptor */
    raGL_program_get_property(block_ptr->owner,
                              RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                             &program_ral);

    if (!ral_program_get_block_variable_by_offset(program_ral,
                                                  block_ptr->name,
                                                  block_variable_offset,
                                                 &variable_ral_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized uniform offset [%d].",
                          block_variable_offset);

        goto end;
    }

    /* Some further sanity checks.. */
    ASSERT_DEBUG_SYNC((dst_array_start_index == 0 || dst_array_item_count == 1) && variable_ral_ptr->size == 1                                                      ||
                       dst_array_start_index >= 0 && dst_array_item_count >= 1  && (int32_t(dst_array_start_index + dst_array_item_count) <= variable_ral_ptr->size),
                      "Invalid dst array start index or dst array item count.");

    ASSERT_DEBUG_SYNC(variable_ral_ptr->block_offset != -1,
                      "Uniform offset is -1");

    ASSERT_DEBUG_SYNC(variable_ral_ptr->size == 1                                         ||
                      variable_ral_ptr->size != 1 && variable_ral_ptr->array_stride != -1,
                      "Uniform's array stride is -1 but the uniform is an array.");

    /* Check if src_data_size is correct */
    #ifdef _DEBUG
    {
        const unsigned int expected_src_data_size = _ogl_program_block_get_expected_src_data_size(variable_ral_ptr,
                                                                                                  dst_array_item_count);

        if (src_data_size != expected_src_data_size)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of bytes storing source data [%d] is not equal to the expected storage size [%d] for uniform at UB offset [%d]",
                              src_data_size,
                              expected_src_data_size,
                              block_variable_offset);

            goto end;
        } /* if (src_data_size != expected_src_data_size) */
    }
    #endif

    /* Proceed with updating the internal cache */
    is_uniform_matrix = _ogl_program_block_is_matrix_uniform(variable_ral_ptr);

    dst_traveller_ptr = block_ptr->block_data + variable_ral_ptr->block_offset                                                +
                        ((variable_ral_ptr->array_stride != -1) ? variable_ral_ptr->array_stride : 0) * dst_array_start_index;
    src_traveller_ptr = (const unsigned char*) src_data;

    if (is_uniform_matrix)
    {
        /* Matrix uniforms, yay */
        if (_ogl_program_block_get_memcpy_friendly_matrix_stride(variable_ral_ptr) != variable_ral_ptr->matrix_stride)
        {
            /* NOTE: The following code may seem redundant but will be useful for code maintainability
             *       when we introduce transposed data support. */
            if (variable_ral_ptr->is_row_major_matrix)
            {
                /* Need to copy the data row-by-row */
                const unsigned int n_matrix_rows = _ogl_program_block_get_n_matrix_rows(variable_ral_ptr);
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
                            modified_region_start = dst_traveller_ptr - block_ptr->block_data;
                        }

                        modified_region_end = dst_traveller_ptr - block_ptr->block_data + row_data_size;
                    }

                    dst_traveller_ptr += variable_ral_ptr->matrix_stride;
                    src_traveller_ptr += row_data_size;
                } /* for (all matrix rows) */
            }
            else
            {
                /* Need to copy the data column-by-column */
                const unsigned int n_matrix_columns = _ogl_program_block_get_n_matrix_columns(variable_ral_ptr);
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
                            modified_region_start = dst_traveller_ptr - block_ptr->block_data;
                        }

                        modified_region_end = dst_traveller_ptr - block_ptr->block_data + column_data_size;
                    }

                    dst_traveller_ptr += variable_ral_ptr->matrix_stride;
                    src_traveller_ptr += column_data_size;
                } /* for (all matrix columns) */
            }
        } /* if (_ogl_program_ub_get_memcpy_friendly_matrix_stride(uniform_ptr) != uniform_ptr->matrix_stride) */
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

                modified_region_start = dst_traveller_ptr     - block_ptr->block_data;
                modified_region_end   = modified_region_start + src_data_size;
            }
        }
    } /* if (is_uniform_matrix) */
    else
    {
        /* Non-matrix uniforms */
        if (variable_ral_ptr->array_stride != 0  && /* no padding             */
            variable_ral_ptr->array_stride != -1)   /* not an arrayed uniform */
        {
            const unsigned int src_single_item_size = _ogl_program_block_get_expected_src_data_size(variable_ral_ptr,
                                                                                                    1);         /* n_array_items */

            /* Not good, need to take the padding into account..
             *
             * TODO: Optimize the case where uniform_ptr->array_stride == src_single_item_size */
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
                        modified_region_start = dst_traveller_ptr - block_ptr->block_data;
                    }

                    modified_region_end = dst_traveller_ptr - block_ptr->block_data + src_single_item_size;
                }

                dst_traveller_ptr += variable_ral_ptr->array_stride;
                src_traveller_ptr += src_single_item_size;
            } /* for (all array items) */
        }
        else
        {
            /* Not an array! We can again go away with a single memcpy */
            ASSERT_DEBUG_SYNC(variable_ral_ptr->size == 1,
                              "Sanity check failed");

            if (memcmp(dst_traveller_ptr,
                       src_traveller_ptr,
                       src_data_size) != 0)
            {
                memcpy(dst_traveller_ptr,
                       src_traveller_ptr,
                       src_data_size);

                modified_region_start = dst_traveller_ptr     - block_ptr->block_data;
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
        ASSERT_DEBUG_SYNC(modified_region_start     < uint32_t(block_ptr->block_data_size) &&
                          (modified_region_end - 1) < uint32_t(block_ptr->block_data_size),
                          "Sanity check failed");

        if (block_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED           ||
            modified_region_start         <  block_ptr->dirty_offset_start)
        {
            block_ptr->dirty_offset_start = modified_region_start;
        }

        if (block_ptr->dirty_offset_end == DIRTY_OFFSET_UNUSED ||
            modified_region_end         >  block_ptr->dirty_offset_end)
        {
            block_ptr->dirty_offset_end = modified_region_end;
        }
    } /* if (modified_region_start != DIRTY_OFFSET_UNUSED && modified_region_end != DIRTY_OFFSET_UNUSED) */

    /* All done */
end:
    ;
}


/** Please see header for spec */
PUBLIC ogl_program_block ogl_program_block_create(ral_context               context,
                                                  raGL_program              owner_program,
                                                  ogl_program_block_type    block_type,
                                                  unsigned int              block_index,
                                                  system_hashed_ansi_string block_name,
                                                  bool                      support_sync_behavior)
{
    _ogl_program_block* new_block_ptr = new (std::nothrow) _ogl_program_block(context,
                                                                              owner_program,
                                                                              block_index,
                                                                              block_name,
                                                                              support_sync_behavior,
                                                                              block_type);

    ASSERT_DEBUG_SYNC(new_block_ptr != NULL,
                      "Out of memory");

    if (new_block_ptr != NULL)
    {
        _ogl_program_block_init(new_block_ptr);
    } /* if (new_ub_ptr != NULL) */

    return (ogl_program_block) new_block_ptr;
}

/** Please see header for spec */
PUBLIC bool ogl_program_block_get_block_variable(ogl_program_block              block,
                                                 unsigned int                   index,
                                                 const _raGL_program_variable** out_variable_ptr)
{
    _ogl_program_block* block_ptr = (_ogl_program_block*) block;
    bool                result    = false;

    result = system_resizable_vector_get_element_at(block_ptr->members_raGL,
                                                    index,
                                                    out_variable_ptr);

    return result;
}

/** Please see header for spec */
PUBLIC bool ogl_program_block_get_block_variable_by_name(ogl_program_block              block,
                                                         system_hashed_ansi_string      name,
                                                         const _raGL_program_variable** out_variable_ptr)
{
    const _ogl_program_block* block_ptr = (const _ogl_program_block*) block;
    bool                      result    = true;

    ASSERT_DEBUG_SYNC(block_ptr != NULL,
                      "Input ogl_program_block instance is NULL");

    if (!system_hash64map_get(block_ptr->members_raGL_by_name,
                              system_hashed_ansi_string_get_hash(name),
                              out_variable_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input variable [%s] is not recognized by ogl_program_block",
                          system_hashed_ansi_string_get_buffer(name) );

        result = false;
    }

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_program_block_get_property(const ogl_program_block    block,
                                           ogl_program_block_property property,
                                           void*                      out_result)
{
    const _ogl_program_block* block_ptr = (const _ogl_program_block*) block;

    switch (property)
    {
        case OGL_PROGRAM_BLOCK_PROPERTY_BLOCK_DATA_SIZE:
        {
            ASSERT_DEBUG_SYNC(block_ptr->syncable,
                              "OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE property only available for syncable ogl_program_ub instances.");

            *(unsigned int*) out_result = block_ptr->block_data_size;

            break;
        }

        case OGL_PROGRAM_BLOCK_PROPERTY_BUFFER_RAL:
        {
            ASSERT_DEBUG_SYNC(block_ptr->syncable,
                              "OGL_PROGRAM_BLOCK_PROPERTY_BUFFER_RAL property only available for syncable ogl_program_ub instances.");

            *(ral_buffer*) out_result = block_ptr->block_bo;

            break;
        }

        case OGL_PROGRAM_BLOCK_PROPERTY_INDEX:
        {
            *(GLuint*) out_result = block_ptr->index;

            break;
        }

        case OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP:
        {
            *(GLuint*) out_result = block_ptr->indexed_bp;

            break;
        }

        case OGL_PROGRAM_BLOCK_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = block_ptr->name;

            break;
        }

        case OGL_PROGRAM_BLOCK_PROPERTY_N_MEMBERS:
        {
            system_resizable_vector_get_property(block_ptr->members_raGL,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

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
PUBLIC void ogl_program_block_release(ogl_program_block block)
{
    delete (_ogl_program_block*) block;
}

/* Please see header for spec */
PUBLIC void ogl_program_block_set_arrayed_variable_value(ogl_program_block block,
                                                         GLuint            block_variable_offset,
                                                         const void*       src_data,
                                                         unsigned int      src_data_size,
                                                         unsigned int      dst_array_start_index,
                                                         unsigned int      dst_array_item_count)
{
    _ogl_program_block_set_uniform_value(block,
                                         block_variable_offset,
                                         src_data,
                                         src_data_size,
                                         dst_array_start_index,
                                         dst_array_item_count);
}

/* Please see header for spec */
PUBLIC void ogl_program_block_set_nonarrayed_variable_value(ogl_program_block block,
                                                            GLuint            block_variable_offset,
                                                            const void*       src_data,
                                                            unsigned int      src_data_size)
{
    _ogl_program_block_set_uniform_value(block,
                                         block_variable_offset,
                                         src_data,
                                         src_data_size,
                                         0,  /* dst_array_start */
                                         1); /* dst_array_item_count */
}

/* Please see header for spec */
PUBLIC void ogl_program_block_set_property(const ogl_program_block    block,
                                           ogl_program_block_property property,
                                           const void*                data)
{
    _ogl_program_block* block_ptr = (_ogl_program_block*) block;

    switch (property)
    {
        case OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP:
        {
            block_ptr->indexed_bp = *(GLuint*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_program_block_property value.");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_program_block_sync(ogl_program_block block)
{
    _ogl_program_block* block_ptr = (_ogl_program_block*) block;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(block_ptr->dirty_offset_end != DIRTY_OFFSET_UNUSED && block_ptr->dirty_offset_start != DIRTY_OFFSET_UNUSED ||
                      block_ptr->dirty_offset_end == DIRTY_OFFSET_UNUSED && block_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED,
                      "Sanity check failed");

    if (!block_ptr->syncable)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Uniform block at index [%d] has not been initialized in synchronizable mode.",
                          block_ptr->index);

        goto end;
    }

    /* Anything to refresh? */
    if (block_ptr->dirty_offset_end   == DIRTY_OFFSET_UNUSED &&
        block_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED)
    {
        /* Nothing to synchronize */
        goto end;
    }

    ral_buffer_client_sourced_update_info bo_update_info;

    bo_update_info.data         = block_ptr->block_data       + block_ptr->dirty_offset_start;
    bo_update_info.data_size    = block_ptr->dirty_offset_end - block_ptr->dirty_offset_start;
    bo_update_info.start_offset = block_ptr->dirty_offset_start;

    ral_buffer_set_data_from_client_memory(block_ptr->block_bo,
                                           1, /* n_updates */
                                          &bo_update_info);

    if (block_ptr->is_intel_driver)
    {
        /* Sigh. */
        block_ptr->pGLFinish();
    }

    /* Reset the offsets */
    block_ptr->dirty_offset_end   = DIRTY_OFFSET_UNUSED;
    block_ptr->dirty_offset_start = DIRTY_OFFSET_UNUSED;

    /* All done */
end:
    ;
}
