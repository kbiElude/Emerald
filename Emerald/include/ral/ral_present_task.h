#ifndef RAL_PRESENT_TASK_H
#define RAL_PRESENT_TASK_H

#include "ral/ral_context.h"
#include "ral/ral_types.h"


typedef enum
{
    /* Read data access */
    RAL_PRESENT_TASK_ACCESS_TYPE_READ,

    /* Write data access */
    RAL_PRESENT_TASK_ACCESS_TYPE_WRITE,
} ral_present_task_access_type;

typedef enum
{
    /* not settable; ral_command_buffer */
    RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_MODIFIED_BUFFERS,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_MODIFIED_TEXTURES,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_READ_BUFFERS,

    /* not settable; uint32_t */
    RAL_PRESENT_TASK_PROPERTY_N_READ_TEXTURES,

} ral_present_task_property;


/** TODO
 *
 *  NOTE: Support for resettable command buffers is TODO.
 ***/
ral_present_task ral_present_task_create(system_hashed_ansi_string name,
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