/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OCL_TYPES_H
#define OCL_TYPES_H

#include "ocl/cl/cl.h"
#include "ocl/CL/cl_gl.h"

DECLARE_HANDLE(ocl_context);
DECLARE_HANDLE(ocl_kdtree);
DECLARE_HANDLE(ocl_kernel);
DECLARE_HANDLE(ocl_program);

/* Executor configuration */
typedef struct
{
    /* Retrieves direction vector */
    const char* get_direction_vector_cl_code;
    /* Name of the executor */
    const char* name;
    /* Resets result buffer */
    const char* reset_cl_code;
    /* Updates result buffer with relevant data */
    const char* update_cl_code;
} _ocl_kdtree_executor_configuration;

typedef enum
{
    /* Bitfield */
    OCL_KDTREE_USAGE_MANY_LOCATIONS_ONE_RAY  = 0x1,
    OCL_KDTREE_USAGE_ONE_LOCATIONS_MANY_RAYS = 0x2
} _ocl_kdtree_usage;

/* Creation flags */
typedef int kdtree_creation_flags;

const int KDTREE_SAVE_SUPPORT = 1;

/* Executor ID */
typedef uint32_t ocl_kdtree_executor_id;

typedef cl_int           (CL_API_CALL *PFNCLBUILDPROGRAMPROC)                  (cl_program,                   cl_uint,        const cl_device_id*,                                            const char*, void (CL_CALLBACK*)(cl_program, void*), void*);
typedef cl_mem           (CL_API_CALL *PFNCLCREATEBUFFERPROC)                  (cl_context,                   cl_mem_flags,   size_t,                                                         void*,       cl_int*);
typedef cl_command_queue (CL_API_CALL *PFNCLCREATECOMMANDQUEUEPROC)            (cl_context,                   cl_device_id,   cl_command_queue_properties,                                    cl_int*);
typedef cl_context       (CL_API_CALL *PFNCLCREATECONTEXTPROC)                 (const cl_context_properties*, cl_uint,        const cl_device_id*,                                            void (CL_CALLBACK*)(const char *, const void *, size_t, void *), void*,   cl_int*);
typedef cl_context       (CL_API_CALL *PFNCLCREATECONTEXTFROMTYPEPROC)         (const cl_context_properties*, cl_device_type, void (CL_CALLBACK*)(const char *, const void *, size_t, void*), void*,                                                           cl_int*);
typedef cl_mem           (CL_API_CALL *PFNCLCREATEIMAGE2DPROC)                 (cl_context,                   cl_mem_flags,   const cl_image_format*,                                         size_t,                                                          size_t,  size_t, void*,  cl_int*);
typedef cl_mem           (CL_API_CALL *PFNCLCREATEIMAGE3DPROC)                 (cl_context,                   cl_mem_flags,   const cl_image_format*,                                         size_t,                                                          size_t,  size_t, size_t, size_t, void*, cl_int*);
typedef cl_kernel        (CL_API_CALL *PFNCLCREATEKERNELPROC)                  (cl_program,                   const char*,    cl_int*);              
typedef cl_int           (CL_API_CALL *PFNCLCREATEKERNELSINPROGRAMPROC)        (cl_program,                   cl_uint,        cl_kernel*,                                                     cl_uint*);
typedef cl_program       (CL_API_CALL *PFNCLCREATEPROGRAMWITHBINARYPROC)       (cl_context,                   cl_uint,        const cl_device_id*,                                            const size_t*,  const unsigned char**, cl_int*, cl_int*);
typedef cl_program       (CL_API_CALL *PFNCLCREATEPROGRAMWITHSOURCEPROC)       (cl_context,                   cl_uint,        const char**,                                                   const size_t*,  cl_int*);
typedef cl_sampler       (CL_API_CALL *PFNCLCREATESAMPLERPROC)                 (cl_context,                   cl_bool,        cl_addressing_mode,                                             cl_filter_mode, cl_int*);
typedef cl_mem           (CL_API_CALL *PFNCLCREATESUBBUFFERPROC)               (cl_mem,                       cl_mem_flags,   cl_buffer_create_type,                                          const void*,    cl_int*);
typedef cl_event         (CL_API_CALL *PFNCLCREATEUSEREVENTPROC)               (cl_context,                   cl_int*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEBARRIERPROC)                (cl_command_queue);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUECOPYBUFFERPROC)             (cl_command_queue, cl_mem,                      cl_mem,  size_t,          size_t,          size_t,        cl_uint,      const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUECOPYBUFFERRECTPROC)         (cl_command_queue, cl_mem,                      cl_mem,  const size_t*,   const size_t*,   const size_t*, size_t,       size_t,          size_t,          size_t,          cl_uint,  const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUECOPYBUFFERTOIMAGEPROC)      (cl_command_queue, cl_mem,                      cl_mem,  size_t,          const size_t*,   const size_t*, cl_uint,      const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUECOPYIMAGEPROC)              (cl_command_queue, cl_mem,                      cl_mem,  const size_t*,   const size_t*,   const size_t*, cl_uint,      const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUECOPYIMAGETOBUFFERPROC)      (cl_command_queue, cl_mem,                      cl_mem,  const size_t*,   const size_t*,   size_t,        cl_uint,      const cl_event*, cl_event*);
typedef void*            (CL_API_CALL *PFNCLENQUEUEMAPBUFFERPROC)              (cl_command_queue, cl_mem,                      cl_bool, cl_map_flags,    size_t,          size_t,        cl_uint,      const cl_event*, cl_event*,       cl_int*);
typedef void*            (CL_API_CALL *PFNCLENQUEUEMAPIMAGEPROC)               (cl_command_queue, cl_mem,                      cl_bool, cl_map_flags,    const size_t*,   const size_t*, size_t*,      size_t*,         cl_uint,         const cl_event*, cl_event*, cl_int*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEMARKERPROC)                 (cl_command_queue, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUENATIVEKERNELPROC)           (cl_command_queue, void (CL_CALLBACK*)(void *), void*,           size_t,          cl_uint,         const cl_mem*, const void**, cl_uint,         const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUENDRANGEKERNELPROC)          (cl_command_queue, cl_kernel,                   cl_uint,         const size_t*,   const size_t*,   const size_t*, cl_uint,      const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEREADBUFFERPROC)             (cl_command_queue, cl_mem,                      cl_bool,         size_t,          size_t,          void*,         cl_uint,      const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEREADBUFFERRECTPROC)         (cl_command_queue, cl_mem,                      cl_bool,         const size_t*,   const size_t*,   const size_t*, size_t,       size_t,          size_t,          size_t,          void*,     cl_uint, const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEREADIMAGEPROC)              (cl_command_queue, cl_mem,                      cl_bool,         const size_t*,   const size_t*,   size_t,        size_t,       void*,           cl_uint,         const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUETASKPROC)                   (cl_command_queue, cl_kernel,                   cl_uint,         const cl_event*, cl_event*);                                   
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEWAITFOREVENTSPROC)          (cl_command_queue, cl_uint,                     const cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEWRITEBUFFERPROC)            (cl_command_queue, cl_mem,                      cl_bool,         size_t,          size_t,          const void*,   cl_uint,      const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEWRITEBUFFERRECTPROC)        (cl_command_queue, cl_mem,                      cl_bool,         const size_t*,   const size_t*,   const size_t*, size_t,       size_t,          size_t,          size_t,           const void*, cl_uint, const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEWRITEIMAGEPROC)             (cl_command_queue, cl_mem,                      cl_bool,         const size_t*,   const size_t*,   size_t,        size_t,       const void*,     cl_uint,         const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLENQUEUEUNMAPMEMOBJECTPROC)         (cl_command_queue, cl_mem,                      void*,           cl_uint,         const cl_event*, cl_event*);
typedef cl_int           (CL_API_CALL *PFNCLFINISHPROC)                        (cl_command_queue);
typedef cl_int           (CL_API_CALL *PFNCLFLUSHPROC)                         (cl_command_queue);
typedef cl_int           (CL_API_CALL *PFNCLGETCOMMANDQUEUEINFOPROC)           (cl_command_queue, cl_command_queue_info, size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETCONTEXTINFOPROC)                (cl_context,       cl_context_info,       size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETDEVICEIDSPROC)                  (cl_platform_id,   cl_device_type,        cl_uint,                   cl_device_id*, cl_uint*);
typedef cl_int           (CL_API_CALL *PFNCLGETDEVICEINFOPROC)                 (cl_device_id,     cl_device_info,        size_t,                    void*,         size_t*);    
typedef void*            (CL_API_CALL *PFNCLGETEXTENSIONFUNCTIONADDRESSPROC)   (const char*);
typedef cl_int           (CL_API_CALL *PFNCLGETEVENTINFOPROC)                  (cl_event,         cl_event_info,         size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETEVENTPROFILINGINFOPROC)         (cl_event,         cl_profiling_info,     size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETIMAGEINFOPROC)                  (cl_mem,           cl_image_info,         size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETKERNELINFOPROC)                 (cl_kernel,        cl_kernel_info,        size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETKERNELWORKGROUPINFOPROC)        (cl_kernel,        cl_device_id,          cl_kernel_work_group_info, size_t,        void*,            size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETMEMOBJECTINFOPROC)              (cl_mem,           cl_mem_info,           size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETPLATFORMIDSPROC)                (cl_uint,          cl_platform_id*,       cl_uint*);
typedef cl_int           (CL_API_CALL *PFNCLGETPLATFORMINFOPROC)               (cl_platform_id,   cl_platform_info,      size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETPROGRAMBUILDINFOPROC)           (cl_program,       cl_device_id,          cl_program_build_info,     size_t,        void*,            size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETPROGRAMINFOPROC)                (cl_program,       cl_program_info,       size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETSAMPLERINFOPROC)                (cl_sampler,       cl_sampler_info,       size_t,                    void*,         size_t*);
typedef cl_int           (CL_API_CALL *PFNCLGETSUPPORTEDIMAGEFORMATSPROC)      (cl_context,       cl_mem_flags,          cl_mem_object_type,        cl_uint,       cl_image_format*, cl_uint*);
typedef cl_int           (CL_API_CALL *PFNCLRELEASECOMMANDQUEUEPROC)           (cl_command_queue);
typedef cl_int           (CL_API_CALL *PFNCLRELEASECONTEXTPROC)                (cl_context);
typedef cl_int           (CL_API_CALL *PFNCLRELEASEEVENTPROC)                  (cl_event);
typedef cl_int           (CL_API_CALL *PFNCLRELEASEKERNELPROC)                 (cl_kernel);
typedef cl_int           (CL_API_CALL *PFNCLRELEASEMEMOBJECTPROC)              (cl_mem);
typedef cl_int           (CL_API_CALL *PFNCLRELEASEPROGRAMPROC)                (cl_program);
typedef cl_int           (CL_API_CALL *PFNCLRELEASESAMPLERPROC)                (cl_sampler);
typedef cl_int           (CL_API_CALL *PFNCLRETAINCOMMANDQUEUEPROC)            (cl_command_queue);
typedef cl_int           (CL_API_CALL *PFNCLRETAINCONTEXTPROC)                 (cl_context);
typedef cl_int           (CL_API_CALL *PFNCLRETAINEVENTPROC)                   (cl_event);
typedef cl_int           (CL_API_CALL *PFNCLRETAINKERNELPROC)                  (cl_kernel);
typedef cl_int           (CL_API_CALL *PFNCLRETAINMEMOBJECTPROC)               (cl_mem);
typedef cl_int           (CL_API_CALL *PFNCLRETAINPROGRAMPROC)                 (cl_program);
typedef cl_int           (CL_API_CALL *PFNCLRETAINSAMPLERPROC)                 (cl_sampler);
typedef cl_int           (CL_API_CALL *PFNCLSETCOMMANDQUEUEPROPERTYPROC)       (cl_command_queue, cl_command_queue_properties,        cl_bool,                                       cl_command_queue_properties);
typedef cl_int           (CL_API_CALL *PFNCLSETEVENTCALLBACKPROC)              (cl_event,         cl_int,                             void (CL_CALLBACK*)(cl_event, cl_int, void *), void*);
typedef cl_int           (CL_API_CALL *PFNCLSETKERNELARGPROC)                  (cl_kernel,        cl_uint,                            size_t,                                        const void*);
typedef cl_int           (CL_API_CALL *PFNCLSETMEMOBJECTDESTRUCTORCALLBACKPROC)(cl_mem,           void (CL_CALLBACK*)(cl_mem, void*), void*);
typedef cl_int           (CL_API_CALL *PFNCLSETUSEREVENTSTATUSPROC)            (cl_event,         cl_int);
typedef cl_int           (CL_API_CALL *PFNCLUNLOADCOMPILERPROC)                ();
typedef cl_int           (CL_API_CALL *PFNCLWAITFOREVENTSPROC)                 (cl_uint,          const cl_event*);

