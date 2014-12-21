/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_MATERIALS_H
#define PLUGIN_MATERIALS_H

/** TODO */
PUBLIC void DeinitMaterialData();

/** TODO */
PUBLIC system_hash64map GetLWSurfaceIDToSceneMaterialMap();

/** TODO */
PUBLIC void InitMaterialData(__in __notnull system_hash64map envelope_id_to_curve_container_map);

#endif /* PLUGIN_MATERIALS_H */