/**
 *
 * Emerald (kbi/elude @2012)
 *
 * object_manager_directory is an object which can encapsulate:
 *
 * - Other directories
 * - Items
 *
 * Main limitation is that directories must not share their names, just as items (although these
 * two namespaces are not shared, so it's okay for a file to be named just like a directory located
 * in the same directory as the file in question). 
 *
 * Directories and directory items are NOT tracked by object manager.
 */
#ifndef OBJECT_MANAGER_DIRECTORY_H
#define OBJECT_MANAGER_DIRECTORY_H

#include "system/system_types.h"

/** Creates a new object_manager_directory instance, represented by user-provided name. This function DOES NOT
 *  attach the directory to any registry path, hence not exported.
 *
 *  @param system_hashed_ansi_string Name the directory should be represented with.
 *
 *  @return object_manager_directory instance or NULL if failed
 */
PUBLIC object_manager_directory object_manager_directory_create(__in __notnull system_hashed_ansi_string name);

/** Recreates a directory structure defined by @param system_hashed_ansi_string. The string is treated as a path,
 *  therefore if it contains an item's name, the name will be ignored.
 *
 *  @param object_manager_directory  Root directory, in which the hierarchy will be recreated.
 *  @param system_hashed_ansi_string Directory hierarchy to recreate.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC bool object_manager_directory_create_directory_structure(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** Removes a sub-directory, located in a directory passed in the first argument. If the sub-directory contains any items,
 *  these items are also going to be removed.
 *
 *  This function does not work for multi-level paths.
 *
 *  @param object_manager_directory  Directory the path defined by @param system_hashed_ansi_string is relative to.
 *  @param system_hashed_ansi_string Name of the directory subject to removal.
 *
 *  @return true if successful, false otherwise.
 * */
PUBLIC EMERALD_API bool object_manager_directory_delete_directory(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** Removes an item from a given path, relative to directory passed in the first argument.
 *
 *  @param object_manager_directory  Directory the file defined by @param system_hashed_ansi_string is located in.
 *  @param system_hashed_ansi_string Name of the item to be removed. 
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool object_manager_directory_delete_item(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** Extracts item name and path parts from a registry path. 
 *
 *  @param system_hashed_ansi_string  Registration path to use.
 *  @param system_hashed_ansi_string* Deref will be used to store the path part. Cannot be null.
 *  @param system_hashed_ansi_string* Deref will be used to store the item name part. Cannot be null.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool object_manager_directory_extract_item_name_and_path(__in __notnull system_hashed_ansi_string registration_path, __out system_hashed_ansi_string* out_item_path, __out system_hashed_ansi_string* out_item_name);

/** Retrieves raw pointer to an object described by a name located relative to @param object_manager_directory directory.
 *
 *  @param object_manager_directory  Directory object, corresponding to a directory in which the seeked item is located.
 *  @param system_hashed_ansi_string Name of the item to retrieve the raw pointer for.
 *
 *  @return object_manager_item instance if successful, NULL otherwise.
 **/
PUBLIC EMERALD_API object_manager_item object_manager_directory_find_item(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API object_manager_item object_manager_directory_find_item_recursive(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** Retrieves object_manager_directory object for a sub-directory, located in @param object_manager_directory user-provided directory.
 *
 *  This function does not work for multi-level paths. If you need one, use object_manager_directory_find_subdirectory_recursive().
 *
 *  @param object_manager_directory  Directory object, in which to look for the sub-directory.
 *  @param system_hashed_ansi_string Name of the sub-directory to look for.
 *
 *  @return object_manager_directory instance if successful, NULL otherwise.
 **/
PUBLIC EMERALD_API object_manager_directory object_manager_directory_find_subdirectory(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** Retrieves object_manager_directory object for a sub-directory, located in @param object_manager_directory user-provided directory.
 *
 *  This function works for multi-level paths.
 *
 *  @param object_manager_directory  Directory object, in which to look for the sub-directory.
 *  @param system_hashed_ansi_string Name of the sub-directory to look for.
 *
 *  @return object_manager_directory instance if successful, NULL otherwise.
 **/
PUBLIC EMERALD_API object_manager_directory object_manager_directory_find_subdirectory_recursive(__in __notnull object_manager_directory, system_hashed_ansi_string);

/** Retrieves a summed amount of directories and items stored in a given directory.
 *
 *  @param object_manager_directory Directory to query for the information.
 * 
 *  @return Amount of directories and items in the directory.
 **/
PUBLIC EMERALD_API uint32_t object_manager_directory_get_amount_of_children_for_directory(__in __notnull object_manager_directory);

/** Retrieves amount of subdirectories stored in a given directory.
 *
 *  @param object_manager_directory Directory to query for the information.
 *
 *  @return Amount of subdirectories in the directory.
 */
PUBLIC EMERALD_API uint32_t object_manager_directory_get_amount_of_subdirectories_for_directory(__in __notnull object_manager_directory);

/** Retrieves name of a directory.
 *
 *  @param obejct_manager_directory Instance to retrieve the name for.
 *
 *  @return Hashed ansi string with the name
 */
PUBLIC EMERALD_API system_hashed_ansi_string object_manager_directory_get_name(object_manager_directory);

/** Retrieves sub-directory at given index. 
 *
 *  @param object_manager_directory Root directory.
 *  @param uint32_t                 Directory index (indexing starts at 0) 
 *
 *  @return object_manager_directory Sub-directory object, if found. NULL otherwise
 */
PUBLIC EMERALD_API object_manager_directory object_manager_directory_get_subdirectory_at(__in __notnull object_manager_directory, uint32_t);

/** Retrieves sub-item at given index.
 *
 *  @param object_manager_directory     Root directory.
 *  @param uint32_t                     Item index (indexing starts at 0)
 *  @param object_manager_item*         Deref will be used to store result object_manager_item instance. Cannot be null.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool object_manager_directory_get_subitem_at(__in __notnull object_manager_directory, uint32_t index, __out __notnull object_manager_item* out_item);

/** Inserts a new item in @param object_manager_directory directory object.
 *
 *  @param object_manager_directory  Directory object, in which a new item should be stored.
 *  @param object_manager_item       Item to store.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool object_manager_directory_insert_item(__in __notnull object_manager_directory directory, object_manager_item item);

/** Inserts a new sub-directory in @param object_manager_directory directory object.
 *
 *  @param object_manager_directory Directory object, in which a new sub-directory should be created.
 *  @param object_manager_directory Sub-directory object, which should be placed in the directory.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool object_manager_directory_insert_subdirectory(__in __notnull object_manager_directory, object_manager_directory);

/** Release a directory object instance. The object should not be accessed after calling the function.
 *
 *  @param object_manager_directory Directory object instance to release.
 **/
PUBLIC void object_manager_directory_release(__in __notnull __post_invalid object_manager_directory);

/** TODO */
PUBLIC void object_manager_directory_set_verbose_mode(__in bool);

#endif /* OBJECT_MANAGER_DIRECTORY_H */