/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "main.h"

#ifdef _WIN32
    #include <CommCtrl.h>
#endif

#include "../resource.h"
#include "curve/curve_container.h"
#include "curve/curve_segment.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_curve_window.h"
#include "curve_editor/curve_editor_main_window.h"
#include "curve_editor/curve_editor_watchdog.h"
#include "object_manager/object_manager_directory.h"
#include "object_manager/object_manager_general.h"
#include "object_manager/object_manager_item.h"
#include "ogl/ogl_context.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_threads.h"
#include "system/system_thread_pool.h"
#include "system/system_variant.h"

#define WM_CUSTOMIZED_SHUT_DOWN (WM_USER + 1)


/* Private definitions */
typedef struct
{
    curve_container           curve;
    HTREEITEM                 curve_node_handle;
    curve_editor_curve_window window;
} _window_map_array_descriptor;

typedef struct
{
    HWND curves_tree_window_handle;
    HWND edit_curve_button_window_handle;
    HWND format_static_window_handle;
    HWND name_static_window_handle;
    HWND type_static_window_handle;
    HWND curve_editbox_window_handle;

    ogl_context           context;
    curve_editor_watchdog watchdog;
    HWND                  window_handle;

    system_event     dialog_created_event;
    system_event     dialog_thread_event;
    system_hash64map node_handle_to_registry_path_map;

    system_critical_section serialization_cs;

    system_hash64map                          curve_node_handle_to_curve_window_map_array_descriptor; /* maps node handle to an array of _window_map_array_descriptor instance */
    PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func;

    bool should_handle_en_change_notifications;
} _curve_editor_main_window;

/* Private variables */

/* Private functions */
/* Forward declarations */
PRIVATE curve_container  _curve_editor_dialog_get_selected_curve          (_curve_editor_main_window* descriptor_ptr);
PRIVATE void             _curve_editor_dialog_handle_edit_request         (_curve_editor_main_window* descriptor_ptr);
PRIVATE bool             _curve_editor_dialog_item_name_comparator        (void*                      has_1,
                                                                           void*                      has_2);
PRIVATE void             _curve_editor_dialog_on_curve_window_release     (void*,
                                                                           curve_container);
PRIVATE void             _curve_editor_dialog_update_curve_tree           (_curve_editor_main_window* descriptor_ptr,
                                                                           HTREEITEM                  parent_node_handle,
                                                                           object_manager_directory   parent_directory,
                                                                           system_hashed_ansi_string  directory_registry_path);
PRIVATE void             _curve_editor_dialog_update_ui                   (_curve_editor_main_window* descriptor_ptr);
PRIVATE void             _curve_editor_dialog_update_ui_curve_edit_buttons(_curve_editor_main_window* descriptor_ptr,
                                                                           HTREEITEM                  curve_node_handle);
PRIVATE INT_PTR CALLBACK _curve_editor_dialog_window_message_handler      (HWND                       dialog_handle,
                                                                           UINT                       message_id,
                                                                           WPARAM                     wparam,
                                                                           LPARAM                     lparam);
PRIVATE void             _curve_editor_main_window_initialize_dialog      (_curve_editor_main_window* descriptor,
                                                                           HWND                       dialog_handle);


/** TODO */
PRIVATE curve_container _curve_editor_dialog_get_selected_curve(_curve_editor_main_window* descriptor_ptr)
{
    curve_container           result                      = NULL;
    HTREEITEM                 selected_item_handle        = TreeView_GetSelection(descriptor_ptr->curves_tree_window_handle);
    system_hashed_ansi_string selected_item_registry_path = NULL;

    if (selected_item_handle != NULL)
    {
        /* Make sure the node corresponds to a curve. Retrieve the registry path */
        bool boolean_result = system_hash64map_get(descriptor_ptr->node_handle_to_registry_path_map,
                                                   (system_hash64) selected_item_handle,
                                                  &selected_item_registry_path);

        ASSERT_DEBUG_SYNC(boolean_result,
                          "Selected node has not been recognized.");
        if (boolean_result)
        {
            /* Now retrieve the container */
            object_manager_item item = object_manager_directory_find_item_recursive(object_manager_get_root_directory(),
                                                                                    selected_item_registry_path);

            ASSERT_DEBUG_SYNC(item != NULL,
                              "Could not retrieve item for path [%s]",
                              system_hashed_ansi_string_get_buffer(selected_item_registry_path) );

            if (item != NULL)
            {
                object_manager_object_type item_type = object_manager_item_get_type(item);

                ASSERT_DEBUG_SYNC(item_type == OBJECT_TYPE_CURVE_CONTAINER,
                                  "Item at path [%s] is not a curve!",
                                  system_hashed_ansi_string_get_buffer(selected_item_registry_path) );

                if (item_type == OBJECT_TYPE_CURVE_CONTAINER)
                {
                    result = (curve_container) object_manager_item_get_raw_pointer(item);

                    ASSERT_DEBUG_SYNC(result != NULL,
                                      "Object corresponding to path [%s] has a NULL raw pointer",
                                      system_hashed_ansi_string_get_buffer(selected_item_registry_path) );
                }
            }
        }
    }

    return result;
}

