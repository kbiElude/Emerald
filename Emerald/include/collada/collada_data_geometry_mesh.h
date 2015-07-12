/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_GEOMETRY_MESH_H
#define COLLADA_DATA_GEOMETRY_MESH_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_geometry_mesh_property
{
    COLLADA_DATA_GEOMETRY_MESH_PROPERTY_N_POLYLISTS,
    COLLADA_DATA_GEOMETRY_MESH_PROPERTY_PARENT_GEOMETRY,

    /* Always last */
    COLLADA_DATA_GEOMETRY_MESH_PROPERTY_COUNT
};

/** TODO */
PUBLIC collada_data_geometry_mesh collada_data_geometry_mesh_create(tinyxml2::XMLElement* mesh_element_ptr,
                                                                    collada_data_geometry parent_geometry,
                                                                    const collada_data    collada_data);

/** TODO */
PUBLIC EMERALD_API void collada_data_geometry_mesh_get_polylist(collada_data_geometry_mesh mesh,
                                                                unsigned int               n_polylist,
                                                                collada_data_polylist*     out_polylist);

/** TODO */
PUBLIC void collada_data_geometry_mesh_get_property(collada_data_geometry_mesh          mesh,
                                                    collada_data_geometry_mesh_property property,
                                                    void*                               out_result_ptr);

/** TODO */
PUBLIC collada_data_source collada_data_geometry_mesh_get_source_by_id(collada_data_geometry_mesh mesh,
                                                                       system_hashed_ansi_string  source_id);

/** TODO */
PUBLIC void collada_data_geometry_mesh_release(collada_data_geometry_mesh mesh);

#endif /* COLLADA_DATA_GEOMETRY_MESH_H */
