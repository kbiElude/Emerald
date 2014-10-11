/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OCL_PROGRAM_H
#define OCL_PROGRAM_H

#include "ocl/ocl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ocl_program, ocl_program);


/** TODO */
PUBLIC EMERALD_API ocl_program ocl_program_create_from_source(__in __notnull                     ocl_context               context,
                                                              __in                               uint32_t                  n_strings,
                                                              __in_ecount(n_strings) __notnull   const char**              strings,
                                                              __in_ecount(n_strings) __maybenull const size_t*             string_lengths,
                                                              __in                   __notnull   system_hashed_ansi_string name);

/** TODO. NEVER release kernel instance returned by this function! */
PUBLIC EMERALD_API ocl_kernel ocl_program_get_kernel_by_name(__in __notnull ocl_program,
                                                             __in __notnull system_hashed_ansi_string);

#endif /* OCL_PROGRAM_H */
