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
        /* Extract surface properties. */
        const char* name            =  surface_funcs_ptr->name  (surface_id);
        float       smoothing_angle = *surface_funcs_ptr->getFlt(surface_id, SURF_SMAN);

        /* Create a corresponding material */
        ASSERT_DEBUG_SYNC(name != NULL,
                          "Surface name is NULL");

        lw_material_dataset_material_id new_material_id = lw_material_dataset_add_material(material_dataset,
                                                                                           system_hashed_ansi_string_create(name) );

        lw_material_dataset_set_material_property(material_dataset,
                                                  new_material_id,
                                                  LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE,
                                                 &smoothing_angle);

        /* Move to the next surface */
        surface_id = surface_funcs_ptr->next(surface_id);
    } /* while (surface_id != 0) */
}


