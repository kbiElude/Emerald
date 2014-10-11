/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "webcam/webcam_device_streamer_handler.h"

extern "C++"
{
    /** TODO */
    webcam_device_streamer_handler::webcam_device_streamer_handler(PFNWEBCAMDEVICESTREAMERHANDLERCALLBACKPROC in_callback_proc, void* in_user_arg)
        :callback_proc   (in_callback_proc),
         can_pass_through(false           ),
         last_sample_time(0               ),
         user_arg        (in_user_arg     )
    {
        ASSERT_DEBUG_SYNC(callback_proc != NULL, "No call-back defined");

        user_arg_cs = system_critical_section_create();
    }

    /** TODO */
    webcam_device_streamer_handler::~webcam_device_streamer_handler()
    {
        system_critical_section_release(user_arg_cs);
    }

    /** TODO */
    ULONG STDMETHODCALLTYPE webcam_device_streamer_handler::AddRef()
    {
        return 1;
    }

    /** TODO */
    HRESULT STDMETHODCALLTYPE webcam_device_streamer_handler::BufferCB(double sample_time, BYTE* buffer_ptr, long buffer_length)
    {
        if (last_sample_time != sample_time && can_pass_through)
        {
            if (callback_proc != NULL)
            {
                system_critical_section_enter(user_arg_cs);
                {
                    callback_proc(sample_time, buffer_ptr, buffer_length, user_arg);
                }
                system_critical_section_leave(user_arg_cs);
            }

            last_sample_time = sample_time;
        }

        return S_OK;
    }

    /** TODO */
    void webcam_device_streamer_handler::Play()
    {
        system_critical_section_enter(user_arg_cs);
        {
            can_pass_through = true;
        }
        system_critical_section_leave(user_arg_cs);
    }

    /** TODO */
    HRESULT STDMETHODCALLTYPE webcam_device_streamer_handler::QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR* __RPC_FAR* ppv_object)
    {
        if (ppv_object == NULL)
        {
            return E_POINTER;
        }

        if (riid == __uuidof(IUnknown) )
        {
            *ppv_object = static_cast<IUnknown*>(this);

            return S_OK;
        }
        else
        if (riid == __uuidof(ISampleGrabberCB))
        {
            *ppv_object = static_cast<ISampleGrabberCB*>(this);

            return S_OK;
        }

        return E_NOTIMPL;
    }

    /** TODO */
    ULONG STDMETHODCALLTYPE webcam_device_streamer_handler::Release()
    {
        return 2;
    }

    /** TODO */
    void webcam_device_streamer_handler::Stop()
    {
        system_critical_section_enter(user_arg_cs);
        {
            can_pass_through = false;
        }
        system_critical_section_leave(user_arg_cs);
    }

    /** TODO */
    HRESULT STDMETHODCALLTYPE webcam_device_streamer_handler::SampleCB(double sample_time, IMediaSample* media_sample_ptr)
    {
        if (last_sample_time != sample_time)
        {
            if (callback_proc != NULL && can_pass_through)
            {
                system_critical_section_enter(user_arg_cs);
                {
                    BYTE* pointer       = NULL;
                    long  buffer_length = media_sample_ptr->GetActualDataLength();
                    
                    media_sample_ptr->GetPointer(&pointer);

                    callback_proc(sample_time, pointer, buffer_length, user_arg);
                }
                system_critical_section_leave(user_arg_cs);
            }

            last_sample_time = sample_time;
        }

        return S_OK;
    }    

    /** TODO */
    void webcam_device_streamer_handler::UpdateUserArgument(void* new_user_arg)
    {
        system_critical_section_enter(user_arg_cs);
        {
            user_arg = new_user_arg;
        }
        system_critical_section_leave(user_arg_cs);
    }
}
