/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OCL_CONTEXT_H
#define OCL_CONTEXT_H

#include "ocl/ocl_types.h"
#include "ogl/ogl_types.h"


REFCOUNT_INSERT_DECLARATIONS(ocl_context, ocl_context)

/** TODO */
PUBLIC EMERALD_API ocl_context ocl_context_create_gpu_only_with_gl_sharing(__in __notnull cl_platform_id            platform_id,
                                                                           __in __notnull ogl_context               gl_context,
                                                                           __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API cl_context ocl_context_get_context(__in __notnull ocl_context);

/** TODO */
PUBLIC EMERALD_API void ocl_context_get_assigned_devices(__in  __notnull ocl_context,
                                                         __out __notnull cl_device_id**,
                                                         __out __notnull uint32_t*);

/** TODO */
PUBLIC EMERALD_API cl_command_queue ocl_context_get_command_queue(__in __notnull ocl_context,
                                                                  __in           uint32_t);

/** TODO */
PUBLIC EMERALD_API cl_platform_id ocl_context_get_platform_id(__in __notnull ocl_context);

/** TODO */
PUBLIC EMERALD_API void ocl_context_set_driver_error_callback_behavior(__in __notnull ocl_context,
                                                                       __in           bool should_cause_assertion_failure);

#endif /* OCL_CONTEXT_H */
