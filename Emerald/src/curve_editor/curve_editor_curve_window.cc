/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "main.h"
#include "../resource.h"
#include "curve/curve_container.h"
#include "curve/curve_segment.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_curve_window.h"
#include "curve_editor/curve_editor_curve_window_renderer.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include "system/system_window.h"
#include "system/system_variant.h"


#define WM_CUSTOMIZEDSHUTDOWN (WM_USER + 1)


/* Private definitions */
typedef struct
{
    curve_container curve;

    /* Control window handles */
    HWND window_handle;
    HWND window_handle_view;
    HWND window_handle_help_groupbox;
    HWND window_handle_help_static;
    HWND window_handle_actions_groupbox;
    HWND window_handle_property_1_static;
    HWND window_handle_property_1_editbox;
    HWND window_handle_property_2_static;
    HWND window_handle_property_2_editbox;
    HWND window_handle_property_3_static;
    HWND window_handle_property_3_editbox;
    HWND window_handle_property_4_static;
    HWND window_handle_property_4_editbox;

    /* Scaling related stuff */
    RECT ref_window_rect_view;
    RECT ref_window_rect_help_groupbox;
    RECT ref_window_rect_help_static;
    RECT ref_window_rect_actions_groupbox;
    RECT ref_window_rect_property_1_static;
    RECT ref_window_rect_property_1_editbox;
    RECT ref_window_rect_property_2_static;
    RECT ref_window_rect_property_2_editbox;
    RECT ref_window_rect_property_3_static;
    RECT ref_window_rect_property_3_editbox;
    RECT ref_window_rect_property_4_static;
    RECT ref_window_rect_property_4_editbox;
    int  ref_client_width;
    int  ref_client_height;
    RECT ref_window_rect;

    /* Rendering handler stuff */
    ogl_context                        context;
    system_hashed_ansi_string          name;
    curve_editor_curve_window_renderer renderer;
    system_critical_section            serialization_cs;

    /* Events */
    system_event dialog_created_event;
    system_event dialog_thread_event;

    /* Call-backs */
    PFNONCURVEWINDOWRELEASECALLBACKHANDLERPROC release_callback_handler;
    void*                                      release_callback_handler_arg;

    /* Handles */
    curve_segment         edited_segment;
    curve_segment_node_id edited_segment_node_id;
    bool                  should_ignore_change_notifications;

} _curve_editor_curve_window;

/* Forward declarations */
PRIVATE void _curve_editor_curve_window_initialize_dialog (__in __notnull _curve_editor_curve_window*, HWND dialog_handle);
PRIVATE void _curve_editor_curve_window_on_property_edited(_curve_editor_curve_window*, uint32_t);
PRIVATE void _curve_editor_curve_window_on_resize         (_curve_editor_curve_window* descriptor, unsigned int new_width, unsigned int new_height);
PRIVATE void _curve_editor_curve_window_release           (_curve_editor_curve_window* descriptor);


/** TODO */
PRIVATE volatile void _curve_editor_curve_window_dialog_close_button_handler(void* descriptor)
{
    _curve_editor_curve_window* descriptor_ptr = (_curve_editor_curve_window*) descriptor;

    _curve_editor_curve_window_release(descriptor_ptr);
}

