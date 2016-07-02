/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 * Generates a circular 2D mesh spanned on a <-1, -1> x <1, 1> rectangle.
 *
 */
#ifndef PROCEDURAL_MESH_CIRCLE_H
#define PROCEDURAL_MESH_CIRCLE_H

#include "procedural/procedural_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_mesh_circle,
                             procedural_mesh_circle)

typedef enum
{
    /* not settable, ral_buffer */
    PROCEDURAL_MESH_CIRCLE_PROPERTY_NONINDEXED_BUFFER,

    /* not settable, uint32_t */
    PROCEDURAL_MESH_CIRCLE_PROPERTY_NONINDEXED_BUFFER_VERTEX_DATA_OFFSET,

    /* not settable, unsigned int */
    PROCEDURAL_MESH_CIRCLE_PROPERTY_N_VERTICES,
} procedural_mesh_circle_property;


/** Generates a buffer storage holding a representation of a 2D sphere.
 *
 * The generated geometry is a 2D triangle fan.
 * The coordinates are stored as 32-bit floats.
 * The generator only supports DATA_BO_ARRAYS mode. Indexed representation is inefficient in case of this mesh.
 * No alignment guarantees are currently made.
 *
 * @param context      Rendering context.
 * @param data_bitmask Must be DATA_BO_ARRAYS.
 * @param n_segments   Defines how many segments should the circle's perimeter be divided into. The larger this
 *                     value is, the more detailed approximation will be generated. Must be at least 4.
 * @param name         Name to use for the object instance.
 *
 * @return Requested instance.
 **/
PUBLIC EMERALD_API procedural_mesh_circle procedural_mesh_circle_create(ral_context                    context,
                                                                        procedural_mesh_data_type_bits mesh_data_types,
                                                                        uint32_t                       n_segments,
                                                                        system_hashed_ansi_string      name);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_circle_get_property(procedural_mesh_circle          circle,
                                                            procedural_mesh_circle_property property,
                                                            void*                           out_result);

#endif /* PROCEDURAL_MESH_CIRCLE_H */