/** TODO */
PRIVATE void _curve_editor_dialog_handle_default_value_change(_curve_editor_main_window* descriptor_ptr)
{
    HWND window_handle = descriptor_ptr->curve_editbox_window_handle;

    int   text_length = ::GetWindowTextLengthA(window_handle);
    char* text_buffer = NULL;

    if (text_length > 0)
    {
        text_buffer = new (std::nothrow) char[text_length + 1];

        if (text_buffer != NULL)
        {
            memset(text_buffer,
                   0,
                   text_length + 1);

            ::GetWindowTextA(window_handle,
                             text_buffer,
                             text_length + 1);
        }
    }

    curve_container curve = _curve_editor_dialog_get_selected_curve(descriptor_ptr);

    if (curve != NULL)
    {
        system_variant_type curve_segment_type = SYSTEM_VARIANT_UNDEFINED;
        system_variant      variant            = NULL;

        curve_container_get_property(curve,
                                     CURVE_CONTAINER_PROPERTY_DATA_TYPE,
                                    &curve_segment_type);

        variant = system_variant_create(curve_segment_type);

        system_variant_set_ansi_string   (variant,
                                          text_buffer != NULL ? system_hashed_ansi_string_create                  (text_buffer) :
                                                                system_hashed_ansi_string_get_default_empty_string(),
                                          true);
        curve_container_set_default_value(curve,
                                          variant);

        system_variant_release(variant);

        /* Force a redraw */
        HTREEITEM                     selected_item_handle            = TreeView_GetSelection(descriptor_ptr->curves_tree_window_handle);
        _window_map_array_descriptor* window_map_array_descriptor_ptr = NULL;

        system_hash64map_get(descriptor_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                             (system_hash64) selected_item_handle,
                            &window_map_array_descriptor_ptr);

        if (window_map_array_descriptor_ptr         != NULL &&
            window_map_array_descriptor_ptr->window != NULL)
        {
            curve_editor_curve_window_redraw(window_map_array_descriptor_ptr->window);
        }
    }
}

/** TODO */
PRIVATE void _curve_editor_dialog_handle_edit_request(_curve_editor_main_window* descriptor_ptr)
{
    HTREEITEM node_handle = TreeView_GetSelection(descriptor_ptr->curves_tree_window_handle);

    /* Retrieve selected curve container */
    curve_container selected_curve = _curve_editor_dialog_get_selected_curve(descriptor_ptr);

    ASSERT_DEBUG_SYNC(selected_curve != NULL,
                      "Could not retrieve selected curve's container.");

    if (selected_curve != NULL)
    {
        /* Verify curve editor has not already been spawned for the dimension */
        _window_map_array_descriptor* window_map_array_descriptor = NULL;

        system_hash64map_get(descriptor_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                             (system_hash64) node_handle,
                            &window_map_array_descriptor);

        if (window_map_array_descriptor == NULL)
        {
            window_map_array_descriptor = new (std::nothrow) _window_map_array_descriptor;

            ASSERT_DEBUG_SYNC(window_map_array_descriptor != NULL,
                              "Out of memory.");

            if (window_map_array_descriptor != NULL)
            {
                window_map_array_descriptor->curve             = selected_curve;
                window_map_array_descriptor->curve_node_handle = node_handle;
                window_map_array_descriptor->window            = NULL;

                system_hash64map_insert(descriptor_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                        (system_hash64) node_handle,
                                        window_map_array_descriptor,
                                        NULL,
                                        NULL);
            }
        }

        ASSERT_DEBUG_SYNC(window_map_array_descriptor->window == NULL,
                          "Curve window has already been opened");

        if (window_map_array_descriptor->window == NULL)
        {
            window_map_array_descriptor->window = curve_editor_curve_window_show(descriptor_ptr->context,
                                                                                 selected_curve,
                                                                                 _curve_editor_dialog_on_curve_window_release,
                                                                                 (curve_editor_main_window) descriptor_ptr,
                                                                                 descriptor_ptr->serialization_cs);

            _curve_editor_dialog_update_ui_curve_edit_buttons(descriptor_ptr,
                                                              node_handle);
        }
    }
}

/** TODO */
PRIVATE bool _curve_editor_dialog_item_name_comparator(void* has_1,
                                                       void* has_2)
{
    system_hashed_ansi_string string_1 = (system_hashed_ansi_string) has_1;
    system_hashed_ansi_string string_2 = (system_hashed_ansi_string) has_2;

    return stricmp(system_hashed_ansi_string_get_buffer(string_1),
                   system_hashed_ansi_string_get_buffer(string_2) ) < 0;
}

