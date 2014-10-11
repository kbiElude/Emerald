/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <comdef.h>
#include <Dbt.h>

#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_types.h"
#include "webcam/webcam_constants.h"
#include "webcam/webcam_device.h"
#include "webcam/webcam_manager.h"

/** Internal type definitions */
typedef struct
{
    void*                                    callback_argument;
    PFNWEBCAMMANAGERNOTIFICATIONCALLBACKPROC pfn_callback;
} _webcam_manager_callback_info;

typedef struct
{
    /** COM pointers */
    ICreateDevEnum* create_dev_enum_ptr;
    IEnumMoniker*   video_input_device_enum_ptr;

    /* Containers */
    system_resizable_vector devices;

    /* Callbacks */
    system_resizable_vector registered_device_arrival_callbacks;     /* Holds _webcam_manager_callback_info* */
    system_resizable_vector registered_device_list_changed_callbacks;/* Holds _webcam_manager_callback_info* */
    system_resizable_vector registered_device_removal_callbacks;     /* Holds _webcam_manager_callback_info* */

    system_critical_section cs;

} _webcam_manager_data;

/** Internal variables */
_webcam_manager_data* webcam_manager_data_ptr = NULL;

/** Forward declarations */
PRIVATE void _deinit_webcam_manager_data_ptr();
PRIVATE void _init_webcam_manager_data_ptr();
PRIVATE void _webcam_manager_clear_stored_video_inputs();
PRIVATE bool _webcam_manager_refresh_available_video_inputs();


/** Deinitializes internal webcam_manager_data_ptr variable */
PRIVATE void _deinit_webcam_manager_data_ptr()
{
    /* Release all video inputs */
    _webcam_manager_clear_stored_video_inputs();

    /* Release the devices vector */
    system_resizable_vector_release(webcam_manager_data_ptr->devices);
    webcam_manager_data_ptr->devices = NULL;

    /* Release callback vectors . Become an owner of cs before continuing */
    system_critical_section_enter  (webcam_manager_data_ptr->cs);

    unsigned int n_device_arrival_callback_handlers      = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_arrival_callbacks);
    unsigned int n_device_list_changed_callback_handlers = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_list_changed_callbacks);
    unsigned int n_device_removal_callback_handlers      = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_removal_callbacks);

    if (n_device_arrival_callback_handlers != 0)
    {
        LOG_ERROR("Warning: [%d] device arrival handlers registered during deinitialization of webcam manager!", n_device_arrival_callback_handlers);

        while (system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_arrival_callbacks) != 0)
        {
            _webcam_manager_callback_info* callback_info = NULL;
            bool                           result_get    = false;

            result_get = system_resizable_vector_pop(webcam_manager_data_ptr->registered_device_arrival_callbacks, &callback_info);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve callback info.");

            delete callback_info;
        }
    }

    if (n_device_list_changed_callback_handlers != 0)
    {
        LOG_ERROR("Warning: [%d] device list changed handlers registered during deinitialization of webcam manager!", n_device_list_changed_callback_handlers);

        while (system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_list_changed_callbacks) != 0)
        {
            _webcam_manager_callback_info* callback_info = NULL;
            bool                           result_get    = false;
            
            result_get = system_resizable_vector_pop(webcam_manager_data_ptr->registered_device_list_changed_callbacks, &callback_info);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve callback info.");

            delete callback_info;
        }
    }

    if (n_device_removal_callback_handlers != 0)
    {
        LOG_ERROR("Warning: [%d] device removal handlers registered during deinitialization of webcam manager!", n_device_removal_callback_handlers);

        while (system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_removal_callbacks) != 0)
        {
            _webcam_manager_callback_info* callback_info = NULL;
            bool                           result_get    = false;
            
            result_get = system_resizable_vector_pop(webcam_manager_data_ptr->registered_device_removal_callbacks, &callback_info);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve callback info.");

            delete callback_info;
        }
    }

    system_resizable_vector_release(webcam_manager_data_ptr->registered_device_arrival_callbacks);
    system_resizable_vector_release(webcam_manager_data_ptr->registered_device_list_changed_callbacks);
    system_resizable_vector_release(webcam_manager_data_ptr->registered_device_removal_callbacks);

    webcam_manager_data_ptr->registered_device_arrival_callbacks = NULL;
    webcam_manager_data_ptr->registered_device_removal_callbacks = NULL;

    system_critical_section_leave  (webcam_manager_data_ptr->cs);
    system_critical_section_release(webcam_manager_data_ptr->cs);

    webcam_manager_data_ptr->cs = NULL;
}

