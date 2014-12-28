/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_VMAPS_H
#define PLUGIN_VMAPS_H

typedef enum
{
    VMAP_TYPE_UVMAP,

} _vmap_type;

typedef enum
{
    VMAP_PROPERTY_N_UV_VMAPS, /* global,        unsigned int              */
    VMAP_PROPERTY_NAME,       /* vmap-specific, system_hashed_ansi_string */
} _vmap_property;


/** TODO */
PUBLIC void DeinitVMapData();

/** TODO */
PUBLIC void GetGlobalVMapProperty(__in            _vmap_property property,
                                  __out __notnull void*          out_result);

/** TODO */
PUBLIC void GetVMapProperty(__in            _vmap_property property,
                            __in            _vmap_type     vmap_type,
                            __in            unsigned int   n_vmap,
                            __out __notnull void*          out_result);

/** TODO */
PUBLIC system_event StartVMapDataExtraction();

#endif /* PLUGIN_VMAPS_H */