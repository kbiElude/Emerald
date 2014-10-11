/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "webcam/webcam_device_streamer.h"
#include "webcam/webcam_device_streamer_handler.h"
#include "webcam/webcam_types.h"

/** Internal type definitions */
typedef struct
{
    PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC callback;
    void*                                      callback_user_arg;
    webcam_device_streamer_handler*            callback_handler_ptr;
    ICaptureGraphBuilder2*                     capture_graph_builder2_ptr;
    system_hashed_ansi_string                  device_name;
    IGraphBuilder*                             graph_builder_ptr;
    IMediaControl*                             media_control_ptr;
    webcam_device_media_info                   media_info;
    IBaseFilter*                               null_renderer_base_filter_ptr;
    IBaseFilter*                               sample_grabber_base_filter_ptr;
    ISampleGrabber*                            sample_grabber_ptr;
    IAMStreamConfig*                           stream_config_ptr;
} _webcam_device_streamer;

/** Internal variables */

/** TODO */
PRIVATE HRESULT _webcam_device_streamer_get_unconnected_pins(IBaseFilter* base_filter_ptr, PIN_DIRECTION pin_direction, IPin** result_pin_ptr)
{
    IEnumPins* enum_pins_ptr = NULL;
    IPin*      pin_ptr       = NULL;
    HRESULT    result        = S_OK;

    *result_pin_ptr = NULL;

    result = base_filter_ptr->EnumPins(&enum_pins_ptr);
    if (FAILED(result) )
    {
        return result;
    }

    while (enum_pins_ptr->Next(1, &pin_ptr, NULL) == S_OK)
    {
        PIN_DIRECTION curr_pin_direction;

        result = pin_ptr->QueryDirection(&curr_pin_direction);
        ASSERT_DEBUG_SYNC(result == S_OK, "Could not query pin's direction.");

        if (curr_pin_direction == pin_direction)
        {
            IPin* temporary_pin_ptr = NULL;

            result = pin_ptr->ConnectedTo(&temporary_pin_ptr);
            
            if (SUCCEEDED(result) )
            {
                /* Already connected - not interested in this pin */
                temporary_pin_ptr->Release();
            }
            else
            {
                /* Unconnected - this is it. */
                enum_pins_ptr->Release();

                *result_pin_ptr = pin_ptr;

                return S_OK;
            }
        }

        pin_ptr->Release();
    }

    enum_pins_ptr->Release();

    /* Did not find a matching pin */
    return E_FAIL;
}

/** TODO */
PRIVATE HRESULT _webcam_device_streamer_connect_filters_by_pins(IGraphBuilder* graph_builder_ptr, IPin* dst_pin_ptr, IBaseFilter* dst_base_filter_ptr)
{
    IPin*   src_pin_ptr = NULL;
    HRESULT result      = _webcam_device_streamer_get_unconnected_pins(dst_base_filter_ptr, PINDIR_INPUT, &src_pin_ptr);

    if (FAILED(result) )
    {
        return result;
    }

    result = graph_builder_ptr->Connect(dst_pin_ptr, src_pin_ptr);
    src_pin_ptr->Release();

    return result;
}

/** TODO */
PRIVATE HRESULT _webcam_device_streamer_connect_filters(IGraphBuilder* graph_builder_ptr, IBaseFilter* src_base_filter_ptr, IBaseFilter* dst_base_filter_ptr)
{
    IPin*   dst_pin_ptr = NULL;
    HRESULT result      = _webcam_device_streamer_get_unconnected_pins(src_base_filter_ptr, PINDIR_OUTPUT, &dst_pin_ptr);

    if (FAILED(result) )
    {
        return result;
    }

    result = _webcam_device_streamer_connect_filters_by_pins(graph_builder_ptr, dst_pin_ptr, dst_base_filter_ptr);
    dst_pin_ptr->Release();

    return result;
}

/** TODO */
PRIVATE void _webcam_device_streamer_free_media_type(AM_MEDIA_TYPE& media_type)
{
    if (media_type.cbFormat != 0)
    {
        ::CoTaskMemFree( (void*) media_type.pbFormat);

        media_type.cbFormat = 0;
        media_type.pbFormat = NULL;
    }

    if (media_type.pUnk != NULL)
    {
        media_type.pUnk->Release();
        media_type.pUnk = NULL;
    }
}

