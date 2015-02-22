
/**
 *
 * Emerald (kbi/elude @2014-2015)
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
    curve_container                             color    [3];
    curve_container                             color_intensity;
    curve_container                             cone_angle_half;
    curve_container                             constant_attenuation;
    float                                       direction[3];
    curve_container                             edge_angle;
    scene_light_falloff                         falloff;
    scene_graph_node                            graph_owner_node;
    curve_container                             linear_attenuation;
    system_hashed_ansi_string                   name;
    float                                       position[3];
    curve_container                             quadratic_attenuation;
    curve_container                             range;
    scene_light_shadow_map_bias                 shadow_map_bias;                  /* NOTE: This property affects the generated ogl_uber! */
    bool                                        shadow_map_cull_front_faces;
    scene_light_shadow_map_filtering            shadow_map_filtering;             /* NOTE: This property affects the generated ogl_uber! */
    ogl_texture_internalformat                  shadow_map_internalformat;
    scene_light_shadow_map_pointlight_algorithm shadow_map_pointlight_algorithm;  /* NOTE: This property affects the generated ogl_uber! */
    float                                       shadow_map_pointlight_far_plane;
    float                                       shadow_map_pointlight_near_plane;
    system_matrix4x4                            shadow_map_projection;
    float                                       shadow_map_spotlight_near_plane;
    unsigned int                                shadow_map_size[2];
    ogl_texture                                 shadow_map_texture;
    system_matrix4x4                            shadow_map_view;
    system_matrix4x4                            shadow_map_vp;
    scene_light_type                            type;
    bool                                        uses_shadow_map;

    REFCOUNT_INSERT_VARIABLES
} _scene_light;

