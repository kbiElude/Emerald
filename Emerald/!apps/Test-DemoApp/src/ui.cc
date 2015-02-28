/**
 *
 * Test demo application @2015
 *
 */
#include "shared.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_dropdown.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui_texture_preview.h"
#include "scene/scene_camera.h"
#include "system/system_critical_section.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

PRIVATE ogl_text       _text_renderer      = NULL;
PRIVATE ogl_ui         _ui                 = NULL;
PRIVATE ogl_ui_control _ui_texture_preview = NULL;


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
    const float  active_camera_dropdown_x1y1[2] = {0.7f, 0.1f};
    const float  text_default_size              = 0.5f;
    int          window_size[2]                 = {0};

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

    /* No UI components for the time being.. */
}


