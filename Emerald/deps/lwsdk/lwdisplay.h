/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWDISPLAY.H -- LightWave Host Display Info
 *
 * The host application provides a variety of services for getting user
 * input, but if all else fails the plug-in may need to open windows.
 * Since it runs in the host's context, it needs to get the host's display
 * information to do this.  This info, which can be normally accessed
 * with the "Host Display Info" global service, contains information about
 * the windows and display context used by the host.  If this ID yeilds a
 * null pointer, the server is probably running in a batch mode and has no
 * display context.
 */
#ifndef LWSDK_DISPLAY_H
#define LWSDK_DISPLAY_H

#ifdef _MSWIN
 #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0400
 #endif
  #include <windows.h>
#endif

#ifdef _XGL
 #include <X11/Xlib.h>
#endif

#if defined( _MACOS)
 #if defined( __MACH__)
  #include <ApplicationServices/ApplicationServices.h>
 #else
  #include <Quickdraw.h>
 #endif
#endif


/*
 * The fields of the HostDisplayInfo structure vary from system to system,
 * but all include the window pointer of the main application window or
 * null if there is none.  On X systems, the window session handle is
 * passed.  On Win32 systems, the application instance is provided, even
 * though it belongs to the host and is probably useless.
 */
#define LWHOSTDISPLAYINFO_GLOBAL	"Host Display Info"

#if defined(_MACOS) && (defined(__OBJC__) || defined(__OBJCPP__))
    /* when using this header for _MACOS, compile your source as Objective C or Objective CPP */
    @class LWSDKView;
#endif

typedef struct st_HostDisplayInfo {
    #ifdef _MSWIN
	HANDLE		 instance;
	HWND		 window;
    #endif
    #ifdef _XGL
	Display		*xsys;
	Window		 window;
    #endif
    #ifdef _MACOS
        #if defined(__OBJC__) || defined(__OBJCPP__)
            union {
                WindowPtr  window; /* for Carbon-based host pre 9.6.1*/
                LWSDKView *view;   /* Cocoa-based host 9.6.1+ */
            } host;
        #else
            WindowPtr	   window; /* for Carbon-based host pre 9.6.1*/
        #endif
    #endif
} HostDisplayInfo;

#endif

