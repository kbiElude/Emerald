/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_label.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"

const float _ui_button_text_color[] = {1, 1, 1, 1.0f};

/** Internal types */
typedef struct
{
    varia_text_renderer_text_string_id text_id;
    varia_text_renderer                text_renderer;
    bool                               visible;
    float                              x1y1[2];
} _ui_label;

/** Please see header for specification */
PUBLIC void ui_label_deinit(void* internal_instance)
{
    const bool new_visibility = false;
    _ui_label* ui_label_ptr   = reinterpret_cast<_ui_label*>(internal_instance);

    varia_text_renderer_set_text_string_property(ui_label_ptr->text_renderer,
                                                 ui_label_ptr->text_id,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                &new_visibility);

    delete ui_label_ptr;
}

/** Please see header for specification */
PUBLIC void ui_label_get_property(const void*         label,
                                  ui_control_property property,
                                  void*               out_result)
{
    const _ui_label* label_ptr = reinterpret_cast<const _ui_label*>(label);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *reinterpret_cast<bool*>(out_result) = label_ptr->visible;

            break;
        }

        case UI_CONTROL_PROPERTY_LABEL_TEXT_HEIGHT_SS:
        {
            varia_text_renderer_get_text_string_property(label_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,
                                                         label_ptr->text_id,
                                                         out_result);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC void* ui_label_init(ui                        instance,
                           varia_text_renderer       text_renderer,
                           system_hashed_ansi_string name,
                           const float*              x1y1)
{
    _ui_label* new_label_ptr = new (std::nothrow) _ui_label;

    ASSERT_ALWAYS_SYNC(new_label_ptr != nullptr,
                       "Out of memory");

    if (new_label_ptr != nullptr)
    {
        /* Initialize fields */
        memset(new_label_ptr,
               0,
               sizeof(_ui_label) );

        new_label_ptr->x1y1[0] =     x1y1[0];
        new_label_ptr->x1y1[1] = 1 - x1y1[1];

        new_label_ptr->text_renderer = text_renderer;
        new_label_ptr->text_id       = varia_text_renderer_add_string(text_renderer);
        new_label_ptr->visible       = true;

        /* Configure the text to be shown on the button */
        varia_text_renderer_set(new_label_ptr->text_renderer,
                                new_label_ptr->text_id,
                                system_hashed_ansi_string_get_buffer(name) );

        varia_text_renderer_set_text_string_property(new_label_ptr->text_renderer,
                                                     new_label_ptr->text_id,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                     _ui_button_text_color);
        varia_text_renderer_set_text_string_property(new_label_ptr->text_renderer,
                                                     new_label_ptr->text_id,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS,
                                                     x1y1);
    }

    return reinterpret_cast<void*>(new_label_ptr);
}

/** Please see header for specification */
PUBLIC void ui_label_set_property(void*               label,
                                  ui_control_property property,
                                  const void*         data)
{
    _ui_label* label_ptr = reinterpret_cast<_ui_label*>(label);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_LABEL_TEXT:
        {
            varia_text_renderer_set(label_ptr->text_renderer,
                                    label_ptr->text_id,
                                    system_hashed_ansi_string_get_buffer(*(system_hashed_ansi_string*) data));

            break;
        }

        case UI_CONTROL_PROPERTY_LABEL_X1Y1:
        {
            const float* x1y1 = (const float*) data;

            label_ptr->x1y1[0] = x1y1[0];
            label_ptr->x1y1[1] = x1y1[1];

            varia_text_renderer_set_text_string_property(label_ptr->text_renderer,
                                                         label_ptr->text_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS,
                                                         label_ptr->x1y1);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}