/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"

/** TODO */
struct _system_resizable_vector
{
    size_t capacity;
    size_t element_size;
    void*  elements;
    size_t n_elements;
    void*  swap_helper; /* is allocated space for holding 1 element */

    system_read_write_mutex access_mutex;
};

/** Initializes a resizable vector descriptor.
 *
 *  @param _resizable_vector_descriptor* Descriptor to initialize.
 *  @param size_t                        Capacity to use.
 *  @param element_size                  Element size to use.
 */
PRIVATE void _init_resizable_vector(_system_resizable_vector* descriptor,
                                    size_t                    capacity,
                                    size_t                    element_size,
                                    bool                      should_be_thread_safe)
{
    descriptor->capacity     = capacity;
    descriptor->elements     = new (std::nothrow) char[capacity * element_size];
    descriptor->element_size = element_size;
    descriptor->n_elements   = 0;
    descriptor->swap_helper  = new (std::nothrow) char[element_size];

    if (should_be_thread_safe)
    {
        descriptor->access_mutex = system_read_write_mutex_create();
    }
    else
    {
        descriptor->access_mutex = NULL;
    }
}

/** Releases a resizable vector descriptor. Do not use the descriptor after this call.
 *
 *  @param _resizable_vector_descriptor Resizable vector descriptor to release.
 */
PRIVATE void _deinit_resizable_vector(_system_resizable_vector* descriptor)
{
    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_release(descriptor->access_mutex);

        descriptor->access_mutex = NULL;
    }

    if (descriptor->elements != NULL)
    {
        delete [] (char*) descriptor->elements;

        descriptor->elements = NULL;
    }

    if (descriptor->swap_helper != NULL)
    {
        delete [] (char*) descriptor->swap_helper;

        descriptor->swap_helper = NULL;
    }

    delete descriptor;
}

/** Resizes a resizable vector object.
 *
 *  NOTE: This function overrides capacity and elements fields of the descriptor. Make
 *        sure to cache them if needed. 
 *
 *  @param system_resizable_vector Resizable vector to resize.
 *  @param size_t                  New capacity to use for the object.
 *
 */
PRIVATE void _system_resizable_vector_resize(system_resizable_vector resizable_vector,
                                             size_t                  new_capacity)
{
    _system_resizable_vector* vector_ptr   = (_system_resizable_vector*) resizable_vector;
    char*                     new_elements = new char[new_capacity * vector_ptr->element_size];

    if (vector_ptr->access_mutex != NULL)
    {
        ASSERT_DEBUG_SYNC(system_read_write_mutex_is_write_locked(vector_ptr->access_mutex),
                          "About to resize a vector without RW mutex locked for write operations");
    }

    ASSERT_DEBUG_SYNC(new_capacity > vector_ptr->capacity,
                      "The new capacity requested is smaller than existing capacity.");

    if (new_capacity > vector_ptr->capacity)
    {
        memcpy(new_elements,
               vector_ptr->elements,
               vector_ptr->element_size * vector_ptr->capacity);

        delete [] (char*) vector_ptr->elements;

        vector_ptr->capacity = new_capacity;
        vector_ptr->elements = new_elements;
    }
}


/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_clear(system_resizable_vector vector)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) vector;

    vector_ptr->n_elements = 0;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create(size_t capacity,
                                                                          bool   should_be_thread_safe)
{
    _system_resizable_vector* vector_ptr = new (std::nothrow) _system_resizable_vector;

    ASSERT_ALWAYS_SYNC(vector_ptr != NULL,
                       "Out of memory");

    _init_resizable_vector(vector_ptr,
                           capacity,
                           sizeof(void*),
                           should_be_thread_safe);

    return (system_resizable_vector) vector_ptr;
}

