#define _MSWIN

#include "shared.h"
#include "lw/lw_material_dataset.h"

#include "plugin.h"
#include "plugin_materials.h"

#include "lw/lw_material_dataset.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

/** TODO */
void FillMaterialDataset(lw_dataset dataset)
{
    lw_material_dataset material_dataset = NULL;

    lw_dataset_get_property(dataset,
                            LW_DATASET_PROPERTY_MATERIAL_DATASET,
                           &material_dataset);

    /* Iterate over all surfaces */
    LWSurfaceID surface_id = surface_funcs_ptr->first();

    while (surface_id != 0)
    {
        /* We're taking a bit of a shortcut here and assume surfaces are uniquely identified by
         * their name. This DOES NOT seem to be always the case in LW, so make sure we detect
         * cases where current approach needs to be reworked */
        lw_material_dataset_material_id existing_material_id = -1;
        const char*                     name                 =  surface_funcs_ptr->name  (surface_id);
        float                           smoothing_angle      = *surface_funcs_ptr->getFlt(surface_id, SURF_SMAN);

        if (lw_material_dataset_get_material_by_name(material_dataset,
                                                     system_hashed_ansi_string_create(name),
                                                    &existing_material_id) )
        {
            float existing_smoothing_angle = 0.0f;

            lw_material_dataset_get_material_property(material_dataset,
                                                      existing_material_id,
                                                      LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE,
                                                     &existing_smoothing_angle);

            ASSERT_DEBUG_SYNC(fabs(existing_smoothing_angle - smoothing_angle) < 1e-5f,
                              "Duplicate surface [%s] entries were reported but feature different settings!",
                              name);
        }
        else
        {
            /* Create a corresponding material */
            ASSERT_DEBUG_SYNC(name != NULL,
                              "Surface name is NULL");

            lw_material_dataset_material_id new_material_id = lw_material_dataset_add_material(material_dataset,
                                                                                               system_hashed_ansi_string_create(name) );

            lw_material_dataset_set_material_property(material_dataset,
                                                      new_material_id,
                                                      LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE,
                                                     &smoothing_angle);
        }

        /* Move to the next surface */
        surface_id = surface_funcs_ptr->next(surface_id);
    } /* while (surface_id != 0) */
}


