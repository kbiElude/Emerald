/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_bag.h"
#include "ui/ui_dropdown.h"
#include "ui/ui_frame.h"
#include "ui/ui_scrollbar.h"
#include "ui/ui_shared.h"

#define INTER_CONTROL_DELTA_HORIZONTAL (5) /* in px */
#define INTER_CONTROL_DELTA_VERTICAL   (5) /* in px */

/** Internal types */
typedef struct _ui_bag
{
    system_resizable_vector controls;
    ui_control              frame;
    float                   x1y1[2];
    ui                      ui_instance;

    explicit _ui_bag(ui           in_ui,
                     const float* in_x1y1)
    {
        controls    = NULL;
        frame       = NULL;
        x1y1[0]     = in_x1y1[0];
        x1y1[1]     = in_x1y1[1];
        ui_instance = in_ui;
    }

    ~_ui_bag()
    {
        if (controls != NULL)
        {
            system_resizable_vector_release(controls);

            controls = NULL;
        }
    }
} _ui_label;

/* Forward declarations */
PRIVATE void _ui_bag_position_controls(_ui_bag* bag_ptr);


/** TODO */
PRIVATE void _ui_bag_position_controls(_ui_bag* bag_ptr)
{
    unsigned int n_controls      = 0;
    float        current_x1y1[2] =
    {
        bag_ptr->x1y1[0],
        bag_ptr->x1y1[1]
    };

    system_resizable_vector_get_property(bag_ptr->controls,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_controls);

    /* Determine deltas */
    ral_context   context              = ui_get_context(bag_ptr->ui_instance);
    system_window window               = NULL;
    int           window_dimensions[2] = {0};

    ral_context_get_property  (context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_dimensions);

    const float h_delta = float(INTER_CONTROL_DELTA_HORIZONTAL) / float(window_dimensions[0]);
    const float v_delta = float(INTER_CONTROL_DELTA_VERTICAL)   / float(window_dimensions[1]);

    /* Determine maximum width of all controls */
    float max_width = 0.0f;
    float max_x1    = 0.0f;
    float min_x1    = 1.0f;

    for (unsigned int n_control = 0;
                      n_control < n_controls;
                    ++n_control)
    {
        ui_control control         = NULL;
        float      control_width   = 0.0f;
        float      control_x1y1[2] = {0.0f};

        system_resizable_vector_get_element_at(bag_ptr->controls,
                                               n_control,
                                              &control);

        ui_get_control_property(control,
                                UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED,
                               &control_width);
        ui_get_control_property(control,
                                UI_CONTROL_PROPERTY_GENERAL_X1Y1,
                               &control_x1y1);

        if (control_width > max_width)
        {
            max_width = control_width;
        }

        if (control_x1y1[0] < min_x1)
        {
            min_x1 = control_x1y1[0];
        }
    }

    max_x1 = 1.0f - max_width - 2.0f * h_delta;

    if (current_x1y1[0] < min_x1)
    {
        current_x1y1[0] = min_x1;
    }

    if (current_x1y1[0] > max_x1)
    {
        current_x1y1[0] = max_x1;
    }

    /* Adjust the positions */
    float actual_min_x1 = current_x1y1[0];

    for (unsigned int n_control = 0;
                      n_control < n_controls;
                    ++n_control)
    {
        ui_control control         = NULL;
        float      control_height  = 0.0f;
        bool       control_visible = false;

        system_resizable_vector_get_element_at(bag_ptr->controls,
                                               n_control,
                                              &control);

        ui_get_control_property(control,
                                UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                               &control_visible);

        if (control_visible)
        {
            ui_get_control_property(control,
                                    UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED,
                                   &control_height);

            current_x1y1[1] += v_delta;

            ui_set_control_property(control,
                                    UI_CONTROL_PROPERTY_GENERAL_X1Y1,
                                    current_x1y1);

            current_x1y1[1] += control_height;
        } /* if (control_visible) */
    } /* for (all bag-controlled controls) */

    current_x1y1[1] += v_delta;

    /* Adjust frame size.
     *
     * We need to re-iterate over all controls, as the X1Y1X2Y2 of the controls has likely
     * changed.
     */
    float frame_x1y1x2y2[4];
    float frame_x1y1    [2] =
    {
        current_x1y1[0],
        bag_ptr->x1y1[1]
    };

    for (unsigned int n_control = 0;
                      n_control < n_controls;
                    ++n_control)
    {
        ui_control control         = NULL;
        float      control_width   = 0.0f;
        float      control_x1y1[2] = {0.0f};

        system_resizable_vector_get_element_at(bag_ptr->controls,
                                               n_control,
                                              &control);

        ui_get_control_property(control,
                                UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED,
                               &control_width);
        ui_get_control_property(control,
                                UI_CONTROL_PROPERTY_GENERAL_X1Y1,
                               &control_x1y1);

        if (control_x1y1[0] < frame_x1y1[0])
        {
            frame_x1y1[0] = control_x1y1[0];
        }
    }

    ui_get_control_property(bag_ptr->frame,
                            UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2,
                            frame_x1y1x2y2);

    frame_x1y1x2y2[0] = frame_x1y1[0] - h_delta;
    frame_x1y1x2y2[1] = frame_x1y1[1];
    frame_x1y1x2y2[2] = frame_x1y1x2y2[0] + max_width + 2.0f * h_delta;
    frame_x1y1x2y2[3] = current_x1y1[1];

    ui_set_control_property(bag_ptr->frame,
                            UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2,
                            frame_x1y1x2y2);
}

