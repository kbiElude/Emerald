/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_types.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

ocl_11_entrypoints             cl_entrypoints;
ocl_khr_gl_sharing_entrypoints cl_khr_gl_sharing_entrypoints;

HMODULE            cl_handle                              = NULL;
system_hash64map   cl_platform_to_platform_descriptor_map = NULL;
system_hash64map   cl_platform_to_device_map_map          = NULL;


/** TODO */
PRIVATE void _ocl_deinit_device_info(ocl_device_info* data_ptr)
{
    if (data_ptr->device_extensions != NULL)
    {
        delete [] data_ptr->device_extensions;

        data_ptr->device_extensions = NULL;
    }

    if (data_ptr->device_max_work_item_sizes != NULL)
    {
        delete [] data_ptr->device_max_work_item_sizes;

        data_ptr->device_max_work_item_sizes = NULL;
    }

    if (data_ptr->device_name != NULL)
    {
        delete [] data_ptr->device_name;

        data_ptr->device_name = NULL;
    }

    if (data_ptr->device_opencl_c_version != NULL)
    {
        delete [] data_ptr->device_opencl_c_version;

        data_ptr->device_opencl_c_version = NULL;
    }

    if (data_ptr->device_profile != NULL)
    {
        delete [] data_ptr->device_profile;

        data_ptr->device_profile = NULL;
    }

    if (data_ptr->device_vendor != NULL)
    {
        delete [] data_ptr->device_vendor;

        data_ptr->device_vendor = NULL;
    }

    if (data_ptr->device_version != NULL)
    {
        delete [] data_ptr->device_version;

        data_ptr->device_version = NULL;
    }

    if (data_ptr->driver_version != NULL)
    {
        delete [] data_ptr->driver_version;

        data_ptr->driver_version = NULL;
    }
}

/** TODO */
PRIVATE void _ocl_deinit_platform_info(ocl_platform_info* data_ptr)
{
    if (data_ptr->platform_extensions != NULL)
    {
        delete [] data_ptr->platform_extensions;
        data_ptr->platform_extensions = 0;
    }

    if (data_ptr->platform_name != NULL)
    {
        delete [] data_ptr->platform_name;
        data_ptr->platform_name = 0;
    }

    if (data_ptr->platform_profile != NULL)
    {
        delete [] data_ptr->platform_profile;
        data_ptr->platform_profile = 0;
    }

    if (data_ptr->platform_vendor != NULL)
    {
        delete [] data_ptr->platform_vendor;
        data_ptr->platform_vendor = 0;
    }

    if (data_ptr->platform_version != NULL)
    {
        delete [] data_ptr->platform_version;
        data_ptr->platform_version = 0;
    }   
}

/** TODO */
PRIVATE void _ocl_init_device_info(ocl_device_info* device_info)
{
    device_info->device_address_bits                            = 0;
    device_info->device_available                               = 0;
    device_info->device_compiler_available                      = 0;
    device_info->device_little_endian                           = 0;
    device_info->device_error_correction_support                = 0;
    device_info->device_kernel_execution_support                = 0;
    device_info->device_native_kernel_execution_support         = 0;
    device_info->device_extensions                              = 0;
    device_info->device_global_mem_cache_size                   = 0;
    device_info->device_global_mem_cache_type                   = 0;
    device_info->device_global_mem_cacheline_size               = 0;
    device_info->device_global_mem_size                         = 0;
    device_info->device_host_unified_memory                     = 0;
    device_info->device_id                                      = 0;
    device_info->device_image_support                           = 0;
    device_info->device_image2d_max_height                      = 0;
    device_info->device_image2d_max_width                       = 0;
    device_info->device_image3d_max_depth                       = 0;
    device_info->device_image3d_max_height                      = 0;
    device_info->device_image3d_max_width                       = 0;
    device_info->device_local_mem_size                          = 0;
    device_info->device_local_mem_type                          = 0;
    device_info->device_max_clock_frequency                     = 0;
    device_info->device_max_compute_units                       = 0;
    device_info->device_max_constant_args                       = 0;
    device_info->device_max_constant_buffer_size                = 0;
    device_info->device_max_mem_alloc_size                      = 0;
    device_info->device_max_parameter_size                      = 0;
    device_info->device_max_read_image_args                     = 0;
    device_info->device_max_samplers                            = 0;
    device_info->device_max_work_group_size                     = 0;
    device_info->device_max_work_item_dimensions                = 0;
    device_info->device_max_work_item_sizes                     = 0;
    device_info->device_max_write_image_args                    = 0;
    device_info->device_mem_base_addr_align                     = 0;
    device_info->device_name                                    = 0;
    device_info->device_native_vector_width_char                = 0;
    device_info->device_native_vector_width_short               = 0;
    device_info->device_native_vector_width_int                 = 0;
    device_info->device_native_vector_width_long                = 0;
    device_info->device_native_vector_width_float               = 0;
    device_info->device_native_vector_width_double              = 0;
    device_info->device_native_vector_width_half                = 0;
    device_info->device_opencl_c_version                        = 0;
    device_info->device_preferred_vector_width_char             = 0;
    device_info->device_preferred_vector_width_short            = 0;
    device_info->device_preferred_vector_width_int              = 0;
    device_info->device_preferred_vector_width_long             = 0;
    device_info->device_preferred_vector_width_float            = 0;
    device_info->device_preferred_vector_width_double           = 0;
    device_info->device_preferred_vector_width_half             = 0;
    device_info->device_profile                                 = 0;
    device_info->device_profiling_timer_resolution              = 0;
    device_info->device_out_of_order_exec_mode_enable_supported = 0;
    device_info->device_queue_profiling_enable_supported        = 0;
    device_info->device_single_fp_denorms_supported             = 0;
    device_info->device_single_fp_inf_nans_supported            = 0;
    device_info->device_single_fp_round_to_inf_supported        = 0;
    device_info->device_single_fp_round_to_nearest_supported    = 0;
    device_info->device_single_fp_round_to_zero_supported       = 0;
    device_info->device_single_fp_fused_multiply_add_supported  = 0;
    device_info->device_type                                    = 0;
    device_info->device_vendor                                  = 0;
    device_info->device_vendor_id                               = 0;
    device_info->device_version                                 = 0;
    device_info->driver_version                                 = 0;
}

