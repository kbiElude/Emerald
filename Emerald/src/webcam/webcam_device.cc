/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "webcam/webcam_constants.h"
#include "webcam/webcam_device.h"
#include "webcam/webcam_device_streamer.h"
#include "webcam/webcam_types.h"

/** Internal type definitions */
typedef struct
{
    /* IBaseFilter ptr impl that is used for controlling the camera */
    IBaseFilter* base_filter_ptr;
    /** TODO */
    IGraphBuilder* graph_builder_ptr;
    /* Contains webcam_device_media_info* entries. */
    system_resizable_vector medias;
    /* Camera friendly name */
    system_hashed_ansi_string name;
    /* Camera device path */
    system_hashed_ansi_string path;
    /* Streamer instance (if opened by user), which is responsible for streaming camera frames
     * to process' space. Only one streamer is allowed for a camera at a given time
     */
    webcam_device_streamer streamer;
} _webcam_device;


/** Converts a few DirectShow's media subtypes to webcam device-recognized enum values.
 *
 *  @param GUID Subtype to convert from.
 *
 *  @return webcam_device_media_format value if @param data_subtype was recognized,
 *          WEBCAM_DEVICE_MEDIA_FORMAT_UNKNOWN otherwise.
 **/
PRIVATE webcam_device_media_format _convert_media_subtype_to_webcam_device_media_format(__in GUID data_subtype)
{
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_IYUV)  ) {return WEBCAM_DEVICE_MEDIA_FORMAT_IYUV;   }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB1)  ) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB1;   }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB24) ) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB24;  }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB32) ) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB32;  }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB4)  ) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB4;   }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB555)) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB555; }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB565)) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB565; }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_RGB8)  ) {return WEBCAM_DEVICE_MEDIA_FORMAT_RGB8;   }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_YUY2)  ) {return WEBCAM_DEVICE_MEDIA_FORMAT_YUY2;   }else
    if (::IsEqualGUID(data_subtype, MEDIASUBTYPE_YUYV)  ) {return WEBCAM_DEVICE_MEDIA_FORMAT_YUYV;   }else
    {
        return WEBCAM_DEVICE_MEDIA_FORMAT_UNKNOWN;
    }
}

/** Initializes webcam_device_media_info structure with user-provided values. Also, fills format field using the provided data.
 *
 *  @param out_result        Deref will be used to store the result. Cannot be NULL.
 *  @param AM_MEDIA_TYPE*   
 *  @param data_subtype      Data subtype to use for the descriptor.
 *  @param width             Width to use for the descriptor.
 *  @param height            Height to use for the descriptor.
 *  @param fps               FPS to use for the descriptor.
 *  @param bitrate           Bitrate to use for the descriptor.
 *  @param video_info_header Video info header.
 **/
PRIVATE void _init_webcam_media_info(__inout __notnull webcam_device_media_info* out_result, __in __notnull AM_MEDIA_TYPE* am_media_type_ptr,__in GUID data_subtype, __in LONG width, __in LONG height, __in float fps, __in DWORD bitrate, __in VIDEOINFOHEADER* video_info_header)
{
    out_result->am_media_type     = am_media_type_ptr;
    out_result->bitrate           = bitrate;
    out_result->data_subtype      = data_subtype;
    out_result->format            = _convert_media_subtype_to_webcam_device_media_format(data_subtype);
    out_result->fps               = fps;
    out_result->height            = height;
    out_result->video_info_header = *video_info_header;
    out_result->width             = width;
}

