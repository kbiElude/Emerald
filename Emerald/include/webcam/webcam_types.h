/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef WEBCAM_TYPES_H
#define WEBCAM_TYPES_H

#include "config.h"

#ifdef INCLUDE_WEBCAM_MANAGER

#include "system/system_types.h"
#include <comdef.h>
#include <DShow.h>
#include "webcam/qedit.h"

/* Handles */
DECLARE_HANDLE(webcam_device);
DECLARE_HANDLE(webcam_device_streamer);

/* Webcam call-back function pointer type definitions */
typedef void (*PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC)(double sample_time, const BYTE* buffer_ptr, long buffer_length, void* user_arg);
typedef void (*PFNWEBCAMMANAGERNOTIFICATIONCALLBACKPROC)  (void*);

/* Webcam manager's notification types */
typedef enum
{
    WEBCAM_MANAGER_NOTIFICATION_WEBCAM_ADDED,
    WEBCAM_MANAGER_NOTIFICATION_WEBCAM_DELETED,
} webcam_manager_notification;

/** Stores information on how data is arranged for each frame that can be reported by the camera. */
typedef enum
{
    WEBCAM_DEVICE_MEDIA_FORMAT_IYUV,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB1,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB24,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB32,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB4,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB555,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB565,
    WEBCAM_DEVICE_MEDIA_FORMAT_RGB8,
    WEBCAM_DEVICE_MEDIA_FORMAT_YUY2,
    WEBCAM_DEVICE_MEDIA_FORMAT_YUYV,

    /* If you add any new media format, make sure to update webcam_types.cc as well */

    WEBCAM_DEVICE_MEDIA_FORMAT_UNKNOWN
} webcam_device_media_format;

/** Media info descriptor - describes a single format of data that the camera can return */
typedef struct
{
    AM_MEDIA_TYPE*             am_media_type;
    DWORD                      bitrate;
    GUID                       data_subtype;
    webcam_device_media_format format; /* human-readable format of data_subtype */
    float                      fps;
    LONG                       height;
    LONG                       width;
    VIDEOINFOHEADER            video_info_header;
} webcam_device_media_info;

/** TODO */
PUBLIC EMERALD_API bool webcam_device_media_info_is_equal(const webcam_device_media_info& in1, const webcam_device_media_info& in2);

/****************************** HELPER FUNCTIONS *******************************/

/** Converts webcam_device_media_format enum to human-readable string.
 *
 *  @param webcam_device_media_format Webcam device media format to convert from.
 *
 *  @return Hashed ansi string representing input value.
 **/
PUBLIC EMERALD_API system_hashed_ansi_string webcam_device_media_format_as_string(__in webcam_device_media_format);

#endif /* INCLUDE_WEBCAM_MANAGER */
#endif /* WEBCAM_TYPES_H */
