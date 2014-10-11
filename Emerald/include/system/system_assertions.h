/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_ASSERTIONS_H
#define SYSTEM_ASSERTIONS_H

#include "dll_exports.h"
#include "system_types.h"


/** Macro to use an assertion check that should be only included for debug builds of the user app AND Emerald core dll. **/
#ifdef _DEBUG
    #define ASSERT_DEBUG_ASYNC(condition, message, ...) system_assertions_assert(false, DEBUG_ONLY_ASSERTION, condition, message, __VA_ARGS__); \
                                                        __analysis_assume(condition);

    #define ASSERT_DEBUG_SYNC(condition, message, ...)  system_assertions_assert(true,  DEBUG_ONLY_ASSERTION, condition, message, __VA_ARGS__); \
                                                        __analysis_assume(condition);
#else
    #define ASSERT_DEBUG_ASYNC(condition, message, ...) 
    #define ASSERT_DEBUG_SYNC(condition, message, ...)
#endif
  
/** Macro to use an assertion check that should be only included for release builds of the user app AND Emerald core dll. **/
#ifndef _DEBUG
    #define ASSERT_RELEASE_ASYNC(condition, message, ...) system_assertions_assert(false, RELEASE_ONLY_ASSERTION, condition, message, __VA_ARGS__); \
                                                          __analysis_assume(condition);
    #define ASSERT_RELEASE_SYNC(condition, message, ...)  system_assertions_assert(true,  RELEASE_ONLY_ASSERTION, condition, message, __VA_ARGS__); \
                                                          __analysis_assume(condition);
#else
    #define ASSERT_RELEASE_ASYNC(condition, message, ...)
    #define ASSERT_RELEASE_SYNC(condition, message, ...)
#endif

/** Macro to always do an assertion check */
#define ASSERT_ALWAYS_ASYNC(condition, message, ...) system_assertions_assert(false, ALWAYS_ASSERTION, condition, message, __VA_ARGS__); \
                                                     __analysis_assume(condition);
#define ASSERT_ALWAYS_SYNC(condition, message, ...)  system_assertions_assert(true,  ALWAYS_ASSERTION, condition, message, __VA_ARGS__); \
                                                     __analysis_assume(condition);

/** Function to use in order to issue a non-/-blocking assertion check. This
 *  implementation will use one of the thread pool's worker threads to show
 *  information about the failing event.
 *
 *  This function MUST NOT be used before thread pool initializes!
 *
 *  @param bool                  True to do a synchronous assertion check, false to do an asynchronous one.
 *  @param system_assertion_type Assertion type
 *  @param bool                  Condition to check
 *  @param const char*           Message to show if the assertion fails.
 *  @param ...                   Varying arguments to use for creating assertion failure string.
 */
PUBLIC EMERALD_API void system_assertions_assert(bool, system_assertion_type, bool, const char*, ...);

/** Function that initializes assertions module. Call only once, preferably from DLL entry point. */
PUBLIC void _system_assertions_init();

/** Function that deinitializes assertions module. Call only once, preferably from DLL entry point. */
PUBLIC void _system_assertions_deinit();

#endif /* SYSTEM_ASSERTIONS_H */