/** Function that clears existing media info descriptors for a given webcam_device instance, retrievse
 *  information from device's IBaseFilter ptr and stores it in the descriptor.
 *
 *  @param webcam_device_ptr Webcam device to use for the process.
 *
 *  @return true if successful, false otherwise.
*/
PRIVATE bool _webcam_device_retrieve_supported_resolutions(__inout __notnull _webcam_device* webcam_device_ptr)
{
    IEnumPins* enum_pins_ptr = NULL;
    bool       func_result = false;
    HRESULT    result;

    result = webcam_device_ptr->base_filter_ptr->EnumPins(&enum_pins_ptr);
    
    ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not obtain IEnumPins ptr");
    if (SUCCEEDED(result) )
    {
        IPin* pin_ptr = NULL;

        /* Iterate through pins */
        while (system_resizable_vector_get_amount_of_elements(webcam_device_ptr->medias) > 0)
        {
            webcam_device_media_info* media_info = NULL;
            bool                      result_get = system_resizable_vector_pop(webcam_device_ptr->medias, &media_info);

            ASSERT_DEBUG_SYNC(result_get, "Could not retrieve media info.");

            delete media_info;
        }

        while (enum_pins_ptr->Next(1, &pin_ptr, 0) == S_OK)
        {
            IEnumMediaTypes* enum_media_types_ptr = NULL;

            result = pin_ptr->EnumMediaTypes(&enum_media_types_ptr);

            ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not retrieve IEnumMediaTypes ptr.");
            if (SUCCEEDED(result) )
            {
                /* Iterate through supported media types */
                AM_MEDIA_TYPE* media_type_ptr = NULL;

                while (enum_media_types_ptr->Next(1, &media_type_ptr, 0) == S_OK)
                {
                    if (media_type_ptr->formattype == FORMAT_VideoInfo)
                    {
                        /* Store the entry */
                        webcam_device_media_info* new_media_info = new (std::nothrow) webcam_device_media_info;

                        ASSERT_ALWAYS_SYNC(new_media_info != NULL, "Could not allocate space for media info data storage.");
                        if (new_media_info != NULL)
                        {
                            _init_webcam_media_info(new_media_info,
                                                    media_type_ptr,
                                                    media_type_ptr->subtype,
                                                    ((VIDEOINFOHEADER*)media_type_ptr->pbFormat)->bmiHeader.biWidth,
                                                    ((VIDEOINFOHEADER*)media_type_ptr->pbFormat)->bmiHeader.biHeight,
                                                    float(1000000000 / ((VIDEOINFOHEADER*)media_type_ptr->pbFormat)->AvgTimePerFrame) / 100.0f,
                                                    ((VIDEOINFOHEADER*)media_type_ptr->pbFormat)->dwBitRate / 8 / 1024,
                                                    ((VIDEOINFOHEADER*)media_type_ptr->pbFormat)
                                                   );

                            LOG_INFO("Reported mode: [width=%d height=%d bitrate=%ldKB/s fps=%f format=[%s] subtype=[%08X:%04X:%04X:%02X]",
                                     new_media_info->width,
                                     new_media_info->height,
                                     new_media_info->bitrate,
                                     new_media_info->fps,
                                     system_hashed_ansi_string_get_buffer( webcam_device_media_format_as_string(new_media_info->format) ),
                                     new_media_info->data_subtype.Data1,
                                     new_media_info->data_subtype.Data2,
                                     new_media_info->data_subtype.Data3,
                                (int)new_media_info->data_subtype.Data4 & 0xFF);

                            system_resizable_vector_push(webcam_device_ptr->medias, new_media_info);
                        }
                    }
                    else
                    {
                        LOG_INFO("Unsupported format type reported [%08X:%04X:%04X:%02X]", 
                                  media_type_ptr->formattype.Data1, 
                                  media_type_ptr->formattype.Data2,
                                  media_type_ptr->formattype.Data3,
                             (int)media_type_ptr->formattype.Data4 & 0xFF);
                    }
                }
                /* Release IEnumMediaTypes ptr */
                enum_media_types_ptr->Release();
            }

            /* Release pin ptr */
            pin_ptr->Release();
        }

        /* Release IEnumPins ptr */
        enum_pins_ptr->Release();

        /* Everything went as expected */
        func_result = true;
    }

    return func_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_device_change_streamer_media_info(__in __notnull webcam_device device,
                                                                 __in __notnull webcam_device_media_info* new_media_info,
                                                                 __in __notnull webcam_device_media_info* new_sample_grabber_media_info)
{
    _webcam_device* device_ptr = (_webcam_device*) device;
    bool            result     = false;

    /* Make sure the streamer has already been created */
    ASSERT_DEBUG_SYNC(device_ptr->streamer != NULL, "Streamer is null for camera [%s]",
                      system_hashed_ansi_string_get_buffer(device_ptr->name) );
    if (device_ptr->streamer != NULL)
    {
        result = webcam_device_streamer_change_media_info(device_ptr->streamer, new_media_info);

        /* If format changes, the streamer wil most likely fail here. In this case, we need to do full reinitialization */
        if (!result && new_media_info->format != webcam_device_streamer_get_media_info(device_ptr->streamer).format != new_media_info->format)
        {
            LOG_INFO("Could not change media format and the formats differ - reinstancing streamer with new media info descriptor");

            /* Release existing streamer */
            PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC streamer_callback          = webcam_device_streamer_get_callback         (device_ptr->streamer);
            void*                                      streamer_callback_user_arg = webcam_device_streamer_get_callback_user_arg(device_ptr->streamer);

            webcam_device_streamer_release(device_ptr->streamer);
            device_ptr->streamer = NULL;
           
            /* Create a new instance.. */
            webcam_device_open_streamer(device,
                                        new_media_info,
                                        new_sample_grabber_media_info,
                                        streamer_callback,
                                        streamer_callback_user_arg);

            if (device_ptr->streamer == NULL)
            {
                LOG_FATAL("Could not reinstantiate new streamer with requested media info! Potential crash ahead");
            }
            else
            {
                result = true;
            }
        }

    }

    return result;
}

/** Please see header for specification */
PUBLIC webcam_device webcam_device_create(__in __notnull system_hashed_ansi_string name, __in __notnull system_hashed_ansi_string path, __in __notnull IBaseFilter* base_filter_ptr)
{
    _webcam_device* new_device = new (std::nothrow) _webcam_device;

    ASSERT_ALWAYS_SYNC(new_device != NULL, "Out of memory while allocating space for new webcam device descriptor.");
    if (new_device != NULL)
    {
        new_device->base_filter_ptr   = base_filter_ptr;
        new_device->graph_builder_ptr = NULL;
        new_device->medias            = system_resizable_vector_create(BASE_MEDIAS_CACHED_FOR_WEBCAM, sizeof(webcam_device_media_info*) );
        new_device->name              = name;
        new_device->path              = path;
        new_device->streamer          = NULL;

        LOG_INFO("Camera [%s] detected.", system_hashed_ansi_string_get_buffer(name) );

        _webcam_device_retrieve_supported_resolutions(new_device);
    }
    
    return (webcam_device) new_device;
}

/** Please see header for specification */
PUBLIC EMERALD_API webcam_device_media_info* webcam_device_get_media_info(__in __notnull webcam_device device, __in uint32_t n_media_info)
{
    webcam_device_media_info* result            = NULL;
    _webcam_device*           webcam_device_ptr = (_webcam_device*) device;

    if (system_resizable_vector_get_amount_of_elements(webcam_device_ptr->medias) > n_media_info)
    {
        bool result_get = system_resizable_vector_get_element_at(webcam_device_ptr->medias, n_media_info, &result);
        ASSERT_DEBUG_SYNC(result_get, "Could not retrieve media info for a device.");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API webcam_device_streamer webcam_device_get_streamer(__in __notnull webcam_device device)
{
    if (device != NULL)
    {
        return ((_webcam_device*)device)->streamer;
    }
    else
    {
        return NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_device_open_streamer(__in __notnull webcam_device device, 
                                                    __in __notnull webcam_device_media_info*                  media_info,
                                                    __in __notnull webcam_device_media_info*                  sample_grabber_media_info,
                                                    __in __notnull PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC callback, 
                                                    __in           void*                                      user_arg)
{
    _webcam_device* device_ptr = (_webcam_device*) device;
    bool            result     = false;

    /* Make sure streamer has not already been created */
    ASSERT_DEBUG_SYNC(device_ptr->streamer == NULL, "Streamer is not null for camera [%s]",
                      system_hashed_ansi_string_get_buffer(device_ptr->name) );

    if (device_ptr->streamer == NULL)
    {
        device_ptr->streamer = webcam_device_streamer_create(device_ptr->name, device_ptr->base_filter_ptr, media_info, sample_grabber_media_info, callback, user_arg);
        result               = (device_ptr->streamer) != NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_device_change_streamer_callback_user_argument(__in __notnull webcam_device device, __in void* new_user_arg)
{
    _webcam_device* device_ptr = (_webcam_device*) device;
    bool            result     = false;

    if (device_ptr->streamer != NULL)
    {
        webcam_device_streamer_set_callback_user_argument(device_ptr->streamer, new_user_arg);

        result = true;
    }
    else
    {
        LOG_ERROR("Cannot update streamer's callback user argument - streamer unavailable.");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string webcam_device_get_name(webcam_device device)
{
    return ((_webcam_device*)device)->name;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string webcam_device_get_path(__in __notnull webcam_device device)
{
    return ((_webcam_device*)device)->path;
}


/** Please see header for specification */
PUBLIC void webcam_device_release(webcam_device device)
{
    LOG_INFO("webcam_device_release(device=[%s]). Stopping streamer, releasing device.", 
             system_hashed_ansi_string_get_buffer( ( (_webcam_device*)device)->name) );

    _webcam_device* device_ptr = (_webcam_device*) device;
    
    /* Release streamer, if available */
    if (device_ptr->streamer != NULL)
    {
        webcam_device_streamer_stop(device_ptr->streamer);
        webcam_device_streamer_release(device_ptr->streamer);
        
        device_ptr->streamer = NULL;
    }

    /* Free medias vector */
    while (system_resizable_vector_get_amount_of_elements(device_ptr->medias) > 0)
    {
        webcam_device_media_info* media_info_ptr = NULL;
        bool                      result_get     = false;

        result_get = system_resizable_vector_pop(device_ptr->medias, &media_info_ptr);
        ASSERT_DEBUG_SYNC(result_get, "Could not retrieve media info pointer.");

        delete media_info_ptr;
    }

    delete device_ptr;
}
