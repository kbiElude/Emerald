/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kernel.h"
#include "ocl/ocl_program.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definitions */
typedef struct
{
    ocl_context             context;
    system_resizable_vector kernels; /* contains ocl_kernel instances */
    cl_program              program;

    system_hash64map        name_to_kernel_map; /* system_hashed_ansi_string to ocl_kernel */

    REFCOUNT_INSERT_VARIABLES
} _ocl_program;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ocl_program, ocl_program, _ocl_program);

/** TODO */
PRIVATE system_hash64 _ocl_program_calculate_source_code_hash(__in __notnull const char** strings,
                                                              __in           uint32_t     n_strings)
{
    system_hash64 result = 0;

    for (uint32_t n = 0; n < n_strings; ++n)
    {
        system_hashed_ansi_string source_code = system_hashed_ansi_string_create(strings[n]);

        result += system_hashed_ansi_string_get_hash(source_code);
    }

    return result;
}

/** TODO */
PRIVATE void _ocl_program_deinit(__in __notnull _ocl_program* program_ptr)
{
    const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();

    if (program_ptr != NULL)
    {
        if (program_ptr->context != NULL)
        {
            ocl_context_release(program_ptr->context);
        }

        if (program_ptr->kernels != NULL)
        {
            ocl_kernel kernel = NULL;

            while (system_resizable_vector_pop(program_ptr->kernels, &kernel) )
            {
                ocl_kernel_release(kernel);
            }

            system_resizable_vector_release(program_ptr->kernels);
        }

        if (program_ptr->program != NULL)
        {
            entry_points->pCLReleaseProgram(program_ptr->program);

            program_ptr->program = NULL;
        }

        if (program_ptr->name_to_kernel_map != NULL)
        {
            system_hash64map_release(program_ptr->name_to_kernel_map);

            program_ptr->name_to_kernel_map = NULL;
        }
    }
}

/** TODO */
PRIVATE void _ocl_program_init(_ocl_program* program_ptr)
{
    program_ptr->context            = NULL;
    program_ptr->name_to_kernel_map = system_hash64map_create(sizeof(ocl_kernel) );
    program_ptr->program            = NULL;
}