/** Initializes webcam manager data ptr. */
PRIVATE void _init_webcam_manager_data_ptr()
{
    webcam_manager_data_ptr->create_dev_enum_ptr                      = NULL;
    webcam_manager_data_ptr->cs                                       = system_critical_section_create();
    webcam_manager_data_ptr->devices                                  = system_resizable_vector_create(BASE_WEBCAMS_AMOUNT,          sizeof(webcam_device) );
    webcam_manager_data_ptr->registered_device_arrival_callbacks      = system_resizable_vector_create(BASE_WEBCAM_CALLBACKS_AMOUNT, sizeof(_webcam_manager_callback_info*) );
    webcam_manager_data_ptr->registered_device_list_changed_callbacks = system_resizable_vector_create(BASE_WEBCAM_CALLBACKS_AMOUNT, sizeof(_webcam_manager_callback_info*) );
    webcam_manager_data_ptr->registered_device_removal_callbacks      = system_resizable_vector_create(BASE_WEBCAM_CALLBACKS_AMOUNT, sizeof(_webcam_manager_callback_info*) );
    webcam_manager_data_ptr->video_input_device_enum_ptr              = NULL;
}

/** Clears webcam_manager_data_ptr->devices resizable vector, releasing all webcam devices held at the time of call.
 *
 ***/
PRIVATE void _webcam_manager_clear_stored_video_inputs()
{
    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        while (system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices) > 0)
        {
            webcam_device device     = NULL;
            bool          result_get = false;
            
            result_get = system_resizable_vector_pop(webcam_manager_data_ptr->devices, &device);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve device.");

            webcam_device_release(device);
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API webcam_device webcam_manager_get_device_by_name(__in __notnull system_hashed_ansi_string name)
{
    webcam_device result = (webcam_device) NULL;

    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        unsigned int n_webcams = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices);

        for (unsigned int n_webcam = 0; n_webcam < n_webcams; ++n_webcam)
        {
            webcam_device device     = NULL;
            bool          result_get = false;
            
            result_get = system_resizable_vector_get_element_at(webcam_manager_data_ptr->devices, n_webcam, &device);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve device instance.");

            if (system_hashed_ansi_string_is_equal_to_hash_string( webcam_device_get_name(device), name) )
            {
                result = device;

                break;
            }
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API webcam_device webcam_manager_get_device_by_path(__in __notnull system_hashed_ansi_string path)
{
    webcam_device result = (webcam_device) NULL;

    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        unsigned int n_webcams = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices);

        for (unsigned int n_webcam = 0; n_webcam < n_webcams; ++n_webcam)
        {
            webcam_device device     = NULL;
            bool          result_get = false;
            
            result_get = system_resizable_vector_get_element_at(webcam_manager_data_ptr->devices, n_webcam, &device);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve device instance.");

            if (system_hashed_ansi_string_is_equal_to_hash_string( webcam_device_get_path(device), path) )
            {
                result = device;

                break;
            }
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API webcam_device webcam_manager_get_device_by_index(__in uint32_t n_camera)
{
    webcam_device result = (webcam_device) NULL;

    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        system_resizable_vector_get_element_at(webcam_manager_data_ptr->devices, n_camera, &result);
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_manager_get_device_index(__in webcam_device device, __out __notnull uint32_t* out_result)
{
    bool result = false;

    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        *out_result = (uint32_t) system_resizable_vector_find(webcam_manager_data_ptr->devices, device);
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);

    return result;
}

/** Please see header for specification */
PUBLIC volatile void _webcam_manager_on_device_arrival(void*)
{
    /* Executed from a worker thread */
    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        unsigned int n_devices_before_refresh = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices);

        if (!_webcam_manager_refresh_available_video_inputs() )
        {
            LOG_FATAL("Could not refresh available video inputs after a new device has been to reported to have arrived.");
        }

        if (n_devices_before_refresh != system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices) )
        {
            /* Call back subscribers and let them know about the event */
            unsigned int n_callbacks = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_arrival_callbacks);

            for (unsigned int n_callback = 0; n_callback < n_callbacks; ++n_callback)
            {
                _webcam_manager_callback_info* callback_info = NULL;
                bool                           result_get    = false;
                
                result_get = system_resizable_vector_get_element_at(webcam_manager_data_ptr->registered_device_arrival_callbacks, n_callback, &callback_info);
                ASSERT_DEBUG_SYNC(result_get, "Could not retrieve callback info.");

                callback_info->pfn_callback(callback_info->callback_argument);
            }
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);
}

