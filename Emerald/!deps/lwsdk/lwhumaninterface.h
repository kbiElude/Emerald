/*
 * LWSDK Header File
 * Copyright 2011, NewTek, Inc.
 *
 * lwhumaninterface -- Human Interface input/output with LightWave

 A device being connected is a state handled by the device and its underlying driving mechanism.
 For example, a device may be powered on, but a bluetooth or network connection to it may not be
 operational.  When the device is actually available to get data in/out of the LightWave Human
 Interface system, then it should notify the system that the device is connected.
 Similarly, if the device loses power (low batteries) or its communication with the underlying driving
 mechanism is severed, then the LightWave Human Interface system should be told the device is disconnected.

 Starting and Stopping a device is handled independent of its behind-the-scenes connected state.
 However, only a started device should be concerned with notifying the LightWave Human Interface
 system of its connected state.  Therefore, once a device has been told to start, it must tell the the system
 that it is connected before I/O can pass through it.  The device does not need to indicate when it is disconnected
 after being told to stop, though.

 A device manager that is told to start should add devices for all those that it knows of at that time.
 As devices come and go, they can be added/removed.
 Each device that is added and started will be told to start.
 Each device that is started and removed will be told to stop.

 There may be two ways to interpret configuaritons: 1) the user has a choice from the application; 2) the user manipualtes the
 device directly.  The latter seems the most useful: a real world device may have a switch to change modes of operation;
 a logical device, like a touch screen panel, may have a menu of choices known to the device.  Rarely, if ever, do I foresee
 the user wanting to change the configuration from within LightWave.

 */

#ifndef LWHUMANINTERFACE_H_INCLUDED
#define LWHUMANINTERFACE_H_INCLUDED

#include <lwhandler.h>
#include <lwserver.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The manager class provides access to devices. */

#define LWHUMANINTERFACE_MANAGER_HCLASS "LWHumanInterfaceManagerHandler"
#define LWHUMANINTERFACE_MANAGER_ICLASS "LWHumanInterfaceManagerInterface"
#define LWHUMANINTERFACE_MANAGER_GCLASS "LWHumanInterfaceManagerGizmo"
#define LWHUMANINTERFACE_MANAGER_VERSION 1

#define LWHUMANINTERFACE_DEVICE_VERSION 1

typedef void *LWStringID; // (need something when our string object is available (in lwstring.h?))

typedef void *LWHumanInterfaceManagerID;
typedef void *LWHumanInterfaceDeviceID;
typedef void *LWHumanInterfaceTrackID;
typedef unsigned int LWDeviceTrackID;
typedef unsigned short LWTrackRecordID;
typedef unsigned int LWTimeStampMS; // timestamps are in milliseconds, up to 1000 samples per second uniqueness

typedef struct st_LWHumanInterfaceTrackAccess LWHumanInterfaceTrackAccess;
typedef struct st_LWHumanInterfaceDeviceAccess LWHumanInterfaceDeviceAccess;
typedef struct st_LWHumanInterfaceManagerAccess LWHumanInterfaceManagerAccess;

typedef struct st_LWHumanInterfaceManagerHandler LWHumanInterfaceManagerHandler;
typedef struct st_LWHumanInterfaceDeviceHandler LWHumanInterfaceDeviceHandler;
typedef struct st_LWHumanInterfaceDeviceCreateContext LWHumanInterfaceDeviceCreateContext;

#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_UNKNOWN           0
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_SIGNED_INT8       1
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_UNSIGNED_INT8     2
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_SIGNED_INT16      3
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_UNSIGNED_INT16    4
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_SIGNED_INT32      5
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_UNSIGNED_INT32    6
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_FLOAT32           7
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_FLOAT64           8
#define LWHUMANINTERFACE_TRACK_RECORD_FORMAT_BOOL              9


// change to configuration
#define LWHUMANINTERFACE_NOTIFY_CONFIGURATION 1
// connection has been made
#define LWHUMANINTERFACE_NOTIFY_CONNECTED 2
// connection has been broken
#define LWHUMANINTERFACE_NOTIFY_DISCONNECTED 3

