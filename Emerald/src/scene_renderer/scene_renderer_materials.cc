/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "mesh/mesh_material.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "scene/scene.h"
#include "scene/scene_light.h"
#include "scene_renderer/scene_renderer_materials.h"
#include "scene_renderer/scene_renderer_sm.h"
#include "scene_renderer/scene_renderer_uber.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_callback_manager.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>


/* Private type definitions */
typedef struct _scene_renderer_materials_mesh_material_setting
{
    mesh_material_property_attachment attachment;
    void*                             attachment_data;
    mesh_material_shading_property    property;

    _scene_renderer_materials_mesh_material_setting()
    {
        attachment      = MESH_MATERIAL_PROPERTY_ATTACHMENT_UNKNOWN;
        attachment_data = nullptr;
        property        = MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN;
    }

    ~_scene_renderer_materials_mesh_material_setting()
    {
        if (attachment_data != nullptr)
        {
            switch (attachment)
            {
                case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
                {
                    delete reinterpret_cast<float*>(attachment_data);

                    attachment_data = nullptr;
                    break;
                }

                case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                {
                    /* ral_texture instance is always owned either by a scene_texture, or by the user.
                     *
                     * DO NOT release the invalidated texture here.
                     */
                    attachment_data = nullptr;
                    break;
                }

                case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                {
                    delete reinterpret_cast<float*>(attachment_data);

                    attachment_data = nullptr;
                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false, 
                                      "Unrecognized mesh material property attachment type");
                }
            }
        }
    }
} _scene_renderer_materials_mesh_material_setting;

typedef struct _scene_renderer_materials_uber_context_data
{
    mesh_material       material;
    scene               owner_scene; /* do not release */
    scene_renderer_uber uber;

    explicit _scene_renderer_materials_uber_context_data()
    {
        material    = nullptr;
        owner_scene = nullptr;
        uber        = nullptr;
    }

    ~_scene_renderer_materials_uber_context_data()
    {
        if (material != nullptr)
        {
            mesh_material_release(material);

            material = nullptr;
        }

        if (uber != nullptr)
        {
            scene_renderer_uber_release(uber);

            uber = nullptr;
        }
    }
} _scene_renderer_materials_uber_context_data;

typedef struct _scene_renderer_materials_uber
{
    system_hash64map per_context_data; /* holds _scene_renderer_materials_uber_context_data instances */
    bool             use_shadow_maps;

    _scene_renderer_materials_uber()
    {
        per_context_data = system_hash64map_create(sizeof(_scene_renderer_materials_uber_context_data*) );
        use_shadow_maps  = false;
    }

    ~_scene_renderer_materials_uber()
    {
        if (per_context_data != nullptr)
        {
            uint32_t n_per_context_data_items = 0;

            system_hash64map_get_property(per_context_data,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_per_context_data_items);

            for (uint32_t n_data_item = 0;
                          n_data_item < n_per_context_data_items;
                        ++n_data_item)
            {
                _scene_renderer_materials_uber_context_data* data_ptr = nullptr;

                system_hash64map_get_element_at(per_context_data,
                                                n_data_item,
                                               &data_ptr,
                                                nullptr /* result_hash_ptr */);

                delete data_ptr;
            }

            system_hash64map_release(per_context_data);
            per_context_data = nullptr;
        }
    }
} _scene_renderer_materials_uber;

typedef struct _scene_renderer_materials
{
    system_resizable_vector callback_enabled_scenes;
    system_resizable_vector forced_mesh_material_settings;
    system_hash64map        per_context_special_materials[SPECIAL_MATERIAL_COUNT]; /* maps ral_context to mesh_material instances */
    system_resizable_vector ubers; /* stores _scene_renderer_materials_uber* */

     _scene_renderer_materials();
    ~_scene_renderer_materials();
} _scene_renderer_materials;


/* Forward declarations */
PRIVATE scene_renderer_uber       _scene_renderer_materials_bake_uber                   (scene_renderer_materials materials,
                                                                                         mesh_material            material,
                                                                                         scene                    scene,
                                                                                         bool                     use_shadow_maps);
PRIVATE system_hashed_ansi_string _scene_renderer_materials_get_uber_name               (mesh_material            material,
                                                                                         scene                    scene,
                                                                                         bool                     use_shadow_maps);
PRIVATE void                      _scene_renderer_materials_on_scene_about_to_be_deleted(const void*              callback_data,
                                                                                         void*                    user_arg);


PRIVATE bool _scene_renderer_materials_does_uber_match_scene (scene_renderer_uber                uber,
                                                              scene                              scene,
                                                              bool                               use_shadow_maps);
PRIVATE void _scene_renderer_materials_get_forced_setting    (scene_renderer_materials           materials,
                                                              mesh_material_shading_property,
                                                              mesh_material_property_attachment* out_attachment,
                                                              void**                             out_attachment_data);
PRIVATE void _scene_renderer_materials_init_special_materials(_scene_renderer_materials*         materials_ptr,
                                                              ral_context                        context);

PRIVATE void _scene_renderer_materials_register_for_callbacks(_scene_renderer_materials*         materials_ptr,
                                                              scene                              scene_to_use,
                                                              bool                               should_register);


_scene_renderer_materials::_scene_renderer_materials()
{
    callback_enabled_scenes       = system_resizable_vector_create(4 /* capacity */);
    forced_mesh_material_settings = system_resizable_vector_create(4 /* capacity */);
    ubers                         = system_resizable_vector_create(4 /* capacity */);

    for (uint32_t n_special_material = 0;
                  n_special_material < SPECIAL_MATERIAL_COUNT;
                ++n_special_material)
    {
        per_context_special_materials[n_special_material] = system_hash64map_create(sizeof(mesh_material) );
    }
}

