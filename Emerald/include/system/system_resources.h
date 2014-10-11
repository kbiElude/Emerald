/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_UNROLLED_LINKED_LIST_H
#define SYSTEM_UNROLLED_LINKED_LIST_H

#include "gfx/gfx_types.h"
#include "system/system_types.h"

/* TODO */
PUBLIC EMERALD_API void* system_resources_get_exe_resource(__in int resource_id, __in int resource_type);

/* TODO */
PUBLIC EMERALD_API gfx_bfg_font_table system_resources_get_meiryo_font_table();

/* TODO */
PUBLIC void _system_resources_deinit();

#endif /* SYSTEM_UNROLLED_LINKED_LIST_H */