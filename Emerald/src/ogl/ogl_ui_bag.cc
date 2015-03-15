/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_bag.h"
#include "ogl/ogl_ui_dropdown.h"
#include "ogl/ogl_ui_frame.h"
#include "ogl/ogl_ui_shared.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"

#define INTER_CONTROL_DELTA_HORIZONTAL (5) /* in px */
#define INTER_CONTROL_DELTA_VERTICAL   (5) /* in px */

/** Internal types */
typedef struct _ogl_ui_bag
{
    system_resizable_vector controls;
    ogl_ui_control          frame;
    float                   x1y1[2];
    ogl_ui                  ui;

    explicit _ogl_ui_bag(__in           __notnull ogl_ui       in_ui,
                         __in_ecount(2) __notnull const float* in_x1y1)
    {
        controls = NULL;
        frame    = NULL;
        x1y1[0]  = in_x1y1[0];
        x1y1[1]  = in_x1y1[1];
        ui       = in_ui;
    }

    ~_ogl_ui_bag()
    {
        if (controls != NULL)
        {
            system_resizable_vector_release(controls);

            controls = NULL;
        }
    }
} _ogl_ui_label;

/* Forward declarations */
PRIVATE void _ogl_ui_bag_position_controls(__in __notnull _ogl_ui_bag* bag_ptr);


/** TODO */
PRIVATE void _ogl_ui_bag_position_controls(__in __notnull _ogl_ui_bag* bag_ptr)
{
    const unsigned int n_controls      = system_resizable_vector_get_amount_of_elements(bag_ptr->controls);
    float              current_x1y1[2] =
    {
        bag_ptr->x1y1[0],
        bag_ptr->x1y1[1]
    };

    /* Determine deltas */
    ogl_context   context              = ogl_ui_get_context(bag_ptr->ui);
    system_window window               = NULL;
    int           window_dimensions[2] = {0};

    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
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
        ogl_ui_control control         = NULL;
        float          control_width   = 0.0f;
        float          control_x1y1[2] = {0.0f};

        system_resizable_vector_get_element_at(bag_ptr->controls,
                                               n_control,
                                              &control);

        ogl_ui_get_control_property(control,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED,
                                   &control_width);
        ogl_ui_get_control_property(control,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_X1Y1,
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
        ogl_ui_control control         = NULL;
        float          control_height  = 0.0f;
        bool           control_visible = false;

        system_resizable_vector_get_element_at(bag_ptr->controls,
                                               n_control,
                                              &control);

        ogl_ui_get_control_property(control,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                   &control_visible);

        if (control_visible)
        {
            ogl_ui_get_control_property(control,
                                        OGL_UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED,
                                       &control_height);

            current_x1y1[1] += v_delta;

            ogl_ui_set_control_property(control,
                                        OGL_UI_CONTROL_PROPERTY_GENERAL_X1Y1,
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
        ogl_ui_control control         = NULL;
        float          control_width   = 0.0f;
        float          control_x1y1[2] = {0.0f};

        system_resizable_vector_get_element_at(bag_ptr->controls,
                                               n_control,
                                              &control);

        ogl_ui_get_control_property(control,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED,
                                   &control_width);
        ogl_ui_get_control_property(control,
                                    OGL_UI_CONTROL_PROPERTY_GENERAL_X1Y1,
                                   &control_x1y1);

        if (control_x1y1[0] < frame_x1y1[0])
        {
            frame_x1y1[0] = control_x1y1[0];
        }
    }

    ogl_ui_get_control_property(bag_ptr->frame,
                                OGL_UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2,
                                frame_x1y1x2y2);

    frame_x1y1x2y2[0] = frame_x1y1[0] - h_delta;
    frame_x1y1x2y2[1] = frame_x1y1[1];
    frame_x1y1x2y2[2] = frame_x1y1x2y2[0] + max_width + 2.0f * h_delta;
    frame_x1y1x2y2[3] = current_x1y1[1];

    ogl_ui_set_control_property(bag_ptr->frame,
                                OGL_UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2,
                                frame_x1y1x2y2);
}

/** TODO */
PRIVATE void _ogl_ui_on_controls_changed_callback(__in __notnull ogl_ui_control control,
                                                  __in           int            callback_id,
                                                  __in __notnull void*          callback_subscriber_data,
                                                  __in __notnull void*          callback_data)
{
    /* Reposition the controls and resize the frame */
    _ogl_ui_bag_position_controls( (_ogl_ui_bag*) callback_subscriber_data);
}

/* Please see header for spec */
PUBLIC EMERALD_API ogl_ui_bag ogl_ui_bag_create(__in                    __notnull ogl_ui                ui,
                                                __in_ecount(2)          __notnull const float*          x1y1,
                                                __in                              unsigned int          n_controls,
                                                __in_ecount(n_controls) __notnull const ogl_ui_control* controls)
{
    _ogl_ui_bag* new_bag_ptr = new (std::nothrow) _ogl_ui_bag(ui,
                                                              x1y1);

    ASSERT_DEBUG_SYNC(new_bag_ptr != NULL,
                      "Out of memory");

    if (new_bag_ptr != NULL)
    {
        /* Spawn the controls vector */
        ASSERT_DEBUG_SYNC(new_bag_ptr->controls == NULL,
                          "_ogl_ui_bag spawned a controls vector");

        new_bag_ptr->controls = system_resizable_vector_create(n_controls,
                                                               sizeof(ogl_ui_control) );

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
        new_bag_ptr->frame = ogl_ui_add_frame(ui,
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

            ogl_ui_get_control_property(controls[n_control],
                                        OGL_UI_CONTROL_PROPERTY_GENERAL_INDEX,
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
            _ogl_ui_control_type control_type = OGL_UI_CONTROL_TYPE_UNKNOWN;

            ogl_ui_get_control_property(controls[n_control],
                                        OGL_UI_CONTROL_PROPERTY_GENERAL_TYPE,
                                       &control_type);

            if (control_type == OGL_UI_CONTROL_TYPE_DROPDOWN)
            {
                ogl_ui_register_control_callback(ui,
                                                 controls[n_control],
                                                 OGL_UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                                 _ogl_ui_on_controls_changed_callback,
                                                 new_bag_ptr);
                ogl_ui_register_control_callback(ui,
                                                 controls[n_control],
                                                 OGL_UI_DROPDOWN_CALLBACK_ID_VISIBILITY_TOGGLE,
                                                 _ogl_ui_on_controls_changed_callback,
                                                 new_bag_ptr);
            }
        } /* for (all controls) */

        /* Insert the frame underneath */
        ogl_ui_reposition_control(new_bag_ptr->frame,
                                  deepest_control_index);

        /* Position the controls in a vertical order, one after another. While on it,
         * also resize the underlying frame.
         */
        _ogl_ui_bag_position_controls(new_bag_ptr);
    } /* if (new_bag_ptr != NULL) */

    /* All done */
    return (ogl_ui_bag) new_bag_ptr;
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_ui_bag_release(__in __notnull ogl_ui_bag bag)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(bag != NULL,
                      "Input bag argument is NULL");

    /* Release the resource */
    delete (_ogl_ui_bag*) bag;

    bag = NULL;
}