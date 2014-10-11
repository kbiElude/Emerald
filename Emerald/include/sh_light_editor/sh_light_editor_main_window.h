/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SH_LIGHT_EDITOR_MAIN_WINDOW_H
#define SH_LIGHT_EDITOR_MAIN_WINDOW_H

#include "ogl/ogl_types.h"
#include "sh_light_editor/sh_light_editor_types.h"

/** TODO */
PUBLIC sh_light_editor_main_window sh_light_editor_main_window_create(PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC pfn_callback, 
                                                                      __in __notnull                            ogl_context);

/** TODO */
PUBLIC void sh_light_editor_main_window_refresh(__in __notnull sh_light_editor_main_window);

/** TODO */
PUBLIC void sh_light_editor_main_window_release(__in __notnull __post_invalid sh_light_editor_main_window);

#endif /* SH_LIGHT_EDITOR_MAIN_WINDOW_H */
