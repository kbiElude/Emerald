/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "main.h"
#include <CommCtrl.h>

#include "../resource.h"
#include "curve/curve_container.h"
#include "curve_editor/curve_editor_curve_window.h"
#include "object_manager/object_manager_directory.h"
#include "object_manager/object_manager_general.h"
#include "object_manager/object_manager_item.h"
#include "ogl/ogl_context.h"
#include "sh_light_editor/sh_light_editor_main_window.h"
#include "sh_light_editor/sh_light_editor_types.h"
#include "sh/sh_projector.h"
#include "sh/sh_projector_combiner.h"
#include "sh/sh_tools.h"
#include "sh/sh_types.h"
#include "system/system_assertions.h"
#include "system/system_context_menu.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_threads.h"
#include "system/system_thread_pool.h"
#include "system/system_variant.h"

#define DEFAULT_STORAGE_CAPACITY (1)
#define N_PROPERTIES             (8)
#define WM_CUSTOMIZED_SHUT_DOWN  (WM_USER + 1)

/* Private definitions */
typedef void  (*PFNEDITBUTTONCALLBACKPROC)(sh_light_editor_main_window, void*);
typedef void* _sh_light_editor_main_window_edit_button_callback_arg;

typedef struct
{
    sh_light_editor_main_window main_window;
    sh_projector_combiner       projection_combiner;
    sh_projection_type          projection_requested;
} _sh_light_editor_main_window_context_menu_add_projection_argument;

typedef struct
{
    sh_light_editor_main_window main_window;
    sh_projector_combiner       projection_combiner;
    sh_projection_id            projection_id;
} _sh_light_editor_main_window_context_menu_delete_projection_argument;

typedef struct
{
    PFNEDITBUTTONCALLBACKPROC pfn_x_callback;
    PFNEDITBUTTONCALLBACKPROC pfn_y_callback;
    PFNEDITBUTTONCALLBACKPROC pfn_z_callback;
    PFNEDITBUTTONCALLBACKPROC pfn_w_callback;
    
    _sh_light_editor_main_window_edit_button_callback_arg x_callback_arg;
    _sh_light_editor_main_window_edit_button_callback_arg y_callback_arg;
    _sh_light_editor_main_window_edit_button_callback_arg z_callback_arg;
    _sh_light_editor_main_window_edit_button_callback_arg w_callback_arg;
    
    HWND x;
    HWND y;
    HWND z;
    HWND w;
} _sh_light_editor_main_window_edit_button;

typedef struct
{
    ogl_context context;
    HWND        window_handle;
    
    HWND                                     sh_light_projections_treeview_handle;
    HWND                                     property_name_static_handles[N_PROPERTIES];
    _sh_light_editor_main_window_edit_button property_edit_button_handles[N_PROPERTIES];
    sh_projector_property                    property_type               [N_PROPERTIES]; /* set to SH_PROJECTION_PROPERTY_LAST if no property is available */

    system_resizable_vector opened_curve_editor_window_descriptors; /* stores _sh_light_editor_main_window_curve_editor_window*, only access from within serialization_cs CS */
    system_resizable_vector projector_combiner_descriptors;         /* stores _sh_light_editor_main_window_projector_combiner* */

    system_event            dialog_created_event;
    system_event            dialog_thread_event;
    system_critical_section serialization_cs;

    sh_projector_combiner projector_combiner_selected;
    sh_projection_id      projector_combiner_selected_projection_id;

    PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func;

    bool should_handle_en_change_notifications;
} _sh_light_editor_main_window;

typedef struct
{
    curve_container           curve;
    curve_editor_curve_window curve_editor_window;
    uint32_t                  n_dimension;
} _sh_light_editor_main_window_curve_editor_window;

typedef struct
{
    sh_projector_combiner instance;
    HTREEITEM             root_node_handle;

    system_resizable_vector projector_descriptors; /* stores _sh_light_editor_main_window_projector* */
} _sh_light_editor_main_window_projector_combiner;

typedef struct
{
    sh_projector_combiner owner;
    sh_projection_id      projection_id;
    sh_projection_type    projection_type;
    HTREEITEM             node_handle;
} _sh_light_editor_main_window_projector;

/* Private variables */

/* Forward declarations */
PRIVATE void          _curve_editor_dialog_thread_entrypoint                                             (                 void*                                             descriptor);
PRIVATE volatile void _sh_light_editor_dialog_close_button_handler                                       (                 void*                                             descriptor);
PRIVATE BOOL CALLBACK _sh_light_editor_dialog_window_message_handler                                     (                 HWND                                              dialog_handle, UINT message_id, WPARAM wparam, LPARAM lparam);
PRIVATE HWND          _sh_light_editor_main_window_get_xyzw_edit_button_window_handle_for_curve_container(__in __notnull   _sh_light_editor_main_window*                     descriptor, __in __notnull curve_container curve, __in uint32_t n_dimension);
PRIVATE void          _sh_light_editor_handle_on_node_selected_notification                              (__in __notnull   _sh_light_editor_main_window*                     descriptor, __in HTREEITEM  selected_node_handle);
PRIVATE void          _sh_light_editor_handle_on_treeview_right_click_notification                       (__in __notnull   _sh_light_editor_main_window*                     descriptor);
PRIVATE void          _sh_light_editor_init_descriptor                                                   (                 _sh_light_editor_main_window*                     descriptor, PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func, ogl_context context);
PRIVATE void          _sh_light_editor_main_window_deinit_curve_editor_window                            (__in __notnull   _sh_light_editor_main_window_curve_editor_window* descriptor_ptr);
PRIVATE void          _sh_light_editor_main_window_deinit_projector                                      (__in __notnull   _sh_light_editor_main_window_projector*           descriptor_ptr);
PRIVATE void          _sh_light_editor_main_window_deinit_projector_combiner                             (__in __notnull   _sh_light_editor_main_window_projector_combiner*  descriptor_ptr);
PRIVATE void          _sh_light_editor_main_window_initialize_dialog                                     (                 _sh_light_editor_main_window*                     descriptor, HWND dialog_handle);
PRIVATE void          _sh_light_editor_main_window_init_curve_editor_window                              (__in __notnull   _sh_light_editor_main_window_curve_editor_window* descriptor_ptr, __in __notnull curve_container curve, __in __notnull curve_editor_curve_window curve_window, __in uint32_t n_dimension);
PRIVATE void          _sh_light_editor_main_window_init_projector                                        (__in __notnull   _sh_light_editor_main_window_projector*           child_descriptor_ptr, __in __notnull sh_projector_combiner combiner_instance, __in sh_projection_id projection_id, __in sh_projection_type projection_type, __in HTREEITEM node_handle);
PRIVATE void          _sh_light_editor_main_window_init_projector_combiner                               (__in __notnull   _sh_light_editor_main_window_projector_combiner*  descriptor_ptr, __in __notnull sh_projector_combiner combiner_instance, __in HTREEITEM node_handle, __in __notnull sh_light_editor_main_window main_window);
PRIVATE void          _sh_light_editor_main_window_projector_combiner_callback_projection_added          (__in __notnull   void* arg);
PRIVATE void          _sh_light_editor_main_window_release                                               (__in __notnull   _sh_light_editor_main_window*                     main_window_ptr, bool do_full_release);
PRIVATE void          _sh_light_editor_main_window_reset_projections_list                                (                 _sh_light_editor_main_window*                     descriptor);
PRIVATE void          _sh_light_editor_main_window_ui_callback_add_sh_projection                         (__in __notnull   void* arg);
PRIVATE void          _sh_light_editor_main_window_ui_callback_curve_editor_window_closed                (void*, curve_container, uint8_t);
PRIVATE void          _sh_light_editor_main_window_ui_callback_delete_sh_projection                      (__in __notnull   void* arg);
PRIVATE void          _sh_light_editor_main_window_ui_callback_edit_curve_container                      (__in __notnull    sh_light_editor_main_window                      main_window, __in __maybenull void* arg, __in uint32_t dimension_index);
PRIVATE void          _sh_light_editor_main_window_ui_callback_edit_curve_container_1                    (__in __notnull    sh_light_editor_main_window                      main_window, __in __maybenull void* arg);
PRIVATE void          _sh_light_editor_main_window_ui_callback_edit_curve_container_2                    (__in __notnull    sh_light_editor_main_window                      main_window, __in __maybenull void* arg);
PRIVATE void          _sh_light_editor_main_window_ui_callback_edit_curve_container_3                    (__in __notnull    sh_light_editor_main_window                      main_window, __in __maybenull void* arg);
PRIVATE void          _sh_light_editor_main_window_ui_callback_edit_curve_container_4                    (__in __notnull    sh_light_editor_main_window                      main_window, __in __maybenull void* arg);
PRIVATE void          _sh_light_editor_select_projector_combiner                                         (__in __notnull   _sh_light_editor_main_window*                     descriptor_ptr, __in __maybenull sh_projector_combiner first_combiner);

