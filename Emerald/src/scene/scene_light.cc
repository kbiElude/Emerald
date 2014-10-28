
/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "scene/scene_light.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"

/* Private declarations */
typedef struct
{
    float                     diffuse[3];
    float                     direction[3];
    system_hashed_ansi_string name;
    scene_light_type          type;

    REFCOUNT_INSERT_VARIABLES
} _scene_light;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_light, scene_light, _scene_light);

/** TODO */
PRIVATE void _scene_light_release(void* data_ptr)
{
    /* Nothing to do here */
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_light_create_directional(__in __notnull system_hashed_ansi_string name)
{
    _scene_light* new_scene_light = new (std::nothrow) _scene_light;

    ASSERT_DEBUG_SYNC(new_scene_light != NULL, "Out of memory");
    if (new_scene_light != NULL)
    {
        new_scene_light->direction[0] =  0;
        new_scene_light->direction[1] =  0;
        new_scene_light->direction[2] = -1;
        new_scene_light->name         = name;
        new_scene_light->type         = SCENE_LIGHT_TYPE_DIRECTIONAL;

        memset(new_scene_light->diffuse, 0, sizeof(new_scene_light->diffuse) );

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_light,
                                                       _scene_light_release,
                                                       OBJECT_TYPE_SCENE_LIGHT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_light) new_scene_light;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_light_get_property(__in  __notnull const scene_light    light,
                                                 __in            scene_light_property property,
                                                 __out __notnull void*                out_result)
{
    const _scene_light* light_ptr = (const _scene_light*) light;

    switch (property)
    {
        case SCENE_LIGHT_PROPERTY_DIFFUSE:
        {
            *((const float**) out_result) = light_ptr->diffuse;

            break;
        }

        case SCENE_LIGHT_PROPERTY_DIRECTION:
        {
            *((const float**) out_result) = light_ptr->direction;

            break;
        }

        case SCENE_LIGHT_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = light_ptr->name;

            break;
        }

        case SCENE_LIGHT_PROPERTY_TYPE:
        {
            *((scene_light_type*) out_result) = light_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light property requested");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC scene_light scene_light_load(__in __notnull system_file_serializer serializer)
{
    system_hashed_ansi_string light_name   = NULL;
    scene_light_type          light_type   = SCENE_LIGHT_TYPE_UNKNOWN;
    bool                      result       = true;
    scene_light               result_light = NULL;

    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &light_name);
    result &= system_file_serializer_read                   (serializer,
                                                            sizeof(light_type),
                                                           &light_type);

    ASSERT_DEBUG_SYNC(light_type == SCENE_LIGHT_TYPE_DIRECTIONAL,
                      "Unrecognized light type");

    if (light_type != SCENE_LIGHT_TYPE_DIRECTIONAL)
    {
        goto end_error;
    }

    result_light = scene_light_create_directional(light_name);

    ASSERT_ALWAYS_SYNC(result_light != NULL, "Out of memory");
    if (result_light != NULL)
    {
        _scene_light* result_light_ptr = (_scene_light*) result_light;

        result &= system_file_serializer_read(serializer,
                                              sizeof(result_light_ptr->diffuse),
                                              result_light_ptr->diffuse);
        result &= system_file_serializer_read(serializer,
                                              sizeof(result_light_ptr->direction),
                                              result_light_ptr->direction);

        if (!result)
        {
            ASSERT_DEBUG_SYNC(false, "Light data serialization failed");

            goto end_error;
        }
    }

    goto end;

end_error:
    if (result_light != NULL)
    {
        scene_light_release(result_light);

        result_light = NULL;
    }

end:
    return result_light;
}

/* Please see header for spec */
PUBLIC bool scene_light_save(__in __notnull system_file_serializer serializer,
                             __in __notnull const scene_light      light)
{
    const _scene_light* light_ptr = (const _scene_light*) light;
    bool                result    = true;

    result &= system_file_serializer_write_hashed_ansi_string(serializer,                               light_ptr->name);
    result &= system_file_serializer_write                   (serializer, sizeof(light_ptr->type),     &light_ptr->type);
    result &= system_file_serializer_write                   (serializer, sizeof(light_ptr->diffuse),   light_ptr->diffuse);
    result &= system_file_serializer_write                   (serializer, sizeof(light_ptr->direction), light_ptr->direction);

    return result;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_light_set_property(__in __notnull scene_light          light,
                                                 __in           scene_light_property property,
                                                 __in __notnull const void*          data)
{
    _scene_light* light_ptr = (_scene_light*) light;

    switch (property)
    {
        case SCENE_LIGHT_PROPERTY_DIFFUSE:
        {
            memcpy(light_ptr->diffuse, data, sizeof(light_ptr->diffuse) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_DIRECTION:
        {
            memcpy(light_ptr->direction, data, sizeof(light_ptr->direction) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecgonized scene_light property");

            break;
        }
    } /* switch (property) */

}