/** Please see header for specifiaction */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create_copy(system_resizable_vector vector)
{
    const _system_resizable_vector* existing_vector_ptr = (const _system_resizable_vector*) vector;
    _system_resizable_vector*       result              = new (std::nothrow) _system_resizable_vector;

    ASSERT_ALWAYS_SYNC(result != NULL,
                       "Out of memory");

    if (existing_vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(existing_vector_ptr->access_mutex,
                                     ACCESS_READ);
    }

    result->access_mutex = (existing_vector_ptr->access_mutex != NULL) ? system_read_write_mutex_create()
                                                                       : NULL;
    result->capacity     = existing_vector_ptr->capacity;
    result->elements     = new (std::nothrow) char[existing_vector_ptr->n_elements * existing_vector_ptr->element_size];
    result->element_size = existing_vector_ptr->element_size;
    result->n_elements   = existing_vector_ptr->n_elements;
    result->swap_helper  = new (std::nothrow) char[existing_vector_ptr->element_size];

    if (result->elements    == NULL ||
        result->swap_helper == NULL)
    {
        system_resizable_vector_release( (system_resizable_vector) result);
        result = NULL;

        goto end;
    }

    memcpy(result->elements,
           existing_vector_ptr->elements,
           existing_vector_ptr->n_elements * existing_vector_ptr->element_size);

end:
    if (existing_vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(existing_vector_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return (system_resizable_vector) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_empty(system_resizable_vector resizable_vector)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    vector_ptr->n_elements = 0;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_delete_element_at(system_resizable_vector resizable_vector,
                                                                  size_t                  index)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;
    bool                      result     = false;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    ASSERT_DEBUG_SYNC(vector_ptr->n_elements > index,
                      "Requested element (%d) unavailable",
                      index);

    if (vector_ptr->n_elements > index)
    {
        if (vector_ptr->n_elements > index + 1)
        {
            char* removed_element_ptr = (char*) vector_ptr->elements + vector_ptr->element_size * index;
            char* start_element_ptr   = (char*) removed_element_ptr  + vector_ptr->element_size;
            char* end_element_ptr     = (char*) vector_ptr->elements + vector_ptr->n_elements * vector_ptr->element_size;

            memmove(removed_element_ptr,
                    start_element_ptr,
                    end_element_ptr - start_element_ptr);
        }

        vector_ptr->n_elements--;
        result = true;
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_resizable_vector_find(system_resizable_vector resizable_vector,
                                                       const void*             item)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;
    size_t                    n_elements = vector_ptr->n_elements;
    size_t                    result     = ITEM_NOT_FOUND;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_READ);
    }

    for (size_t n_element = 0;
                n_element < n_elements;
              ++n_element)
    {
        if (memcmp( (char*) vector_ptr->elements + n_element * vector_ptr->element_size,
                   &item,
                   vector_ptr->element_size) == 0)
        {
            result = n_element;

            break;
        }
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_get_property(system_resizable_vector          resizable_vector,
                                                             system_resizable_vector_property property,
                                                             void*                            out_result_ptr)
{
    _system_resizable_vector* resizable_vector_ptr = (_system_resizable_vector*) resizable_vector;

    switch (property)
    {
        case SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY:
        {
            *(void**) out_result_ptr = ((_system_resizable_vector*) resizable_vector)->elements;

            break;
        }

        case SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS:
        {
            if (resizable_vector_ptr->access_mutex != NULL)
            {
                system_read_write_mutex_lock(resizable_vector_ptr->access_mutex,
                                             ACCESS_READ);
            }

            *(unsigned int*) out_result_ptr = resizable_vector_ptr->n_elements;

            if (resizable_vector_ptr->access_mutex != NULL)
            {
                system_read_write_mutex_unlock(resizable_vector_ptr->access_mutex,
                                               ACCESS_READ);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_resizable_vector_property value");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_get_element_at(system_resizable_vector resizable_vector,
                                                               size_t                  index,
                                                               void*                   result)
{
    bool                      bool_result = false;
    _system_resizable_vector* vector_ptr  = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_READ);
    }

    if (vector_ptr->n_elements > index)
    {
        memcpy(result,
               (char*) vector_ptr->elements + vector_ptr->element_size * index,
               vector_ptr->element_size);

        bool_result = true;
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return bool_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_insert_element_at(system_resizable_vector resizable_vector,
                                                                  size_t                  index,
                                                                  void*                   element)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    ASSERT_DEBUG_SYNC(index < vector_ptr->capacity,
                      "Cannot insert element at index %d - capacity would be exceeded",
                      index);

    if (index < vector_ptr->capacity)
    {
        if (vector_ptr->n_elements + 1 >= vector_ptr->capacity)
        {
            _system_resizable_vector_resize(resizable_vector,
                                            (vector_ptr->capacity << 1)
                                           );
        }

        if (vector_ptr->n_elements != index)
        {
            char* src_ptr = (char*) vector_ptr->elements + index * vector_ptr->element_size;

            #ifdef _DEBUG
            {
                char*  dst_ptr         =  (char*) vector_ptr->elements + (index + 1)            * vector_ptr->element_size;
                size_t n_bytes_to_move = ((char*) vector_ptr->elements + vector_ptr->n_elements * vector_ptr->element_size - src_ptr);

                ASSERT_DEBUG_SYNC(dst_ptr + n_bytes_to_move <= (char*)vector_ptr->elements +
                                                                      vector_ptr->capacity * vector_ptr->element_size,
                                  "Sanity check failure");
            }
            #endif

            char* end_element_ptr = (char*) vector_ptr->elements + vector_ptr->n_elements * vector_ptr->element_size;
            char* data_ptr        = end_element_ptr - vector_ptr->element_size;

            /* Move elements from [index] onward till the last entry, one element at a time */
            for (char* traveller_ptr  = end_element_ptr;
                       traveller_ptr != src_ptr;
                       traveller_ptr -= vector_ptr->element_size, data_ptr -= vector_ptr->element_size)
            {
                memcpy(traveller_ptr,
                       data_ptr,
                       vector_ptr->element_size);
            }

            /* We have a free slot at [index] at this point */
        }

        memcpy((char*) vector_ptr->elements + index * vector_ptr->element_size,
               &element,
               vector_ptr->element_size);

        ++vector_ptr->n_elements;
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_is_equal(system_resizable_vector resizable_vector_1,
                                                         system_resizable_vector resizable_vector_2)
{
    _system_resizable_vector* vector_ptr_1 = (_system_resizable_vector*) resizable_vector_1;
    _system_resizable_vector* vector_ptr_2 = (_system_resizable_vector*) resizable_vector_2;
    bool                      result       = false;

    if (vector_ptr_1->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr_1->access_mutex,
                                     ACCESS_READ);
    }

    if (vector_ptr_2->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr_2->access_mutex,
                                     ACCESS_READ);
    }

    if (vector_ptr_1->n_elements   == vector_ptr_2->n_elements   &&
        vector_ptr_1->element_size == vector_ptr_2->element_size &&
        vector_ptr_1->capacity     == vector_ptr_2->capacity)
    {
        size_t n_element = 0;

        if (memcmp(vector_ptr_1->elements,
                   vector_ptr_2->elements,
                   vector_ptr_1->element_size * vector_ptr_1->n_elements) == 0)
        {
            result = true;
        }
    }

    if (vector_ptr_1->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr_1->access_mutex,
                                       ACCESS_READ);
    }

    if (vector_ptr_2->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr_2->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_lock(system_resizable_vector             vector,
                                                     system_read_write_mutex_access_type access_type)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) vector;

    ASSERT_DEBUG_SYNC(vector_ptr->access_mutex != NULL,
                      "Vector not initialized for thread-safe access");

    system_read_write_mutex_lock(vector_ptr->access_mutex,
                                 access_type);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_pop(system_resizable_vector resizable_vector,
                                                    void*                   result)
{
    bool                      bool_result = false;
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    if (vector_ptr->n_elements >= 1)
    {
        memcpy(result,
               (char*) vector_ptr->elements + (vector_ptr->n_elements - 1) * vector_ptr->element_size,
               vector_ptr->element_size);

        --vector_ptr->n_elements;
        bool_result = true;
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }

    return bool_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_push(system_resizable_vector resizable_vector,
                                                     void*                    element)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    if (vector_ptr->n_elements < vector_ptr->capacity)
    {
        memcpy( (char*) vector_ptr->elements + vector_ptr->n_elements * vector_ptr->element_size,
                &element,
                 vector_ptr->element_size);

        ++vector_ptr->n_elements;
    }
    else
    {
        _system_resizable_vector_resize(resizable_vector,
                                        (vector_ptr->capacity << 1));

        // Store new element
        memcpy( (char*) vector_ptr->elements + (vector_ptr->n_elements) * vector_ptr->element_size,
                &element,
                 vector_ptr->element_size);

        vector_ptr->n_elements++;
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_release(system_resizable_vector resizable_vector)
{
    _deinit_resizable_vector( (_system_resizable_vector*) resizable_vector);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_set_element_at(system_resizable_vector resizable_vector,
                                                               size_t                  index,
                                                               void*                   element)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    ASSERT_DEBUG_SYNC(vector_ptr->n_elements > index,
                      "Element %d unavailable", index);

    if (vector_ptr->n_elements > index)
    {
        memcpy( (char*) vector_ptr->elements + vector_ptr->element_size * index,
               &element,
                vector_ptr->element_size);
    }

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** TODO 
 *
 *  comparator_func_ptr() = operator<
 */
PRIVATE void _system_resizable_vector_sort_recursive(system_resizable_vector resizable_vector,
                                                     bool                  (*comparator_func_ptr)(const void*, const void*),
                                                     int                     start_index,
                                                     int                     end_index)
{
    /* Based on http://www.mycstutorials.com/articles/sorting/quicksort */
    int   left   = start_index; // i
    void* pivot  = NULL;
    bool  result = false;
    int   right  = end_index;   // j
    void* value  = NULL;

    if (end_index - start_index >= 1)
    {
        result = system_resizable_vector_get_element_at(resizable_vector,
                                                        start_index,
                                                       &pivot);

        ASSERT_DEBUG_SYNC(result,
                          "Could not retrieve reference element [%d] from the vector",
                          start_index);

        while (right > left)
        {
            while (true)
            {
                if (left > end_index)
                {
                    break;
                }

                result = system_resizable_vector_get_element_at(resizable_vector,
                                                                left,
                                                               &value);

                ASSERT_DEBUG_SYNC(result,
                                 "system_resizable_vector_get_element_at() failed.");

                if (comparator_func_ptr(value, pivot) && right > left)
                {
                    ++left;
                }
                else
                {
                    break;
                }
            } /* while (true) */

            while (true)
            {
                if (right < start_index)
                {
                    break;
                }

                result = system_resizable_vector_get_element_at(resizable_vector,
                                                                right,
                                                               &value);

                ASSERT_DEBUG_SYNC(result,
                                  "system_resizable_vector_get_element_at() failed.");

                if (!comparator_func_ptr(value, pivot) && right >= left)
                {
                    --right;
                }
                else
                {
                    break;
                }
            } /* while (true) */

            if (right > left)
            {
                system_resizable_vector_swap(resizable_vector,
                                             left,
                                             right);
            }
        } /* while (right > left) */

        if (right >= 0)
        {
            system_resizable_vector_swap(resizable_vector,
                                         start_index,
                                         right);

            _system_resizable_vector_sort_recursive(resizable_vector,
                                                    comparator_func_ptr,
                                                    start_index,
                                                    right - 1);
            _system_resizable_vector_sort_recursive(resizable_vector,
                                                    comparator_func_ptr,
                                                    right + 1,
                                                    end_index);
        }
    } /* if (end_index - start_index >= 1) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_sort(system_resizable_vector resizable_vector,
                                                     bool                  (*comparator_func_ptr)(const void*, const void*) )
{
    uint32_t                  n_vector_items = 0;
    _system_resizable_vector* vector_ptr     = (_system_resizable_vector*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    system_resizable_vector_get_property(resizable_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_vector_items);

    _system_resizable_vector_sort_recursive(resizable_vector,
                                            comparator_func_ptr,
                                            0,
                                            n_vector_items - 1);

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_swap(system_resizable_vector resizable_vector,
                                                     uint32_t                index_a,
                                                     uint32_t                index_b)
{
    _system_resizable_vector* resizable_vector_ptr = (_system_resizable_vector*) resizable_vector;
    bool                      result               = true;

    ASSERT_DEBUG_SYNC(index_a < resizable_vector_ptr->n_elements,
                      "A index [%d] is too large [%d]",
                      index_a,
                      resizable_vector_ptr->n_elements);
    ASSERT_DEBUG_SYNC(index_b < resizable_vector_ptr->n_elements,
                      "B index [%d] is too large [%d]",
                      index_b,
                      resizable_vector_ptr->n_elements);

    if (resizable_vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(resizable_vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    memcpy(       resizable_vector_ptr->swap_helper,
           (char*)resizable_vector_ptr->elements + resizable_vector_ptr->element_size * index_a,
           resizable_vector_ptr->element_size);
    memcpy((char*)resizable_vector_ptr->elements + resizable_vector_ptr->element_size * index_a,
           (char*)resizable_vector_ptr->elements + resizable_vector_ptr->element_size * index_b,
           resizable_vector_ptr->element_size);
    memcpy((char*)resizable_vector_ptr->elements + resizable_vector_ptr->element_size * index_b,
           resizable_vector_ptr->swap_helper,
           resizable_vector_ptr->element_size);

    if (resizable_vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(resizable_vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_unlock(system_resizable_vector             vector,
                                                       system_read_write_mutex_access_type access_type)
{
    _system_resizable_vector* vector_ptr = (_system_resizable_vector*) vector;

    ASSERT_DEBUG_SYNC(vector_ptr->access_mutex != NULL,
                      "Vector not initialized for thread-safe access");

    system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                   access_type);
}
