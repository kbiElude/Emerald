
/**
 *
 * Emerald (kbi/elude @2012-2016)
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
REFCOUNT_INSERT_IMPLEMENTATION(scene_curve,
                               scene_curve,
                              _scene_curve);

/** TODO */
PRIVATE void _scene_curve_init(_scene_curve*             data_ptr,
                               system_hashed_ansi_string name,
                               scene_curve_id            id,
                               curve_container           instance)
{
    data_ptr->name              = name;
    data_ptr->property_id       = id;
    data_ptr->property_instance = instance;
}

/** TODO */
PRIVATE void _scene_curve_release(void* data_ptr)
{
    _scene_curve* curve_ptr = reinterpret_cast<_scene_curve*>(data_ptr);

    curve_container_release(curve_ptr->property_instance);
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_curve_create(system_hashed_ansi_string name,
                                                  scene_curve_id            id,
                                                  curve_container           instance)
{
    _scene_curve* new_scene_curve = new (std::nothrow) _scene_curve;

    ASSERT_DEBUG_SYNC(new_scene_curve != nullptr,
                      "Out of memory");

    if (new_scene_curve != nullptr)
    {
        _scene_curve_init(new_scene_curve,
                          name,
                          id,
                          instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_curve,
                                                       _scene_curve_release,
                                                       OBJECT_TYPE_SCENE_CURVE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Curves\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_curve) new_scene_curve;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_curve_get(scene_curve          instance,
                                        scene_curve_property property,
                                        void*                result_ptr)
{
    _scene_curve* curve_ptr = reinterpret_cast<_scene_curve*>(instance);

    switch (property)
    {
        case SCENE_CURVE_PROPERTY_ID:
        {
            *reinterpret_cast<scene_curve_id*>(result_ptr) = curve_ptr->property_id;

            break;
        }

        case SCENE_CURVE_PROPERTY_INSTANCE:
        {
            *reinterpret_cast<curve_container*>(result_ptr) = curve_ptr->property_instance;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized scene curve property [%d]",
                               property);
        }
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_curve scene_curve_load(system_file_serializer    serializer,
                                                system_hashed_ansi_string object_manager_path)
{
    system_hashed_ansi_string name     = nullptr;
    scene_curve               result   = nullptr;
    scene_curve_id            id       = -1;
    curve_container           instance = nullptr;

    if (!system_file_serializer_read_hashed_ansi_string(serializer,
                                                       &name)                ||
        !system_file_serializer_read                   (serializer,
                                                        sizeof(id),
                                                       &id)                  ||
        !system_file_serializer_read_curve_container   (serializer,
                                                        object_manager_path,
                                                       &instance))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not load curve properties");

        goto end_with_error;
    }
    else
    {
        /* In order to avoid collisions in environments where multiple scenes are loaded,
         * we embed the scene's name in the curve's name */
        system_hashed_ansi_string file_name  = nullptr;
        system_hashed_ansi_string final_name = nullptr;
        const char*               limiter    = "\\";

        system_file_serializer_get_property(serializer,
                                            SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,
                                           &file_name);

        const char* file_name_parts[] =
        {
            system_hashed_ansi_string_get_buffer(file_name),
            limiter,
            system_hashed_ansi_string_get_buffer(name)
        };
        const unsigned int n_file_name_parts = sizeof(file_name_parts) / sizeof(file_name_parts[0]);

        result = scene_curve_create(system_hashed_ansi_string_create_by_merging_strings(n_file_name_parts,
                                                                                        file_name_parts),
                                    id,
                                    instance);

        if (result == nullptr)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create curve instance");

            goto end_with_error;
        }
    }

    return result;

end_with_error:
    if (result != nullptr)
    {
        scene_curve_release(result);
    }

    return nullptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool scene_curve_save(system_file_serializer serializer,
                                         scene_curve            curve)
{
    bool          result    = false;
    _scene_curve* curve_ptr = reinterpret_cast<_scene_curve*>(curve);
    
    if (curve_ptr != nullptr)
    {
        if (system_file_serializer_write_hashed_ansi_string(serializer,
                                                            curve_ptr->name)                &&
            system_file_serializer_write                   (serializer,
                                                            sizeof(curve_ptr->property_id),
                                                           &curve_ptr->property_id)         &&
            system_file_serializer_write_curve_container   (serializer,
                                                            curve_ptr->property_instance))
        {
            result = true;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not save curve");
        }
    }

    return result;
}