/** TODO */
PRIVATE void _curve_editor_dialog_thread_entrypoint(void* descriptor)
{
    _sh_light_editor_main_window* descriptor_ptr = (_sh_light_editor_main_window*) descriptor;

    ::DialogBoxParamA(_global_instance, MAKEINTRESOURCE(IDD_SH_LIGHT_EDITOR_MAIN), 0, _sh_light_editor_dialog_window_message_handler, (LPARAM) descriptor_ptr);
}

/** TODO */
PRIVATE volatile void _sh_light_editor_dialog_close_button_handler(void* descriptor)
{
    _sh_light_editor_main_window* descriptor_ptr = (_sh_light_editor_main_window*) descriptor;

    sh_light_editor_main_window_release( (sh_light_editor_main_window) descriptor);
}

/** TODO */
PRIVATE BOOL CALLBACK _sh_light_editor_dialog_window_message_handler(HWND dialog_handle, UINT message_id, WPARAM wparam, LPARAM lparam)
{
    switch (message_id)
    {
        case WM_COMMAND:
        {
            _sh_light_editor_main_window* descriptor = (_sh_light_editor_main_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);

            if (HIWORD(wparam) == BN_CLICKED)
            {
                /* Maybe X/Y/Z/W button was pushed? */
                for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property)
                {
                    if (descriptor->property_edit_button_handles[n_property].x == (HWND) lparam)
                    {
                        descriptor->property_edit_button_handles[n_property].pfn_x_callback((sh_light_editor_main_window) descriptor,
                                                                                            descriptor->property_edit_button_handles[n_property].x_callback_arg);
                    }
                    else
                    if (descriptor->property_edit_button_handles[n_property].y == (HWND) lparam)
                    {
                        descriptor->property_edit_button_handles[n_property].pfn_y_callback((sh_light_editor_main_window) descriptor,
                                                                                            descriptor->property_edit_button_handles[n_property].y_callback_arg);
                    }
                    else
                    if (descriptor->property_edit_button_handles[n_property].z == (HWND) lparam)
                    {
                        descriptor->property_edit_button_handles[n_property].pfn_z_callback((sh_light_editor_main_window) descriptor,
                                                                                            descriptor->property_edit_button_handles[n_property].z_callback_arg);
                    }
                    else
                    if (descriptor->property_edit_button_handles[n_property].w == (HWND) lparam)
                    {
                        descriptor->property_edit_button_handles[n_property].pfn_w_callback((sh_light_editor_main_window) descriptor,
                                                                                            descriptor->property_edit_button_handles[n_property].w_callback_arg);
                    }
                }
            }

            break;
        }

        case WM_CUSTOMIZED_SHUT_DOWN:
        {
            ::DestroyWindow(dialog_handle);

            break;
        }

        case WM_CLOSE:
        {
            /* In order to close the dialog, we need to perform the whole process from another thread. Otherwise we'll get a classical thread lock-up,
             * as sh_light_editor_main_window_release() blocks until the window thread exits, which can't happen from within a message handler.
             */
            _sh_light_editor_main_window*      descriptor = (_sh_light_editor_main_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);            
            system_thread_pool_task_descriptor task       = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL, _sh_light_editor_dialog_close_button_handler, descriptor);

            /* Disable close button first */
            ::EnableMenuItem( ::GetSystemMenu(descriptor->window_handle, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

            /* Okay, pass the request to another thread */
            system_thread_pool_submit_single_task(task);
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
            _sh_light_editor_main_window* descriptor = (_sh_light_editor_main_window*) lparam;

            _sh_light_editor_main_window_initialize_dialog(descriptor, dialog_handle);

            /* Reset the SH projections list UI-wise to fill it with already initialized entries */
            _sh_light_editor_main_window_reset_projections_list(descriptor);

            /* Set the 'dialog created' event so the caller's thread can continue */
            system_event_set(descriptor->dialog_created_event);

            return 1;
        }

        case WM_NOTIFY:
        {
            _sh_light_editor_main_window* descriptor = (_sh_light_editor_main_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);

            /* Tree-view selection might have changed! */
            if ( ((LPNMHDR) lparam)->code == TVN_SELCHANGING)
            {
                _sh_light_editor_handle_on_node_selected_notification(descriptor, ((LPNMTREEVIEW) lparam)->itemNew.hItem);
            }
            else
            /* Right click on a tree-view? */
            if ( ((LPNMHDR) lparam)->code == NM_RCLICK)
            {
                _sh_light_editor_handle_on_treeview_right_click_notification(descriptor);
            }

            break;
        }

    }

    return 0;
}

