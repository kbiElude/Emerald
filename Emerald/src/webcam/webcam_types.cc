/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_hashed_ansi_string.h"
#include "webcam/webcam_types.h"

/** Please see header for description */
PUBLIC EMERALD_API system_hashed_ansi_string webcam_device_media_format_as_string(__in webcam_device_media_format format)
{
    switch (format)
    {
        case WEBCAM_DEVICE_MEDIA_FORMAT_IYUV:    return system_hashed_ansi_string_create("IYUV");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB1:    return system_hashed_ansi_string_create("RGB1");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB24:   return system_hashed_ansi_string_create("RGB24");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB32:   return system_hashed_ansi_string_create("RGB32");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB4:    return system_hashed_ansi_string_create("RGB4");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB555:  return system_hashed_ansi_string_create("RGB555");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB565:  return system_hashed_ansi_string_create("RGB565");
        case WEBCAM_DEVICE_MEDIA_FORMAT_RGB8:    return system_hashed_ansi_string_create("RGB8");
        case WEBCAM_DEVICE_MEDIA_FORMAT_YUY2:    return system_hashed_ansi_string_create("YUY2");
        case WEBCAM_DEVICE_MEDIA_FORMAT_YUYV:    return system_hashed_ansi_string_create("YUYV");
        case WEBCAM_DEVICE_MEDIA_FORMAT_UNKNOWN: 
        default:                                 return system_hashed_ansi_string_create("Unknown");
    }
}

/** TODO */
PUBLIC EMERALD_API bool webcam_device_media_info_is_equal(const webcam_device_media_info& in1, const webcam_device_media_info& in2)
{
    if (in1.am_media_type == in2.am_media_type &&
        in1.bitrate       == in2.bitrate       &&
        in1.data_subtype  == in2.data_subtype  &&
        in1.format        == in2.format        &&
        in1.fps           == in2.fps           &&
        in1.height        == in2.height        &&
        in1.width         == in2.width)
    {
        return true;
    }
    else
    {
        return false;
    }
}