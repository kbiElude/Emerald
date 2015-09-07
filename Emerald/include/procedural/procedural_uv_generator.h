/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Procedural texture coordinate generator. Uses compute shaders to calculate texture coordinate data streams
 * and configures mesh, specified at creation time, to use the maintained data stream.
 *
 * Since the stream data is calculated completely on GPU, the texture coordinate stream can be updated many times
 * per frame.
 *
 */
#ifndef PROCEDURAL_UV_GENERATOR_H
#define PROCEDURAL_UV_GENERATOR_H

#include "mesh/mesh_types.h"
#include "procedural/procedural_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_uv_generator,
                             procedural_uv_generator)

typedef enum
{
    /* Texture coordinates based on the angle of surface at each vertex.
     *
     * Requires normal data to be stored in VRAM for the specified layer ID
     * at _create() and _update() call times.
     */
    PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS,
} procedural_uv_generator_type;


/** TODO.
 *
 *  The call does NOT add the texture coordinates data stream to the specified mesh.
 *  The stream, sourced from buffer memory, will be added at the next _update() call time.
 *
 *  @param in_cntext        Rendering context to use. Must not be NULL. The contxt will be retained
 *                          and released at tear-down time.
 *  @param in_mesh          TODO. The object will be retained and released at
 *                          UV generator's tear-down time. Must not be NULL.
 *  @param in_mesh_layer_id ID of the mesh layer to add the texture coordinates data stream to.
 *  @param type             TODO
 *  @param name             TODO
 *
 *  @return Object instance.
 ***/
PUBLIC EMERALD_API procedural_uv_generator procedural_uv_generator_create(ogl_context                  in_context,
                                                                          mesh                         in_mesh,
                                                                          mesh_layer_id                in_mesh_layer_id,
                                                                          procedural_uv_generator_type type,
                                                                          system_hashed_ansi_string    name);

/** TODO.
 *
 *  The call adds a texture coordinates data stream to the owned mesh layer, if this is the first
 *  _update() call made for the @param generator instance.
 *
 *  @param generator UV generator instance to update the texture coordinate data stream for.
 *
 */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void procedural_uv_generator_update(procedural_uv_generator generator);

#endif /* PROCEDURAL_UV_GENERATOR_H */
