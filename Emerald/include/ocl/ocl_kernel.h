/**
 *
 * Emerald (kbi/elude @2012)
 *
 * This object is currently useful for maintenance purposes only but it might come in
 * handy in the future.
 */
#ifndef OCL_KERNEL_H
#define OCL_KERNEL_H

#include "ocl/ocl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ocl_kernel, ocl_kernel);


/** TODO */
PUBLIC EMERALD_API  ocl_kernel ocl_kernel_create(__in                   __notnull ocl_program               owner,
                                                 __in                             uint32_t                  n_devices,
                                                 __in_ecount(n_devices) __notnull cl_device_id*             devices,
                                                 __in                   __notnull cl_kernel                 kernel,
                                                 __in                   __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API cl_kernel ocl_kernel_get_kernel(__in __notnull ocl_kernel);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string ocl_kernel_get_name(__in __notnull ocl_kernel);

/** TODO */
PUBLIC EMERALD_API uint32_t ocl_kernel_get_work_group_size(__in __notnull ocl_kernel);

#endif /* OCL_KERNEL_H */
