/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef CURVE_EDITOR_GENERAL_H
#define CURVE_EDITOR_GENERAL_H

#include "ogl/ogl_types.h"

typedef enum
{
    CURVE_EDITOR_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH /* float, settable. expressed in seconds. */
} curve_editor_property;


/** TODO */
PUBLIC void _curve_editor_deinit();

/** TODO */
PUBLIC void _curve_editor_init();

/** TODO */
PUBLIC EMERALD_API void curve_editor_set_property(__in __notnull ogl_context           context,
                                                  __in           curve_editor_property property,
                                                  __in __notnull void*                 data);

/** TODO */
PUBLIC EMERALD_API bool curve_editor_show(ogl_context);

/** TODO */
PUBLIC EMERALD_API bool curve_editor_hide();

#endif /* CURVE_EDITOR_GENERAL_H */
