/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "postprocessing/postprocessing_bloom.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

#if (0)
/** Internal type definition */
typedef struct
{
    ogl_context context;

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_bloom;


/** Internal variables */ 

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_bloom,
                               postprocessing_bloom,
                              _postprocessing_bloom);


/** TODO */
PRIVATE void _postprocessing_bloom_release(void* ptr)
{
    _postprocessing_bloom* data_ptr = (_postprocessing_bloom*) ptr;
    
    // ...
}


/** Please see header for specification */
PUBLIC EMERALD_API postprocessing_bloom postprocessing_bloom_create(ogl_context               context, 
                                                                    system_hashed_ansi_string name)
{
    /* Instantiate the object */
    _postprocessing_bloom* result_object = new (std::nothrow) _postprocessing_bloom;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object, 
                                                   _postprocessing_bloom_release,
                                                   OBJECT_TYPE_POSTPROCESSING_BLOOM,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Post-processing Bloom\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (postprocessing_bloom) result_object;

end:
    if (result_object != NULL)
    {
        _postprocessing_bloom_release(result_object);

        delete result_object;
    }

    return NULL;
}

/* Please see header for specification */
PUBLIC EMERALD_API void postprocessing_bloom_execute(postprocessing_bloom bloom,
                                                     texture              texture_insgance,
                                                     uint32_t             downsampled_texture_instance,
                                                     uint32_t             result_texture_instance)
{
    // ..
}

#endif