_scene_renderer_materials::~_scene_renderer_materials()
{
    if (callback_enabled_scenes != nullptr)
    {
        scene current_scene = nullptr;

        while (system_resizable_vector_pop(callback_enabled_scenes,
                                          &current_scene))
        {
            _scene_renderer_materials_register_for_callbacks(this,
                                                             current_scene,
                                                             false); /* should_register */
        }

        system_resizable_vector_release(callback_enabled_scenes);
        callback_enabled_scenes = nullptr;
    }

    if (forced_mesh_material_settings != nullptr)
    {
        _scene_renderer_materials_mesh_material_setting* mesh_material_setting_ptr = nullptr;

        while (system_resizable_vector_pop(forced_mesh_material_settings,
                                          &mesh_material_setting_ptr) )
        {
            delete mesh_material_setting_ptr;

            mesh_material_setting_ptr = nullptr;
        }
    }

    for (scene_renderer_materials_special_material special_material  = SPECIAL_MATERIAL_FIRST;
                                                   special_material != SPECIAL_MATERIAL_COUNT;
                                            ((int&)special_material)++)
    {
        uint32_t n_contexts = 0;

        system_hash64map_get_property(per_context_special_materials[special_material],
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_contexts);

        for (uint32_t n_context = 0;
                      n_context < n_contexts;
                    ++n_context)
        {
            mesh_material context_material = nullptr;

            system_hash64map_get_element_at(per_context_special_materials[special_material],
                                            n_context,
                                           &context_material,
                                            nullptr /* result_hash_ptr */);

            mesh_material_release(context_material);
        }

        system_hash64map_release(per_context_special_materials[special_material]);
        per_context_special_materials[special_material] = nullptr;
    }

    if (ubers != nullptr)
    {
        _scene_renderer_materials_uber* uber_ptr = nullptr;

        while (system_resizable_vector_pop(ubers,
                                          &uber_ptr) )
        {
            delete uber_ptr;
        }
        system_resizable_vector_release(ubers);

        ubers = nullptr;
    }
}

/** TODO */
PRIVATE scene_renderer_uber _scene_renderer_materials_bake_uber(scene_renderer_materials materials,
                                                                mesh_material            material,
                                                                scene                    scene,
                                                                bool                     use_shadow_maps)
{
    _scene_renderer_materials* materials_ptr = reinterpret_cast<_scene_renderer_materials*>(materials);
    ral_context                context       = nullptr;

    LOG_INFO("Performance warning: _scene_renderer_materials_bake_uber() called.");

    /* Spawn a new uber  */
    mesh_material_type        material_type = MESH_MATERIAL_TYPE_UNDEFINED;
    scene_renderer_uber       new_uber      = nullptr;
    system_hashed_ansi_string uber_name     = _scene_renderer_materials_get_uber_name(material,
                                                                                      scene,
                                                                                      use_shadow_maps);

    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_CONTEXT,
                              &context);
    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_TYPE,
                              &material_type);

    switch (material_type)
    {
        case MESH_MATERIAL_TYPE_GENERAL:
        {
            new_uber = scene_renderer_uber_create(context,
                                                  uber_name);

            ASSERT_ALWAYS_SYNC(new_uber != nullptr,
                               "Could not spawn an uber instance");

            if (new_uber != nullptr)
            {
                mesh_material_shading material_shading             = MESH_MATERIAL_SHADING_UNKNOWN;
                bool                  scene_shadow_mapping_enabled = false;

                mesh_material_get_property(material,
                                           MESH_MATERIAL_PROPERTY_SHADING,
                                          &material_shading);
                scene_get_property        (scene,
                                           SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                                          &scene_shadow_mapping_enabled);

                if (material_shading == MESH_MATERIAL_SHADING_LAMBERT ||
                    material_shading == MESH_MATERIAL_SHADING_PHONG)
                {
                    /* Map mesh_material property attachments to shaders_fragment_uber data source equivalents */
                    struct _mapping
                    {
                        mesh_material_shading_property material_property;
                        shaders_fragment_uber_property fragment_uber_property;
                    } mappings[] =
                    {
                        {
                            MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                            SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                            SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                            SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                            SHADERS_FRAGMENT_UBER_PROPERTY_SHININESS_DATA_SOURCE
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                            SHADERS_FRAGMENT_UBER_PROPERTY_SPECULAR_DATA_SOURCE
                        },
                    };
                    const unsigned int n_mappings = sizeof(mappings) / sizeof(mappings[0]);

                    unsigned int n_uber_fragment_property_value_pairs = 0;
                    unsigned int uber_fragment_property_value_pairs[(n_mappings + 1 /* view vector setting */) * 2];

                    /* Add ambient/diffuse/emission factors */
                    for (unsigned int n_mapping = 0;
                                      n_mapping < n_mappings;
                                    ++n_mapping)
                    {
                        _mapping                          mapping             = mappings[n_mapping];
                        mesh_material_property_attachment material_attachment = mesh_material_get_shading_property_attachment_type(material,
                                                                                                                                   mapping.material_property);

                        _scene_renderer_materials_get_forced_setting(materials,
                                                                     mapping.material_property,
                                                                    &material_attachment,
                                                                    nullptr /* out_attachment_data */);

                        if (material_attachment != MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE)
                        {
                            uber_fragment_property_value_pairs[n_uber_fragment_property_value_pairs * 2 + 0] = mapping.fragment_uber_property;

                            switch (material_attachment)
                            {
                                case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
                                {
                                    uber_fragment_property_value_pairs[n_uber_fragment_property_value_pairs * 2 + 1] = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_FLOAT;

                                    break;
                                }

                                case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
                                {
                                    uber_fragment_property_value_pairs[n_uber_fragment_property_value_pairs * 2 + 1] = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_VEC3;

                                    break;
                                }

                                case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
                                {
                                    uber_fragment_property_value_pairs[n_uber_fragment_property_value_pairs * 2 + 1] = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_FLOAT;

                                    break;
                                }

                                case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                                {
                                    uber_fragment_property_value_pairs[n_uber_fragment_property_value_pairs * 2 + 1] = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D;

                                    break;
                                }

                                case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                                {
                                    uber_fragment_property_value_pairs[n_uber_fragment_property_value_pairs * 2 + 1] = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_VEC4;

                                    break;
                                }

                                default:
                                {
                                    ASSERT_DEBUG_SYNC(false,
                                                      "Unrecognized material attachment type");
                                }
                            }

                            n_uber_fragment_property_value_pairs++;
                        }
                    }

                    /* Add lights */
                    if (scene != nullptr)
                    {
                        unsigned int n_scene_lights = 0;

                        scene_get_property(scene,
                                           SCENE_PROPERTY_N_LIGHTS,
                                          &n_scene_lights);

                        for (unsigned int n_scene_light = 0;
                                          n_scene_light < n_scene_lights;
                                        ++n_scene_light)
                        {
                            scene_light      current_light      = scene_get_light_by_index(scene,
                                                                                           n_scene_light);
                            scene_light_type current_light_type = SCENE_LIGHT_TYPE_UNKNOWN;

                            scene_light_get_property(current_light,
                                                     SCENE_LIGHT_PROPERTY_TYPE,
                                                    &current_light_type);

                            /* Determine uber light type, given scene light type and material's BRDF type */
                            shaders_fragment_uber_light_type uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE;

                            switch (material_shading)
                            {
                                case MESH_MATERIAL_SHADING_LAMBERT:
                                {
                                    switch (current_light_type)
                                    {
                                        case SCENE_LIGHT_TYPE_DIRECTIONAL:
                                        {
                                            uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL;

                                            break;
                                        }

                                        case SCENE_LIGHT_TYPE_POINT:
                                        {
                                            uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT;

                                            break;
                                        }

                                        default:
                                        {
                                            ASSERT_DEBUG_SYNC(false,
                                                              "TODO: Unrecognized scene light type for Lambert shading");
                                        }
                                    }

                                    break;
                                }

                                case MESH_MATERIAL_SHADING_PHONG:
                                {
                                    switch (current_light_type)
                                    {
                                        case SCENE_LIGHT_TYPE_AMBIENT:
                                        {
                                            uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT;

                                            break;
                                        }

                                        case SCENE_LIGHT_TYPE_DIRECTIONAL:
                                        {
                                            uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL;

                                            break;
                                        }

                                        case SCENE_LIGHT_TYPE_POINT:
                                        {
                                            uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT;

                                            break;
                                        }

                                        case SCENE_LIGHT_TYPE_SPOT:
                                        {
                                            uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT;

                                            break;
                                        }

                                        default:
                                        {
                                            ASSERT_DEBUG_SYNC(false,
                                                              "TODO: Unrecognized scene light type for phong shading");
                                        }
                                    }

                                    break;
                                }

                                default:
                                {
                                    ASSERT_DEBUG_SYNC(false,
                                                      "TODO: Unsupported shading algorithm");
                                }
                            }

                            /* Determine if we should check visibility for the light.
                             *
                             * Note that scene has a property called SCENE_PROPERTY_SHADOW_MAPPING_ENABLED which
                             * should be ANDed with light's property.
                             *
                             * Also note that we must not forget about the global "use shadow maps" setting here.
                             */
                            bool uses_shadow_mapping = false;

                            scene_light_get_property(current_light,
                                                     SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                                    &uses_shadow_mapping);

                            uses_shadow_mapping &= scene_shadow_mapping_enabled;
                            uses_shadow_mapping &= use_shadow_maps;

                            /* Add the light item if not a nullptr light */
                            if (uber_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE)
                            {
                                scene_renderer_uber_add_light_item(new_uber,
                                                                   current_light,
                                                                   uber_light_type,
                                                                   uses_shadow_mapping,
                                                                   n_uber_fragment_property_value_pairs,
                                                                   uber_fragment_property_value_pairs);
                            }
                        }
                    }
                }
                else
                if (material_shading == MESH_MATERIAL_SHADING_NONE)
                {
                    /* Nothing to be done here! */
                }
                else
                {
                    /* Sanity checks */
                    mesh_material_input_fragment_attribute input_fragment_attribute   = MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN;
                    mesh_material_property_attachment      input_attribute_attachment = MESH_MATERIAL_PROPERTY_ATTACHMENT_UNKNOWN;

                    ASSERT_DEBUG_SYNC(material_shading == MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE,
                                      "Unrecognized mesh material shading");

                    input_attribute_attachment = mesh_material_get_shading_property_attachment_type(material,
                                                                                                    MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE);

                    ASSERT_DEBUG_SYNC(input_attribute_attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE,
                                      "Invalid input attribute attachment");

                    /* Retrieve the attribute we want to use as color data source */
                    mesh_material_get_shading_property_value_input_fragment_attribute(material,
                                                                                      MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                                     &input_fragment_attribute);

                    /* Configure the ogl_uber instance */
                    scene_renderer_uber_input_fragment_attribute uber_input_attribute = SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN;

                    switch (input_fragment_attribute)
                    {
                        case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL:
                        {
                            uber_input_attribute = SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL;

                            break;
                        }

                        case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD:
                        {
                            uber_input_attribute = SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD;

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized input fragment attribute");
                        }
                    }

                    if (uber_input_attribute != SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN)
                    {
                        scene_renderer_uber_add_input_fragment_attribute_item(new_uber,
                                                                              uber_input_attribute);
                    }
                }
            }

            break;
        }

        case MESH_MATERIAL_TYPE_PROGRAM:
        {
            ral_program material_program = nullptr;

            mesh_material_get_property(material,
                                       MESH_MATERIAL_PROPERTY_SOURCE_RAL_PROGRAM,
                                      &material_program);

            new_uber = scene_renderer_uber_create_from_ral_program(context,
                                                                   uber_name,
                                                                   material_program);

            ASSERT_ALWAYS_SYNC(new_uber != nullptr,
                               "Could not spawn an uber instance");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_type value.");
        }
    }

    /* Return the result */
    return (scene_renderer_uber) new_uber;
}

