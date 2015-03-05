#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_common.h"
#include "plugin_curves.h"
#include "plugin_pack.h"
#include "plugin_materials.h"
#include "plugin_ui.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_material.h"
#include "scene/scene_texture.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include <sstream>

/** Global data.
 *
 *  Yes, I know. I'm sorry. :-)
 */
PRIVATE system_event            job_done_event                   = NULL;
PRIVATE system_resizable_vector materials_vector                 = NULL;
PRIVATE system_hash64map        surface_id_to_scene_material_map = NULL;
PRIVATE system_resizable_vector texture_filenames_vector         = NULL;


/** TODO */
volatile void ExtractMaterialDataWorkerThreadEntryPoint(__in __notnull void* in_scene_arg)
{
    scene            in_scene          = (scene) in_scene_arg;
    char             text_buffer[1024];

    /* Initialize containers */
    materials_vector                 = system_resizable_vector_create(4, /* capacity */
                                                                      sizeof(scene_material) );
    surface_id_to_scene_material_map = system_hash64map_create       (sizeof(scene_material) );
    texture_filenames_vector         = system_resizable_vector_create(4, /* capacity */
                                                                      sizeof(system_hashed_ansi_string) );

    /* Iterate over all surfaces.
     *
     * NOTE: LW permits surface names to repeat. This does not work exactly correct
     *       with Emerald's Object Manager, so we stick in surface index number right
     *       after LW name, in order to avoid naming collisions.
     */
    unsigned int n_surface  = 0;
    LWSurfaceID  surface_id = surface_funcs_ptr->first();

    while (surface_id != 0)
    {
        curve_container           surface_color[3]                     = {NULL};
        system_hashed_ansi_string surface_color_texture_file_name      = NULL;
        curve_container           surface_glosiness_curve              = NULL;
        curve_container           surface_luminance_curve              = NULL;
        system_hashed_ansi_string surface_luminance_texture_file_name  = NULL;
        system_hashed_ansi_string surface_normal_texture_file_name     = NULL;
        curve_container           surface_reflection_ratio             = NULL;
        system_hashed_ansi_string surface_reflection_texture_file_name = NULL;
        float                     surface_smoothing_angle              = (float) *surface_funcs_ptr->getFlt(surface_id, SURF_SMAN);
        curve_container           surface_specular_ratio               = NULL;
        system_hashed_ansi_string surface_specular_texture_file_name   = NULL;

        /* Form the name */
        system_hashed_ansi_string surface_name;
        const char*               surface_name_raw     = surface_funcs_ptr->name(surface_id);
        std::stringstream         surface_name_sstream;

        surface_name_sstream << surface_name_raw
                             << " "
                             << n_surface;

        surface_name = system_hashed_ansi_string_create(surface_name_sstream.str().c_str() );

        /* Update activity description */
        sprintf_s(text_buffer,
                  sizeof(text_buffer),
                  "Extracting surface [%s] data..",
                  surface_name_raw);

        SetActivityDescription(text_buffer);

        /* Retrieve non-texture property values */
        GetCurveContainerForProperty(surface_name,
                                     ITEM_PROPERTY_SURFACE_GLOSINESS,
                                     surface_id,
                                    &surface_glosiness_curve,
                                     NULL); /* out_curve_id_ptr */
        GetCurveContainerForProperty(surface_name,
                                     ITEM_PROPERTY_SURFACE_LUMINOSITY,
                                     surface_id,
                                    &surface_luminance_curve,
                                     NULL); /* out_curve_id_ptr */

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

                        /* Also stash the filename if not already stored in the filename vector. */
                        if (system_resizable_vector_find(texture_filenames_vector,
                                                        *current_texture.out_texture_file_name_ptr) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(texture_filenames_vector,
                                                        *current_texture.out_texture_file_name_ptr);
                        }

                        /* Make sure the file is included in the final blob */
                        AddFileToFinalBlob(*current_texture.out_texture_file_name_ptr);
                    } /* if (image_id != 0) */
                    else
                    {
                        current_texture_id = 0;
                    }
                } /* if (texture_funcs_ptr->layerType(layer_id) == TLT_IMAGE) */
            } /* if (current_texture_id != 0) */

            for (unsigned int n_property = 0;
                              n_property < sizeof(current_texture.out_property_curves) / sizeof(current_texture.out_property_curves[0]);
                            ++n_property)
            {
                if (current_texture.out_property_curves[n_property] != NULL)
                {
                    GetCurveContainerForProperty(surface_name,
                                                 current_texture.emerald_item_properties[n_property],
                                                 surface_id,
                                                 current_texture.out_property_curves[n_property],
                                                 NULL); /* out_curve_id_ptr */
                }
            }
        } /* for (all texture descriptors) */

        /* Configure new material instance */
        scene_material new_material = scene_material_create(surface_name,
                                                            NULL); /* object_manager_path */

        ASSERT_DEBUG_SYNC(new_material != NULL,
                          "scene_material_create() failed.");
        if (new_material == NULL)
        {
            continue;
        }

        if (surface_color_texture_file_name != NULL)
        {
            scene_material_set_property(new_material,
                                        SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME,
                                       &surface_color_texture_file_name);
        }

        scene_material_set_property(new_material,
                                    SCENE_MATERIAL_PROPERTY_COLOR,
                                    surface_color);

        scene_material_set_property(new_material,
                                    SCENE_MATERIAL_PROPERTY_GLOSINESS,
                                   &surface_glosiness_curve);

        if (surface_luminance_texture_file_name != NULL)
        {
            scene_material_set_property(new_material,
                                        SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME,
                                       &surface_luminance_texture_file_name);
        }

        scene_material_set_property(new_material,
                                    SCENE_MATERIAL_PROPERTY_LUMINANCE,
                                   &surface_luminance_curve);

        if (surface_normal_texture_file_name != NULL)
        {
            scene_material_set_property(new_material,
                                        SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME,
                                       &surface_normal_texture_file_name);
        }

        if (surface_reflection_texture_file_name != NULL)
        {
            scene_material_set_property(new_material,
                                        SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME,
                                       &surface_reflection_texture_file_name);
        }

        scene_material_set_property(new_material,
                                    SCENE_MATERIAL_PROPERTY_REFLECTION_RATIO,
                                   &surface_reflection_ratio);

        scene_material_set_property(new_material,
                                    SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE,
                                   &surface_smoothing_angle);

        if (surface_specular_texture_file_name != NULL)
        {
            scene_material_set_property(new_material,
                                        SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME,
                                       &surface_specular_texture_file_name);
        }

        scene_material_set_property(new_material,
                                    SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO,
                                   &surface_specular_ratio);

        /* Store the surface */
        system_hash64map_insert     (surface_id_to_scene_material_map,
                                     (system_hash64) surface_id,
                                     new_material,
                                     NULL,  /* on_remove_callback */
                                     NULL); /* on_remove_callback_user_arg */
        system_resizable_vector_push(materials_vector,
                                     new_material);

        scene_add_material(in_scene,
                           new_material);

        /* Move to the next surface */
        n_surface ++;
        surface_id = surface_funcs_ptr->next(surface_id);
    } /* while (surface_id != 0) */

    /* Now, scene_multiloader can optimize the texture loading process by loading scene textures
     * in parallel, using multiple threads at a time. Material loading process, on the other hand,
     * is single-threaded.
     *
     * To leverage the architecture, we also create a scene texture for each file name we came up
     * with while preparing materials for the scene.
     */
    const uint32_t n_texture_filenames = system_resizable_vector_get_amount_of_elements(texture_filenames_vector);

    for (uint32_t n_texture_filename = 0;
                  n_texture_filename < n_texture_filenames;
                ++n_texture_filename)
    {
        system_hashed_ansi_string texture_filename = NULL;

        system_resizable_vector_get_element_at(texture_filenames_vector,
                                               n_texture_filename,
                                              &texture_filename);

        ASSERT_DEBUG_SYNC(texture_filename != NULL,
                          "Texture filename is NULL");

        /* Spawn a new scene_texture instance */
        scene_texture new_texture = scene_texture_create(texture_filename,
                                                         NULL, /* object_manager_path */
                                                         texture_filename);

        ASSERT_DEBUG_SYNC(new_texture != NULL,
                          "scene_texture_create() returned a NULL scene_texture instance.");

        scene_add_texture(in_scene,
                          new_texture);
    } /* for (all texture filenames) */

    SetActivityDescription("Inactive");

    system_event_set(job_done_event);
}

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

    if (texture_filenames_vector != NULL)
    {
        system_resizable_vector_release(texture_filenames_vector);

        texture_filenames_vector = NULL;
    }
}

/** Please see header for spec */
PUBLIC system_hash64map GetLWSurfaceIDToSceneMaterialMap()
{
    ASSERT_DEBUG_SYNC(surface_id_to_scene_material_map != NULL,
                      "Material Data module not initialized");

    return surface_id_to_scene_material_map;
}

/** Please see header for spec */
PUBLIC system_event StartMaterialDataExtraction(__in __notnull scene in_scene)
{
    job_done_event = system_event_create(false,  /* manual_reset */
                                         false); /* start_state */

    /* Spawn a worker thread so that we can report the progress. */
    system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                     ExtractMaterialDataWorkerThreadEntryPoint,
                                                                                                     in_scene);

    system_thread_pool_submit_single_task(task);

    return job_done_event;
}