/* Forward declarations */
PRIVATE void _scene_light_init_default_attenuation_curves   (__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init_default_color_curves         (__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init_default_color_intensity_curve(__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init_default_point_light_curves   (__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init_default_spot_light_curves    (__in     __notnull _scene_light*          light_ptr);
PRIVATE void _scene_light_init                              (__in     __notnull _scene_light*          light_ptr);
PRIVATE bool _scene_light_load_curve                        (__in_opt           scene                  owner_scene,
                                                             __in     __notnull curve_container*       curve_ptr,
                                                             __in     __notnull system_file_serializer serializer);
PRIVATE void _scene_light_release                           (                   void*                  data_ptr);
PRIVATE bool _scene_light_save_curve                        (__in_opt           scene                  owner_scene,
                                                             __in     __notnull curve_container        in_curve,
                                                             __in     __notnull system_file_serializer serializer);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_light,
                               scene_light,
                              _scene_light);


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
PRIVATE void _scene_light_init_default_point_light_curves(__in __notnull _scene_light* light_ptr)
{
    system_variant temp_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    /* Range */
    ASSERT_DEBUG_SYNC(light_ptr->range == NULL,
                      "Light edge angle curve already instantiated");

    light_ptr->range = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                      " range"),
                                              SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (temp_variant,
                                      DEG_TO_RAD(2.5f) );
    curve_container_set_default_value(light_ptr->range,
                                      temp_variant);

    /* Clean up */
    system_variant_release(temp_variant);
}

/** TODO */
PRIVATE void _scene_light_init_default_spot_light_curves(__in __notnull _scene_light* light_ptr)
{
    system_variant temp_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    /* Cone angle */
    ASSERT_DEBUG_SYNC(light_ptr->cone_angle_half == NULL,
                      "Light cone angle half curve already instantiated");

    light_ptr->cone_angle_half = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                                " cone angle (half)"),
                                                        SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (temp_variant,
                                      DEG_TO_RAD(30.0f) );
    curve_container_set_default_value(light_ptr->cone_angle_half,
                                      temp_variant);

    /* Edge angle */
    ASSERT_DEBUG_SYNC(light_ptr->edge_angle == NULL,
                      "Light edge angle curve already instantiated");

    light_ptr->edge_angle = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(light_ptr->name),
                                                                                                           " edge angle"),
                                                   SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (temp_variant,
                                      DEG_TO_RAD(20.0f) );
    curve_container_set_default_value(light_ptr->edge_angle,
                                      temp_variant);

    /* Clean up */
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
    light_ptr->cone_angle_half       = NULL;
    light_ptr->constant_attenuation  = NULL;
    light_ptr->edge_angle            = NULL;
    light_ptr->falloff               = SCENE_LIGHT_FALLOFF_LINEAR;
    light_ptr->linear_attenuation    = NULL;
    light_ptr->quadratic_attenuation = NULL;
    light_ptr->range                 = NULL;

    light_ptr->shadow_map_bias                  = SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE;
    light_ptr->shadow_map_cull_front_faces      = (light_ptr->type != SCENE_LIGHT_TYPE_POINT &&
                                                   light_ptr->type != SCENE_LIGHT_TYPE_SPOT);
    light_ptr->shadow_map_filtering             = SCENE_LIGHT_SHADOW_MAP_FILTERING_PCF;
    light_ptr->shadow_map_internalformat        = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT24;
    light_ptr->shadow_map_pointlight_algorithm  = SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID;
    light_ptr->shadow_map_pointlight_far_plane  = 0.0f;
    light_ptr->shadow_map_pointlight_near_plane = 0.1f;
    light_ptr->shadow_map_projection            = system_matrix4x4_create();
    light_ptr->shadow_map_spotlight_near_plane  = 0.1f;
    light_ptr->shadow_map_size[0]               = DEFAULT_SHADOW_MAP_SIZE;
    light_ptr->shadow_map_size[1]               = DEFAULT_SHADOW_MAP_SIZE;
    light_ptr->shadow_map_texture               = NULL;
    light_ptr->shadow_map_view                  = system_matrix4x4_create();
    light_ptr->shadow_map_vp                    = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(light_ptr->shadow_map_projection);
    system_matrix4x4_set_to_identity(light_ptr->shadow_map_view);
    system_matrix4x4_set_to_identity(light_ptr->shadow_map_vp);

    /* Direction: used by directional & spot lights */
    if (light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
        light_ptr->type == SCENE_LIGHT_TYPE_SPOT)
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
       &light_ptr->cone_angle_half,
       &light_ptr->constant_attenuation,
       &light_ptr->edge_angle,
       &light_ptr->linear_attenuation,
       &light_ptr->quadratic_attenuation,
       &light_ptr->range
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

    if (light_ptr->shadow_map_projection != NULL)
    {
        system_matrix4x4_release(light_ptr->shadow_map_projection);

        light_ptr->shadow_map_projection = NULL;
    }

    if (light_ptr->shadow_map_view != NULL)
    {
        system_matrix4x4_release(light_ptr->shadow_map_view);

        light_ptr->shadow_map_view = NULL;
    }

    if (light_ptr->shadow_map_vp != NULL)
    {
        system_matrix4x4_release(light_ptr->shadow_map_vp);

        light_ptr->shadow_map_vp = NULL;
    }

    /* NOTE: We do not release the shadow map texture at this point, as it is
     *       owned by the texture pool.
     */
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
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
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
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
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
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_light) new_scene_light;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_light scene_light_create_spot(__in __notnull system_hashed_ansi_string name)
{
    _scene_light* new_scene_light = new (std::nothrow) _scene_light;

    ASSERT_DEBUG_SYNC(new_scene_light != NULL, "Out of memory");
    if (new_scene_light != NULL)
    {
        memset(new_scene_light,
               0,
               sizeof(_scene_light) );

        new_scene_light->name = name;
        new_scene_light->type = SCENE_LIGHT_TYPE_SPOT;

        _scene_light_init(new_scene_light);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_scene_light,
                                                       _scene_light_release,
                                                       OBJECT_TYPE_SCENE_LIGHT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Lights\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
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

        case SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF property only available for spot lights");

            if (light_ptr->cone_angle_half == NULL)
            {
                _scene_light_init_default_spot_light_curves(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->cone_angle_half;

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
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_DIRECTION property only available for directional & spot lights");

            memcpy(out_result,
                   light_ptr->direction,
                   sizeof(light_ptr->direction) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_EDGE_ANGLE:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_EDGE_ANGLE property only available for spot lights");

            if (light_ptr->edge_angle == NULL)
            {
                _scene_light_init_default_spot_light_curves(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->edge_angle;

            break;
        }

        case SCENE_LIGHT_PROPERTY_FALLOFF:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_FALLOFF property only available for point / spot lights");

            *(scene_light_falloff*) out_result = light_ptr->falloff;

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
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_POSITION property only available for point & spot lights");

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

        case SCENE_LIGHT_PROPERTY_RANGE:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_RANGE property only available for point & spot lights");

            if (light_ptr->range == NULL)
            {
                _scene_light_init_default_point_light_curves(light_ptr);
            }

            *(curve_container*) out_result = light_ptr->range;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS:
        {
            *(scene_light_shadow_map_bias*) out_result = light_ptr->shadow_map_bias;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_CULL_FRONT_FACES:
        {
            *(bool*) out_result = light_ptr->shadow_map_cull_front_faces;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING:
        {
            *(scene_light_shadow_map_filtering*) out_result = light_ptr->shadow_map_filtering;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT:
        {
            *(ogl_texture_internalformat*) out_result = light_ptr->shadow_map_internalformat;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM:
        {
            *(scene_light_shadow_map_pointlight_algorithm*) out_result = light_ptr->shadow_map_pointlight_algorithm;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE:
        {
            *(float*) out_result = light_ptr->shadow_map_pointlight_far_plane;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE:
        {
            *(float*) out_result = light_ptr->shadow_map_pointlight_near_plane;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_PROJECTION:
        {
            *(system_matrix4x4*) out_result = light_ptr->shadow_map_projection;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE:
        {
            memcpy(out_result,
                   light_ptr->shadow_map_size,
                   sizeof(light_ptr->shadow_map_size) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_SPOTLIGHT_NEAR_PLANE:
        {
            *(float*) out_result = light_ptr->shadow_map_spotlight_near_plane;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE:
        {
            *(ogl_texture*) out_result = light_ptr->shadow_map_texture;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_VIEW:
        {
            *(system_matrix4x4*) out_result = light_ptr->shadow_map_view;

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
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_falloff_has(__in scene_light_falloff falloff)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* falloff_custom_name                   = "custom";
    static const char* falloff_inversed_distance_name        = "inversed distance";
    static const char* falloff_inversed_distance_square_name = "inversed distance squared";
    static const char* falloff_linear_name                   = "linear";
    static const char* falloff_off_name                      = "off";

    switch (falloff)
    {
        case SCENE_LIGHT_FALLOFF_OFF:
        {
            result = system_hashed_ansi_string_create(falloff_off_name);

            break;
        }

        case SCENE_LIGHT_FALLOFF_CUSTOM:
        {
            result = system_hashed_ansi_string_create(falloff_custom_name);

            break;
        }

        case SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE:
        {
            result = system_hashed_ansi_string_create(falloff_inversed_distance_name);

            break;
        }

        case SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE_SQUARE:
        {
            result = system_hashed_ansi_string_create(falloff_inversed_distance_square_name);

            break;
        }

        case SCENE_LIGHT_FALLOFF_LINEAR:
        {
            result = system_hashed_ansi_string_create(falloff_linear_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light_falloff value.");
        }
    } /* switch (falloff) */

    return result;
}

/* Please see header for spec */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_bias_has(__in scene_light_shadow_map_bias bias)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* bias_adaptive_fast_name = "adaptive bias (fast)";
    static const char* bias_adaptive_name      = "adaptive bias";
    static const char* bias_constant_name      = "constant bias";
    static const char* bias_none_name          = "no bias";

    switch (bias)
    {
        case SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE:
        {
            result = system_hashed_ansi_string_create(bias_adaptive_name);

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE_FAST:
        {
            result = system_hashed_ansi_string_create(bias_adaptive_fast_name);

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_CONSTANT:
        {
            result = system_hashed_ansi_string_create(bias_constant_name);

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_BIAS_NONE:
        {
            result = system_hashed_ansi_string_create(bias_none_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light_shadow_map_bias value.");
        }
    } /* switch (bias) */

    return result;
}

/* Please see header for spec */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_filtering_has(__in scene_light_shadow_map_filtering filtering)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* filtering_pcf_name   = "pcf";
    static const char* filtering_plain_name = "plain";

    switch (filtering)
    {
        case SCENE_LIGHT_SHADOW_MAP_FILTERING_PCF:
        {
            result = system_hashed_ansi_string_create(filtering_pcf_name);

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_FILTERING_PLAIN:
        {
            result = system_hashed_ansi_string_create(filtering_plain_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light_shadow_map_filtering value.");
        }
    } /* switch (filtering) */

    return result;
}

/* Please see header for spec */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_pointlight_algorithm_has(__in scene_light_shadow_map_pointlight_algorithm algorithm)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* algorithm_cubical_name         = "cubical";
    static const char* algorithm_dual_paraboloid_name = "dual paraboloid";

    switch (algorithm)
    {
        case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL:
        {
            result = system_hashed_ansi_string_create(algorithm_cubical_name);

            break;
        }

        case SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID:
        {
            result = system_hashed_ansi_string_create(algorithm_dual_paraboloid_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light_shadow_map_pointlight_algorithm value.");
        }
    } /* switch (algorithm) */

    return result;
}

/* Please see header for spec */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_type_has(__in scene_light_type light_type)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* light_ambient_name     = "ambient";
    static const char* light_directional_name = "directional";
    static const char* light_point_name       = "point";
    static const char* light_spot_name        = "spot";

    switch (light_type)
    {
        case SCENE_LIGHT_TYPE_AMBIENT:
       {
           result = system_hashed_ansi_string_create(light_ambient_name);

           break;
       }

        case SCENE_LIGHT_TYPE_DIRECTIONAL:
        {
            result = system_hashed_ansi_string_create(light_directional_name);

            break;
        }

        case SCENE_LIGHT_TYPE_POINT:
        {
            result = system_hashed_ansi_string_create(light_point_name);

            break;
        }

        case SCENE_LIGHT_TYPE_SPOT:
        {
            result = system_hashed_ansi_string_create(light_spot_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_light_type value.");
        }
    } /* switch (light_type) */

    return result;
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

        case SCENE_LIGHT_TYPE_SPOT:
        {
            result_light = scene_light_create_spot(light_name);

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

        if (light_type == SCENE_LIGHT_TYPE_POINT ||
            light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            result &= system_file_serializer_read(serializer,
                                                  sizeof(result_light_ptr->falloff),
                                                 &result_light_ptr->falloff);

            if (result_light_ptr->falloff == SCENE_LIGHT_FALLOFF_CUSTOM)
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
            else
            if (result_light_ptr->falloff == SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE        ||
                result_light_ptr->falloff == SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE_SQUARE ||
                result_light_ptr->falloff == SCENE_LIGHT_FALLOFF_LINEAR)
            {
                result &= _scene_light_load_curve(owner_scene,
                                                 &result_light_ptr->range,
                                                  serializer);
            }
        }

        if (light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            result &= _scene_light_load_curve(owner_scene,
                                             &result_light_ptr->cone_angle_half,
                                              serializer);
            result &= _scene_light_load_curve(owner_scene,
                                             &result_light_ptr->edge_angle,
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

    if (light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
        light_ptr->type == SCENE_LIGHT_TYPE_SPOT)
    {
        result &= system_file_serializer_write(serializer,
                                               sizeof(light_ptr->falloff),
                                              &light_ptr->falloff);

        if (light_ptr->falloff == SCENE_LIGHT_FALLOFF_CUSTOM)
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
        else
        if (light_ptr->falloff == SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE        ||
            light_ptr->falloff == SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE_SQUARE ||
            light_ptr->falloff == SCENE_LIGHT_FALLOFF_LINEAR)
        {
            result &= _scene_light_save_curve(owner_scene,
                                              light_ptr->range,
                                              serializer);
        }
    }

    if (light_ptr->type == SCENE_LIGHT_TYPE_SPOT)
    {
        result &= _scene_light_save_curve(owner_scene,
                                          light_ptr->cone_angle_half,
                                          serializer);
        result &= _scene_light_save_curve(owner_scene,
                                          light_ptr->edge_angle,
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

        case SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF property only settable for spot lights");

            if (light_ptr->cone_angle_half != NULL)
            {
                curve_container_release(light_ptr->cone_angle_half);

                light_ptr->cone_angle_half = NULL;
            }

            light_ptr->cone_angle_half = *(curve_container*) data;

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

        case SCENE_LIGHT_PROPERTY_EDGE_ANGLE:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_EDGE_ANGLE property only settable for spot lights");

            if (light_ptr->edge_angle != NULL)
            {
                curve_container_release(light_ptr->edge_angle);

                light_ptr->edge_angle = NULL;
            }

            light_ptr->edge_angle = *(curve_container*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_FALLOFF:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_FALLOFF property only settable for point & spot lights");

            light_ptr->falloff = *(scene_light_falloff*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_DIRECTION:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_DIRECTION property only settable for directional & spot lights");

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
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_POSITION property only available for point & spot lights");

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

        case SCENE_LIGHT_PROPERTY_RANGE:
        {
            ASSERT_DEBUG_SYNC(light_ptr->type == SCENE_LIGHT_TYPE_POINT ||
                              light_ptr->type == SCENE_LIGHT_TYPE_SPOT,
                              "SCENE_LIGHT_PROPERTY_RANGE property only settable for point & spot lights");

            if (light_ptr->range != NULL)
            {
                curve_container_release(light_ptr->range);

                light_ptr->range = NULL;
            }

            light_ptr->range = *(curve_container*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_CULL_FRONT_FACES:
        {
            light_ptr->shadow_map_cull_front_faces = *(bool*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS:
        {
            light_ptr->shadow_map_bias = *(scene_light_shadow_map_bias*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING:
        {
            light_ptr->shadow_map_filtering = *(scene_light_shadow_map_filtering*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT:
        {
            light_ptr->shadow_map_internalformat = *(ogl_texture_internalformat*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM:
        {
            light_ptr->shadow_map_pointlight_algorithm = *(scene_light_shadow_map_pointlight_algorithm*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE:
        {
            light_ptr->shadow_map_pointlight_far_plane = *(float*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE:
        {
            light_ptr->shadow_map_pointlight_near_plane = *(float*) data;

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE:
        {
            memcpy(light_ptr->shadow_map_size,
                   data,
                   sizeof(light_ptr->shadow_map_size) );

            break;
        }

        case SCENE_LIGHT_PROPERTY_SHADOW_MAP_SPOTLIGHT_NEAR_PLANE:
        {
            light_ptr->shadow_map_spotlight_near_plane = *(float*) data;

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
