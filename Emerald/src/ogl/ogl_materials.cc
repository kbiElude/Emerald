/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
#include "scene/scene_light.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/* Private type definitions */
typedef struct _ogl_materials_mesh_material_setting
{
    mesh_material_property_attachment attachment;
    void*                             attachment_data;
    mesh_material_shading_property    property;

    _ogl_materials_mesh_material_setting()
    {
        attachment      = MESH_MATERIAL_PROPERTY_ATTACHMENT_UNKNOWN;
        attachment_data = NULL;
        property        = MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN;
    }

    ~_ogl_materials_mesh_material_setting()
    {
        if (attachment_data != NULL)
        {
            switch (attachment)
            {
                case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
                {
                    delete (float*) attachment_data;

                    attachment_data = NULL;
                    break;
                }

                case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                {
                    ogl_texture_release( (ogl_texture&) attachment_data);

                    attachment_data = NULL;
                    break;
                }

                case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                {
                    delete (float*) attachment_data;

                    attachment_data = NULL;
                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false, "Unrecognized mesh material property attachment type");
                }
            } /* switch (attachment) */
        } /* if (attachment_data != NULL) */
    }
} _ogl_materials_mesh_material_setting;

typedef struct _ogl_materials_uber
{
    mesh_material material;
    ogl_uber      uber;
    bool          use_shadow_maps;

    _ogl_materials_uber()
    {
        material        = NULL;
        uber            = NULL;
        use_shadow_maps = false;
    }

    ~_ogl_materials_uber()
    {
        if (material != NULL)
        {
            mesh_material_release(material);

            material = NULL;
        }

        if (uber != NULL)
        {
            ogl_uber_release(uber);

            uber = NULL;
        }
    }
} _ogl_materials_uber;

typedef struct _ogl_materials
{
    ogl_context             context;
    system_resizable_vector forced_mesh_material_settings;
    mesh_material           special_materials[SPECIAL_MATERIAL_COUNT];
    system_resizable_vector ubers; /* stores _ogl_materials_uber* */

    _ogl_materials()
    {
        context                       = NULL;
        forced_mesh_material_settings = system_resizable_vector_create(4 /* capacity */,
                                                                       sizeof(_ogl_materials_mesh_material_setting*) );
        ubers                         = system_resizable_vector_create(4 /* capacity */,
                                                                       sizeof(_ogl_materials_uber*) );

        memset(special_materials,
               0,
               sizeof(special_materials) );
    }

    ~_ogl_materials()
    {
        if (forced_mesh_material_settings != NULL)
        {
            _ogl_materials_mesh_material_setting* mesh_material_setting_ptr = NULL;

            while (system_resizable_vector_pop(forced_mesh_material_settings,
                                              &mesh_material_setting_ptr) )
            {
                delete mesh_material_setting_ptr;

                mesh_material_setting_ptr = NULL;
            }
        }

        if (special_materials != NULL)
        {
            for (_ogl_materials_special_material special_material  = SPECIAL_MATERIAL_FIRST;
                                                 special_material != SPECIAL_MATERIAL_COUNT;
                                          ((int&)special_material)++)
            {
                if (special_materials[special_material] != NULL)
                {
                    mesh_material_release(special_materials[special_material]);

                    special_materials[special_material] = NULL;
                }
            }
        }

        if (ubers != NULL)
        {
            _ogl_materials_uber* uber_ptr = NULL;

            while (system_resizable_vector_pop(ubers,
                                              &uber_ptr) )
            {
                if (uber_ptr->material != NULL)
                {
                    mesh_material_release(uber_ptr->material);

                    uber_ptr->material = NULL;
                }

                if (uber_ptr->uber != NULL)
                {
                    ogl_uber_release(uber_ptr->uber);

                    uber_ptr = NULL;
                }
            }
            system_resizable_vector_release(ubers);

            ubers = NULL;
        }
    }
} _ogl_materials;


/* Forward declarations */
PRIVATE ogl_uber _ogl_materials_bake_uber             (__in      __notnull ogl_materials                      materials,
                                                       __in      __notnull mesh_material                      material,
                                                       __in      __notnull scene                              scene);