/** TODO */
PRIVATE BOOL CALLBACK _curve_editor_curve_window_dialog_message_handler(HWND dialog_handle, UINT message_id, WPARAM wparam, LPARAM lparam)
{
    switch (message_id)
    {
        case WM_CLOSE:
        {
            /* In order to close the dialog, we need to perform the whole process from another thread. Otherwise we'll get a classical thread lock-up,
             * as curve_editor_curve_window_release() blocks until the window thread exits, which can't happen from within a message handler.
             */
            _curve_editor_curve_window*        descriptor = (_curve_editor_curve_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);
            system_thread_pool_task_descriptor task       = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL, _curve_editor_curve_window_dialog_close_button_handler, descriptor);

            system_thread_pool_submit_single_task(task);
            break;
        }

        case WM_COMMAND:
        {
            switch (HIWORD(wparam) )
            {
                case EN_CHANGE:
                {
                    _curve_editor_curve_window* descriptor = (_curve_editor_curve_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);

                    if (!descriptor->should_ignore_change_notifications)
                    {
                        switch (LOWORD(wparam))
                        {
                            case IDC_CURVE_EDITOR_CURVE_PROPERTY_1_EDITBOX: _curve_editor_curve_window_on_property_edited(descriptor, 0); break;
                            case IDC_CURVE_EDITOR_CURVE_PROPERTY_2_EDITBOX: _curve_editor_curve_window_on_property_edited(descriptor, 1); break;
                            case IDC_CURVE_EDITOR_CURVE_PROPERTY_3_EDITBOX: _curve_editor_curve_window_on_property_edited(descriptor, 2); break;
                            case IDC_CURVE_EDITOR_CURVE_PROPERTY_4_EDITBOX: _curve_editor_curve_window_on_property_edited(descriptor, 3); break;
                        }
                    }

                    break;
                }
            }

            break;
        }

        case WM_CUSTOMIZEDSHUTDOWN:
        {
            ::DestroyWindow(dialog_handle);

            break;
        }

        case WM_DESTROY:
        {
            ::PostQuitMessage(0);

            break;
        }

        case WM_INITDIALOG:
        {
            /* Set up the window instance, so that other messages can be handled */
            ::SetClassLongPtr (dialog_handle, GCLP_HICON,    (LONG_PTR) ::LoadIconA(0, IDI_APPLICATION) );
            ::SetWindowLongPtr(dialog_handle, GWLP_USERDATA, (LONG_PTR) lparam);

            /* Now that we're done, carry on and initialize the dialog using impl handler */
            _curve_editor_curve_window* descriptor = (_curve_editor_curve_window*) lparam;

            _curve_editor_curve_window_initialize_dialog(descriptor, dialog_handle);

            /* Set the 'dialog created' event so the caller's thread can continue */
            system_event_set(descriptor->dialog_created_event);

            return 1;
        }

        case WM_SIZE:
        {
            _curve_editor_curve_window* descriptor = (_curve_editor_curve_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);
            unsigned short              new_height = (unsigned short) HIWORD(lparam);
            unsigned short              new_width  = (unsigned short) LOWORD(lparam);
            
            _curve_editor_curve_window_on_resize(descriptor, new_width, new_height);

            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            /* Wheel events suck - we need to manually push them to renderer window */
            _curve_editor_curve_window* descriptor = (_curve_editor_curve_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);

            if (descriptor->renderer != NULL)
            {
                system_window        renderer_window        = curve_editor_curve_window_renderer_get_window(descriptor->renderer);
                system_window_handle renderer_window_handle = NULL;

                system_window_get_handle(renderer_window, &renderer_window_handle);

                ::SendMessageA(renderer_window_handle, WM_MOUSEWHEEL, wparam, lparam);
            }

            return 1;
        }
    }

    return 0;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_on_property_edited(_curve_editor_curve_window* descriptor_ptr, uint32_t n_property)
{
    /* Retrieve edit-box text */
    HWND editbox_handle = (n_property == 0 ? descriptor_ptr->window_handle_property_1_editbox :
                           n_property == 1 ? descriptor_ptr->window_handle_property_2_editbox :
                           n_property == 2 ? descriptor_ptr->window_handle_property_3_editbox :
                           n_property == 3 ? descriptor_ptr->window_handle_property_4_editbox : NULL);

    if (editbox_handle != NULL && descriptor_ptr->edited_segment != NULL)
    {
        int   text_length = ::GetWindowTextLengthA(editbox_handle);
        char* text        = NULL;

        if (text_length > 0)
        {
            text = new (std::nothrow) char[text_length+1];

            if (text != NULL)
            {
                memset(text, 0, text_length+1);

                ::GetWindowTextA(editbox_handle, text, text_length+1);
            }
        }

        /* Configure the property */
        curve_segment_type  edited_segment_type;
        system_variant_type edited_segment_variant_type;

        if (curve_segment_get_type                   (descriptor_ptr->edited_segment, &edited_segment_type) &&
            curve_segment_get_node_value_variant_type(descriptor_ptr->edited_segment, &edited_segment_variant_type) )
        {
            system_variant variant = system_variant_create(edited_segment_variant_type);

            system_variant_set_ansi_string(variant, text != NULL ? system_hashed_ansi_string_create(text) : system_hashed_ansi_string_get_default_empty_string(), true);

            switch (edited_segment_type)
            {
                case CURVE_SEGMENT_LERP:
                case CURVE_SEGMENT_STATIC:
                {
                    system_timeline_time node_time = -1;

                    if (curve_segment_get_node(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, &node_time, NULL) )
                    {
                        curve_segment_modify_node_time_value(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, node_time, variant, true);
                    }

                    break;
                }

                case CURVE_SEGMENT_TCB:
                {
                    if (n_property >= 0 && n_property <= 2)
                    {
                        switch (n_property)
                        {
                            case 0: curve_segment_modify_node_property(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, CURVE_SEGMENT_NODE_PROPERTY_BIAS,       variant); break;
                            case 1: curve_segment_modify_node_property(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY, variant); break;
                            case 2: curve_segment_modify_node_property(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, CURVE_SEGMENT_NODE_PROPERTY_TENSION,    variant); break;
                        }
                    }
                    else
                    {
                        system_timeline_time node_time = -1;

                        if (curve_segment_get_node(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, &node_time, NULL) )
                        {
                            curve_segment_modify_node_time_value(descriptor_ptr->edited_segment, descriptor_ptr->edited_segment_node_id, node_time, variant, true);
                        }
                    }

                    break;
                }
            } /* switch (edited_segment_type) */

            system_variant_release(variant);
        } /* if (curve_segment_get_type(descriptor_ptr->edited_segment, &edited_segment_type) ) */

        /* Release character buffer */
        if (text != NULL)
        {
            delete [] text;

            text = NULL;
        }

        /* Redraw */
        curve_editor_curve_window_renderer_redraw(descriptor_ptr->renderer);
    }
}

