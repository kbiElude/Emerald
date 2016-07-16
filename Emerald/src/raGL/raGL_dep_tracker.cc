#include "shared.h"
#include "raGL/raGL_dep_tracker.h"
#include "system/system_hash64map.h"
#include "system/system_resource_pool.h"


typedef struct _raGL_dep_tracker_object
{
    int32_t barriers_needed;

    _raGL_dep_tracker_object()
    {
        barriers_needed = ~0;
    }

} _raGL_dep_tracker_object;

typedef struct _raGL_dep_tracker
{
    system_hash64map     handles;      /* maps object ptr to _raGL_dep_tracker_object instance */
    system_resource_pool handle_pool;

    _raGL_dep_tracker()
    {
        handle_pool = system_resource_pool_create(sizeof(_raGL_dep_tracker_object),
                                                  16,       /* n_elements_to_preallocate */
                                                  nullptr,  /* init_fn                   */
                                                  nullptr); /* deinit_fn                 */
        handles     = system_hash64map_create    (16);      /* capacity                  */
    }

    ~_raGL_dep_tracker()
    {
        if (handle_pool != nullptr)
        {
            system_resource_pool_release(handle_pool);

            handle_pool = nullptr;
        }

        if (handles != nullptr)
        {
            system_hash64map_release(handles);

            handles = nullptr;
        }
    }
} _raGL_dep_tracker;


/** TODO */
PRIVATE void _raGL_dep_tracker_unset_needed_barriers(_raGL_dep_tracker* tracker_ptr,
                                                     GLenum             barrier_bits)
{
    _raGL_dep_tracker_object* object_ptr = nullptr;
    uint32_t                  n_objects  = 0;

    system_hash64map_get_property(tracker_ptr->handles,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        system_hash64map_get_element_at(tracker_ptr->handles,
                                        n_object,
                                       &object_ptr,
                                        nullptr); /* result_hash_ptr */

        object_ptr->barriers_needed &= ~(barrier_bits);
    }
}

/** Please see header for specification */
PUBLIC raGL_dep_tracker raGL_dep_tracker_create()
{
    _raGL_dep_tracker* tracker_ptr = new (std::nothrow) _raGL_dep_tracker;

    ASSERT_DEBUG_SYNC(tracker_ptr != nullptr,
                      "Out of memory");

    return (raGL_dep_tracker) tracker_ptr;
}

/** Please see header for specification */
PUBLIC bool raGL_dep_tracker_is_dirty(raGL_dep_tracker tracker,
                                      void*            object,
                                      GLbitfield       barrier_bits)
{
    bool                      result      = false;
    _raGL_dep_tracker_object* object_ptr  = nullptr;
    _raGL_dep_tracker*        tracker_ptr = reinterpret_cast<_raGL_dep_tracker*>(tracker);

    if (barrier_bits == GL_ALL_BARRIER_BITS)
    {
        barrier_bits = ~0;
    }

    if (!system_hash64map_get(tracker_ptr->handles,
                              (system_hash64) object,
                             &object_ptr) )
    {
        goto end;
    }

    result = ((object_ptr->barriers_needed & barrier_bits) != 0);

    if (result)
    {
        object_ptr->barriers_needed &= ~barrier_bits;

        if (barrier_bits & GL_TEXTURE_UPDATE_BARRIER_BIT)
        {
            _raGL_dep_tracker_unset_needed_barriers(tracker_ptr,
                                                    GL_TEXTURE_UPDATE_BARRIER_BIT);
        }
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC void raGL_dep_tracker_mark_as_dirty(raGL_dep_tracker tracker,
                                           void*            object)
{
    _raGL_dep_tracker_object* object_ptr  = nullptr;
    _raGL_dep_tracker*        tracker_ptr = reinterpret_cast<_raGL_dep_tracker*>(tracker);

    if (!system_hash64map_get(tracker_ptr->handles,
                              (system_hash64) object,
                             &object_ptr) )
    {
        object_ptr = reinterpret_cast<_raGL_dep_tracker_object*>(system_resource_pool_get_from_pool(tracker_ptr->handle_pool) );

        if (object_ptr == nullptr)
        {
            ASSERT_DEBUG_SYNC(object_ptr != nullptr,
                              "Could not retrieve a _raGL_dep_tracker_object instance.");

            goto end;
        }

        system_hash64map_insert(tracker_ptr->handles,
                                (system_hash64) object,
                                object_ptr,
                                nullptr,  /* release_callback          */
                                nullptr); /* release_callback_user_arg */
    }

    object_ptr->barriers_needed = ~0;

end:
    ;
}

/** Please see header for specification */
PUBLIC void raGL_dep_tracker_release(raGL_dep_tracker tracker)
{
    delete reinterpret_cast<_raGL_dep_tracker*>(tracker);
}

/** Please see header for specification */
PUBLIC void raGL_dep_tracker_reset(raGL_dep_tracker tracker)
{
    _raGL_dep_tracker* tracker_ptr = reinterpret_cast<_raGL_dep_tracker*>(tracker);

    system_resource_pool_return_all_allocations(tracker_ptr->handle_pool);
}