/** TODO */
PRIVATE void _curve_editor_dialog_on_curve_window_release(void*           owner,
                                                          curve_container curve)
{
    _curve_editor_main_window* owner_ptr = (_curve_editor_main_window*) owner;

    /* Try to find the descriptor corresponding to the curve */
    bool     has_found = false;
    uint32_t n_windows = 0;

    system_hash64map_get_property(owner_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_windows);

    for (uint32_t n_window = 0;
                  n_window < n_windows;
                ++n_window)
    {
        _window_map_array_descriptor* descriptor      = NULL;
        system_hash64                 descriptor_hash = NULL;
        bool                          result          = false;

        result = system_hash64map_get_element_at(owner_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                                 n_window,
                                                &descriptor,
                                                &descriptor_hash);

        ASSERT_DEBUG_SYNC(result,
                          "Could not retrieve [%dth] element from window map array descriptor map.",
                          n_window);

        if (result)
        {
            if (descriptor->curve == curve)
            {
                /* Caller will release so no need to delete the instance */
                descriptor->window = NULL;

                if (descriptor->curve_node_handle == TreeView_GetSelection(owner_ptr->curves_tree_window_handle))
                {
                    _curve_editor_dialog_update_ui_curve_edit_buttons(owner_ptr,
                                                                      descriptor->curve_node_handle);
                }

                has_found = true;
                break;
            }
        }
    }

    ASSERT_DEBUG_SYNC(has_found, "Could not handle curve window release call-back.");
}

/** TODO */
PRIVATE void _curve_editor_dialog_thread_entrypoint(void* descriptor)
{
    _curve_editor_main_window* descriptor_ptr = (_curve_editor_main_window*) descriptor;

    ::DialogBoxParamA(_global_instance,
                      MAKEINTRESOURCE(IDD_CURVE_EDITOR_MAIN),
                      0,
                      _curve_editor_dialog_window_message_handler,
                      (LPARAM) descriptor_ptr);
}

/** TODO */
PRIVATE void _curve_editor_dialog_update_curve_tree(_curve_editor_main_window* descriptor_ptr,
                                                    HTREEITEM                  parent_node_handle,
                                                    object_manager_directory   parent_directory,
                                                    system_hashed_ansi_string  directory_registry_path)
{
    if (parent_node_handle == NULL)
    {
        /* Clean up the UI */
        TreeView_DeleteAllItems(descriptor_ptr->curves_tree_window_handle);

        /* Clean up internal data */
        system_hash64map_clear(descriptor_ptr->node_handle_to_registry_path_map);
    }

    /* Recreate sub-directory structure first - subdirectory names need to be sorted beforehand. */
    uint32_t n_subdirectories = object_manager_directory_get_amount_of_subdirectories_for_directory(parent_directory);

    if (n_subdirectories != 0)
    {
        system_resizable_vector subdirectory_names = system_resizable_vector_create(n_subdirectories);

        ASSERT_DEBUG_SYNC(subdirectory_names != NULL,
                          "Could not create subdirectory names' resizable vector.");

        if (subdirectory_names != NULL)
        {
            for (uint32_t n_subdirectory = 0;
                          n_subdirectory < n_subdirectories;
                        ++n_subdirectory)
            {
                object_manager_directory  subdirectory      = object_manager_directory_get_subdirectory_at(parent_directory,
                                                                                                           n_subdirectory);
                system_hashed_ansi_string subdirectory_name = object_manager_directory_get_name(subdirectory);

                system_resizable_vector_push(subdirectory_names,
                                             subdirectory_name);
            }

            /* Sort them */
            system_resizable_vector_sort(subdirectory_names,
                                         _curve_editor_dialog_item_name_comparator);

            /* Add to the control */
            for (uint32_t n_subdirectory = 0;
                          n_subdirectory < n_subdirectories;
                        ++n_subdirectory)
            {
                object_manager_directory  subdirectory      = NULL;
                system_hashed_ansi_string subdirectory_name = NULL;

                system_resizable_vector_get_element_at(subdirectory_names,
                                                       n_subdirectory,
                                                      &subdirectory_name);

                subdirectory = object_manager_directory_find_subdirectory(parent_directory,
                                                                          subdirectory_name);

                /* Create the directory node */
                TV_INSERTSTRUCT insert_struct;
                HTREEITEM       new_node_handle;

                insert_struct.hParent      = parent_node_handle;
                insert_struct.hInsertAfter = TVI_LAST;
                insert_struct.item.mask    = TVIF_TEXT;
                insert_struct.item.pszText = (LPSTR) system_hashed_ansi_string_get_buffer(subdirectory_name);

                new_node_handle = (HTREEITEM) ::SendDlgItemMessageA(descriptor_ptr->window_handle,
                                                                    IDC_CURVE_EDITOR_MAIN_CURVES_TREE,
                                                                    TVM_INSERTITEM,
                                                                    0,
                                                                    (LPARAM) &insert_struct);

                /* It's a tree, so do the recursion magic now. */
                const char* directory_path_strings[3] = {system_hashed_ansi_string_get_buffer(directory_registry_path),
                                                         system_hashed_ansi_string_get_buffer(subdirectory_name),
                                                         "\\"};

                _curve_editor_dialog_update_curve_tree(descriptor_ptr,
                                                       new_node_handle,
                                                       subdirectory,
                                                       system_hashed_ansi_string_create_by_merging_strings(3,
                                                                                                           directory_path_strings) );
            }

            system_resizable_vector_release(subdirectory_names);
            subdirectory_names = NULL;
        }
    }

    /* Add item nodes: first sort their names, then add in alphabetical order. */
    uint32_t                n_items    = object_manager_directory_get_amount_of_children_for_directory(parent_directory) - n_subdirectories;
    system_resizable_vector item_names = system_resizable_vector_create(n_items);

    ASSERT_DEBUG_SYNC(item_names != NULL,
                      "Could not create item names' resizable vector");

    if (item_names != NULL)
    {
        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            object_manager_item item   = NULL;
            bool                result = object_manager_directory_get_subitem_at(parent_directory,
                                                                                 n_item,
                                                                                &item);

            system_resizable_vector_push(item_names,
                                         object_manager_item_get_name(item) );
        }

        /* Sort them */
        system_resizable_vector_sort(item_names,
                                     _curve_editor_dialog_item_name_comparator);

        /* Add to the control*/
        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            system_hashed_ansi_string item_name = NULL;

            system_resizable_vector_get_element_at(item_names,
                                                   n_item,
                                                  &item_name);

            /* Insert the node */
            TV_INSERTSTRUCT insert_struct;

            insert_struct.hParent      = parent_node_handle;
            insert_struct.hInsertAfter = TVI_LAST;
            insert_struct.item.mask    = TVIF_TEXT;
            insert_struct.item.pszText = (LPSTR) system_hashed_ansi_string_get_buffer(item_name);

            HTREEITEM node_handle = (HTREEITEM) ::SendDlgItemMessageA(descriptor_ptr->window_handle,
                                                                      IDC_CURVE_EDITOR_MAIN_CURVES_TREE,
                                                                      TVM_INSERTITEM,
                                                                      0,
                                                                      (LPARAM) &insert_struct);

            system_hash64map_insert(descriptor_ptr->node_handle_to_registry_path_map,
                                    (system_hash64) node_handle,
                                    system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(directory_registry_path),
                                                                                            system_hashed_ansi_string_get_buffer(item_name) ),
                                    NULL,
                                    NULL);
        }

        system_resizable_vector_release(item_names);
    }
}