/** TODO */
PRIVATE void _webcam_device_streamer_deinit_webcam_device_streamer(__inout __notnull _webcam_device_streamer* streamer_ptr)
{
    if (streamer_ptr->media_control_ptr != NULL)
    {
        streamer_ptr->media_control_ptr->Stop();
        streamer_ptr->media_control_ptr->Release();

        streamer_ptr->media_control_ptr = NULL;
    }

    if (streamer_ptr->capture_graph_builder2_ptr != NULL)
    {
        streamer_ptr->capture_graph_builder2_ptr->Release();
        streamer_ptr->capture_graph_builder2_ptr = NULL;
    }

    if (streamer_ptr->graph_builder_ptr != NULL)
    {
        streamer_ptr->graph_builder_ptr->Release();
        streamer_ptr->graph_builder_ptr = NULL;
    }

    if (streamer_ptr->null_renderer_base_filter_ptr != NULL)
    {
        streamer_ptr->null_renderer_base_filter_ptr->Release();
        streamer_ptr->null_renderer_base_filter_ptr = NULL;
    }

    if (streamer_ptr->sample_grabber_base_filter_ptr != NULL)
    {
        streamer_ptr->sample_grabber_base_filter_ptr->Release();
        streamer_ptr->sample_grabber_base_filter_ptr = NULL;
    }

    if (streamer_ptr->sample_grabber_ptr != NULL)
    {
        streamer_ptr->sample_grabber_ptr->Release();
        streamer_ptr->sample_grabber_ptr = NULL;
    }

    if (streamer_ptr->stream_config_ptr != NULL)
    {
        streamer_ptr->stream_config_ptr->Release();
        streamer_ptr->stream_config_ptr = NULL;
    }

    if (streamer_ptr->callback_handler_ptr != NULL)
    {
        delete streamer_ptr->callback_handler_ptr;

        streamer_ptr->callback_handler_ptr = NULL;
    }
}

/** TODO */
PRIVATE void _webcam_device_streamer_init_webcam_device_streamer(__inout __notnull _webcam_device_streamer* streamer_ptr)
{
    streamer_ptr->callback                       = NULL;
    streamer_ptr->callback_user_arg              = NULL;
    streamer_ptr->callback_handler_ptr           = NULL;
    streamer_ptr->capture_graph_builder2_ptr     = NULL;
    streamer_ptr->device_name                    = system_hashed_ansi_string_get_default_empty_string();
    streamer_ptr->graph_builder_ptr              = NULL;
    streamer_ptr->media_control_ptr              = NULL;
    streamer_ptr->null_renderer_base_filter_ptr  = NULL;
    streamer_ptr->sample_grabber_base_filter_ptr = NULL;
    streamer_ptr->sample_grabber_ptr             = NULL;
    streamer_ptr->stream_config_ptr              = NULL;
}

/** Please see header for specification */
PUBLIC bool webcam_device_streamer_change_media_info(__in __notnull webcam_device_streamer streamer, __in __notnull webcam_device_media_info* media_info)
{
    bool                     result       = false;
    _webcam_device_streamer* streamer_ptr = (_webcam_device_streamer*) streamer;
    HRESULT                  temp_result;

    /* If needed, stop the streaming first. Ignore the outcome */
    streamer_ptr->media_control_ptr->Stop();

    /* Reconfigure both capture and sample grabber filter to use the new media format */
    temp_result = streamer_ptr->stream_config_ptr->SetFormat(media_info->am_media_type);

    if (FAILED(temp_result) )
    {
        LOG_ERROR("Could not update stream config's format!");

        goto end;
    }

    temp_result = streamer_ptr->sample_grabber_ptr->SetMediaType(media_info->am_media_type);

    if (FAILED(temp_result) )
    {
        LOG_ERROR("Could not update sample grabber's format!");

        goto end;
    }

    result                   = true;
    streamer_ptr->media_info = *media_info;
end:
    return result;
}

