/**
 *
 *  Emerald (kbi/elude @2012-2015)
 *
 *  @brief File serializer is an efficient file proxy object.
 *
 *         The serializer can work in different modes:
 *
 *         1) File mode, where an actual file is read from or
 *            written to.
 *         2) Memory region mode, where the serializer acts as
 *            a shim layer for load & store ops.
 *
 * For:
 *
 *  - reading: the serilizer loads the file asynchronously, allowing the caller to do other
 *             things while the file contents loads. Read calls are blocked until file contents
 *             become available.
 *  - writing: it caches data to be stored and flushes the file when releasing the serializer.
 */
#ifndef SYSTEM_FILE_SERIALIZER_H
#define SYSTEM_FILE_SERIALIZER_H

#include "curve/curve_types.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(system_file_serializer,
                             system_file_serializer)


typedef enum
{
    /* not settable, uint32_t */
    SYSTEM_FILE_SERIALIZER_PROPERTY_CURRENT_OFFSET,

    /* settable, system_hashed_ansi_string.
     *
     * Works for both file and memory region serializers.
     */
    SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,

    /* not settable, system_hashed_ansi_string.
     *
     * Works for both file and memory region serializers.
     */
    SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_PATH,

    /* not settable, system_hashed_ansi_string.
     *
     * Works for both file and memory region serializers.
     */
    SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_PATH_AND_NAME,

    /* not settable, const char*. Internal usage only. */
    SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,

    /* not settable, size_t */
    SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE,

    SYSTEM_FILE_SERIALIZER_PROPERTY_UNKNOWN
} system_file_serializer_property;


/** Creates a file serializer instance for reading a memory region.
 *
 *  @param data      Memory region to use as a data source.
 *  @param data_size Number of bytes available for reading under @param data.
 *
 */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_reading_memory_region(void*        data,
                                                                                                  unsigned int data_size);

/** Creates a file serializer instance for reading a file.
 *
 *  @param system_hashed_ansi_string File name (with path, if necessary)
 *
 *  @return File serializer instance if file was found. Otherwise null.
 */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_reading(system_hashed_ansi_string file_name,
                                                                                    bool                      async_read = true);

/** Creates a file serializer instance for writing a file.
 *
 *  @param system_hashed_ansi_string File name (with path, if necessary)
 *
 *  @return File serializer instance if file could have been created. Otherwise null.
 */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_writing(system_hashed_ansi_string file_name);

/** TODO.
 *
 *  Should only be issued against write serializers.
 */
PUBLIC EMERALD_API void system_file_serializer_flush_writes(system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void system_file_serializer_get_property(system_file_serializer          serializer,
                                                            system_file_serializer_property property,
                                                            void*                           out_data);

/** Copies @param uint32_t bytes to user-provided location and moves the reading pointer, so that next read request will
 *  retrieve bytes following the read sequence.
 *
 *  Only reading file serializer can be used for this operation. Using writing serializer will result in an assertion
 *  failure.
 *
 *  @param system_file_serializer File serializer instance to use.
 *  @param uint32_t               Amount of bytes to read.
 *  @param void*                  Pointer the data should be stored at.
 *
 *  @return true if successful, false otherwise
 */
PUBLIC EMERALD_API bool system_file_serializer_read(system_file_serializer serializer,
                                                    uint32_t               n_bytes,
                                                    void*                  out_result);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_read_curve_container(system_file_serializer    serializer,
                                                                    system_hashed_ansi_string object_manager_path,
                                                                    curve_container*          result_container);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_read_hashed_ansi_string(system_file_serializer     serializer,
                                                                       system_hashed_ansi_string* result_string);

/** TODO.
 *
 *  NOTE: It is caller's responsibility to release result 4x4 matrix when it's
 *        no longer needed.
 *
 **/
PUBLIC EMERALD_API bool system_file_serializer_read_matrix4x4(system_file_serializer serializer,
                                                              system_matrix4x4*      out_result);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_read_variant(system_file_serializer serializer,
                                                            system_variant         out_result);

/** TODO */
PUBLIC EMERALD_API void system_file_serializer_set_property(system_file_serializer          serializer,
                                                            system_file_serializer_property property,
                                                            void*                           data);

/** Schedules @param uint32_t bytes to be written to the file. This operation DOES NOT result in writing the data.
 *  This operation should only be used for a writing file serializer - otherwise an assertion failure will occur.
 *
 *  @param system_file_serializer File serializer instance to use.
 *  @param uint32_t               Amount of bytes to write from @param const char*
 *  @param const void*            Source of the data to store.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_file_serializer_write(system_file_serializer serializer,
                                                     uint32_t               n_bytes,
                                                     const void*            data_to_write);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_curve_container(      system_file_serializer serializer,
                                                                     const curve_container        curve);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_hashed_ansi_string(system_file_serializer    serializer,
                                                                        system_hashed_ansi_string string);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_matrix4x4(system_file_serializer serializer,
                                                               system_matrix4x4       matrix);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_variant(system_file_serializer serializer,
                                                             system_variant         variant);

#endif /* SYSTEM_FILE_SERIALIZER_H */