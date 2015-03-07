/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef PLUGIN_PACK_H
#define PLUGIN_PACK_H

#include "system/system_types.h"

/** TODO */
PUBLIC void AddFileToFinalBlob(__in __notnull system_hashed_ansi_string filename);

/** TODO */
PUBLIC void DeinitPackData();

/** TODO */
PUBLIC void InitPackData();

/** TODO */
PUBLIC void SaveFinalBlob(__in __notnull system_hashed_ansi_string packed_scene_filename);

#endif /* PLUGIN_PACK_H */