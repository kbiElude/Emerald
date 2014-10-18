/**
 *
 * Emerald (kbi/elude @2014)
 */
#ifndef LW_CURVE_DATASET_H
#define LW_CURVE_DATASET_H

#include "curve/curve_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(lw_curve_dataset);

REFCOUNT_INSERT_DECLARATIONS(lw_curve_dataset, lw_curve_dataset);


/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_add_curve(__in __notnull lw_curve_dataset          dataset,
                                                   __in __notnull system_hashed_ansi_string object_name,
                                                   __in __notnull system_hashed_ansi_string curve_name,
                                                   __in __notnull curve_container           curve);

/** TODO */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                          __in __notnull system_file_serializer    serializer);

/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_save(__in __notnull lw_curve_dataset       dataset,
                                              __in __notnull system_file_serializer serializer);

#endif /* LW_CURVE_DATASET_H */