/** TODO */
PRIVATE void _curve_editor_dialog_update_ui_curve_edit_buttons(_curve_editor_main_window* descriptor_ptr,
                                                               HTREEITEM                  curve_node_handle)
{
    /* Enable "edit" controls only if a corresponding "edit curve" window is not already shown */
    _window_map_array_descriptor* descriptor = NULL;

    system_hash64map_get(descriptor_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                          (system_hash64) curve_node_handle,
                         &descriptor);

    if (descriptor != NULL)
    {
        ::EnableWindow(descriptor_ptr->edit_curve_button_window_handle,
                       descriptor->window != NULL ? TRUE : FALSE);
    }
    else
    {
        ::EnableWindow(descriptor_ptr->edit_curve_button_window_handle,
                       TRUE);
    }
}

/** TODO */
PRIVATE void _curve_editor_dialog_update_ui_curve_details(_curve_editor_main_window* descriptor_ptr,
                                                          curve_container            curve,
                                                          HTREEITEM                  curve_node_handle)
{
    system_variant_type data_variant_type = SYSTEM_VARIANT_UNDEFINED;

    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_DATA_TYPE,
                                &data_variant_type);

    /* Set name */
    system_hashed_ansi_string curve_name = NULL;

    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_NAME,
                                &curve_name);

    ::SetWindowTextA(descriptor_ptr->name_static_window_handle,
                     system_hashed_ansi_string_get_buffer(curve_name) );

    /* Set format */
    switch (data_variant_type)
    {
        case SYSTEM_VARIANT_ANSI_STRING:
        {
            ::SetWindowTextA(descriptor_ptr->format_static_window_handle,
                             "Ansi string");

            break;
            }

        case SYSTEM_VARIANT_BOOL:
        {
            ::SetWindowTextA(descriptor_ptr->format_static_window_handle,
                             "Boolean");

            break;
        }

        case SYSTEM_VARIANT_INTEGER:
        {
            ::SetWindowTextA(descriptor_ptr->format_static_window_handle,
                             "Integer");

            break;
        }

        case SYSTEM_VARIANT_FLOAT:
        {
            ::SetWindowTextA(descriptor_ptr->format_static_window_handle,
                             "Float");

            break;
        }

        default:
        {
            ::SetWindowTextA(descriptor_ptr->format_static_window_handle,
                             "Unknown (!)");

            break;
        }
    } /* switch (data_variant_type) */

    /* Set type */
    ::SetWindowTextA(descriptor_ptr->type_static_window_handle,
                     "1D");

    /* Update "edit" buttons availability. */
    _curve_editor_dialog_update_ui_curve_edit_buttons(descriptor_ptr,
                                                      curve_node_handle);

    /* Set default values */
    system_variant            default_variant     = system_variant_create(data_variant_type);
    system_hashed_ansi_string default_variant_has = NULL;

    descriptor_ptr->should_handle_en_change_notifications = false;
    {
        curve_container_get_default_value(curve,
                                          false,
                                          default_variant);
        system_variant_get_ansi_string   (default_variant,
                                           true,
                                          &default_variant_has);

        ::SetWindowTextA(descriptor_ptr->curve_editbox_window_handle,
                         system_hashed_ansi_string_get_buffer(default_variant_has) );
    }
    descriptor_ptr->should_handle_en_change_notifications = true;

    system_variant_release(default_variant);

    /* Configure default value editboxes' visibility */
    ::EnableWindow(descriptor_ptr->curve_editbox_window_handle,
                   TRUE);
    ::EnableWindow(descriptor_ptr->edit_curve_button_window_handle,
                   TRUE);
}