/** TODO. **/
PRIVATE bool _scene_renderer_materials_does_uber_match_scene(scene_renderer_uber uber,
                                                             scene               scene,
                                                             bool                use_shadow_mapping)
{
    bool result = true;

    /* The purpose of this function is to make sure that uber we're fed implements shading
     * for exactly the same light configuration as defined in scene.
     */
    scene_light  current_light    = nullptr;
    unsigned int n_scene_lights   = 0;
    unsigned int n_uber_lights    = 0;
    bool         scene_sm_enabled = false;

    scene_get_property                             (scene,
                                                    SCENE_PROPERTY_N_LIGHTS,
                                                   &n_scene_lights);
    scene_get_property                             (scene,
                                                    SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                                                   &scene_sm_enabled);
    scene_renderer_uber_get_shader_general_property(uber,
                                                    SCENE_RENDERER_UBER_GENERAL_PROPERTY_N_ITEMS,
                                                   &n_uber_lights);

    if (n_uber_lights != n_scene_lights)
    {
        /* Nope */
        result = false;

        goto end;
    }

    /* Let's make sure the light types match on both ends */
    for (unsigned int n_light = 0;
                      n_light < n_uber_lights;
                    ++n_light)
    {
        scene_light_falloff              current_light_falloff              = SCENE_LIGHT_FALLOFF_UNKNOWN;
        bool                             current_light_is_shadow_caster     = false;
        scene_light_shadow_map_bias      current_light_shadow_map_bias      = SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN;
        scene_light_type                 current_light_type                 = SCENE_LIGHT_TYPE_UNKNOWN;
        scene_light_falloff              current_uber_item_falloff          = SCENE_LIGHT_FALLOFF_UNKNOWN;
        bool                             current_uber_item_is_shadow_caster = false;
        shaders_fragment_uber_light_type current_uber_item_light_type       = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE;
        scene_light_shadow_map_bias      current_uber_item_shadow_map_bias  = SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN;
        scene_renderer_uber_item_type    current_uber_item_type             = SCENE_RENDERER_UBER_ITEM_UNKNOWN;

        current_light = scene_get_light_by_index(scene,
                                                 n_light);

        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS,
                                &current_light_shadow_map_bias);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                &current_light_is_shadow_caster);
        scene_light_get_property(current_light,
                                 SCENE_LIGHT_PROPERTY_TYPE,
                                &current_light_type);

        scene_renderer_uber_get_shader_item_property(uber,
                                                     n_light,
                                                     SCENE_RENDERER_UBER_ITEM_PROPERTY_TYPE,
                                                    &current_uber_item_type);

        if (current_uber_item_type != SCENE_RENDERER_UBER_ITEM_LIGHT)
        {
            /* Nope */
            result = false;

            goto end;
        }

        /* Check if the light should be considered a shadow caster. Note that this setting
         * is overriden by a scene property called SCENE_PROPERTY_SHADOW_MAPPING_ENABLED
         * which is toggled on and off at different stages of the rendering pipeline execution.
         */
        scene_renderer_uber_get_shader_item_property(uber,
                                                     n_light,
                                                     SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP,
                                                    &current_uber_item_is_shadow_caster);

        current_light_is_shadow_caster &= (use_shadow_mapping && scene_sm_enabled);

        /* Make sure the shadow mapping algorithms match */
        scene_light_shadow_map_algorithm current_light_shadow_map_algorithm     = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
        scene_light_shadow_map_algorithm current_uber_item_shadow_map_algorithm = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;

        scene_light_get_property                    (current_light,
                                                     SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                                                    &current_light_shadow_map_algorithm);
        scene_renderer_uber_get_shader_item_property(uber,
                                                     n_light,
                                                     SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM,
                                                    &current_uber_item_shadow_map_algorithm);

        if (current_light_shadow_map_algorithm != current_uber_item_shadow_map_algorithm)
        {
            /* Nope */
            result = false;

            goto end;
        }

        /* Carry on with other light stuff */
        if (current_light_type == SCENE_LIGHT_TYPE_POINT ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_FALLOFF,
                                    &current_light_falloff);

            scene_renderer_uber_get_shader_item_property(uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_FALLOFF,
                                                        &current_uber_item_falloff);

            if (current_light_falloff != current_uber_item_falloff)
            {
                /* Nope */
                result = false;

                goto end;
            }
        }

        if (current_light_type == SCENE_LIGHT_TYPE_POINT)
        {
            scene_light_shadow_map_pointlight_algorithm current_light_shadow_map_pointlight_algorithm     = SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_UNKNOWN;
            scene_light_shadow_map_pointlight_algorithm current_uber_item_shadow_map_pointlight_algorithm = SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_UNKNOWN;

            scene_light_get_property                    (current_light,
                                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                                        &current_light_shadow_map_pointlight_algorithm);
            scene_renderer_uber_get_shader_item_property(uber,
                                                         n_light,
                                                         SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                                        &current_uber_item_shadow_map_pointlight_algorithm);

            if (current_light_shadow_map_pointlight_algorithm != current_uber_item_shadow_map_pointlight_algorithm)
            {
                /* Nope */
                result = false;

                goto end;
            }
        }

        scene_renderer_uber_get_shader_item_property(uber,
                                                     n_light,
                                                     SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS,
                                                    &current_uber_item_shadow_map_bias);
        scene_renderer_uber_get_shader_item_property(uber,
                                                     n_light,
                                                     SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_TYPE,
                                                    &current_uber_item_light_type);

        /* TODO: Expand to support other light types */
        ASSERT_DEBUG_SYNC(current_light_type == SCENE_LIGHT_TYPE_AMBIENT     ||
                          current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
                          current_light_type == SCENE_LIGHT_TYPE_POINT       ||
                          current_light_type == SCENE_LIGHT_TYPE_SPOT,
                          "TODO: Unsupported light type, expand.");

        ASSERT_DEBUG_SYNC(current_uber_item_light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT             ||
                          current_uber_item_light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL ||
                          current_uber_item_light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT       ||
                          current_uber_item_light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL   ||
                          current_uber_item_light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT         ||
                          current_uber_item_light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT,
                          "Unrecognized uber item light type.");

        if (current_light_type == SCENE_LIGHT_TYPE_AMBIENT     &&  current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT             ||
            current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL && (current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL &&
                                                                   current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL)  ||
            current_light_type == SCENE_LIGHT_TYPE_POINT       && (current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT       &&
                                                                   current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT         &&
                                                                   current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)         ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT        && (current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT) )
        {
            /* Nope */
            result = false;

            goto end;
        }

        if (current_uber_item_shadow_map_bias != current_light_shadow_map_bias)
        {
            /* Nope */
            result = false;

            goto end;
        }

        if (current_uber_item_is_shadow_caster != current_light_is_shadow_caster)
        {
            /* Nope */
            result = false;

            goto end;
        }
    }

    /* Hey, that's a match! */
