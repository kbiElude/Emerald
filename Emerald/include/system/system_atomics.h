/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_ATOMICS_H
#define SYSTEM_ATOMICS_H

/* TODO */
inline unsigned int system_atomics_decrement(volatile long* value_ptr)
{
    unsigned int previous_value;

    #ifdef _WIN32
    {
        previous_value = ::InterlockedDecrement(value_ptr);
    }
    #else
    {
        previous_value = __sync_fetch_and_sub(value_ptr, 1);
    }
    #endif

    return previous_value;
}

inline unsigned int system_atomics_decrement(volatile unsigned int* value_ptr)
{
    unsigned int previous_value;

    #ifdef _WIN32
    {
        previous_value = ::InterlockedDecrement(value_ptr);
    }
    #else
    {
        previous_value = __sync_fetch_and_sub(value_ptr, 1);
    }
    #endif

    return previous_value;
}

inline unsigned int system_atomics_increment(volatile long* value_ptr)
{
    unsigned int previous_value;

    #ifdef _WIN32
    {
        previous_value = ::InterlockedIncrement(value_ptr);
    }
    #else
    {
        previous_value = __sync_fetch_and_add(value_ptr, 1);
    }
    #endif

    return previous_value;
}

inline unsigned int system_atomics_increment(volatile unsigned int* value_ptr)
{
    unsigned int previous_value;

    #ifdef _WIN32
    {
        previous_value = ::InterlockedIncrement(value_ptr);
    }
    #else
    {
        previous_value = __sync_fetch_and_add(value_ptr, 1);
    }
    #endif

    return previous_value;
}


#endif /* SYSTEM_ATOMICS_H */
