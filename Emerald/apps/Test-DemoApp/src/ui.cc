/**
 *
 * Test demo application @2015
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_bag.h"
#include "ogl/ogl_ui_dropdown.h"
#include "ogl/ogl_shadow_mapping.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "scene/scene_camera.h"
#include "scene/scene_light.h"
#include "system/system_critical_section.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

PRIVATE system_variant _temp_variant_float                           = NULL;
PRIVATE ogl_text       _text_renderer                                = NULL;
PRIVATE ogl_ui         _ui                                           = NULL;
PRIVATE ogl_ui_bag     _ui_bag                                       = NULL;
PRIVATE ogl_ui_control _ui_color_shadow_map_blur_n_passes_scrollbar  = NULL;
PRIVATE ogl_ui_control _ui_color_shadow_map_blur_n_taps_scrollbar    = NULL;
PRIVATE ogl_ui_control _ui_color_shadow_map_blur_resolution_dropdown = NULL;
PRIVATE ogl_ui_control _ui_color_shadow_map_internalformat_dropdown  = NULL;
PRIVATE ogl_ui_control _ui_depth_shadow_map_internalformat_dropdown  = NULL;
PRIVATE ogl_ui_control _ui_shadow_map_algorithm_dropdown             = NULL;
PRIVATE ogl_ui_control _ui_shadow_map_pl_algorithm_dropdown          = NULL;
PRIVATE ogl_ui_control _ui_shadow_map_size_dropdown                  = NULL;
PRIVATE ogl_ui_control _ui_vsm_cutoff_scrollbar                      = NULL;
PRIVATE ogl_ui_control _ui_vsm_max_variance_scrollbar                = NULL;
PRIVATE ogl_ui_control _ui_vsm_min_variance_scrollbar                = NULL;


scene_light_shadow_map_algorithm shadow_map_algorithm_emerald_enums[] =
{
    SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN,
    SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM
};
system_hashed_ansi_string shadow_map_algorithm_strings[] =
{
    system_hashed_ansi_string_create("Plain Shadow Mapping"),
    system_hashed_ansi_string_create("Variance Shadow Mapping")
};
const uint32_t n_shadow_map_algorithm_emerald_enums = sizeof(shadow_map_algorithm_emerald_enums) /
                                                      sizeof(shadow_map_algorithm_emerald_enums[0]);


scene_light_shadow_map_pointlight_algorithm shadow_map_pointlight_algorithm_emerald_enums[] =
{
    SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL,
    SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID
};
system_hashed_ansi_string shadow_map_pointlight_algorithm_strings[] =
{
    system_hashed_ansi_string_create("6-pass cubical"),
    system_hashed_ansi_string_create("2-pass paraboloid")
};
const uint32_t n_shadow_map_pointlight_algorithm_strings = sizeof(shadow_map_pointlight_algorithm_strings) /
                                                           sizeof(shadow_map_pointlight_algorithm_strings[0]);


const postprocessing_blur_gaussian_resolution color_shadow_map_blur_resolution_enums[] =
{
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_ORIGINAL,
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_HALF,
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_QUARTER,
};
system_hashed_ansi_string color_shadow_map_blur_resolution_strings[] =
{
    system_hashed_ansi_string_create("Original size"),
    system_hashed_ansi_string_create("Half the size"),
    system_hashed_ansi_string_create("Quarter the size"),
};
const uint32_t n_color_shadow_map_blur_resolution_strings = sizeof(color_shadow_map_blur_resolution_strings) /
                                                            sizeof(color_shadow_map_blur_resolution_strings[0]);


const ogl_texture_internalformat color_shadow_map_internalformat_enums[] =
{
    OGL_TEXTURE_INTERNALFORMAT_GL_RG16F,
    OGL_TEXTURE_INTERNALFORMAT_GL_RG32F
};
system_hashed_ansi_string color_shadow_map_internalformat_strings[] =
{
    system_hashed_ansi_string_create("GL_RG16F"),
    system_hashed_ansi_string_create("GL_RG32F"),
};
const uint32_t n_color_shadow_map_internalformat_strings = sizeof(color_shadow_map_internalformat_strings) /
                                                           sizeof(color_shadow_map_internalformat_strings[0]);


const ogl_texture_internalformat depth_shadow_map_internalformat_enums[] =
{
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT24,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32,
    OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32F
};
system_hashed_ansi_string depth_shadow_map_internalformat_strings[] =
{
    system_hashed_ansi_string_create("GL_DEPTH_COMPONENT16"),
    system_hashed_ansi_string_create("GL_DEPTH_COMPONENT24"),
    system_hashed_ansi_string_create("GL_DEPTH_COMPONENT32"),
    system_hashed_ansi_string_create("GL_DEPTH_COMPONENT32F")
};
const uint32_t n_depth_shadow_map_internalformat_strings = sizeof(depth_shadow_map_internalformat_strings) /
                                                           sizeof(depth_shadow_map_internalformat_strings[0]);


const unsigned int shadow_map_size_ints[] =
{
    /* NOTE: The order must correspond to the order used in shadow_map_size_strings */
    4096,
    2048,
    1024,
    512,
    256,
    128
};
system_hashed_ansi_string shadow_map_size_strings[] =
{
    /* NOTE: The order must correspond to the order used in shadow_map_size_ints */
    system_hashed_ansi_string_create("4096x4096"),
    system_hashed_ansi_string_create("2048x2048"),
    system_hashed_ansi_string_create("1024x1024"),
    system_hashed_ansi_string_create("512x512"),
    system_hashed_ansi_string_create("256x256"),
    system_hashed_ansi_string_create("128x128")
};
const uint32_t n_shadow_map_size_strings = sizeof(shadow_map_size_strings)    /
                                           sizeof(shadow_map_size_strings[0]);


