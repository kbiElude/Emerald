/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_label.h"
#include "ogl/ogl_ui_shared.h"
#include "ral/ral_context.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

const float _ui_button_text_color[] = {1, 1, 1, 1.0f};

/** Internal types */
typedef struct
{
    ogl_text_string_id  text_id;
    ogl_text            text_renderer;
    bool                visible;
    float               x1y1[2];
} _ogl_ui_label;

/** Please see header for specification */
PUBLIC void ogl_ui_label_deinit(void* internal_instance)
{
    const bool     new_visibility = false;
    _ogl_ui_label* ui_label_ptr   = (_ogl_ui_label*) internal_instance;

    ogl_text_set_text_string_property(ui_label_ptr->text_renderer,
                                      ui_label_ptr->text_id,
                                      OGL_TEXT_STRING_PROPERTY_VISIBILITY,
                                      &new_visibility);

    delete ui_label_ptr;
}

/** Please see header for specification */
PUBLIC void ogl_ui_label_get_property(const void*              label,
                                      _ogl_ui_control_property property,
                                      void*                    out_result)
{
    const _ogl_ui_label* label_ptr = (const _ogl_ui_label*) label;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = label_ptr->visible;

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_LABEL_TEXT_HEIGHT_SS:
        {
            ogl_text_get_text_string_property(label_ptr->text_renderer,
                                              OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,
                                              label_ptr->text_id,
                                              out_result);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}

/** Please see header for specification */
PUBLIC void* ogl_ui_label_init(ogl_ui                    instance,
                               ogl_text                  text_renderer,
                               system_hashed_ansi_string name,
                               const float*              x1y1)
{
    _ogl_ui_label* new_label = new (std::nothrow) _ogl_ui_label;

    ASSERT_ALWAYS_SYNC(new_label != NULL,
                       "Out of memory");

    if (new_label != NULL)
    {
        /* Initialize fields */
        memset(new_label,
               0,
               sizeof(_ogl_ui_label) );

        new_label->x1y1[0] =     x1y1[0];
        new_label->x1y1[1] = 1 - x1y1[1];

        new_label->text_renderer = text_renderer;
        new_label->text_id       = ogl_text_add_string(text_renderer);
        new_label->visible       = true;

        /* Configure the text to be shown on the button */
        ogl_text_set(new_label->text_renderer,
                     new_label->text_id,
                     system_hashed_ansi_string_get_buffer(name) );

        ogl_text_set_text_string_property(new_label->text_renderer,
                                          new_label->text_id,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          _ui_button_text_color);
        ogl_text_set_text_string_property(new_label->text_renderer,
                                          new_label->text_id,
                                          OGL_TEXT_STRING_PROPERTY_POSITION_SS,
                                          x1y1);
    } /* if (new_label != NULL) */

    return (void*) new_label;
}

/** Please see header for specification */
PUBLIC void ogl_ui_label_set_property(void*                    label,
                                      _ogl_ui_control_property property,
                                      const void*              data)
{
    _ogl_ui_label* label_ptr = (_ogl_ui_label*) label;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_LABEL_TEXT:
        {
            ogl_text_set(label_ptr->text_renderer,
                         label_ptr->text_id,
                         system_hashed_ansi_string_get_buffer(*(system_hashed_ansi_string*) data));

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_LABEL_X1Y1:
        {
            const float* x1y1 = (const float*) data;

            label_ptr->x1y1[0] = x1y1[0];
            label_ptr->x1y1[1] = x1y1[1];

            ogl_text_set_text_string_property(label_ptr->text_renderer,
                                              label_ptr->text_id,
                                              OGL_TEXT_STRING_PROPERTY_POSITION_SS,
                                              label_ptr->x1y1);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}