/** TODO */
PRIVATE void _ocl_init_entrypoints()
{
    cl_entrypoints.pCLBuildProgram                           = NULL;
    cl_entrypoints.pCLCreateBuffer                           = NULL;
    cl_entrypoints.pCLCreateCommandQueue                     = NULL;
    cl_entrypoints.pCLCreateContext                          = NULL;
    cl_entrypoints.pCLCreateContextFromType                  = NULL;
    cl_entrypoints.pCLCreateImage2D                          = NULL; 
    cl_entrypoints.pCLCreateImage3D                          = NULL;
    cl_entrypoints.pCLCreateKernel                           = NULL;
    cl_entrypoints.pCLCreateKernelsInProgram                 = NULL;
    cl_entrypoints.pCLCreateProgramWithBinary                = NULL;
    cl_entrypoints.pCLCreateProgramWithSource                = NULL;
    cl_entrypoints.pCLCreateSampler                          = NULL;
    cl_entrypoints.pCLCreateSubBuffer                        = NULL;
    cl_entrypoints.pCLCreateUserEvent                        = NULL;
    cl_entrypoints.pCLEnqueueBarrier                         = NULL;
    cl_entrypoints.pCLEnqueueCopyBuffer                      = NULL;
    cl_entrypoints.pCLEnqueueCopyBufferRect                  = NULL;
    cl_entrypoints.pCLEnqueueCopyBufferToImage               = NULL;
    cl_entrypoints.pCLEnqueueCopyImage                       = NULL;
    cl_entrypoints.pCLEnqueueCopyImageToBuffer               = NULL;
    cl_entrypoints.pCLEnqueueMapBuffer                       = NULL;
    cl_entrypoints.pCLEnqueueMapImage                        = NULL;
    cl_entrypoints.pCLEnqueueMarker                          = NULL;
    cl_entrypoints.pCLEnqueueNativeKernel                    = NULL;
    cl_entrypoints.pCLEnqueueNDRangeKernel                   = NULL;
    cl_entrypoints.pCLEnqueueReadBuffer                      = NULL;
    cl_entrypoints.pCLEnqueueReadBufferRect                  = NULL;
    cl_entrypoints.pCLEnqueueReadImage                       = NULL;
    cl_entrypoints.pCLEnqueueTask                            = NULL;
    cl_entrypoints.pCLEnqueueUnmapMemObject                  = NULL;
    cl_entrypoints.pCLEnqueueWaitForEvents                   = NULL;
    cl_entrypoints.pCLEnqueueWriteBuffer                     = NULL;
    cl_entrypoints.pCLEnqueueWriteBufferRect                 = NULL;
    cl_entrypoints.pCLEnqueueWriteImage                      = NULL;
    cl_entrypoints.pCLFinish                                 = NULL;
    cl_entrypoints.pCLFlush                                  = NULL;
    cl_entrypoints.pCLGetCommandQueueInfo                    = NULL;
    cl_entrypoints.pCLGetContextInfo                         = NULL;
    cl_entrypoints.pCLGetDeviceIDs                           = NULL;
    cl_entrypoints.pCLGetDeviceInfo                          = NULL;
    cl_entrypoints.pCLGetExtensionFunctionAddress            = NULL;
    cl_entrypoints.pCLGetEventInfo                           = NULL;
    cl_entrypoints.pCLGetEventProfilingInfo                  = NULL;
    cl_entrypoints.pCLGetImageInfo                           = NULL;
    cl_entrypoints.pCLGetKernelInfo                          = NULL;
    cl_entrypoints.pCLGetKernelWorkGroupInfo                 = NULL;
    cl_entrypoints.pCLGetMemObjectInfo                       = NULL;
    cl_entrypoints.pCLGetPlatformIDs                         = NULL;
    cl_entrypoints.pCLGetPlatformInfo                        = NULL;
    cl_entrypoints.pCLGetProgramBuildInfo                    = NULL;
    cl_entrypoints.pCLGetProgramInfo                         = NULL;
    cl_entrypoints.pCLGetSamplerInfo                         = NULL;
    cl_entrypoints.pCLGetSupportedImageFormats               = NULL;
    cl_entrypoints.pCLReleaseCommandQueue                    = NULL;
    cl_entrypoints.pCLReleaseContext                         = NULL;
    cl_entrypoints.pCLReleaseEvent                           = NULL;
    cl_entrypoints.pCLReleaseKernel                          = NULL;
    cl_entrypoints.pCLReleaseMemObject                       = NULL;
    cl_entrypoints.pCLReleaseProgram                         = NULL;
    cl_entrypoints.pCLReleaseSampler                         = NULL;
    cl_entrypoints.pCLRetainCommandQueue                     = NULL;
    cl_entrypoints.pCLRetainContext                          = NULL;
    cl_entrypoints.pCLRetainEvent                            = NULL;
    cl_entrypoints.pCLRetainKernel                           = NULL;
    cl_entrypoints.pCLRetainMemObject                        = NULL;
    cl_entrypoints.pCLRetainProgram                          = NULL;
    cl_entrypoints.pCLRetainSampler                          = NULL;
    cl_entrypoints.pCLSetCommandQueueProperty                = NULL;
    cl_entrypoints.pCLSetEventCallback                       = NULL;
    cl_entrypoints.pCLSetKernelArg                           = NULL;
    cl_entrypoints.pCLSetMemObjectDestructorCallback         = NULL;
    cl_entrypoints.pCLSetUserEventStatus                     = NULL;
    cl_entrypoints.pCLUnloadCompiler                         = NULL;
    cl_entrypoints.pCLWaitForEvents                          = NULL;

    cl_khr_gl_sharing_entrypoints.pCLCreateFromGLBuffer       = NULL;
    cl_khr_gl_sharing_entrypoints.pCLCreateFromGLRenderbuffer = NULL;
    cl_khr_gl_sharing_entrypoints.pCLCreateFromGLTexture2D    = NULL;
    cl_khr_gl_sharing_entrypoints.pCLCreateFromGLTexture3D    = NULL;
    cl_khr_gl_sharing_entrypoints.pCLEnqueueAcquireGLObjects  = NULL;
    cl_khr_gl_sharing_entrypoints.pCLEnqueueReleaseGLObjects  = NULL;
    cl_khr_gl_sharing_entrypoints.pCLGetGLObjectInfo          = NULL;
    cl_khr_gl_sharing_entrypoints.pCLGetGLTextureInfo         = NULL;
}

