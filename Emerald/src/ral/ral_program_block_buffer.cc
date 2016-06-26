/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "system/system_log.h"

#define DIRTY_OFFSET_UNUSED (-1)


typedef struct _ral_program_block_buffer
{
    ral_buffer  buffer_ral;
    ral_context context_ral;
    ral_program program_ral;

    unsigned char*            data;
    system_hashed_ansi_string name;
    unsigned int              size;
    ral_program_block_type    type;

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

    _ral_program_block_buffer()
    {
        buffer_ral         = nullptr;
        context_ral        = nullptr;
        data               = nullptr;
        dirty_offset_end   = DIRTY_OFFSET_UNUSED;
        dirty_offset_start = DIRTY_OFFSET_UNUSED;
        name               = nullptr;
        program_ral        = nullptr;
        size               = 0;
    }

    ~_ral_program_block_buffer()
    {
        if (buffer_ral != nullptr)
        {
            ral_context_delete_objects(context_ral,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_objects */
                                       (const void**) &buffer_ral);

            buffer_ral = nullptr;
        }

        if (data != nullptr)
        {
            delete [] data;

            data = nullptr;
        }
    }
} _ral_program_block_buffer;


/** TODO */
PRIVATE unsigned int _ral_program_block_buffer_get_expected_src_data_size(const ral_program_variable* variable_ptr,
                                                                          unsigned int                n_array_items)
{
    unsigned int result = 0;

    switch (variable_ptr->type)
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
                              "Unrecognized variable type [%d]",
                              variable_ptr->type);
        }
    }

    result *= n_array_items;

    /* All done */
    return result;
}

/** TODO */
PRIVATE unsigned int _ral_program_block_buffer_get_memcpy_friendly_matrix_stride(const ral_program_variable* variable_ptr)
{
    unsigned int result = 0;

    switch (variable_ptr->type)
    {
        /* Square matrices */
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2: result = sizeof(float) * 2; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3: result = sizeof(float) * 3; break;
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4: result = sizeof(float) * 4; break;

        /* Non-square matrices */
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3:
        {
            result = (variable_ptr->is_row_major_matrix) ? sizeof(float) * 3
                                                         : sizeof(float) * 2;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4:
        {
            result = (variable_ptr->is_row_major_matrix) ? sizeof(float) * 4
                                                         : sizeof(float) * 2;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2:
        {
            result = (variable_ptr->is_row_major_matrix) ? sizeof(float) * 2
                                                         : sizeof(float) * 3;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4:
        {
            result = (variable_ptr->is_row_major_matrix) ? sizeof(float) * 4
                                                         : sizeof(float) * 3;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2:
        {
            result = (variable_ptr->is_row_major_matrix) ? sizeof(float) * 2
                                                         : sizeof(float) * 4;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3:
        {
            result = (variable_ptr->is_row_major_matrix) ? sizeof(float) * 3
                                                         : sizeof(float) * 4;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized variable type [%d]",
                              variable_ptr->type);
        }
    }

    return result;
}

/** TODO */
PRIVATE unsigned int _ral_program_block_buffer_get_n_matrix_columns(const ral_program_variable* variable_ptr)
{
    unsigned int result = 0;

    switch (variable_ptr->type)
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
                              "Unrecognized variable type [%d]",
                              variable_ptr->type);
        }
    }

    return result;
}

/** TODO */
PRIVATE unsigned int _ral_program_block_buffer_get_n_matrix_rows(const ral_program_variable* variable_ptr)
{
    unsigned int result = 0;

    switch (variable_ptr->type)
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
                              "Unrecognized variable type [%d]",
                              variable_ptr->type);
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _ral_program_block_buffer_is_matrix_variable(const ral_program_variable* variable_ptr)
{
    bool result = false;

    switch (variable_ptr->type)
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
    }

    return result;
}