// track access
struct st_LWHumanInterfaceTrackAccess
{
    // create a track.  Storage of the track is up to the client-side device
    LWHumanInterfaceTrackID (*createTrack)( LWHumanInterfaceDeviceID in_device_id, LWDeviceTrackID in_track_id, const char *in_display_name, int in_record_format, int in_event_driven);

    // destroy the given track.  Storage of the track is up to the client-side device
    void (*destroyTrack)( LWHumanInterfaceTrackID in_track);

    // Obtain the next available RecordID to be filled.
    LWTrackRecordID (*advance)( LWHumanInterfaceTrackID in_track);

    // Obtain the RecordID of the earliest available record
    LWTrackRecordID (*earliest)( LWHumanInterfaceTrackID in_track);

    // Obtain the RecordID of the latest available record
    LWTrackRecordID (*latest)( LWHumanInterfaceTrackID in_track);

    // Obtain the next RecordID after the given RecordID
    LWTrackRecordID (*next)( LWHumanInterfaceTrackID in_track, LWTrackRecordID in_record);

    // Obtain the previous RecordID before the given RecordID
    LWTrackRecordID (*previous)( LWHumanInterfaceTrackID in_track, LWTrackRecordID in_record);

    // set the byte size of each record buffer and the number of records for a track
    void (*setSize)( LWHumanInterfaceTrackID in_track, unsigned int in_record_size, unsigned short in_num_records);

    // obtain the byte size of each record buffer and the number of record for a track.
    void (*getSize)( LWHumanInterfaceTrackID in_track, unsigned int *out_record_size, unsigned short *out_num_records);

    // obtain direct access to the buffer for a given record.
    int (*access)(  LWHumanInterfaceTrackID in_track, LWTrackRecordID in_record, unsigned char **out_data_ptr, LWTimeStampMS **out_timestamp_ptr);

    // obtain the device for this track
    LWHumanInterfaceDeviceID (*device)( LWHumanInterfaceTrackID in_track);

};

// device access
struct st_LWHumanInterfaceDeviceAccess
{
    // access to host-side track functions
    const LWHumanInterfaceTrackAccess *track_access;

    // obtain the manager for this device
    LWHumanInterfaceManagerID (*manager)( LWHumanInterfaceDeviceID in_device);

    // notify the human interface system of a change it would be interested in.  see the LWHUMANINTERFACE_NOTIFY_... defines.
    void (*notify)( LWHumanInterfaceDeviceID in_device, int in_notification);

    // adds an already created track as an input track for the device.
    void (*addInputTrack)( LWHumanInterfaceDeviceID in_device, LWHumanInterfaceTrackID in_track);

    // adds an already created track as an output track for the device.
    void (*addOutputTrack)( LWHumanInterfaceDeviceID in_device, LWHumanInterfaceTrackID in_track);

    // returns the number of input tracks supported.
    unsigned int (*countInputTracks)( LWHumanInterfaceDeviceID in_device);

    // input track access (returns 0 for non-existent tracks)
    // The client is responsible for track storage access.
    LWHumanInterfaceTrackID (*getInputTrackAtIndex)( LWHumanInterfaceDeviceID in_device, unsigned int in_index);

    // find the track handle for a given track identifier
    LWHumanInterfaceTrackID (*findInputTrack)( LWHumanInterfaceDeviceID in_device, LWDeviceTrackID in_track_id);

    // returns the number of output tracks supported.
    unsigned int (*countOutputTracks)( LWHumanInterfaceDeviceID in_device);

    // output track access (returns 0 for non-existent tracks)
    LWHumanInterfaceTrackID (*getOutputTrackAtIndex)( LWHumanInterfaceDeviceID in_device, unsigned int in_index);

    // find the track handle for a given track identifier
    LWHumanInterfaceTrackID (*findOutputTrack)( LWHumanInterfaceDeviceID in_device, LWDeviceTrackID in_track_id);

    // get the instance returned by the handler's create routine.
    LWInstance (*getHandlerInstance)( LWHumanInterfaceDeviceID in_device);

};