/** Please see header for specification */
PUBLIC volatile void _webcam_manager_on_device_removal(void*)
{
    /* Executed from a worker thread */
    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        unsigned int n_devices_before_refresh = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices);

        if (!_webcam_manager_refresh_available_video_inputs() )
        {
            LOG_FATAL("Could not refresh available video inputs after a new device has been to reported to have arrived.");
        }

        if (n_devices_before_refresh != system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices) )
        {
            /* Call back subscribers and let them know about the event */
            unsigned int n_callbacks = system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->registered_device_removal_callbacks);

            for (unsigned int n_callback = 0; n_callback < n_callbacks; ++n_callback)
            {
                _webcam_manager_callback_info* callback_info = NULL;
                bool                           result_get    = false;

                result_get = system_resizable_vector_get_element_at(webcam_manager_data_ptr->registered_device_removal_callbacks, n_callback, &callback_info);
                ASSERT_DEBUG_SYNC(result_get, "Could not retrieve callback info.");

                callback_info->pfn_callback(callback_info->callback_argument);
            }
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);
}

/** Please see header for specification */
PRIVATE bool _webcam_manager_refresh_available_video_inputs()
{
    bool func_result = true;
    
    ASSERT_ALWAYS_SYNC(webcam_manager_data_ptr != NULL, "Cannot refresh available video input - module not initialized.");
    if (webcam_manager_data_ptr != NULL)
    {
        system_critical_section_enter(webcam_manager_data_ptr->cs);

        /* Create video input device enumerator */
        HRESULT result = webcam_manager_data_ptr->create_dev_enum_ptr->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &webcam_manager_data_ptr->video_input_device_enum_ptr, 0);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not obtain video input device enumerator - no cameras available?");
        if (FAILED(result) )
        {
            func_result = false;

            goto end;
        }

        /* Enumerate. For some cases (like with VHScrCam) we need to set up a time-out for BindToObject() request. Therefore
         * we also need to set up a IBindCtx instance and set up the time-out */
        VARIANT                 camera_name_variant;
        IBindCtx*               bind_ctx_ptr         = NULL;
        system_resizable_vector found_camera_paths   = system_resizable_vector_create(4, sizeof(system_hashed_ansi_string) );
        IMoniker*               moniker_ptr          = NULL;
        IPropertyBag*           property_bag_ptr     = NULL;

        VariantInit(&camera_name_variant);

        result = ::CreateBindCtx(0, &bind_ctx_ptr);
        
        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not create bind context.");
        if (SUCCEEDED(result) )
        {
            BIND_OPTS3 bind_options;

            bind_options.cbStruct = sizeof(BIND_OPTS3);

            /* Update bind time-out */
            result = bind_ctx_ptr->GetBindOptions(&bind_options);
            ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not retrieve current bind options from the bind context impl.");

            bind_options.dwTickCountDeadline = CAMERA_BINDING_TIMEOUT;

            result = bind_ctx_ptr->SetBindOptions(&bind_options);
            ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not update bind options for the bind context impl.");

            /* Carry on with enumeration .. Make sure we only operate on real devices. */
            while (webcam_manager_data_ptr->video_input_device_enum_ptr->Next(1, &moniker_ptr, 0) != S_FALSE)
            {
                result = moniker_ptr->BindToStorage(bind_ctx_ptr, 0, IID_IPropertyBag, (void**) &property_bag_ptr);

                ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not bind video input device moniker to property bag.");
                if (SUCCEEDED(result) )
                {
                    result = property_bag_ptr->Read(L"FriendlyName", &camera_name_variant, 0);

                    ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not obtain video input's friendly name.");
                    if (SUCCEEDED(result) )
                    {
                        _bstr_t                   camera_name_bstr(camera_name_variant);
                        const char*               camera_name                    = (const char*) camera_name_bstr;
                        system_hashed_ansi_string camera_name_hashed_ansi_string = system_hashed_ansi_string_create(camera_name);

                        /* Check if there is a DevicePath property - this lets us distinguish between real hardware and software implementations
                         * which can lock up BindToObject() if badly implemented (eg. VhScrCam) */
                        VARIANT device_path_variant;

                        VariantInit(&device_path_variant);

                        if (SUCCEEDED(property_bag_ptr->Read(L"DevicePath", &device_path_variant, 0) ))
                        {
                            /* Cache the name, so we can later on check what cameras were removed */
                            _bstr_t                   device_path_bstr              (device_path_variant);
                            const char*               device_path                   (device_path_bstr);
                            system_hashed_ansi_string device_path_hashed_ansi_string(system_hashed_ansi_string_create(device_path));

                            system_resizable_vector_push(found_camera_paths, device_path_hashed_ansi_string);

                            /* Add the new device, if not already available */
                            if (webcam_manager_get_device_by_path(device_path_hashed_ansi_string) == NULL)
                            {
                                IBaseFilter* camera_base_filter = NULL;

                                result = moniker_ptr->BindToObject(bind_ctx_ptr, 0, IID_IBaseFilter, (void**) &camera_base_filter);
                        
                                if (SUCCEEDED(result) )
                                {
                                    webcam_device new_device = webcam_device_create(camera_name_hashed_ansi_string, device_path_hashed_ansi_string, camera_base_filter);

                                    ASSERT_ALWAYS_SYNC(new_device != NULL, "Could not create webcam device [%s] instance", camera_name);
                                    if (new_device != NULL)
                                    {
                                        system_resizable_vector_push(webcam_manager_data_ptr->devices, new_device);
                                    }
                                }
                                else
                                {
                                    LOG_ERROR("Could not retrieve base filter ptr for camera [%s]", camera_name);
                                }                            
                            }
                        }

                        VariantClear(&device_path_variant);
                        VariantClear(&camera_name_variant);
                    }

                    /* Safe to release the property bag impl. */
                    property_bag_ptr->Release();
                }

                /* Safe to release the moniker */
                moniker_ptr->Release();
            }
            bind_ctx_ptr->Release();
        }

        /* Now that the enumeration is over, make sure all cached devices are in the found friendly name vector.
         * If not, that means a given camera has been unplugged */
        LOG_INFO("Detection of deleted cameras==>");
        for (unsigned int n_existing_entry = 0; n_existing_entry < system_resizable_vector_get_amount_of_elements(webcam_manager_data_ptr->devices); )
        {
            webcam_device             existing_device                = NULL;
            bool                      result_get                     = false;
            
            result_get = system_resizable_vector_get_element_at(webcam_manager_data_ptr->devices, n_existing_entry, &existing_device);
            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve existing device instance.");

            system_hashed_ansi_string existing_device_path           = webcam_device_get_path(existing_device);
            unsigned int              n_paths                        = system_resizable_vector_get_amount_of_elements(found_camera_paths);
            bool                      has_found_existing_device_name = false;

            for (unsigned int n_path = 0; n_path < n_paths; ++n_path)
            {
                system_hashed_ansi_string found_path = NULL;
                
                result_get = system_resizable_vector_get_element_at(found_camera_paths, n_path, &found_path);
                ASSERT_DEBUG_SYNC(result_get, "Could not retrieve friendly name for found device.");
                
                if (system_hashed_ansi_string_is_equal_to_hash_string(found_path, existing_device_path) )
                {
                    has_found_existing_device_name = true;

                    break;
                }
            }

            if (!has_found_existing_device_name)
            {
                LOG_INFO("Freeing %p", existing_device);
                webcam_device_release(existing_device);

                system_resizable_vector_delete_element_at(webcam_manager_data_ptr->devices, n_existing_entry);
            }
            else
            {
                ++n_existing_entry;
            }
        }
        LOG_INFO("<==Detection of deleted cameras");
end:
        system_critical_section_leave  (webcam_manager_data_ptr->cs);
        system_resizable_vector_release(found_camera_paths);
    }

    return func_result;
}

