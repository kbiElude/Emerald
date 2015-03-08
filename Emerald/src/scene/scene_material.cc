
/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_curve.h"
#include "scene/scene_material.h"
#include "system/system_file_serializer.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"


/* Private declarations */
typedef struct
{
    curve_container                  color[3];
    system_hashed_ansi_string        color_texture_file_name;
    scene_material_texture_filtering color_texture_mag_filter;
    scene_material_texture_filtering color_texture_min_filter;
    curve_container                  glosiness;
    curve_container                  luminance;
    system_hashed_ansi_string        luminance_texture_file_name;
    scene_material_texture_filtering luminance_texture_mag_filter;
    scene_material_texture_filtering luminance_texture_min_filter;
    system_hashed_ansi_string        name;
    system_hashed_ansi_string        normal_texture_file_name;
    scene_material_texture_filtering normal_texture_mag_filter;
    scene_material_texture_filtering normal_texture_min_filter;
    system_hashed_ansi_string        object_manager_path;
    scene                            owner_scene;                    /* scene_material DOES NOT increment owner_scene ref cnt */
    curve_container                  reflection_ratio;
    system_hashed_ansi_string        reflection_texture_file_name;
    scene_material_texture_filtering reflection_texture_mag_filter;
    scene_material_texture_filtering reflection_texture_min_filter;
    float                            smoothing_angle;
    curve_container                  specular_ratio;
    system_hashed_ansi_string        specular_texture_file_name;
    scene_material_texture_filtering specular_texture_mag_filter;
    scene_material_texture_filtering specular_texture_min_filter;

    REFCOUNT_INSERT_VARIABLES
} _scene_material;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_material,
                               scene_material,
                              _scene_material);


/** TODO */
PRIVATE void _scene_material_init(__in __notnull _scene_material*          data_ptr,
                                  __in __notnull system_hashed_ansi_string name,
                                  __in __notnull system_hashed_ansi_string object_manager_path,
                                  __in __notnull scene                     owner_scene)
{
    memset(data_ptr,
           0,
           sizeof(_scene_material) );

    data_ptr->color[0]            = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Color R [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);
    data_ptr->color[1]            = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Color G [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);
    data_ptr->color[2]            = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Color B [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);
    data_ptr->glosiness           = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Glosiness [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);
    data_ptr->luminance           = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Luminance [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);
    data_ptr->name                = name;
    data_ptr->object_manager_path = object_manager_path;
    data_ptr->owner_scene         = owner_scene;
    data_ptr->reflection_ratio    = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Reflection Ratio [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);
    data_ptr->specular_ratio      = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                   " Specular Ratio [default]"),
                                                           object_manager_path,
                                                           SYSTEM_VARIANT_FLOAT);

    data_ptr->color_texture_mag_filter      = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR;
    data_ptr->color_texture_min_filter      = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR;
    data_ptr->luminance_texture_mag_filter  = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR;
    data_ptr->luminance_texture_min_filter  = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR;
    data_ptr->normal_texture_mag_filter     = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR;
    data_ptr->normal_texture_min_filter     = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR;
    data_ptr->reflection_texture_mag_filter = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR;
    data_ptr->reflection_texture_min_filter = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR;
    data_ptr->specular_texture_mag_filter   = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR;
    data_ptr->specular_texture_min_filter   = SCENE_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR;
}

/** TODO */
PRIVATE bool _scene_material_load_curve(__in_opt           scene                     owner_scene,
                                        __in_opt           system_hashed_ansi_string object_manager_path,
                                        __in     __notnull curve_container*          curve_ptr,
                                        __in     __notnull system_file_serializer    serializer)
{
    bool result = true;

    if (owner_scene != NULL)
    {
        scene_curve_id curve_id = 0;

        result &= system_file_serializer_read(serializer,
                                              sizeof(curve_id),
                                             &curve_id);

        if (result)
        {
            scene_curve scene_curve = scene_get_curve_by_id(owner_scene,
                                                            curve_id);

            scene_curve_get(scene_curve,
                            SCENE_CURVE_PROPERTY_INSTANCE,
                            curve_ptr);

            curve_container_retain(*curve_ptr);
        }
    }
    else
    {
        result &= system_file_serializer_read_curve_container(serializer,
                                                              object_manager_path,
                                                              curve_ptr);
    }

    return result;
}

/** TODO */
PRIVATE void _scene_material_release(void* data_ptr)
{
    _scene_material* material_ptr = (_scene_material*) data_ptr;

    material_ptr->smoothing_angle = 0.0f;

    if (material_ptr->color[0] != NULL)
    {
        curve_container_release(material_ptr->color[0]);

        material_ptr->color[0] = NULL;
    }

    if (material_ptr->color[1] != NULL)
    {
        curve_container_release(material_ptr->color[1]);

        material_ptr->color[1] = NULL;
    }

    if (material_ptr->color[2] != NULL)
    {
        curve_container_release(material_ptr->color[2]);

        material_ptr->color[2] = NULL;
    }

    if (material_ptr->glosiness != NULL)
    {
        curve_container_release(material_ptr->glosiness);

        material_ptr->glosiness = NULL;
    }

    if (material_ptr->luminance != NULL)
    {
        curve_container_release(material_ptr->luminance);

        material_ptr->luminance = NULL;
    }

    if (material_ptr->reflection_ratio != NULL)
    {
        curve_container_release(material_ptr->reflection_ratio);

        material_ptr->reflection_ratio = NULL;
    }

    if (material_ptr->specular_ratio != NULL)
    {
        curve_container_release(material_ptr->specular_ratio);

        material_ptr->specular_ratio = NULL;
    }
}