/** TODO */
PRIVATE void _ocl_init_platform_info(ocl_platform_info* data_ptr)
{
    data_ptr->platform_extensions = NULL;
    data_ptr->platform_id         = 0;
    data_ptr->platform_name       = NULL;
    data_ptr->platform_profile    = NULL;
    data_ptr->platform_vendor     = NULL;
    data_ptr->platform_version    = NULL;
}


/* Please see header for specification */
PUBLIC void _ocl_deinit()
{
    /* Release all descriptors and maps */
    if (cl_platform_to_platform_descriptor_map != NULL)
    {
        while (system_hash64map_get_amount_of_elements(cl_platform_to_platform_descriptor_map) > 0)
        {
            ocl_platform_info* platform_descriptor = NULL;
            system_hash64      temp                = 0;

            if (system_hash64map_get_element_at(cl_platform_to_platform_descriptor_map, 0, &platform_descriptor, &temp) )
            {
                _ocl_deinit_platform_info(platform_descriptor);

                delete platform_descriptor;

                system_hash64map_remove(cl_platform_to_platform_descriptor_map, temp);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve element from CL platform=>platform descriptor map");
            }
        }

        system_hash64map_release(cl_platform_to_platform_descriptor_map);
        cl_platform_to_platform_descriptor_map = NULL;
    }

    if (cl_platform_to_device_map_map != NULL)
    {
        while (system_hash64map_get_amount_of_elements(cl_platform_to_device_map_map) > 0)
        {
            system_hash64map devices_map = NULL;
            system_hash64    temp        = 0;

            if (system_hash64map_get_element_at(cl_platform_to_device_map_map, 0, &devices_map, &temp) )
            {
                ocl_device_info* device_ptr = NULL;
                uint32_t         n_element  = 0;
                system_hash64    temp2      = 0;

                while (system_hash64map_get_element_at(devices_map, n_element, &device_ptr, &temp2) )
                {
                    _ocl_deinit_device_info(device_ptr);

                    delete device_ptr;

                    system_hash64map_remove(devices_map, temp2);
                }

                system_hash64map_release(devices_map);
                system_hash64map_remove (cl_platform_to_device_map_map, temp);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve element from CL platform=>device map map");
            }
        }

        system_hash64map_release(cl_platform_to_device_map_map);
        cl_platform_to_device_map_map = NULL;
    }

    /* Release DLL handle */
    if (cl_handle != NULL)
    {
        ::FreeLibrary(cl_handle);

        cl_handle = NULL;
    }
}

