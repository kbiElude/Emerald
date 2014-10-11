
/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "curve/curve_types.h"
#include "scene/scene_curve.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"


/* Private declarations */
typedef struct
{
    system_hashed_ansi_string name;

    scene_curve_id  property_id;
    curve_container property_instance;

    REFCOUNT_INSERT_VARIABLES
} _scene_curve;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_curve, scene_curve, _scene_curve);

/** TODO */
PRIVATE void _scene_curve_init(__in __notnull _scene_curve*             data_ptr,
                               __in __notnull system_hashed_ansi_string name,
                               __in           scene_curve_id            id,
                               __in __notnull curve_container           instance)
{
    data_ptr->name              = name;
    data_ptr->property_id       = id;
    data_ptr->property_instance = instance;
}

/** TODO */
PRIVATE void _scene_curve_release(void* data_ptr)
{
    _scene_curve* curve_ptr = (_scene_curve*) data_ptr;

    curve_container_release(curve_ptr->property_instance);
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_curve_create(__in __notnull system_hashed_ansi_string name,
                                                  __in           scene_curve_id            id,
                                                  __in __notnull curve_container           instance)
{
    _scene_curve* new_scene_curve = new (std::nothrow) _scene_curve;

    ASSERT_DEBUG_SYNC(new_scene_curve != NULL, "Out of memory");
    if (new_scene_curve != NULL)
    {
        _scene_curve_init(new_scene_curve, name, id, instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_curve, 
                                                       _scene_curve_release, 
                                                       OBJECT_TYPE_SCENE_CURVE, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Curves\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_curve) new_scene_curve;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_curve_get(__in  __notnull scene_curve          instance,
                                        __in            scene_curve_property property,
                                        __out __notnull void*                result_ptr)
{
    _scene_curve* curve_ptr = (_scene_curve*) instance;

    switch (property)
    {
        case SCENE_CURVE_PROPERTY_ID:
        {
            *((scene_curve_id*) result_ptr) = curve_ptr->property_id;

            break;
        }

        case SCENE_CURVE_PROPERTY_INSTANCE:
        {
            *((curve_container*) result_ptr) = curve_ptr->property_instance;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized scene curve property [%d]", property);
        }
    } /* switch */
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_curve_load(__in __notnull system_file_serializer serializer)
{
    system_hashed_ansi_string name     = NULL;
    scene_curve               result   = NULL;
    scene_curve_id            id       = -1;
    curve_container           instance = NULL;

    if (!system_file_serializer_read_hashed_ansi_string(serializer,             &name)     ||
        !system_file_serializer_read                   (serializer, sizeof(id), &id)       ||
        !system_file_serializer_read_curve_container   (serializer,             &instance))
    {
        ASSERT_DEBUG_SYNC(false, "Could not load curve properties");

        goto end_with_error;
    }
    else
    {
        result = scene_curve_create(name, id, instance);

        if (result == NULL)
        {
            ASSERT_DEBUG_SYNC(false, "Could not create curve instance");

            goto end_with_error;
        }
    }

    return result;

end_with_error:
    if (result != NULL)
    {
        scene_curve_release(result);
    }

    return NULL;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_curve_save(__in __notnull system_file_serializer serializer,
                                         __in __notnull scene_curve            curve)
{
    bool          result    = false;
    _scene_curve* curve_ptr = (_scene_curve*) curve;
    
    if (curve_ptr != NULL)
    {
        if (system_file_serializer_write_hashed_ansi_string(serializer,                                  curve_ptr->name)               &&
            system_file_serializer_write                   (serializer, sizeof(curve_ptr->property_id), &curve_ptr->property_id)        &&
            system_file_serializer_write_curve_container   (serializer,                                  curve_ptr->property_instance))
        {
            result = true;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not save curve");
        }
    }

    return result;
}

