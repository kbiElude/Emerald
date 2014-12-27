/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_capabilities.h"

PRIVATE unsigned int n_cpu_cores = 0;


/** Please see header for spec */
PUBLIC EMERALD_API void system_capabilities_get(__in            system_capabilities_property property,
                                                __out __notnull void*                        out_result)
{
    switch (property)
    {
        case SYSTEM_CAPABILITIES_PROPERTY_NUMBER_OF_CPU_CORES:
        {
            ASSERT_DEBUG_SYNC(n_cpu_cores != 0,
                              "0 CPU cores are recognized!");

            *(unsigned int*) out_result = n_cpu_cores;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized system_capabilities_property value");
        }
    } /* switch (property) */
}


/** Please see header for spec */
PUBLIC void system_capabilities_init()
{
    SYSTEM_INFO system_info;

    ::GetSystemInfo(&system_info);

    n_cpu_cores = system_info.dwNumberOfProcessors;
}
