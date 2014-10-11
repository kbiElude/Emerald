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
PUBLIC collada_data_geometry_mesh collada_data_geometry_mesh_create(__in __notnull tinyxml2::XMLElement* mesh_element_ptr,
                                                                    __in __notnull collada_data_geometry parent_geometry,
                                                                    __in __notnull const collada_data    collada_data);

/** TODO */
PUBLIC EMERALD_API void collada_data_geometry_mesh_get_polylist(__in  __notnull collada_data_geometry_mesh mesh,
                                                                __in            unsigned int               n_polylist,
                                                                __out           collada_data_polylist*     out_polylist);

/** TODO */
PUBLIC void collada_data_geometry_mesh_get_property(__in  __notnull collada_data_geometry_mesh          mesh,
                                                    __in            collada_data_geometry_mesh_property property,
                                                    __out __notnull void*                               out_result_ptr);

/** TODO */
PUBLIC collada_data_source collada_data_geometry_mesh_get_source_by_id(__in __notnull collada_data_geometry_mesh mesh,
                                                                       __in __notnull system_hashed_ansi_string  source_id);

/** TODO */
PUBLIC void collada_data_geometry_mesh_release(__in __notnull __post_invalid collada_data_geometry_mesh mesh);

#endif /* COLLADA_DATA_GEOMETRY_MESH_H */