/** TODO */
PRIVATE unsigned int _ui_get_current_color_shadow_map_blur_resolution_index()
{
    postprocessing_blur_gaussian_resolution current_shadow_map_blur_resolution = state_get_color_shadow_map_blur_resolution();
    unsigned int                            result                             = -1;

    for (unsigned int n_shadow_map_blur_resolution = 0;
                      n_shadow_map_blur_resolution < n_color_shadow_map_blur_resolution_strings;
                    ++n_shadow_map_blur_resolution)
    {
        if (color_shadow_map_blur_resolution_enums[n_shadow_map_blur_resolution] == current_shadow_map_blur_resolution)
        {
            result = n_shadow_map_blur_resolution;

            break;
        }
    } /* for (all supported shadow map blur resolutions) */

    ASSERT_DEBUG_SYNC(result != -1,
                      "No corresponding shadow map blur resolutions UI entry found");

    return result;
}

/** TODO */
PRIVATE unsigned int _ui_get_current_color_shadow_map_internalformat_index()
{
    ogl_texture_internalformat current_color_shadow_map_internalformat = state_get_color_shadow_map_internalformat();
    unsigned int               result                                  = -1;

    for (unsigned int n_color_shadow_map_internalformat = 0;
                      n_color_shadow_map_internalformat < n_color_shadow_map_internalformat_strings;
                    ++n_color_shadow_map_internalformat)
    {
        if (color_shadow_map_internalformat_enums[n_color_shadow_map_internalformat] == current_color_shadow_map_internalformat)
        {
            result = n_color_shadow_map_internalformat;

            break;
        }
    } /* for (all supported color shadow map internalformats) */

    ASSERT_DEBUG_SYNC(result != -1,
                      "No corresponding color shadow map internalformat UI entry found");

    return result;
}

/** TODO */
PRIVATE unsigned int _ui_get_current_depth_shadow_map_internalformat_index()
{
    ogl_texture_internalformat current_depth_shadow_map_internalformat = state_get_depth_shadow_map_internalformat();
    unsigned int               result                                  = -1;

    for (unsigned int n_depth_shadow_map_internalformat = 0;
                      n_depth_shadow_map_internalformat < n_depth_shadow_map_internalformat_strings;
                    ++n_depth_shadow_map_internalformat)
    {
        if (depth_shadow_map_internalformat_enums[n_depth_shadow_map_internalformat] == current_depth_shadow_map_internalformat)
        {
            result = n_depth_shadow_map_internalformat;

            break;
        }
    } /* for (all supported depth shadow map internalformats) */

    ASSERT_DEBUG_SYNC(result != -1,
                      "No corresponding depth shadow map internalformat UI entry found");

    return result;
}

