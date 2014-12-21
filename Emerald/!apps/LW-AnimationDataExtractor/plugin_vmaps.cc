#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_common.h"
#include "plugin_vmaps.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

typedef struct _vmap
{
    system_hashed_ansi_string name;

    _vmap()
    {
        name = NULL;
    }
} _vmap;


/** Global data.
 *
 *  Yes, I know. I'm sorry. :-)
 */
/** Stores _vmap instances of LWVMAP_TXUV type */
PRIVATE system_resizable_vector uv_maps_vector = NULL;


/** Please see header for spec */
PUBLIC void DeinitVMapData()
{
    if (uv_maps_vector != NULL)
    {
        _vmap* current_vmap_ptr = NULL;

        while (!system_resizable_vector_pop(uv_maps_vector,
                                           &current_vmap_ptr) )
        {
            delete current_vmap_ptr;

            current_vmap_ptr = NULL;
        }

        system_resizable_vector_release(uv_maps_vector);
        uv_maps_vector = NULL;
    }
}

/** Please see header for spec */
PUBLIC void GetGlobalVMapProperty(__in            _vmap_property property,
                                  __out __notnull void*          out_result)
{
    switch (property)
    {
        case VMAP_PROPERTY_N_UV_VMAPS:
        {
            *(unsigned int*) out_result = system_resizable_vector_get_amount_of_elements(uv_maps_vector);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized global VMap property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void GetVMapProperty(__in            _vmap_property property,
                            __in            _vmap_type     vmap_type,
                            __in            unsigned int   n_vmap,
                            __out __notnull void*          out_result)
{
    system_resizable_vector vmap_vector = NULL;

    /* Retrieve the vmap vector */
    switch (vmap_type)
    {
        case VMAP_TYPE_UVMAP:
        {
            vmap_vector = uv_maps_vector;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized VMap type");

            goto end;
        }
    }

    /* Retrieve the vmap descriptor */
    _vmap* vmap_ptr = NULL;

    if (!system_resizable_vector_get_element_at(vmap_vector,
                                                n_vmap,
                                               &vmap_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "VMap not found");

        goto end;
    }

    /* Retrieve the property value */
    switch (property)
    {
        case VMAP_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = vmap_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized VMap property");
        }
    } /* switch (property) */

end:
    ;
}

/** Please see header for spec */
PUBLIC void InitVMapData()
{
    /* Initialize containers */
    uv_maps_vector = system_resizable_vector_create(4, /* capacity */
                                                    sizeof(_vmap*) );

    /* Iterate over all vertex maps */
    const int n_uv_vmaps = object_funcs_ptr->numVMaps(LWVMAP_TXUV);

    for (int n_uv_vmap = 0;
             n_uv_vmap < n_uv_vmaps;
           ++n_uv_vmap)
    {
        const char* vmap_name = object_funcs_ptr->vmapName(LWVMAP_TXUV,
                                                           n_uv_vmap);

        ASSERT_DEBUG_SYNC(object_funcs_ptr->vmapDim(LWVMAP_TXUV,
                                                    n_uv_vmap) == 2,
                          "UV vmap uses != 2 coordinates");

        /* Spawn the descriptor */
        _vmap* new_vmap = new (std::nothrow) _vmap;

        ASSERT_ALWAYS_SYNC(new_vmap != NULL,
                           "Out of memory");
        if (new_vmap != NULL)
        {
            new_vmap->name = system_hashed_ansi_string_create(vmap_name);

            /* Store it */
            system_resizable_vector_push(uv_maps_vector,
                                         new_vmap);
        }
    } /* for (all UV vmaps) */
}


