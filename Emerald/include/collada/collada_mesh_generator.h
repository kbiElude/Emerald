/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 *
 * Used by collada_data to form an Emerald mesh instance.
 */
#ifndef COLLADA_MESH_GENERATOR_H
#define COLLADA_MESH_GENERATOR_H

#include "collada/collada_types.h"
#include "mesh/mesh_types.h"

/** TODO */
PUBLIC mesh collada_mesh_generator_create(__in __notnull ogl_context  context,
                                          __in __notnull collada_data data,
                                          __in __notnull unsigned int n_geometry);

#endif /* COLLADA_MESH_GENERATOR_H */