typedef struct
{
    PFNCLBUILDPROGRAMPROC                           pCLBuildProgram;
    PFNCLCREATEBUFFERPROC                           pCLCreateBuffer;
    PFNCLCREATECOMMANDQUEUEPROC                     pCLCreateCommandQueue;
    PFNCLCREATECONTEXTPROC                          pCLCreateContext;
    PFNCLCREATECONTEXTFROMTYPEPROC                  pCLCreateContextFromType;
    PFNCLCREATEIMAGE2DPROC                          pCLCreateImage2D;
    PFNCLCREATEIMAGE3DPROC                          pCLCreateImage3D;
    PFNCLCREATEKERNELPROC                           pCLCreateKernel;
    PFNCLCREATEKERNELSINPROGRAMPROC                 pCLCreateKernelsInProgram;
    PFNCLCREATEPROGRAMWITHBINARYPROC                pCLCreateProgramWithBinary;
    PFNCLCREATEPROGRAMWITHSOURCEPROC                pCLCreateProgramWithSource;
    PFNCLCREATESAMPLERPROC                          pCLCreateSampler;
    PFNCLCREATESUBBUFFERPROC                        pCLCreateSubBuffer;
    PFNCLCREATEUSEREVENTPROC                        pCLCreateUserEvent;
    PFNCLENQUEUEBARRIERPROC                         pCLEnqueueBarrier;
    PFNCLENQUEUECOPYBUFFERPROC                      pCLEnqueueCopyBuffer;
    PFNCLENQUEUECOPYBUFFERRECTPROC                  pCLEnqueueCopyBufferRect;
    PFNCLENQUEUECOPYBUFFERTOIMAGEPROC               pCLEnqueueCopyBufferToImage;
    PFNCLENQUEUECOPYIMAGEPROC                       pCLEnqueueCopyImage;
    PFNCLENQUEUECOPYIMAGETOBUFFERPROC               pCLEnqueueCopyImageToBuffer;
    PFNCLENQUEUEMAPBUFFERPROC                       pCLEnqueueMapBuffer;
    PFNCLENQUEUEMAPIMAGEPROC                        pCLEnqueueMapImage;
    PFNCLENQUEUEMARKERPROC                          pCLEnqueueMarker;
    PFNCLENQUEUENATIVEKERNELPROC                    pCLEnqueueNativeKernel;
    PFNCLENQUEUENDRANGEKERNELPROC                   pCLEnqueueNDRangeKernel;
    PFNCLENQUEUEREADBUFFERPROC                      pCLEnqueueReadBuffer;
    PFNCLENQUEUEREADBUFFERRECTPROC                  pCLEnqueueReadBufferRect;
    PFNCLENQUEUEREADIMAGEPROC                       pCLEnqueueReadImage;
    PFNCLENQUEUETASKPROC                            pCLEnqueueTask;
    PFNCLENQUEUEUNMAPMEMOBJECTPROC                  pCLEnqueueUnmapMemObject;
    PFNCLENQUEUEWAITFOREVENTSPROC                   pCLEnqueueWaitForEvents;
    PFNCLENQUEUEWRITEBUFFERPROC                     pCLEnqueueWriteBuffer;
    PFNCLENQUEUEWRITEBUFFERRECTPROC                 pCLEnqueueWriteBufferRect;
    PFNCLENQUEUEWRITEIMAGEPROC                      pCLEnqueueWriteImage;
    PFNCLFINISHPROC                                 pCLFinish;
    PFNCLFLUSHPROC                                  pCLFlush;
    PFNCLGETCOMMANDQUEUEINFOPROC                    pCLGetCommandQueueInfo;
    PFNCLGETCONTEXTINFOPROC                         pCLGetContextInfo;
    PFNCLGETDEVICEIDSPROC                           pCLGetDeviceIDs;
    PFNCLGETDEVICEINFOPROC                          pCLGetDeviceInfo;
    PFNCLGETEVENTINFOPROC                           pCLGetEventInfo;
    PFNCLGETEVENTPROFILINGINFOPROC                  pCLGetEventProfilingInfo;
    PFNCLGETEXTENSIONFUNCTIONADDRESSPROC            pCLGetExtensionFunctionAddress;
    PFNCLGETIMAGEINFOPROC                           pCLGetImageInfo;
    PFNCLGETKERNELINFOPROC                          pCLGetKernelInfo;
    PFNCLGETKERNELWORKGROUPINFOPROC                 pCLGetKernelWorkGroupInfo;
    PFNCLGETMEMOBJECTINFOPROC                       pCLGetMemObjectInfo;
    PFNCLGETPLATFORMIDSPROC                         pCLGetPlatformIDs;
    PFNCLGETPLATFORMINFOPROC                        pCLGetPlatformInfo;
    PFNCLGETPROGRAMBUILDINFOPROC                    pCLGetProgramBuildInfo;
    PFNCLGETPROGRAMINFOPROC                         pCLGetProgramInfo;
    PFNCLGETSAMPLERINFOPROC                         pCLGetSamplerInfo;
    PFNCLGETSUPPORTEDIMAGEFORMATSPROC               pCLGetSupportedImageFormats;
    PFNCLRELEASECOMMANDQUEUEPROC                    pCLReleaseCommandQueue;
    PFNCLRELEASECONTEXTPROC                         pCLReleaseContext;
    PFNCLRELEASEEVENTPROC                           pCLReleaseEvent;
    PFNCLRELEASEKERNELPROC                          pCLReleaseKernel;
    PFNCLRELEASEMEMOBJECTPROC                       pCLReleaseMemObject;
    PFNCLRELEASEPROGRAMPROC                         pCLReleaseProgram;
    PFNCLRELEASESAMPLERPROC                         pCLReleaseSampler;
    PFNCLRETAINCOMMANDQUEUEPROC                     pCLRetainCommandQueue;
    PFNCLRETAINCONTEXTPROC                          pCLRetainContext;
    PFNCLRETAINEVENTPROC                            pCLRetainEvent;
    PFNCLRETAINKERNELPROC                           pCLRetainKernel;
    PFNCLRETAINMEMOBJECTPROC                        pCLRetainMemObject;
    PFNCLRETAINPROGRAMPROC                          pCLRetainProgram;
    PFNCLRETAINSAMPLERPROC                          pCLRetainSampler;
    PFNCLSETCOMMANDQUEUEPROPERTYPROC                pCLSetCommandQueueProperty;
    PFNCLSETEVENTCALLBACKPROC                       pCLSetEventCallback;
    PFNCLSETKERNELARGPROC                           pCLSetKernelArg;
    PFNCLSETMEMOBJECTDESTRUCTORCALLBACKPROC         pCLSetMemObjectDestructorCallback;
    PFNCLSETUSEREVENTSTATUSPROC                     pCLSetUserEventStatus;
    PFNCLUNLOADCOMPILERPROC                         pCLUnloadCompiler;
    PFNCLWAITFOREVENTSPROC                          pCLWaitForEvents;
} ocl_11_entrypoints;

