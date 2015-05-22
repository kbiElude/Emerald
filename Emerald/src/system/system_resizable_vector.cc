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
struct _resizable_vector_descriptor
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
PRIVATE void _init_resizable_vector_descriptor(_resizable_vector_descriptor* descriptor,
                                               size_t                        capacity,
                                               size_t                        element_size,
                                               bool                          should_be_thread_safe)
{
    descriptor->capacity     = capacity;
    descriptor->elements     = new (std::nothrow) unsigned char[capacity * element_size];
    descriptor->element_size = element_size;
    descriptor->n_elements   = 0;
    descriptor->swap_helper  = new (std::nothrow) unsigned char[element_size];

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
PRIVATE void _deinit_resizable_vector_descriptor(_resizable_vector_descriptor* descriptor)
{
    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_release(descriptor->access_mutex);

        descriptor->access_mutex = NULL;
    }

    if (descriptor->elements != NULL)
    {
        delete [] descriptor->elements;

        descriptor->elements = NULL;
    }

    if (descriptor->swap_helper != NULL)
    {
        delete [] descriptor->swap_helper;

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
    _resizable_vector_descriptor* descriptor   = (_resizable_vector_descriptor*) resizable_vector;
    unsigned char*                new_elements = new unsigned char[new_capacity * descriptor->element_size];

    if (descriptor->access_mutex != NULL)
    {
        ASSERT_DEBUG_SYNC(system_read_write_mutex_is_write_locked(descriptor->access_mutex),
                          "About to resize a vector without RW mutex locked for write operations");
    }

    ASSERT_DEBUG_SYNC(new_capacity > descriptor->capacity,
                      "The new capacity requested is smaller than existing capacity.");

    if (new_capacity > descriptor->capacity)
    {
        CopyMemory(new_elements,
                   descriptor->elements,
                   descriptor->element_size * descriptor->capacity);

        delete [] descriptor->elements;

        descriptor->capacity = new_capacity;
        descriptor->elements = new_elements;
    }
}


/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_clear(__in __notnull system_resizable_vector vector)
{
    _resizable_vector_descriptor* vector_ptr = (_resizable_vector_descriptor*) vector;

    vector_ptr->n_elements = 0;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create(__in size_t capacity,
                                                                          __in bool   should_be_thread_safe)
{
    _resizable_vector_descriptor* result = new _resizable_vector_descriptor;

    _init_resizable_vector_descriptor(result,
                                      capacity,
                                      sizeof(void*),
                                      should_be_thread_safe);

    return (system_resizable_vector) result;
}

/** Please see header for specifiaction */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create_copy(__in __notnull system_resizable_vector vector)
{
    const _resizable_vector_descriptor* existing_vector_ptr = (const _resizable_vector_descriptor*) vector;
    _resizable_vector_descriptor*       result              = new _resizable_vector_descriptor;

    if (existing_vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(existing_vector_ptr->access_mutex, ACCESS_READ);
    }

    result->access_mutex = (existing_vector_ptr->access_mutex != NULL) ? system_read_write_mutex_create() : NULL;
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
        system_read_write_mutex_unlock(existing_vector_ptr->access_mutex, ACCESS_READ);
    }

    return (system_resizable_vector) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_empty(__in system_resizable_vector resizable_vector)
{
    _resizable_vector_descriptor* vector_ptr = (_resizable_vector_descriptor*) resizable_vector;

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
PUBLIC EMERALD_API bool system_resizable_vector_delete_element_at(__in system_resizable_vector resizable_vector,
                                                                  __in size_t                  index)
{
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;
    bool                          result     = false;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    ASSERT_DEBUG_SYNC(descriptor->n_elements > index,
                      "Requested element (%d) unavailable",
                      index);

    if (descriptor->n_elements > index)
    {
        if (descriptor->n_elements > index + 1)
        {
            char* removed_element_ptr = (char*) descriptor->elements + descriptor->element_size * index;
            char* start_element_ptr   = (char*) removed_element_ptr  + descriptor->element_size;
            char* end_element_ptr     = (char*) descriptor->elements + descriptor->n_elements * descriptor->element_size;

            MoveMemory(removed_element_ptr,
                       start_element_ptr,
                       end_element_ptr - start_element_ptr);
        }

        descriptor->n_elements--;
        result = true;
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_resizable_vector_find(__in system_resizable_vector resizable_vector,
                                                       __in void*                   item)
{
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;
    size_t                        n_elements = descriptor->n_elements;
    size_t                        result     = ITEM_NOT_FOUND;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    for (size_t n_element = 0;
                n_element < n_elements;
              ++n_element)
    {
        if (memcmp( (char*) descriptor->elements + n_element * descriptor->element_size,
                   &item,
                   descriptor->element_size) == 0)
        {
            result = n_element;

            break;
        }
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API unsigned int system_resizable_vector_get_amount_of_elements(__in system_resizable_vector resizable_vector)
{
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;
    unsigned int                  result     = 0;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    result = descriptor->n_elements;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API const void* system_resizable_vector_get_array(__in __notnull system_resizable_vector resizable_vector)
{
    ASSERT_DEBUG_SYNC(((_resizable_vector_descriptor*) resizable_vector)->access_mutex == NULL,
                      "get_array() request for a thread-safe resizable vector instance");

    return (const void*) ((_resizable_vector_descriptor*) resizable_vector)->elements;
}

/** Please see header for specification */
PUBLIC EMERALD_API unsigned int system_resizable_vector_get_capacity(__in system_resizable_vector resizable_vector)
{
    ASSERT_DEBUG_SYNC(((_resizable_vector_descriptor*) resizable_vector)->access_mutex == NULL,
                      "get_capacity() request for a thread-safe resizable vector instance");

    return (unsigned int) ((_resizable_vector_descriptor*) resizable_vector)->capacity;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_get_element_at(__in            system_resizable_vector resizable_vector,
                                                                               size_t                  index,
                                                               __out __notnull void*                   result)
{
    bool                          bool_result = false;
    _resizable_vector_descriptor* descriptor  = (_resizable_vector_descriptor*) resizable_vector;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    if (descriptor->n_elements > index)
    {
        memcpy(result,
               (char*) descriptor->elements + descriptor->element_size * index,
               descriptor->element_size);

        bool_result = true;
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    ASSERT_DEBUG_SYNC(bool_result,
                      "Could not retrieve requested vector element");

    return bool_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_insert_element_at(__in system_resizable_vector resizable_vector,
                                                                       size_t                  index,
                                                                       void*                   element)
{
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    ASSERT_DEBUG_SYNC(index < descriptor->capacity,
                      "Cannot insert element at index %d - capacity would be exceeded",
                      index);

    if (index < descriptor->capacity)
    {
        if (descriptor->n_elements + 1 >= descriptor->capacity)
        {
            _system_resizable_vector_resize(resizable_vector,
                                            (descriptor->capacity << 1)
                                           );
        }

        if (descriptor->n_elements != index)
        {
            char* src_ptr = (char*) descriptor->elements + index * descriptor->element_size;

            #ifdef _DEBUG
            {
                char*  dst_ptr         =  (char*) descriptor->elements + (index + 1)            * descriptor->element_size;
                size_t n_bytes_to_move = ((char*) descriptor->elements + descriptor->n_elements * descriptor->element_size - src_ptr);

                ASSERT_DEBUG_SYNC(dst_ptr + n_bytes_to_move <= (char*)descriptor->elements +
                                                                      descriptor->capacity * descriptor->element_size,
                                  "Sanity check failure");
            }
            #endif

            char* end_element_ptr = (char*) descriptor->elements + descriptor->n_elements * descriptor->element_size;
            char* data_ptr        = end_element_ptr - descriptor->element_size;

            /* Move elements from [index] onward till the last entry, one element at a time */
            for (char* traveller_ptr  = end_element_ptr;
                       traveller_ptr != src_ptr;
                       traveller_ptr -= descriptor->element_size, data_ptr -= descriptor->element_size)
            {
                memcpy(traveller_ptr,
                       data_ptr,
                       descriptor->element_size);
            }

            /* We have a free slot at [index] at this point */
        }

        memcpy((char*) descriptor->elements + index * descriptor->element_size,
               &element,
               descriptor->element_size);

        ++descriptor->n_elements;
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_is_equal(__in system_resizable_vector resizable_vector_1,
                                                         __in system_resizable_vector resizable_vector_2)
{
    _resizable_vector_descriptor* descriptor_1 = (_resizable_vector_descriptor*) resizable_vector_1;
    _resizable_vector_descriptor* descriptor_2 = (_resizable_vector_descriptor*) resizable_vector_2;
    bool                          result       = false;

    if (descriptor_1->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor_1->access_mutex,
                                     ACCESS_READ);
    }

    if (descriptor_2->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor_2->access_mutex,
                                     ACCESS_READ);
    }

    if (descriptor_1->n_elements   == descriptor_2->n_elements   &&
        descriptor_1->element_size == descriptor_2->element_size &&
        descriptor_1->capacity     == descriptor_2->capacity)
    {
        size_t n_element = 0;

        if (memcmp(descriptor_1->elements,
                   descriptor_2->elements,
                   descriptor_1->element_size * descriptor_1->n_elements) == 0)
        {
            result = true;
        }
    }

    if (descriptor_1->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor_1->access_mutex,
                                       ACCESS_READ);
    }

    if (descriptor_2->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor_2->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_lock(__in __notnull system_resizable_vector             vector,
                                                     __in           system_read_write_mutex_access_type access_type)
{
    _resizable_vector_descriptor* vector_ptr = (_resizable_vector_descriptor*) vector;

    ASSERT_DEBUG_SYNC(vector_ptr->access_mutex != NULL,
                      "Vector not initialized for thread-safe access");

    system_read_write_mutex_lock(vector_ptr->access_mutex,
                                 access_type);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_pop(__in            system_resizable_vector resizable_vector,
                                                    __out __notnull void*                   result)
{
    bool                          bool_result = false;
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    if (descriptor->n_elements >= 1)
    {
        memcpy(result,
               (char*) descriptor->elements + (descriptor->n_elements - 1) * descriptor->element_size,
               descriptor->element_size);

        --descriptor->n_elements;
        bool_result = true;
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }

    return bool_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_push(__in     system_resizable_vector resizable_vector,
                                                     __in_opt void*                    element)
{
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    if (descriptor->n_elements < descriptor->capacity)
    {
        CopyMemory( (char*) descriptor->elements + descriptor->n_elements * descriptor->element_size,
                  &element,
                   descriptor->element_size);

        ++descriptor->n_elements;
    }
    else
    {
        _system_resizable_vector_resize(resizable_vector,
                                        (descriptor->capacity << 1));

        // Store new element
        CopyMemory( (char*) descriptor->elements + (descriptor->n_elements) * descriptor->element_size,
                   &element,
                   descriptor->element_size);

        descriptor->n_elements++;
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_release(__in __deallocate(mem) system_resizable_vector resizable_vector)
{
    _deinit_resizable_vector_descriptor( (_resizable_vector_descriptor*) resizable_vector);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_set_element_at(__in             system_resizable_vector resizable_vector,
                                                               __in             size_t                  index,
                                                               __in __maybenull void*                   element)
{
    _resizable_vector_descriptor* descriptor = (_resizable_vector_descriptor*) resizable_vector;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    ASSERT_DEBUG_SYNC(descriptor->n_elements > index,
                      "Element %d unavailable", index);

    if (descriptor->n_elements > index)
    {
        CopyMemory( (char*) descriptor->elements + descriptor->element_size * index,
                   &element,
                   descriptor->element_size);
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** TODO 
 *
 *  comparator_func_ptr() = operator<
 */
PRIVATE void _system_resizable_vector_sort_recursive(__in __notnull system_resizable_vector resizable_vector,
                                                                    int                   (*comparator_func_ptr)(void*, void*),
                                                                    int start_index,
                                                                    int end_index)
{
    if (start_index == end_index)
    {
        return;
    }

    /* Shamelessly adapted from http://pl.wikisource.org/wiki/Sortowanie_szybkie/kod <ANSI C, normal version> */
    int   x         = (end_index + start_index) >> 1; //rand() % (end_index - start_index) + start_index + 1;
    int   left      = start_index; // i
    int   right     = end_index;   // j
    bool  result    = false;
    void* element_x = NULL;

    result = system_resizable_vector_get_element_at(resizable_vector,
                                                    x,
                                                   &element_x);

    ASSERT_DEBUG_SYNC(result,
                      "Could not retrieve reference element [%d] from the vector", x);

    do
    {
        /* while (T[i] < x) ++i; */
        void* element = NULL;

        while (true)
        {
            result = system_resizable_vector_get_element_at(resizable_vector,
                                                            left,
                                                           &element);

            ASSERT_DEBUG_SYNC(result,
                              "Could not retrieve %dth element from the vector",
                              left);

            if (!(comparator_func_ptr(element,
                                      element_x) < 0))
            {
                break;
            }
            else
            {
                if (left < end_index)
                {
                    ++left;
                }
                else
                {
                    break;
                }
            }
        }

        /* While (T[j] > x) --j; */
        while (true)
        {
            result = system_resizable_vector_get_element_at(resizable_vector,
                                                            right,
                                                           &element);

            ASSERT_DEBUG_SYNC(result,
                              "Could not retrieve %dth element from the vector",
                              right);

            if (!(comparator_func_ptr(element,
                                      element_x) > 0))
            {
                break;
            }
            else
            {
                if (right > start_index)
                {
                    --right;
                }
                else
                {
                    break;
                }
            }
        }

        if (left <= right)
        {
            system_resizable_vector_swap(resizable_vector,
                                         left,
                                         right);

            ++left;
            --right;
        }
    }
    while (left < right);

    if (start_index < right)
    {
        _system_resizable_vector_sort_recursive(resizable_vector,
                                                comparator_func_ptr,
                                                start_index,
                                                right);
    }
    if (end_index > left)
    {
        _system_resizable_vector_sort_recursive(resizable_vector,
                                                comparator_func_ptr,
                                                left,
                                                end_index);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resizable_vector_sort(__in __notnull system_resizable_vector resizable_vector,
                                                                    int                   (*comparator_func_ptr)(void*, void*) )
{
    _resizable_vector_descriptor* vector_ptr = (_resizable_vector_descriptor*) resizable_vector;

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(vector_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    _system_resizable_vector_sort_recursive(resizable_vector,
                                            comparator_func_ptr,
                                            0,
                                            system_resizable_vector_get_amount_of_elements(resizable_vector) - 1);

    if (vector_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_resizable_vector_swap(__in __notnull system_resizable_vector resizable_vector,
                                                                    uint32_t                index_a,
                                                                    uint32_t                index_b)
{
    _resizable_vector_descriptor* resizable_vector_ptr = (_resizable_vector_descriptor*) resizable_vector;
    bool                          result               = true;

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
PUBLIC EMERALD_API void system_resizable_vector_unlock(__in __notnull system_resizable_vector             vector,
                                                       __in           system_read_write_mutex_access_type access_type)
{
    _resizable_vector_descriptor* vector_ptr = (_resizable_vector_descriptor*) vector;

    ASSERT_DEBUG_SYNC(vector_ptr->access_mutex != NULL,
                      "Vector not initialized for thread-safe access");

    system_read_write_mutex_unlock(vector_ptr->access_mutex,
                                   access_type);
}
