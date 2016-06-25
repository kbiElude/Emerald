#ifndef RAGL_DEP_TRACKER_H
#define RAGL_DEP_TRACKER_H

#include "raGL/raGL_types.h"
#include "ral/ral_types.h"


/** TODO */
PUBLIC raGL_dep_tracker raGL_dep_tracker_create();

/** TODO */
PUBLIC bool raGL_dep_tracker_is_dirty(raGL_dep_tracker tracker,
                                      void*            object,
                                      GLbitfield       barrier_bits);

/** TODO */
PUBLIC void raGL_dep_tracker_mark_as_dirty(raGL_dep_tracker tracker,
                                           void*            object);

/** TODO */
PUBLIC void raGL_dep_tracker_release(raGL_dep_tracker tracker);

/** TODO */
PUBLIC void raGL_dep_tracker_reset(raGL_dep_tracker tracker);


#endif /* RAGL_DEP_TRACKER_H */