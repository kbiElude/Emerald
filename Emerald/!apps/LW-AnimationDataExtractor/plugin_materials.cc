#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_common.h"
#include "plugin_materials.h"
#include "curve/curve_container.h"
#include "scene/scene_material.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Global data.
 *
 *  Yes, I know. I'm sorry. :-)
 */
/** Stores scene_material instances */
system_resizable_vector materials_vector = NULL;

/** Maps LWSurfaceID to scene_material instances */
system_hash64map surface_id_to_scene_material_map = NULL;

/** Please see header for spec */
PUBLIC void DeinitMaterialData()
{
    if (materials_vector != NULL)
    {
        scene_material current_material = NULL;

        while (!system_resizable_vector_pop(materials_vector,
                                           &current_material) )
        {
            scene_material_release(current_material);

            current_material = NULL;
        }

        system_resizable_vector_release(materials_vector);
        materials_vector = NULL;
    }

    if (surface_id_to_scene_material_map != NULL)
    {
        system_hash64map_release(surface_id_to_scene_material_map);

        surface_id_to_scene_material_map = NULL;
    }
}

/** Please see header for spec */
PUBLIC void InitMaterialData(__in __notnull system_hash64map envelope_id_to_curve_container_map)
{
    /* Initialize containers */
    materials_vector                 = system_resizable_vector_create(4, /* capacity */
                                                                      sizeof(scene_material) );
    surface_id_to_scene_material_map = system_hash64map_create       (sizeof(scene_material) );

    /* Iterate over all surfaces */
    LWSurfaceID surface_id = surface_funcs_ptr->first();

    while (surface_id != 0)
    {
        curve_container           surface_color[3]                     = {NULL};
        system_hashed_ansi_string surface_color_texture_file_name      = NULL;
        curve_container           surface_glosiness_curve              = NULL;
        curve_container           surface_luminance_curve              = NULL;
        system_hashed_ansi_string surface_luminance_texture_file_name  = NULL;
        system_hashed_ansi_string surface_name                         = system_hashed_ansi_string_create(surface_funcs_ptr->name(surface_id) );
        system_hashed_ansi_string surface_normal_texture_file_name     = NULL;
        curve_container           surface_reflection_ratio             = NULL;
        system_hashed_ansi_string surface_reflection_texture_file_name = NULL;
        float                     surface_smoothing_angle              = (float) *surface_funcs_ptr->getFlt(surface_id, SURF_SMAN);
        curve_container           surface_specular_ratio               = NULL;
        system_hashed_ansi_string surface_specular_texture_file_name   = NULL;

        /* Retrieve non-texture property values */
        surface_glosiness_curve = GetCurveContainerForProperty(surface_name,
                                                               ITEM_PROPERTY_SURFACE_GLOSINESS,
                                                               surface_id,
                                                               envelope_id_to_curve_container_map);
        surface_luminance_curve = GetCurveContainerForProperty(surface_name,
                                                               ITEM_PROPERTY_SURFACE_LUMINOSITY,
                                                               surface_id,
                                                               envelope_id_to_curve_container_map);

        /* Iterate over various texture types */
        struct _texture
        {
            _item_property             emerald_item_properties[3];
            const char*                lw_surface_id;
            curve_container*           out_property_curves[3];
            system_hashed_ansi_string* out_texture_file_name_ptr;

        } textures[] =
        {
            /* Color texture */
            {
                ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_R,
                ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_G,
                ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_B,
                SURF_COLR,
                surface_color + 0,
                surface_color + 1,
                surface_color + 2,
               &surface_color_texture_file_name
            },

            /* Reflection texture */
            {
                ITEM_PROPERTY_SURFACE_REFLECTION_RATIO,
                ITEM_PROPERTY_UNKNOWN,
                ITEM_PROPERTY_UNKNOWN,
                SURF_RIMG,
               &surface_reflection_ratio,
                NULL,
                NULL,
               &surface_reflection_texture_file_name,
            },

            /* Specular texture */
            {
                ITEM_PROPERTY_SURFACE_SPECULAR_RATIO,
                ITEM_PROPERTY_UNKNOWN,
                ITEM_PROPERTY_UNKNOWN,
                SURF_SPEC,
                &surface_specular_ratio,
                NULL,
                NULL,
               &surface_specular_texture_file_name
            },

            /* Normal texture */
            {
                ITEM_PROPERTY_UNKNOWN,
                ITEM_PROPERTY_UNKNOWN,
                ITEM_PROPERTY_UNKNOWN,
                SURF_BUMP,
                NULL,
                NULL,
                NULL,
               &surface_normal_texture_file_name
            },

            /* Luminance texture */
            {
                ITEM_PROPERTY_UNKNOWN,
                ITEM_PROPERTY_UNKNOWN,
                ITEM_PROPERTY_UNKNOWN,
                SURF_LUMI,
                NULL,
                NULL,
                NULL,
               &surface_luminance_texture_file_name
            }
        };
        const unsigned int n_textures = sizeof(textures) / sizeof(textures[0]);

        for (unsigned int n_texture = 0;
                          n_texture < n_textures;
                        ++n_texture)
        {
            const _texture& current_texture    = textures[n_texture];
            LWTextureID     current_texture_id = surface_funcs_ptr->getTex(surface_id,
                                                                           current_texture.lw_surface_id);

            if (current_texture_id != 0)
            {
                /* Ignore multiple layers. Only take image maps into account */
                LWTLayerID layer_id = texture_funcs_ptr->firstLayer(current_texture_id);

                if (texture_funcs_ptr->layerType(layer_id) == TLT_IMAGE)
                {
                    LWImageID image_id = 0;

                    texture_funcs_ptr->getParam(layer_id,
                                                TXTAG_IMAGE,
                                               &image_id);

                    if (image_id != 0)
                    {
                        const char* image_file_name = image_list_ptr->filename(image_id, 0 /* frame */);

                        *current_texture.out_texture_file_name_ptr = system_hashed_ansi_string_create(image_file_name);
                    } /* if (image_id != 0) */
                    else
                    {
                        current_texture_id = 0;
                    }
                } /* if (texture_funcs_ptr->layerType(layer_id) == TLT_IMAGE) */
            } /* if (current_texture_id != 0) */

            if (current_texture_id == 0)
            {
                for (unsigned int n_property = 0;
                                  n_property < sizeof(current_texture.out_property_curves) / sizeof(current_texture.out_property_curves[0]);
                                ++n_property)
                {
                    if (current_texture.out_property_curves[n_property] != NULL)
                    {
                        *current_texture.out_property_curves[n_property] = GetCurveContainerForProperty(surface_name,
                                                                                                        current_texture.emerald_item_properties[n_property],
                                                                                                        surface_id,
                                                                                                        envelope_id_to_curve_container_map);
                    }
                }
            } /* if (current_texture_id == 0) */
        } /* for (all texture descriptors) */

        /* Configure new material instance */
        scene_material new_material = scene_material_create(surface_name);

        ASSERT_DEBUG_SYNC(new_material != NULL,
                          "scene_material_create() failed.");
        if (new_material == NULL)
        {
            continue;
        }


        /* Move to the next surface */
        surface_id = surface_funcs_ptr->next(surface_id);
    } /* while (surface_id != 0) */
}


