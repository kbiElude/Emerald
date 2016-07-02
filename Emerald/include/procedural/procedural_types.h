/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef PROCEDURAL_TYPES_H
#define PROCEDURAL_TYPES_H

DECLARE_HANDLE(procedural_mesh_box);
DECLARE_HANDLE(procedural_mesh_circle);
DECLARE_HANDLE(procedural_mesh_sphere);
DECLARE_HANDLE(procedural_uv_generator);

typedef enum
{
    /* Bakes a BO region with data that can be used for a glDrawArrays() call */
    PROCEDURAL_MESH_DATA_TYPE_NONINDEXED_BIT = 1 << 0,

    /* Bakes a BO region with data that can be used for a glDrawElements() call */
    PROCEDURAL_MESH_DATA_TYPE_INDEXED_BIT = 1 << 1,

    /* Exposes raw data, as prepared for PROCEDURAL_MESH_DATA_NONINDEXED */
    PROCEDURAL_MESH_DATA_TYPE_RAW_BIT = 1 << 2

} procedural_mesh_data_type_bits;

#endif /* PROCEDURAL_TYPES_H */