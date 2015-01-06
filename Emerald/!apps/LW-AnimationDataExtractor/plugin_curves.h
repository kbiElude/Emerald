/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef PLUGIN_CURVES_H
#define PLUGIN_CURVES_H

#include "curve/curve_types.h"
#include "scene/scene_types.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"

typedef uint32_t curve_id;


/** TODO */
PUBLIC curve_id AddCurveContainerToEnvelopeIDToCurveContainerHashMap(__in __notnull curve_container curve);

/** TODO */
PUBLIC void DeinitCurveData();

/** TODO */
PUBLIC void FillSceneWithCurveData(__in __notnull scene in_scene);

/** TODO */
PUBLIC system_hash64map GetEnvelopeIDToCurveContainerHashMap();

/** TODO */
PUBLIC void InitCurveData();

#endif /* PLUGIN_CURVES_H */