/** TODO */
PRIVATE void _curve_editor_curve_window_on_resize(_curve_editor_curve_window* descriptor, unsigned int new_width, unsigned int new_height)
{
    int x_size_frame = ::GetSystemMetrics(SM_CXSIZEFRAME);
    int y_border     = ::GetSystemMetrics(SM_CYBORDER);
    int y_size       = ::GetSystemMetrics(SM_CYSIZE);
    int y_size_frame = ::GetSystemMetrics(SM_CYSIZEFRAME);

    int dx =                     x_size_frame;
    int dy = y_border + y_size + y_size_frame;

    /* Help group-box should rescale horizontally, and move vertically as the window gets rescaled. */
    int y2 = new_height - (descriptor->ref_window_rect.bottom               - descriptor->ref_window_rect_help_groupbox.bottom - y_size_frame);
    int y1 = y2 -         (descriptor->ref_window_rect_help_groupbox.bottom - descriptor->ref_window_rect_help_groupbox.top);
    
    int x1 = descriptor->ref_window_rect_help_groupbox.left - descriptor->ref_window_rect.left - dx;
    int x2 = x1 + int(float(new_width) / float(descriptor->ref_client_width) * (descriptor->ref_window_rect_help_groupbox.right - descriptor->ref_window_rect_help_groupbox.left) );

    ::MoveWindow(descriptor->window_handle_help_groupbox, x1, y1, x2 - x1, y2 - y1, FALSE);

    int cached_help_groupbox_x2 = x2;
    int cached_help_groupbox_y1 = y1;

    /* View window should resize vertically and horizontally */
    x1 = descriptor->ref_window_rect_view.left - descriptor->ref_window_rect.left  - dx;
    x2 = new_width - (descriptor->ref_window_rect.right - descriptor->ref_window_rect_view.right) + x_size_frame;
    y2 = cached_help_groupbox_y1 - (descriptor->ref_window_rect_help_groupbox.top - descriptor->ref_window_rect_view.bottom);
    y1 = descriptor->ref_window_rect_view.top - descriptor->ref_window_rect.top - dy;

    //::MoveWindow(descriptor->window_handle_view, x1, y1, x2 - x1, y2 - y1, FALSE);

    int cached_renderer_x1y1x2y2[4] = {x1, y1, x2, y2};

    /* Help static should behave similarly */
    y2 = new_height - (descriptor->ref_window_rect.bottom             - descriptor->ref_window_rect_help_static.bottom - y_size_frame);
    y1 = y2         - (descriptor->ref_window_rect_help_static.bottom - descriptor->ref_window_rect_help_static.top);

    x1 = descriptor->ref_window_rect_help_static.left - descriptor->ref_window_rect.left - dx;
    x2 = x1 + int(float(new_width) / float(descriptor->ref_client_width) * (descriptor->ref_window_rect_help_static.right - descriptor->ref_window_rect_help_static.left) );

    ::MoveWindow(descriptor->window_handle_help_static, x1, y1, x2 - x1, y2 - y1, FALSE);

    /* Actions group-box  */
    y2 = new_height - (descriptor->ref_window_rect.bottom - descriptor->ref_window_rect_actions_groupbox.bottom - y_size_frame);
    y1 = y2         - (descriptor->ref_window_rect_actions_groupbox.bottom - descriptor->ref_window_rect_actions_groupbox.top);

    x2 = new_width               - (descriptor->ref_window_rect.right - descriptor->ref_window_rect_actions_groupbox.right - x_size_frame);
    x1 = cached_help_groupbox_x2 + (descriptor->ref_window_rect_actions_groupbox.left - descriptor->ref_window_rect_help_groupbox.right);

    ::MoveWindow(descriptor->window_handle_actions_groupbox, x1, y1, x2 - x1, y2 - y1, FALSE);

    int cached_actions_groupbox_x1 = x1;

    /* Update "property 1" / "property 2" / "property 3" static controls */
    y2 = new_height                 - (descriptor->ref_window_rect.bottom                   - descriptor->ref_window_rect_property_1_static.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_1_static.bottom - descriptor->ref_window_rect_property_1_static.top);
    x1 = cached_actions_groupbox_x1 + (descriptor->ref_window_rect_property_1_static.left   - descriptor->ref_window_rect_actions_groupbox.left);
    x2 = x1 +                         (descriptor->ref_window_rect_property_1_static.right  - descriptor->ref_window_rect_property_1_static.left);

    ::MoveWindow(descriptor->window_handle_property_1_static, x1, y1, x2 - x1, y2 - y1, FALSE);

    int cached_property1_static_x2 = x2;

    y2 = new_height                 - (descriptor->ref_window_rect.bottom                   - descriptor->ref_window_rect_property_2_static.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_2_static.bottom - descriptor->ref_window_rect_property_2_static.top);
    x1 = cached_actions_groupbox_x1 + (descriptor->ref_window_rect_property_1_static.left   - descriptor->ref_window_rect_actions_groupbox.left);
    x2 = x1 +                         (descriptor->ref_window_rect_property_1_static.right  - descriptor->ref_window_rect_property_1_static.left);

    ::MoveWindow(descriptor->window_handle_property_2_static, x1, y1, x2 - x1, y2 - y1, FALSE);

    int cached_property2_static_x2 = x2;

    y2 = new_height                 - (descriptor->ref_window_rect.bottom                   - descriptor->ref_window_rect_property_3_static.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_3_static.bottom - descriptor->ref_window_rect_property_3_static.top);
    x1 = cached_actions_groupbox_x1 + (descriptor->ref_window_rect_property_1_static.left   - descriptor->ref_window_rect_actions_groupbox.left);
    x2 = x1 +                         (descriptor->ref_window_rect_property_1_static.right  - descriptor->ref_window_rect_property_1_static.left);

    ::MoveWindow(descriptor->window_handle_property_3_static, x1, y1, x2 - x1, y2 - y1, FALSE);

    int cached_property3_static_x2 = x2;

    y2 = new_height                 - (descriptor->ref_window_rect.bottom                   - descriptor->ref_window_rect_property_4_static.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_4_static.bottom - descriptor->ref_window_rect_property_4_static.top);
    x1 = cached_actions_groupbox_x1 + (descriptor->ref_window_rect_property_1_static.left   - descriptor->ref_window_rect_actions_groupbox.left);
    x2 = x1 +                         (descriptor->ref_window_rect_property_1_static.right  - descriptor->ref_window_rect_property_1_static.left);

    ::MoveWindow(descriptor->window_handle_property_4_static, x1, y1, x2 - x1, y2 - y1, FALSE);

    /* Followed by editboxes */
    y2 = new_height                 - (descriptor->ref_window_rect.bottom                    - descriptor->ref_window_rect_property_1_editbox.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_1_editbox.bottom - descriptor->ref_window_rect_property_1_editbox.top);
    x2 = new_width                  - (descriptor->ref_window_rect.right                     - descriptor->ref_window_rect_property_1_editbox.right - x_size_frame);
    x1 = cached_property1_static_x2 + (descriptor->ref_window_rect_property_1_editbox.left   - descriptor->ref_window_rect_property_1_static.right);

    ::MoveWindow(descriptor->window_handle_property_1_editbox, x1, y1, x2 - x1, y2 - y1, FALSE);

    y2 = new_height                 - (descriptor->ref_window_rect.bottom                    - descriptor->ref_window_rect_property_2_editbox.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_2_editbox.bottom - descriptor->ref_window_rect_property_2_editbox.top);
    x2 = new_width                  - (descriptor->ref_window_rect.right                     - descriptor->ref_window_rect_property_2_editbox.right - x_size_frame);
    x1 = cached_property2_static_x2 + (descriptor->ref_window_rect_property_2_editbox.left   - descriptor->ref_window_rect_property_2_static.right);

    ::MoveWindow(descriptor->window_handle_property_2_editbox, x1, y1, x2 - x1, y2 - y1, FALSE);

    y2 = new_height                 - (descriptor->ref_window_rect.bottom                    - descriptor->ref_window_rect_property_3_editbox.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_3_editbox.bottom - descriptor->ref_window_rect_property_3_editbox.top);
    x2 = new_width                  - (descriptor->ref_window_rect.right                     - descriptor->ref_window_rect_property_3_editbox.right - x_size_frame);
    x1 = cached_property1_static_x2 + (descriptor->ref_window_rect_property_3_editbox.left   - descriptor->ref_window_rect_property_3_static.right);

    ::MoveWindow(descriptor->window_handle_property_3_editbox, x1, y1, x2 - x1, y2 - y1, FALSE);

    y2 = new_height                 - (descriptor->ref_window_rect.bottom                    - descriptor->ref_window_rect_property_4_editbox.bottom - y_size_frame);
    y1 = y2                         - (descriptor->ref_window_rect_property_4_editbox.bottom - descriptor->ref_window_rect_property_4_editbox.top);
    x2 = new_width                  - (descriptor->ref_window_rect.right                     - descriptor->ref_window_rect_property_4_editbox.right - x_size_frame);
    x1 = cached_property1_static_x2 + (descriptor->ref_window_rect_property_4_editbox.left   - descriptor->ref_window_rect_property_4_static.right);

    ::MoveWindow(descriptor->window_handle_property_4_editbox, x1, y1, x2 - x1, y2 - y1, FALSE);

    /* Issue window redraw */
    ::RedrawWindow(descriptor->window_handle, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ERASENOW);

    /* Redraw the renderer view */
    curve_editor_curve_window_renderer_resize(descriptor->renderer, cached_renderer_x1y1x2y2);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_dialog_thread_entrypoint(void* descriptor)
{
    _curve_editor_curve_window* descriptor_ptr = (_curve_editor_curve_window*) descriptor;

    ::DialogBoxParamA(_global_instance, MAKEINTRESOURCE(IDD_CURVE_EDITOR_CURVE), HWND_DESKTOP, _curve_editor_curve_window_dialog_message_handler, (LPARAM) descriptor_ptr);
}

/** TODO */
PRIVATE volatile void _curve_editor_curve_window_initialize_renderer(__in __notnull void* descriptor)
{
    _curve_editor_curve_window* descriptor_ptr = (_curve_editor_curve_window*) descriptor;

    /* Curve window creation needs to be serialized */
    system_critical_section_enter(descriptor_ptr->serialization_cs);
    {
        descriptor_ptr->renderer = curve_editor_curve_window_renderer_create(descriptor_ptr->name,
                                                                             descriptor_ptr->window_handle_view,
                                                                             descriptor_ptr->context,
                                                                             descriptor_ptr->curve,
                                                                             (curve_editor_curve_window) descriptor_ptr);
        ASSERT_DEBUG_SYNC(descriptor_ptr->renderer != NULL, "Could not create renderer");
    }
    system_critical_section_leave(descriptor_ptr->serialization_cs);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_initialize_dialog(__in __notnull _curve_editor_curve_window* descriptor, HWND dialog_handle)
{
    descriptor->edited_segment                     = NULL;
    descriptor->edited_segment_node_id             = -1;
    descriptor->should_ignore_change_notifications = false;
    descriptor->window_handle                      = dialog_handle;
    descriptor->window_handle_actions_groupbox     = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_ACTIONS_GROUPBOX);
    descriptor->window_handle_help_groupbox        = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_HELP_GROUPBOX);
    descriptor->window_handle_help_static          = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_HELP_STATIC);
    descriptor->window_handle_property_1_editbox   = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_1_EDITBOX);
    descriptor->window_handle_property_1_static    = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_1_STATIC);
    descriptor->window_handle_property_2_editbox   = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_2_EDITBOX);
    descriptor->window_handle_property_2_static    = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_2_STATIC);
    descriptor->window_handle_property_3_editbox   = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_3_EDITBOX);
    descriptor->window_handle_property_3_static    = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_3_STATIC);
    descriptor->window_handle_property_4_editbox   = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_4_EDITBOX);
    descriptor->window_handle_property_4_static    = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_PROPERTY_4_STATIC);
    descriptor->window_handle_view                 = ::GetDlgItem(dialog_handle, IDC_CURVE_EDITOR_CURVE_VIEW);

    ASSERT_DEBUG_SYNC(descriptor->window_handle_actions_groupbox   != NULL &&
                      descriptor->window_handle_help_groupbox      != NULL &&
                      descriptor->window_handle_help_static        != NULL &&
                      descriptor->window_handle_property_1_editbox != NULL &&
                      descriptor->window_handle_property_1_static  != NULL &&
                      descriptor->window_handle_property_2_editbox != NULL &&
                      descriptor->window_handle_property_2_static  != NULL &&
                      descriptor->window_handle_property_3_editbox != NULL &&
                      descriptor->window_handle_property_3_static  != NULL &&
                      descriptor->window_handle_property_4_editbox != NULL &&
                      descriptor->window_handle_property_4_static  != NULL &&
                      descriptor->window_handle_view               != NULL &&
                      descriptor->window_handle                    != NULL,
                      "Could not retrieve all window handles.");

    /* Retrieve client & window rect for the window */
    RECT rect;

    ::GetClientRect(descriptor->window_handle, &rect);
    ::GetWindowRect(descriptor->window_handle, &descriptor->ref_window_rect);

    descriptor->ref_client_height = rect.bottom;
    descriptor->ref_client_width  = rect.right;

    /* Retrieve client rects for other controls */
    ::GetWindowRect(descriptor->window_handle_actions_groupbox,   &descriptor->ref_window_rect_actions_groupbox);
    ::GetWindowRect(descriptor->window_handle_help_groupbox,      &descriptor->ref_window_rect_help_groupbox);
    ::GetWindowRect(descriptor->window_handle_help_static,        &descriptor->ref_window_rect_help_static);
    ::GetWindowRect(descriptor->window_handle_property_1_editbox, &descriptor->ref_window_rect_property_1_editbox);
    ::GetWindowRect(descriptor->window_handle_property_1_static,  &descriptor->ref_window_rect_property_1_static);
    ::GetWindowRect(descriptor->window_handle_property_2_editbox, &descriptor->ref_window_rect_property_2_editbox);
    ::GetWindowRect(descriptor->window_handle_property_2_static,  &descriptor->ref_window_rect_property_2_static);
    ::GetWindowRect(descriptor->window_handle_property_3_editbox, &descriptor->ref_window_rect_property_3_editbox);
    ::GetWindowRect(descriptor->window_handle_property_3_static,  &descriptor->ref_window_rect_property_3_static);
    ::GetWindowRect(descriptor->window_handle_property_4_editbox, &descriptor->ref_window_rect_property_4_editbox);
    ::GetWindowRect(descriptor->window_handle_property_4_static,  &descriptor->ref_window_rect_property_4_static);
    ::GetWindowRect(descriptor->window_handle_view,               &descriptor->ref_window_rect_view);

    /* Hide value property static/editboxes - no node is selected on start-up */
    curve_editor_select_node( (curve_editor_curve_window) descriptor, -1, -1);

    /* Renderer must be initialized from a separate thread */
    system_thread_pool_task_descriptor renderer_initialization_task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL, _curve_editor_curve_window_initialize_renderer, descriptor);

    ASSERT_DEBUG_SYNC(renderer_initialization_task != NULL, "Could not create renderer initialization task.");
    if (renderer_initialization_task != NULL)
    {
        system_thread_pool_submit_single_task(renderer_initialization_task);
    }    
}