/** TODO */
PRIVATE void _curve_editor_dialog_update_ui(_curve_editor_main_window* descriptor_ptr)
{
    /* Retrieve selection - if it does not correspond to a valid resource path, assume no node is selected */
    bool                      has_selected_curve          = false;
    HTREEITEM                 selected_node               = TreeView_GetSelection(descriptor_ptr->curves_tree_window_handle);
    system_hashed_ansi_string selected_node_registry_path = NULL;

    if (selected_node != NULL)
    {
        /* Make sure the node corresponds to a curve. Retrieve the registry path */
        if (system_hash64map_get(descriptor_ptr->node_handle_to_registry_path_map,
                                  (system_hash64) selected_node,
                                 &selected_node_registry_path) )
        {
            has_selected_curve = true;
        }
    }

    if (has_selected_curve)
    {
        /* Enable all components */
        ::EnableWindow(descriptor_ptr->format_static_window_handle,
                       TRUE);
        ::EnableWindow(descriptor_ptr->name_static_window_handle,
                       TRUE);
        ::EnableWindow(descriptor_ptr->type_static_window_handle,
                       TRUE);
        ::EnableWindow(descriptor_ptr->curve_editbox_window_handle,
                       TRUE);

        /* Retrieve curve object in question */
        object_manager_item selected_item = object_manager_directory_find_item_recursive(object_manager_get_root_directory(),
                                                                                         selected_node_registry_path);

        ASSERT_DEBUG_SYNC(selected_item != NULL,
                          "Could not find item instance for path [%s]",
                          system_hashed_ansi_string_get_buffer(selected_node_registry_path) );

        if (selected_item != NULL)
        {
            curve_container selected_curve = (curve_container) object_manager_item_get_raw_pointer(selected_item);

            _curve_editor_dialog_update_ui_curve_details(descriptor_ptr,
                                                         selected_curve,
                                                         selected_node);
        }
        else
        {
            /* Could not retrieve curve container - disable edit buttons */
            ::EnableWindow(descriptor_ptr->edit_curve_button_window_handle,
                           FALSE);
        }
    }
    else
    {
        /* Disable all components */
        ::EnableWindow(descriptor_ptr->edit_curve_button_window_handle,
                       FALSE);
        ::EnableWindow(descriptor_ptr->format_static_window_handle,
                       FALSE);
        ::EnableWindow(descriptor_ptr->name_static_window_handle,
                       FALSE);
        ::EnableWindow(descriptor_ptr->type_static_window_handle,
                       FALSE);
        ::EnableWindow(descriptor_ptr->curve_editbox_window_handle,
                       FALSE);

        /* Reset values shown */
        ::SetWindowTextA(descriptor_ptr->format_static_window_handle,
                         "");
        ::SetWindowTextA(descriptor_ptr->name_static_window_handle,
                         "");
        ::SetWindowTextA(descriptor_ptr->type_static_window_handle,
                         "");

        /* Reset edit-box values */
        descriptor_ptr->should_handle_en_change_notifications = false;
        {
            ::SetWindowTextA(descriptor_ptr->curve_editbox_window_handle,
                              "");
        }
        descriptor_ptr->should_handle_en_change_notifications = true;
    }
}