end:
    /* Done */
    return result;
}

/** TODO */
PRIVATE void _scene_renderer_materials_get_forced_setting(scene_renderer_materials           materials,
                                                          mesh_material_shading_property     shading_property,
                                                          mesh_material_property_attachment* out_attachment,
                                                          void**                             out_attachment_data)
{
    const _scene_renderer_materials* materials_ptr     = reinterpret_cast<const _scene_renderer_materials*>(materials);
    unsigned int                     n_forced_settings = 0;

    system_resizable_vector_get_property(materials_ptr->forced_mesh_material_settings,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_forced_settings);

    for (unsigned int n_setting = 0;
                      n_setting < n_forced_settings;
                    ++n_setting)
    {
        _scene_renderer_materials_mesh_material_setting* setting_ptr = nullptr;

        if (system_resizable_vector_get_element_at(materials_ptr->forced_mesh_material_settings,
                                                   n_setting,
                                                  &setting_ptr) )
        {
            if (setting_ptr->property == shading_property)
            {
                if (out_attachment != nullptr)
                {
                    *out_attachment = setting_ptr->attachment;
                }

                if (out_attachment_data != nullptr)
                {
                    *out_attachment_data = setting_ptr->attachment_data;
                }

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieved forced mesh material setting descriptor");
        }
    }
}

/** TODO */
PRIVATE system_hashed_ansi_string _scene_renderer_materials_get_uber_name(mesh_material material,
                                                                          scene         scene,
                                                                          bool          use_shadow_maps)
{
    system_hashed_ansi_string material_name = nullptr;
    std::stringstream         name_sstream;
    std::string               name_string;

    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_NAME,
                              &material_name);

    /* Prefix */
    name_sstream << "\n"
                 << "Material name:["
                 << system_hashed_ansi_string_get_buffer(material_name)
                 << "]\n";

    /* Type */
    mesh_material_type material_type = MESH_MATERIAL_TYPE_UNDEFINED;

    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_TYPE,
                              &material_type);

    name_sstream << "Type:["
                 << system_hashed_ansi_string_get_buffer(mesh_material_get_mesh_material_type_has(material_type) )
                 << "]\n";

    if (material_type == MESH_MATERIAL_TYPE_GENERAL)
    {
        /* Shading */
        mesh_material_shading material_shading = MESH_MATERIAL_SHADING_UNKNOWN;

        mesh_material_get_property(material,
                                   MESH_MATERIAL_PROPERTY_SHADING,
                                  &material_shading);

        name_sstream << "Shading:["
                     << system_hashed_ansi_string_get_buffer(mesh_material_get_mesh_material_shading_has(material_shading) )
                     << "]\n";

        /* Iterate over all shading properties, whose attachments are not MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE */
        name_sstream << "Mesh material shading properties:\n>>\n";

        for (mesh_material_shading_property current_property  = MESH_MATERIAL_SHADING_PROPERTY_FIRST;
                                            current_property != MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                                    ++(int&)current_property)
        {
            mesh_material_property_attachment property_attachment = mesh_material_get_shading_property_attachment_type(material,
                                                                                                                       current_property);

            if (property_attachment != MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE)
            {
                system_hashed_ansi_string current_attachment_has = mesh_material_get_mesh_material_property_attachment_has(property_attachment);
                system_hashed_ansi_string current_property_has   = mesh_material_get_mesh_material_shading_property_has   (current_property);

                name_sstream << "Property ["
                             << system_hashed_ansi_string_get_buffer(current_property_has)
                             << "] uses an attachment of type ["
                             << system_hashed_ansi_string_get_buffer(current_attachment_has)
                             << "]\n";
            }
        }

        name_sstream << "<<\n\n";

        /* Store material info details, if available */
        if (material_shading != MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE &&
            material_shading != MESH_MATERIAL_SHADING_NONE)
        {
            /* Light configuration */
            unsigned int n_lights = 0;

            scene_get_property(scene,
                               SCENE_PROPERTY_N_LIGHTS,
                              &n_lights);

            name_sstream << "Lights:["
                         << n_lights
                         << "]:\n>>\n";

            for (unsigned int n_light = 0;
                              n_light < n_lights;
                            ++n_light)
            {
                scene_light               current_light           = scene_get_light_by_index(scene,
                                                                                             n_light);
                bool                      current_light_is_caster = false;
                scene_light_type          current_light_type      = SCENE_LIGHT_TYPE_UNKNOWN;
                system_hashed_ansi_string current_light_type_has  = nullptr;

                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_TYPE,
                                        &current_light_type);
                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                        &current_light_is_caster);

                current_light_type_has = scene_light_get_scene_light_type_has(current_light_type);

                name_sstream << "Light ["
                             << n_light
                             << "] (type:["
                             << system_hashed_ansi_string_get_buffer(current_light_type_has)
                             << "]):\n";

                /* If this light is a caster, include SM-specific info */
                if (current_light_is_caster)
                {
                    scene_light_shadow_map_algorithm shadow_map_algorithm     = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
                    system_hashed_ansi_string        shadow_map_algorithm_has = nullptr;
                    scene_light_shadow_map_bias      shadow_map_bias          = SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN;
                    system_hashed_ansi_string        shadow_map_bias_has      = nullptr;
                    scene_light_shadow_map_filtering shadow_map_filtering     = SCENE_LIGHT_SHADOW_MAP_FILTERING_UNKNOWN;
                    system_hashed_ansi_string        shadow_map_filtering_has = nullptr;

                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                                            &shadow_map_algorithm);
                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS,
                                            &shadow_map_bias);
                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING,
                                            &shadow_map_filtering);

                    shadow_map_algorithm_has = scene_light_get_scene_light_shadow_map_algorithm_has(shadow_map_algorithm);
                    shadow_map_bias_has      = scene_light_get_scene_light_shadow_map_bias_has     (shadow_map_bias);
                    shadow_map_filtering_has = scene_light_get_scene_light_shadow_map_filtering_has(shadow_map_filtering);

                    name_sstream << "Shadow caster: "
                                    "algorithm:["
                                 << system_hashed_ansi_string_get_buffer(shadow_map_algorithm_has)
                                 << "] bias:["
                                 << system_hashed_ansi_string_get_buffer(shadow_map_bias_has)
                                 << "] filtering:["
                                 << system_hashed_ansi_string_get_buffer(shadow_map_filtering_has)
                                 << "]\n";
                }

                /* If this is a point or a spot light, include falloff setting */
                if (current_light_type == SCENE_LIGHT_TYPE_POINT ||
                    current_light_type == SCENE_LIGHT_TYPE_SPOT)
                {
                    scene_light_falloff       current_light_falloff     = SCENE_LIGHT_FALLOFF_UNKNOWN;
                    system_hashed_ansi_string current_light_falloff_has = nullptr;

                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_FALLOFF,
                                            &current_light_falloff);

                    current_light_falloff_has = scene_light_get_scene_light_falloff_has(current_light_falloff);

                    name_sstream << "Light falloff:["
                                 << system_hashed_ansi_string_get_buffer(current_light_falloff_has)
                                 << "]\n";
                }

                /* If this is a point light, also query the SM algorithm */
                if (current_light_type == SCENE_LIGHT_TYPE_POINT)
                {
                    scene_light_shadow_map_pointlight_algorithm sm_algorithm;
                    system_hashed_ansi_string                   sm_algorithm_has;

                    scene_light_get_property(current_light,
                                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                            &sm_algorithm);

                    sm_algorithm_has = scene_light_get_scene_light_shadow_map_pointlight_algorithm_has(sm_algorithm);

                    name_sstream << "SM pointlight algorithm:["
                                 << system_hashed_ansi_string_get_buffer(sm_algorithm_has)
                                 << "]\n";
                }

                name_sstream << "\n";
            }
        }

        /* SM global setting */
        name_sstream << "<<\n"
                     << "\n"
                     << "Global SM setting:["
                     << ((use_shadow_maps) ? "ON" : "OFF")
                     << "]\n";
    }

    name_string = name_sstream.str();

    return system_hashed_ansi_string_create(name_string.c_str() );
}

