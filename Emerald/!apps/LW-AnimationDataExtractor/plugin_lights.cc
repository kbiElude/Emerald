#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_lights.h"

#include "lw/lw_light_dataset.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

/** TODO */
void FillLightDataset(lw_dataset dataset)
{
    lw_curve_dataset curve_dataset = NULL;
    lw_light_dataset light_dataset = NULL;

    lw_dataset_get_property(dataset,
                            LW_DATASET_PROPERTY_CURVE_DATASET,
                           &curve_dataset);
    lw_dataset_get_property(dataset,
                            LW_DATASET_PROPERTY_LIGHT_DATASET,
                           &light_dataset);

    /* Extract ambient light info */
    LWDVector ambient_color_double;
    float     ambient_color_float[3] = {0};

    light_info_ptr->ambient(0, /* LWTime */
                            ambient_color_double);

    ambient_color_float[0] = (float) ambient_color_double[0];
    ambient_color_float[1] = (float) ambient_color_double[1];
    ambient_color_float[2] = (float) ambient_color_double[2];

    lw_light_dataset_set_property(light_dataset,
                                  LW_LIGHT_DATASET_PROPERTY_AMBIENT_COLOR,
                                  ambient_color_float);

    /* Make sure no ambient color / intensity curves are defined */
    curve_container ambient_color_b_curve   = NULL;
    curve_container ambient_color_g_curve   = NULL;
    curve_container ambient_color_r_curve   = NULL;
    curve_container ambient_intensity_curve = NULL;

    lw_curve_dataset_get_curve_by_curve_name(curve_dataset,
                                             system_hashed_ansi_string_create("Global.AmbientIntensity"),
                                            &ambient_intensity_curve);
    lw_curve_dataset_get_curve_by_curve_name(curve_dataset,
                                             system_hashed_ansi_string_create("Global.AmbientColor.R"),
                                            &ambient_color_r_curve);
    lw_curve_dataset_get_curve_by_curve_name(curve_dataset,
                                             system_hashed_ansi_string_create("Global.AmbientColor.G"),
                                            &ambient_color_g_curve);
    lw_curve_dataset_get_curve_by_curve_name(curve_dataset,
                                             system_hashed_ansi_string_create("Global.AmbientColor.B"),
                                            &ambient_color_b_curve);

    ASSERT_DEBUG_SYNC(ambient_color_b_curve   == NULL &&
                      ambient_color_g_curve   == NULL &&
                      ambient_color_r_curve   == NULL &&
                      ambient_intensity_curve == NULL,
                      "Curve-based ambient light properties are NOT supported (yet)");
}


