/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kernel.h"
#include "ocl/ocl_program.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    cl_kernel                 kernel;
    system_hashed_ansi_string name;
    cl_uint                   n_kernel_arguments;
    ocl_program               owner_program;
    uint32_t                  work_group_size;
    REFCOUNT_INSERT_VARIABLES
} _ocl_kernel;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ocl_kernel, ocl_kernel, _ocl_kernel);

/** TODO */
PRIVATE void _ocl_kernel_deinit(_ocl_kernel* kernel_ptr)
{
    const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();

    if (kernel_ptr->kernel != NULL)
    {
        entry_points->pCLReleaseKernel(kernel_ptr->kernel);
        
        kernel_ptr->kernel = NULL;
    }
}

/** TODO */
PRIVATE void _ocl_kernel_init(_ocl_kernel* kernel_ptr)
{
    kernel_ptr->kernel        = 0;
    kernel_ptr->owner_program = NULL;
}


/** TODO */
PRIVATE void _ocl_kernel_release(void* arg)
{
    _ocl_kernel* kernel_ptr = (_ocl_kernel*) arg;

    _ocl_kernel_deinit(kernel_ptr);
}


/* Please see header for specification */
PUBLIC EMERALD_API ocl_kernel ocl_kernel_create(__in                   __notnull ocl_program               owner,
                                                __in                             uint32_t                  n_devices,
                                                __in_ecount(n_devices) __notnull cl_device_id*             devices,
                                                __in                   __notnull cl_kernel                 kernel,
                                                __in                   __notnull system_hashed_ansi_string name)
{
    const ocl_11_entrypoints* ocl_entrypoints = ocl_get_entrypoints();
    _ocl_kernel*              new_instance    = new (std::nothrow) _ocl_kernel;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        new_instance->kernel          = kernel;
        new_instance->name            = name;
        new_instance->owner_program   = owner;
        new_instance->work_group_size = INT_MAX;

        /* Retrieve number of recognized arguments */
        cl_int error_code = ocl_entrypoints->pCLGetKernelInfo(kernel,
                                                              CL_KERNEL_NUM_ARGS,
                                                              sizeof(new_instance->n_kernel_arguments),
                                                              &new_instance->n_kernel_arguments,
                                                              NULL);
        ASSERT_DEBUG_SYNC(error_code == CL_SUCCESS, "Could not retrieve number of kernel arguments");

        /* Calculate minimum work group size allowed */
        uint32_t device_work_group_size = 0;

        for (uint32_t n = 0; n < n_devices; ++n)
        {
            error_code = ocl_entrypoints->pCLGetKernelWorkGroupInfo(kernel,
                                                                    devices[n],
                                                                    CL_KERNEL_WORK_GROUP_SIZE,
                                                                    sizeof(new_instance->work_group_size),
                                                                    &device_work_group_size,
                                                                    NULL);
            ASSERT_DEBUG_SYNC(error_code == CL_SUCCESS, "Could not retrieve kernel work group size");

            if (device_work_group_size < new_instance->work_group_size)
            {
                new_instance->work_group_size = device_work_group_size;
            }
        }

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _ocl_kernel_release,
                                                       OBJECT_TYPE_OCL_KERNEL, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenCL Kernels\\", system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }

    return (ocl_kernel) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API cl_kernel ocl_kernel_get_kernel(__in __notnull ocl_kernel instance)
{
    return ((_ocl_kernel*) instance)->kernel;
}

/* Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ocl_kernel_get_name(__in __notnull ocl_kernel instance)
{
    return ((_ocl_kernel*) instance)->name; 
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t ocl_kernel_get_work_group_size(__in __notnull ocl_kernel instance)
{
    return ((_ocl_kernel*) instance)->work_group_size;
}