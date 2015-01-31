
/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_curve.h"
#include "scene/scene_light.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_variant.h"
#include <sstream>

#define DEFAULT_SHADOW_MAP_SIZE (1024)


/* Private declarations */
typedef struct
{
    curve_container            constant_attenuation;
    curve_container            color    [3];
    curve_container            color_intensity;
    float                      direction[3];
    scene_graph_node           graph_owner_node;
    curve_container            linear_attenuation;
    system_hashed_ansi_string  name;
    float                      position[3];
    curve_container            quadratic_attenuation;
    ogl_texture_internalformat shadow_map_internalformat;
    unsigned int               shadow_map_size[2];
    ogl_texture                shadow_map_texture;
    system_matrix4x4           shadow_map_vp;
    scene_light_type           type;
    bool                       uses_shadow_map;

    REFCOUNT_INSERT_VARIABLES
} _scene_light;

/* Forward declarations */
PRIVATE void _scene_light_init_default_attenuation_curves   (__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init_default_color_curves         (__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init_default_color_intensity_curve(__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init                              (__in     __notnull _scene_light*          light_ptr);
PRIVATE bool _scene_light_load_curve                        (__in_opt           scene                  owner_scene,
                                                             __in     __notnull curve_container*       curve_ptr,
                                                             __in     __notnull system_file_serializer serializer);
PRIVATE void _scene_light_release                           (                   void*                  data_ptr);
PRIVATE bool _scene_light_save_curve                        (__in_opt           scene                  owner_scene,
                                                             __in     __notnull curve_container        in_curve,
                                                             __in     __notnull system_file_serializer serializer);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_light, scene_light, _scene_light);


/** TODO */
PRIVATE void _scene_light_init_default_attenuation_curves(__in __notnull _scene_light* light_ptr)
{
    system_variant temp_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    /* Constant attenuation: used by point light.
     *
     *  Default value: 1.0
     */
    if (light_ptr->type == SCENE_LIGHT_TYPE_POINT)
    {
        ASSERT_DEBUG_SYNC(light_ptr->constant_attenuation == NULL,
                          "Light constant attenuation curve already instantiated");

        light_ptr->constant_attenuation = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                                         " constant attenuation"),
                                                                 SYSTEM_VARIANT_FLOAT);

        system_variant_set_float         (temp_variant,
                                          1.0f);
        curve_container_set_default_value(light_ptr->constant_attenuation,
                                          temp_variant);
    }

    /* Linear attenuation: used by point light.
     *
     * Default value: 0.0
     */
    if (light_ptr->type == SCENE_LIGHT_TYPE_POINT)
    {
        ASSERT_DEBUG_SYNC(light_ptr->linear_attenuation == NULL,
                          "Light linear attenuation curve already instantiated");

        light_ptr->linear_attenuation = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                                       " linear attenuation"),
                                                                 SYSTEM_VARIANT_FLOAT);

        system_variant_set_float         (temp_variant,
                                          0.0f);
        curve_container_set_default_value(light_ptr->linear_attenuation,
                                          temp_variant);
    }

    /* Quadratic attenuation: used by point light.
     *
     * Default value: 0.0
     */
    if (light_ptr->type == SCENE_LIGHT_TYPE_POINT)
    {
        ASSERT_DEBUG_SYNC(light_ptr->quadratic_attenuation == NULL,
                          "Light quadratic attenuation curve already instantiated");

        light_ptr->quadratic_attenuation = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                                          " quadratic attenuation"),
                                                                 SYSTEM_VARIANT_FLOAT);

        system_variant_set_float         (temp_variant,
                                          0.0f);
        curve_container_set_default_value(light_ptr->quadratic_attenuation,
                                          temp_variant);
    }

    system_variant_release(temp_variant);
}

