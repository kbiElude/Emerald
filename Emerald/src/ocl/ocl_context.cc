/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ocl/CL/cl_gl.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ogl/ogl_context.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    cl_context     context;
    ogl_context    gl_context;
    cl_platform_id platform_id;
    bool           should_ocl_error_callbacks_cause_assertion_failure;

                        uint32_t          n_devices;
    __ecount(n_devices) cl_command_queue* command_queues;
    __ecount(n_devices) cl_device_id*     devices;

    REFCOUNT_INSERT_VARIABLES
} _ocl_context;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ocl_context, ocl_context, _ocl_context);


/** TODO */
PRIVATE void CL_API_CALL _ocl_context_driver_notification_entrypoint(const char* err_info, const void* private_info, size_t n_bytes, void* context)
{
    _ocl_context* context_ptr = (_ocl_context*) context;

    if (context_ptr->should_ocl_error_callbacks_cause_assertion_failure)
    {
        ASSERT_DEBUG_SYNC(false, "OpenCL: Error [%s] reported by OpenCL for context [%08x]", err_info, context);
    }
    else
    {
        LOG_ERROR("OpenCL (assertion failures disabled): Error [%s] reported by OpenCL for context [%p]", err_info, context);
    }
}

/** TODO */
PRIVATE void _ocl_context_init(_ocl_context* context_ptr, cl_platform_id platform_id)
{
    context_ptr->context                                            = NULL;
    context_ptr->command_queues                                     = NULL;
    context_ptr->devices                                            = NULL;
    context_ptr->gl_context                                         = NULL;
    context_ptr->n_devices                                          = 0;
    context_ptr->platform_id                                        = platform_id;
    context_ptr->should_ocl_error_callbacks_cause_assertion_failure = true;
}

/** TODO */
PRIVATE void _ocl_context_release(void* arg)
{
    _ocl_context*             context_ptr     = (_ocl_context*) arg;
    const ocl_11_entrypoints* ocl_entrypoints = ocl_get_entrypoints();

    if (context_ptr->command_queues != NULL)
    {
        for (uint32_t n = 0; n < context_ptr->n_devices; ++n)
        {
            if (ocl_entrypoints->pCLReleaseCommandQueue(context_ptr->command_queues[n]) != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false, "Could not release command queue for device index [%d]", n);
            }
        }

        delete [] context_ptr->command_queues;
        context_ptr->command_queues = NULL;
    }

    if (context_ptr->context != NULL)
    {
        ocl_entrypoints->pCLReleaseContext(context_ptr->context);

        context_ptr->context = NULL;
    }

    if (context_ptr->devices != NULL)
    {
        delete [] context_ptr->devices;

        context_ptr->devices = NULL;
    }

    if (context_ptr->gl_context != NULL)
    {
        ogl_context_release(context_ptr->gl_context);
    }
}

/** TODO */
PRIVATE void _ocl_context_create_gpu_only_with_gl_sharing_rendering_callback(__in __notnull ogl_context gl_context, __in __notnull void* arg)
{
    const ocl_11_entrypoints* ocl_entrypoints = ocl_get_entrypoints();
    _ocl_context*             instance        = (_ocl_context*) arg;

    /* Try to create context as per function name */
    HDC   dc_context          = 0;
    HGLRC gl_resource_context = 0;

    if (!ogl_context_get_dc        (gl_context, &dc_context)          ||
        !ogl_context_get_gl_context(gl_context, &gl_resource_context) )
    {
        LOG_ERROR        ("Could not retrieve OGL DC - cannot create CL context");
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }

    
    const cl_context_properties context_properties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties) instance->platform_id,
                                                        CL_GL_CONTEXT_KHR,   (cl_context_properties) gl_resource_context,
                                                        CL_WGL_HDC_KHR,      (cl_context_properties) dc_context,
                                                        0};
    cl_int                      context_error        = CL_SUCCESS;

    instance->context = ocl_entrypoints->pCLCreateContextFromType(context_properties,
                                                                  CL_DEVICE_TYPE_GPU,
                                                                  _ocl_context_driver_notification_entrypoint,
                                                                  instance,
                                                                  &context_error);

    if (instance->context == NULL)
    {
        LOG_ERROR        ("CL context creation error - error code [%d] reported", context_error);
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }

    /* Retrieve devices that have been associated with the context */
    cl_int  error_code = 0;
    cl_uint n_devices  = 0;

    error_code = ocl_entrypoints->pCLGetContextInfo(instance->context,
                                                    CL_CONTEXT_NUM_DEVICES,
                                                    sizeof(n_devices),
                                                    &n_devices,
                                                    NULL);

    if (error_code != CL_SUCCESS)
    {
        LOG_ERROR        ("Could not retrieve number of devices associated with CL context");
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }

    instance->n_devices = n_devices;
    instance->devices   = new (std::nothrow) cl_device_id[n_devices];

    ASSERT_ALWAYS_SYNC(instance->devices != NULL, "Out of memory");
    if (instance->devices == NULL)
    {
        goto end;
    }

    error_code = ocl_entrypoints->pCLGetContextInfo(instance->context,
                                                    CL_CONTEXT_DEVICES,
                                                    sizeof(cl_device_id) * n_devices,
                                                    instance->devices,
                                                    NULL);

    if (error_code != CL_SUCCESS)
    {
        LOG_ERROR        ("Could not retrieve devices associated with CL context");
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }        

    /* Spawn command queues for all devices */
    instance->command_queues = new (std::nothrow) cl_command_queue[n_devices];

    ASSERT_ALWAYS_SYNC(instance->command_queues != NULL, "Out of memory");
    if (instance->command_queues == NULL)
    {
        goto end;
    }

    for (uint32_t n = 0; n < n_devices; ++n)
    {
        instance->command_queues[n] = ocl_entrypoints->pCLCreateCommandQueue(instance->context,
                                                                             instance->devices[n],
                                                                             0,
                                                                             &error_code);
        
        ASSERT_DEBUG_SYNC(instance->command_queues[n] != NULL, "Could not create a command queue for device index [%d]: error code=[%d]",
                          n,
                          error_code);
        if (instance->command_queues[n] == NULL)
        {
            goto end;
        }
    }

