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
    DATA_BO_ARRAYS   = 1 << 0,

    /* Bakes a BO region with data that can be used for a glDrawElements() call */
    DATA_BO_ELEMENTS = 1 << 1,

    /* Exposes raw data, as prepared for DATA_BO_ARRAYS */
    DATA_RAW         = 1 << 2

} _procedural_mesh_data_bitmask;

#endif /* PROCEDURAL_TYPES_H */