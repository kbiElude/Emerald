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
PUBLIC void GetGlobalVMapProperty(_vmap_property property,
                                  void*          out_result);

/** TODO */
PUBLIC void GetVMapProperty(_vmap_property property,
                            _vmap_type     vmap_type,
                            unsigned int   n_vmap,
                            void*          out_result);

/** TODO */
PUBLIC system_event StartVMapDataExtraction();

#endif /* PLUGIN_VMAPS_H */