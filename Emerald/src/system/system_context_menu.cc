/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_context_menu.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/* Start-up amount of items allocated for a context menu */
#define N_BASE_ITEMS (4)


/* Type definitions */
typedef enum
{
    CONTEXT_MENU_ENTRY_ITEM,
    CONTEXT_MENU_ENTRY_MENU,
    CONTEXT_MENU_ENTRY_SEPARATOR
} _system_context_menu_entry_type;

typedef struct
{
    int                             id;
    _system_context_menu_entry_type type;

    /* item-specific */
    PFNCONTEXTMENUCALLBACK                item_callback_func;
    system_context_menu_callback_argument item_callback_func_argument;
    bool                                  item_checked;
    bool                                  item_enabled;
    system_hashed_ansi_string             item_name;

    /* menu-specific */
    system_context_menu menu;

} _system_context_menu_entry;

typedef struct
{
    system_resizable_vector entries;
    
    bool  has_been_shown;
    HMENU system_handle;

    REFCOUNT_INSERT_VARIABLES
} _system_context_menu;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(system_context_menu,
                               system_context_menu,
                              _system_context_menu);


/* Forward declarations */
PRIVATE bool         _system_context_menu_call_back_owner                       (__in __notnull _system_context_menu*       menu_ptr,
                                                                                 __in           int                         id);
PRIVATE MENUITEMINFO _system_context_menu_create_item_system_representation     (__in __notnull _system_context_menu_entry* entry_ptr,
                                                                                 __in           int                         id);
PRIVATE void         _system_context_menu_create_menu_system_representation     (__in __notnull _system_context_menu*       menu_ptr);
PRIVATE MENUITEMINFO _system_context_menu_create_separator_system_representation(__in __notnull _system_context_menu_entry* entry_ptr,
                                                                                 __in           int                         id);
PRIVATE MENUITEMINFO _system_context_menu_create_submenu_system_representation  (__in __notnull _system_context_menu_entry* entry_ptr,
                                                                                 __in           int                         id);

/* Private functions */
/** TODO */
PRIVATE bool _system_context_menu_call_back_owner(__in __notnull _system_context_menu* menu_ptr,
                                                  __in           int                   id)
{
    bool         result    = false;
    unsigned int n_entries = system_resizable_vector_get_amount_of_elements(menu_ptr->entries);

    for (unsigned int n = 0;
                      n < n_entries;
                    ++n)
    {
        _system_context_menu_entry* entry_ptr = NULL;

        system_resizable_vector_get_element_at(menu_ptr->entries,
                                               n,
                                              &entry_ptr);

        if (entry_ptr != NULL)
        {
            if (entry_ptr->type == CONTEXT_MENU_ENTRY_ITEM &&
                entry_ptr->id   == id)
            {
                entry_ptr->item_callback_func(entry_ptr->item_callback_func_argument);

                result = true;
                break;
            }
            else
            if (entry_ptr->type == CONTEXT_MENU_ENTRY_MENU)
            {
                result = _system_context_menu_call_back_owner((_system_context_menu*) entry_ptr->menu,
                                                              id);

                if (result)
                {
                    break;
                }
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve %dth context menu entry.",
                      n);
        }
    }

    return result;
}

/** TODO */
PRIVATE MENUITEMINFO _system_context_menu_create_item_system_representation(__in __notnull _system_context_menu_entry* entry_ptr,
                                                                            __in           int                         id)
{
    MENUITEMINFO result;

    memset(&result,
           0,
           sizeof(MENUITEMINFO) );

    result.cbSize     = sizeof(MENUITEMINFO);
    result.cch        =         system_hashed_ansi_string_get_length(entry_ptr->item_name);
    result.dwTypeData = (char*) system_hashed_ansi_string_get_buffer(entry_ptr->item_name);
    result.fMask      = MIIM_ID | MIIM_TYPE;
    result.fType      = MFT_STRING;
    result.wID        = id;

    if (entry_ptr->item_checked)
    {
        result.fMask |= MIIM_STATE;
        result.fType |= MFT_RADIOCHECK;
        result.fState = MFS_CHECKED;
    }

    if (!entry_ptr->item_enabled)
    {
        result.fMask  |= MIIM_STATE;
        result.fState |= MF_DISABLED;
    }

    return result;
}