/** TODO */
PRIVATE void _scene_renderer_materials_init_special_materials(_scene_renderer_materials* materials_ptr,
                                                              ral_context                context)
{
    const mesh_material_shading shading_type_attribute_data = MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE;
    const mesh_material_shading shading_type_none           = MESH_MATERIAL_SHADING_NONE;

    /* Configure materials using predefined shader bodies.
     *
     * NOTE: These bodies need to adhere to the requirements inposed
     *       by how ogl_uber works.
     */
    mesh_material depth_clip                                        = nullptr;
    mesh_material depth_clip_and_depth_clip_squared                 = nullptr;
    mesh_material depth_clip_and_depth_clip_squared_dual_paraboloid = nullptr;
    mesh_material depth_clip_dual_paraboloid                        = nullptr;
    mesh_material normal                                            = nullptr;
    mesh_material texcoord                                          = nullptr;

    depth_clip = mesh_material_create_from_shader_bodies(system_hashed_ansi_string_create("Special material: depth clip space"),
                                                         context,
                                                         nullptr, /* object_manager_path */
                                                         scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_FS),
                                                         nullptr, /* gs_body */
                                                         nullptr, /* tc_body */
                                                         nullptr, /* te_body */
                                                         scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_VS) );

    depth_clip_and_depth_clip_squared_dual_paraboloid = mesh_material_create_from_shader_bodies(system_hashed_ansi_string_create("Special material: depth clip space and depth squared clip space dual paraboloid"),
                                                                                                context,
                                                                                                nullptr, /* object_manager_path */
                                                                                                scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_FS),
                                                                                                nullptr, /* gs_body */
                                                                                                nullptr, /* tc_body */
                                                                                                nullptr, /* te_body */
                                                                                                scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_VS) );

    depth_clip_dual_paraboloid = mesh_material_create_from_shader_bodies(system_hashed_ansi_string_create("Special material: depth clip space dual paraboloid"),
                                                                         context,
                                                                         nullptr, /* object_manager_path */
                                                                         scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_FS),
                                                                         nullptr, /* gs_body */
                                                                         nullptr, /* tc_body */
                                                                         nullptr, /* te_body */
                                                                         scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_VS) );

    depth_clip_and_depth_clip_squared = mesh_material_create_from_shader_bodies(system_hashed_ansi_string_create("Special material: depth clip and squared depth clip"),
                                                                                context,
                                                                                nullptr, /* object_manager_path */
                                                                                scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_FS),
                                                                                nullptr, /* gs_body */
                                                                                nullptr, /* tc_body */
                                                                                nullptr, /* te_body */
                                                                                scene_renderer_sm_get_special_material_shader_body(SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_VS) );

    normal = mesh_material_create(system_hashed_ansi_string_create("Special material: normals"),
                                  context,
                                  nullptr); /* object_manager_path */

    texcoord = mesh_material_create(system_hashed_ansi_string_create("Special material: texcoord"),
                                    context,
                                    nullptr); /* object_manager_path */

    /* Configure "normal preview" material */
    mesh_material_set_property                                    (normal,
                                                                   MESH_MATERIAL_PROPERTY_SHADING,
                                                                  &shading_type_attribute_data);
    mesh_material_set_shading_property_to_input_fragment_attribute(normal,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL);

    /* Configure "texcoord preview" material */
    mesh_material_set_property                                    (texcoord,
                                                                   MESH_MATERIAL_PROPERTY_SHADING,
                                                                  &shading_type_attribute_data);
    mesh_material_set_shading_property_to_input_fragment_attribute(texcoord,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD);

    /* Store the materials */
    ASSERT_DEBUG_SYNC(!system_hash64map_contains(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP],                                        (system_hash64) context) &&
                      !system_hash64map_contains(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP_DUAL_PARABOLOID],                        (system_hash64) context) &&
                      !system_hash64map_contains(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED],                 (system_hash64) context) &&
                      !system_hash64map_contains(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED_DUAL_PARABOLOID], (system_hash64) context) &&
                      !system_hash64map_contains(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_NORMALS],                                           (system_hash64) context) &&
                      !system_hash64map_contains(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_TEXCOORD],                                          (system_hash64) context),
                      "Special materials are already stored for the specified RAL context");

    system_hash64map_insert(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP],
                            (system_hash64) context,
                            depth_clip,
                            nullptr, /* callback */
                            nullptr  /* callback_argument*/);
    system_hash64map_insert(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP_DUAL_PARABOLOID],
                            (system_hash64) context,
                            depth_clip_dual_paraboloid,
                            nullptr, /* callback */
                            nullptr  /* callback_argument*/);
    system_hash64map_insert(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED],
                            (system_hash64) context,
                            depth_clip_and_depth_clip_squared,
                            nullptr, /* callback */
                            nullptr  /* callback_argument*/);
    system_hash64map_insert(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED_DUAL_PARABOLOID],
                            (system_hash64) context,
                            depth_clip_and_depth_clip_squared_dual_paraboloid,
                            nullptr, /* callback */
                            nullptr  /* callback_argument*/);
    system_hash64map_insert(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_NORMALS],
                            (system_hash64) context,
                            normal,
                            nullptr, /* callback */
                            nullptr  /* callback_argument*/);
    system_hash64map_insert(materials_ptr->per_context_special_materials[SPECIAL_MATERIAL_TEXCOORD],
                            (system_hash64) context,
                            texcoord,
                            nullptr, /* callback */
                            nullptr  /* callback_argument*/);
}