/** Please see header for specification */
PUBLIC webcam_device_streamer webcam_device_streamer_create(__in    __notnull   system_hashed_ansi_string                  device_name, 
                                                            __in    __notnull   IBaseFilter*                               base_filter_ptr, 
                                                            __inout __notnull   webcam_device_media_info*                  media_info, 
                                                            __in    __maybenull webcam_device_media_info*                  sample_grabber_media_info,
                                                                    __maybenull PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC callback, 
                                                            __in    __maybenull void*                                      user_arg)
{
    _webcam_device_streamer* new_device_streamer = new (std::nothrow) _webcam_device_streamer;

    ASSERT_ALWAYS_SYNC(new_device_streamer != NULL, "Out of memory while allocating space for webcam device streamer");
    if (new_device_streamer != NULL)
    {
        HRESULT result = S_OK;

        _webcam_device_streamer_init_webcam_device_streamer(new_device_streamer);

        /* IGraphBuilder ptr */
        result = ::CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC, IID_IGraphBuilder, (void**) &new_device_streamer->graph_builder_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not create an instance of IGraphBuilder.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Retrieve IMediaControl ptr */
        result = new_device_streamer->graph_builder_ptr->QueryInterface(IID_IMediaControl, (void**) &new_device_streamer->media_control_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not retrieve IMediaControl ptr.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* ICaptureGraphBuilder2 ptr */
        result = ::CoCreateInstance(CLSID_CaptureGraphBuilder2, 0, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, (void**) &new_device_streamer->capture_graph_builder2_ptr);
        
        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not create an instance of ICaptureGraphBuilder2.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Attach filter graph to the capture graph. */
        result = new_device_streamer->capture_graph_builder2_ptr->SetFiltergraph(new_device_streamer->graph_builder_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not attach graph builder to capture graph builder.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Add the camera capture filter to the graph */
        result = new_device_streamer->graph_builder_ptr->AddFilter(base_filter_ptr, L"Video capture");

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not add camera capture filter to the graph.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Retrieve IAMStreamConfig ptr */
        result = new_device_streamer->capture_graph_builder2_ptr->FindInterface(0,
                                                                                &MEDIATYPE_Video,
                                                                                base_filter_ptr,
                                                                                IID_IAMStreamConfig, 
                                                                                (void**) &new_device_streamer->stream_config_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not retrieve IAMStreamConfig ptr");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Update format to be used for capturing */
        result = new_device_streamer->stream_config_ptr->SetFormat(media_info->am_media_type);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not set format for capture filter.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Instantiate sample grabber */
        result = ::CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC, IID_IBaseFilter, (void**) &new_device_streamer->sample_grabber_base_filter_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not create an instance of sample grabber's base filter.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Retrieve ISampleGrabber ptr */
        result = new_device_streamer->sample_grabber_base_filter_ptr->QueryInterface(IID_ISampleGrabber, (void**) &new_device_streamer->sample_grabber_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not retrieve ISampleGrabber ptr for sample grabber.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Add the sample grabber to the graph. */
        result = new_device_streamer->graph_builder_ptr->AddFilter(new_device_streamer->sample_grabber_base_filter_ptr, L"Sample grabber");

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not attach sample grabber to the graph.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Configure the sample grabber to use the media type as chosen by the caller */
        result = new_device_streamer->sample_grabber_ptr->SetMediaType(sample_grabber_media_info->am_media_type);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not configure sample grabber to use specified media type.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Connect capture filter to the sample grabber filter */
        result = _webcam_device_streamer_connect_filters(new_device_streamer->graph_builder_ptr,
                                                         base_filter_ptr,
                                                         new_device_streamer->sample_grabber_base_filter_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not connect capture filter to the sample grabber filter.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Instantiate Null renderer */
        result = ::CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC, IID_IBaseFilter, (void**) &new_device_streamer->null_renderer_base_filter_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not create an instance of null renderer's base filter.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Add null renderer to the graph */
        new_device_streamer->graph_builder_ptr->AddFilter(new_device_streamer->null_renderer_base_filter_ptr, L"Null renderer");

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not add null renderer to the graph.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Connect sample grabber filter to the null renderer. */
        result = _webcam_device_streamer_connect_filters(new_device_streamer->graph_builder_ptr,
                                                         new_device_streamer->sample_grabber_base_filter_ptr,
                                                         new_device_streamer->null_renderer_base_filter_ptr);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not connect sample grabber filter to the null renderer.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Okay, let's configure the streamer now */
        result = new_device_streamer->sample_grabber_ptr->SetOneShot(FALSE);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not put sample grabber into non-one-shot mode.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        result = new_device_streamer->sample_grabber_ptr->SetBufferSamples(FALSE);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not put sample grabber into sample buffering mode.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Instantiate the call-back handler object */
        new_device_streamer->callback_handler_ptr = new (std::nothrow) webcam_device_streamer_handler(callback, user_arg);
        
        ASSERT_ALWAYS_SYNC(new_device_streamer->callback_handler_ptr != NULL, "Could not allocate streamer handler object.");
        if (new_device_streamer == NULL)
        {
            goto failed_end;
        }

        /* Tell the streamer about the call-back handler */
        result = new_device_streamer->sample_grabber_ptr->SetCallback(new_device_streamer->callback_handler_ptr, 1);

        ASSERT_ALWAYS_SYNC(SUCCEEDED(result), "Could not set sample grabber's callback.");
        if (FAILED(result) )
        {
            goto failed_end;
        }

        /* Store device name */
        new_device_streamer->callback          = callback;
        new_device_streamer->callback_user_arg = user_arg;
        new_device_streamer->device_name       = device_name;        
        new_device_streamer->media_info        = *media_info; 
    }

    return (webcam_device_streamer) new_device_streamer;

failed_end:
    _webcam_device_streamer_deinit_webcam_device_streamer(new_device_streamer);

    delete new_device_streamer;
    new_device_streamer = NULL;

    return (webcam_device_streamer) new_device_streamer;
}

/** Please see header for specification */
PUBLIC PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC webcam_device_streamer_get_callback(__in __notnull webcam_device_streamer streamer)
{
    return ((_webcam_device_streamer*)streamer)->callback;
}

/** Please see header for specification */
PUBLIC void* webcam_device_streamer_get_callback_user_arg(__in __notnull webcam_device_streamer streamer)
{
    return ((_webcam_device_streamer*)streamer)->callback_user_arg;
}

/** Please see header for specification */
PUBLIC webcam_device_media_info webcam_device_streamer_get_media_info(__in __notnull webcam_device_streamer streamer)
{
    return ((_webcam_device_streamer*)streamer)->media_info;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_device_streamer_pause(__in __notnull webcam_device_streamer streamer)
{
    _webcam_device_streamer* streamer_ptr = (_webcam_device_streamer*) streamer;
    HRESULT                  result       = streamer_ptr->media_control_ptr->Pause();

    return SUCCEEDED(result);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_device_streamer_play(__in __notnull webcam_device_streamer streamer)
{
    _webcam_device_streamer* streamer_ptr = (_webcam_device_streamer*) streamer;
    HRESULT                  result       = streamer_ptr->media_control_ptr->Run();

    LOG_INFO("Requested playback for device [%s] with result [%d]", 
             system_hashed_ansi_string_get_buffer(streamer_ptr->device_name),
                                                  SUCCEEDED(result)
            );
    if (!SUCCEEDED(result) )
    {
        LOG_FATAL("Could not start playback for device [%s]: result=%08x", 
                  system_hashed_ansi_string_get_buffer(streamer_ptr->device_name),
                  result);
    }
    else
    {
        /* Pass the notification to callback handler */
        if (streamer_ptr->callback_handler_ptr != NULL)
        {
            streamer_ptr->callback_handler_ptr->Play();
        }
    }

    return SUCCEEDED(result);
}

/** Please see header for specification */
PUBLIC void webcam_device_streamer_release(__in __notnull webcam_device_streamer streamer)
{
    _webcam_device_streamer* streamer_ptr = (_webcam_device_streamer*) streamer;

    _webcam_device_streamer_deinit_webcam_device_streamer(streamer_ptr);
    delete streamer_ptr;
}

/** Please see header for specification */
PUBLIC void webcam_device_streamer_set_callback_user_argument(__in __notnull webcam_device_streamer streamer, __in void* new_user_arg)
{
    _webcam_device_streamer* streamer_ptr = (_webcam_device_streamer*) streamer;

    if (streamer_ptr->callback_handler_ptr != NULL)
    {
        streamer_ptr->callback_handler_ptr->UpdateUserArgument(new_user_arg);
    }
    else
    {
        LOG_ERROR("Cannot update callback user argument - callback handler is NULL.");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool webcam_device_streamer_stop(__in __notnull webcam_device_streamer streamer)
{
    _webcam_device_streamer* streamer_ptr = (_webcam_device_streamer*) streamer;
    HRESULT                  result       = E_FAIL;

    if (streamer_ptr != NULL)
    {
        /* Before we request media control to stop, disable call-backs from the handler. Otherwise we could run into thread
         * racing conditions
         */
        if (streamer_ptr->callback_handler_ptr != NULL)
        {
            streamer_ptr->callback_handler_ptr->Stop();
        }

        result = streamer_ptr->media_control_ptr->Stop();
    }

    return SUCCEEDED(result);
}