/** TODO */
PRIVATE bool _scene_material_save_curve(__in_opt           scene                  owner_scene,
                                        __in     __notnull curve_container        in_curve,
                                        __in     __notnull system_file_serializer serializer)
{
    bool result = true;

    if (owner_scene != NULL)
    {
        scene_curve    curve    = scene_get_curve_by_container(owner_scene,
                                                               in_curve);
        scene_curve_id curve_id = 0;

        scene_curve_get(curve,
                        SCENE_CURVE_PROPERTY_ID,
                       &curve_id);

        result &= system_file_serializer_write(serializer,
                                               sizeof(curve_id),
                                              &curve_id);
    }
    else
    {
        result &= system_file_serializer_write_curve_container(serializer,
                                                               in_curve);
    }

    return result;
}


/* Please see header for specification */
PUBLIC EMERALD_API scene_material scene_material_create(__in     __notnull system_hashed_ansi_string name,
                                                        __in_opt           system_hashed_ansi_string object_manager_path,
                                                        __in     __notnull scene                     owner_scene)
{
    _scene_material* new_scene_material = new (std::nothrow) _scene_material;

    ASSERT_DEBUG_SYNC(new_scene_material != NULL,
                      "Out of memory");
    if (new_scene_material != NULL)
    {
        _scene_material_init(new_scene_material,
                             name,
                             object_manager_path,
                             owner_scene);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_material,
                                                       _scene_material_release,
                                                       OBJECT_TYPE_SCENE_MATERIAL,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_SCENE_MATERIAL,
                                                                       object_manager_path) );
    }

    return (scene_material) new_scene_material;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_material_get_property(__in  __notnull scene_material          material_instance,
                                                    __in            scene_material_property property,
                                                    __out __notnull void*                   out_result)
{
    _scene_material* material_ptr = (_scene_material*) material_instance;

    switch (property)
    {
        case SCENE_MATERIAL_PROPERTY_COLOR:
        {
            *(curve_container**) out_result = material_ptr->color;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = material_ptr->color_texture_file_name;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MAG_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->color_texture_mag_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MIN_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->color_texture_min_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_GLOSINESS:
        {
            *(curve_container*) out_result = material_ptr->glosiness;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_LUMINANCE:
        {
            *(curve_container*) out_result = material_ptr->luminance;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->luminance_texture_file_name;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_MAG_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->luminance_texture_mag_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_MIN_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->luminance_texture_min_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = material_ptr->name;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->normal_texture_file_name;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_MAG_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->normal_texture_mag_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_MIN_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->normal_texture_min_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_OBJECT_MANAGER_PATH:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->object_manager_path;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_OWNER_SCENE:
        {
            *(scene*) out_result = material_ptr->owner_scene;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_REFLECTION_RATIO:
        {
            *(curve_container*) out_result = material_ptr->reflection_ratio;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->reflection_texture_file_name;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_MAG_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->reflection_texture_mag_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_MIN_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->reflection_texture_min_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE:
        {
            *(float*) out_result = material_ptr->smoothing_angle;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO:
        {
            *(curve_container*) out_result = material_ptr->specular_ratio;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->specular_texture_file_name;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_MAG_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->specular_texture_mag_filter;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_MIN_FILTER:
        {
            *(scene_material_texture_filtering*) out_result = material_ptr->specular_texture_min_filter;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_material property requested");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC scene_material scene_material_load(__in     __notnull system_file_serializer    serializer,
                                          __in_opt           scene                     owner_scene,
                                          __in_opt           system_hashed_ansi_string object_manager_path)
{
    system_hashed_ansi_string name           = NULL;
    bool                      result         = true;
    scene_material            result_material = NULL;

    /* Retrieve mesh instance properties. */
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &name);

    if (!result)
    {
        goto end;
    }

    /* Spawn the instance.  */
    result_material = scene_material_create(name,
                                            object_manager_path,
                                            owner_scene);

    ASSERT_DEBUG_SYNC(result_material != NULL,
                      "Could not instantiate a scene_material object");

    if (result_material == NULL)
    {
        goto end;
    }

    /* Read all the properties */
    _scene_material* result_material_ptr = (_scene_material*) result_material;

    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                             result_material_ptr->color + 0,
                                                             serializer);
    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                             result_material_ptr->color + 1,
                                                             serializer);
    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                             result_material_ptr->color + 2,
                                                             serializer);
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &result_material_ptr->color_texture_file_name);
    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                            &result_material_ptr->glosiness,
                                                             serializer);
    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                            &result_material_ptr->luminance,
                                                             serializer);
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &result_material_ptr->luminance_texture_file_name);
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &result_material_ptr->normal_texture_file_name);
    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                            &result_material_ptr->reflection_ratio,
                                                             serializer);
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &result_material_ptr->reflection_texture_file_name);
    result &= system_file_serializer_read                   (serializer,
                                                             sizeof(result_material_ptr->smoothing_angle),
                                                            &result_material_ptr->smoothing_angle);
    result &= _scene_material_load_curve                    (owner_scene,
                                                             result_material_ptr->object_manager_path,
                                                            &result_material_ptr->specular_ratio,
                                                             serializer);
    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &result_material_ptr->specular_texture_file_name);

    /* All done */
