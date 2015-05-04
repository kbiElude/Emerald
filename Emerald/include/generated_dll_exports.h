
#ifndef _EMERALD_API_H
#define _EMERALD_API_H

#ifdef EMERALD_STATIC_DEFINE
#  define _EMERALD_API
#  define EMERALD_NO_EXPORT
#else
#  ifndef _EMERALD_API
#    ifdef EMERALD_EXPORTS
        /* We are building this library */
#      define _EMERALD_API __declspec(dllexport)
#    else
        /* We are using this library */
#      define _EMERALD_API __declspec(dllimport)
#    endif
#  endif

#  ifndef EMERALD_NO_EXPORT
#    define EMERALD_NO_EXPORT 
#  endif
#endif

#ifndef EMERALD_DEPRECATED
#  define EMERALD_DEPRECATED __declspec(deprecated)
#  define EMERALD_DEPRECATED_EXPORT _EMERALD_API __declspec(deprecated)
#  define EMERALD_DEPRECATED_NO_EXPORT EMERALD_NO_EXPORT __declspec(deprecated)
#endif

#define DEFINE_NO_DEPRECATED 0
#if DEFINE_NO_DEPRECATED
# define EMERALD_NO_DEPRECATED
#endif

#endif
