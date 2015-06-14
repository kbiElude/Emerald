/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

#ifdef _WIN32
    #include <intrin.h>
#else
    #include <stdarg.h>
#endif


/** Internal typedefs */
typedef struct
{
    system_critical_section cs;
    system_resizable_vector messages; /* contains const char* with formatted assertion strings */
} _system_assertions_internal;

/** Internal variables */
_system_assertions_internal* internals = NULL;


/** TODO */
PRIVATE void _show_assertion_failure(const char* message)
{
    #if defined(_DEBUG)
    {
        __debugbreak();

#ifdef _WIN32
        ::MessageBoxA(HWND_DESKTOP,
                      message,
                      "Assertion failure",
                      MB_OK | MB_ICONWARNING);
#else
        /* TODO */
#endif
    }
    #endif /* _DEBUG */
}


/** Please see header for specfication */
PUBLIC EMERALD_API void system_assertions_assert(bool                  is_blocking,
                                                 system_assertion_type type,
                                                 bool                  condition,
                                                 const char*           message,
                                                 ...)
{
    #ifdef _DEBUG
        static const bool is_debug = true;
    #else
        static const bool is_debug = false;
    #endif

    bool should_fail = false;

    switch (type)
    {
        case DEBUG_ONLY_ASSERTION:  should_fail = (( is_debug) && !condition); break;
        case RELEASE_ONLY_ASSERTION:should_fail = ((!is_debug) && !condition); break;
        case ALWAYS_ASSERTION:      should_fail = (               !condition); break;
        default:                    should_fail = false;
    }

    if (should_fail)
    {
        char    formatted_message[ASSERTION_FAILURE_MAX_LENGTH];
        va_list arguments;

        memset(formatted_message,
               0,
               ASSERTION_FAILURE_MAX_LENGTH);

        va_start (arguments,
                  message);
        vsnprintf(formatted_message,
                  ASSERTION_FAILURE_MAX_LENGTH,
                  message,
                  arguments);
        va_end   (arguments);

        /* Insert a log entry */
        system_log_write(LOGLEVEL_ERROR,
                         formatted_message);

        /* Alloc message container */
        size_t message_length    = strlen(formatted_message);
        char*  message_container = new (std::nothrow) char[message_length + 1];

        if (message_container != NULL)
        {
            memcpy(message_container,
                   formatted_message,
                   message_length);

            message_container[message_length] = 0x00;

            /* Show the message now (if blocking) or queue for showing (if non-blocking) */
            if (is_blocking)
            {
                _show_assertion_failure(message_container);

                delete [] message_container;
            }
            else
            {
                system_critical_section_enter(internals->cs);
                {
                    /* Push the message onto our internal messages queue */
                    system_resizable_vector_push(internals->messages,
                                                &message_container);

                    /* Obtain the message's index */
                    unsigned int message_index = system_resizable_vector_get_amount_of_elements(internals->messages);
                }
                system_critical_section_leave(internals->cs);

                /* TODO: THREAD POOL INTEGRATION */
            }
        }
    }
}

/** TODO */
PUBLIC void _system_assertions_init()
{
    if (internals == NULL)
    {
        internals = new (std::nothrow) _system_assertions_internal;

        if (internals != NULL)
        {
            internals->cs       = system_critical_section_create();
            internals->messages = system_resizable_vector_create(ASSERTIONS_BASE_CAPACITY);
        }
    }
}

/** TODO */
PUBLIC void _system_assertions_deinit()
{
    if (internals != NULL)
    {
        system_critical_section_release(internals->cs);
        system_resizable_vector_release(internals->messages);

        delete internals;
        internals = NULL;
    }
}