/* Please see header for specification */
PUBLIC void _ocl_init()
{
    _ocl_init_entrypoints();

    #define LOAD_ENTRY_POINT(target_func_ptr, func_type, func_name)                                                 \
        target_func_ptr = (func_type) ::GetProcAddress(cl_handle, func_name);                                       \
        if (target_func_ptr == NULL)                                                                                \
        {                                                                                                           \
            LOG_ERROR(func_name "() is unavailable - demo may crash if it requires this OpenCL function to work."); \
        }

    /* Instantiate global variables */
    cl_platform_to_device_map_map          = system_hash64map_create(sizeof(system_hash64map)  );
    cl_platform_to_platform_descriptor_map = system_hash64map_create(sizeof(ocl_platform_info*));

    /* Try to open corresponding DLL and load the imports */
    cl_handle = ::LoadLibraryA("opencl.dll");

    ASSERT_DEBUG_SYNC(cl_handle != NULL, "OpenCL is unavailable - application will crash if it requires OpenCL to work");
    if (cl_handle != NULL)
    {
        LOAD_ENTRY_POINT(cl_entrypoints.pCLBuildProgram,                           PFNCLBUILDPROGRAMPROC,                   "clBuildProgram");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateBuffer,                           PFNCLCREATEBUFFERPROC,                   "clCreateBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateCommandQueue,                     PFNCLCREATECOMMANDQUEUEPROC,             "clCreateCommandQueue");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateContext,                          PFNCLCREATECONTEXTPROC,                  "clCreateContext");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateContextFromType,                  PFNCLCREATECONTEXTFROMTYPEPROC,          "clCreateContextFromType");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateImage2D,                          PFNCLCREATEIMAGE2DPROC,                  "clCreateImage2D");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateImage3D,                          PFNCLCREATEIMAGE3DPROC,                  "clCreateImage3D");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateSampler,                          PFNCLCREATESAMPLERPROC,                  "clCreateSampler");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateKernel,                           PFNCLCREATEKERNELPROC,                   "clCreateKernel");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateKernelsInProgram,                 PFNCLCREATEKERNELSINPROGRAMPROC,         "clCreateKernelsInProgram");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateProgramWithBinary,                PFNCLCREATEPROGRAMWITHBINARYPROC,        "clCreateProgramWithBinary");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateProgramWithSource,                PFNCLCREATEPROGRAMWITHSOURCEPROC,        "clCreateProgramWithSource");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLCreateSubBuffer,                        PFNCLCREATESUBBUFFERPROC,                "clCreateSubBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueBarrier,                         PFNCLENQUEUEBARRIERPROC,                 "clEnqueueBarrier");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueCopyBuffer,                      PFNCLENQUEUECOPYBUFFERPROC,              "clEnqueueCopyBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueCopyBufferRect,                  PFNCLENQUEUECOPYBUFFERRECTPROC,          "clEnqueueCopyBufferRect");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueCopyBufferToImage,               PFNCLENQUEUECOPYBUFFERTOIMAGEPROC,       "clEnqueueCopyBufferToImage");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueCopyImage,                       PFNCLENQUEUECOPYIMAGEPROC,               "clEnqueueCopyImage");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueCopyImageToBuffer,               PFNCLENQUEUECOPYIMAGETOBUFFERPROC,       "clEnqueueCopyImageToBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueMapBuffer,                       PFNCLENQUEUEMAPBUFFERPROC,               "clEnqueueMapBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueMapImage,                        PFNCLENQUEUEMAPIMAGEPROC,                "clEnqueueMapImage");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueMarker,                          PFNCLENQUEUEMARKERPROC,                  "clEnqueueMarker");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueNativeKernel,                    PFNCLENQUEUENATIVEKERNELPROC,            "clEnqueueNativeKernel");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueNDRangeKernel,                   PFNCLENQUEUENDRANGEKERNELPROC,           "clEnqueueNDRangeKernel");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueReadBuffer,                      PFNCLENQUEUEREADBUFFERPROC,              "clEnqueueReadBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueReadBufferRect,                  PFNCLENQUEUEREADBUFFERRECTPROC,          "clEnqueueReadBufferRect");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueReadImage,                       PFNCLENQUEUEREADIMAGEPROC,               "clEnqueueReadImage");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueTask,                            PFNCLENQUEUETASKPROC,                    "clEnqueueTask");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueUnmapMemObject,                  PFNCLENQUEUEUNMAPMEMOBJECTPROC,          "clEnqueueUnmapMemObject");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueWaitForEvents,                   PFNCLENQUEUEWAITFOREVENTSPROC,           "clEnqueueWaitForEvents");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueWriteBuffer,                     PFNCLENQUEUEWRITEBUFFERPROC,             "clEnqueueWriteBuffer");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueWriteBufferRect,                 PFNCLENQUEUEWRITEBUFFERRECTPROC,         "clEnqueueWriteBufferRect");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLEnqueueWriteImage,                      PFNCLENQUEUEWRITEIMAGEPROC,              "clEnqueueWriteImage");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLFinish,                                 PFNCLFINISHPROC,                         "clFinish");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLFlush,                                  PFNCLFLUSHPROC,                          "clFlush");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetCommandQueueInfo,                    PFNCLGETCOMMANDQUEUEINFOPROC,            "clGetCommandQueueInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetContextInfo,                         PFNCLGETCONTEXTINFOPROC,                 "clGetContextInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetDeviceIDs,                           PFNCLGETDEVICEIDSPROC,                   "clGetDeviceIDs");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetDeviceInfo,                          PFNCLGETDEVICEINFOPROC,                  "clGetDeviceInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetExtensionFunctionAddress,            PFNCLGETEXTENSIONFUNCTIONADDRESSPROC,    "clGetExtensionFunctionAddress");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetEventInfo,                           PFNCLGETEVENTINFOPROC,                   "clGetEventInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetEventProfilingInfo,                  PFNCLGETEVENTPROFILINGINFOPROC,          "clGetEventProfilingInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetImageInfo,                           PFNCLGETIMAGEINFOPROC,                   "clGetImageInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetKernelInfo,                          PFNCLGETKERNELINFOPROC,                  "clGetKernelInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetKernelWorkGroupInfo,                 PFNCLGETKERNELWORKGROUPINFOPROC,         "clGetKernelWorkGroupInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetMemObjectInfo,                       PFNCLGETMEMOBJECTINFOPROC,               "clGetMemObjectInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetProgramBuildInfo,                    PFNCLGETPROGRAMBUILDINFOPROC,            "clGetProgramBuildInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetPlatformIDs,                         PFNCLGETPLATFORMIDSPROC,                 "clGetPlatformIDs");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetPlatformInfo,                        PFNCLGETPLATFORMINFOPROC,                "clGetPlatformInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetProgramInfo,                         PFNCLGETPROGRAMINFOPROC,                 "clGetProgramInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetSamplerInfo,                         PFNCLGETSAMPLERINFOPROC,                 "clGetSamplerInfo");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLGetSupportedImageFormats,               PFNCLGETSUPPORTEDIMAGEFORMATSPROC,       "clGetSupportedImageFormats");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseCommandQueue,                    PFNCLRELEASECOMMANDQUEUEPROC,            "clReleaseCommandQueue");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseContext,                         PFNCLRELEASECONTEXTPROC,                 "clReleaseContext");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseEvent,                           PFNCLRELEASEEVENTPROC,                   "clReleaseEvent");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseKernel,                          PFNCLRELEASEKERNELPROC,                  "clReleaseKernel");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseMemObject,                       PFNCLRELEASEMEMOBJECTPROC,               "clReleaseMemObject");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseProgram,                         PFNCLRELEASEPROGRAMPROC,                 "clReleaseProgram");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLReleaseSampler,                         PFNCLRELEASESAMPLERPROC,                 "clReleaseSampler");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainCommandQueue,                     PFNCLRETAINCOMMANDQUEUEPROC,             "clRetainCommandQueue");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainContext,                          PFNCLRETAINCONTEXTPROC,                  "clRetainContext");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainEvent,                            PFNCLRETAINEVENTPROC,                    "clRetainEvent");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainKernel,                           PFNCLRETAINKERNELPROC,                   "clRetainKernel");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainMemObject,                        PFNCLRETAINMEMOBJECTPROC,                "clRetainMemObject");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainProgram,                          PFNCLRETAINPROGRAMPROC,                  "clRetainProgram");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLRetainSampler,                          PFNCLRETAINSAMPLERPROC,                  "clRetainSampler");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLSetCommandQueueProperty,                PFNCLSETCOMMANDQUEUEPROPERTYPROC,        "clSetCommandQueueProperty");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLSetEventCallback,                       PFNCLSETEVENTCALLBACKPROC,               "clSetEventCallback");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLSetKernelArg,                           PFNCLSETKERNELARGPROC,                   "clSetKernelArg");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLSetMemObjectDestructorCallback,         PFNCLSETMEMOBJECTDESTRUCTORCALLBACKPROC, "clSetMemObjectDestructorCallback");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLSetUserEventStatus,                     PFNCLSETUSEREVENTSTATUSPROC,             "clSetUserEventStatus");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLUnloadCompiler,                         PFNCLUNLOADCOMPILERPROC,                 "clUnloadCompiler");
        LOAD_ENTRY_POINT(cl_entrypoints.pCLWaitForEvents,                          PFNCLWAITFOREVENTSPROC,                  "clWaitForEvents");

        /* We expect GL sharing to be present so load the entry-points as well */
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLCreateFromGLBuffer,      PFNCLCREATEFROMGLBUFFERPROC,             "clCreateFromGLBuffer");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLCreateFromGLRenderbuffer,PFNCLCREATEFROMGLRENDERBUFFERPROC,       "clCreateFromGLRenderbuffer");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLCreateFromGLTexture2D,   PFNCLCREATEFROMGLTEXTURE2DPROC,          "clCreateFromGLTexture2D");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLCreateFromGLTexture3D,   PFNCLCREATEFROMGLTEXTURE3DPROC,          "clCreateFromGLTexture3D");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLEnqueueAcquireGLObjects, PFNCLENQUEUEACQUIREGLOBJECTSPROC,        "clEnqueueAcquireGLObjects");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLEnqueueReleaseGLObjects, PFNCLENQUEUERELEASEGLOBJECTSPROC,        "clEnqueueReleaseGLObjects");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLGetGLObjectInfo,         PFNCLGETGLOBJECTINFOPROC,                "clGetGLObjectInfo");
        LOAD_ENTRY_POINT(cl_khr_gl_sharing_entrypoints.pCLGetGLTextureInfo,        PFNCLGETGLTEXTUREINFOPROC,               "clGetGLTextureInfo");

        /* Now that we have some imports, stash device data */
        cl_platform_id* cl_platform_ids = NULL; 
        cl_int          cl_result       = 0;
        cl_uint         n_platforms     = 0;

        cl_result = cl_entrypoints.pCLGetPlatformIDs(NULL, NULL, &n_platforms);

        if (cl_result != CL_SUCCESS)
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve ids of available platforms or no CL platforms supported.");

            goto end;
        }
        
        cl_platform_ids = new (std::nothrow) cl_platform_id[n_platforms];
        
        if (cl_platform_ids == NULL)
        {
            ASSERT_DEBUG_SYNC(false, "Out of memory while allocating cl_platform_id array");

            goto end;
        }

        cl_result = cl_entrypoints.pCLGetPlatformIDs(n_platforms, cl_platform_ids, NULL);

        if (cl_result != CL_SUCCESS)
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve platform IDs");

            goto end;
        }

        /* Enumerate available platforms */
        for (cl_uint n_platform = 0; n_platform < n_platforms; ++n_platform)
        {
            uint32_t n_devices_for_platform = 0;

            cl_result = cl_entrypoints.pCLGetDeviceIDs(cl_platform_ids[n_platform], CL_DEVICE_TYPE_ALL, NULL, NULL, &n_devices_for_platform);
            if (cl_result != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve device identifiers for platform [%d]", n_platform);
                goto end;
            }

            /* We are going to use a hashed map for device descriptors, but meanwhile let's just grab memory from heap to read CL feedback */
            cl_device_id* devices_for_platform = new (std::nothrow) cl_device_id[n_devices_for_platform];

            if (devices_for_platform == NULL)
            {
                ASSERT_DEBUG_SYNC(false, "Out of memory");
                goto end;
            }

            cl_result = cl_entrypoints.pCLGetDeviceIDs(cl_platform_ids[n_platform], CL_DEVICE_TYPE_ALL, n_devices_for_platform, devices_for_platform, NULL);

            if (cl_result != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve device ids for platform [%d]", n_platform);
                goto end;
            }

            /* Query platform info */
            #define STORE_CL_PLATFORM_PROPERTY(property_name, target, target_size)                                                                        \
                if ((cl_result = cl_entrypoints.pCLGetPlatformInfo(cl_platform_ids[n_platform], property_name, target_size, target, NULL)) != CL_SUCCESS) \
                {                                                                                                                                         \
                    LOG_ERROR("OpenCL platform property [%s] couldn't be queried due to error [%d]", #property_name, cl_result);                          \
                }                                                                                                                                                     

            uint32_t platform_profile_string_size    = 0;
            uint32_t platform_version_string_size    = 0;
            uint32_t platform_name_string_size       = 0;
            uint32_t platform_vendor_string_size     = 0;
            uint32_t platform_extensions_string_size = 0;

            cl_result += cl_entrypoints.pCLGetPlatformInfo(cl_platform_ids[n_platform], CL_PLATFORM_PROFILE,    NULL, NULL, &platform_profile_string_size);
            cl_result += cl_entrypoints.pCLGetPlatformInfo(cl_platform_ids[n_platform], CL_PLATFORM_VERSION,    NULL, NULL, &platform_version_string_size);
            cl_result += cl_entrypoints.pCLGetPlatformInfo(cl_platform_ids[n_platform], CL_PLATFORM_NAME,       NULL, NULL, &platform_name_string_size);
            cl_result += cl_entrypoints.pCLGetPlatformInfo(cl_platform_ids[n_platform], CL_PLATFORM_VENDOR,     NULL, NULL, &platform_vendor_string_size);
            cl_result += cl_entrypoints.pCLGetPlatformInfo(cl_platform_ids[n_platform], CL_PLATFORM_EXTENSIONS, NULL, NULL, &platform_extensions_string_size);

            if (cl_result != 0)
            {
                ASSERT_DEBUG_SYNC(false, "Error querying platform string size");

                goto end;
            }

            /* Instantiate and fill platform descriptor */
            ocl_platform_info* platform_info = new (std::nothrow) ocl_platform_info;

            if (platform_info == NULL)
            {
                ASSERT_DEBUG_SYNC(false, "Out of memory");

                goto end;
            }

            _ocl_init_platform_info(platform_info);

            platform_info->platform_extensions = new (std::nothrow) char[platform_extensions_string_size+1];
            platform_info->platform_id         = cl_platform_ids[n_platform];
            platform_info->platform_name       = new (std::nothrow) char[platform_name_string_size      +1];
            platform_info->platform_profile    = new (std::nothrow) char[platform_profile_string_size   +1];
            platform_info->platform_vendor     = new (std::nothrow) char[platform_vendor_string_size    +1];
            platform_info->platform_version    = new (std::nothrow) char[platform_version_string_size   +1];

            if (platform_info->platform_extensions == NULL || platform_info->platform_name   == NULL ||
                platform_info->platform_profile    == NULL || platform_info->platform_vendor == NULL ||
                platform_info->platform_version    == NULL)
            {
                ASSERT_DEBUG_SYNC(false, "Out of memory");

                goto end;
            }

            memset(platform_info->platform_extensions, 0, platform_extensions_string_size+1);
            memset(platform_info->platform_name,       0, platform_name_string_size+1);
            memset(platform_info->platform_profile,    0, platform_profile_string_size+1);
            memset(platform_info->platform_vendor,     0, platform_vendor_string_size+1);
            memset(platform_info->platform_version,    0, platform_version_string_size+1);

            STORE_CL_PLATFORM_PROPERTY(CL_PLATFORM_EXTENSIONS, platform_info->platform_extensions, platform_extensions_string_size);
            STORE_CL_PLATFORM_PROPERTY(CL_PLATFORM_NAME,       platform_info->platform_name,       platform_name_string_size);
            STORE_CL_PLATFORM_PROPERTY(CL_PLATFORM_PROFILE,    platform_info->platform_profile,    platform_profile_string_size);
            STORE_CL_PLATFORM_PROPERTY(CL_PLATFORM_VENDOR,     platform_info->platform_vendor,     platform_vendor_string_size);
            STORE_CL_PLATFORM_PROPERTY(CL_PLATFORM_VERSION,    platform_info->platform_version,    platform_version_string_size);

            LOG_INFO("OpenCL platform extensions: [%s]", platform_info->platform_extensions);

            /* Make sure that GL sharing is supported by the platform */
            if (strstr(platform_info->platform_extensions, "cl_khr_gl_sharing") == NULL)
            {
                ::MessageBoxA(NULL, "OpenCL implementation does not support cl_khr_gl_sharing extension; demo will crash if CL/GL sharing is required", "Error", MB_OK | MB_ICONERROR);
            }

            /* Store it */
            system_hash64map_insert(cl_platform_to_platform_descriptor_map, system_hash64(cl_platform_ids[n_platform]), platform_info, NULL, NULL);

            /* Instantiate and fill descriptors for all devices. */
            for (uint32_t n_device = 0; n_device < n_devices_for_platform; ++n_device)
            {
                ocl_device_info* device_descriptor = new (std::nothrow) ocl_device_info;

                if (device_descriptor == NULL)
                {
                    ASSERT_DEBUG_SYNC(false, "Out of memory");
                    goto end;
                }

                /* Reset the descriptor */
                _ocl_init_device_info(device_descriptor);

                device_descriptor->device_id = devices_for_platform[n_device];

                /* Query device's properties. Ignore strings, bitfields, GL_DEVICE_MAX_WORK_ITEM_SIZES for now. 
                 *
                 * Note: CL_SUCCESS is zero, so it's easy to determine something bad has happened just by looking at the value later on */
                uint32_t expected_target_size = 0;

                #define STORE_CL_DEVICE_PROPERTY(property_name, target) \
                    if ((cl_result = cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], property_name, sizeof(target), &target, &expected_target_size)) != CL_SUCCESS) \
                    {                                                                                                                                                               \
                        LOG_ERROR("OpenCL property [%s] couldn't be queried due to error [%d]", #property_name, cl_result);                                                         \
                    }                                                                                                                                                               \
                    ASSERT_DEBUG_SYNC(expected_target_size == sizeof(target), "");

                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_ADDRESS_BITS,              device_descriptor->device_address_bits);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_AVAILABLE,                 device_descriptor->device_available);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_COMPILER_AVAILABLE,        device_descriptor->device_compiler_available);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_ENDIAN_LITTLE,             device_descriptor->device_little_endian);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_ERROR_CORRECTION_SUPPORT,  device_descriptor->device_error_correction_support);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,     device_descriptor->device_global_mem_cache_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,     device_descriptor->device_global_mem_cache_type);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, device_descriptor->device_global_mem_cacheline_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_GLOBAL_MEM_SIZE,           device_descriptor->device_global_mem_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_HOST_UNIFIED_MEMORY,       device_descriptor->device_host_unified_memory);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_IMAGE_SUPPORT,             device_descriptor->device_image_support);

                if (device_descriptor->device_image_support)
                {
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_IMAGE2D_MAX_HEIGHT,    device_descriptor->device_image2d_max_height);
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_IMAGE2D_MAX_WIDTH,     device_descriptor->device_image2d_max_width);
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_IMAGE3D_MAX_DEPTH,     device_descriptor->device_image3d_max_depth);
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_IMAGE3D_MAX_HEIGHT,    device_descriptor->device_image3d_max_height);
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_IMAGE3D_MAX_WIDTH,     device_descriptor->device_image3d_max_width);
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_READ_IMAGE_ARGS,   device_descriptor->device_max_read_image_args);
                    STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_WRITE_IMAGE_ARGS,  device_descriptor->device_max_write_image_args);
                }

                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_LOCAL_MEM_SIZE,                device_descriptor->device_local_mem_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_LOCAL_MEM_TYPE,                device_descriptor->device_local_mem_type);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_CLOCK_FREQUENCY,           device_descriptor->device_max_clock_frequency);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_COMPUTE_UNITS,             device_descriptor->device_max_compute_units);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_CONSTANT_ARGS,             device_descriptor->device_max_constant_args);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,      device_descriptor->device_max_constant_buffer_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_MEM_ALLOC_SIZE,            device_descriptor->device_max_mem_alloc_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_PARAMETER_SIZE,            device_descriptor->device_max_parameter_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_SAMPLERS,                  device_descriptor->device_max_samplers);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_WORK_GROUP_SIZE,           device_descriptor->device_max_work_group_size);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,      device_descriptor->device_max_work_item_dimensions);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_MEM_BASE_ADDR_ALIGN,           device_descriptor->device_mem_base_addr_align);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,      device_descriptor->device_native_vector_width_char);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,     device_descriptor->device_native_vector_width_short);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,       device_descriptor->device_native_vector_width_int);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,      device_descriptor->device_native_vector_width_long);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,     device_descriptor->device_native_vector_width_float);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,    device_descriptor->device_native_vector_width_double);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,      device_descriptor->device_native_vector_width_half);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,   device_descriptor->device_preferred_vector_width_char);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,  device_descriptor->device_preferred_vector_width_short);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,    device_descriptor->device_preferred_vector_width_int);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,   device_descriptor->device_preferred_vector_width_long);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,  device_descriptor->device_preferred_vector_width_float);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, device_descriptor->device_preferred_vector_width_double);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,   device_descriptor->device_preferred_vector_width_half);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_PROFILING_TIMER_RESOLUTION,    device_descriptor->device_profiling_timer_resolution);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_TYPE,                          device_descriptor->device_type);
                STORE_CL_DEVICE_PROPERTY(CL_DEVICE_VENDOR_ID,                     device_descriptor->device_vendor_id);

                ASSERT_DEBUG_SYNC(cl_result == 0, "At least one property could not have been queried!");
                cl_result = 0;

                /* Continue with strings */
                uint32_t device_built_in_kernels_string_size = 0;
                uint32_t device_extensions_string_size       = 0;
                uint32_t device_name_string_size             = 0;
                uint32_t device_opencl_c_version_string_size = 0;
                uint32_t device_profile_string_size          = 0;
                uint32_t device_vendor_string_size           = 0;
                uint32_t device_version_string_size          = 0;
                uint32_t driver_version_string_size          = 0;

                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_EXTENSIONS,       NULL, NULL, &device_extensions_string_size);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_NAME,             NULL, NULL, &device_name_string_size);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_OPENCL_C_VERSION, NULL, NULL, &device_opencl_c_version_string_size);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_PROFILE,          NULL, NULL, &device_profile_string_size);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_VENDOR,           NULL, NULL, &device_vendor_string_size);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_VERSION,          NULL, NULL, &device_version_string_size);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DRIVER_VERSION,          NULL, NULL, &driver_version_string_size);

                ASSERT_DEBUG_SYNC(cl_result == 0, "At least one string length could not have been queried!");
                if (cl_result == 0)
                {
                    device_descriptor->device_extensions       = new (std::nothrow) char[device_extensions_string_size      +1];
                    device_descriptor->device_name             = new (std::nothrow) char[device_name_string_size            +1];
                    device_descriptor->device_opencl_c_version = new (std::nothrow) char[device_opencl_c_version_string_size+1];
                    device_descriptor->device_profile          = new (std::nothrow) char[device_profile_string_size         +1];
                    device_descriptor->device_vendor           = new (std::nothrow) char[device_vendor_string_size          +1];
                    device_descriptor->device_version          = new (std::nothrow) char[device_version_string_size         +1];
                    device_descriptor->driver_version          = new (std::nothrow) char[driver_version_string_size         +1];

                    if (device_descriptor->device_extensions       == NULL ||
                        device_descriptor->device_name             == NULL ||
                        device_descriptor->device_opencl_c_version == NULL ||
                        device_descriptor->device_profile          == NULL ||
                        device_descriptor->device_vendor           == NULL ||
                        device_descriptor->device_version          == NULL ||
                        device_descriptor->driver_version          == NULL)
                    {
                        ASSERT_DEBUG_SYNC(false, "Out of memory");

                        goto end;
                    }

                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_EXTENSIONS,       device_extensions_string_size,       device_descriptor->device_extensions,       NULL);
                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_NAME,             device_name_string_size,             device_descriptor->device_name,             NULL);
                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_OPENCL_C_VERSION, device_opencl_c_version_string_size, device_descriptor->device_opencl_c_version, NULL);
                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_PROFILE,          device_profile_string_size,          device_descriptor->device_profile,          NULL);
                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_VENDOR,           device_vendor_string_size,           device_descriptor->device_vendor,           NULL);
                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_VERSION,          device_version_string_size,          device_descriptor->device_version,          NULL);
                    cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DRIVER_VERSION,          driver_version_string_size,          device_descriptor->driver_version,          NULL);

                    LOG_INFO("OpenCL device extensions for [%s]: [%s]", device_descriptor->device_name, device_descriptor->device_extensions);

                    ASSERT_DEBUG_SYNC(cl_result == 0, "Could not query at least one device info string");
                }

                /* Carry on with bitfields */
                cl_device_exec_capabilities device_exec_capabilities;
                cl_command_queue_properties device_queue_properties;
                cl_device_fp_config         device_single_fp_config;

                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_EXECUTION_CAPABILITIES,   sizeof(device_exec_capabilities), &device_exec_capabilities, NULL);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_QUEUE_PROPERTIES,         sizeof(device_queue_properties),  &device_queue_properties,  NULL);
                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_SINGLE_FP_CONFIG,         sizeof(device_single_fp_config),  &device_single_fp_config,  NULL);

                ASSERT_DEBUG_SYNC(cl_result == 0, "Could not query at least one bitfield property");
                if (cl_result == 0)
                {
                    device_descriptor->device_single_fp_denorms_supported             = (device_single_fp_config & CL_FP_DENORM)           != 0;
                    device_descriptor->device_single_fp_fused_multiply_add_supported  = (device_single_fp_config & CL_FP_FMA)              != 0;
                    device_descriptor->device_single_fp_inf_nans_supported            = (device_single_fp_config & CL_FP_INF_NAN)          != 0;
                    device_descriptor->device_single_fp_round_to_inf_supported        = (device_single_fp_config & CL_FP_ROUND_TO_INF)     != 0;
                    device_descriptor->device_single_fp_round_to_nearest_supported    = (device_single_fp_config & CL_FP_ROUND_TO_NEAREST) != 0;
                    device_descriptor->device_single_fp_round_to_zero_supported       = (device_single_fp_config & CL_FP_ROUND_TO_ZERO)    != 0;
                    device_descriptor->device_single_soft_float                       = (device_single_fp_config & CL_FP_SOFT_FLOAT)       != 0;

                    device_descriptor->device_kernel_execution_support                = device_exec_capabilities & CL_EXEC_KERNEL;
                    device_descriptor->device_native_kernel_execution_support         = device_exec_capabilities & CL_EXEC_NATIVE_KERNEL;

                    device_descriptor->device_out_of_order_exec_mode_enable_supported = (device_queue_properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) != 0;
                    device_descriptor->device_queue_profiling_enable_supported        = (device_queue_properties & CL_QUEUE_PROFILING_ENABLE)              != 0;
                }

                /* Finally - the arrays */
                uint32_t device_max_work_item_sizes_size = 0;

                cl_result += cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_MAX_WORK_ITEM_SIZES, NULL, NULL, &device_max_work_item_sizes_size);

                ASSERT_DEBUG_SYNC(cl_result == 0, "Could not query at least one arrayed property");
                if (cl_result == 0)
                {
                    device_descriptor->device_max_work_item_sizes = new (std::nothrow) size_t[device_max_work_item_sizes_size / sizeof(size_t)];

                    ASSERT_DEBUG_SYNC(device_descriptor->device_max_work_item_sizes != NULL, "Out of memory");
                    if (device_descriptor->device_max_work_item_sizes != NULL)
                    {
                        cl_result = cl_entrypoints.pCLGetDeviceInfo(devices_for_platform[n_device], CL_DEVICE_MAX_WORK_ITEM_SIZES, device_max_work_item_sizes_size, device_descriptor->device_max_work_item_sizes, NULL);

                        ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not query CL_DEVICE_MAX_WORK_ITEM_SIZES");
                    }
                }

                /* The descriptor is now there. Store it */
                system_hash64map platform_device_map = NULL;

                if (!system_hash64map_get(cl_platform_to_device_map_map, system_hash64(cl_platform_ids[n_platform]), &platform_device_map) )
                {
                    platform_device_map = system_hash64map_create(sizeof(system_resizable_vector) );

                    system_hash64map_insert(cl_platform_to_device_map_map, system_hash64(cl_platform_ids[n_platform]), platform_device_map, NULL, NULL);
                }

                if (platform_device_map == NULL)
                {
                    ASSERT_DEBUG_SYNC(platform_device_map != NULL, "Platform device map is NULL");
                    goto end;
                }

                system_hash64map_insert(platform_device_map, system_hash64(devices_for_platform[n_device]), device_descriptor, NULL, NULL);
            }
        }

        /* We no longer need the array */
        delete [] cl_platform_ids;