/** TODO */
PRIVATE void _sh_light_editor_init_descriptor(_sh_light_editor_main_window* descriptor, PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func, ogl_context context)
{
    descriptor->context                                   = context;
    descriptor->dialog_created_event                      = system_event_create(true, false);
    descriptor->on_release_callback_handler_func          = on_release_callback_handler_func;
    descriptor->opened_curve_editor_window_descriptors    = system_resizable_vector_create(DEFAULT_STORAGE_CAPACITY, sizeof(_sh_light_editor_main_window_curve_editor_window*) );
    descriptor->projector_combiner_descriptors            = system_resizable_vector_create(DEFAULT_STORAGE_CAPACITY, sizeof(_sh_light_editor_main_window_projector_combiner*) );
    descriptor->projector_combiner_selected               = NULL;
    descriptor->projector_combiner_selected_projection_id = -1;
    descriptor->serialization_cs                          = system_critical_section_create();
    descriptor->should_handle_en_change_notifications     = true;
    descriptor->window_handle                             = NULL;
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_deinit_curve_editor_window(__in __notnull _sh_light_editor_main_window_curve_editor_window* descriptor_ptr)
{
    if (descriptor_ptr->curve != NULL)
    {
        curve_container_release(descriptor_ptr->curve);
    }

    if (descriptor_ptr->curve_editor_window != NULL)
    {
        curve_editor_curve_window_hide(descriptor_ptr->curve_editor_window);

        descriptor_ptr->curve_editor_window = NULL;
    }
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_deinit_projector(__in __notnull _sh_light_editor_main_window_projector* descriptor_ptr)
{
    if (descriptor_ptr->owner != NULL)
    {
        sh_projector_combiner_release(descriptor_ptr->owner);
    }
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_deinit_projector_combiner(__in __notnull _sh_light_editor_main_window_projector_combiner* descriptor_ptr)
{
    _sh_light_editor_main_window_projector* projector_ptr = NULL;

    sh_projector_combiner_release(descriptor_ptr->instance);
    
    while (system_resizable_vector_pop(descriptor_ptr->projector_descriptors, &projector_ptr) )
    {
        _sh_light_editor_main_window_deinit_projector(projector_ptr);
    }
    system_resizable_vector_release(descriptor_ptr->projector_descriptors);
}

/** TODO */
PRIVATE void _sh_light_editor_handle_on_node_selected_notification(__in __notnull _sh_light_editor_main_window* descriptor, 
                                                                   __in           HTREEITEM                     selected_node_handle)
{
    /* Retrieve selected combiner and projector instances */
    bool                  is_handled              = false;
    uint32_t              n_combiner_descriptors  = system_resizable_vector_get_amount_of_elements(descriptor->projector_combiner_descriptors);
    sh_projector_combiner selected_combiner       = NULL;
    sh_projection_id      selected_projection_id  = -1;

    for (uint32_t n_combiner_descriptor = 0; n_combiner_descriptor < n_combiner_descriptors && !is_handled; n_combiner_descriptor++)
    {
        _sh_light_editor_main_window_projector_combiner* combiner_ptr = NULL;

        if (system_resizable_vector_get_element_at(descriptor->projector_combiner_descriptors, n_combiner_descriptor, &combiner_ptr) )
        {
            _sh_light_editor_main_window_projector* projector_ptr = NULL;
            uint32_t                                n_projectors  = system_resizable_vector_get_amount_of_elements(combiner_ptr->projector_descriptors);

            if (combiner_ptr->root_node_handle == selected_node_handle)
            {
                is_handled             = true;
                selected_combiner      = combiner_ptr->instance;
                selected_projection_id = -1;

                break;
            }

            for (uint32_t n_projector = 0; n_projector < n_projectors && !is_handled; ++n_projector)
            {
                if (system_resizable_vector_get_element_at(combiner_ptr->projector_descriptors, n_projector, &projector_ptr) )
                {
                    if (projector_ptr->node_handle == selected_node_handle)
                    {
                        is_handled             = true;
                        selected_combiner      = combiner_ptr->instance;
                        selected_projection_id = projector_ptr->projection_id;

                        break;
                    } /* if (projector_ptr->node_handle == selected_node_handle) */
                } /* if (system_resizable_vector_get_element_at(combiner_ptr->projector_descriptors, n_projector, &projector_ptr) ) */
            } /* for (uint32_t n_projector = 0; n_projector < n_projectors && !is_handled; ++n_projector) */
        } /* if (system_resizable_vector_get_element_at(descriptor->projector_combiner_descriptors, n_combiner_descriptor, &combiner_ptr) */
    } /* for (uint32_t n_combiner_descriptor = 0; n_combiner_descriptor < n_combiner_descriptors && !is_handled; n_combiner++) */

    /* Reset all names and disable both names and corresponding edit buttons */
    for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property)
    {
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].x, FALSE);
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].y, FALSE);
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].z, FALSE);
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].w, FALSE);
        ::EnableWindow(descriptor->property_name_static_handles[n_property],   FALSE);

        descriptor->property_edit_button_handles[n_property].pfn_x_callback = NULL;
        descriptor->property_edit_button_handles[n_property].pfn_y_callback = NULL;
        descriptor->property_edit_button_handles[n_property].pfn_z_callback = NULL;
        descriptor->property_edit_button_handles[n_property].pfn_w_callback = NULL;
        descriptor->property_edit_button_handles[n_property].x_callback_arg = NULL;
        descriptor->property_edit_button_handles[n_property].y_callback_arg = NULL;
        descriptor->property_edit_button_handles[n_property].z_callback_arg = NULL;
        descriptor->property_edit_button_handles[n_property].w_callback_arg = NULL;

        ::SetWindowTextA(descriptor->property_name_static_handles[n_property], "None");

        descriptor->property_type[n_property] = SH_PROJECTOR_PROPERTY_LAST;
    } /* for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property) */

    /* If a projector was selected, bind actions to the buttons and show corresponding UI elements */
    if (selected_combiner != NULL && selected_projection_id != -1)
    {
        int n_property = 0;

        for (uint32_t property_id = SH_PROJECTOR_PROPERTY_FIRST; property_id < SH_PROJECTOR_PROPERTY_LAST; ++property_id)
        {
            void* property_instance = NULL;

            ASSERT_DEBUG_SYNC(n_property < N_PROPERTIES, "Number of supported properties exceeded!");

            if (sh_projector_combiner_get_sh_projection_property_details(selected_combiner, selected_projection_id, (sh_projector_property) property_id, &property_instance) )
            {
                sh_projector_property_datatype property_datatype = sh_tools_get_sh_projector_property_datatype((sh_projector_property) property_id);
                system_hashed_ansi_string      property_name     = sh_tools_get_sh_projector_property_string  ((sh_projector_property) property_id);

                /* Configure the property static to tell the property name */
                ::SetWindowTextA(descriptor->property_name_static_handles[n_property], system_hashed_ansi_string_get_buffer(property_name) );

                /* Enable edit buttons, depending on how many dimensions the property object has */
                if (property_datatype == SH_PROJECTOR_PROPERTY_DATATYPE_CURVE_CONTAINER)
                {
                    curve_container curve              = (curve_container) property_instance;
                    uint32_t        n_curve_dimensions = curve_container_get_amount_of_dimensions(curve);

                    ASSERT_DEBUG_SYNC(n_curve_dimensions <= 4, "Curves of up to 4 dimensions are only supported by SH light editor!");

                    ::EnableWindow(descriptor->property_name_static_handles[n_property],   (n_curve_dimensions >= 1) );
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].x, (n_curve_dimensions >= 1) );
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].y, (n_curve_dimensions >= 2) );
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].z, (n_curve_dimensions >= 3) );
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].w, (n_curve_dimensions >= 4) );

                    if (n_curve_dimensions >= 1)
                    {
                        descriptor->property_edit_button_handles[n_property].pfn_x_callback = _sh_light_editor_main_window_ui_callback_edit_curve_container_1;
                        descriptor->property_edit_button_handles[n_property].x_callback_arg = curve;

                        if (n_curve_dimensions >= 2)
                        {
                            descriptor->property_edit_button_handles[n_property].pfn_y_callback = _sh_light_editor_main_window_ui_callback_edit_curve_container_2;
                            descriptor->property_edit_button_handles[n_property].y_callback_arg = curve;

                            if (n_curve_dimensions >= 3)
                            {
                                descriptor->property_edit_button_handles[n_property].pfn_z_callback = _sh_light_editor_main_window_ui_callback_edit_curve_container_3;
                                descriptor->property_edit_button_handles[n_property].z_callback_arg = curve;

                                if (n_curve_dimensions >= 4)
                                {
                                    descriptor->property_edit_button_handles[n_property].pfn_w_callback = _sh_light_editor_main_window_ui_callback_edit_curve_container_4;
                                    descriptor->property_edit_button_handles[n_property].w_callback_arg = curve;
                                } /* if (n_curve_dimensions >= 4) */
                            } /* if (n_curve_dimensions >= 3) */
                        } /* if (n_curve_dimensions >= 2) */
                    } /* if (n_curve_dimensions >= 1) */
                } /* if (property_datatype == SH_PROJECTOR_PROPERTY_DATATYPE_CURVE_CONTAINER) */
                else
                {
                    /* Not a curve container - not editable */
                    ::EnableWindow(descriptor->property_name_static_handles[n_property],   FALSE);
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].x, FALSE);
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].y, FALSE);
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].z, FALSE);
                    ::EnableWindow(descriptor->property_edit_button_handles[n_property].w, FALSE);

                    memset(descriptor->property_edit_button_handles + n_property, 0, sizeof(descriptor->property_edit_button_handles[n_property]) );
                }

                /* Stash the property type so we can quickly react to edit button clicks */
                descriptor->property_type[n_property] = (sh_projector_property) property_id;

                /* Increment the property counter. */
                n_property++;
            } /* if (sh_projector_combiner_get_sh_projection_property_details(selected_combiner, selected_projection_id, (sh_projection_property) property_id, &property_instance) ) */
        } /* for (uint32_t n_property = SH_PROJECTION_PROPERTY_FIRST; n_property < SH_PROJECTION_PROPERTY_LAST; ++n_property) */
    } /* if (selected_combiner != NULL && selected_projection_id != -1) */

    /* Update SH light editor descriptor */
    descriptor->projector_combiner_selected               = selected_combiner;
    descriptor->projector_combiner_selected_projection_id = selected_projection_id;

    /* Redraw the whole UI */
    ::InvalidateRect(descriptor->window_handle, NULL, TRUE);
    ::UpdateWindow  (descriptor->window_handle);
}