/** TODO */
PRIVATE unsigned int _ui_get_current_shadow_map_algorithm_index()
{
    scene_light_shadow_map_algorithm current_sm_algo = state_get_shadow_map_algorithm();
    unsigned int                     result          = -1;

    for (unsigned int n_sm_algo = 0;
                      n_sm_algo < n_shadow_map_algorithm_emerald_enums;
                    ++n_sm_algo)
    {
        if (current_sm_algo == shadow_map_algorithm_emerald_enums[n_sm_algo])
        {
            result = n_sm_algo;

            break;
        }
    } /* for (all recognized shadow mapping algorithms) */

    ASSERT_DEBUG_SYNC(result != -1,
                      "Unrecognized sadow map algorithm is currently selected");

    return result;
}

/** TODO */
PRIVATE unsigned int _ui_get_current_color_shadow_map_pointlight_algorithm_index()
{
    scene_light_shadow_map_pointlight_algorithm current_pl_algo = state_get_shadow_map_pointlight_algorithm();
    unsigned int                                result          = -1;

    for (unsigned int n_pl_algo = 0;
                      n_pl_algo < n_shadow_map_pointlight_algorithm_strings;
                    ++n_pl_algo)
    {
        if (current_pl_algo == shadow_map_pointlight_algorithm_emerald_enums[n_pl_algo])
        {
            result = n_pl_algo;

            break;
        }
    }

    ASSERT_DEBUG_SYNC(result != -1,
                      "Unrecognized shadow map point light algorithm is currently selected");

    return result;
}

/** TODO */
PRIVATE unsigned int _ui_get_current_shadow_map_size_index()
{
    unsigned int current_shadow_map_size = state_get_shadow_map_size();
    unsigned int result                  = -1;

    for (unsigned int n_shadow_map_size = 0;
                      n_shadow_map_size < n_shadow_map_size_strings;
                    ++n_shadow_map_size)
    {
        if (shadow_map_size_ints[n_shadow_map_size] == current_shadow_map_size)
        {
            result = n_shadow_map_size;

            break;
        }
    } /* for (all supported shadow map sizes) */

    ASSERT_DEBUG_SYNC(result != -1,
                      "No corresponding shadow map size UI entry found");

    return result;
}

/** TODO */
PRIVATE void _ui_get_current_vsm_blur_n_passes_value(void*          unused,
                                                     system_variant result)
{
    float vsm_blur_n_passes = state_get_shadow_map_vsm_blur_n_passes();

    system_variant_set_float(result,
                             vsm_blur_n_passes);
}

/** TODO */
PRIVATE void _ui_get_current_vsm_blur_taps_value(void*          unused,
                                                 system_variant result)
{
    unsigned int vsm_blur_taps = state_get_shadow_map_vsm_blur_taps();

    system_variant_set_float(result,
                             (float) vsm_blur_taps);
}

/** TODO */
PRIVATE void _ui_get_current_vsm_cutoff_value(void*          unused,
                                              system_variant result)
{
    float vsm_cutoff = state_get_shadow_map_vsm_cutoff();

    system_variant_set_float(result,
                             vsm_cutoff);
}

/** TODO */
PRIVATE void _ui_get_current_vsm_max_variance_value(void*          unused,
                                                    system_variant result)
{
    float vsm_max_variance = state_get_shadow_map_vsm_max_variance();

    system_variant_set_float(result,
                             vsm_max_variance);
}

/** TODO */
PRIVATE void _ui_get_current_vsm_min_variance_value(void*          unused,
                                                    system_variant result)
{
    float vsm_min_variance = state_get_shadow_map_vsm_min_variance();

    system_variant_set_float(result,
                             vsm_min_variance);
}