/** TODO */
PRIVATE volatile void _curve_editor_dialog_close_button_handler(void* descriptor)
{
    _curve_editor_main_window* descriptor_ptr = (_curve_editor_main_window*) descriptor;

    curve_editor_main_window_release( (curve_editor_main_window) descriptor);
}

/** TODO */
PRIVATE INT_PTR CALLBACK _curve_editor_dialog_window_message_handler(HWND   dialog_handle,
                                                                     UINT   message_id,
                                                                     WPARAM wparam,
                                                                     LPARAM lparam)
{
    switch (message_id)
    {
        case WM_COMMAND:
        {
            _curve_editor_main_window* descriptor = (_curve_editor_main_window*) ::GetWindowLongPtr(dialog_handle,
                                                                                                    GWLP_USERDATA);

            switch (HIWORD(wparam) )
            {
                case EN_CHANGE:
                {
                    if (descriptor->should_handle_en_change_notifications)
                    {
                        switch (LOWORD(wparam) )
                        {
                            case IDC_CURVE_EDITOR_MAIN_X_CURVE:
                            {
                                _curve_editor_dialog_handle_default_value_change(descriptor);

                                break;
                            }

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Unrecognized EN_CHANGE owner");
                            }
                        } /* switch (LOWORD(wparam) ) */
                    } /* if (descriptor->should_handle_en_change_notifications) */

                    break;
                }

                case BN_CLICKED:
                {
                    switch ( LOWORD(wparam) )
                    {
                        case IDC_CURVE_EDITOR_MAIN_EDIT_CURVE:
                        {
                            _curve_editor_dialog_handle_edit_request(descriptor);

                            break;
                        }
                    } /* switch(lParam) */

                    break;
                } /* case BN_CLICKED */
            } /* switch (wParam) */

            break;
        } /* case WM_COMMAND: */

        case WM_CUSTOMIZED_SHUT_DOWN:
        {
            ::DestroyWindow(dialog_handle);

            break;
        }

        case WM_CLOSE:
        {
            /* In order to close the dialog, we need to perform the whole process from another thread. Otherwise we'll get a classical thread lock-up,
             * as curve_editor_main_window_release() blocks until the window thread exits, which can't happen from within a message handler.
             */
            _curve_editor_main_window*         descriptor = (_curve_editor_main_window*) ::GetWindowLongPtr(dialog_handle, GWLP_USERDATA);
            system_thread_pool_task_descriptor task       = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                   _curve_editor_dialog_close_button_handler,
                                                                                                                   descriptor);

            /* Disable close button first */
            ::EnableMenuItem(::GetSystemMenu(descriptor->window_handle,
                                             FALSE), /* bRevert */
                             SC_CLOSE,
                             MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

            /* Okay, pass the request to another thread */
            system_thread_pool_submit_single_task(task);
            break;
        }

        case WM_INITDIALOG:
        {
            /* Set up the window instance, so that other messages can be handled */
            ::SetClassLongPtr (dialog_handle,
                               GCLP_HICON,
                               (LONG_PTR) ::LoadIconA(0, IDI_APPLICATION) );
            ::SetWindowLongPtr(dialog_handle,
                               GWLP_USERDATA,
                               (LONG_PTR) lparam);

            /* Now that we're done, carry on and initialize the dialog using impl handler */
            _curve_editor_main_window* descriptor = (_curve_editor_main_window*) lparam;

            _curve_editor_main_window_initialize_dialog(descriptor,
                                                        dialog_handle);

            /* Configure the UI, according to registry */
            _curve_editor_dialog_update_curve_tree(descriptor,
                                                   NULL,
                                                   object_manager_get_directory(system_hashed_ansi_string_create("Curves") ),
                                                   system_hashed_ansi_string_create("\\Curves\\")
                                                  );

            /* Update UI component states */
            _curve_editor_dialog_update_ui(descriptor);

            /* Set the 'dialog created' event so the caller's thread can continue */
            system_event_set(descriptor->dialog_created_event);

            return 1;
        }

        case WM_NOTIFY:
        {
            _curve_editor_main_window* descriptor          = (_curve_editor_main_window*) ::GetWindowLongPtr(dialog_handle,
                                                                                                             GWLP_USERDATA);
            NMHDR*                     notification_header = (NMHDR*) lparam;
            
            if (notification_header->code == TVN_SELCHANGED)
            {
                _curve_editor_dialog_update_ui(descriptor);
            }

            break;
        }

        case WM_DESTROY:
        {
            ::PostQuitMessage(0);

            break;
        }
    } /* switch (message_id) */

    return 0;
}

