/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef WEBCAM_DEVICE_H
#define WEBCAM_DEVICE_H

#ifdef INCLUDE_WEBCAM_MANAGER

#include "system/system_types.h"
#include "webcam/webcam_types.h"
#include <DShow.h>

/** TODO */
PUBLIC EMERALD_API bool webcam_device_change_streamer_media_info(__in __notnull webcam_device,
                                                                 __in __notnull webcam_device_media_info* media_info,
                                                                 __in __notnull webcam_device_media_info* sample_grabber_media_info);

/** Creates a new webcam device instance that will hold user-provided name and use the provided
 *  IBaseFilter implementation.
 *
 *  Module does not retain the IBaseFilter ptr.
 *
 *  @param system_hashed_ansi_string Name to use for the device.
 *  @param system_hashed_ansi_string Device system path.
 *  @param IBaseFilter*              IBaseFilter ptr to use for the device.
 *
 *  @return Webcam device instance, if successful, NULL otherwise
 **/
PUBLIC webcam_device webcam_device_create(__in __notnull system_hashed_ansi_string, 
                                          __in __notnull system_hashed_ansi_string,
                                          __in __notnull IBaseFilter*);

/** Retrieves media info descriptor. A device usually has many media formats it can support, so use
 *  @param uint32_t argument to control which media format index you want to query for.
 *
 *  @param webcam_device Webcam device to retrieve media info descriptor for.
 *  @param uint32_t      Index of media info descriptor to return. Indexing starts with 0.
 *
 *  @return Media info descriptor if found at @param uint32_t index, or NULL otherwise.
 **/
PUBLIC EMERALD_API webcam_device_media_info* webcam_device_get_media_info(__in __notnull webcam_device, 
                                                                          __in           uint32_t);

/** Retrieves name used for the webcam instance.
 *
 *  @param webcam_device Webcam device instance.
 *
 *  @return Hashed ansi string containing the name.
 **/
PUBLIC EMERALD_API system_hashed_ansi_string webcam_device_get_name(__in __notnull webcam_device);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string webcam_device_get_path(__in __notnull webcam_device);

/** TODO */
PUBLIC EMERALD_API webcam_device_streamer webcam_device_get_streamer(__in __notnull webcam_device);

/** TODO.
 *
 *  This function DOES NOT return the streamer handle on purpose. This is due to the fact that 
 *  in some cases webcam_device_change_streamer_media_info() may reinstantiate the streamer.
 *
 *  In order to get the streamer handle, always use webcam_device_get_streamer() function.
 *
 *  @param sample_grabber_media_info may be null, in which case sample grabber filter will be fed media_info.
 **/
PUBLIC EMERALD_API bool webcam_device_open_streamer(__in __notnull webcam_device                              device,
                                                    __in __notnull webcam_device_media_info*                  media_info, 
                                                    __in __notnull webcam_device_media_info*                  sample_grabber_media_info,
                                                    __in __notnull PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC callback,
                                                    __in           void*                                      user_arg);

/** Releases a webcam device instance. Do not use the object after calling this function.
 *
 *  @param webcam_device Webcam device instance to release.
 *
 **/
PUBLIC void webcam_device_release(__in __notnull webcam_device);

/** TODO */
PUBLIC EMERALD_API bool webcam_device_change_streamer_callback_user_argument(__in __notnull webcam_device,
                                                                             __in           void*);

#endif /* INCLUDE_WEBCAM_MANAGER */

#endif /* WEBCAM_DEVICE_H */