/** TODO */
PRIVATE void _ui_on_controls_changed_callback(ui_control control,
                                              int        callback_id,
                                              void*      callback_subscriber_data,
                                              void*      callback_data)
{
    /* Reposition the controls and resize the frame */
    _ui_bag_position_controls( (_ui_bag*) callback_subscriber_data);
}

/* Please see header for spec */
PUBLIC EMERALD_API ui_bag ui_bag_create(ui                ui_instance,
                                        const float*      x1y1,
                                        unsigned int      n_controls,
                                        const ui_control* controls)
{
    _ui_bag* new_bag_ptr = new (std::nothrow) _ui_bag(ui_instance,
                                                      x1y1);

    ASSERT_DEBUG_SYNC(new_bag_ptr != NULL,
                      "Out of memory");

    if (new_bag_ptr != NULL)
    {
        /* Spawn the controls vector */
        ASSERT_DEBUG_SYNC(new_bag_ptr->controls == NULL,
                          "_ui_bag spawned a controls vector");

        new_bag_ptr->controls = system_resizable_vector_create(n_controls);

        /* Cache UI controls */
        for (unsigned int n_control = 0;
                          n_control < n_controls;
                        ++n_control)
        {
            ASSERT_DEBUG_SYNC(controls[n_control] != NULL,
                              "Input control at index [%d] is NULL.",
                              n_control);

            system_resizable_vector_push(new_bag_ptr->controls,
                                         controls[n_control]);
        } /* for (all controls) */

        /* Spawn a new frame control */
        new_bag_ptr->frame = ui_add_frame(ui_instance,
                                          x1y1);

        /* The frame should be located underneath all controls.
         *
         * Retrieve the smallest index used by all of the input controls */
        unsigned int deepest_control_index = 0xFFFFFFFF;

        for (unsigned int n_control = 0;
                          n_control < n_controls;
                        ++n_control)
        {
            unsigned int temp_index = 0xFFFFFFFF;

            ui_get_control_property(controls[n_control],
                                    UI_CONTROL_PROPERTY_GENERAL_INDEX,
                                   &temp_index);

            if (temp_index < deepest_control_index)
            {
                deepest_control_index = temp_index;
            }
        } /* for (all controls) */

        /* Register for call-backs from dropdown controls. If we ever receive the
         * call-back, we need to re-position the controls */
        for (unsigned int n_control = 0;
                          n_control < n_controls;
                        ++n_control)
        {
            _ui_control_type control_type = UI_CONTROL_TYPE_UNKNOWN;

            ui_get_control_property(controls[n_control],
                                    UI_CONTROL_PROPERTY_GENERAL_TYPE,
                                   &control_type);

            if (control_type == UI_CONTROL_TYPE_DROPDOWN)
            {
                ui_register_control_callback(ui_instance,
                                             controls[n_control],
                                             UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                             _ui_on_controls_changed_callback,
                                             new_bag_ptr);
                ui_register_control_callback(ui_instance,
                                             controls[n_control],
                                             UI_DROPDOWN_CALLBACK_ID_VISIBILITY_TOGGLE,
                                             _ui_on_controls_changed_callback,
                                             new_bag_ptr);
            }
            else
            if (control_type == UI_CONTROL_TYPE_SCROLLBAR)
            {
                ui_register_control_callback(ui_instance,
                                             controls[n_control],
                                             UI_SCROLLBAR_CALLBACK_ID_VISIBILITY_TOGGLE,
                                             _ui_on_controls_changed_callback,
                                             new_bag_ptr);
            }
        } /* for (all controls) */

        /* Insert the frame underneath */
        ui_reposition_control(new_bag_ptr->frame,
                              deepest_control_index);

        /* Position the controls in a vertical order, one after another. While on it,
         * also resize the underlying frame.
         */
        _ui_bag_position_controls(new_bag_ptr);
    } /* if (new_bag_ptr != NULL) */

    /* All done */
    return (ui_bag) new_bag_ptr;
}

/* Please see header for spec */
PUBLIC EMERALD_API void ui_bag_release(ui_bag bag)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(bag != NULL,
                      "Input bag argument is NULL");

    /* Release the resource */
    delete (_ui_bag*) bag;

    bag = NULL;
}