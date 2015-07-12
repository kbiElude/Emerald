/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef CURVE_EDITOR_H
#define CURVE_EDITOR_H

#include "curve_editor/curve_editor_types.h"
#include "system/system_types.h"


/** TODO */
PUBLIC curve_editor_watchdog curve_editor_watchdog_create();

/** TODO */
PUBLIC void curve_editor_watchdog_release(curve_editor_watchdog watchdog);

/** TODO */
PUBLIC void curve_editor_watchdog_signal(curve_editor_watchdog watchdog);


#endif /* CURVE_EDITOR_WATCHDOG_H */
