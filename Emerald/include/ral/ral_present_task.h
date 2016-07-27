#ifndef RAL_PRESENT_TASK_H
#define RAL_PRESENT_TASK_H

#include "ral/ral_context.h"
#include "ral/ral_types.h"

typedef struct
{
    ral_context_object_type object_type;

    union
    {
        ral_buffer       buffer;
        void*            object;
        ral_texture      texture;
        ral_texture_view texture_view;
    };
} ral_present_task_io;

typedef enum
{
    RAL_PRESENT_TASK_IO_TYPE_FIRST,

    /* Read data access */
    RAL_PRESENT_TASK_IO_TYPE_INPUT = RAL_PRESENT_TASK_IO_TYPE_FIRST,

    /* Write data access */
    RAL_PRESENT_TASK_IO_TYPE_OUTPUT,

    /* Always last */
    RAL_PRESENT_TASK_IO_TYPE_UNKNOWN,
    RAL_PRESENT_TASK_IO_TYPE_COUNT = RAL_PRESENT_TASK_IO_TYPE_UNKNOWN
} ral_present_task_io_type;

typedef struct
{
    /* NOTE: Unique input objects may be set to null if input object should be extracted during run-time */
    PFNRALPRESENTTASKCPUCALLBACKPROC pfn_cpu_task_callback_proc;
    void*                            cpu_task_callback_user_arg;

    uint32_t                         n_unique_inputs;
    uint32_t                         n_unique_outputs;
    const ral_present_task_io*       unique_inputs;
    const ral_present_task_io*       unique_outputs;

} ral_present_task_cpu_create_info;

typedef struct
{
    /* NOTE: May be null, in which case the task is a nop. */
    ral_command_buffer         command_buffer;

    /* NOTE: Unique input objects may be set to null if input object should be extracted during run-time */
    uint32_t                   n_unique_inputs;
    uint32_t                   n_unique_outputs;
    const ral_present_task_io* unique_inputs;
    const ral_present_task_io* unique_outputs;

} ral_present_task_gpu_create_info;

typedef struct
{
    uint32_t group_task_io_index;
    uint32_t n_present_task;
    uint32_t present_task_io_index;

} ral_present_task_group_mapping;

typedef struct
{
    uint32_t input_present_task_index; /* index to the present_tasks[] array */
    uint32_t input_present_task_io_index;
    uint32_t output_present_task_index; /* index to the present_tasks[] array */
    uint32_t output_present_task_io_index;

} ral_present_task_ingroup_connection;

typedef struct
{
    uint32_t          n_present_tasks;
    ral_present_task* present_tasks;

    uint32_t                                   n_ingroup_connections;
    const ral_present_task_ingroup_connection* ingroup_connections;

    /* NOTE: One unique input can map to multiple present task inputs
     *
     *       If more than one source is specified, all such inputs need to be assigned
     *       (directly or indirectly) the same ral_texture instance.
     */
    uint32_t                              n_total_unique_inputs;
    uint32_t                              n_unique_input_to_ingroup_task_mappings;
    const ral_present_task_group_mapping* unique_input_to_ingroup_task_mapping;

    /* NOTE: One unique output can take multiple present task outputs, if more than one
     *       present task contributes to the result.
     *
     *       If more than one output is specified, all such outputs need to refer (directly
     *       or indirectly) to the same ral_texture instance.
     */
    uint32_t                              n_total_unique_outputs;
    uint32_t                              n_unique_output_to_ingroup_task_mappings;
    const ral_present_task_group_mapping* unique_output_to_ingroup_task_mapping;

} ral_present_task_group_create_info;

typedef enum
{
    /* not settable; void* (depends on the reported OBJECT_TYPE value) */
    RAL_PRESENT_TASK_IO_PROPERTY_OBJECT,

    /* not settable; ral_context_object_type */
    RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
} ral_present_task_io_property;

typedef enum
{
    /* not settable; ral_command_buffer
     *
     * Only available for GPU tasks.
     **/
    RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_INPUTS,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS,

    /* not settable; system_hashed_ansi_string */
    RAL_PRESENT_TASK_PROPERTY_NAME,


    /* not settable; ral_present_task_type */
    RAL_PRESENT_TASK_PROPERTY_TYPE
} ral_present_task_property;

typedef enum
{
    RAL_PRESENT_TASK_TYPE_CPU_TASK,
    RAL_PRESENT_TASK_TYPE_GPU_TASK,

    /* This task is a bag consisting of:
     *
     * - present tasks
     * - description of IO connection between these tasks.
     *
     * plus the usual unique input/output connections.
     *
     */
    RAL_PRESENT_TASK_TYPE_GROUP,
} ral_present_task_type;

/** TODO */
PUBLIC ral_present_task ral_present_task_create_cpu(system_hashed_ansi_string               name,
                                                    const ral_present_task_cpu_create_info* create_info_ptr);

/** TODO
 *
 *  Specified ral_present_tasks are retained.
 */
PUBLIC ral_present_task ral_present_task_create_group(const ral_present_task_group_create_info* create_info_ptr);

/** Wraps a RAL command buffer inside a RAL present task. The command buffer is processed
 *  and information about read & modified RAL objects is extracted & cached.
 *
 *  NOTE: Support for resettable command buffers is TODO.
 *
 ***/
PUBLIC ral_present_task ral_present_task_create_gpu(system_hashed_ansi_string               name,
                                                    const ral_present_task_gpu_create_info* create_info_ptr);

/** TODO */
PUBLIC bool ral_present_task_get_io_property(ral_present_task             task,
                                             ral_present_task_io_type     io_type,
                                             uint32_t                     n_io,
                                             ral_present_task_io_property property,
                                             void**                       out_result_ptr);

/** TODO */
PUBLIC void ral_present_task_get_property(ral_present_task          task,
                                          ral_present_task_property property,
                                          void*                     out_result_ptr);

/** TODO */
PUBLIC void ral_present_task_release(ral_present_task task);

/** TODO */
PUBLIC void ral_present_task_retain(ral_present_task task);


#endif /* RAL_PRESENT_TASK_H */