/** TODO */
PRIVATE void _curve_editor_init_descriptor(_curve_editor_main_window*                descriptor,
                                           PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func,
                                           ogl_context                               context)
{
    descriptor->context                                                = context;
    descriptor->curve_node_handle_to_curve_window_map_array_descriptor = system_hash64map_create(sizeof(_window_map_array_descriptor*) );
    descriptor->curves_tree_window_handle                              = NULL;
    descriptor->dialog_created_event                                   = system_event_create(true); /* manual_reset */
    descriptor->edit_curve_button_window_handle                        = NULL;
    descriptor->format_static_window_handle                            = NULL;
    descriptor->name_static_window_handle                              = NULL;
    descriptor->node_handle_to_registry_path_map                       = system_hash64map_create(sizeof(system_hashed_ansi_string) );
    descriptor->on_release_callback_handler_func                       = on_release_callback_handler_func;
    descriptor->serialization_cs                                       = system_critical_section_create();
    descriptor->should_handle_en_change_notifications                  = true;
    descriptor->type_static_window_handle                              = NULL;
    descriptor->curve_editbox_window_handle                            = NULL;
    descriptor->watchdog                                               = curve_editor_watchdog_create();
    descriptor->window_handle                                          = NULL;
}

/** TODO */
PRIVATE void _curve_editor_main_window_initialize_dialog(_curve_editor_main_window* descriptor,
                                                         HWND                       dialog_handle)
{
    ASSERT_DEBUG_SYNC(dialog_handle != NULL,
                      "Reported dialog handle is null!");

    descriptor->window_handle = dialog_handle;

    descriptor->curves_tree_window_handle       = ::GetDlgItem(dialog_handle,
                                                               IDC_CURVE_EDITOR_MAIN_CURVES_TREE);
    descriptor->edit_curve_button_window_handle = ::GetDlgItem(dialog_handle,
                                                               IDC_CURVE_EDITOR_MAIN_EDIT_CURVE);
    descriptor->format_static_window_handle     = ::GetDlgItem(dialog_handle,
                                                               IDC_CURVE_EDITOR_MAIN_FORMAT);
    descriptor->name_static_window_handle       = ::GetDlgItem(dialog_handle,
                                                               IDC_CURVE_EDITOR_MAIN_NAME);
    descriptor->type_static_window_handle       = ::GetDlgItem(dialog_handle,
                                                               IDC_CURVE_EDITOR_MAIN_TYPE);
    descriptor->curve_editbox_window_handle     = ::GetDlgItem(dialog_handle,
                                                               IDC_CURVE_EDITOR_MAIN_X_CURVE);

    if (descriptor->curves_tree_window_handle       == NULL ||
        descriptor->edit_curve_button_window_handle == NULL ||
        descriptor->format_static_window_handle     == NULL ||
        descriptor->name_static_window_handle       == NULL ||
        descriptor->type_static_window_handle       == NULL ||
        descriptor->curve_editbox_window_handle     == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "At least one of the window handles could not have been retrieved.");
    }
}

/** TODO */
PRIVATE void _curve_editor_main_window_on_curves_changed_callback(const void* callback_data,
                                                                        void* user_arg)
{
    _curve_editor_main_window* main_window_ptr = (_curve_editor_main_window*) user_arg;

    curve_editor_watchdog_signal(main_window_ptr->watchdog);
}

/* Please see header for specification */
PUBLIC curve_editor_main_window curve_editor_main_window_create(PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC on_release_callback_handler_func,
                                                                ogl_context                               context)
{
    /* Create result container */
    _curve_editor_main_window* result = new (std::nothrow) _curve_editor_main_window;

    if (result != NULL)
    {
        /* Fill with default values */
        _curve_editor_init_descriptor(result,
                                      on_release_callback_handler_func,
                                      context);

        /* Retain the context - we need to cache it */
        ogl_context_retain(context);

        /* Dialog must be hosted within a thread of its own. Therefore, create a new thread, launch the dialog in there and wait till it
         * notifies us stuff is set up.
         */
        system_threads_spawn(_curve_editor_dialog_thread_entrypoint,
                             result,
                            &result->dialog_thread_event);

        /* Block till everything is ready */
        system_event_wait_single(result->dialog_created_event);

        /* Sign for the call-backs */
        system_callback_manager_subscribe_for_callbacks(system_callback_manager_get(),
                                                        CALLBACK_ID_CURVE_CONTAINER_ADDED,
                                                        CALLBACK_SYNCHRONICITY_ASYNCHRONOUS,
                                                        _curve_editor_main_window_on_curves_changed_callback,
                                                        result); /* callback_proc_user_arg */

        system_callback_manager_subscribe_for_callbacks(system_callback_manager_get(),
                                                        CALLBACK_ID_CURVE_CONTAINER_DELETED,
                                                        CALLBACK_SYNCHRONICITY_ASYNCHRONOUS,
                                                        _curve_editor_main_window_on_curves_changed_callback,
                                                        result); /* callback_proc_user_arg */
    }

    return (curve_editor_main_window) result;
}


