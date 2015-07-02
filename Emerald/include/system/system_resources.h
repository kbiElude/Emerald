/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_RESOURCES_H
#define SYSTEM_RESOURCES_H

#include "gfx/gfx_types.h"
#include "system/system_types.h"


#ifdef _WIN32

/* TODO */
PUBLIC EMERALD_API void* system_resources_get_exe_resource(__in int resource_id,
                                                           __in int resource_type);

#endif

/* TODO */
PUBLIC EMERALD_API gfx_bfg_font_table system_resources_get_meiryo_font_table();

/* TODO */
PUBLIC void _system_resources_deinit();

#endif /* SYSTEM_RESOURCES_H */