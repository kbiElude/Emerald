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
PUBLIC EMERALD_API ogl_ui_bag ogl_ui_bag_create(__in                    __notnull ogl_ui                ui,
                                                __in_ecount(2)          __notnull const float*          x1y1,
                                                __in                              unsigned int          n_controls,
                                                __in_ecount(n_controls) __notnull const ogl_ui_control* controls);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_bag_release(__in __notnull ogl_ui_bag bag);


#endif /* OGL_UI_BAG_H */
