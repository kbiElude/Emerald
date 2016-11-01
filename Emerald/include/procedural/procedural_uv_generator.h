/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 * Procedural texture coordinate generator. Uses a compute shader to calculate texture coordinate data stream contents
 * and updates mesh configuration to use the maintained data buffer(s).
 *
 * Since the stream data is calculated completely on GPU, the texture coordinate stream can be updated many times
 * per frame with no significant performance cost.
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
    /* Texture coordinates are created by calculating distance from each vertex to a plane, whose normal is defined
     * via a procedural_uv_generator parameter, separately for U and V components.
     *
     * This is nonaccidentally similar to the good ol' GL_OBJECT_LINEAR known from OpenGL Compatibility Profile.
     *
     * Requires vertex data to be stored in buffer memory for the specified layer ID
     * at _update() time. If the number of vertices defined for the mesh layer increases,
     * a buffer memory alloc may be needed.
     */
    PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR,

    /* Texture coordinates created by projecting a vector from the center of the object outward through each vertex,
     * and reusing XY components of that vector, after it is normalized.
     *
     * Requires vertex data to be stored in buffer memory for the specified layer ID
     * at _update() time. If the number of vertices defined for the mesh layer increases,
     * a buffer memory alloc may be needed.
     */
    PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING,

    /* Texture coordinates based on the angle of surface at each vertex.
     *
     * Requires normal data to be stored in buffer memory for the specified layer ID
     * at _update() time. If the number of normals defined for the mesh layer increases,
     * a buffer memory alloc may be needed.
     */
    PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS,
} procedural_uv_generator_type;

typedef uint32_t procedural_uv_generator_object_id;

typedef enum
{
    /* settable, float[4].
     *
     * Default value: vec4(1.0, 0.0, 0.0, 0.0)
     *
     * Defines the four coefficients of the plane used for U coordinate computations for
     * generators of PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR.
     *
     * For all other generator types, this property is considered invalid. Any attempt to
     * set it will result in an assertion failure.
     */
    PROCEDURAL_UV_GENERATOR_OBJECT_PROPERTY_U_PLANE_COEFFS,

    /* settable, float[4].
     *
     * Default value: vec4(0.0, 1.0, 0.0, 0.0)
     *
     * Defines the four coefficients of the plane used for V coordinate computations for
     * generators of PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR.
     *
     * For all other generator types, this property is considered invalid. Any attempt to
     * set it will result in an assertion failure.
     */
    PROCEDURAL_UV_GENERATOR_OBJECT_PROPERTY_V_PLANE_COEFFS,

} procedural_uv_generator_object_property;


/** Registers a new mesh for usage with the procedural UV generator.
 *
 *  This function does NOT preallocate storage for the result data. You need to manually do
 *  a procedural_uv_generator_alloc_result_buffer_memory() call, or else the buffer will be
 *  allocated at the first _update() call.
 *
 *  @param in_generator     UV generator use.
 *  @param in_mesh          TODO. The object will be retained and released at
 *                          UV generator's tear-down time. Must not be NULL.
 *  @param in_mesh_layer_id ID of the mesh layer to add the texture coordinates data stream to.
 *
 *  @return ID to be used for the _update() call.
 */
PUBLIC EMERALD_API procedural_uv_generator_object_id procedural_uv_generator_add_mesh(procedural_uv_generator in_generator,
                                                                                      mesh                    in_mesh,
                                                                                      mesh_layer_id           in_mesh_layer_id);

/** TODO */
PUBLIC EMERALD_API bool procedural_uv_generator_alloc_result_buffer_memory(procedural_uv_generator           in_generator,
                                                                           procedural_uv_generator_object_id in_object_id,
                                                                           uint32_t                          n_bytes_to_preallocate);

/** TODO.
 *
 *  The call does NOT add the texture coordinates data stream to the specified mesh.
 *  The stream, sourced from buffer memory, will be added at the next _update() call time.
 *
 *  @param in_context Rendering context to use. Must not be NULL. The contxt will be retained
 *                    and released at tear-down time.
 *  @param type       TODO
 *  @param name       TODO
 *
 *  @return Object instance.
 ***/
PUBLIC EMERALD_API procedural_uv_generator procedural_uv_generator_create(ral_context                  in_context,
                                                                          procedural_uv_generator_type in_type,
                                                                          system_hashed_ansi_string    in_name);

/** Creates a return task which fills the UV data stream with data.
 *
 *  The size of the data buffer, into which the UV data will be generated, must be known at call time.
 *  This can either be derived from a non-zero, *client memory-backed* MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS property
 *  value, or the data buffer must have been earlier defined by calling procedural_uv_generator_prealloc_result_buffer_memory().
 *  For the former case, value of the MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS property may not come from buffer memory,
 *  since that would require a pipeline finish operation which is something we really do not want to do.
 *
 *  If this is the first _update() call made for the @param generator instance, the call adds
 *  a texture coordinates data stream to the owned mesh layer,
 *
 *  If the number of defined normals has changed since the last update time, this call may
 *  result in a buffer memory re-alloc operation.
 *
 *  @param in_generator UV generator instance to update the texture coordinate data stream for.
 *  @param in_object_id ID of a UV generator object the call should be made for.
 *
 */
PUBLIC EMERALD_API ral_present_task procedural_uv_generator_get_present_task(procedural_uv_generator           in_generator,
                                                                             procedural_uv_generator_object_id in_object_id);

/** TODO */
PUBLIC EMERALD_API void procedural_uv_generator_set_object_property(procedural_uv_generator                 in_generator,
                                                                    procedural_uv_generator_object_id       in_object_id,
                                                                    procedural_uv_generator_object_property in_property,
                                                                    const void*                             in_data);

#endif /* PROCEDURAL_UV_GENERATOR_H */