end: ;
}

/* Please see header for specification */
PUBLIC EMERALD_API ocl_context ocl_context_create_gpu_only_with_gl_sharing(__in __notnull cl_platform_id            platform_id,
                                                                           __in __notnull ogl_context               gl_context,
                                                                           __in __notnull system_hashed_ansi_string name)
{
    const ocl_11_entrypoints* ocl_entrypoints   = ocl_get_entrypoints();
    const ocl_platform_info*  platform_info_ptr = ocl_get_platform_info(platform_id);
    _ocl_context*             new_instance      = NULL;

    if (platform_info_ptr == NULL)
    {
        LOG_ERROR        ("Platform [%p] not recognized - cannot create CL context", platform_id);
        ASSERT_DEBUG_SYNC(false, "");

        goto end;
    }

    /* Create new CL context instance */
    new_instance = new (std::nothrow) _ocl_context;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        /* Initialize */
        _ocl_context_init(new_instance, platform_id);

        /* Since we need GL sharing to work, the remaining part of the initialization process needs to be done
         * from within a rendering thread */
        ogl_context_request_callback_from_context_thread(gl_context,
                                                         _ocl_context_create_gpu_only_with_gl_sharing_rendering_callback,
                                                         new_instance);        

        /* Retain GL context */
        new_instance->gl_context = gl_context;
        ogl_context_retain(new_instance->gl_context);

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _ocl_context_release,
                                                       OBJECT_TYPE_OCL_CONTEXT, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenCL Contexts\\", system_hashed_ansi_string_get_buffer(name) )
                                                       );
    }

end:
    return (ocl_context) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ocl_context_get_assigned_devices(__in  __notnull ocl_context    context,
                                                         __out __notnull cl_device_id** out_devices,
                                                         __out __notnull uint32_t*      out_n_devices)
{
    _ocl_context* context_ptr = (_ocl_context*) context;

    *out_devices   = context_ptr->devices;
    *out_n_devices = context_ptr->n_devices;
}

/* Please see header for specification */
PUBLIC EMERALD_API cl_command_queue ocl_context_get_command_queue(__in __notnull ocl_context context, __in uint32_t n_device)
{
    _ocl_context* context_ptr = (_ocl_context*) context;

    ASSERT_DEBUG_SYNC(context_ptr->n_devices > n_device, "Device index [%d] is invalid!", n_device);
    if (context_ptr->n_devices > n_device)
    {
        return context_ptr->command_queues[n_device];
    }
    else
    {
        return NULL;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API cl_context ocl_context_get_context(__in __notnull ocl_context context)
{
    return ((_ocl_context*)context)->context;
}

/* Please see header for specification */
PUBLIC EMERALD_API cl_platform_id ocl_context_get_platform_id(__in __notnull ocl_context context)
{
    return ((_ocl_context*)context)->platform_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ocl_context_set_driver_error_callback_behavior(__in __notnull ocl_context context,
                                                                       __in           bool        should_cause_assertion_failure)
{
    ((_ocl_context*)context)->should_ocl_error_callbacks_cause_assertion_failure = should_cause_assertion_failure;
}