end: ;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API const ocl_device_info* ocl_get_device_info(__in __notnull cl_platform_id platform_id, 
                                                              __in __notnull cl_device_id   device_id)
{
    system_hash64map       device_map = NULL;
    const ocl_device_info* result     = NULL;

    if (system_hash64map_get(cl_platform_to_device_map_map, system_hash64(platform_id), &device_map) )
    {
        system_hash64map_get(device_map, system_hash64(device_id), &result);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_get_device_id(__in  __notnull cl_platform_id platform_id,
                                          __in            uint32_t       n_device_id, 
                                          __out __notnull cl_device_id*  result)
{
    bool             b_result   = false;
    system_hash64map device_map = NULL;

    if (system_hash64map_get(cl_platform_to_device_map_map, system_hash64(platform_id), &device_map) )
    {
        ocl_device_info* device_info = NULL;
        system_hash64    temp        = 0;

        if (system_hash64map_get_element_at(device_map, n_device_id, &device_info, &temp) )
        {
            *result  = device_info->device_id;
            b_result = true;
        }
    }

    return b_result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_get_device_id_by_name(__in  __notnull cl_platform_id            platform_id,
                                                  __in  __notnull system_hashed_ansi_string name,
                                                  __out __notnull cl_device_id*             out_result)
{
    system_hash64map device_map = NULL;
    bool             result     = false;

    if (system_hash64map_get(cl_platform_to_device_map_map, system_hash64(platform_id), &device_map) )
    {
        uint32_t n_devices = system_hash64map_get_amount_of_elements(device_map);

        for (uint32_t n_device = 0; n_device < n_devices; ++n_device)
        {
            cl_device_id  temp_result      = NULL;
            system_hash64 temp_result_hash = 0;

            if (system_hash64map_get_element_at(device_map, n_device, &temp_result, &temp_result_hash) )
            {
                /* Retrieve device descriptor */
                const ocl_device_info* device_info_ptr = ocl_get_device_info(platform_id, (cl_device_id) temp_result_hash);

                ASSERT_DEBUG_SYNC(device_info_ptr != NULL, "Could not retrieve device info descriptor for device [%p]", temp_result);
                if (device_info_ptr != NULL)
                {
                    if (system_hashed_ansi_string_is_equal_to_raw_string(name, device_info_ptr->device_name) )
                    {
                        /* We have a match */
                        *out_result = (cl_device_id) temp_result_hash;
                        result      = true;

                        break;
                    }
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve hash-map element at index [%d]", n_device);
            }
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API const ocl_11_entrypoints* ocl_get_entrypoints()
{
    return &cl_entrypoints;
}

/* Please see header for specification */
PUBLIC EMERALD_API const ocl_khr_gl_sharing_entrypoints* ocl_get_khr_gl_sharing_entrypoints()
{
    return &cl_khr_gl_sharing_entrypoints;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_get_platform_id(__in            uint32_t        n_platform_id, 
                                            __out __notnull cl_platform_id* result)
{
    ocl_platform_info* platform_info = NULL;
    bool               b_result      = false;
    system_hash64      temp          = 0;

    if (system_hash64map_get_element_at(cl_platform_to_platform_descriptor_map, n_platform_id, &platform_info, &temp) )
    {
        *result  = platform_info->platform_id;
        b_result = true;
    }

    return b_result;
}


/* Please see header for specification */
PUBLIC EMERALD_API const ocl_platform_info* ocl_get_platform_info(__in __notnull cl_platform_id platform_id)
{
    ocl_platform_info* result = NULL;

    system_hash64map_get(cl_platform_to_platform_descriptor_map, system_hash64(platform_id), &result);

    return result;
}