typedef cl_mem (CL_API_CALL *PFNCLCREATEFROMGLBUFFERPROC)      (cl_context,       cl_mem_flags,       GLuint,        cl_int*);
typedef cl_mem (CL_API_CALL *PFNCLCREATEFROMGLRENDERBUFFERPROC)(cl_context,       cl_mem_flags,       GLuint,        cl_int*);
typedef cl_mem (CL_API_CALL *PFNCLCREATEFROMGLTEXTURE2DPROC)   (cl_context,       cl_mem_flags,       GLenum,        GLint,   GLuint,          cl_int*);
typedef cl_mem (CL_API_CALL *PFNCLCREATEFROMGLTEXTURE3DPROC)   (cl_context,       cl_mem_flags,       GLenum,        GLint,   GLuint,          cl_int*);
typedef cl_int (CL_API_CALL *PFNCLENQUEUEACQUIREGLOBJECTSPROC) (cl_command_queue, cl_uint,            const cl_mem*, cl_uint, const cl_event*, cl_event*);
typedef cl_int (CL_API_CALL *PFNCLENQUEUERELEASEGLOBJECTSPROC) (cl_command_queue, cl_uint,            const cl_mem*, cl_uint, const cl_event*, cl_event*);
typedef cl_int (CL_API_CALL *PFNCLGETGLOBJECTINFOPROC)         (cl_mem,           cl_gl_object_type*, GLuint*);
typedef cl_int (CL_API_CALL *PFNCLGETGLTEXTUREINFOPROC)        (cl_mem,           cl_gl_texture_info, size_t,        void*,   size_t*);

