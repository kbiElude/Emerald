#ifndef RAL_PRESENT_JOB_H
#define RAL_PRESENT_JOB_H

#include "ral/ral_types.h"

typedef enum
{
    /* not settable; ral_present_task_id */
    RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_ID,

    /* not settable; uint32_t */
    RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_INPUT_INDEX,

    /* not settable; ral_present_task_id */
    RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_ID,

    /* not settable; uint32_t */
    RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_OUTPUT_INDEX
} ral_present_job_connection_property;

typedef enum
{
    /* set to true with first ral_present_job_set_presentable_output() invocation; bool */
    RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED,

    /* settable with ral_present_job_set_presentable_output(); ral_present_task_id */
    RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID,

    /* settable with ral_present_job_set_presentable_output(); uint32_t */
    RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_OUTPUT_INDEX,
} ral_present_job_property;

/** TODO */
PUBLIC bool ral_present_job_add_task(ral_present_job      job,
                                     ral_present_task     task,
                                     ral_present_task_id* out_task_id_ptr);

/** TODO */
PUBLIC bool ral_present_job_connect_tasks(ral_present_job                job,
                                          ral_present_task_id            src_task_id,
                                          uint32_t                       n_src_task_output,
                                          ral_present_task_id            dst_task_id,
                                          uint32_t                       n_dst_task_input,
                                          ral_present_job_connection_id* out_connection_id_ptr);

/** TODO */
PUBLIC ral_present_job ral_present_job_create();

/** TODO */
PUBLIC bool ral_present_job_get_connection_property(ral_present_job                     job,
                                                    ral_present_job_connection_id       connection_id,
                                                    ral_present_job_connection_property property,
                                                    void*                               out_result_ptr);

/** TODO */
PUBLIC bool ral_present_job_get_connection_id_at_index(ral_present_job                job,
                                                       uint32_t                       n_connection,
                                                       ral_present_job_connection_id* out_result_ptr);

/** TODO */
PUBLIC void ral_present_job_get_property(ral_present_job          job,
                                         ral_present_job_property property,
                                         void*                    out_result_ptr);

/** TODO */
PUBLIC void ral_present_job_release(ral_present_job job);

/** TODO */
PUBLIC bool ral_present_job_set_presentable_output(ral_present_job     job,
                                                   ral_present_task_id task_id,
                                                   uint32_t            n_output);

#endif /* RAL_PRESENT_JOB_H */