/** TODO */
PRIVATE void _system_context_menu_create_menu_system_representation(__in __notnull _system_context_menu* menu_ptr)
{
    int          counter   = 1;
    unsigned int n_entries = system_resizable_vector_get_amount_of_elements(menu_ptr->entries);

    menu_ptr->system_handle = ::CreatePopupMenu();

    for (unsigned int n = 0;
                      n < n_entries;
                    ++n)
    {
        _system_context_menu_entry* entry_ptr = NULL;

        if (system_resizable_vector_get_element_at(menu_ptr->entries,
                                                   n,
                                                  &entry_ptr) )
        {
            entry_ptr->id = counter++;

            switch (entry_ptr->type)
            {
                case CONTEXT_MENU_ENTRY_ITEM:
                {
                    MENUITEMINFO menu_item_info = _system_context_menu_create_item_system_representation(entry_ptr,
                                                                                                         entry_ptr->id);

                    if (::InsertMenuItemA(menu_ptr->system_handle,
                                          0xFFFF /* append mode */,
                                          FALSE, /* fByPosition */
                                         &menu_item_info) == 0)
                    {
                        LOG_ERROR("Could not insert item to context menu.");
                    }

                    break;
                }

                case CONTEXT_MENU_ENTRY_MENU:
                {
                    MENUITEMINFO menu_item_info = _system_context_menu_create_submenu_system_representation(entry_ptr,
                                                                                                            entry_ptr->id);

                    if (::InsertMenuItemA(menu_ptr->system_handle,
                                          0xFFFF /* append mode */,
                                          FALSE, /* fByPosition  */
                                         &menu_item_info) == 0)
                    {
                        LOG_ERROR("Could not insert menu to context menu.");
                    }

                    break;
                }

                case CONTEXT_MENU_ENTRY_SEPARATOR:
                {
                    MENUITEMINFO menu_item_info = _system_context_menu_create_separator_system_representation(entry_ptr,
                                                                                                              entry_ptr->id);
                    
                    if (::InsertMenuItemA(menu_ptr->system_handle,
                                          0xFFFF /* append mode */,
                                          FALSE, /* fByPosition */
                                         &menu_item_info) == 0)
                    {
                        LOG_ERROR("Could not insert separator to context menu.");
                    }

                    break;
                }

                default:
                {
                    LOG_ERROR("Unrecognized context menu entry type [%d]",
                              entry_ptr->type);

                    ASSERT_DEBUG_SYNC(false,
                                      "");
                }
            } /* switch (entry_ptr->type) */
        } /* if (entry exists) */
        else
        {
            LOG_ERROR("Could not retrieve %dth entry of context menu.",
                      n);
        }
    }
}

/** TODO */
PRIVATE MENUITEMINFO _system_context_menu_create_separator_system_representation(__in __notnull _system_context_menu_entry* entry_ptr,
                                                                                 __in           int                         id)
{
    MENUITEMINFO result;

    memset(&result,
           0,
           sizeof(MENUITEMINFO));

    result.cbSize  = sizeof(MENUITEMINFO);
    result.fMask   = MIIM_TYPE;
    result.fType   = MFT_SEPARATOR;

    return result;
}

/** TODO */
PRIVATE MENUITEMINFO _system_context_menu_create_submenu_system_representation(__in __notnull _system_context_menu_entry* entry_ptr,
                                                                               __in           int                         id)
{
    MENUITEMINFO          result;
    _system_context_menu* submenu_ptr = (_system_context_menu*) entry_ptr->menu;
    
    _system_context_menu_create_menu_system_representation(submenu_ptr);

    memset(&result,
           0,
           sizeof(MENUITEMINFO) );

    result.cbSize     = sizeof(MENUITEMINFO);
    result.cch        =         system_hashed_ansi_string_get_length(entry_ptr->item_name);
    result.dwTypeData = (char*) system_hashed_ansi_string_get_buffer(entry_ptr->item_name);
    result.fMask      = MIIM_ID | MIIM_SUBMENU | MIIM_TYPE;
    result.fType      = MFT_STRING;
    result.hSubMenu   = submenu_ptr->system_handle;
    result.wID        = id;

    return result;
}