/** TODO */
PRIVATE void _scene_light_init_default_color_curves(__in __notnull _scene_light* light_ptr)
{
    /* Color: common for all light types.
     *
     * Default color: (1, 0, 0)
     */
    ASSERT_DEBUG_SYNC(light_ptr->color[0] == NULL &&
                      light_ptr->color[1] == NULL &&
                      light_ptr->color[2] == NULL,
                      "Light color curve(s) already instantiated");

    system_variant temp_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    for (unsigned int n_component = 0;
                      n_component < sizeof(light_ptr->color) / sizeof(light_ptr->color[0]);
                    ++n_component)
    {
        std::stringstream color_component_curve_name_sstream;

        color_component_curve_name_sstream << system_hashed_ansi_string_get_buffer(light_ptr->name)
                                           << " color component "
                                           << n_component;

        light_ptr->color[n_component] = curve_container_create(system_hashed_ansi_string_create(color_component_curve_name_sstream.str().c_str() ),
                                                               SYSTEM_VARIANT_FLOAT);

        system_variant_set_float         (temp_variant,
                                          (n_component == 0) ? 1.0f : 0.0f);
        curve_container_set_default_value(light_ptr->color[n_component],
                                          temp_variant);
    }

    system_variant_release(temp_variant);
}

/** TODO */
PRIVATE void _scene_light_init_default_color_intensity_curve(__in __notnull _scene_light* light_ptr)
{
    system_variant temp_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    /* Color intensity: common for all light types */
    ASSERT_DEBUG_SYNC(light_ptr->color_intensity == NULL,
                      "Light color intensity curve already instantiated");

    light_ptr->color_intensity = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                                " color intensity"),
                                                        SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (temp_variant,
                                      1.0f);
    curve_container_set_default_value(light_ptr->color_intensity,
                                      temp_variant);

    system_variant_release(temp_variant);
}

/** TODO */
PRIVATE void _scene_light_init(__in __notnull _scene_light* light_ptr)
{
    /* Curves - set all to NULL */
    memset(light_ptr->color,
           0,
           sizeof(light_ptr->color) );

    light_ptr->color_intensity       = NULL;
    light_ptr->constant_attenuation  = NULL;
    light_ptr->linear_attenuation    = NULL;
    light_ptr->quadratic_attenuation = NULL;

    light_ptr->shadow_map_internalformat = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16;
    light_ptr->shadow_map_size[0]        = DEFAULT_SHADOW_MAP_SIZE;
    light_ptr->shadow_map_size[1]        = DEFAULT_SHADOW_MAP_SIZE;
    light_ptr->shadow_map_texture        = NULL;
    light_ptr->shadow_map_vp             = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(light_ptr->shadow_map_vp);

    /* Direction: used by directional light */
    if (light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL)
    {
        memset(light_ptr->direction,
               0,
               sizeof(light_ptr->direction) );

        light_ptr->direction[2] = -1.0f;
    }

    /* Position: float[3] */
    memset(light_ptr->position,
           0,
           sizeof(float) * 3);

    /* Uses shadow map: common */
    light_ptr->uses_shadow_map = true;
}

/** TODO */
PRIVATE bool _scene_light_load_curve(__in_opt           scene                  owner_scene,
                                     __in     __notnull curve_container*       curve_ptr,
                                     __in     __notnull system_file_serializer serializer)
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
                                                              curve_ptr);
    }

    return result;
}

/** TODO */
PRIVATE void _scene_light_release(void* data_ptr)
{
    _scene_light*    light_ptr    = (_scene_light*) data_ptr;
    curve_container* containers[] =
    {
        light_ptr->color + 0,
        light_ptr->color + 1,
        light_ptr->color + 2,
       &light_ptr->color_intensity,
       &light_ptr->constant_attenuation,
       &light_ptr->linear_attenuation,
       &light_ptr->quadratic_attenuation,
    };
    const uint32_t n_containers = sizeof(containers) / sizeof(containers[0]);

    for (uint32_t n_container = 0;
                  n_container < n_containers;
                ++n_container)
    {
        if (*containers[n_container] != NULL)
        {
            curve_container_release(*containers[n_container]);

            *containers[n_container] = NULL;
        }
    } /* for (all curve containers) */

    if (light_ptr->shadow_map_vp != NULL)
    {
        system_matrix4x4_release(light_ptr->shadow_map_vp);

        light_ptr->shadow_map_vp = NULL;
    }

    ASSERT_DEBUG_SYNC(light_ptr->shadow_map_texture == NULL,
                      "Shadow map texture should never be assigned to a scene_light instance at release time");
}