/** TODO */
PRIVATE void _ral_program_block_buffer_set_variable_value(_ral_program_block_buffer* block_buffer_ptr,
                                                          unsigned int               block_variable_offset,
                                                          const void*                src_data,
                                                          unsigned int               src_data_size,
                                                          unsigned int               dst_array_start_index,
                                                          unsigned int               dst_array_item_count)
{
    unsigned char*              dst_traveller_ptr     = nullptr;
    bool                        is_matrix_variable    = false;
    unsigned int                modified_region_end   = DIRTY_OFFSET_UNUSED;
    unsigned int                modified_region_start = DIRTY_OFFSET_UNUSED;
    const unsigned char*        src_traveller_ptr     = nullptr;
    const ral_program_variable* variable_ral_ptr      = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(block_buffer_ptr != nullptr,
                      "Input UB is NULL - crash ahead.");

    ASSERT_DEBUG_SYNC(block_variable_offset < block_buffer_ptr->size,
                      "Input variable offset [%d] is larger than the preallocated data buffer! (size:%d)",
                      block_variable_offset,
                      block_buffer_ptr->size);

    /* Retrieve uniform descriptor */
    if (!ral_program_get_block_variable_by_offset(block_buffer_ptr->program_ral,
                                                  block_buffer_ptr->name,
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
        const unsigned int expected_src_data_size = _ral_program_block_buffer_get_expected_src_data_size(variable_ral_ptr,
                                                                                                         dst_array_item_count);

        if (src_data_size != expected_src_data_size)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of bytes storing source data [%d] is not equal to the expected storage size [%d] for variable at block offset [%d]",
                              src_data_size,
                              expected_src_data_size,
                              block_variable_offset);

            goto end;
        }
    }
    #endif

    /* Proceed with updating the internal cache */
    is_matrix_variable = _ral_program_block_buffer_is_matrix_variable(variable_ral_ptr);

    dst_traveller_ptr = block_buffer_ptr->data + variable_ral_ptr->block_offset                                               +
                        ((variable_ral_ptr->array_stride != -1) ? variable_ral_ptr->array_stride : 0) * dst_array_start_index;
    src_traveller_ptr = (const unsigned char*) src_data;

    if (is_matrix_variable)
    {
        /* Matrix variables, yay */
        if (_ral_program_block_buffer_get_memcpy_friendly_matrix_stride(variable_ral_ptr) != variable_ral_ptr->matrix_stride)
        {
            /* NOTE: The following code may seem redundant but will be useful for code maintainability
             *       if we introduce transposed data support. */
            if (variable_ral_ptr->is_row_major_matrix)
            {
                /* Need to copy the data row-by-row */
                const unsigned int n_matrix_rows = _ral_program_block_buffer_get_n_matrix_rows(variable_ral_ptr);
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
                            modified_region_start = dst_traveller_ptr - block_buffer_ptr->data;
                        }

                        modified_region_end = dst_traveller_ptr - block_buffer_ptr->data + row_data_size;
                    }

                    dst_traveller_ptr += variable_ral_ptr->matrix_stride;
                    src_traveller_ptr += row_data_size;
                }
            }
            else
            {
                /* Need to copy the data column-by-column */
                const unsigned int n_matrix_columns = _ral_program_block_buffer_get_n_matrix_columns(variable_ral_ptr);
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
                            modified_region_start = dst_traveller_ptr - block_buffer_ptr->data;
                        }

                        modified_region_end = dst_traveller_ptr - block_buffer_ptr->data + column_data_size;
                    }

                    dst_traveller_ptr += variable_ral_ptr->matrix_stride;
                    src_traveller_ptr += column_data_size;
                }
            }
        }
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

                modified_region_start = dst_traveller_ptr     - block_buffer_ptr->data;
                modified_region_end   = modified_region_start + src_data_size;
            }
        }
    }
    else
    {
        /* Non-matrix variables */
        if (variable_ral_ptr->array_stride != 0  && /* no padding             */
            variable_ral_ptr->array_stride != -1)   /* not an arrayed uniform */
        {
            const unsigned int src_single_item_size = _ral_program_block_buffer_get_expected_src_data_size(variable_ral_ptr,
                                                                                                           1);         /* n_array_items */

            /* Not good, need to take the padding into account..
             *
             * TODO: Optimize the case where variable_ptr->array_stride == src_single_item_size */
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
                        modified_region_start = dst_traveller_ptr - block_buffer_ptr->data;
                    }

                    modified_region_end = dst_traveller_ptr - block_buffer_ptr->data + src_single_item_size;
                }

                dst_traveller_ptr += variable_ral_ptr->array_stride;
                src_traveller_ptr += src_single_item_size;
            }
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

                modified_region_start = dst_traveller_ptr     - block_buffer_ptr->data;
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
        ASSERT_DEBUG_SYNC(modified_region_start     < uint32_t(block_buffer_ptr->size) &&
                          (modified_region_end - 1) < uint32_t(block_buffer_ptr->size),
                          "Sanity check failed");

        if (block_buffer_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED                  ||
            modified_region_start                <  block_buffer_ptr->dirty_offset_start)
        {
            block_buffer_ptr->dirty_offset_start = modified_region_start;
        }

        if (block_buffer_ptr->dirty_offset_end == DIRTY_OFFSET_UNUSED                ||
            modified_region_end                >  block_buffer_ptr->dirty_offset_end)
        {
            block_buffer_ptr->dirty_offset_end = modified_region_end;
        }
    }

    /* All done */
