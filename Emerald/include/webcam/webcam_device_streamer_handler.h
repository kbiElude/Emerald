/**
 *
 * Emerald (kbi/elude @2012)
 *
 * Webcam device streamer handler is one of the very few (if not the only) cases in Emerald
 * where we have to use a class. That is due to DirectShow requiring a COM-like object to handle
 * call-backs.
 *
 * Oh well.
 */
#ifndef WEBCAM_DEVICE_STREAMER_HANDLER_H
#define WEBCAM_DEVICE_STREAMER_HANDLER_H

#include "webcam/webcam_types.h"

#ifdef INCLUDE_WEBCAM_MANAGER

class webcam_device_streamer_handler : public ISampleGrabberCB
{
public:
    /* Public methods */
     webcam_device_streamer_handler(PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC, void* in_user_arg);
    ~webcam_device_streamer_handler();
    
    void Play();
    void Stop();
    void UpdateUserArgument(void*);

    /* ISampleGrabberCB */
    HRESULT STDMETHODCALLTYPE SampleCB(double sample_time, IMediaSample* media_sample_ptr);
    HRESULT STDMETHODCALLTYPE BufferCB(double sample_time, BYTE* buffer_ptr, long buffer_length);

    /* IUnknown */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR* __RPC_FAR* ppv_object);
    ULONG   STDMETHODCALLTYPE AddRef();
    ULONG   STDMETHODCALLTYPE Release();

    /* Private variables */
    PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC callback_proc;
    bool                                       can_pass_through;
    double                                     last_sample_time;
    system_critical_section                    user_arg_cs;
    void*                                      user_arg;
};

#endif /* INCLUDE_WEBCAM_MANAGER */
#endif /* WEBCAM_DEVICE_STREAMER_HANDLER_H */