/** TODO */
PRIVATE bool _ocl_program_load_program_binary_blobs(__in                           __notnull _ocl_program*             program_ptr,
                                                    __in                           __notnull ocl_context               context,
                                                    __in                           __notnull system_hashed_ansi_string name,
                                                    __in                                     system_hash64             source_code_hash,
                                                    __in                                     uint32_t                  context_n_devices,
                                                    __in_ecount(context_n_devices) __notnull cl_device_id*             context_devices)
{
    cl_platform_id         platform_id = ocl_context_get_platform_id(context);
    bool                   result      = false;
    system_file_serializer serializer  = system_file_serializer_create_for_reading(name);

    ASSERT_DEBUG_SYNC(serializer != NULL, "Could not spawn file serializer");
    if (serializer != NULL)
    {
        uint32_t      blob_n_devices        = 0;
        system_hash64 blob_source_code_hash = 0;
        bool          temp_result           = false;

        temp_result  = system_file_serializer_read(serializer, sizeof(blob_n_devices),        &blob_n_devices);
        temp_result &= system_file_serializer_read(serializer, sizeof(blob_source_code_hash), &blob_source_code_hash);

        ASSERT_DEBUG_SYNC(temp_result, "Read operation failed");
        if (temp_result)
        {
            if (blob_n_devices != context_n_devices)
            {
                LOG_ERROR("Blob file for CL program [%s] was saved for [%d] devices, whereas current platform uses [%d] devices. Will recompile",
                          system_hashed_ansi_string_get_buffer(name),
                          blob_n_devices,
                          context_n_devices);

                goto end;
            }
            else
            if (source_code_hash != blob_source_code_hash)
            {
                LOG_ERROR("Blob file for CL program [%s] uses hash [%lld] whereas original source code uses hash [%lld]. Will recompile",
                          system_hashed_ansi_string_get_buffer(name),
                          blob_source_code_hash,
                          source_code_hash);

                goto end;
            }
            else
            {
                cl_device_id* blob_devices = new (std::nothrow) cl_device_id[blob_n_devices];
                uint32_t*     blob_sizes   = new (std::nothrow) uint32_t    [blob_n_devices];
                char**        blobs        = new (std::nothrow) char*       [blob_n_devices];

                ASSERT_DEBUG_SYNC(blob_devices != NULL && blob_sizes != NULL && blobs != NULL, "Out of memory");
                if (blob_devices == NULL || blob_sizes == NULL || blobs == NULL)
                {
                    goto end;
                }

                for (uint32_t n_device = 0; n_device < blob_n_devices; ++n_device)
                {
                    system_hashed_ansi_string blob_device_name = NULL;

                    temp_result  = system_file_serializer_read_hashed_ansi_string(serializer, &blob_device_name);
                    temp_result &= system_file_serializer_read                   (serializer, sizeof(uint32_t), blob_sizes + n_device);

                    ASSERT_DEBUG_SYNC(temp_result, "Read operation failed");
                    if (temp_result)
                    {
                        /* A device name is cool but we need a device id instead */
                        if (!ocl_get_device_id_by_name(platform_id, blob_device_name, blob_devices + n_device) )
                        {
                            ASSERT_DEBUG_SYNC(false, "Could not find device id for device name [%s]. Will recompile",
                                              system_hashed_ansi_string_get_buffer(blob_device_name) );

                            goto end;
                        }

                        /* Carry on */
                        blobs[n_device] = new (std::nothrow) char[ blob_sizes[n_device] ];

                        ASSERT_DEBUG_SYNC(blobs[n_device] != NULL, "Out of memory");
                        if (blobs[n_device] != NULL)
                        {
                            temp_result = system_file_serializer_read(serializer, blob_sizes[n_device], blobs[n_device]);
                            
                            ASSERT_DEBUG_SYNC(temp_result, "Read operation failed");
                        }
                        else
                        {
                            goto end;
                        }
                    }
                    else
                    {
                        goto end;
                    }
                }

                /* Create CL program, using the blobs we have */
                const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();
                cl_int                    error_code   = 0;

                program_ptr->program = entry_points->pCLCreateProgramWithBinary(ocl_context_get_context(program_ptr->context),
                                                                                blob_n_devices,
                                                                                blob_devices,
                                                                                blob_sizes,
                                                                                (const unsigned char**) blobs,
                                                                                NULL,         /* we're not really intesreted in binary status */
                                                                                &error_code); /* error code might come in handy though */

                ASSERT_DEBUG_SYNC(program_ptr->program != NULL && error_code == CL_SUCCESS,
                                  "Could not load blob file for OCL program [%s]: error code [%d]",
                                  system_hashed_ansi_string_get_buffer(name),
                                  error_code);
                if (program_ptr->program == NULL || error_code != CL_SUCCESS)
                {
                    goto end;
                }

                /* That's it :) Clean up */
                for (uint32_t n_device = 0; n_device < blob_n_devices; ++n_device)
                {
                    delete [] blobs[n_device];
                }

                delete [] blobs;
                delete [] blob_devices;
                delete [] blob_sizes;

                result = true;
            }
        }
    }

end:
    if (serializer != NULL)
    {
        system_file_serializer_release(serializer);
    }

    return result;
}

/** TODO */
PRIVATE void _ocl_program_release(void* arg)
{
    _ocl_program* program_ptr = (_ocl_program*) arg;

    _ocl_program_deinit(program_ptr);
}

