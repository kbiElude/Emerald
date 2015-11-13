/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ral/ral_command_buffer.h"
#include "system/system_linear_alloc_pin.h"
#include "system/system_resizable_vector.h"


typedef struct _ral_command_buffer
{
    system_linear_alloc_pin allocator;
    system_resizable_vector operations;
    int                     usage_bits;

    _ral_command_buffer(int in_usage_bits)
    {
        allocator  = system_linear_alloc_pin_create(sizeof(void*), /* entry_size            */
                                                    16,            /* n_entries_to_prealloc */
                                                    1);            /* n_pins_to_prealloc    */
        operations = system_resizable_vector_create(16);           /* capacity              */
        usage_bits = in_usage_bits;

        ASSERT_DEBUG_SYNC(allocator != NULL,
                          "Could not instantiate linear memory allocator");
        ASSERT_DEBUG_SYNC(operations != NULL,
                          "Could not instantiate op vector");
    }

    ~_ral_command_buffer()
    {
        if (allocator != NULL)
        {
            system_linear_alloc_pin_release(allocator);

            allocator = NULL;
        } /* if (allocator != NULL) */

        if (operations != NULL)
        {
            system_resizable_vector_release(operations);

            operations = NULL;
        } /* if (operations != NULL) */
    }
} _ral_command_buffer;


/** Please see header for specification */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context context,
                                                    int         usage_bits)
{
    _ral_command_buffer* new_command_buffer_ptr = new (std::nothrow) _ral_command_buffer(usage_bits);

    ASSERT_ALWAYS_SYNC(new_command_buffer_ptr != NULL,
                       "Out of memory");

    if (new_command_buffer_ptr != NULL)
    {
        // ..
    }

    return (ral_command_buffer) new_command_buffer_ptr;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer)
{
    ASSERT_DEBUG_SYNC(command_buffer != NULL,
                      "Input ral_command_buffer instance is NULL");

    delete (_ral_command_buffer*) command_buffer;
}