/** TODO */
PRIVATE void _sh_light_editor_handle_on_treeview_right_click_notification(__in __notnull _sh_light_editor_main_window* descriptor)
{
    /* Only proceed if a combiner was selected */
    if (descriptor->projector_combiner_selected != NULL)
    {
        system_context_menu new_context_menu = system_context_menu_create(system_hashed_ansi_string_create("SH light editor menu") );

        ASSERT_DEBUG_SYNC(new_context_menu != NULL, "Out of memory");
        if (new_context_menu != NULL)
        {
            /* When request to show the context menu, we're blocked until the menu is hidden. This means we're cool to allocate the descriptors on heap,
             * and then free them once we're done with the operation.
             */
            _sh_light_editor_main_window_context_menu_add_projection_argument*    argument_descriptors       = new (std::nothrow) _sh_light_editor_main_window_context_menu_add_projection_argument[SH_PROJECTION_TYPE_LAST];
            _sh_light_editor_main_window_context_menu_delete_projection_argument* delete_argument_descriptor = NULL;

            ASSERT_DEBUG_SYNC(argument_descriptors != NULL, "Out of memory");
            if (argument_descriptors != NULL)
            {
                ASSERT_DEBUG_SYNC(SH_PROJECTION_TYPE_FIRST == 0, "First projection type must start from 0");

                for (sh_projection_type projection_type = SH_PROJECTION_TYPE_FIRST; projection_type < SH_PROJECTION_TYPE_LAST; ((int&)projection_type)++)
                {
                    /* Initialize descriptor for the argument */
                    argument_descriptors[projection_type].main_window          = (sh_light_editor_main_window) descriptor;
                    argument_descriptors[projection_type].projection_combiner  = descriptor->projector_combiner_selected;
                    argument_descriptors[projection_type].projection_requested = projection_type;

                    /* Add a new item */
                    const char* projection_name = system_hashed_ansi_string_get_buffer(sh_tools_get_sh_projection_type_string(projection_type) );

                    system_context_menu_append_item(new_context_menu,
                                                    system_hashed_ansi_string_create_by_merging_two_strings("Add: ", projection_name),
                                                    _sh_light_editor_main_window_ui_callback_add_sh_projection,
                                                    argument_descriptors + projection_type,
                                                    false, /* not checked */
                                                    true); /* enabled */
                } /* for (sh_projection_type projection_type = SH_PROJECTION_TYPE_FIRST; projection_type < SH_PROJECTION_TYPE_LAST; ((int&)projection_type)++) */

                /* Also allow to delete selected projection (if any) */
                if (descriptor->projector_combiner_selected_projection_id != -1)
                {
                    delete_argument_descriptor = new (std::nothrow) _sh_light_editor_main_window_context_menu_delete_projection_argument;

                    ASSERT_DEBUG_SYNC(delete_argument_descriptor != NULL, "Out of memory");
                    if (delete_argument_descriptor != NULL)
                    {
                        sh_projection_type selected_projection_type      = SH_PROJECTION_TYPE_UNKNOWN;
                        const char*        selected_projection_type_name = NULL;

                        delete_argument_descriptor->main_window         = (sh_light_editor_main_window) descriptor;
                        delete_argument_descriptor->projection_combiner = descriptor->projector_combiner_selected;
                        delete_argument_descriptor->projection_id       = descriptor->projector_combiner_selected_projection_id;

                        sh_projector_combiner_get_sh_projection_property_details(descriptor->projector_combiner_selected,
                                                                                 descriptor->projector_combiner_selected_projection_id,
                                                                                 SH_PROJECTOR_PROPERTY_TYPE,
                                                                                 &selected_projection_type);

                        selected_projection_type_name = system_hashed_ansi_string_get_buffer(sh_tools_get_sh_projection_type_string(selected_projection_type) );

                        system_context_menu_append_item(new_context_menu,
                                                        system_hashed_ansi_string_create_by_merging_two_strings("Delete selected: ", selected_projection_type_name),
                                                        _sh_light_editor_main_window_ui_callback_delete_sh_projection,
                                                        delete_argument_descriptor,
                                                        false, /* not checked */
                                                        true); /* enabled */
                    }
                } /* if (descriptor->projector_combiner_selected_projection_id != -1) */

                /* Good! Show the menu */
                POINT mouse_pos;

                ::GetCursorPos(&mouse_pos);

                system_context_menu_show(new_context_menu, (system_window_handle) descriptor->sh_light_projections_treeview_handle, mouse_pos.x, mouse_pos.y);

                /* Done, we're good to release the descriptors */
                delete [] argument_descriptors;
                delete    delete_argument_descriptor;

                argument_descriptors       = NULL;
                delete_argument_descriptor = NULL;
            } /* if (argument_descriptors != NULL) */

            /* Release the menu instance */
            system_context_menu_release(new_context_menu);
        } /* if (new_context_menu != NULL) */
    } /* if (descriptor->projector_combiner_selected != NULL) */

}

