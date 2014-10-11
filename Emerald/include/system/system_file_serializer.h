/**
 *
 *  Emerald (kbi/elude @2012)
 *
 *  @brief File serializer is an efficient file proxy object. For:
 *
 *  - reading: it loads the file asynchronously, allowing the caller to do other things while the
 *             file contents loads. Read calls are blocked until file contents become available.
 *  - writing: it caches data to be stored and flushes the file when releasing the serializer.
 */
#ifndef SYSTEM_FILE_SERIALIZER_H
#define SYSTEM_FILE_SERIALIZER_H

#include "curve/curve_types.h"
#include "system/system_types.h"


/** Creates a file serializer instance for reading a file.
 *
 *  @param system_hashed_ansi_string File name (with path, if necessary)
 *
 *  @return File serializer instance if file was found. Otherwise null.
 */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_reading(__in __notnull system_hashed_ansi_string,
                                                                                    __in           bool                      async_read = true);

/** Creates a file serializer instance for writing a file.
 *
 *  @param system_hashed_ansi_string File name (with path, if necessary)
 *
 *  @return File serializer instance if file could have been created. Otherwise null.
 */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_writing(__in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API uint32_t system_file_serializer_get_current_offset(__in __notnull system_file_serializer);

/** Retrieves file name used for serializing operation.
 *
 *  @param system_file_serializer Serializer to retrieve the name for.
 *
 *  @return Name of the serialized file.
 */
PUBLIC EMERALD_API system_hashed_ansi_string system_file_serializer_get_file_name(__in __notnull system_file_serializer);

/** TODO.
 *
 *  Only supported for file serializers initialized for reading mode.
 **/
PUBLIC EMERALD_API system_hashed_ansi_string system_file_serializer_get_file_path(__in __notnull const system_file_serializer);

/** Returns a pointer to internal storage of the serializer. This is for internal usage only.
 *
 */
PUBLIC const void* system_file_serializer_get_raw_storage(__in __notnull system_file_serializer serializer);

/** Retrieves size of a file read by the serializer OR amount of bytes scheduled for writing.
 *
 *  This function should not be used for a serializer that was initialized for writing.
 *
 *  @param system_file_serializer Serializer to use.
 *
 *  @return Size of the file.
 */
PUBLIC EMERALD_API size_t system_file_serializer_get_size(__in __notnull system_file_serializer);

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
PUBLIC EMERALD_API bool system_file_serializer_read(__in __notnull                       system_file_serializer,
                                                    __in                                 uint32_t n_bytes,
                                                    __deref_out_bcount_full_opt(n_bytes) void*);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_read_curve_container(__in  __notnull system_file_serializer,
                                                                    __out __notnull curve_container*);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_read_hashed_ansi_string(__in  __notnull system_file_serializer,
                                                                       __out __notnull system_hashed_ansi_string*);

/** TODO.
 *
 *  NOTE: It is caller's responsibility to release result 4x4 matrix when it's
 *        no longer needed.
 *
 **/
PUBLIC EMERALD_API bool system_file_serializer_read_matrix4x4(__in  __notnull system_file_serializer,
                                                              __out __notnull system_matrix4x4*);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_read_variant(__in  __notnull system_file_serializer,
                                                            __out __notnull system_variant);

/** Releases an existing file serializer instance. If serializer was created for writing, data will be written to the file.
 *
 *  @param system_file_serializer File serializer to release.
 */
PUBLIC EMERALD_API void system_file_serializer_release(__in __notnull __deallocate(mem) system_file_serializer);

/** Schedules @param uint32_t bytes to be written to the file. This operation DOES NOT result in writing the data.
 *  This operation should only be used for a writing file serializer - otherwise an assertion failure will occur.
 *
 *  @param system_file_serializer File serializer instance to use.
 *  @param uint32_t               Amount of bytes to write from @param const char*
 *  @param const void*            Source of the data to store.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_file_serializer_write(__in __notnull system_file_serializer,
                                                     __in           uint32_t,
                                                     __in __notnull const void*);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_curve_container(__in __notnull system_file_serializer,
                                                                     __in __notnull curve_container);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_hashed_ansi_string(__in __notnull system_file_serializer,
                                                                        __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_matrix4x4(__in __notnull system_file_serializer,
                                                               __in __notnull system_matrix4x4);

/** TODO */
PUBLIC EMERALD_API bool system_file_serializer_write_variant(__in __notnull system_file_serializer,
                                                             __in __notnull system_variant);

#endif /* SYSTEM_FILE_SERIALIZER_H */