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

    _ogl_materials_uber()
    {
        material = NULL;
        uber     = NULL;
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
        forced_mesh_material_settings = system_resizable_vector_create(4 /* capacity */, sizeof(_ogl_materials_mesh_material_setting*) );
        ubers                         = system_resizable_vector_create(4 /* capacity */, sizeof(_ogl_materials_uber*) );

        memset(special_materials, 0, sizeof(special_materials) );
    }

    ~_ogl_materials()
    {
        if (forced_mesh_material_settings != NULL)
        {
            _ogl_materials_mesh_material_setting* mesh_material_setting_ptr = NULL;

            while (system_resizable_vector_pop(forced_mesh_material_settings, &mesh_material_setting_ptr) )
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

            while (system_resizable_vector_pop(ubers, &uber_ptr) )
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
PRIVATE bool     _ogl_materials_are_materials_a_match (__in      __notnull mesh_material                      material_a,
                                                       __in      __notnull mesh_material                      material_b);
PRIVATE ogl_uber _ogl_materials_bake_uber             (__in      __notnull ogl_materials                      materials,
                                                       __in      __notnull mesh_material                      material,
                                                       __in      __notnull scene                              scene);
PRIVATE void     _ogl_materials_get_forced_setting    (__in      __notnull ogl_materials                      materials,
                                                       __in                mesh_material_shading_property,
                                                       __out_opt __notnull mesh_material_property_attachment* out_attachment,
                                                       __out_opt __notnull void**                             out_attachment_data);
PRIVATE void     _ogl_materials_init_special_materials(__in      __notnull _ogl_materials*                    materials_ptr);

/** TODO */
PRIVATE bool _ogl_materials_are_materials_a_match(__in __notnull mesh_material material_a,
                                                  __in __notnull mesh_material material_b)
{
    bool result = false;

    /* We only check the properties that affect how the shaders are built */

    /* 1. Shading */
    mesh_material_shading material_a_shading = MESH_MATERIAL_SHADING_UNKNOWN;
    mesh_material_shading material_b_shading = MESH_MATERIAL_SHADING_UNKNOWN;

    mesh_material_get_property(material_a,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &material_a_shading);
    mesh_material_get_property(material_b,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &material_b_shading);

    if (material_a_shading != material_b_shading)
    {
        goto end;
    }

    /* 2. Shading properties */
    for (unsigned int n_material_property = 0;
                      n_material_property < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                    ++n_material_property)
    {
        mesh_material_shading_property    property              = (mesh_material_shading_property) n_material_property;
        mesh_material_property_attachment material_a_attachment = mesh_material_get_shading_property_attachment_type(material_a, property);
        mesh_material_property_attachment material_b_attachment = mesh_material_get_shading_property_attachment_type(material_b, property);

        if (material_a_attachment != material_b_attachment)
        {
            goto end;
        }

        switch (material_a_attachment)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
            {
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
            {
                /* Single-component floating-point attachments are handled by uniforms so the actual data is irrelevant */
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                ogl_texture_dimensionality material_a_dimensionality;
                ogl_texture                material_a_texture = NULL;
                ogl_texture_dimensionality material_b_dimensionality;
                ogl_texture                material_b_texture = NULL;

                mesh_material_get_shading_property_value_texture(material_a,
                                                                 property,
                                                                &material_a_texture,
                                                                 NULL, /* out_mipmap_level - irrelevant */
                                                                 NULL);/* out_sampler - irrelevant */

                mesh_material_get_shading_property_value_texture(material_b,
                                                                 property,
                                                                &material_b_texture,
                                                                 NULL, /* out_mipmap_level - irrelevant */
                                                                 NULL);/* out_sampler - irrelevant */

                ogl_texture_get_mipmap_property(material_a_texture,
                                                0, /* mipmap_level */
                                                OGL_TEXTURE_MIPMAP_PROPERTY_DIMENSIONALITY,
                                               &material_a_dimensionality);
                ogl_texture_get_mipmap_property(material_b_texture,
                                                0, /* mipmap_level */
                                                OGL_TEXTURE_MIPMAP_PROPERTY_DIMENSIONALITY,
                                               &material_b_dimensionality);

                if (material_a_dimensionality != material_b_dimensionality)
                {
                    goto end;
                }

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
            {
                /* Vec4 attachments are handled by uniforms so the actual data is irrelevant */
                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false, "Unrecognized material property attachment [%d]", material_a_attachment);
            }
        } /* switch (material_a_attachment) */
    } /* for (all material properties) */

    /* Done */
    result = true;
end:
    return result;
}

/** TODO */
PRIVATE ogl_uber _ogl_materials_bake_uber(__in __notnull ogl_materials materials,
                                          __in __notnull mesh_material material,
                                          __in __notnull scene         scene)
{
    _ogl_materials* materials_ptr = (_ogl_materials*) materials;
    ogl_context     context       = materials_ptr->context;

    LOG_INFO("Performance warning: _ogl_materials_bake_uber() called.");

    /* Spawn a new uber  */
    system_hashed_ansi_string material_name = mesh_material_get_name(material);
    ogl_uber                  new_uber      = ogl_uber_create       (context,
                                                                     system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(material_name),
                                                                                                                             " uber") );

    ASSERT_ALWAYS_SYNC(new_uber != NULL,
                       "Could not spawn an uber instance");
    if (new_uber != NULL)
    {
        mesh_material_shading material_shading = MESH_MATERIAL_SHADING_UNKNOWN;

        mesh_material_get_property(material,
                                   MESH_MATERIAL_PROPERTY_SHADING,
                                  &material_shading);

        if (material_shading == MESH_MATERIAL_SHADING_LAMBERT ||
            material_shading == MESH_MATERIAL_SHADING_PHONG)
        {
            const unsigned int n_max_uber_fragment_property_value_pairs = 3; /* sizeof(mappings)  */
            unsigned int       n_uber_fragment_property_value_pairs     = 0;
            unsigned int       uber_fragment_property_value_pairs[n_max_uber_fragment_property_value_pairs*2];

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
                    MESH_MATERIAL_SHADING_PROPERTY_EMISSION,
                    SHADERS_FRAGMENT_UBER_PROPERTY_EMISSION_DATA_SOURCE
                }
            };
            const unsigned int n_mappings = sizeof(mappings) / sizeof(mappings[0]);

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

            /* Add emission/shininess/specular factors (if defined for material) */
            mesh_material_property_attachment emission_attachment  = mesh_material_get_shading_property_attachment_type(material,
                                                                                                                        MESH_MATERIAL_SHADING_PROPERTY_EMISSION);
            mesh_material_property_attachment shininess_attachment = mesh_material_get_shading_property_attachment_type(material,
                                                                                                                        MESH_MATERIAL_SHADING_PROPERTY_SHININESS);
            mesh_material_property_attachment specular_attachment  = mesh_material_get_shading_property_attachment_type(material,
                                                                                                                        MESH_MATERIAL_SHADING_PROPERTY_SPECULAR);

            if (emission_attachment  != MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE ||
                shininess_attachment != MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE ||
                specular_attachment  != MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE)
            {
                /* TODO */
                ASSERT_ALWAYS_SYNC(false, "TODO");
            }

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

                    if (material_shading == MESH_MATERIAL_SHADING_LAMBERT)
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
                                                  "TODO: Unrecognized scene light type");
                            }
                        } /* switch (current_light_type) */
                    } /* if (material_shading == MESH_MATERIAL_SHADING_LAMBERT) */
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "TODO: Unsupported shading algorithm");
                    }

                    if (uber_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE)
                    {
                        ogl_uber_add_light_item(new_uber,
                                                uber_light_type,
                                                n_uber_fragment_property_value_pairs,
                                                uber_fragment_property_value_pairs);
                    } /* if (uber_light_type != UBER_LIGHT_NONE) */
                } /* for (all scene lights) */
            } /* if (scene != NULL) */
        } /* if (material_shading == MESH_MATERIAL_SHADING_LAMBERT || material_shading == MESH_MATERIAL_SHADING_PHONG) */
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
                case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL:           uber_input_attribute = OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL;           break;
                case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_AMBIENT: uber_input_attribute = OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_AMBIENT; break;
                case MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_DIFFUSE: uber_input_attribute = OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_DIFFUSE; break;

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
    scene_light  current_light  = NULL;
    unsigned int n_uber_lights  = 0;
    unsigned int n_scene_lights = 0;

    scene_get_property                  (scene,
                                         SCENE_PROPERTY_N_LIGHTS,
                                        &n_scene_lights);
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
        scene_light_type                 current_light_type      = SCENE_LIGHT_TYPE_UNKNOWN;
        _ogl_uber_item_type              current_uber_item_type  = OGL_UBER_ITEM_UNKNOWN;
        shaders_fragment_uber_light_type current_uber_light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE;

        current_light = scene_get_light_by_index(scene, n_light);

        scene_light_get_property         (current_light,
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

        ogl_uber_get_shader_item_property(uber,
                                          n_light,
                                          OGL_UBER_ITEM_PROPERTY_LIGHT_TYPE,
                                         &current_uber_light_type);

        /* TODO: Expand to support other light types */
        ASSERT_DEBUG_SYNC(current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL ||
                          current_light_type == SCENE_LIGHT_TYPE_POINT,
                          "TODO: Unsupported light type, expand.");

        if (current_light_type == SCENE_LIGHT_TYPE_DIRECTIONAL && (current_uber_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL &&
                                                                   current_uber_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL)  ||
            current_light_type == SCENE_LIGHT_TYPE_POINT       && (current_uber_light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT) )
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

    for (unsigned int n_setting = 0; n_setting < n_forced_settings; ++n_setting)
    {
        _ogl_materials_mesh_material_setting* setting_ptr = NULL;

        if (system_resizable_vector_get_element_at(materials_ptr->forced_mesh_material_settings, n_setting, &setting_ptr) )
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
    mesh_material_shading shading_type                      = MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE;
    mesh_material         special_material_normal           = mesh_material_create(system_hashed_ansi_string_create("Special material: normals"),            materials_ptr->context);
    mesh_material         special_material_texcoord_ambient = mesh_material_create(system_hashed_ansi_string_create("Special material: texcoord (ambient)"), materials_ptr->context);
    mesh_material         special_material_texcoord_diffuse = mesh_material_create(system_hashed_ansi_string_create("Special material: texcoord (diffuse)"), materials_ptr->context);

    mesh_material_set_property(special_material_normal,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type);
    mesh_material_set_property(special_material_texcoord_ambient,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type);
    mesh_material_set_property(special_material_texcoord_diffuse,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type);

    mesh_material_set_shading_property_to_input_fragment_attribute(special_material_normal,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL);
    mesh_material_set_shading_property_to_input_fragment_attribute(special_material_texcoord_ambient,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_AMBIENT);
    mesh_material_set_shading_property_to_input_fragment_attribute(special_material_texcoord_diffuse,
                                                                   MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                                                                   MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_DIFFUSE);

    materials_ptr->special_materials[SPECIAL_MATERIAL_NORMALS]          = special_material_normal;
    materials_ptr->special_materials[SPECIAL_MATERIAL_TEXCOORD_AMBIENT] = special_material_texcoord_ambient;
    materials_ptr->special_materials[SPECIAL_MATERIAL_TEXCOORD_DIFFUSE] = special_material_texcoord_diffuse;
}


/** Please see header for specification */
PUBLIC ogl_materials ogl_materials_create(__in __notnull ogl_context context)
{
    _ogl_materials* materials_ptr = new (std::nothrow) _ogl_materials;

    ASSERT_ALWAYS_SYNC(materials_ptr != NULL, "Out of memory");
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

    ASSERT_ALWAYS_SYNC(new_setting_ptr != NULL, "Out of memory");
    if (new_setting_ptr != NULL)
    {
        /* TODO: This really could check if the property is not already overloaded with another attachment */
        new_setting_ptr->attachment      = attachment;
        new_setting_ptr->attachment_data = attachment_data;
        new_setting_ptr->property        = property;

        system_resizable_vector_push(materials_ptr->forced_mesh_material_settings, new_setting_ptr);
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
                                       __in_opt           scene         scene)
{
    _ogl_materials* materials_ptr = (_ogl_materials*) materials;
    ogl_uber        result        = NULL;

    /* First, iterate over existing uber containers and check if there's a match */
    const unsigned int n_materials = system_resizable_vector_get_amount_of_elements(materials_ptr->ubers);

    for (unsigned int n_material = 0; n_material < n_materials; ++n_material)
    {
        _ogl_materials_uber* uber_ptr = NULL;

        if (system_resizable_vector_get_element_at(materials_ptr->ubers, n_material, &uber_ptr) )
        {
            if (_ogl_materials_are_materials_a_match(uber_ptr->material, material)            &&
                (scene == NULL                                                                ||
                 scene != NULL && _ogl_materials_does_uber_match_scene(uber_ptr->uber, scene) ))
            {
                result = uber_ptr->uber;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not get uber descriptor at index [%d]", n_material);
        }
    } /* for (all materials) */

    if (result == NULL)
    {
        /* Nope? Gotta bake a new uber then */
        ogl_uber new_uber = _ogl_materials_bake_uber(materials, material, scene);

        ASSERT_DEBUG_SYNC(new_uber != NULL, "Could not bake a new uber");
        if (new_uber != NULL)
        {
            _ogl_materials_uber* new_uber_descriptor = new (std::nothrow) _ogl_materials_uber;

            ASSERT_ALWAYS_SYNC(new_uber_descriptor != NULL, "Out of memory");
            if (new_uber_descriptor != NULL)
            {
                const char* src_material_name = system_hashed_ansi_string_get_buffer(mesh_material_get_name(material) );

                new_uber_descriptor->material = mesh_material_create_copy(system_hashed_ansi_string_create_by_merging_two_strings(src_material_name, " copy"), material);
                new_uber_descriptor->uber     = new_uber;
                result                        = new_uber;

                system_resizable_vector_push(materials_ptr->ubers, new_uber_descriptor);
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