/** Please see header for specification */
PUBLIC void webcam_manager_register_for_notifications(__in webcam_manager_notification notification_type, __in __notnull PFNWEBCAMMANAGERNOTIFICATIONCALLBACKPROC pfn_callback, __in void* callback_argument)
{
    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        switch (notification_type)
        {
            case WEBCAM_MANAGER_NOTIFICATION_WEBCAM_ADDED:
            {
                _webcam_manager_callback_info* callback_info = new (std::nothrow) _webcam_manager_callback_info;

                if (callback_info != NULL)
                {
                    callback_info->callback_argument = callback_argument;
                    callback_info->pfn_callback      = pfn_callback;

                    system_resizable_vector_push(webcam_manager_data_ptr->registered_device_arrival_callbacks, callback_info);
                }
                else
                {
                    LOG_FATAL("Out of memory while allocating space for callback info.");
                }

                break;
            }

            case WEBCAM_MANAGER_NOTIFICATION_WEBCAM_DELETED:
            {
                _webcam_manager_callback_info* callback_info = new (std::nothrow) _webcam_manager_callback_info;

                if (callback_info != NULL)
                {
                    callback_info->callback_argument = callback_argument;
                    callback_info->pfn_callback      = pfn_callback;

                    system_resizable_vector_push(webcam_manager_data_ptr->registered_device_removal_callbacks, callback_info);
                }
                else
                {
                    LOG_FATAL("Out of memory while allocating space for callback info.");
                }

                break;
            }

            default:
            {
                LOG_ERROR("Unrecognized notification type [%d] requested for registration", notification_type);
            }
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);
}