end:
    ;
}


/** Please see header for specification */
PUBLIC EMERALD_API ral_program_block_buffer ral_program_block_buffer_create(ral_context               context,
                                                                            ral_program               program,
                                                                            system_hashed_ansi_string block_name)
{
    ral_program_block_type     block_type;
    bool                       is_successful        = true;
    _ral_program_block_buffer* new_block_buffer_ptr = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(program != nullptr,
                          "Input RAL program instance is NULL");

        goto end;
    }

    if (!ral_program_is_block_defined(program,
                                      block_name) )
    {
        system_hashed_ansi_string program_name = nullptr;

        ral_program_get_property(program,
                                 RAL_PROGRAM_PROPERTY_NAME,
                                &program_name);

        LOG_ERROR("Block [%s] is not defined for program [%s]",
                  system_hashed_ansi_string_get_buffer(block_name),
                  system_hashed_ansi_string_get_buffer(program_name) );

        goto end;
    }

    ral_program_get_block_property(program,
                                   block_name,
                                   RAL_PROGRAM_BLOCK_PROPERTY_TYPE,
                                  &block_type);

    /* Create new block instance */
    new_block_buffer_ptr = new (std::nothrow) _ral_program_block_buffer;

    ASSERT_ALWAYS_SYNC(new_block_buffer_ptr != nullptr,
                       "Out of memory");

    new_block_buffer_ptr->context_ral = context;
    new_block_buffer_ptr->name        = block_name;
    new_block_buffer_ptr->program_ral = program;

    if (new_block_buffer_ptr != nullptr)
    {
        ral_buffer_create_info new_block_create_info;
        uint32_t               new_block_size = 0;

        ral_program_get_block_property(program,
                                       block_name,
                                       RAL_PROGRAM_BLOCK_PROPERTY_SIZE,
                                      &new_block_size);

        new_block_buffer_ptr->size = new_block_size;
        new_block_buffer_ptr->type = block_type;

        if (block_type == RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER)
        {
            new_block_buffer_ptr->data = new (std::nothrow) unsigned char[new_block_size];

            if (new_block_buffer_ptr->data == nullptr)
            {
                ASSERT_ALWAYS_SYNC(new_block_buffer_ptr->data != nullptr,
                                   "Out of memory");

                is_successful = false;
                goto end;
            }

            /* Set all uniforms to zeroes. */
            memset(new_block_buffer_ptr->data,
                   0,
                   new_block_size);

            new_block_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
            new_block_create_info.parent_buffer    = nullptr;
            new_block_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
            new_block_create_info.size             = new_block_size;
            new_block_create_info.start_offset     = 0;
            new_block_create_info.usage_bits       = RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            new_block_create_info.user_queue_bits  = 0xFFFFFFFF;

            ral_context_create_buffers(context,
                                       1,
                                      &new_block_create_info,
                                      &new_block_buffer_ptr->buffer_ral);

            /* Force a data sync next time the buffer is accessed */
            new_block_buffer_ptr->dirty_offset_end   = new_block_size;
            new_block_buffer_ptr->dirty_offset_start = 0;
        }
        else
        {
            new_block_buffer_ptr->buffer_ral         = nullptr;
            new_block_buffer_ptr->data               = nullptr;
            new_block_buffer_ptr->dirty_offset_end   = DIRTY_OFFSET_UNUSED;
            new_block_buffer_ptr->dirty_offset_start = DIRTY_OFFSET_UNUSED;
        }
    }