PRIVATE void     _ogl_materials_get_forced_setting    (__in      __notnull ogl_materials                      materials,
                                                       __in                mesh_material_shading_property,
                                                       __out_opt __notnull mesh_material_property_attachment* out_attachment,
                                                       __out_opt __notnull void**                             out_attachment_data);
PRIVATE void     _ogl_materials_init_special_materials(__in      __notnull _ogl_materials*                    materials_ptr);

/** TODO */
PRIVATE ogl_uber _ogl_materials_bake_uber(__in __notnull ogl_materials materials,
                                          __in __notnull mesh_material material,
                                          __in __notnull scene         scene,
                                          __in           bool          use_shadow_maps)
{
    _ogl_materials* materials_ptr = (_ogl_materials*) materials;
    ogl_context     context       = materials_ptr->context;

    LOG_INFO("Performance warning: _ogl_materials_bake_uber() called.");

    /* Spawn a new uber  */
    system_hashed_ansi_string material_name    = mesh_material_get_name(material);
    const char*               uber_name_suffix = use_shadow_maps ? " uber with SM" : " uber without SM";
    mesh_material_vs_behavior vs_behavior      = MESH_MATERIAL_VS_BEHAVIOR_DEFAULT;

    mesh_material_get_property(material,
                               MESH_MATERIAL_PROPERTY_VS_BEHAVIOR,
                              &vs_behavior);

    ogl_uber                  new_uber         = ogl_uber_create       (context,
                                                                        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(material_name),
                                                                                                                                uber_name_suffix),
                                                                        shaders_vertex_uber_get_vs_uber_type_for_vs_behavior   (vs_behavior) );

    ASSERT_ALWAYS_SYNC(new_uber != NULL,
                       "Could not spawn an uber instance");
    if (new_uber != NULL)
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

            unsigned int n_uber_fragment_property_value_pairs     = 0;
            unsigned int uber_fragment_property_value_pairs[(n_mappings + 1 /* view vector setting */) * 2];

            /* Add ambient/diffuse/emission factors */
            for (unsigned int n_mapping = 0;
                              n_mapping < n_mappings;
                            ++n_mapping)
            {
                _mapping                          mapping             = mappings[n_mapping];
                mesh_material_property_attachment material_attachment = mesh_material_get_shading_property_attachment_type(material,
                                                                                                                           mapping.material_property);

                _ogl_materials_get_forced_setting(materials,
                                                  mapping.material_property,
                                                 &material_attachment,
                                                 NULL /* out_attachment_data */);

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
                    } /* switch (material_attachment) */

                    n_uber_fragment_property_value_pairs++;
                }
            } /* for (all mappings) */

            /* Add lights */
            if (scene != NULL)
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
                            } /* switch (current_light_type) */

                            break;
                        } /* case MESH_MATERIAL_SHADING_LAMBERT: */

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
                            } /* switch (current_light_type) */

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "TODO: Unsupported shading algorithm");
                        }
                    } /* switch (material_shading) */

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

                    /* Add the light item if not a NULL light */
                    if (uber_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE)
                    {
                        ogl_uber_add_light_item(new_uber,
                                                current_light,
                                                uber_light_type,
                                                uses_shadow_mapping,
                                                n_uber_fragment_property_value_pairs,
                                                uber_fragment_property_value_pairs);
                    } /* if (uber_light_type != UBER_LIGHT_NONE) */
                } /* for (all scene lights) */
            } /* if (scene != NULL) */
        } /* if (material_shading == MESH_MATERIAL_SHADING_LAMBERT || material_shading == MESH_MATERIAL_SHADING_PHONG) */
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
            _ogl_uber_input_fragment_attribute uber_input_attribute = OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN;

            switch (input_fragment_attribute)
            {
                case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL:
                {
                    uber_input_attribute = OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL;

                    break;
                }

                case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD:
                {
                    uber_input_attribute = OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false, "Unrecognized input fragment attribute");
                }
            }

            if (uber_input_attribute != OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN)
            {
                ogl_uber_add_input_fragment_attribute_item(new_uber,
                                                           uber_input_attribute);
            }
        }

        /* Done! */
    }

    /* Return the result */
    return (ogl_uber) new_uber;
}

/** TODO.
 **/