/** TODO */
PRIVATE void _scene_renderer_materials_on_scene_about_to_be_deleted(const void* callback_data,
                                                                    void*       user_arg)
{
    uint32_t                   scene_index   = UINT32_MAX;
    _scene_renderer_materials* materials_ptr = reinterpret_cast<_scene_renderer_materials*>(user_arg);
    uint32_t                   n_ubers       = 0;

    system_resizable_vector_get_property(materials_ptr->ubers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_ubers);

    scene_index = system_resizable_vector_find(materials_ptr->callback_enabled_scenes,
                                               callback_data);

    if (scene_index == ITEM_NOT_FOUND)
    {
        goto end;
    }

    for (uint32_t n_uber = 0;
                  n_uber < n_ubers;
                ++n_uber)
    {
        uint32_t                        n_items  = 0;
        _scene_renderer_materials_uber* uber_ptr = nullptr;

        system_resizable_vector_get_element_at(materials_ptr->ubers,
                                               n_uber,
                                              &uber_ptr);
        system_hash64map_get_property         (uber_ptr->per_context_data,
                                               SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                              &n_items);

        for (int32_t n_item = 0;
                     n_item < static_cast<int32_t>(n_items);
                   ++n_item)
        {
            system_hash64                                item_hash;
            _scene_renderer_materials_uber_context_data* item_ptr = nullptr;

            system_hash64map_get_element_at(uber_ptr->per_context_data,
                                            n_item,
                                           &item_ptr,
                                           &item_hash);

            if (item_ptr->owner_scene == user_arg)
            {
                delete item_ptr;

                system_hash64map_remove(uber_ptr->per_context_data,
                                        item_hash);

                --n_item;
            }
        }
    }

    _scene_renderer_materials_register_for_callbacks(materials_ptr,
                                                     (scene) callback_data,
                                                     false); /* should_register */

    system_resizable_vector_delete_element_at(materials_ptr->callback_enabled_scenes,
                                              scene_index);

end:
    ;
}