/** TODO */
PRIVATE void _ocl_program_save_program_binary_blobs(__in                           __notnull system_hashed_ansi_string name,
                                                    __in                                     system_hash64             source_code_hash,
                                                    __in                           __notnull ocl_context               context,
                                                    __in                                     uint32_t                  context_n_devices,
                                                    __in_ecount(context_n_devices) __notnull cl_device_id*             context_devices,
                                                    __in_ecount(context_n_devices) __notnull char**                    program_binaries,
                                                    __in_ecount(context_n_devices) __notnull uint32_t*                 program_binary_sizes)
{
    cl_platform_id         platform_id = ocl_context_get_platform_id(context);
    system_file_serializer serializer  = system_file_serializer_create_for_writing(name);

    ASSERT_DEBUG_SYNC(serializer != NULL, "Could not spawn file serializer");
    if (serializer != NULL)
    {
        bool result = system_file_serializer_write(serializer, sizeof(context_n_devices), &context_n_devices);

        if (result)
        {
            result = system_file_serializer_write(serializer, sizeof(source_code_hash), &source_code_hash);
        }

        if (result)
        {
            for (uint32_t n_device = 0; n_device < context_n_devices; ++n_device)
            {
                const ocl_device_info* device_info_ptr = ocl_get_device_info(platform_id, context_devices[n_device]);

                ASSERT_DEBUG_SYNC(device_info_ptr != NULL, "Could not retrieve device info descriptor");
                if (device_info_ptr != NULL)
                {
                    result = system_file_serializer_write_hashed_ansi_string(serializer, system_hashed_ansi_string_create(device_info_ptr->device_name) );

                    if (result)
                    {
                        result = system_file_serializer_write(serializer, sizeof(uint32_t), program_binary_sizes + n_device);
                    }

                    if (result)
                    {
                        result = system_file_serializer_write(serializer, program_binary_sizes[n_device], program_binaries[n_device]);
                    }
                }
            }
        }

        ASSERT_ALWAYS_SYNC(result, "CL program binary blob serialization failed");

        /* That's it */
        system_file_serializer_release(serializer);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API ocl_program ocl_program_create_from_source(__in __notnull                     ocl_context               context,
                                                              __in                               uint32_t                  n_strings,
                                                              __in_ecount(n_strings) __notnull   const char**              strings,
                                                              __in_ecount(n_strings) __maybenull const size_t*             string_lengths,
                                                              __in                   __notnull   system_hashed_ansi_string name)
{
    const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();
    _ocl_program*             new_instance = new (std::nothrow) _ocl_program;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        cl_int build_error_code = -1;
        cl_int error_code       = -1;

        /* Init the instance's fields */
        _ocl_program_init(new_instance);

        new_instance->context = context;
        ocl_context_retain(context);

        /* Retrieve CL devices */
        cl_device_id* context_devices   = NULL;
        uint32_t      context_n_devices = 0;

        ocl_context_get_assigned_devices(new_instance->context, 
                                         &context_devices,
                                         &context_n_devices);

        /* IF there already is a blob available, try using it. If not - well then, we've got to use the source code! */
        FILE* blob_file_handle    = ::fopen(system_hashed_ansi_string_get_buffer(name), "rb");
        bool  was_program_created = false;

        if (blob_file_handle != NULL)
        {
            fclose(blob_file_handle);

            /* Try reading the blob then */
            was_program_created = _ocl_program_load_program_binary_blobs(new_instance,
                                                                         context,
                                                                         name,
                                                                         _ocl_program_calculate_source_code_hash(strings, n_strings),
                                                                         context_n_devices,
                                                                         context_devices);
        }

        if (!was_program_created)
        {
            new_instance->program = entry_points->pCLCreateProgramWithSource(ocl_context_get_context(context),
                                                                             n_strings,
                                                                             strings,
                                                                             string_lengths,
                                                                             &error_code);

            if (new_instance->program == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "CL program [%s] creation failed - error code [%d] was reported", 
                                  system_hashed_ansi_string_get_buffer(name),
                                  error_code);

                goto end_with_fail;
            }
        }

        /* Build the program */
        build_error_code = entry_points->pCLBuildProgram(new_instance->program,
                                                         NULL, /* use context devices */
                                                         NULL, /* use context devices */
                                                         "-w", /* include warnings */
                                                         NULL, /* build notification not needed */
                                                         NULL  /* build notification not needed */);

        /* No matter what the result is, try to retrieve build info log */        
        for (uint32_t n = 0; n < context_n_devices; ++n)
        {
            char*    log_ptr        = NULL;
            uint32_t n_bytes_needed = 0;

            error_code = entry_points->pCLGetProgramBuildInfo(new_instance->program,
                                                              context_devices[n],
                                                              CL_PROGRAM_BUILD_LOG,
                                                              NULL,
                                                              NULL,
                                                              &n_bytes_needed);

            if (error_code != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve build log for program [%s] and device [%p] due to error [%d]",
                                  system_hashed_ansi_string_get_buffer(name),
                                  context_devices[n],
                                  error_code);

                goto end_with_fail;
            }

            log_ptr = new (std::nothrow) char[n_bytes_needed + 1];

            if (log_ptr == NULL)
            {
                ASSERT_ALWAYS_SYNC(false, "Out of memory");

                goto end_with_fail;
            }

            memset(log_ptr, 0, n_bytes_needed+1);

            error_code = entry_points->pCLGetProgramBuildInfo(new_instance->program,
                                                              context_devices[n],
                                                              CL_PROGRAM_BUILD_LOG,
                                                              n_bytes_needed + 1,
                                                              (void*) log_ptr,
                                                              NULL);

            if (error_code != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve build log for program [%s] and device [%p] due to error [%d] (2)",
                                  system_hashed_ansi_string_get_buffer(name),
                                  context_devices[n],
                                  error_code);

                goto end_with_fail;
            }

            /* Log the build log */
            LOG_INFO("OpenCL build log for program [%s] and device [%p]: \n>>\n%s\n<<",
                     system_hashed_ansi_string_get_buffer(name),
                     context_devices[n],
                     log_ptr);

            /* Release it - we won't be needing it any more */
            delete [] log_ptr;
            log_ptr = NULL;
        }

        if (build_error_code != CL_SUCCESS)
        {
            ASSERT_DEBUG_SYNC(false,
                              "CL program [%s] failed to build - error code [%d] was reported",
                              system_hashed_ansi_string_get_buffer(name),
                              error_code);

            goto end_with_fail;
        }

        /* Retrieve program binaries */
        size_t* program_binary_sizes = new (std::nothrow) size_t[context_n_devices];
        char**  program_binaries     = new (std::nothrow) char* [context_n_devices];

        ASSERT_DEBUG_SYNC(program_binary_sizes != NULL && program_binaries != NULL, "Out of memory");
        if (program_binary_sizes == NULL || program_binaries == NULL)
        {
            goto end_with_fail;
        }

        error_code = entry_points->pCLGetProgramInfo(new_instance->program,
                                                     CL_PROGRAM_BINARY_SIZES,
                                                     sizeof(size_t) * context_n_devices,
                                                     program_binary_sizes,
                                                     NULL);

        ASSERT_DEBUG_SYNC(error_code == CL_SUCCESS, "Could not retrieve program binary sizes information!");
        if (error_code != CL_SUCCESS)
        {
            goto end_with_fail;
        }

        for (uint32_t n = 0; n < context_n_devices; ++n)
        {
            program_binaries[n] = new (std::nothrow) char[ program_binary_sizes[n] ];

            ASSERT_DEBUG_SYNC(program_binaries[n] != NULL, "Out of memory");
            if (program_binaries[n] == NULL)
            {
                goto end_with_fail;
            }
        }

        /* Retrieve the program binaries */
        error_code = entry_points->pCLGetProgramInfo(new_instance->program,
                                                     CL_PROGRAM_BINARIES,
                                                     sizeof(char*) * context_n_devices,
                                                     program_binaries,
                                                     NULL);

        ASSERT_DEBUG_SYNC(error_code == CL_SUCCESS, "Could not retrieve program binary");
        if (error_code != CL_SUCCESS)
        {
            goto end_with_fail;
        }

        /* Write them down to a file */
        if (!was_program_created)
        {
            _ocl_program_save_program_binary_blobs(name,
                                                   _ocl_program_calculate_source_code_hash(strings, n_strings),
                                                   context,
                                                   context_n_devices,
                                                   context_devices,
                                                   program_binaries,
                                                   program_binary_sizes);
        }

        /* Clean up before we continue */
        if (program_binary_sizes != NULL)
        {
            delete [] program_binary_sizes;

            program_binary_sizes = NULL;
        }

        if (program_binaries != NULL)
        {
            for (uint32_t n = 0; n < context_n_devices; ++n)
            {
                if (program_binaries[n] != NULL)
                {
                    delete [] program_binaries[n];
                }
            }

            delete [] program_binaries;
            program_binaries = NULL;
        }

        /* The program has been built successfully so we may now proceed with instatiating kernel representations */
        cl_kernel* kernels   = NULL;
        cl_uint    n_kernels = 0;

        error_code = entry_points->pCLCreateKernelsInProgram(new_instance->program,
                                                             NULL,
                                                             NULL,
                                                             &n_kernels);

        if (error_code != CL_SUCCESS)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not query number of program kernels for program [%s]",
                              system_hashed_ansi_string_get_buffer(name) );

            goto end_with_fail;
        }

        new_instance->kernels = system_resizable_vector_create(n_kernels, sizeof(ocl_kernel) );
        kernels               = new (std::nothrow) cl_kernel[n_kernels];

        if (new_instance->kernels == NULL || kernels == NULL)
        {
            ASSERT_ALWAYS_SYNC(false, "Out of memory");

            if (kernels != NULL)
            {
                delete [] kernels;

                kernels = NULL;
            }

            goto end_with_fail;
        }

        /* Fill the kernels array */
        error_code = entry_points->pCLCreateKernelsInProgram(new_instance->program,
                                                             n_kernels,
                                                             kernels,
                                                             NULL);

        if (error_code != CL_SUCCESS)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve kernels for program [%s]",
                              system_hashed_ansi_string_get_buffer(name) );

            goto end_with_fail;
        }

        /* Register the instance - it's not the most elegant place to insert this,
         * but kernel instances will try to retain their owner and they will fail hard
         * if refcounting is not initialized by then.
         */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _ocl_program_release,
                                                       OBJECT_TYPE_OCL_PROGRAM, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenCL Programs\\", system_hashed_ansi_string_get_buffer(name) )
                                                       );

        /* Create kernel instances and store them */
        uint32_t name_length = system_hashed_ansi_string_get_length(name);
        
        for (uint32_t n = 0; n < n_kernels; ++n)
        {
            /* Retrieve kernel name */
            char*    kernel_name        = NULL;
            uint32_t kernel_name_length = 0;

            error_code = entry_points->pCLGetKernelInfo(kernels[n],
                                                        CL_KERNEL_FUNCTION_NAME,
                                                        NULL,
                                                        NULL,
                                                        &kernel_name_length);
            if (error_code != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve kernel function name length for program [%s]",
                                  system_hashed_ansi_string_get_buffer(name) );

                goto end_with_fail;
            }

            kernel_name = new (std::nothrow) char[name_length + 1 + kernel_name_length + 1];

            if (kernel_name == NULL)
            {
                ASSERT_ALWAYS_SYNC(false, "Out of memory");

                goto end_with_fail;
            }

            memcpy(kernel_name, system_hashed_ansi_string_get_buffer(name), name_length);
            kernel_name[name_length] = ' ';

            error_code = entry_points->pCLGetKernelInfo(kernels[n],
                                                        CL_KERNEL_FUNCTION_NAME,
                                                        kernel_name_length+1,
                                                        kernel_name + name_length + 1,
                                                        NULL);
            if (error_code != CL_SUCCESS)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve kernel function name for program [%s]",
                                  system_hashed_ansi_string_get_buffer(name) );

                goto end_with_fail;
            }

            /* Instantiate new kernel representation */
            ocl_kernel kernel = ocl_kernel_create((ocl_program) new_instance,
                                                  context_n_devices,
                                                  context_devices,
                                                  kernels[n],
                                                  system_hashed_ansi_string_create(kernel_name) );
                              
            if (kernel == NULL)
            {
                ASSERT_DEBUG_SYNC(kernel != NULL,
                                  "Could not create kernel [%s] instance for program [%s]",
                                  kernel_name,
                                  system_hashed_ansi_string_get_buffer(name) );

                goto end_with_fail;
            }
            
            /* Stash it */
            system_resizable_vector_push(new_instance->kernels, kernel);
            system_hash64map_insert     (new_instance->name_to_kernel_map, system_hash64(system_hashed_ansi_string_create(kernel_name + name_length + 1) ), kernel, NULL, NULL);

            /* Good to release kernel name at this point */
            delete [] kernel_name;
        }

        /* Clean up */
        if (kernels != NULL)
        {
            delete [] kernels;

            kernels = NULL;
        }
    }

    return (ocl_program) new_instance;

end_with_fail:
    if (new_instance != NULL)
    {
        _ocl_program_deinit(new_instance);
            
        delete new_instance;
        new_instance = NULL;
    }

    return NULL;
}

/* Please see header for specification */
PUBLIC EMERALD_API ocl_kernel ocl_program_get_kernel_by_name(__in __notnull ocl_program instance, __in __notnull system_hashed_ansi_string name)
{
    _ocl_program* program_ptr = (_ocl_program*) instance;
    ocl_kernel    result      = NULL;

    system_hash64map_get(program_ptr->name_to_kernel_map, system_hash64(name), &result);

    return result;
}