/** TODO */
PUBLIC void _system_context_menu_release(__in __notnull __post_invalid void* menu)
{
    _system_context_menu* menu_ptr  = (_system_context_menu*) menu;
    unsigned int          n_entries = system_resizable_vector_get_amount_of_elements(menu_ptr->entries);

    for (unsigned int n = 0;
                      n < n_entries;
                    ++n)
    {
        _system_context_menu_entry* menu_entry_ptr = NULL;

        system_resizable_vector_get_element_at(menu_ptr->entries,
                                               n,
                                              &menu_entry_ptr);

        if (menu_entry_ptr != NULL)
        {
            /* Sub-menus need to be released, too */
            if (menu_entry_ptr->type == CONTEXT_MENU_ENTRY_MENU)
            {
                _system_context_menu_release( (system_context_menu) menu_entry_ptr);
            }
        }
    }

    /* Free system handle for the menu */
    ::DestroyMenu(menu_ptr->system_handle);

    /* Release the vector */
    system_resizable_vector_release(menu_ptr->entries);
    menu_ptr->entries = NULL;

    /* context_menu will be released automatically */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_context_menu_append_item(__in __notnull   system_context_menu                   menu,
                                                        __in __notnull   system_hashed_ansi_string             name,
                                                        __in __notnull   PFNCONTEXTMENUCALLBACK                callback_func,
                                                        __in __maybenull system_context_menu_callback_argument callback_func_argument,
                                                                         bool                                  is_checked,
                                                                         bool                                  is_enabled)
{
    _system_context_menu*       menu_ptr  = (_system_context_menu*) menu;
    _system_context_menu_entry* new_entry = new (std::nothrow) _system_context_menu_entry;

    ASSERT_DEBUG_SYNC(new_entry != NULL,
                      "Out of memory while allocating space for context menu entry.");

    if (new_entry != NULL)
    {
        new_entry->type                        = CONTEXT_MENU_ENTRY_ITEM;
        new_entry->item_callback_func          = callback_func;
        new_entry->item_callback_func_argument = callback_func_argument;
        new_entry->item_checked                = is_checked;
        new_entry->item_enabled                = is_enabled;
        new_entry->item_name                   = name;

        system_resizable_vector_push(menu_ptr->entries,
                                     new_entry);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void context_menu_append_menu(__in __notnull system_context_menu menu,
                                                 __in __notnull system_context_menu menu_to_append)
{
    _system_context_menu*       menu_ptr  = (_system_context_menu*) menu;
    _system_context_menu_entry* new_entry = new (std::nothrow) _system_context_menu_entry;

    ASSERT_DEBUG_SYNC(new_entry != NULL,
                      "Out of memory while allocating space for context menu entry.");

    if (new_entry != NULL)
    {
        new_entry->type = CONTEXT_MENU_ENTRY_MENU;
        new_entry->menu = menu_to_append;

        system_resizable_vector_push(menu_ptr->entries,
                                     new_entry);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_context_menu_append_separator(__in __notnull system_context_menu menu)
{
    _system_context_menu*       menu_ptr  = (_system_context_menu*) menu;
    _system_context_menu_entry* new_entry = new (std::nothrow) _system_context_menu_entry;

    ASSERT_DEBUG_SYNC(new_entry != NULL,
                      "Out of memory while allocating space for context menu entry.");

    if (new_entry != NULL)
    {
        new_entry->type = CONTEXT_MENU_ENTRY_SEPARATOR;

        system_resizable_vector_push(menu_ptr->entries,
                                     new_entry);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_context_menu system_context_menu_create(__in __notnull system_hashed_ansi_string name)
{
    _system_context_menu* instance = new (std::nothrow) _system_context_menu;

    ASSERT_DEBUG_SYNC(instance != NULL,
                      "Out of memory while allocating space for context menu");

    if (instance != NULL)
    {
        instance->entries        = system_resizable_vector_create(N_BASE_ITEMS);
        instance->has_been_shown = false;

        ASSERT_DEBUG_SYNC(instance->entries != NULL,
                          "Out of memory while allocating space for context menu entries");

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(instance, 
                                                       _system_context_menu_release,
                                                       OBJECT_TYPE_CONTEXT_MENU, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Context Menus\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name) )
                                                      );
    }

    return (system_context_menu) instance;
}

/** TODO */
PUBLIC void system_context_menu_show(__in __notnull system_context_menu  menu,
                                     __in           system_window_handle parent_window_handle,
                                     __in           int                  x,
                                     __in           int                  y)
{
    int                   id_clicked = 0;
    _system_context_menu* menu_ptr   = (_system_context_menu*) menu;

    if (!menu_ptr->has_been_shown)
    {
        _system_context_menu_create_menu_system_representation(menu_ptr);

        if ( (id_clicked = ::TrackPopupMenu(menu_ptr->system_handle,
                                            TPM_LEFTALIGN | TPM_RETURNCMD,
                                            x,
                                            y,
                                            0 /* reserved */,
                                            parent_window_handle,
                                            NULL /* no specific rect */)) > 0)
        {
            _system_context_menu_call_back_owner(menu_ptr,
                                                 id_clicked);
        }

        menu_ptr->has_been_shown = true;
    }
    else
    {
        LOG_ERROR("Cannot show context menu - a context menu can only be shown once per instance.");
    }
}