/** TODO */
PRIVATE void _curve_editor_curve_window_init_curve_editor_curve_window(_curve_editor_curve_window*                descriptor,
                                                                       curve_container                            curve,
                                                                       PFNONCURVEWINDOWRELEASECALLBACKHANDLERPROC release_callback_handler,
                                                                       void*                                      release_callback_handler_arg,
                                                                       ogl_context                                context,
                                                                       system_hashed_ansi_string                  name,
                                                                       system_critical_section                    serialization_cs)
{
    descriptor->context                          = context;
    descriptor->curve                            = curve;
    descriptor->dialog_created_event             = system_event_create(true, false);
    descriptor->dialog_thread_event              = NULL;
    descriptor->name                             = name;
    descriptor->ref_client_height                = 0;
    descriptor->ref_client_width                 = 0;
    descriptor->release_callback_handler         = release_callback_handler;
    descriptor->release_callback_handler_arg     = release_callback_handler_arg;
    descriptor->renderer                         = NULL;
    descriptor->serialization_cs                 = serialization_cs;
    descriptor->window_handle_actions_groupbox   = NULL;
    descriptor->window_handle_help_groupbox      = NULL;
    descriptor->window_handle_help_static        = NULL;
    descriptor->window_handle_property_1_editbox = NULL;
    descriptor->window_handle_property_1_static  = NULL;
    descriptor->window_handle_property_2_editbox = NULL;
    descriptor->window_handle_property_2_static  = NULL;
    descriptor->window_handle_property_3_editbox = NULL;
    descriptor->window_handle_property_3_static  = NULL;
    descriptor->window_handle_view               = NULL;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_release(_curve_editor_curve_window* descriptor)
{
    /* Shut down renderer */
    if (descriptor->renderer != NULL)
    {
        curve_editor_curve_window_renderer_release(descriptor->renderer);

        descriptor->renderer = NULL;
    }

    /* Close the window in the first place and wait till the window thread dies down */
    ::SendMessage(descriptor->window_handle, WM_CUSTOMIZEDSHUTDOWN, 0, 0);

    system_event_wait_single_infinite(descriptor->dialog_thread_event);

    /* Proceed with releasing */
    if (descriptor->dialog_created_event != NULL)
    {
        system_event_release(descriptor->dialog_created_event);

        descriptor->dialog_created_event = NULL;
    }

    if (descriptor->dialog_thread_event != NULL)
    {
        system_event_release(descriptor->dialog_thread_event);

        descriptor->dialog_thread_event = NULL;
    }

    /* Call back if asked */
    if (descriptor->release_callback_handler != NULL)
    {
        descriptor->release_callback_handler(descriptor->release_callback_handler_arg,
                                             descriptor->curve);
    }

    /* Release the descriptor */
    delete descriptor;
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_create_static_segment_handler(void* arg)
{
    system_variant_type                                        curve_type     = SYSTEM_VARIANT_UNDEFINED;
    _curve_window_renderer_segment_creation_callback_argument* data           = (_curve_window_renderer_segment_creation_callback_argument*) arg;
    curve_segment_id                                           new_segment_id = 0;
    system_variant                                             value_variant  = NULL;

    curve_container_get_property(data->curve,
                                 CURVE_CONTAINER_PROPERTY_DATA_TYPE,
                                &curve_type);

    value_variant = system_variant_create(curve_type);

    curve_container_get_default_value(data->curve,
                                      false,
                                      value_variant);

    if (!curve_container_add_static_value_segment(data->curve,
                                                  data->start_time,
                                                  data->end_time,
                                                  value_variant,
                                                  &new_segment_id) )
    {
        LOG_ERROR("Could not add static value segment!");
    }
    else
    {
        curve_editor_curve_window_renderer_redraw(data->renderer);
    }

    system_variant_release(value_variant);
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_create_lerp_segment_handler(void* arg)
{
    system_variant_type                                        curve_data_type      = SYSTEM_VARIANT_UNDEFINED;
    _curve_window_renderer_segment_creation_callback_argument* data                 = (_curve_window_renderer_segment_creation_callback_argument*) arg;
    curve_segment_id                                           new_segment_id       = 0;
    float                                                      start_value;
    system_variant                                             start_value_variant  = NULL;
    system_variant                                             end_value_variant    = NULL;

    curve_container_get_property(data->curve,
                                 CURVE_CONTAINER_PROPERTY_DATA_TYPE,
                                &curve_data_type);

    start_value_variant = system_variant_create(curve_data_type);
    end_value_variant   = system_variant_create(curve_data_type);

    curve_container_get_default_value(data->curve,
                                      false,
                                      start_value_variant);
    system_variant_get_float         (start_value_variant,
                                     &start_value);
    system_variant_set_float_forced  (end_value_variant,
                                      start_value + 1.0f);

    if (!curve_container_add_lerp_segment(data->curve,
                                          data->start_time,
                                          data->end_time,
                                          start_value_variant,
                                          end_value_variant,
                                          &new_segment_id) )
    {
        LOG_ERROR("Could not add lerp value segment!");
    }
    else
    {
        curve_editor_curve_window_renderer_redraw(data->renderer);
    }

    system_variant_release(start_value_variant);
    system_variant_release(end_value_variant);
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_create_tcb_segment_handler(void* arg)
{
    system_variant_type                                        curve_data_type     = SYSTEM_VARIANT_UNDEFINED;
    _curve_window_renderer_segment_creation_callback_argument* data                = (_curve_window_renderer_segment_creation_callback_argument*) arg;
    curve_segment_id                                           new_segment_id      = 0;
    system_variant                                             start_value_variant = NULL;
    system_variant                                             end_value_variant   = NULL;

    curve_container_get_property(data->curve,
                                 CURVE_CONTAINER_PROPERTY_DATA_TYPE,
                                &curve_data_type);

    start_value_variant = system_variant_create(curve_data_type);
    end_value_variant   = system_variant_create(curve_data_type);

    curve_container_get_default_value(data->curve,
                                      false,
                                      start_value_variant);
    system_variant_set_float_forced  (end_value_variant,
                                      1.0f);

    if (!curve_container_add_tcb_segment(data->curve,
                                         data->start_time,
                                         data->end_time,
                                         start_value_variant,
                                         1.0f,
                                         1.0f,
                                         1.0f,
                                         end_value_variant,
                                         1.0f,
                                         1.0f,
                                         1.0f,
                                         &new_segment_id) )
    {
        LOG_ERROR("Could not add tcb value segment!");
    }
    else
    {
        curve_editor_curve_window_renderer_redraw(data->renderer);
    }

    system_variant_release(start_value_variant);
    system_variant_release(end_value_variant);
}

/* Please see header for specification */
PUBLIC curve_editor_curve_window curve_editor_curve_window_show(ogl_context                                context,
                                                                curve_container                            curve,
                                                                PFNONCURVEWINDOWRELEASECALLBACKHANDLERPROC release_callback_handler,
                                                                void*                                      owner,
                                                                system_critical_section                    serialization_cs)
{
    /** Allocate space for the object */
    _curve_editor_curve_window* result = new (std::nothrow) _curve_editor_curve_window;

    ASSERT_DEBUG_SYNC(result != NULL, "Out of memory while allocating space for _curve_editor_curve_window instance.");
    if (result != NULL)
    {
        /* Initialize the descriptor with default values */
        system_hashed_ansi_string curve_name = NULL;

        curve_container_get_property(curve,
                                     CURVE_CONTAINER_PROPERTY_NAME,
                                    &curve_name);

        _curve_editor_curve_window_init_curve_editor_curve_window(result,
                                                                  curve,
                                                                  release_callback_handler,
                                                                  owner,
                                                                  context,
                                                                  curve_name,
                                                                  serialization_cs);

        /* Spawn a new thread for the dialog */
        system_threads_spawn(_curve_editor_curve_window_dialog_thread_entrypoint, result, &result->dialog_thread_event);
    }

    return (curve_editor_curve_window) result;
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_hide(curve_editor_curve_window curve_window)
{
    _curve_editor_curve_window* curve_window_ptr = (_curve_editor_curve_window*) curve_window;

    _curve_editor_curve_window_release(curve_window_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API void curve_editor_curve_window_redraw(curve_editor_curve_window curve_window)
{
    _curve_editor_curve_window* curve_window_ptr = (_curve_editor_curve_window*) curve_window;

    if (curve_window_ptr->renderer != NULL)
    {
        curve_editor_curve_window_renderer_redraw(curve_window_ptr->renderer);
    }
}

/** TODO */
PRIVATE void _curve_editor_use_variant_for_editbox(system_variant variant, HWND editbox_handle)
{
    system_hashed_ansi_string value_has;
    
    system_variant_get_ansi_string(variant,        true, &value_has);    
    ::SetWindowTextA              (editbox_handle, system_hashed_ansi_string_get_buffer(value_has) );
}

/** TODO */
PRIVATE void _curve_editor_select_lerp_node(_curve_editor_curve_window* curve_window_ptr, curve_segment segment, curve_segment_node_id node_id)
{
    ::SetWindowTextA(curve_window_ptr->window_handle_property_1_static, "1st value:");
    ::SetWindowTextA(curve_window_ptr->window_handle_property_2_static, "2nd value:");

    ::ShowWindow(curve_window_ptr->window_handle_property_1_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_1_static,  SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_2_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_2_static,  SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_3_editbox, SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_3_static,  SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_4_editbox, SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_4_static,  SW_HIDE);

    /* Set the editboxes */
    system_variant       end_data_variant   = NULL;
    system_variant       start_data_variant = NULL;
    system_variant_type  data_variant_type  = (system_variant_type) -1;
    system_timeline_time node_time          = -1;

    if (curve_segment_get_node_value_variant_type(segment, &data_variant_type) )
    {
        start_data_variant = system_variant_create(data_variant_type);
        end_data_variant   = system_variant_create(data_variant_type);

        curve_window_ptr->should_ignore_change_notifications = true;
        {
            if (curve_segment_get_node(segment, 0, &node_time, start_data_variant) &&
                curve_segment_get_node(segment, 1, &node_time, end_data_variant) )
            {
                _curve_editor_use_variant_for_editbox(start_data_variant, curve_window_ptr->window_handle_property_1_editbox);
                _curve_editor_use_variant_for_editbox(end_data_variant,   curve_window_ptr->window_handle_property_2_editbox);
            }
        }
        curve_window_ptr->should_ignore_change_notifications = false;

        system_variant_release(end_data_variant);
        system_variant_release(start_data_variant);
    }
}

/** TODO */
PRIVATE void _curve_editor_select_static_node(_curve_editor_curve_window* curve_window_ptr, curve_segment segment, curve_segment_node_id node_id)
{
    ::SetWindowTextA(curve_window_ptr->window_handle_property_1_static, "Value:");

    ::ShowWindow(curve_window_ptr->window_handle_property_1_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_1_static,  SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_2_editbox, SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_2_static,  SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_3_editbox, SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_3_static,  SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_4_editbox, SW_HIDE);
    ::ShowWindow(curve_window_ptr->window_handle_property_4_static,  SW_HIDE);

    /* Set the editboxes */
    system_variant       data_variant      = NULL;
    system_variant_type  data_variant_type = (system_variant_type) -1;
    system_timeline_time node_time         = -1;

    if (curve_segment_get_node_value_variant_type(segment, &data_variant_type) )
    {
        data_variant = system_variant_create(data_variant_type);

        if (curve_segment_get_node(segment, 0, &node_time, data_variant) )
        {
            curve_window_ptr->should_ignore_change_notifications = true;
            {
                _curve_editor_use_variant_for_editbox(data_variant, curve_window_ptr->window_handle_property_1_editbox);
            }
            curve_window_ptr->should_ignore_change_notifications = false;
        }

        system_variant_release(data_variant);
    }
}

/** TODO */
PRIVATE void _curve_editor_select_tcb_node(_curve_editor_curve_window* curve_window_ptr, curve_segment segment, curve_segment_node_id node_id)
{
    ::SetWindowTextA(curve_window_ptr->window_handle_property_1_static, "Bias:");
    ::SetWindowTextA(curve_window_ptr->window_handle_property_2_static, "Continuity:");
    ::SetWindowTextA(curve_window_ptr->window_handle_property_3_static, "Tension:");
    ::SetWindowTextA(curve_window_ptr->window_handle_property_4_static, "Value:");

    ::ShowWindow(curve_window_ptr->window_handle_property_1_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_1_static,  SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_2_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_2_static,  SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_3_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_3_static,  SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_4_editbox, SW_SHOW);
    ::ShowWindow(curve_window_ptr->window_handle_property_4_static,  SW_SHOW);

    /* Set the editboxes */
    system_variant       bias_variant       = system_variant_create(SYSTEM_VARIANT_FLOAT);
    system_variant       continuity_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);
    system_variant       data_variant       = NULL;
    system_variant_type  data_variant_type  = (system_variant_type) -1;
    system_timeline_time node_time          = -1;
    system_variant       tension_variant    = system_variant_create(SYSTEM_VARIANT_FLOAT);

    if (curve_segment_get_node_value_variant_type(segment, &data_variant_type) )
    {
        data_variant = system_variant_create(data_variant_type);

        if (curve_segment_get_node         (segment, node_id, &node_time,                             data_variant)       &&
            curve_segment_get_node_property(segment, node_id, CURVE_SEGMENT_NODE_PROPERTY_BIAS,       bias_variant)       &&
            curve_segment_get_node_property(segment, node_id, CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY, continuity_variant) &&
            curve_segment_get_node_property(segment, node_id, CURVE_SEGMENT_NODE_PROPERTY_TENSION,    tension_variant) )
        {
            curve_window_ptr->should_ignore_change_notifications = true;
            {
                _curve_editor_use_variant_for_editbox(bias_variant,       curve_window_ptr->window_handle_property_1_editbox);
                _curve_editor_use_variant_for_editbox(continuity_variant, curve_window_ptr->window_handle_property_2_editbox);
                _curve_editor_use_variant_for_editbox(tension_variant,    curve_window_ptr->window_handle_property_3_editbox);
                _curve_editor_use_variant_for_editbox(data_variant,       curve_window_ptr->window_handle_property_4_editbox);
            }
            curve_window_ptr->should_ignore_change_notifications = false;
        }

        system_variant_release(bias_variant);
        system_variant_release(continuity_variant);
        system_variant_release(data_variant);
        system_variant_release(tension_variant);
    }
}

/* Please see header for specification */
PUBLIC void curve_editor_select_node(curve_editor_curve_window curve_window, curve_segment_id segment_id, curve_segment_node_id node_id)
{
    _curve_editor_curve_window* curve_window_ptr = (_curve_editor_curve_window*) curve_window;

    if (segment_id == -1 || node_id == -1)
    {
        ::ShowWindow(curve_window_ptr->window_handle_property_1_editbox, SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_1_static,  SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_2_editbox, SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_2_static,  SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_3_editbox, SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_3_static,  SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_4_editbox, SW_HIDE);
        ::ShowWindow(curve_window_ptr->window_handle_property_4_static,  SW_HIDE);
    }
    else
    {
        curve_segment      selected_segment = curve_container_get_segment(curve_window_ptr->curve,
                                                                          segment_id);
        curve_segment_type selected_segment_type;

        if (selected_segment != NULL && curve_segment_get_type(selected_segment, &selected_segment_type) )
        {
            switch (selected_segment_type)
            {
                case CURVE_SEGMENT_STATIC: _curve_editor_select_static_node(curve_window_ptr, selected_segment, node_id); break;
                case CURVE_SEGMENT_LERP:   _curve_editor_select_lerp_node  (curve_window_ptr, selected_segment, node_id); break;
                case CURVE_SEGMENT_TCB:    _curve_editor_select_tcb_node   (curve_window_ptr, selected_segment, node_id); break;

                default: ASSERT_DEBUG_SYNC(false, "Unknown segment type [%d]", selected_segment_type);
            }

            /* Store handles in case user alters any of the editboxes */
            curve_window_ptr->edited_segment         = selected_segment;
            curve_window_ptr->edited_segment_node_id = node_id;
        }
    }
}

/** Please see header for specification */
PUBLIC void curve_editor_curve_window_set_property(__in __notnull curve_editor_curve_window          window,
                                                   __in           curve_editor_curve_window_property property,
                                                   __in __notnull void*                              data)
{
    _curve_editor_curve_window* window_ptr = (_curve_editor_curve_window*) window;

    switch (property)
    {
        case CURVE_EDITOR_CURVE_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH:
        {
            curve_editor_curve_window_renderer_set_property(window_ptr->renderer,
                                                            CURVE_EDITOR_CURVE_WINDOW_RENDERER_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH,
                                                            data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_curve_window_property value");
        }
    } /* switch (property) */
}
