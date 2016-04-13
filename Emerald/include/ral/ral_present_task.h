#ifndef RAL_PRESENT_TASK_H
#define RAL_PRESENT_TASK_H

#include "ral/ral_context.h"
#include "ral/ral_types.h"

typedef void (*PFNRALPRESENTTASKCPUCALLBACKPROC)(void* user_arg);

typedef enum
{
    /* Read data access */
    RAL_PRESENT_TASK_ACCESS_TYPE_READ,

    /* Write data access */
    RAL_PRESENT_TASK_ACCESS_TYPE_WRITE,
} ral_present_task_access_type;

typedef struct
{
    PFNRALPRESENTTASKCPUCALLBACKPROC pfn_cpu_task_callback_proc;
    void*                            cpu_task_callback_user_arg;

    uint32_t                         n_modified_buffers;
    uint32_t                         n_modified_textures;
    uint32_t                         n_read_buffers;
    uint32_t                         n_read_textures;

    ral_buffer*  modified_buffers;
    ral_texture* modified_textures;
    ral_buffer*  read_buffers;
    ral_texture* read_textures;

} ral_present_task_cpu_create_info;

typedef enum
{
    /* not settable; ral_command_buffer
     *
     * Only available for GPU tasks.
     **/
    RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_MODIFIED_BUFFERS,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_MODIFIED_TEXTURES,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_READ_BUFFERS,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_READ_TEXTURES,

    /* not settable; system_hashed_ansi_string */
    RAL_PRESENT_TASK_PROPERTY_NAME,

    /* not settable; ral_present_task_type */
    RAL_PRESENT_TASK_PROPERTY_TYPE
} ral_present_task_property;

typedef enum
{
    RAL_PRESENT_TASK_TYPE_CPU_TASK,
    RAL_PRESENT_TASK_TYPE_GPU_TASK
} ral_present_task_type;


/** Wraps a RAL command buffer inside a RAL present task. The command buffer is processed
 *  and information about read & modified RAL objects is extracted & cached.
 *
 *  NOTE: Support for resettable command buffers is TODO.
 *
 ***/
ral_present_task ral_present_task_create_gpu(system_hashed_ansi_string name,
                                             ral_command_buffer        command_buffer);

/** TODO */
bool ral_present_task_get_modified_object(ral_present_task             task,
                                          ral_context_object_type      object_type,
                                          ral_present_task_access_type access_type,
                                          uint32_t                     n_object,
                                          void**                       out_object_ptr);

/** TODO */
void ral_present_task_get_property(ral_present_task          task,
                                   ral_present_task_property property,
                                   void*                     out_result_ptr);

/** TODO */
void ral_present_task_release(ral_present_task task);


#endif /* RAL_PRESENT_TASK_H */