#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_common.h"
#include "plugin_lights.h"

#include "scene/scene.h"
#include "scene/scene_light.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

/** TODO */
PRIVATE void _update_light_properties(__in __notnull scene_light               new_light,
                                      __in           scene_light_type          new_light_type,
                                      __in __notnull system_hashed_ansi_string new_light_name_has,
                                      __in           LWItemID                  new_light_item_id,
                                      __in __notnull system_hash64map          curve_id_to_curve_container_map)
{
    curve_container new_light_color_r         = NULL;
    curve_container new_light_color_g         = NULL;
    curve_container new_light_color_b         = NULL;
    curve_container new_light_color_intensity = NULL;
    curve_container new_light_rotation_b      = NULL;
    curve_container new_light_rotation_h      = NULL;
    curve_container new_light_rotation_p      = NULL;
    curve_container new_light_translation_x   = NULL;
    curve_container new_light_translation_y   = NULL;
    curve_container new_light_translation_z   = NULL;
    bool            new_light_uses_shadow_map = false;

    new_light_color_r         = GetCurveContainerForProperty(new_light_name_has,
                                                             (new_light_type == SCENE_LIGHT_TYPE_AMBIENT) ? ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_R
                                                                                                          : ITEM_PROPERTY_LIGHT_COLOR_R,
                                                             new_light_item_id,
                                                             curve_id_to_curve_container_map);
    new_light_color_g         = GetCurveContainerForProperty(new_light_name_has,
                                                             (new_light_type == SCENE_LIGHT_TYPE_AMBIENT) ? ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_G
                                                                                                          : ITEM_PROPERTY_LIGHT_COLOR_G,
                                                             new_light_item_id,
                                                             curve_id_to_curve_container_map);
    new_light_color_b         = GetCurveContainerForProperty(new_light_name_has,
                                                             (new_light_type == SCENE_LIGHT_TYPE_AMBIENT) ? ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_B
                                                                                                          : ITEM_PROPERTY_LIGHT_COLOR_B,
                                                             new_light_item_id,
                                                             curve_id_to_curve_container_map);
    new_light_color_intensity = GetCurveContainerForProperty(new_light_name_has,
                                                             (new_light_type == SCENE_LIGHT_TYPE_AMBIENT) ? ITEM_PROPERTY_LIGHT_AMBIENT_INTENSITY
                                                                                                          : ITEM_PROPERTY_LIGHT_COLOR_INTENSITY,
                                                             new_light_item_id,
                                                             curve_id_to_curve_container_map);

    if (new_light_type != SCENE_LIGHT_TYPE_AMBIENT)
    {
        new_light_rotation_b      = GetCurveContainerForProperty(new_light_name_has,
                                                                 ITEM_PROPERTY_ROTATION_B,
                                                                 new_light_item_id,
                                                                 curve_id_to_curve_container_map);
        new_light_rotation_h      = GetCurveContainerForProperty(new_light_name_has,
                                                                 ITEM_PROPERTY_ROTATION_H,
                                                                 new_light_item_id,
                                                                 curve_id_to_curve_container_map);
        new_light_rotation_p      = GetCurveContainerForProperty(new_light_name_has,
                                                                 ITEM_PROPERTY_ROTATION_P,
                                                                 new_light_item_id,
                                                                 curve_id_to_curve_container_map);
        new_light_translation_x   = GetCurveContainerForProperty(new_light_name_has,
                                                                 ITEM_PROPERTY_TRANSLATION_X,
                                                                 new_light_item_id,
                                                                 curve_id_to_curve_container_map);
        new_light_translation_y   = GetCurveContainerForProperty(new_light_name_has,
                                                                 ITEM_PROPERTY_TRANSLATION_Y,
                                                                 new_light_item_id,
                                                                 curve_id_to_curve_container_map);
        new_light_translation_z   = GetCurveContainerForProperty(new_light_name_has,
                                                                 ITEM_PROPERTY_TRANSLATION_Z,
                                                                 new_light_item_id,
                                                                 curve_id_to_curve_container_map);
        new_light_uses_shadow_map = !(light_info_ptr->shadowType(new_light_item_id) == LWLSHAD_OFF);
    }

    /* Configure the light using the properties we've retrieved */
    curve_container new_color_curves[] =
    {
        new_light_color_r,
        new_light_color_g,
        new_light_color_b
    };
    curve_container new_light_rotation_curves[] =
    {
        new_light_rotation_h,
        new_light_rotation_p,
        new_light_rotation_b
    };
    curve_container new_light_translation_curves[] =
    {
        new_light_translation_x,
        new_light_translation_y,
        new_light_translation_z
    };

    scene_light_set_property(new_light,
                             SCENE_LIGHT_PROPERTY_COLOR,
                             new_color_curves);
    scene_light_set_property(new_light,
                             SCENE_LIGHT_PROPERTY_COLOR_INTENSITY,
                            &new_light_color_intensity);

    if (new_light_type != SCENE_LIGHT_TYPE_AMBIENT)
    {
        scene_light_set_property(new_light,
                                 SCENE_LIGHT_PROPERTY_ROTATION,
                                 new_light_rotation_curves);
        scene_light_set_property(new_light,
                                 SCENE_LIGHT_PROPERTY_TRANSLATION,
                                 new_light_translation_curves);
        scene_light_set_property(new_light,
                                 SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,
                                &new_light_uses_shadow_map);
    }
}