/** TODO */
PRIVATE void _ui_on_color_shadow_map_blur_resolution_changed(void* unused,
                                                             void* event_user_arg)
{
    postprocessing_blur_gaussian_resolution new_color_sm_blur_resolution = (postprocessing_blur_gaussian_resolution) (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_color_shadow_map_blur_resolution(new_color_sm_blur_resolution);
}

/** TODO */
PRIVATE void _ui_on_color_shadow_map_internalformat_changed(void* unused,
                                                            void* event_user_arg)
{
    ogl_texture_internalformat new_color_sm_internalformat = (ogl_texture_internalformat) (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_color_shadow_map_internalformat(new_color_sm_internalformat);
}

/** TODO */
PRIVATE void _ui_on_depth_shadow_map_internalformat_changed(void* unused,
                                                            void* event_user_arg)
{
    ogl_texture_internalformat new_depth_sm_internalformat = (ogl_texture_internalformat) (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_depth_shadow_map_internalformat(new_depth_sm_internalformat);
}

/** TODO */
PRIVATE void _ui_on_shadow_map_algorithm_changed(void* unused,
                                                 void* event_user_arg)
{
    scene_light_shadow_map_algorithm new_sm_algo = (scene_light_shadow_map_algorithm) (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_shadow_map_algorithm(new_sm_algo);

    /* If VSM is enabled, how both color & depth SM internalformat dropdowns.
     * Otherwise, only show depth SM internalformat dropdown*/
    bool not_visible = false;
    bool visible     = true;

    if (new_sm_algo == SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN)
    {
        ogl_ui_set_control_property(_ui_color_shadow_map_blur_n_passes_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
        ogl_ui_set_control_property(_ui_color_shadow_map_blur_n_taps_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
        ogl_ui_set_control_property(_ui_color_shadow_map_blur_resolution_dropdown,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
        ogl_ui_set_control_property(_ui_color_shadow_map_internalformat_dropdown,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
        ogl_ui_set_control_property(_ui_depth_shadow_map_internalformat_dropdown,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_vsm_cutoff_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
        ogl_ui_set_control_property(_ui_vsm_max_variance_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
        ogl_ui_set_control_property(_ui_vsm_min_variance_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &not_visible);
    }
    else
    {
        ogl_ui_set_control_property(_ui_color_shadow_map_blur_n_passes_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_color_shadow_map_blur_n_taps_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_color_shadow_map_blur_resolution_dropdown,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_color_shadow_map_internalformat_dropdown,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_depth_shadow_map_internalformat_dropdown,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_vsm_cutoff_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_vsm_max_variance_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
        ogl_ui_set_control_property(_ui_vsm_min_variance_scrollbar,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &visible);
    }
}

/** TODO */
PRIVATE void _ui_on_shadow_map_pointlight_algorithm_changed(void* unused,
                                                            void* event_user_arg)
{
    scene_light_shadow_map_pointlight_algorithm new_pl_algo = (scene_light_shadow_map_pointlight_algorithm) (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_shadow_map_pointlight_algorithm(new_pl_algo);
}

/** TODO */
PRIVATE void _ui_on_shadow_map_size_changed(void* unused,
                                            void* event_user_arg)
{
    unsigned int new_shadow_map_size = (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_shadow_map_size(new_shadow_map_size);
}

/** TODO */
PRIVATE void _ui_set_current_vsm_blur_n_passes_value(void*          unused,
                                                     system_variant new_value)
{
    float new_vsm_blur_n_passes_value_float;

    system_variant_get_float              (new_value,
                                          &new_vsm_blur_n_passes_value_float);
    state_set_shadow_map_vsm_blur_n_passes(new_vsm_blur_n_passes_value_float);
}

/** TODO */
PRIVATE void _ui_set_current_vsm_blur_taps_value(void*          unused,
                                                 system_variant new_value)
{
    float new_vsm_blur_taps_value_float;

    system_variant_get_float          (new_value,
                                      &new_vsm_blur_taps_value_float);
    state_set_shadow_map_vsm_blur_taps( (unsigned int) new_vsm_blur_taps_value_float);
}

/** TODO */
PRIVATE void _ui_set_current_vsm_cutoff_value(void*          unused,
                                              system_variant new_value)
{
    float new_vsm_cutoff_value;

    system_variant_get_float       (new_value,
                                   &new_vsm_cutoff_value);
    state_set_shadow_map_vsm_cutoff(new_vsm_cutoff_value);
}

/** TODO */
PRIVATE void _ui_set_current_vsm_max_variance_value(void*          unused,
                                                    system_variant new_value)
{
    float new_vsm_max_variance_value;

    system_variant_get_float             (new_value,
                                         &new_vsm_max_variance_value);
    state_set_shadow_map_vsm_max_variance(new_vsm_max_variance_value);
}

/** TODO */
PRIVATE void _ui_set_current_vsm_min_variance_value(void*          unused,
                                                    system_variant new_value)
{
    float new_vsm_min_variance_value;

    system_variant_get_float             (new_value,
                                         &new_vsm_min_variance_value);
    state_set_shadow_map_vsm_min_variance(new_vsm_min_variance_value);
}


/** Please see header for spec */
PUBLIC void ui_deinit()
{
    ogl_ui_bag_release(_ui_bag);
    ogl_ui_release    (_ui);
    ogl_text_release  (_text_renderer);

    system_variant_release(_temp_variant_float);
}

/** Please see header for spec */
PUBLIC void ui_draw()
{
    ogl_ui_draw  (_ui);
    ogl_text_draw(_context,
                  _text_renderer);
}

/** Please see header for spec */
PUBLIC void ui_init()
{
    const float temp_x1y1[2]      = {0.0f};
    const float text_default_size = 0.5f;
    int         window_size[2]    = {0};

    /* Initialize components required to power UI */
    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    _text_renderer = ogl_text_create(system_hashed_ansi_string_create("Text renderer"),
                                     _context,
                                     system_resources_get_meiryo_font_table(),
                                     window_size[0],
                                     window_size[1]);

    ogl_text_set_text_string_property(_text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                     &text_default_size);

    _ui = ogl_ui_create(_text_renderer,
                        system_hashed_ansi_string_create("UI") );

    /* Add shadow map algorithm dropdown */
    _ui_shadow_map_algorithm_dropdown = ogl_ui_add_dropdown(_ui,
                                                            n_shadow_map_algorithm_emerald_enums,
                                                            shadow_map_algorithm_strings,
                                                            (void**) shadow_map_algorithm_emerald_enums,
                                                            _ui_get_current_shadow_map_algorithm_index(),
                                                            system_hashed_ansi_string_create("Shadow map algorithm"),
                                                            temp_x1y1,
                                                            _ui_on_shadow_map_algorithm_changed,
                                                            NULL); /* fire_user_arg */

    /* Add color shadow map blur n passes scrollbar */
    system_variant blur_n_passes_max_value = system_variant_create_float(4.0f);
    system_variant blur_n_passes_min_value = system_variant_create_float(0.0f);

    _ui_color_shadow_map_blur_n_passes_scrollbar = ogl_ui_add_scrollbar(_ui,
                                                                        system_hashed_ansi_string_create("Color Shadow map blur passes"),
                                                                        OGL_UI_SCROLLBAR_TEXT_LOCATION_LEFT_TO_SLIDER,
                                                                        blur_n_passes_min_value,
                                                                        blur_n_passes_max_value,
                                                                        temp_x1y1,
                                                                        _ui_get_current_vsm_blur_n_passes_value,
                                                                        NULL,
                                                                        _ui_set_current_vsm_blur_n_passes_value,
                                                                        NULL);

    system_variant_release(blur_n_passes_max_value);
    system_variant_release(blur_n_passes_min_value);

    /* Add color shadow map blur n taps dropdown */
    ogl_shadow_mapping context_shadow_mapping = NULL;
    unsigned int       max_n_taps             = 0;
    unsigned int       min_n_taps             = 0;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_SHADOW_MAPPING,
                            &context_shadow_mapping);

    ogl_shadow_mapping_get_property(context_shadow_mapping,
                                    OGL_SHADOW_MAPPING_PROPERTY_N_MAX_BLUR_TAPS,
                                   &max_n_taps);
    ogl_shadow_mapping_get_property(context_shadow_mapping,
                                    OGL_SHADOW_MAPPING_PROPERTY_N_MIN_BLUR_TAPS,
                                   &min_n_taps);

    system_variant max_n_taps_variant = system_variant_create_float((float) max_n_taps);
    system_variant min_n_taps_variant = system_variant_create_float((float) min_n_taps);

    _ui_color_shadow_map_blur_n_taps_scrollbar = ogl_ui_add_scrollbar(_ui,
                                                                      system_hashed_ansi_string_create("Color Shadow map blur taps"),
                                                                      OGL_UI_SCROLLBAR_TEXT_LOCATION_LEFT_TO_SLIDER,
                                                                      min_n_taps_variant,
                                                                      max_n_taps_variant,
                                                                      temp_x1y1,
                                                                      _ui_get_current_vsm_blur_taps_value,
                                                                      NULL,
                                                                      _ui_set_current_vsm_blur_taps_value,
                                                                      NULL);

    system_variant_release(max_n_taps_variant);
    system_variant_release(min_n_taps_variant);

    /* Add color shadow map blur resolution dropdown */
    _ui_color_shadow_map_blur_resolution_dropdown = ogl_ui_add_dropdown(_ui,
                                                                        n_color_shadow_map_blur_resolution_strings,
                                                                        color_shadow_map_blur_resolution_strings,
                                                                        (void**) color_shadow_map_blur_resolution_enums,
                                                                        _ui_get_current_color_shadow_map_blur_resolution_index(),
                                                                        system_hashed_ansi_string_create("Color shadow map blur resolution"),
                                                                        temp_x1y1,
                                                                        _ui_on_color_shadow_map_blur_resolution_changed,
                                                                        NULL); /* fire_user_arg */

    /* Add color shadow map internalformat dropdown */
    _ui_color_shadow_map_internalformat_dropdown = ogl_ui_add_dropdown(_ui,
                                                                       n_color_shadow_map_internalformat_strings,
                                                                       color_shadow_map_internalformat_strings,
                                                                       (void**) color_shadow_map_internalformat_enums,
                                                                       _ui_get_current_color_shadow_map_internalformat_index(),
                                                                       system_hashed_ansi_string_create("Color shadow map internalformat"),
                                                                       temp_x1y1,
                                                                       _ui_on_color_shadow_map_internalformat_changed,
                                                                       NULL); /* fire_user_arg */

    /* Add depth shadow map internalformat dropdown */
    _ui_depth_shadow_map_internalformat_dropdown = ogl_ui_add_dropdown(_ui,
                                                                       n_depth_shadow_map_internalformat_strings,
                                                                       depth_shadow_map_internalformat_strings,
                                                                       (void**) depth_shadow_map_internalformat_enums,
                                                                       _ui_get_current_depth_shadow_map_internalformat_index(),
                                                                       system_hashed_ansi_string_create("Depth shadow map internalformat"),
                                                                       temp_x1y1,
                                                                       _ui_on_depth_shadow_map_internalformat_changed,
                                                                       NULL); /* fire_user_arg */

    /* Add shadow map size dropdown */
    _ui_shadow_map_size_dropdown = ogl_ui_add_dropdown(_ui,
                                                       n_shadow_map_size_strings,
                                                       shadow_map_size_strings,
                                                       (void**) shadow_map_size_ints,
                                                       _ui_get_current_shadow_map_size_index(),
                                                       system_hashed_ansi_string_create("Shadow map size"),
                                                       temp_x1y1,
                                                       _ui_on_shadow_map_size_changed,
                                                       NULL); /* fire_user_arg */

    /* Add point light algorithm dropdown */
    _ui_shadow_map_pl_algorithm_dropdown = ogl_ui_add_dropdown(_ui,
                                                               n_shadow_map_pointlight_algorithm_strings,
                                                               shadow_map_pointlight_algorithm_strings,
                                                               (void**) shadow_map_pointlight_algorithm_emerald_enums,
                                                               _ui_get_current_color_shadow_map_pointlight_algorithm_index(),
                                                               system_hashed_ansi_string_create("Point light CM algorithm"),
                                                               temp_x1y1,
                                                               _ui_on_shadow_map_pointlight_algorithm_changed,
                                                               NULL); /* fire_user_arg */

    /* Add VSM cutoff scrollbar */
    system_variant vsm_cutoff_max_value = system_variant_create_float(1.0f);
    system_variant vsm_cutoff_min_value = system_variant_create_float(0.0f);

    _ui_vsm_cutoff_scrollbar = ogl_ui_add_scrollbar(_ui,
                                                    system_hashed_ansi_string_create("VSM cut-off"),
                                                    OGL_UI_SCROLLBAR_TEXT_LOCATION_LEFT_TO_SLIDER,
                                                    vsm_cutoff_min_value,
                                                    vsm_cutoff_max_value,
                                                    temp_x1y1,
                                                    _ui_get_current_vsm_cutoff_value,
                                                    NULL,
                                                    _ui_set_current_vsm_cutoff_value,
                                                    NULL);

    system_variant_release(vsm_cutoff_max_value);
    system_variant_release(vsm_cutoff_min_value);

    /* Add VSM max variance scrollbar */
    system_variant vsm_max_variance_max_value = system_variant_create_float(0.1f);
    system_variant vsm_max_variance_min_value = system_variant_create_float(4.0f);

    _ui_vsm_max_variance_scrollbar = ogl_ui_add_scrollbar(_ui,
                                                          system_hashed_ansi_string_create("VSM maximum variance"),
                                                          OGL_UI_SCROLLBAR_TEXT_LOCATION_LEFT_TO_SLIDER,
                                                          vsm_max_variance_min_value,
                                                          vsm_max_variance_max_value,
                                                          temp_x1y1,
                                                          _ui_get_current_vsm_max_variance_value,
                                                          NULL,
                                                          _ui_set_current_vsm_max_variance_value,
                                                          NULL);

    system_variant_release(vsm_max_variance_max_value);
    system_variant_release(vsm_max_variance_min_value);

    /* Add VSM min variance scrollbar */
    system_variant vsm_min_variance_max_value = system_variant_create_float(1e-3f);
    system_variant vsm_min_variance_min_value = system_variant_create_float(1e-6f);

    _ui_vsm_min_variance_scrollbar = ogl_ui_add_scrollbar(_ui,
                                                          system_hashed_ansi_string_create("VSM minimum variance"),
                                                          OGL_UI_SCROLLBAR_TEXT_LOCATION_LEFT_TO_SLIDER,
                                                          vsm_min_variance_min_value,
                                                          vsm_min_variance_max_value,
                                                          temp_x1y1,
                                                          _ui_get_current_vsm_min_variance_value,
                                                          NULL,
                                                          _ui_set_current_vsm_min_variance_value,
                                                          NULL);

    system_variant_release(vsm_min_variance_max_value);
    system_variant_release(vsm_min_variance_min_value);

    /* Add a bag which will re-adjust control positions whenever any of the dropdowns
     * is opened */
    const float          control_bag_x1y1[2] = {0.9f, 0.1f};
    const ogl_ui_control ui_controls[]       =
    {
        _ui_shadow_map_algorithm_dropdown,
        _ui_color_shadow_map_blur_n_passes_scrollbar,
        _ui_color_shadow_map_blur_n_taps_scrollbar,
        _ui_color_shadow_map_blur_resolution_dropdown,
        _ui_color_shadow_map_internalformat_dropdown,
        _ui_depth_shadow_map_internalformat_dropdown,
        _ui_shadow_map_size_dropdown,
        _ui_shadow_map_pl_algorithm_dropdown,
        _ui_vsm_cutoff_scrollbar,
        _ui_vsm_max_variance_scrollbar,
        _ui_vsm_min_variance_scrollbar
    };
    const unsigned int n_ui_controls = sizeof(ui_controls) / sizeof(ui_controls[0]);

    _ui_bag = ogl_ui_bag_create(_ui,
                                control_bag_x1y1,
                                n_ui_controls,
                                ui_controls);

    /* Set up a temp variant */
    _temp_variant_float = system_variant_create(SYSTEM_VARIANT_FLOAT);

    /* Configure control visibility */
    _ui_on_shadow_map_algorithm_changed(NULL,
                                        (void*) state_get_shadow_map_algorithm() );
}


