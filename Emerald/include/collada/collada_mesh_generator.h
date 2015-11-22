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
#include "ral/ral_types.h"

/** TODO */
PUBLIC mesh collada_mesh_generator_create(ral_context  context,
                                          collada_data data,
                                          unsigned int n_geometry);

#endif /* COLLADA_MESH_GENERATOR_H */
