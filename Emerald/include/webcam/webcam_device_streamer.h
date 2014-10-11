/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef WEBCAM_DEVICE_STREAMER_H
#define WEBCAM_DEVICE_STREAMER_H

#include "webcam/webcam_types.h"

#ifdef INCLUDE_WEBCAM_MANAGER

/** TODO. Stops streaming (if active) so make sure you issue a play request if you need playback to continue
 *        after calling this function.
 */
PUBLIC bool webcam_device_streamer_change_media_info(__in __notnull webcam_device_streamer, 
                                                     __in __notnull webcam_device_media_info* media_info);

/** TODO.
 *
 *  Sample grabber media info may be NULL, in which case the sample grabber filter will be fed media_info.
 **/
PUBLIC webcam_device_streamer webcam_device_streamer_create(__in    __notnull   system_hashed_ansi_string                  device_name, 
                                                            __in    __notnull   IBaseFilter*                               base_filter_ptr,
                                                            __inout __notnull   webcam_device_media_info*                  media_info, 
                                                            __in    __maybenull webcam_device_media_info*                  sample_grabber_media_info,
                                                            __in    __notnull   PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC callback,
                                                            __in                void*                                      user_arg);

/** TODO */
PUBLIC PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC webcam_device_streamer_get_callback(__in __notnull webcam_device_streamer streamer);

/** TODO */
PUBLIC void* webcam_device_streamer_get_callback_user_arg(__in __notnull webcam_device_streamer streamer);

/** TODO */
PUBLIC webcam_device_media_info webcam_device_streamer_get_media_info(__in __notnull webcam_device_streamer streamer);

/** TODO */
PUBLIC EMERALD_API bool webcam_device_streamer_pause(__in __notnull webcam_device_streamer streamer);

/** TODO */
PUBLIC EMERALD_API bool webcam_device_streamer_play(__in __notnull webcam_device_streamer streamer);

/** TODO */
PUBLIC void webcam_device_streamer_release(__in __notnull webcam_device_streamer);

/** TODO */
PUBLIC void webcam_device_streamer_set_callback_user_argument(__in __notnull webcam_device_streamer,
                                                              __in           void*);

/** TODO */
PUBLIC EMERALD_API bool webcam_device_streamer_stop(__in __notnull webcam_device_streamer streamer);

#endif /* INCLUDE_WEBCAM_MANAGER */

#endif /* WEBCAM_DEVICE_STREAMER_H */