typedef struct
{
    PFNCLCREATEFROMGLBUFFERPROC       pCLCreateFromGLBuffer;
    PFNCLCREATEFROMGLRENDERBUFFERPROC pCLCreateFromGLRenderbuffer;
    PFNCLCREATEFROMGLTEXTURE2DPROC    pCLCreateFromGLTexture2D;
    PFNCLCREATEFROMGLTEXTURE3DPROC    pCLCreateFromGLTexture3D;
    PFNCLENQUEUEACQUIREGLOBJECTSPROC  pCLEnqueueAcquireGLObjects;
    PFNCLENQUEUERELEASEGLOBJECTSPROC  pCLEnqueueReleaseGLObjects;
    PFNCLGETGLOBJECTINFOPROC          pCLGetGLObjectInfo;
    PFNCLGETGLTEXTUREINFOPROC         pCLGetGLTextureInfo;

} ocl_khr_gl_sharing_entrypoints;

typedef struct
{
    cl_uint                  device_address_bits;
    cl_bool                  device_available;
    cl_bool                  device_compiler_available;
    cl_bool                  device_little_endian;
    cl_bool                  device_error_correction_support;
    bool                     device_kernel_execution_support;
    cl_bool                  device_native_kernel_execution_support;
    char*                    device_extensions;
    cl_ulong                 device_global_mem_cache_size;
    cl_device_mem_cache_type device_global_mem_cache_type;
    cl_uint                  device_global_mem_cacheline_size;
    cl_ulong                 device_global_mem_size;
    cl_bool                  device_host_unified_memory;
    cl_device_id             device_id;
    cl_bool                  device_image_support;
    size_t                   device_image2d_max_height;
    size_t                   device_image2d_max_width;
    size_t                   device_image3d_max_depth;
    size_t                   device_image3d_max_height;
    size_t                   device_image3d_max_width;
    cl_ulong                 device_local_mem_size;
    cl_device_local_mem_type device_local_mem_type;
    cl_uint                  device_max_clock_frequency;
    cl_uint                  device_max_compute_units;
    cl_uint                  device_max_constant_args;
    cl_ulong                 device_max_constant_buffer_size;
    cl_ulong                 device_max_mem_alloc_size;
    size_t                   device_max_parameter_size;
    cl_uint                  device_max_read_image_args;
    cl_uint                  device_max_samplers;
    size_t                   device_max_work_group_size;
    cl_uint                  device_max_work_item_dimensions;
    size_t*                  device_max_work_item_sizes; /* has device_max_work_item_dimensions size */
    cl_uint                  device_max_write_image_args;
    cl_uint                  device_mem_base_addr_align;
    char*                    device_name;
    cl_uint                  device_native_vector_width_char;
    cl_uint                  device_native_vector_width_short;
    cl_uint                  device_native_vector_width_int;
    cl_uint                  device_native_vector_width_long;
    cl_uint                  device_native_vector_width_float;
    cl_uint                  device_native_vector_width_double;
    cl_uint                  device_native_vector_width_half;
    char*                    device_opencl_c_version;
    cl_uint                  device_preferred_vector_width_char;
    cl_uint                  device_preferred_vector_width_short;
    cl_uint                  device_preferred_vector_width_int;
    cl_uint                  device_preferred_vector_width_long;
    cl_uint                  device_preferred_vector_width_float;
    cl_uint                  device_preferred_vector_width_double;
    cl_uint                  device_preferred_vector_width_half;
    char*                    device_profile;
    size_t                   device_profiling_timer_resolution;
    bool                     device_out_of_order_exec_mode_enable_supported;
    bool                     device_queue_profiling_enable_supported;
    bool                     device_single_fp_denorms_supported;
    bool                     device_single_fp_inf_nans_supported;
    bool                     device_single_fp_round_to_inf_supported;
    bool                     device_single_fp_round_to_nearest_supported;
    bool                     device_single_fp_round_to_zero_supported;
    bool                     device_single_fp_fused_multiply_add_supported;
    bool                     device_single_soft_float;
    cl_device_type           device_type;
    char*                    device_vendor;
    cl_uint                  device_vendor_id;
    char*                    device_version;
    char*                    driver_version;
} ocl_device_info;

typedef struct
{
    char*          platform_extensions;
    cl_platform_id platform_id;
    char*          platform_name;
    char*          platform_profile;
    char*          platform_vendor;
    char*          platform_version;
} ocl_platform_info;

#endif /* OCL_TYPES_H */