/** Please see header for specification */
PUBLIC void webcam_manager_unregister_from_notifications(__in webcam_manager_notification notification_type, __in __notnull PFNWEBCAMMANAGERNOTIFICATIONCALLBACKPROC pfn_callback, __in void* callback_argument)
{
    system_critical_section_enter(webcam_manager_data_ptr->cs);
    {
        system_resizable_vector vector_to_use = NULL;

        switch (notification_type)
        {
            case WEBCAM_MANAGER_NOTIFICATION_WEBCAM_ADDED:        vector_to_use = webcam_manager_data_ptr->registered_device_arrival_callbacks;      break;
            case WEBCAM_MANAGER_NOTIFICATION_WEBCAM_DELETED:      vector_to_use = webcam_manager_data_ptr->registered_device_removal_callbacks;      break;
            default:
            {
                LOG_ERROR("Unrecognized notification type [%d] requested for unregistration", notification_type);
            }
        }

        if (vector_to_use != NULL)
        {
            /* Try to find the pair */
            unsigned int n_callbacks    = system_resizable_vector_get_amount_of_elements(vector_to_use);
            bool         callback_found = false;

            for (unsigned int n_callback = 0; n_callback < n_callbacks; ++n_callback)
            {
                _webcam_manager_callback_info* callback_info = NULL;
                bool                           result_get    = false;
                
                result_get = system_resizable_vector_get_element_at(vector_to_use, n_callback, &callback_info);
                ASSERT_DEBUG_SYNC(result_get, "Could not retrieve callback info.");

                if (callback_info                    != NULL &&
                    callback_info->callback_argument == callback_argument &&
                    callback_info->pfn_callback      == pfn_callback)
                {
                    system_resizable_vector_delete_element_at(vector_to_use, n_callback);

                    callback_found = true;
                    break;
                }
            }

            if (!callback_found)
            {
                LOG_ERROR("Could not find requested handler to unregister!");
            }
        }
    }
    system_critical_section_leave(webcam_manager_data_ptr->cs);
}

/** Please see header for specification */
PUBLIC void _webcam_manager_init()
{
    ASSERT_ALWAYS_SYNC(webcam_manager_data_ptr == NULL, "Webcam manager already initialized.");

    if (webcam_manager_data_ptr == NULL)
    {
        webcam_manager_data_ptr = new (std::nothrow) _webcam_manager_data;

        ASSERT_ALWAYS_SYNC(webcam_manager_data_ptr != NULL, "Out of memory while trying to allocate space for webcam manager data.");
        if (webcam_manager_data_ptr != NULL)
        {
            _init_webcam_manager_data_ptr();

            /* Initialize COM for the current thread. */
            ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

            /* Retrieve ICreateDevEnum impl */
            HRESULT result = ::CoCreateInstance(CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**) &webcam_manager_data_ptr->create_dev_enum_ptr);

            ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not obtain pointer to ICreateDevEnum implementation.");
            if (FAILED(result) )
            {
                goto failed_end;
            }

            /* Enumerate available video inputs */
            _webcam_manager_refresh_available_video_inputs();
        }
    }

    goto end;

failed_end:;

end:;
}

/** Please see header for specification  */
PUBLIC void _webcam_manager_deinit()
{
    ASSERT_ALWAYS_SYNC(webcam_manager_data_ptr != NULL, "Cannot deinit - webcam manager not initialized.");

    if (webcam_manager_data_ptr != NULL)
    {
        _deinit_webcam_manager_data_ptr();
    }

    delete webcam_manager_data_ptr;
    webcam_manager_data_ptr = NULL;
}
