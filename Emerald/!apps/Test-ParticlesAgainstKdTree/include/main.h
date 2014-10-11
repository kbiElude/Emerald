/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef MAIN_H
#define MAIN_H

#include "system/system_types.h"
#include "ocl/ocl_kdtree.h"

/** TODO */
PUBLIC ocl_context main_get_ocl_context();

/** TODO */
PUBLIC system_matrix4x4 main_get_projection_matrix();

/** TODO */
PUBLIC scene main_get_scene();

/** TODO */
PUBLIC ocl_kdtree main_get_scene_kdtree();

/** TODO */
PUBLIC mesh main_get_scene_mesh();

#endif /* MAIN_H */
