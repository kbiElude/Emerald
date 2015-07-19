/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef SYSTEM_CAPABILITIES_H
#define SYSTEM_CAPABILITIES_H

#include "system_types.h"


typedef enum
{
    /* unsigned int */
    SYSTEM_CAPABILITIES_PROPERTY_NUMBER_OF_CPU_CORES,
} system_capabilities_property;


/** TODO */
PUBLIC void system_capabilities_deinit();

/** TODO */
PUBLIC EMERALD_API void system_capabilities_get(system_capabilities_property property,
                                                void*                        out_result);

/** TODO */
PUBLIC void system_capabilities_init();

#endif /* SYSTEM_CAPABILITIES_H */