/* Please see header for specification */
PUBLIC void curve_editor_main_window_release(curve_editor_main_window main_window)
{
    _curve_editor_main_window* main_window_ptr = (_curve_editor_main_window*) main_window;

    /* Sign out of the call-backs before we continue with actual destruction */
    system_callback_manager_unsubscribe_from_callbacks(system_callback_manager_get(),
                                                       CALLBACK_ID_CURVE_CONTAINER_ADDED,
                                                       _curve_editor_main_window_on_curves_changed_callback,
                                                       main_window_ptr);

    system_callback_manager_unsubscribe_from_callbacks(system_callback_manager_get(),
                                                       CALLBACK_ID_CURVE_CONTAINER_DELETED,
                                                       _curve_editor_main_window_on_curves_changed_callback,
                                                       main_window_ptr);

    /* Release the watchdog */
    curve_editor_watchdog_release(main_window_ptr->watchdog);

    /* Call back release handler */
    main_window_ptr->on_release_callback_handler_func();

    /* Release sub-windows */
    if (main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor != NULL)
    {
        while (true)
        {
            uint32_t n_items = 0;

            system_hash64map_get_property(main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_items);

            if (n_items == 0)
            {
                break;
            }

            _window_map_array_descriptor* descriptor      = NULL;
            system_hash64                 descriptor_hash = 0;

            system_hash64map_get_element_at(main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                            0, /* index */
                                           &descriptor,
                                           &descriptor_hash);

            ASSERT_DEBUG_SYNC(descriptor != NULL,
                              "Retrieved a NULL subwindow");

            if (descriptor != NULL)
            {
                if (descriptor->window != NULL)
                {
                    curve_editor_curve_window_hide(descriptor->window);
                }

                delete descriptor;
                descriptor = NULL;

                system_hash64map_remove(main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                        descriptor_hash);
            }
        }
    } /* if (main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor != NULL) */

    /* Release the context - we are cool if it was to be released now */
    ogl_context_release(main_window_ptr->context);

    /* If the window is up, get rid of it */
    if (main_window_ptr->window_handle != NULL)
    {
        ::SendMessageA(main_window_ptr->window_handle,
                       WM_CUSTOMIZED_SHUT_DOWN,
                       0,
                       0);

        system_event_wait_single(main_window_ptr->dialog_thread_event);
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

    if (main_window_ptr->node_handle_to_registry_path_map != NULL)
    {
        system_hash64map_release(main_window_ptr->node_handle_to_registry_path_map);

        main_window_ptr->node_handle_to_registry_path_map = NULL;
    }

    if (main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor != NULL)
    {
        system_hash64map_release(main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor);

        main_window_ptr->curve_node_handle_to_curve_window_map_array_descriptor = NULL;
    }

    if (main_window_ptr->serialization_cs != NULL)
    {
        system_critical_section_release(main_window_ptr->serialization_cs);

        main_window_ptr->serialization_cs = NULL;
    }

    delete main_window_ptr;
    main_window_ptr = NULL;
}

/* Please see header for specification */
PUBLIC void curve_editor_main_window_set_property(curve_editor_main_window          window,
                                                  curve_editor_main_window_property property,
                                                  void*                             data)
{
    _curve_editor_main_window* window_ptr = (_curve_editor_main_window*) window;

    system_critical_section_enter(window_ptr->serialization_cs);
    {
        switch (property)
        {
            case CURVE_EDITOR_MAIN_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH:
            {
                uint32_t n_windows = 0;

                system_hash64map_get_property(window_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_windows);

                for (uint32_t n_window = 0;
                              n_window < n_windows;
                            ++n_window)
                {
                    _window_map_array_descriptor* entry_ptr = NULL;

                    if (system_hash64map_get_element_at(window_ptr->curve_node_handle_to_curve_window_map_array_descriptor,
                                                        n_window,
                                                       &entry_ptr,
                                                        NULL) )
                    {
                        curve_editor_curve_window_set_property(entry_ptr->window,
                                                               CURVE_EDITOR_CURVE_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH,
                                                               data);
                    } /* if (entry descriptor was found) */

                } /* for (all spawned windows) */

                break;
            } /* case CURVE_EDITOR_MAIN_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH: */

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized curve_editor_main_window_property value");
            }
        } /* switch (property) */
    }
    system_critical_section_leave(window_ptr->serialization_cs);
}

/* Please see header for specification */
PUBLIC void curve_editor_main_window_update_curve_list(curve_editor_main_window window)
{
    _curve_editor_main_window* window_ptr = (_curve_editor_main_window*) window;

    /* Unselect any item that may have been selected */
    TreeView_Select(window_ptr->curves_tree_window_handle,
                    NULL,
                    TVGN_CARET);

    /* Re-create the treeview */
    _curve_editor_dialog_update_curve_tree(window_ptr,
                                           NULL,
                                           object_manager_get_directory(system_hashed_ansi_string_create("Curves") ),
                                           system_hashed_ansi_string_create("\\Curves\\")
                                          );
}