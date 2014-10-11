/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OCL_GENERAL_H
#define OCL_GENERAL_H

#include "ocl/ocl_types.h"


/** TODO */
PUBLIC void _ocl_deinit();

/** TODO */
PUBLIC void _ocl_init();

/** TODO */
PUBLIC EMERALD_API const ocl_device_info* ocl_get_device_info(__in __notnull cl_platform_id, __in __notnull cl_device_id);

/** TODO */
PUBLIC EMERALD_API bool ocl_get_device_id(__in __notnull cl_platform_id, __in uint32_t n_device_id, __out __notnull cl_device_id*);

/** TODO */
PUBLIC EMERALD_API bool ocl_get_device_id_by_name(__in  __notnull cl_platform_id,
                                                  __in  __notnull system_hashed_ansi_string,
                                                  __out __notnull cl_device_id*);

/** TODO */
PUBLIC EMERALD_API const ocl_11_entrypoints* ocl_get_entrypoints();

/** TODO */
PUBLIC EMERALD_API const ocl_khr_gl_sharing_entrypoints* ocl_get_khr_gl_sharing_entrypoints();

/** TODO */
PUBLIC EMERALD_API bool ocl_get_platform_id(uint32_t n_platform_id, __out __notnull cl_platform_id*);

/** TODO */
PUBLIC EMERALD_API const ocl_platform_info* ocl_get_platform_info(__in __notnull cl_platform_id);

#endif /* OCL_GENERAL_H */