// manager access
struct st_LWHumanInterfaceManagerAccess
{
    // access to host-side device functions
    const LWHumanInterfaceDeviceAccess *device_access;

//    // create a handler using the given activation function for a specific version
//    LWHumanInterfaceDeviceHandler *(*createDeviceHandler)( ActivateFunc in_device_activation, int version);

    // create an instance of a device attached to the provided manager and handled by the given handler
    LWHumanInterfaceDeviceID (*registerDevice)( LWHumanInterfaceManagerID in_manager, LWHumanInterfaceDeviceHandler *in_handler, void *context);

    // destroy the given device
    void (*unregisterDevice)( LWHumanInterfaceDeviceID in_device);

    // notify the human interface system of a change it would be interested in.  see the LWHUMANINTERFACE_NOTIFY_... defines.
    void (*notify)( LWHumanInterfaceManagerID in_manager, int in_notification);

};

// A device handler create is passed this structure to help it create its device as intended.
// The create routine should return 0 if an instance could not be created for it.
struct st_LWHumanInterfaceDeviceCreateContext
{
    // The device id is useful when accessing the device
    LWHumanInterfaceDeviceID device;

    // Access to the device
    LWHumanInterfaceDeviceAccess *device_access;

    // The context is the same as passed into the registerDevice manager access call.
    void *context;

};

#define LWHUMANINTERFACE_IMAGE_PURPOSE_GENERAL  0
#define LWHUMANINTERFACE_IMAGE_PURPOSE_NODE     1
#define LWHUMANINTERFACE_IMAGE_PURPOSE_SETTINGS 2

// a custom device handler must supply these routines
struct st_LWHumanInterfaceDeviceHandler
{
    LWInstanceFuncs *inst;
    // create context is a LWHumanInterfaceDeviceCreateContext structure.

    // start the device (make it send/receive).  Return 0 if failed to start, else 1
    // The provided access structure is valid until the device is stopped.
    int (*start)( LWInstance in_inst);

    // configure the device (setup input/output tracks)
    int (*configure)( LWInstance in_inst);

    // transfer output tracks data to device.
    int (*flush)( LWInstance in_inst);

    // stop the device (make it stop send/receive).  Return 0 if failed to start, else 1
    int (*stop)( LWInstance in_inst);

    // return the unique signature for the device
    const char *(*signature)( LWInstance in_inst);
    
    // fill in a pixel map to represent the device
    // A hint of its purpose is provided so that the handler can supply the most appropriate image.
    // (see LWHUMANINTERFACE_IMAGE_PURPOSE_... defines for available purposes)
    int (*image)( LWInstance in_inst, int in_purpose, LWPixmapID out_pixmap);

    // obtain a user interface to manipulate this device
    // Only modal panels are presently supported via the options member of LWInterface.
    int (*user_interface)( LWInstance in_inst, LWInterface *in_interface, int in_version);

};

// a custom manager must supply these routines
struct st_LWHumanInterfaceManagerHandler
{
    LWInstanceFuncs *inst;
    // create context is a LWHumanInterfaceManagerID

    // start the manager (devices it finds must be added).  Return 0 if failed to start, else 1
    // The provided access structure is valid until the manager is stopped.
    int (*start)( LWInstance in_inst);

    // configure the manager while it is started
    int (*configure)( LWInstance in_inst, const LWHumanInterfaceManagerAccess *in_access);

    // stop the manager (devices it has added must be removed). Return 0 if failed to stop, else 1
    int (*stop)( LWInstance in_inst);

    // return the unique signature for the manager
    const char *(*signature)( LWInstance in_inst);

    // fill in a pixel map to represent the device
    // A hint of its purpose is provided so that the handler can supply the most appropriate image.
    // (see LWHUMANINTERFACE_IMAGE_PURPOSE_... defines for available purposes)
    int (*image)( LWInstance in_inst, int in_purpose, LWPixmapID out_pixmap);

};

#ifdef __cplusplus
}
#endif

#endif /* LWHUMANINTERFACE_H_INCLUDED */