/** TODO */
PRIVATE void _sh_light_editor_handle_on_treeview_right_click_notification(__in __notnull _sh_light_editor_main_window* descriptor);

/** TODO */
PRIVATE HWND _sh_light_editor_main_window_get_xyzw_edit_button_window_handle_for_curve_container(__in __notnull _sh_light_editor_main_window* descriptor_ptr,
                                                                                                 __in __notnull curve_container               curve,
                                                                                                 __in           uint32_t                      n_dimension)
{
    HWND result = NULL;

    ASSERT_DEBUG_SYNC(curve       != NULL,                     "Curve container is NULL");
    ASSERT_DEBUG_SYNC(n_dimension >= 0    && n_dimension <= 3, "Dimension index out of range");

    for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property)
    {
        if ( (curve_container) descriptor_ptr->property_edit_button_handles[n_property].x_callback_arg == curve &&
                               n_dimension                                                             == 0)
        {
            result = descriptor_ptr->property_edit_button_handles[n_property].x;

            break;
        }
        else
        if ( (curve_container) descriptor_ptr->property_edit_button_handles[n_property].y_callback_arg == curve &&
                               n_dimension                                                             == 1)
        {
            result = descriptor_ptr->property_edit_button_handles[n_property].y;

            break;
        }
        else
        if ( (curve_container) descriptor_ptr->property_edit_button_handles[n_property].z_callback_arg == curve &&
                               n_dimension                                                             == 2)
        {
            result = descriptor_ptr->property_edit_button_handles[n_property].z;

            break;
        }
        else
        if ( (curve_container) descriptor_ptr->property_edit_button_handles[n_property].w_callback_arg == curve &&
                               n_dimension                                                             == 3)
        {
            result = descriptor_ptr->property_edit_button_handles[n_property].w;

            break;
        }
    } /* for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property) */

    return result;
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_initialize_dialog(_sh_light_editor_main_window* descriptor, HWND dialog_handle)
{
    ASSERT_DEBUG_SYNC(dialog_handle != NULL, "Reported dialog handle is null!");

    /* Set window handle */
    descriptor->window_handle = dialog_handle;

    /* Retrieve window component handles */
    const uint32_t resource_name_table[N_PROPERTIES * 5] = 
    {
        IDC_PROPERTY_NAME1, IDC_PROPERTY_EDITX1, IDC_PROPERTY_EDITY1, IDC_PROPERTY_EDITZ1, IDC_PROPERTY_EDITW1,
        IDC_PROPERTY_NAME2, IDC_PROPERTY_EDITX2, IDC_PROPERTY_EDITY2, IDC_PROPERTY_EDITZ2, IDC_PROPERTY_EDITW2,
        IDC_PROPERTY_NAME3, IDC_PROPERTY_EDITX3, IDC_PROPERTY_EDITY3, IDC_PROPERTY_EDITZ3, IDC_PROPERTY_EDITW3,
        IDC_PROPERTY_NAME4, IDC_PROPERTY_EDITX4, IDC_PROPERTY_EDITY4, IDC_PROPERTY_EDITZ4, IDC_PROPERTY_EDITW4,
        IDC_PROPERTY_NAME5, IDC_PROPERTY_EDITX5, IDC_PROPERTY_EDITY5, IDC_PROPERTY_EDITZ5, IDC_PROPERTY_EDITW5,
        IDC_PROPERTY_NAME6, IDC_PROPERTY_EDITX6, IDC_PROPERTY_EDITY6, IDC_PROPERTY_EDITZ6, IDC_PROPERTY_EDITW6,
        IDC_PROPERTY_NAME7, IDC_PROPERTY_EDITX7, IDC_PROPERTY_EDITY7, IDC_PROPERTY_EDITZ7, IDC_PROPERTY_EDITW7,
        IDC_PROPERTY_NAME8, IDC_PROPERTY_EDITX8, IDC_PROPERTY_EDITY8, IDC_PROPERTY_EDITZ8, IDC_PROPERTY_EDITW8,
    };

    descriptor->sh_light_projections_treeview_handle = ::GetDlgItem(dialog_handle, IDC_SH_LIGHT_PROJECTIONS);
    ASSERT_DEBUG_SYNC(descriptor->sh_light_projections_treeview_handle != NULL, "Could not retrieve treeview window handle");

    for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property)
    {
        descriptor->property_name_static_handles[n_property]   = ::GetDlgItem(dialog_handle, resource_name_table[n_property * 5 + 0]);
        descriptor->property_edit_button_handles[n_property].x = ::GetDlgItem(dialog_handle, resource_name_table[n_property * 5 + 1]);
        descriptor->property_edit_button_handles[n_property].y = ::GetDlgItem(dialog_handle, resource_name_table[n_property * 5 + 2]);
        descriptor->property_edit_button_handles[n_property].z = ::GetDlgItem(dialog_handle, resource_name_table[n_property * 5 + 3]);
        descriptor->property_edit_button_handles[n_property].w = ::GetDlgItem(dialog_handle, resource_name_table[n_property * 5 + 4]);

        ASSERT_DEBUG_SYNC(descriptor->property_edit_button_handles[n_property].x != NULL, "Could not retrieve property X edit button window handle");
        ASSERT_DEBUG_SYNC(descriptor->property_edit_button_handles[n_property].y != NULL, "Could not retrieve property Y edit button window handle");
        ASSERT_DEBUG_SYNC(descriptor->property_edit_button_handles[n_property].z != NULL, "Could not retrieve property Z edit button window handle");
        ASSERT_DEBUG_SYNC(descriptor->property_edit_button_handles[n_property].w != NULL, "Could not retrieve property W edit button window handle");
        ASSERT_DEBUG_SYNC(descriptor->property_name_static_handles[n_property]   != NULL, "Could not retrieve property name static window handle");
    }

    /* Reset property types - these will be set later on */
    for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property)
    {
        descriptor->property_type[n_property] = SH_PROJECTOR_PROPERTY_LAST;
    }
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_init_curve_editor_window(__in __notnull _sh_light_editor_main_window_curve_editor_window* descriptor_ptr,
                                                                   __in __notnull curve_container                                   curve,
                                                                   __in __notnull curve_editor_curve_window                         curve_window,
                                                                   __in           uint32_t                                          n_dimension)
{
    descriptor_ptr->curve               = curve;
    descriptor_ptr->curve_editor_window = curve_window;
    descriptor_ptr->n_dimension         = n_dimension;

    curve_container_retain(curve);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_init_projector(__in __notnull _sh_light_editor_main_window_projector* child_descriptor_ptr, 
                                                         __in __notnull sh_projector_combiner                   combiner_instance,
                                                         __in           sh_projection_id                        projection_id,
                                                         __in           sh_projection_type                      projection_type,
                                                         __in           HTREEITEM                               node_handle)
{
    child_descriptor_ptr->node_handle     = node_handle;
    child_descriptor_ptr->owner           = combiner_instance;
    child_descriptor_ptr->projection_id   = projection_id;
    child_descriptor_ptr->projection_type = projection_type;

    sh_projector_combiner_retain(combiner_instance);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_init_projector_combiner(__in __notnull _sh_light_editor_main_window_projector_combiner* descriptor_ptr,
                                                                  __in __notnull sh_projector_combiner                            combiner_instance, 
                                                                  __in           HTREEITEM                                        node_handle,
                                                                  __in __notnull sh_light_editor_main_window                      main_window)
{
    descriptor_ptr->instance              = combiner_instance;
    descriptor_ptr->projector_descriptors = system_resizable_vector_create(DEFAULT_STORAGE_CAPACITY, sizeof(_sh_light_editor_main_window_projector*) );
    descriptor_ptr->root_node_handle      = node_handle;

    sh_projector_combiner_retain(combiner_instance);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_projector_combiner_callback_projection_added(__in __notnull void* arg)
{
    sh_light_editor_main_window_refresh( (sh_light_editor_main_window) arg);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_projector_combiner_callback_projection_deleted(__in __notnull void* arg)
{
    sh_light_editor_main_window_refresh( (sh_light_editor_main_window) arg);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_release(__in __notnull _sh_light_editor_main_window* main_window_ptr, bool do_full_release)
{
    if (main_window_ptr->projector_combiner_descriptors != NULL)
    {
        _sh_light_editor_main_window_projector_combiner* descriptor_ptr = NULL;

        while (system_resizable_vector_pop(main_window_ptr->projector_combiner_descriptors, &descriptor_ptr) )
        {
            _sh_light_editor_main_window_deinit_projector_combiner(descriptor_ptr);

            delete descriptor_ptr;
            descriptor_ptr = NULL;
        }
    }

    if (main_window_ptr->opened_curve_editor_window_descriptors != NULL)
    {
        _sh_light_editor_main_window_curve_editor_window* descriptor_ptr = NULL;

        while (system_resizable_vector_pop(main_window_ptr->opened_curve_editor_window_descriptors, &descriptor_ptr) )
        {
            _sh_light_editor_main_window_deinit_curve_editor_window(descriptor_ptr);

            delete descriptor_ptr;
            descriptor_ptr = NULL;
        }
    }

    if (do_full_release)
    {
        if (main_window_ptr->projector_combiner_descriptors != NULL)
        {
            system_resizable_vector_release(main_window_ptr->projector_combiner_descriptors);

            main_window_ptr->projector_combiner_descriptors = NULL;
        }

        if (main_window_ptr->opened_curve_editor_window_descriptors != NULL)
        {
            system_resizable_vector_release(main_window_ptr->opened_curve_editor_window_descriptors);

            main_window_ptr->opened_curve_editor_window_descriptors = NULL;
        }
    }
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_reset_projections_list(_sh_light_editor_main_window* descriptor)
{
    /* Clear existing tree-view contents */
    {
        DWORD treeview_style = ::GetWindowLongA(descriptor->sh_light_projections_treeview_handle, GWL_STYLE);
        
        TreeView_DeleteItem(descriptor->sh_light_projections_treeview_handle, TVI_ROOT);
        ::SetWindowLongA   (descriptor->sh_light_projections_treeview_handle, GWL_STYLE, treeview_style);
    }

    /* Reset edit buttons & names */
    for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property)
    {
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].x, FALSE);
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].y, FALSE);
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].z, FALSE);
        ::EnableWindow(descriptor->property_edit_button_handles[n_property].w, FALSE);
        ::EnableWindow(descriptor->property_name_static_handles[n_property],   FALSE);

        descriptor->property_edit_button_handles[n_property].pfn_x_callback = NULL;
        descriptor->property_edit_button_handles[n_property].pfn_y_callback = NULL;
        descriptor->property_edit_button_handles[n_property].pfn_z_callback = NULL;
        descriptor->property_edit_button_handles[n_property].pfn_w_callback = NULL;
        descriptor->property_edit_button_handles[n_property].x_callback_arg = NULL;
        descriptor->property_edit_button_handles[n_property].y_callback_arg = NULL;
        descriptor->property_edit_button_handles[n_property].z_callback_arg = NULL;
        descriptor->property_edit_button_handles[n_property].w_callback_arg = NULL;

        ::SetWindowText(descriptor->property_name_static_handles[n_property], "None");
    } /* for (uint32_t n_property = 0; n_property < N_PROPERTIES; ++n_property) */

    /* Clean up before we continue! */
    _sh_light_editor_main_window_release(descriptor, false);

    descriptor->projector_combiner_selected               = NULL;
    descriptor->projector_combiner_selected_projection_id = -1;

    /* Retrieve pointers to projection combiners - these will be used to create root nodes */
    system_hashed_ansi_string projection_combiner_directory_name = object_manager_convert_object_manager_object_type_to_hashed_ansi_string(OBJECT_TYPE_SH_PROJECTOR_COMBINER);
    object_manager_directory  projection_combiner_directory      = object_manager_get_directory                                           (projection_combiner_directory_name);

    ASSERT_DEBUG_SYNC(projection_combiner_directory_name != NULL, "Projector combiners are not recognized by object manager");
    if (projection_combiner_directory == NULL)
    {
        /* No projection combiner instances available - our job here is done */
        return;
    }

    /* Iterate through instances and create descriptors for every projector combiner.
     * We'll use them to store combiner-specific data */
    sh_projector_combiner first_combiner = NULL;
    uint32_t              n_combiners    = object_manager_directory_get_amount_of_children_for_directory(projection_combiner_directory);

    for (uint32_t n_combiner = 0; n_combiner < n_combiners; ++n_combiner)
    {
        object_manager_item projection_item = NULL;

        if (object_manager_directory_get_subitem_at(projection_combiner_directory, n_combiner, &projection_item) )
        {
            sh_projector_combiner combiner_instance = (sh_projector_combiner) object_manager_item_get_raw_pointer(projection_item);

            /* Cache the very first combiner instance on the list - we'll use it later for selecting the first item */
            if (n_combiner == 0)
            {
                first_combiner = combiner_instance;
            }

            ASSERT_DEBUG_SYNC(combiner_instance != NULL, "Combiner instance is NULL");
            if (combiner_instance != NULL)
            {
                system_hashed_ansi_string combiner_name = object_manager_item_get_name(projection_item);

                /* Add a new root node to the tree */
                HTREEITEM       root_node_handle;
                TV_INSERTSTRUCT ui_descriptor;

                memset(&ui_descriptor, 0, sizeof(ui_descriptor) );

                ui_descriptor.hParent         = TVI_ROOT;
                ui_descriptor.hInsertAfter    = TVI_LAST;
                ui_descriptor.item.mask       = TVIF_TEXT;
                ui_descriptor.item.pszText    = (char*) system_hashed_ansi_string_get_buffer(combiner_name);
                ui_descriptor.item.cchTextMax = strlen(ui_descriptor.item.pszText);

                root_node_handle = (HTREEITEM) ::SendDlgItemMessageA(descriptor->window_handle,
                                                                     IDC_SH_LIGHT_PROJECTIONS,
                                                                     TVM_INSERTITEM,
                                                                     0,
                                                                     (LPARAM) &ui_descriptor);

                /* Initialize the descriptor */
                _sh_light_editor_main_window_projector_combiner* descriptor_ptr = new (std::nothrow) _sh_light_editor_main_window_projector_combiner;
                ASSERT_ALWAYS_SYNC(descriptor_ptr != NULL, "Out of memory");

                _sh_light_editor_main_window_init_projector_combiner(descriptor_ptr, combiner_instance, root_node_handle, (sh_light_editor_main_window) descriptor);

                /* Iterate through assigned SH projections and add them as children */
                uint32_t n_projections = sh_projector_combiner_get_amount_of_sh_projections(combiner_instance);

                for (uint32_t n_projection = 0; n_projection < n_projections; n_projection++)
                {
                    sh_projection_id   projection_id;
                    sh_projection_type projection_type;

                    if (sh_projector_combiner_get_sh_projection_property_id     (combiner_instance, n_projection,  &projection_id)                              &&
                        sh_projector_combiner_get_sh_projection_property_details(combiner_instance, projection_id, SH_PROJECTOR_PROPERTY_TYPE, &projection_type))
                    {
                        /* Retrieve SH projection name */
                        system_hashed_ansi_string projection_name = sh_tools_get_sh_projection_type_string(projection_type);

                        /* Create a child node */
                        HTREEITEM node_handle;

                        memset(&ui_descriptor, 0, sizeof(ui_descriptor) );

                        ui_descriptor.hParent      = root_node_handle;
                        ui_descriptor.hInsertAfter = TVI_LAST;
                        ui_descriptor.item.mask    = TVIF_TEXT;
                        ui_descriptor.item.pszText = (LPSTR) system_hashed_ansi_string_get_buffer(projection_name);

                        node_handle = (HTREEITEM) ::SendDlgItemMessageA(descriptor->window_handle,
                                                                        IDC_SH_LIGHT_PROJECTIONS,
                                                                        TVM_INSERTITEM,
                                                                        0,
                                                                        (LPARAM) &ui_descriptor);

                        /* Initialize the descriptor */
                        _sh_light_editor_main_window_projector* child_descriptor_ptr = new (std::nothrow) _sh_light_editor_main_window_projector;
                        ASSERT_ALWAYS_SYNC(child_descriptor_ptr != NULL, "Out of memory");

                        _sh_light_editor_main_window_init_projector(child_descriptor_ptr, combiner_instance, projection_id, projection_type, node_handle);

                        /* Attach it to our root node descriptor */
                        system_resizable_vector_push(descriptor_ptr->projector_descriptors, child_descriptor_ptr);
                    } /* if (sh_projector_combiner_get_sh_projection_property_id(combiner_instance, n_projection, &projection_id)) */
                    else
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve SH Projection details");
                    }
                } /* for (uint32_t n_projection = 0; n_projection < n_projections; n_projection++) */

                /* Expand the root node */
                TreeView_Expand(descriptor->sh_light_projections_treeview_handle, root_node_handle, TVE_EXPAND);

                /* Store the descriptor */
                system_resizable_vector_push(descriptor->projector_combiner_descriptors, descriptor_ptr);
            } /* if (combiner_instance != NULL) */
        } /* if (object_manager_directory_get_subitem_at(projection_combiner_directory, n_projection, &projection_item) ) */
    } /* for (uint32_t n_projection = 0; n_projection < n_projections; ++n_projection)*/

    /* Reset selection */
    _sh_light_editor_select_projector_combiner(descriptor, NULL);

    /* Update the tree view */
    ::InvalidateRect(descriptor->sh_light_projections_treeview_handle, NULL, TRUE);
    ::UpdateWindow  (descriptor->sh_light_projections_treeview_handle);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_add_sh_projection(__in __notnull void* arg)
{
    _sh_light_editor_main_window_context_menu_add_projection_argument* arg_ptr         = (_sh_light_editor_main_window_context_menu_add_projection_argument*) arg;
    _sh_light_editor_main_window*                                      main_window_ptr = (_sh_light_editor_main_window*)                                      arg_ptr->main_window;
    bool                                                               result          = false;

    result = sh_projector_combiner_add_sh_projection(arg_ptr->projection_combiner,
                                                     arg_ptr->projection_requested,
                                                     NULL);

    ASSERT_DEBUG_SYNC(result, "Could not add SH projection");
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_curve_editor_window_closed(void* arg, curve_container curve, uint8_t dimension_index)
{
    bool                          is_handled       = false;
    _sh_light_editor_main_window* main_window_ptr  = (_sh_light_editor_main_window*) arg;
    
    system_critical_section_enter(main_window_ptr->serialization_cs);
    {
        uint32_t n_opened_windows = system_resizable_vector_get_amount_of_elements(main_window_ptr->opened_curve_editor_window_descriptors);

        for (size_t n_window = 0; n_window < n_opened_windows; ++n_window)
        {
            _sh_light_editor_main_window_curve_editor_window* descriptor_ptr = NULL;

            if (system_resizable_vector_get_element_at(main_window_ptr->opened_curve_editor_window_descriptors, n_window, &descriptor_ptr) )
            {
                /* Is this the curve we got a report about? */
                if (descriptor_ptr->curve == curve)
                {
                    /* Indeed - if a UI component is around, re-enable it. */
                    HWND edit_handle = _sh_light_editor_main_window_get_xyzw_edit_button_window_handle_for_curve_container(main_window_ptr, curve, dimension_index);

                    if (edit_handle != NULL)
                    {
                        ::EnableWindow(edit_handle, TRUE);
                    } /* if (edit_handle != NULL) */

                    /* Release the descriptor and get it off the vector */
                    descriptor_ptr->curve_editor_window = NULL;

                    system_resizable_vector_delete_element_at(main_window_ptr->opened_curve_editor_window_descriptors, n_window);

                    _sh_light_editor_main_window_deinit_curve_editor_window(descriptor_ptr);
                    delete descriptor_ptr;

                    /* Done */
                    is_handled = true;

                    break;
                } /* if (descriptor_ptr->curve == curve) */
            } /* if (system_resizable_vector_get_element_at(main_window_ptr->opened_curve_editor_window_descriptors, n_window, &descriptor_ptr) )*/
        } /* for (uint32_t n_window = 0; n_window < n_opened_windows; ++n_window) */
    }
    system_critical_section_leave(main_window_ptr->serialization_cs);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_delete_sh_projection(__in __notnull void* arg)
{
    _sh_light_editor_main_window_context_menu_delete_projection_argument* arg_ptr         = (_sh_light_editor_main_window_context_menu_delete_projection_argument*) arg;
    _sh_light_editor_main_window*                                         main_window_ptr = (_sh_light_editor_main_window*)                                         arg_ptr->main_window;
    bool                                                                  result          = false;

    result = sh_projector_combiner_delete_sh_projection(arg_ptr->projection_combiner,
                                                        arg_ptr->projection_id);
    ASSERT_DEBUG_SYNC(result, "Could not delete SH projection");
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_edit_curve_container(__in __notnull sh_light_editor_main_window main_window, __in __maybenull void* arg, __in uint32_t dimension_index)
{
    curve_container               curve           = (curve_container)               arg;
    _sh_light_editor_main_window* main_window_ptr = (_sh_light_editor_main_window*) main_window;

    system_critical_section_enter(main_window_ptr->serialization_cs);
    {
        /* Open the window */
        curve_editor_curve_window new_curve_window = curve_editor_curve_window_show(main_window_ptr->context,
                                                                                    curve,
                                                                                    dimension_index,
                                                                                    _sh_light_editor_main_window_ui_callback_curve_editor_window_closed,
                                                                                    main_window_ptr,
                                                                                    main_window_ptr->serialization_cs);

        /* Create a new descriptor */
        _sh_light_editor_main_window_curve_editor_window* descriptor_ptr = new (std::nothrow) _sh_light_editor_main_window_curve_editor_window;

        ASSERT_DEBUG_SYNC(descriptor_ptr != NULL, "Out of memory");
        if (descriptor_ptr != NULL)
        {
            _sh_light_editor_main_window_init_curve_editor_window(descriptor_ptr, curve, new_curve_window, dimension_index);

            /* Store the descriptor */
            system_resizable_vector_push(main_window_ptr->opened_curve_editor_window_descriptors,
                                         descriptor_ptr);

            /* Find the UI widget we need to disable so that the user is not allowed to launch more than one editor at a time */
            HWND edit_button = _sh_light_editor_main_window_get_xyzw_edit_button_window_handle_for_curve_container(main_window_ptr, curve, dimension_index);

            if (edit_button != 0)
            {
                ::EnableWindow(edit_button, FALSE);
            }
        } /* if (descriptor_ptr != NULL) */
    }
    system_critical_section_leave(main_window_ptr->serialization_cs);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_edit_curve_container_1(__in __notnull sh_light_editor_main_window main_window, __in __maybenull void* arg)
{
    _sh_light_editor_main_window_ui_callback_edit_curve_container(main_window, arg, 0);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_edit_curve_container_2(__in __notnull sh_light_editor_main_window main_window, __in __maybenull void* arg)
{
    _sh_light_editor_main_window_ui_callback_edit_curve_container(main_window, arg, 1);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_edit_curve_container_3(__in __notnull sh_light_editor_main_window main_window, __in __maybenull void* arg)
{
    _sh_light_editor_main_window_ui_callback_edit_curve_container(main_window, arg, 2);
}

/** TODO */
PRIVATE void _sh_light_editor_main_window_ui_callback_edit_curve_container_4(__in __notnull sh_light_editor_main_window main_window, __in __maybenull void* arg)
{
    _sh_light_editor_main_window_ui_callback_edit_curve_container(main_window, arg, 3);
}

/** TODO */
PRIVATE void _sh_light_editor_select_projector_combiner(__in __notnull   _sh_light_editor_main_window* descriptor_ptr, 
                                                        __in __maybenull sh_projector_combiner         first_combiner)
{
    /* Find a descriptor that matches the requested instance */
    bool     is_handled    = false;
    uint32_t n_descriptors = system_resizable_vector_get_amount_of_elements(descriptor_ptr->projector_combiner_descriptors);

    for (uint32_t n_descriptor = 0; n_descriptor < n_descriptors && !is_handled; ++n_descriptor)
    {
        _sh_light_editor_main_window_projector_combiner* projector_combiner_descriptor_ptr = NULL;
        
        if (system_resizable_vector_get_element_at(descriptor_ptr->projector_combiner_descriptors, n_descriptor, &projector_combiner_descriptor_ptr) )
        {
            /* Select the node */
            TreeView_Select(descriptor_ptr->sh_light_projections_treeview_handle,
                            projector_combiner_descriptor_ptr->root_node_handle,
                            TVGN_FIRSTVISIBLE | TVGN_CARET);

            is_handled = true;
        } /* if (system_resizable_vector_get_element_at(descriptor_ptr->projector_combiner_descriptors, n_descriptor, &projector_combiner_descriptor_ptr) ) */
    } /* for (uint32_t n_descriptor = 0; n_descriptor < n_descriptors; ++n_descriptor) */

    /* Not handled? Unselect then */
    if (!is_handled)
    {
        TreeView_Select(descriptor_ptr->sh_light_projections_treeview_handle,
                        NULL,
                        TVGN_CARET);
    }
}

/* Please see header for specification */
PUBLIC sh_light_editor_main_window sh_light_editor_main_window_create(PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func, __in __notnull ogl_context context)
{
    /* Create result container */
    _sh_light_editor_main_window* result = new (std::nothrow) _sh_light_editor_main_window;

    if (result != NULL)
    {
        /* Fill with default values */
        _sh_light_editor_init_descriptor(result, on_release_callback_handler_func, context);

        /* Retain the context - we need to cache it */
        ogl_context_retain(context);

        /* Set up callbacks */
        sh_projector_combiner_set_callback(SH_PROJECTOR_COMBINER_CALLBACK_TYPE_ON_PROJECTION_ADDED,
                                           _sh_light_editor_main_window_projector_combiner_callback_projection_added,
                                           result);

        sh_projector_combiner_set_callback(SH_PROJECTOR_COMBINER_CALLBACK_TYPE_ON_PROJECTION_DELETED,
                                           _sh_light_editor_main_window_projector_combiner_callback_projection_deleted,
                                           result);

        /* Dialog must be hosted within a thread of its own. Therefore, create a new thread, launch the dialog in there and wait till it
         * notifies us stuff is set up.
         */
        system_threads_spawn(_curve_editor_dialog_thread_entrypoint, result, &result->dialog_thread_event);

        /* Block till everything is ready */
        system_event_wait_single_infinite(result->dialog_created_event);
    }

    return (sh_light_editor_main_window) result;
}

/* Please see header for specification */
PUBLIC void sh_light_editor_main_window_refresh(__in __notnull sh_light_editor_main_window main_window)
{
    _sh_light_editor_main_window* main_window_ptr = (_sh_light_editor_main_window*) main_window;

    /* Wait till the window is initialized! */
    system_event_wait_single_infinite(main_window_ptr->dialog_created_event);

    /* Okay, carry on */
    _sh_light_editor_main_window_reset_projections_list(main_window_ptr);
}

/* Please see header for specification */
PUBLIC void sh_light_editor_main_window_release(__in __notnull __post_invalid sh_light_editor_main_window main_window)
{
    _sh_light_editor_main_window* main_window_ptr = (_sh_light_editor_main_window*) main_window;

    /* Call back release handler */
    main_window_ptr->on_release_callback_handler_func();

    /* Release the context - we are cool if it was to be released now */
    ogl_context_release(main_window_ptr->context);

    /* If the window is up, get rid of it */
    if (main_window_ptr->window_handle != NULL)
    {
        ::SendMessageA(main_window_ptr->window_handle, WM_CUSTOMIZED_SHUT_DOWN, 0, 0);
        
        system_event_wait_single_infinite(main_window_ptr->dialog_thread_event);
    }

    /* Release all objects allocated for the window */
    if (main_window_ptr->dialog_created_event != NULL)
    {
        system_event_release(main_window_ptr->dialog_created_event);

        main_window_ptr->dialog_created_event = NULL;
    }

    if (main_window_ptr->dialog_thread_event != NULL)
    {
        system_event_release(main_window_ptr->dialog_thread_event);

        main_window_ptr->dialog_thread_event = NULL;
    }

    /* Release sub-items */
    _sh_light_editor_main_window_release(main_window_ptr, true);

    delete main_window_ptr;
    main_window_ptr = NULL;
}
