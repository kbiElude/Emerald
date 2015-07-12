/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Object manager has been implemented for two reasons:
 *
 * - To make it possible to quickly pinpoint resource leaks;
 * - To allow querying of properties of engine-based objects, perhaps with some kind of a preview in the future. 
 *
 */
#ifndef OBJECT_MANAGER_GENERAL_H
#define OBJECT_MANAGER_GENERAL_H

#include "system/system_types.h"

/** Converts an object_manager_object_type enum to corresponding hashed ansi string.
 *
 *  @param object_manager_object_type Engine object type to convert from.
 *
 *  @return Corresponding hashed ansi string with name of the object type or a hashed ansi string
 *          saying "Unknown" (without the quotations marks)
 **/
PUBLIC system_hashed_ansi_string object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_manager_object_type);

/** Deinitializes object manager. Only called from DllMain() - hence not exported.
 *
 **/
PUBLIC void _object_manager_deinit();

/** Initializes object manager. Only called from DllMain() - hence not exported.
 *
 **/
PUBLIC void _object_manager_init();

/** Retrieves a directory object instance with given name. The directory is looked for in  registry's root.
 *
 *  @param system_hashed_ansi_string Name of the directory to look for.
 *
 *  @return object_manager_directory instance or NULL if not found.
 **/
PUBLIC EMERALD_API object_manager_directory object_manager_get_directory(system_hashed_ansi_string);

/** TODO */
PUBLIC system_hashed_ansi_string object_manager_get_object_path(system_hashed_ansi_string  object_name,
                                                                object_manager_object_type object_type,
                                                                system_hashed_ansi_string  scene_name);

/** TODO */
PUBLIC EMERALD_API object_manager_directory object_manager_get_root_directory();

/** Registers a reference-counted object in the registry. This function can ONLY be called from within the engine,
 *  hence not exported.
 * 
 *  @param void*                      Raw pointer to the object.
 *  @param system_hashed_ansi_string  Registry path to register the object at.
 *  @param const char*                Source code file, in which the object was created.
 *  @param int                        Line, at which the object was created.
 *  @param object_manager_object_type Type of the created object.
 */
PUBLIC void _object_manager_register_refcounted_object(void*                      ptr,
                                                       system_hashed_ansi_string  hierarchy_path,
                                                       const char*                source_code_file_name,
                                                       int                        source_code_file_line,
                                                       object_manager_object_type object_type);

/** Unregisters a reference-counted object from the registry. This function can ONLY be called from within the engine,
 *  hence not exported.
 *
 *  @param system_hashed_ansi_string Registry path to unregister the object from.
 **/
PUBLIC void _object_manager_unregister_refcounted_object(system_hashed_ansi_string hierarchy_path);

#endif /* OBJECT_MANAGER_GENERAL_H */