/** TODO */
PRIVATE void _scene_renderer_materials_register_for_callbacks(_scene_renderer_materials* materials_ptr,
                                                              scene                      scene_to_use,
                                                              bool                       should_register)
{
    system_callback_manager scene_callback_manager = nullptr;

    scene_get_property(scene_to_use,
                       SCENE_PROPERTY_CALLBACK_MANAGER,
                      &scene_callback_manager);

    if (should_register)
    {
        system_resizable_vector_push(materials_ptr->callback_enabled_scenes,
                                     scene_to_use);

        system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                        SCENE_CALLBACK_ID_ABOUT_TO_BE_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _scene_renderer_materials_on_scene_about_to_be_deleted,
                                                        materials_ptr);
    }
    else
    {
        /* NOTE: This call is very likely going to have to be handled asynchronously,
         *       as this entrypoint can only be called by scene callback managers.
         *       If we had tried to stick to in-thread execution, we would have locked
         *       up.
         */
        system_callback_manager_unsubscribe_from_callbacks(scene_callback_manager,
                                                           SCENE_CALLBACK_ID_ABOUT_TO_BE_DELETED,
                                                           _scene_renderer_materials_on_scene_about_to_be_deleted,
                                                           materials_ptr,
                                                           true); /* allow_deferred_execution */
    }
}

/** TODO */
PRIVATE void _scene_renderer_materials_window_about_to_be_destroyed_callback(const void* callback_data,
                                                                             void*       user_arg)
{
    _scene_renderer_materials* materials_ptr = reinterpret_cast<_scene_renderer_materials*>(user_arg);
    demo_window                window        = (demo_window)                               (callback_data);

    /* Iterate over all ubers and release those which have been created for the context attached to
     * the window, which is about to be destroyed. */
    ral_context context = nullptr;
    uint32_t    n_ubers = 0;

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &context);

    system_resizable_vector_get_property(materials_ptr->ubers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_ubers);

    for (uint32_t n_uber = 0;
                  n_uber < n_ubers;
                ++n_uber)
    {
        _scene_renderer_materials_uber*              uber_ptr                  = nullptr;
        _scene_renderer_materials_uber_context_data* uber_per_context_data_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(materials_ptr->ubers,
                                                    n_uber,
                                                   &uber_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve uber descriptor at index [%d]",
                              n_uber);

            continue;
        }

        if (!system_hash64map_get(uber_ptr->per_context_data,
                                  (system_hash64) context,
                                 &uber_per_context_data_ptr) )
        {
            continue;
        }

        delete uber_per_context_data_ptr;

        system_hash64map_remove(uber_ptr->per_context_data,
                                (system_hash64) context);
    }

    /* Don't forget about special materials */
    for (uint32_t n_special_material = 0;
                  n_special_material < SPECIAL_MATERIAL_COUNT;
                ++n_special_material)
    {
        mesh_material material;

        if (!system_hash64map_get(materials_ptr->per_context_special_materials[n_special_material],
                                 (system_hash64) context,
                                &material) )
        {
            continue;
        }

        mesh_material_release(material);

        system_hash64map_remove(materials_ptr->per_context_special_materials[n_special_material],
                                (system_hash64) context);
    }
}

/** TODO */
PRIVATE void _scene_renderer_materials_window_created_callback(const void* callback_data,
                                                               void*       user_arg)
{
    ral_context                context       = nullptr;
    _scene_renderer_materials* materials_ptr = reinterpret_cast<_scene_renderer_materials*>(user_arg);
    demo_window                window        = (demo_window)                               (callback_data);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &context);

    _scene_renderer_materials_init_special_materials(materials_ptr,
                                                     context);
}


