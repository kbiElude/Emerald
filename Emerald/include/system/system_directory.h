/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_DIRECTORY_H
#define SYSTEM_DIRECTORY_H

#include "system_types.h"


/** Creates a new directory in the process' working directory.
 *
 *  @param directory_name Name of the directory to create. Must not be NULL.
 *
 *  @return true if the directory was successfully created, false otherwise.
 */
PUBLIC EMERALD_API bool system_directory_create(system_hashed_ansi_string directory_name);

#endif /* SYSTEM_DIRECTORY_H */