PRIVATE bool _ogl_materials_does_uber_match_scene(__in __notnull ogl_uber uber,
                                                  __in __notnull scene    scene)
{
    bool result = true;

    /* The purpose of this function is to make sure that uber we're fed implements shading
     * for exactly the same light configuration as defined in scene.
     */
    scene_light  current_light                = NULL;
    unsigned int n_scene_lights               = 0;
    unsigned int n_uber_lights                = 0;
    bool         scene_shadow_mapping_enabled = false;

    scene_get_property                  (scene,
                                         SCENE_PROPERTY_N_LIGHTS,
                                        &n_scene_lights);
    scene_get_property                  (scene,
                                         SCENE_PROPERTY_SHADOW_MAPPING_ENABLED,
                                        &scene_shadow_mapping_enabled);
    ogl_uber_get_shader_general_property(uber,
                                         OGL_UBER_GENERAL_PROPERTY_N_ITEMS,
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
        _ogl_uber_item_type              current_uber_item_type             = OGL_UBER_ITEM_UNKNOWN;

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

        ogl_uber_get_shader_item_property(uber,
                                          n_light,
                                          OGL_UBER_ITEM_PROPERTY_TYPE,
                                         &current_uber_item_type);

        if (current_uber_item_type != OGL_UBER_ITEM_LIGHT)
        {
            /* Nope */
            result = false;

            goto end;
        }

        /* Check if the light should be considered a shadow caster. Note that this setting
         * is overriden by a scene property called SCENE_PROPERTY_SHADOW_MAPPING_ENABLED
         * which is toggled on and off at different stages of the rendering pipeline execution.
         */
        ogl_uber_get_shader_item_property(uber,
                                          n_light,
                                          OGL_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP,
                                         &current_uber_item_is_shadow_caster);

        current_light_is_shadow_caster     &= scene_shadow_mapping_enabled;
        current_uber_item_is_shadow_caster &= scene_shadow_mapping_enabled;

        /* Carry on with other light stuff */
        if (current_light_type == SCENE_LIGHT_TYPE_POINT ||
            current_light_type == SCENE_LIGHT_TYPE_SPOT)
        {
            scene_light_get_property         (current_light,
                                              SCENE_LIGHT_PROPERTY_FALLOFF,
                                             &current_light_falloff);

            ogl_uber_get_shader_item_property(uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF,
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

            scene_light_get_property         (current_light,
                                              SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                             &current_light_shadow_map_pointlight_algorithm);
            ogl_uber_get_shader_item_property(uber,
                                              n_light,
                                              OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                             &current_uber_item_shadow_map_pointlight_algorithm);

            if (current_light_shadow_map_pointlight_algorithm != current_uber_item_shadow_map_pointlight_algorithm)
            {
                /* Nope */
                result = false;

                goto end;
            }
        }

        ogl_uber_get_shader_item_property(uber,
                                          n_light,
                                          OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS,
                                         &current_uber_item_shadow_map_bias);
        ogl_uber_get_shader_item_property(uber,
                                          n_light,
                                          OGL_UBER_ITEM_PROPERTY_LIGHT_TYPE,
                                         &current_uber_item_light_type);

        /* TODO: Expand to support other light types */
        ASSERT_DEBUG_SYNC(current_light_type == SCENE_LIGHT_TYPE_AMBIENT     ||
                          current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
                          current_light_type == SCENE_LIGHT_TYPE_POINT,
                          "TODO: Unsupported light type, expand.");

        if (current_light_type == SCENE_LIGHT_TYPE_AMBIENT     &&  current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT             ||
            current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL && (current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL &&
                                                                   current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL)  ||
            current_light_type == SCENE_LIGHT_TYPE_POINT       && (current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT       &&
                                                                   current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT         &&
                                                                   current_uber_item_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT) )
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
    } /* for (all lights attached to the scene) */

    /* Hey, that's a match! */
end:
    /* Done */
    return result;
}

/** TODO */
PRIVATE void _ogl_materials_get_forced_setting(__in      __notnull ogl_materials                      materials,
                                               __in                mesh_material_shading_property     shading_property,
                                               __out_opt __notnull mesh_material_property_attachment* out_attachment,
                                               __out_opt __notnull void**                             out_attachment_data)
{
    const _ogl_materials* materials_ptr     = (const _ogl_materials*) materials;
    const unsigned int    n_forced_settings = system_resizable_vector_get_amount_of_elements(materials_ptr->forced_mesh_material_settings);

    for (unsigned int n_setting = 0;
                      n_setting < n_forced_settings;
                    ++n_setting)
    {
        _ogl_materials_mesh_material_setting* setting_ptr = NULL;

        if (system_resizable_vector_get_element_at(materials_ptr->forced_mesh_material_settings,
                                                   n_setting,
                                                  &setting_ptr) )
        {
            if (setting_ptr->property == shading_property)
            {
                if (out_attachment != NULL)
                {
                    *out_attachment = setting_ptr->attachment;
                }

                if (out_attachment_data != NULL)
                {
                    *out_attachment_data = setting_ptr->attachment_data;
                }

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieved forced mesh material setting descriptor");
        }
    }
}

/** TODO */
PRIVATE void _ogl_materials_init_special_materials(__in __notnull _ogl_materials* materials_ptr)
{
    const mesh_material_shading     shading_type_attribute_data                 = MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE;
    const mesh_material_shading     shading_type_none                           = MESH_MATERIAL_SHADING_NONE;
    const mesh_material_vs_behavior vs_behavior_dpsm                            = MESH_MATERIAL_VS_BEHAVIOR_DUAL_PARABOLOID_SM;
          mesh_material             special_material_depth_clip                 = mesh_material_create(system_hashed_ansi_string_create("Special material: depth clip space"),
                                                                                                       materials_ptr->context);
          mesh_material             special_material_depth_dual_paraboloid_clip = mesh_material_create(system_hashed_ansi_string_create("Special material: depth dual paraboloid clip"),
                                                                                                       materials_ptr->context);
          mesh_material             special_material_normal                     = mesh_material_create(system_hashed_ansi_string_create("Special material: normals"),
                                                                                                       materials_ptr->context);
          mesh_material             special_material_texcoord                   = mesh_material_create(system_hashed_ansi_string_create("Special material: texcoord"),
                                                                                                       materials_ptr->context);

    mesh_material_set_property(special_material_depth_clip,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type_none);

    mesh_material_set_property(special_material_depth_dual_paraboloid_clip,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type_none);
    mesh_material_set_property(special_material_depth_dual_paraboloid_clip,
                               MESH_MATERIAL_PROPERTY_VS_BEHAVIOR,
                              &vs_behavior_dpsm);

    mesh_material_set_property                                    (special_material_normal,
                                                                   MESH_MATERIAL_PROPERTY_SHADING,
                                                                  &shading_type_attribute_data);
    mesh_material_set_shading_property_to_input_fragment_attribute(special_material_normal,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL);

    mesh_material_set_property                                    (special_material_texcoord,
                                                                   MESH_MATERIAL_PROPERTY_SHADING,
                                                                  &shading_type_attribute_data);
    mesh_material_set_shading_property_to_input_fragment_attribute(special_material_texcoord,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD);

    materials_ptr->special_materials[SPECIAL_MATERIAL_DEPTH_CLIP]            = special_material_depth_clip;
    materials_ptr->special_materials[SPECIAL_MATERIAL_DEPTH_DUAL_PARABOLOID] = special_material_depth_dual_paraboloid_clip;
    materials_ptr->special_materials[SPECIAL_MATERIAL_NORMALS]               = special_material_normal;
    materials_ptr->special_materials[SPECIAL_MATERIAL_TEXCOORD]              = special_material_texcoord;
}


/** Please see header for specification */
PUBLIC ogl_materials ogl_materials_create(__in __notnull ogl_context context)
{
    _ogl_materials* materials_ptr = new (std::nothrow) _ogl_materials;

    ASSERT_ALWAYS_SYNC(materials_ptr != NULL,
                       "Out of memory");

    if (materials_ptr != NULL)
    {
        materials_ptr->context = context;

        _ogl_materials_init_special_materials(materials_ptr);
    }

    return (ogl_materials) materials_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_materials_force_mesh_material_shading_property_attachment(__in __notnull ogl_materials                     materials,
                                                                                      __in           mesh_material_shading_property    property,
                                                                                      __in           mesh_material_property_attachment attachment,
                                                                                      __in __notnull void*                             attachment_data)
{
    _ogl_materials*                       materials_ptr   = (_ogl_materials*) materials;
    _ogl_materials_mesh_material_setting* new_setting_ptr = new _ogl_materials_mesh_material_setting;

    ASSERT_ALWAYS_SYNC(new_setting_ptr != NULL,
                       "Out of memory");

    if (new_setting_ptr != NULL)
    {
        /* TODO: This really could check if the property is not already overloaded with another attachment */
        new_setting_ptr->attachment      = attachment;
        new_setting_ptr->attachment_data = attachment_data;
        new_setting_ptr->property        = property;

        system_resizable_vector_push(materials_ptr->forced_mesh_material_settings,
                                     new_setting_ptr);
    } /* if (new_setting_ptr != NULL) */
}

/** Please see header for specification */
PUBLIC mesh_material ogl_materials_get_special_material(__in __notnull ogl_materials                   materials,
                                                        __in           _ogl_materials_special_material special_material)
{
    return ( (_ogl_materials*) materials)->special_materials[special_material];
}

/** Please see header for specification */
PUBLIC ogl_uber ogl_materials_get_uber(__in     __notnull ogl_materials materials,
                                       __in     __notnull mesh_material material,
                                       __in_opt           scene         scene,
                                       __in               bool          use_shadow_maps)
{
    _ogl_materials* materials_ptr = (_ogl_materials*) materials;
    ogl_uber        result        = NULL;

    /* First, iterate over existing uber containers and check if there's a match */
    const unsigned int n_materials = system_resizable_vector_get_amount_of_elements(materials_ptr->ubers);

    for (unsigned int n_material = 0;
                      n_material < n_materials;
                    ++n_material)
    {
        _ogl_materials_uber* uber_ptr = NULL;

        if (system_resizable_vector_get_element_at(materials_ptr->ubers,
                                                   n_material,
                                                  &uber_ptr) )
        {
            bool do_materials_match        = mesh_material_is_a_match_to_mesh_material             (uber_ptr->material,
                                                                                                    material);
            bool does_material_match_scene = (scene == NULL                                                          ||
                                              scene != NULL && _ogl_materials_does_uber_match_scene(uber_ptr->uber,
                                                                                                    scene) );
            bool does_sm_setting_match     = (uber_ptr->use_shadow_maps == use_shadow_maps);

            if ( do_materials_match       &&
                does_material_match_scene &&
                does_sm_setting_match)
            {
                result = uber_ptr->uber;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not get uber descriptor at index [%d]",
                              n_material);
        }
    } /* for (all materials) */

    if (result == NULL)
    {
        /* Nope? Gotta bake a new uber then */
        ogl_uber new_uber = _ogl_materials_bake_uber(materials,
                                                     material,
                                                     scene,
                                                     use_shadow_maps);

        ASSERT_DEBUG_SYNC(new_uber != NULL,
                          "Could not bake a new uber");

        if (new_uber != NULL)
        {
            _ogl_materials_uber* new_uber_descriptor = new (std::nothrow) _ogl_materials_uber;

            ASSERT_ALWAYS_SYNC(new_uber_descriptor != NULL,
                               "Out of memory");

            if (new_uber_descriptor != NULL)
            {
                const char* src_material_name = system_hashed_ansi_string_get_buffer(mesh_material_get_name(material) );
                const char* name_suffix       = (use_shadow_maps) ? " copy with SM" : " copy without SM";

                new_uber_descriptor->material        = mesh_material_create_copy(system_hashed_ansi_string_create_by_merging_two_strings(src_material_name,
                                                                                                                                         name_suffix),
                                                                                 material);
                new_uber_descriptor->uber            = new_uber;
                new_uber_descriptor->use_shadow_maps = use_shadow_maps;
                result                               = new_uber;

                system_resizable_vector_push(materials_ptr->ubers,
                                             new_uber_descriptor);
            }
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_materials_release(__in __notnull ogl_materials materials)
{
    if (materials != NULL)
    {
        /* Release the container */
        delete (_ogl_materials*) materials;

        materials = NULL;
    }
}
