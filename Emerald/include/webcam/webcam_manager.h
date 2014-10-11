/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef WEBCAM_MANAGER_H
#define WEBCAM_MANAGER_H

#include "webcam/webcam_types.h"

#ifdef INCLUDE_WEBCAM_MANAGER

/** Deinitializes webcam manager module. Only called from DLL entry-point */
PUBLIC void _webcam_manager_deinit();

/** TODO */
PUBLIC EMERALD_API webcam_device webcam_manager_get_device_by_index(__in uint32_t);

/** Retrieves a webcam_device instance for a camera described by @param system_hashed_ansi_string name.
 *
 *  @param system_hashed_ansi_string Name of webcam to retrieve webcam_device instance for.
 *
 *  @return Webcam device instance, if webcam was found, NULL otherwise.
 **/
PUBLIC EMERALD_API webcam_device webcam_manager_get_device_by_name(__in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API webcam_device webcam_manager_get_device_by_path(__in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API bool webcam_manager_get_device_index(__in webcam_device, __out __notnull uint32_t*);

/** Initializes webcam manager module. Only called from DLL entry-point */
PUBLIC void _webcam_manager_init();

/** Called back from a thread pool worker thread to notify on new device arrival. Webcam manager then refreshes
 *  its device list and notifies subscribers of the change if it concerned a registered webcam.
 *
 *  @param void* Not used.
 */
PUBLIC volatile void _webcam_manager_on_device_arrival(void*);

/** Called back from a thread pool worker thread to notify on device removal. Webcam manager then refreshes
 *  its device list and notifies subscribers of the change if a new webcam has been installed.
 *
 *  @param void* Not used.
 */
PUBLIC volatile void _webcam_manager_on_device_removal(void*);

/** TODO */
PUBLIC EMERALD_API void webcam_manager_register_for_notifications(__in           webcam_manager_notification, 
                                                                  __in __notnull PFNWEBCAMMANAGERNOTIFICATIONCALLBACKPROC,
                                                                  __in           void*);

/** TODO */
PUBLIC EMERALD_API void webcam_manager_unregister_from_notifications(__in           webcam_manager_notification, 
                                                                     __in __notnull PFNWEBCAMMANAGERNOTIFICATIONCALLBACKPROC,
                                                                     __in           void*);

#endif /* INCLUDE_WEBCAM_MANAGER */
#endif /* WEBCAM_MANAGER_H */