/** TODO */
PRIVATE bool _scene_light_save_curve(__in_opt           scene                  owner_scene,
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
PUBLIC EMERALD_API scene_light scene_light_create_ambient(__in __notnull system_hashed_ansi_string name)
{
    _scene_light* new_scene_light = new (std::nothrow) _scene_light;

    ASSERT_DEBUG_SYNC(new_scene_light != NULL,
                      "Out of memory");

    if (new_scene_light != NULL)
    {
        memset(new_scene_light,
               0,
               sizeof(_scene_light) );

        new_scene_light->name = name;
        new_scene_light->type = SCENE_LIGHT_TYPE_AMBIENT;

        _scene_light_init(new_scene_light);

        /* Ambient light never casts shadows */
        new_scene_light->uses_shadow_map = false;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_light,
                                                       _scene_light_release,
                                                       OBJECT_TYPE_SCENE_LIGHT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_light) new_scene_light;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_light_create_directional(__in __notnull system_hashed_ansi_string name)
{
    _scene_light* new_scene_light = new (std::nothrow) _scene_light;

    ASSERT_DEBUG_SYNC(new_scene_light != NULL, "Out of memory");
    if (new_scene_light != NULL)
    {
        memset(new_scene_light,
               0,
               sizeof(_scene_light) );

        new_scene_light->name = name;
        new_scene_light->type = SCENE_LIGHT_TYPE_DIRECTIONAL;

        _scene_light_init(new_scene_light);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_light,
                                                       _scene_light_release,
                                                       OBJECT_TYPE_SCENE_LIGHT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_light) new_scene_light;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_light_create_point(__in __notnull system_hashed_ansi_string name)
{
    _scene_light* new_scene_light = new (std::nothrow) _scene_light;

    ASSERT_DEBUG_SYNC(new_scene_light != NULL, "Out of memory");
    if (new_scene_light != NULL)
    {
        memset(new_scene_light,
               0,
               sizeof(_scene_light) );

        new_scene_light->name = name;
        new_scene_light->type = SCENE_LIGHT_TYPE_POINT;

        _scene_light_init(new_scene_light);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_light,
                                                       _scene_light_release,
                                                       OBJECT_TYPE_SCENE_LIGHT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_light) new_scene_light;
}

/* Please see header for spec */
PUBLIC EMERALD_API void scene_light_get_property(__in  __notnull scene_light          light,
                                                 __in            scene_light_property property,
                                                 __out __notnull void*                out_result)
{
    _scene_light* light_ptr = (_scene_light*) light;

    switch (property)
    {
        case SCENE_LIGHT_PROPERTY_COLOR:
        {
            if (light_ptr->color[0] == NULL ||
                light_ptr->color[1] == NULL ||
                light_ptr->color[2] == NULL)
            {
                _scene_light_init_default_color_curves(light_ptr);
            }

            memcpy(out_result,
                   light_ptr->color,
                   sizeof(light_ptr->color) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_COLOR_INTENSITY:
        {
            if (light_ptr->color_intensity == NULL)
            {
                _scene_light_init_default_color_intensity_curve(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->color_intensity;

            break;
        }

        case SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION property only available for point lights");

            if (light_ptr->constant_attenuation == NULL)
            {
                _scene_light_init_default_attenuation_curves(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->constant_attenuation;

            break;
        }

        case SCENE_LIGHT_PROPERTY_DIRECTION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL,
                              "SCENE_LIGHT_PROPERTY_DIRECTION property only available for directional lights");

            memcpy(out_result,
                   light_ptr->direction,
                   sizeof(light_ptr->direction) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE:
        {
            *(scene_graph_node*) out_result = light_ptr->graph_owner_node;

            break;
        }

        case SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION property only available for point lights");

            if (light_ptr->linear_attenuation == NULL)
            {
                _scene_light_init_default_attenuation_curves(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->linear_attenuation;

            break;
        }

        case SCENE_LIGHT_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = light_ptr->name;

            break;
        }

        case SCENE_LIGHT_PROPERTY_POSITION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_POSITION property only available for point lights");

            memcpy(out_result,
                   light_ptr->position,
                   sizeof(light_ptr->position) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION property only available for point lights");

            if (light_ptr->quadratic_attenuation == NULL)
            {
                _scene_light_init_default_attenuation_curves(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->quadratic_attenuation;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT:
        {
            *(ogl_texture_internalformat*) out_result = light_ptr->shadow_map_internalformat;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE:
        {
            memcpy(out_result,
                   light_ptr->shadow_map_size,
                   sizeof(light_ptr->shadow_map_size) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE:
        {
            *(ogl_texture*) out_result = light_ptr->shadow_map_texture;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP:
        {
            *(system_matrix4x4*) out_result = light_ptr->shadow_map_vp;

            break;
        }

        case SCENE_LIGHT_PROPERTY_TYPE:
        {
            *((scene_light_type*) out_result) = light_ptr->type;

            break;
        }

        case SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP:
        {
            *(bool*) out_result = light_ptr->uses_shadow_map;

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
PUBLIC scene_light scene_light_load(__in __notnull system_file_serializer serializer,
                                    __in_opt       scene                  owner_scene)
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

    switch (light_type)
    {
        case SCENE_LIGHT_TYPE_AMBIENT:
        {
            result_light = scene_light_create_ambient(light_name);

            break;
        }

        case SCENE_LIGHT_TYPE_DIRECTIONAL:
        {
            result_light = scene_light_create_directional(light_name);

            break;
        }

        case SCENE_LIGHT_TYPE_POINT:
        {
            result_light = scene_light_create_point(light_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized light type");

            goto end_error;
        }
    } /* switch (light_type) */

    ASSERT_ALWAYS_SYNC(result_light != NULL, "Out of memory");
    if (result_light != NULL)
    {
        _scene_light* result_light_ptr = (_scene_light*) result_light;

        for (uint32_t n_color_component = 0;
                      n_color_component < sizeof(result_light_ptr->color) / sizeof(result_light_ptr->color[0]);
                    ++n_color_component)
        {
            result &= _scene_light_load_curve(owner_scene,
                                              result_light_ptr->color + n_color_component,
                                              serializer);
        }

        result &= _scene_light_load_curve(owner_scene,
                                         &result_light_ptr->color_intensity,
                                          serializer);

        if (light_type == SCENE_LIGHT_TYPE_DIRECTIONAL)
        {
            result &= system_file_serializer_read(serializer,
                                                  sizeof(result_light_ptr->direction),
                                                  result_light_ptr->direction);
        }
        else
        if (light_type == SCENE_LIGHT_TYPE_POINT)
        {
            result &= _scene_light_load_curve(owner_scene,
                                             &result_light_ptr->constant_attenuation,
                                              serializer);
            result &= _scene_light_load_curve(owner_scene,
                                             &result_light_ptr->linear_attenuation,
                                              serializer);
            result &= _scene_light_load_curve(owner_scene,
                                             &result_light_ptr->quadratic_attenuation,
                                              serializer);
        }

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
                             __in __notnull const scene_light      light,
                             __in_opt       scene                  owner_scene)
{
    const _scene_light* light_ptr = (const _scene_light*) light;
    bool                result    = true;

    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              light_ptr->name);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(light_ptr->type),
                                                             &light_ptr->type);

    for (uint32_t n_color_component = 0;
                  n_color_component < sizeof(light_ptr->color) / sizeof(light_ptr->color[0]);
                ++n_color_component)
    {
        result &= _scene_light_save_curve(owner_scene,
                                          light_ptr->color[n_color_component],
                                          serializer);
    }

    result &= _scene_light_save_curve(owner_scene,
                                      light_ptr->color_intensity,
                                      serializer);

    if (light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL)
    {
        result &= system_file_serializer_write(serializer,
                                               sizeof(light_ptr->direction),
                                               light_ptr->direction);
    }
    else
    if (light_ptr->type == SCENE_LIGHT_TYPE_POINT)
    {
        result &= _scene_light_save_curve(owner_scene,
                                          light_ptr->constant_attenuation,
                                          serializer);
        result &= _scene_light_save_curve(owner_scene,
                                          light_ptr->linear_attenuation,
                                          serializer);
        result &= _scene_light_save_curve(owner_scene,
                                          light_ptr->quadratic_attenuation,
                                          serializer);
    }

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
        case SCENE_LIGHT_PROPERTY_COLOR:
        {
            for (unsigned int n_component = 0;
                              n_component < 3;
                            ++n_component)
            {
                if (light_ptr->color[n_component] != NULL)
                {
                    curve_container_release(light_ptr->color[n_component]);

                    light_ptr->color[n_component] = NULL;
                }
            }

            memcpy(light_ptr->color,
                   data,
                   sizeof(light_ptr->color) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_COLOR_INTENSITY:
        {
            if (light_ptr->color_intensity != NULL)
            {
                curve_container_release(light_ptr->color_intensity);

                light_ptr->color_intensity = NULL;
            }

            light_ptr->color_intensity = *(curve_container*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION property only settable for point lights");

            if (light_ptr->constant_attenuation != NULL)
            {
                curve_container_release(light_ptr->constant_attenuation);

                light_ptr->constant_attenuation = NULL;
            }

            light_ptr->constant_attenuation = *(curve_container*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_DIRECTION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL,
                              "SCENE_LIGHT_PROPERTY_DIRECTION property only settable for directional lights");

            memcpy(light_ptr->direction,
                   data,
                   sizeof(light_ptr->direction) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE:
        {
            light_ptr->graph_owner_node = *(scene_graph_node*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION property only settable for point lights");

            if (light_ptr->linear_attenuation != NULL)
            {
                curve_container_release(light_ptr->linear_attenuation);

                light_ptr->linear_attenuation = NULL;
            }

            light_ptr->linear_attenuation = *(curve_container*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_POSITION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_POSITION property only available for point lights");

            for (unsigned int n_component = 0;
                              n_component < sizeof(light_ptr->position) / sizeof(light_ptr->position[0]);
                            ++n_component)
            {
                memcpy(light_ptr->position,
                       data,
                       sizeof(light_ptr->position) );
            }

            break;
        }

        case SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT,
                              "SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION property only settable for point lights");

            if (light_ptr->quadratic_attenuation != NULL)
            {
                curve_container_release(light_ptr->quadratic_attenuation);

                light_ptr->quadratic_attenuation = NULL;
            }

            light_ptr->quadratic_attenuation = *(curve_container*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT:
        {
            light_ptr->shadow_map_internalformat = *(ogl_texture_internalformat*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE:
        {
            memcpy(light_ptr->shadow_map_size,
                   data,
                   sizeof(light_ptr->shadow_map_size) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE:
        {
            /* NOTE: ogl_texture is considered a state by scene_light. It's not used
             *       by scene_light in any way, so its reference counter is left intact.
             */
            light_ptr->shadow_map_texture = *(ogl_texture*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP:
        {
            light_ptr->uses_shadow_map = *(bool*) data;

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
