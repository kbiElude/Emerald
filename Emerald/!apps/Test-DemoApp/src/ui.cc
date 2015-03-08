/**
 *
 * Test demo application @2015
 *
 */
#include "shared.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_dropdown.h"
#include "scene/scene_camera.h"
#include "system/system_critical_section.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

PRIVATE ogl_text       _text_renderer               = NULL;
PRIVATE ogl_ui         _ui                          = NULL;
PRIVATE ogl_ui_control _ui_shadow_map_size_dropdown = NULL;


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
PRIVATE void _on_shadow_map_size_changed(void* unused,
                                         void* event_user_arg)
{
    unsigned int new_shadow_map_size = (unsigned int) event_user_arg;

    /* Update Emerald state */
    state_set_shadow_map_size(new_shadow_map_size);
}


/** Please see header for spec */
PUBLIC void ui_deinit()
{
    ogl_ui_release  (_ui);
    ogl_text_release(_text_renderer);
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
    const float  shadow_map_size_dropdown_x1y1[2] = {0.9f, 0.1f};
    const float  text_default_size                = 0.5f;
    int          window_size[2]                   = {0};

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

    /* Add shadow map size dropdown */
    _ui_shadow_map_size_dropdown = ogl_ui_add_dropdown(_ui,
                                                       n_shadow_map_size_strings,
                                                       shadow_map_size_strings,
                                                       (void**) shadow_map_size_ints,
                                                       _ui_get_current_shadow_map_size_index(),
                                                       system_hashed_ansi_string_create("Shadow map size"),
                                                       shadow_map_size_dropdown_x1y1,
                                                       _on_shadow_map_size_changed,
                                                       NULL); /* fire_user_arg */
}