end:
    return result_material;
}

/* Please see header for spec */
PUBLIC bool scene_material_save(__in     __notnull system_file_serializer serializer,
                                __in     __notnull const scene_material   material,
                                __in_opt           scene                  owner_scene)
{
    const _scene_material* material_ptr = (const _scene_material*) material;
    bool                   result;

    result  = system_file_serializer_write_hashed_ansi_string(serializer,
                                                              material_ptr->name);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->color[0],
                                                              serializer);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->color[1],
                                                              serializer);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->color[2],
                                                              serializer);
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              (material_ptr->color_texture_file_name == NULL) ? system_hashed_ansi_string_get_default_empty_string()
                                                                                                              : material_ptr->color_texture_file_name);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->glosiness,
                                                              serializer);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->luminance,
                                                              serializer);
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              (material_ptr->luminance_texture_file_name == NULL) ? system_hashed_ansi_string_get_default_empty_string()
                                                                                                                  : material_ptr->luminance_texture_file_name);
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              (material_ptr->normal_texture_file_name    == NULL) ? system_hashed_ansi_string_get_default_empty_string()
                                                                                                                  : material_ptr->normal_texture_file_name);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->reflection_ratio,
                                                              serializer);
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              (material_ptr->reflection_texture_file_name == NULL) ? system_hashed_ansi_string_get_default_empty_string()
                                                                                                                   : material_ptr->reflection_texture_file_name);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(material_ptr->smoothing_angle),
                                                             &material_ptr->smoothing_angle);
    result &= _scene_material_save_curve                     (owner_scene,
                                                              material_ptr->specular_ratio,
                                                              serializer);
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              (material_ptr->specular_texture_file_name == NULL) ? system_hashed_ansi_string_get_default_empty_string()
                                                                                                                 : material_ptr->specular_texture_file_name);

    return result;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_material_set_property(__in __notnull scene_material          material,
                                                    __in           scene_material_property property,
                                                    __in __notnull const void*             data)
{
    _scene_material* material_ptr = (_scene_material*) material;

    switch (property)
    {
        case SCENE_MATERIAL_PROPERTY_COLOR:
        {
            for (unsigned int n_component = 0;
                              n_component < 3;
                            ++n_component)
            {
                if (material_ptr->color[n_component] != NULL)
                {
                    curve_container_release(material_ptr->color[n_component]);
                }

                material_ptr->color[n_component] = *((curve_container*) data + n_component);
                curve_container_retain(material_ptr->color[n_component]);
            }

            break;
        }

        case SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME:
        {
            material_ptr->color_texture_file_name = *((system_hashed_ansi_string*) data);

            break;
        }

        case SCENE_MATERIAL_PROPERTY_GLOSINESS:
        {
            if (material_ptr->glosiness != NULL)
            {
                curve_container_release(material_ptr->glosiness);
            }

            material_ptr->glosiness = *(curve_container*) data;
            curve_container_retain(material_ptr->glosiness);

            break;
        }

        case SCENE_MATERIAL_PROPERTY_LUMINANCE:
        {
            if (material_ptr->luminance != NULL)
            {
                curve_container_release(material_ptr->luminance);
            }

            material_ptr->luminance = *(curve_container*) data;
            curve_container_retain(material_ptr->luminance);

            break;
        }

        case SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME:
        {
            material_ptr->luminance_texture_file_name = *(system_hashed_ansi_string*) data;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME:
        {
            material_ptr->normal_texture_file_name = *(system_hashed_ansi_string*) data;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_REFLECTION_RATIO:
        {
            if (material_ptr->reflection_ratio != NULL)
            {
                curve_container_release(material_ptr->reflection_ratio);
            }

            material_ptr->reflection_ratio = *(curve_container*) data;
            curve_container_retain(material_ptr->reflection_ratio);

            break;
        }

        case SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME:
        {
            material_ptr->reflection_texture_file_name = *(system_hashed_ansi_string*) data;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE:
        {
            material_ptr->smoothing_angle = *(float*) data;

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO:
        {
            if (material_ptr->specular_ratio != NULL)
            {
                curve_container_release(material_ptr->specular_ratio);
            }

            material_ptr->specular_ratio = *(curve_container*) data;
            curve_container_retain(material_ptr->specular_ratio);

            break;
        }

        case SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME:
        {
            material_ptr->specular_texture_file_name = *(system_hashed_ansi_string*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_material property requested");
        }
    } /* switch (property) */
}