/** Please see header for spec */
void FillSceneWithLightData(__in __notnull scene            in_scene,
                            __in __notnull system_hash64map curve_id_to_curve_container_map)
{
    /* Extract available lights.
     *
     * Also remember to extract ambient light data.
     */
    LWItemID light_item_id = item_info_ptr->first(LWI_LIGHT,
                                                  LWITEM_NULL);

    while (light_item_id != LWITEM_NULL)
    {
        /* Extract light name & type */
        const char*               light_name         = item_info_ptr->name             (light_item_id);
        system_hashed_ansi_string light_name_has     = system_hashed_ansi_string_create(light_name);
        int                       light_type         = light_info_ptr->type            (light_item_id);
        scene_light_type          light_type_emerald = SCENE_LIGHT_TYPE_UNKNOWN;

        /* Instantiate a new light */
        scene_light new_light = NULL;

        switch (light_type)
        {
            case LWLIGHT_DISTANT:
            {
                light_type_emerald = SCENE_LIGHT_TYPE_DIRECTIONAL;
                new_light          = scene_light_create_directional(light_name_has);

                break;
            } /* case LWLIGHT_DISTANT: */

            case LWLIGHT_POINT:
            {
                light_type_emerald = SCENE_LIGHT_TYPE_POINT;
                new_light          = scene_light_create_point(light_name_has);

                break;
            } /* case LWLIGHT_POINT: */

            default:
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized light type");
            }
        } /* switch (light_type) */

        ASSERT_ALWAYS_SYNC(new_light != NULL,
                           "scene_light_create() failed.");

        /* Extract light properties */
        _update_light_properties(new_light,
                                 light_type_emerald,
                                 light_name_has,
                                 light_item_id,
                                 curve_id_to_curve_container_map);

        /* Add the light */
        if (!scene_add_light(in_scene,
                             new_light) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not add new light [%s]",
                               light_name);
        }

        /* Move to next light */
        light_item_id = item_info_ptr->next(light_item_id);
    } /* while (light_item_id != LWITEM_NULL) */

    /* Extract ambient light props & add the light to the scene */
    system_hashed_ansi_string scene_name = NULL;

    scene_get_property(in_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    system_hashed_ansi_string ambient_light_name_has = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(scene_name),
                                                                                                                                 " ambient light");
    scene_light               ambient_light          = scene_light_create_ambient(ambient_light_name_has);

    _update_light_properties(ambient_light,
                             SCENE_LIGHT_TYPE_AMBIENT,
                             ambient_light_name_has,
                             0, /* new_light_item_id */
                             curve_id_to_curve_container_map);

    if (!scene_add_light(in_scene,
                         ambient_light) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not add ambient light.");
    }
}


