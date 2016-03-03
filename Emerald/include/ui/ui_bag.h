/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 * A UI bag lays out UI controls & ensures that they do not overlap.
 * It also provides a frame control to ensure the background is dimmed.
 *
 */
#ifndef UI_BAG_H
#define UI_BAG_H

#include "system/system_types.h"
#include "ui/ui_types.h"


/** TODO */
PUBLIC EMERALD_API ui_bag ui_bag_create(ui                ui_instance,
                                        const float*      x1y1,
                                        unsigned int      n_controls,
                                        const ui_control* controls);

/** TODO */
PUBLIC EMERALD_API void ui_bag_release(ui_bag bag);


#endif /* UI_BAG_H */
