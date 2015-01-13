/*
 * LWSDK Header File
 * Copyright 2011, NewTek, Inc.
 *
 * lwhumaninterface_hid -- Human Interface HID manager/device plugin support
 */

#ifndef LWHUMANINTERFACE_HID_H_INCLUDED
#define LWHUMANINTERFACE_HID_H_INCLUDED

#include <lwhumaninterface.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////
// The HID manager has its own SDK thru which additional client devices can be added
// The devices must be HID compatible devices, though, and have a vendor/product id combination that is not already
// handled by another device client.

typedef struct st_LWHumanInterfaceHIDHandler LWHumanInterfaceHIDHandler;
typedef struct st_LWHumanInterfaceHIDCreateContext LWHumanInterfaceHIDCreateContext;
typedef struct st_LWHumanInterfaceHIDAccess LWHumanInterfaceHIDAccess;
typedef struct st_LWHumanInterfaceHIDReceiveAccess LWHumanInterfaceHIDReceiveAccess;

#define LWHUMANINTERFACE_HID_HCLASS "LWHumanInterfaceHIDHandler"
#define LWHUMANINTERFACE_HID_ICLASS "LWHumanInterfaceHIDInterface"
#define LWHUMANINTERFACE_HID_GCLASS "LWHumanInterfaceHIDGizmo"
#define LWHUMANINTERFACE_HID_VERSION 1

typedef void *LWHumanInterfaceHIDID;

struct st_LWHumanInterfaceHIDCreateContext
{
    LWHumanInterfaceHIDID in_hid;
    unsigned int vendor_id;
    unsigned int product_id;
    const char *manufacturer_name;
    const char *product_name;
    const char *transport_name;
    const char *serial_string;
    char reserved[32];
};


#define LWHumanInterfaceHID_ReportType_INPUT   0
#define LWHumanInterfaceHID_ReportType_OUTPUT  1
#define LWHumanInterfaceHID_ReportType_FEATURE 2

// HID access
struct st_LWHumanInterfaceHIDAccess
{
    // access to host-side device functions
    const LWHumanInterfaceDeviceAccess *device_access;
    
    LWHumanInterfaceDeviceID (*getDevice)( LWHumanInterfaceHIDID in_hid);
    
    // get a single report, return 1 if successful
    int (*getReport)( LWHumanInterfaceHIDID in_hid, unsigned int in_report_type, unsigned int in_report_id, unsigned char *out_report_data, unsigned int *in_report_data_length);

    // set a single report, return 1 if successful
    int (*setReport)( LWHumanInterfaceHIDID in_hid, unsigned int in_report_type, unsigned int in_report_id, unsigned char *in_report_data, unsigned int in_report_data_length);
};

struct st_LWHumanInterfaceHIDReceiveAccess
{
    unsigned int report_id;
    unsigned char *report_data;
    unsigned int report_length;
};

// a custom HID handler must supply these routines
// When asked to create an instance, a create context specification is provided.
// The handler can either create valid private data if it supports the given device, or not if it does not.
// An instance will remain for each specification that was supported.
// During configuration, input and output tracks can be created.
struct st_LWHumanInterfaceHIDHandler
{
	LWInstanceFuncs *inst; // create/destroy/descln/etc.
    // create context is a LWHumanInterfaceHIDCreateContext

    // configure the given device (setup input/output tracks)
    int (*configure)( LWInstance in_inst, const LWHumanInterfaceHIDAccess *in_access);

    // handle incoming HID reports (convert to input tracks).
    // handle this quickly to avoid delaying further input handling.
    int (*receive)( LWInstance in_inst, const LWHumanInterfaceHIDReceiveAccess *in_access);

    // place output tracks into one or more buffers to send to the device.
    // The access structure handles to dirty work.
    int (*send)( LWInstance in_inst);

    // fill in a pixel map to represent the device
    // A hint of its purpose is provided so that the handler can supply the most appropriate image.
    // (see LWHUMANINTERFACE_IMAGE_PURPOSE_... defines for available purposes)
    int (*image)( LWInstance in_inst, int in_purpose, LWPixmapID out_pixmap);

    // fill in the given character buffer with a UTF-8 signature that attempts to uniquely
    // identify the given LWInstance from the point of view of the device it represents.
    // For example, some devices have a unique serial code embedded within them that distinquishes
    // one from another.
    // This signature is used by the manager to help create a unique signature for device it wants to manage.
    // Some devices have no such uniqueness about them and therefore do not have to provide one.
    // In this case, the handler case either not provide this callback or can return 0 when called.
    // Return 1 when the given buffer has been filled in appropriately.
    int (*signature)( LWInstance in_inst, char *out_signature, unsigned int in_max_bytes);
};

#ifdef __cplusplus
}
#endif

#endif /* LWHUMANINTERFACE_HID_H_INCLUDED */