end:
    if (!is_successful)
    {
        ral_program_block_buffer_release( (ral_program_block_buffer) new_block_buffer_ptr);

        new_block_buffer_ptr = nullptr;
    }

    return (ral_program_block_buffer) new_block_buffer_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_program_block_buffer_get_property(ral_program_block_buffer          block_buffer,
                                                              ral_program_block_buffer_property property,
                                                              void*                             out_result_ptr)
{
    _ral_program_block_buffer* block_buffer_ptr = (_ral_program_block_buffer*) block_buffer;

    if (block_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(block_buffer != nullptr,
                          "Input ral_program_block_buffer instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL:
        {
            *(ral_buffer*) out_result_ptr = block_buffer_ptr->buffer_ral;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_program_block_buffer_property value.");
        }
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_program_block_buffer_release(ral_program_block_buffer block_buffer)
{
    delete (_ral_program_block_buffer*) block_buffer;
}

/* Please see header for spec */
PUBLIC EMERALD_API void ral_program_block_buffer_set_arrayed_variable_value(ral_program_block_buffer block_buffer,
                                                                            unsigned int             block_variable_offset,
                                                                            const void*              src_data,
                                                                            unsigned int             src_data_size,
                                                                            unsigned int             dst_array_start_index,
                                                                            unsigned int             dst_array_item_count)
{
    _ral_program_block_buffer_set_variable_value((_ral_program_block_buffer*) block_buffer,
                                                 block_variable_offset,
                                                 src_data,
                                                 src_data_size,
                                                 dst_array_start_index,
                                                 dst_array_item_count);
}

/* Please see header for spec */
PUBLIC EMERALD_API void ral_program_block_buffer_set_nonarrayed_variable_value(ral_program_block_buffer block_buffer,
                                                                               unsigned int             block_variable_offset,
                                                                               const void*              src_data,
                                                                               unsigned int             src_data_size)
{
    _ral_program_block_buffer_set_variable_value((_ral_program_block_buffer*) block_buffer,
                                                 block_variable_offset,
                                                 src_data,
                                                 src_data_size,
                                                 0,  /* dst_array_start */
                                                 1); /* dst_array_item_count */
}

/* Please see header for spec */
PUBLIC EMERALD_API void ral_program_block_buffer_sync(ral_program_block_buffer block_buffer)
{
    _ral_program_block_buffer*                                           block_buffer_ptr = (_ral_program_block_buffer*) block_buffer;
    ral_buffer_client_sourced_update_info                                bo_update_info;
    std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > bo_update_ptrs;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(block_buffer_ptr->dirty_offset_end != DIRTY_OFFSET_UNUSED && block_buffer_ptr->dirty_offset_start != DIRTY_OFFSET_UNUSED ||
                      block_buffer_ptr->dirty_offset_end == DIRTY_OFFSET_UNUSED && block_buffer_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED,
                      "Sanity check failed");

    /* Anything to refresh? */
    if (block_buffer_ptr->dirty_offset_end   == DIRTY_OFFSET_UNUSED &&
        block_buffer_ptr->dirty_offset_start == DIRTY_OFFSET_UNUSED)
    {
        /* Nothing to synchronize */
        goto end;
    }

    bo_update_info.data         = block_buffer_ptr->data             + block_buffer_ptr->dirty_offset_start;
    bo_update_info.data_size    = block_buffer_ptr->dirty_offset_end - block_buffer_ptr->dirty_offset_start;
    bo_update_info.start_offset = block_buffer_ptr->dirty_offset_start;

    bo_update_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&bo_update_info,
                                                                                    NullDeleter<ral_buffer_client_sourced_update_info>() ));

    ral_buffer_set_data_from_client_memory(block_buffer_ptr->buffer_ral,
                                           bo_update_ptrs,
                                           false, /* async               */
                                           false  /* sync_other_contexts */); /* NOTE: in the future, we may need to make this arg value customizable */

#if 0
    if (block_ptr->is_intel_driver)
    {
        /* Sigh. */
        block_ptr->pGLFinish();
    }
#endif

    /* Reset the offsets */
    block_buffer_ptr->dirty_offset_end   = DIRTY_OFFSET_UNUSED;
    block_buffer_ptr->dirty_offset_start = DIRTY_OFFSET_UNUSED;

    /* All done */
end:
    ;
}
