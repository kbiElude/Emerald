/**
 *
 * Emerald (kbi/elude @2015)
 *
 * A UI bag lays out UI controls & ensures that they do not overlap.
 * It also provides a frame control to ensure the background is dimmed.
 *
 */
#ifndef OGL_UI_BAG_H
#define OGL_UI_BAG_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API ogl_ui_bag ogl_ui_bag_create(ogl_ui                ui,
                                                const float*          x1y1,
                                                unsigned int          n_controls,
                                                const ogl_ui_control* controls);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_bag_release(ogl_ui_bag bag);


#endif /* OGL_UI_BAG_H */
