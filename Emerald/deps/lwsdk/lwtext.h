/*
 * LWSDK Header File
 * Copyright 2011,  NewTek, Inc.
 *
 * LWTEXT.H -- LightWave Text
 *
 * Jamie L. Finch
 * Senile Programmer
 */

#ifndef LWSDK_LWTEXT_H
#define LWSDK_LWTEXT_H

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWTEXTFUNCS_GLOBAL "LWTextureFuncs"

/*
 *      Mac Operating system.
 */

#if( defined( _MACOS ) )

#define LW_CHAR wchar_t

#define LW_STRLEN( A )            wcslen( A )
#define LW_STRCMP( A, B )         wcscmp( A, B )
//#define LW_STRICMP( A, B )      _wcsicmp( A, B )
#define LW_STRNCMP( A, B, C )    wcsncmp( A, B, C )
//#define LW_STRNICMP( A, B, C ) _wcsnicmp( A, B, C )
#define LW_STRCAT( A, B )         wcscat( A, B )
#define LW_STRCPY( A, B )         wcscpy( A, B )
#define LW_STRSTR( A, B )         wcsstr( A, B )
#define LW_TOUPPER( A )         towupper( A )
#define LW_TOLOWER( A )         towlower( A )

/*
 *      Microsoft Windows.
 */

#else

#define LW_CHAR wchar_t

#define LW_STRLEN( A )            wcslen( A )
#define LW_STRCMP( A, B )         wcscmp( A, B )
#define LW_STRICMP( A, B )      _wcsicmp( A, B )
#define LW_STRNCMP( A, B, C )    wcsncmp( A, B, C )
#define LW_STRNICMP( A, B, C ) _wcsnicmp( A, B, C )
#define LW_STRCAT( A, B )         wcscat( A, B )
#define LW_STRCPY( A, B )         wcscpy( A, B )
#define LW_STRSTR( A, B )         wcsstr( A, B )
#define LW_TOUPPER( A )         towupper( A )
#define LW_TOLOWER( A )         towlower( A )

#endif

/*
 *      String conversion functions.
 */

typedef struct lw_textfuncs {
    int      (*stringToAscii )( LW_CHAR *, char *, int );   /* Converts a  UTF16  string to an ascii  string. */
    int      (*stringToUTF8  )( LW_CHAR *, char *, int );   /* Converts a  UTF16  string to a  UTF8   string. */
    int      (*stringToExport)( LW_CHAR *, char *, int );   /* Converts a  UTF16  string to an export string. */
    int      (*asciiToString )( char *, LW_CHAR *, int );   /* Converts an ascii  string to a  UTF16  string. */
    int      (*uTF8ToString  )( char *, LW_CHAR *, int );   /* Converts a  UTF8   string to a  UTF16  string. */
    int      (*importToString)( char *, LW_CHAR *, int );   /* Converts an import string to a  UTF16  string. */
    LW_CHAR *(*stringCopy    )( LW_CHAR * );                /* Allocates a new string and copies to it.       */
    int      (*stringFree    )( LW_CHAR * );                /* Free a string that was allocated.              */
} LWTEXTFUNCS;

#ifdef __cplusplus
}
#endif

#endif
