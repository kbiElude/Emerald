/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef MESH_MARCHINGCUBES_H
#define MESH_MARCHINGCUBES_H

#include "ogl/ogl_types.h"
#include "mesh/mesh_types.h"
#include "raGL/raGL_buffer.h"

REFCOUNT_INSERT_DECLARATIONS(mesh_marchingcubes,
                             mesh_marchingcubes)


typedef enum
{
    /* float; settable. */
    MESH_MARCHINGCUBES_PROPERTY_ISOLEVEL,

    /* mesh; not settable. */
    MESH_MARCHINGCUBES_PROPERTY_MESH,

    /* ral_buffer; settable. */
    MESH_MARCHINGCUBES_PROPERTY_SCALAR_DATA_BUFFER_RAL,
} mesh_marchingcubes_property;

/** Instantiates a mesh_marchingcubes instance.
 *
 *  NOTE: This function may be time-consuming.
 *  NOTE: This function will do a number of GPU memory allocations. The amount of committed memory
 *        is related to the requested grid size and the reduction hint, optionally passed as the
 *        last argument.
 *
 *  @param context                         OpenGL context to instantiate the object for. Must not be NULL.
 *  @param grid_size_xyz                   An array of three integers specifying X, Y and Z size of the grid.
 *                                         Usually the resolution of 100x100x100 is sufficient.
 *                                         The grid size cannot be changed after the object has been instantiated.
 *                                         Must not be NULL.
 *  @param scalar_data_bo_id               ID of a buffer object holding scalar data. The buffer is assumed
 *                                         to store non-padded floats describing scalar field densities.
 *                                         The values must be stored row-by-row, slice-by-slice.
 *                                         This value can be changed later by setting a relevant property.
 *                                         Must not be 0.
 *  @param scalar_data_bo_start_offset     Start offset, from which the density data starts.
 *                                         This value can be changed later by setting a relevant property.
 *  @param scalar_data_bo_size             Size of the buffer memory region. This must be larger than or equal to:
 *                                         grid_size_xyz[0] * grid_size_xyz[1] * grid_size_xyz[2] * sizeof(float)
 *                                         This value can be changed later by setting a relevant property.
 *  @param material                        Material to use for the instance. Must not be NULL.
 *  @param name                            Name to use for the instance. Must not be NULL.
 *  @param polygonized_data_size_reduction mesh_marchingcubes creates a number of buffer objects for internal usage.
 *                                         One of these buffers holds polygonized normal & vertex data. Its size,
 *                                         when naively calculated, becomes notable for large grids. This argument
 *                                         presents a divisor, by which the size is going to be divided before proceeding
 *                                         with actual allocation. Usually, the default value is fine, but if you
 *                                         note that some parts of the blob is missing, please reduce the argument value
 *                                         and re-check.
 *                                         Values below 1 and above 99 are illegal.
 *                                         The assigned divisor cannot be changed after instantiation time.
 *
 *  @return Requested instance.
 * */
PUBLIC EMERALD_API mesh_marchingcubes mesh_marchingcubes_create(ral_context               context,
                                                                const unsigned int*       grid_size_xyz,
                                                                ral_buffer                scalar_data_bo,
                                                                float                     isolevel,
                                                                scene_material            material,
                                                                system_hashed_ansi_string name,
                                                                unsigned int              polygonized_data_size_reduction = 2);

/** TODO */
PUBLIC EMERALD_API void mesh_marchingcubes_get_property(const mesh_marchingcubes    in_mesh,
                                                        mesh_marchingcubes_property property,
                                                        void*                       out_result_ptr);

/** Returns a present task which polygonizes the scalar field and stores the result normal & vertex data
 *  in a buffer object. The buffers are attached to a mesh instance, which can be extracted by querying the
 *  MESH_MARCHINGCUBES_PROPERTY_MESH property.
 *
 *  The present task exposes a single unique buffer output which holds the polygonized data.
 *
 *
 *  NOTE: This function performs an indirect draw call.
 *  NOTE: This function may return different present tasks, depending on pending workload. Never cache
 *        the returned present task.
 *
 *  @param in_mesh                  mesh_marchingcubes instance to use. Must not be NULL.
 *  @param has_scalar_field_changed true to force the polygonization, false otherwise.
 *
 **/
PUBLIC EMERALD_API ral_present_task mesh_marchingcubes_get_polygonize_present_task(mesh_marchingcubes in_mesh,
                                                                                   bool               has_scalar_field_changed);

/** TODO */
PUBLIC EMERALD_API void mesh_marchingcubes_set_property(mesh_marchingcubes          in_mesh,
                                                        mesh_marchingcubes_property property,
                                                        const void*                 data);

#endif /* MESH_MARCHINGCUBES_H */