/** Please see header for specification */
PUBLIC scene_renderer_materials scene_renderer_materials_create()
{
    _scene_renderer_materials* materials_ptr = new (std::nothrow) _scene_renderer_materials;

    ASSERT_ALWAYS_SYNC(materials_ptr != nullptr,
                       "Out of memory");

    if (materials_ptr != nullptr)
    {
        /* Sign up for window created/destroyed notifications */
        system_callback_manager app_callback_manager = nullptr;

        demo_app_get_property(DEMO_APP_PROPERTY_CALLBACK_MANAGER,
                             &app_callback_manager);

        system_callback_manager_subscribe_for_callbacks(app_callback_manager,
                                                        DEMO_APP_CALLBACK_ID_WINDOW_ABOUT_TO_BE_DESTROYED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _scene_renderer_materials_window_about_to_be_destroyed_callback,
                                                        materials_ptr);
        system_callback_manager_subscribe_for_callbacks(app_callback_manager,
                                                        DEMO_APP_CALLBACK_ID_WINDOW_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _scene_renderer_materials_window_created_callback,
                                                        materials_ptr);
    }

    return (scene_renderer_materials) materials_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_renderer_materials_force_mesh_material_shading_property_attachment(scene_renderer_materials          materials,
                                                                                                 mesh_material_shading_property    property,
                                                                                                 mesh_material_property_attachment attachment,
                                                                                                 void*                             attachment_data)
{
    _scene_renderer_materials*                       materials_ptr   = reinterpret_cast<_scene_renderer_materials*>(materials);
    _scene_renderer_materials_mesh_material_setting* new_setting_ptr = new _scene_renderer_materials_mesh_material_setting;

    ASSERT_ALWAYS_SYNC(new_setting_ptr != nullptr,
                       "Out of memory");

    if (new_setting_ptr != nullptr)
    {
        /* TODO: This really could check if the property is not already overloaded with another attachment */
        new_setting_ptr->attachment      = attachment;
        new_setting_ptr->attachment_data = attachment_data;
        new_setting_ptr->property        = property;

        system_resizable_vector_push(materials_ptr->forced_mesh_material_settings,
                                     new_setting_ptr);
    }
}

/** Please see header for specification */
PUBLIC mesh_material scene_renderer_materials_get_special_material(scene_renderer_materials                  materials,
                                                                   ral_context                               context,
                                                                   scene_renderer_materials_special_material special_material)
{
    _scene_renderer_materials* materials_ptr = reinterpret_cast<_scene_renderer_materials*>(materials);
    mesh_material              result        = nullptr;

    system_hash64map_get(materials_ptr->per_context_special_materials[special_material],
                         (system_hash64) context,
                        &result);

    ASSERT_DEBUG_SYNC(result != nullptr,
                      "Could not retrieve a special material for the specified RAL context");

    return result;
}

/** Please see header for specification */
PUBLIC scene_renderer_uber scene_renderer_materials_get_uber(scene_renderer_materials materials,
                                                             mesh_material            material,
                                                             scene                    scene,
                                                             bool                     use_shadow_maps)
{
    ral_context                     context                   = nullptr;
    _scene_renderer_materials*      materials_ptr             = reinterpret_cast<_scene_renderer_materials*>(materials);
    _scene_renderer_materials_uber* parent_materials_uber_ptr = nullptr;
    uint32_t                        scene_index;
    scene_renderer_uber             result                    = nullptr;

    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_CONTEXT,
                              &context);

    if ( (scene_index = system_resizable_vector_find(materials_ptr->callback_enabled_scenes,
                                                     scene) ) == ITEM_NOT_FOUND)
    {
        _scene_renderer_materials_register_for_callbacks(materials_ptr,
                                                         scene,
                                                         true); /* should_register */
    }
    else
    {
        /* First, iterate over existing uber containers and check if there's a match */
        unsigned int n_materials = 0;

        system_resizable_vector_get_property(materials_ptr->ubers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_materials);

        for (unsigned int n_material = 0;
                          n_material < n_materials;
                        ++n_material)
        {
            _scene_renderer_materials_uber_context_data* uber_context_data_ptr = nullptr;
            _scene_renderer_materials_uber*              uber_ptr              = nullptr;

            if (system_resizable_vector_get_element_at(materials_ptr->ubers,
                                                       n_material,
                                                      &uber_ptr) )
            {
                bool do_contexts_match         = true;
                bool do_materials_match        = false;
                bool does_material_match_scene = false;
                bool does_scene_matter         = true;
                bool does_sm_setting_match     = (uber_ptr->use_shadow_maps == use_shadow_maps);

                if (!system_hash64map_get(uber_ptr->per_context_data,
                                          (system_hash64) context,
                                         &uber_context_data_ptr) )
                {
                    do_contexts_match = false;

                    system_hash64map_get_element_at(uber_ptr->per_context_data,
                                                    0,        /* n_element */
                                                    &uber_context_data_ptr,
                                                    nullptr); /* result_hash_ptr */
                    continue;
                }

                do_materials_match = mesh_material_is_a_match_to_mesh_material(uber_context_data_ptr->material,
                                                                               material);

                if (do_materials_match)
                {
                    does_material_match_scene = (scene == nullptr                                                                                ||
                                                 scene != nullptr && _scene_renderer_materials_does_uber_match_scene(uber_context_data_ptr->uber,
                                                                                                                  scene,
                                                                                                                  use_shadow_maps) );

                    /* Do not take scene input into account if IFA shading is used */
                    mesh_material_shading material_shading = MESH_MATERIAL_SHADING_NONE;

                    mesh_material_get_property(uber_context_data_ptr->material,
                                               MESH_MATERIAL_PROPERTY_SHADING,
                                              &material_shading);

                    does_scene_matter = (material_shading != MESH_MATERIAL_SHADING_NONE);
                }

                /* Determine the outcome */
                if (  do_materials_match                              &&
                    (!does_scene_matter                               ||
                      does_scene_matter && does_material_match_scene) &&
                      does_sm_setting_match)
                {
                    if (do_contexts_match)
                    {
                        result = uber_context_data_ptr->uber;
                    }
                    else
                    {
                        parent_materials_uber_ptr = nullptr;
                    }

                    break;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not get uber descriptor at index [%d]",
                                  n_material);
            }
        }
    }

    if (result == nullptr)
    {
        scene_renderer_uber new_uber = nullptr;

        /* Nope? Gotta bake a new uber then */
        if (parent_materials_uber_ptr == nullptr)
        {
            parent_materials_uber_ptr = new _scene_renderer_materials_uber;

            ASSERT_ALWAYS_SYNC(parent_materials_uber_ptr != nullptr,
                               "Out of memory");

            parent_materials_uber_ptr->use_shadow_maps = use_shadow_maps;

            system_resizable_vector_push(materials_ptr->ubers,
                                         parent_materials_uber_ptr);
        }

        new_uber = _scene_renderer_materials_bake_uber(materials,
                                                       material,
                                                       scene,
                                                       use_shadow_maps);

        ASSERT_DEBUG_SYNC(new_uber != nullptr,
                          "Could not bake a new uber");

        if (new_uber != nullptr)
        {
            _scene_renderer_materials_uber_context_data* new_uber_context_data_ptr = nullptr;

            new_uber_context_data_ptr = new (std::nothrow) _scene_renderer_materials_uber_context_data;

            ASSERT_ALWAYS_SYNC(new_uber_context_data_ptr != nullptr,
                               "Out of memory");

            if (new_uber_context_data_ptr != nullptr)
            {
                const char*               name_suffix   = (use_shadow_maps) ? " copy with SM"
                                                                            : " copy without SM";
                const char*               uber_name     = nullptr;
                system_hashed_ansi_string uber_name_has = nullptr;

                scene_renderer_uber_get_shader_general_property(new_uber,
                                                                SCENE_RENDERER_UBER_GENERAL_PROPERTY_NAME,
                                                               &uber_name_has);

                uber_name                                  = system_hashed_ansi_string_get_buffer(uber_name_has);
                new_uber_context_data_ptr->material        = mesh_material_create_copy           (system_hashed_ansi_string_create_by_merging_two_strings(uber_name,
                                                                                                                                                          name_suffix),
                                                                                                  material);
                new_uber_context_data_ptr->owner_scene     = scene;
                new_uber_context_data_ptr->uber            = new_uber;
                result                                     = new_uber;

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(parent_materials_uber_ptr->per_context_data,
                                                             (system_hash64) context),
                                  "Uber data already registered for the specified RAL context");

                system_hash64map_insert(parent_materials_uber_ptr->per_context_data,
                                        (system_hash64) context,
                                        new_uber_context_data_ptr,
                                        nullptr,  /* callback          */
                                        nullptr); /* callback_argument */
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC void scene_renderer_materials_release(scene_renderer_materials materials)
{
    if (materials != nullptr)
    {
        /* Release the container */
        delete reinterpret_cast<_scene_renderer_materials*>(materials);

        materials = nullptr;
    }
}
