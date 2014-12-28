/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_UI_H
#define PLUGIN_UI_H

#include "system/system_types.h"

extern system_event ui_initialized_event;


/** TODO */
PUBLIC void DeinitUI();

/** TODO */
PUBLIC void InitUI();

/** TODO */
PUBLIC void SetActivityDescription(__in __notnull char* text);

#endif /* PLUGIN_UI_H */