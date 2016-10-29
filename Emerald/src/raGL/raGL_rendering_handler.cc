/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_command_buffer.h"
#include "raGL/raGL_dep_tracker.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_framebuffers.h"
#include "raGL/raGL_rendering_handler.h"
#include "raGL/raGL_types.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_dag.h"
#include "system/system_event.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"
#include "system/system_threads.h"
#include "system/system_time.h"
#include "system/system_window.h"

enum
{
    /* Order must be reflected in raGL_rendering_handler_get_wait_event() */
    RAGL_RENDERING_HANDLER_RENDERING_THREAD_CALLBACK_REQUEST_ID,
    RAGL_RENDERING_HANDLER_CONTEXT_SHARING_SETUP_REQUEST_ID,
};

typedef struct _raGL_rendering_handler
{
    system_window           context_window;
    system_hash64map        dag_present_task_to_node_map;
    system_resizable_vector ordered_present_tasks;
    ral_rendering_handler   rendering_handler_ral;

    ral_rendering_handler_custom_wait_event_handler* handlers;

    ogl_context call_passthrough_context;
    bool        call_passthrough_mode;

    system_event bind_context_request_event;
    system_event bind_context_request_ack_event;
    system_event unbind_context_request_event;
    system_event unbind_context_request_ack_event;

    system_critical_section                     callback_request_cs;
    system_event                                callback_request_ack_event;
    system_event                                callback_request_event;
    void*                                       callback_request_user_arg;
    PFNRAGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc;

    bool                                             ral_callback_buffer_swap_needed;
    system_critical_section                          ral_callback_cs;
    volatile PFNRALRENDERINGHANDLERRENDERINGCALLBACK ral_callback_pfn_callback_proc;
    volatile void*                                   ral_callback_user_arg;

    raGL_backend     backend;
    ral_backend_type backend_type;
    ogl_context      context_gl;
    bool             default_fb_has_depth_attachment;
    bool             default_fb_has_stencil_attachment;
    bool             is_helper_context;
    bool             is_multisample_pf;
    bool             is_vsync_enabled;

    PFNGLBINDFRAMEBUFFERPROC  pGLBindFramebuffer;
    PFNGLBLITFRAMEBUFFERPROC  pGLBlitFramebuffer;
    PFNGLCLEARPROC            pGLClear;
    PFNGLCLEARCOLORPROC       pGLClearColor;
    PFNGLDISABLEPROC          pGLDisable;
    PFNGLENABLEPROC           pGLEnable;
    PFNGLFINISHPROC           pGLFinish;
    PFNGLMEMORYBARRIERPROC    pGLMemoryBarrier;
    PFNGLSCISSORPROC          pGLScissor;
    PFNGLVIEWPORTPROC         pGLViewport;


    explicit _raGL_rendering_handler(ral_rendering_handler in_rendering_handler_ral);

    ~_raGL_rendering_handler()
    {
        system_critical_section_release(ral_callback_cs);

        if (dag_present_task_to_node_map != nullptr)
        {
            uint32_t n_map_entries = 0;

            system_hash64map_get_property(dag_present_task_to_node_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_map_entries);

            ASSERT_DEBUG_SYNC(n_map_entries == 0,
                              "DAG present task->DAG node map contains entries at destruction time");

            system_hash64map_release(dag_present_task_to_node_map);
            dag_present_task_to_node_map = nullptr;
        }

        if (ordered_present_tasks != nullptr)
        {
            uint32_t n_present_tasks = 0;

            system_resizable_vector_get_property(ordered_present_tasks,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_present_tasks);

            ASSERT_DEBUG_SYNC(n_present_tasks == 0,
                              "ordered_present_tasks vector contains entries at destruction time");

            system_resizable_vector_release(ordered_present_tasks);
            ordered_present_tasks = nullptr;
        }

        system_event_release(bind_context_request_event);
        system_event_release(bind_context_request_ack_event);
        system_event_release(callback_request_ack_event);
        system_event_release(callback_request_event);
        system_event_release(unbind_context_request_event);
        system_event_release(unbind_context_request_ack_event);

        delete [] handlers;
        handlers = nullptr;

        /* Release user callback serialization cs */
        system_critical_section_release(callback_request_cs);

    }
} _raGL_rendering_handler;

PRIVATE void       _raGL_rendering_handler_rendering_thread_callback_requested_event_handler(uint32_t                 ignored,
                                                                                             void*                    rendering_handler_raBackend);
PRIVATE void       _raGL_rendering_handler_context_sharing_setup_request_event_handler      (uint32_t                 ignored,
                                                                                             void*                    rendering_handler_raBackend);
PRIVATE system_dag _raGL_rendering_handler_create_dag_from_present_job                      (_raGL_rendering_handler* rendering_handler_ptr,
                                                                                             ral_present_job          present_job);


static const ral_rendering_handler_custom_wait_event_handler ref_handlers[] =
{
    { nullptr, RAGL_RENDERING_HANDLER_RENDERING_THREAD_CALLBACK_REQUEST_ID, _raGL_rendering_handler_rendering_thread_callback_requested_event_handler},
    { nullptr, RAGL_RENDERING_HANDLER_CONTEXT_SHARING_SETUP_REQUEST_ID,     _raGL_rendering_handler_context_sharing_setup_request_event_handler}
};
static const uint32_t n_ref_handlers = sizeof(ref_handlers) / sizeof(ref_handlers[0]);


_raGL_rendering_handler::_raGL_rendering_handler(ral_rendering_handler in_rendering_handler_ral)
{
    dag_present_task_to_node_map = system_hash64map_create(sizeof(system_dag_node) );
    ordered_present_tasks        = system_resizable_vector_create(16); /* capacity */
    rendering_handler_ral        = in_rendering_handler_ral;

    call_passthrough_context = nullptr;
    call_passthrough_mode    = false;

    bind_context_request_event       = system_event_create(false); /* manual_reset */
    bind_context_request_ack_event   = system_event_create(false); /* manual_reset */
    callback_request_ack_event       = system_event_create(false); /* manual_reset */
    callback_request_event           = system_event_create(false); /* manual_reset */
    callback_request_cs              = system_critical_section_create();
    callback_request_user_arg        = nullptr;
    is_helper_context                = false;
    is_multisample_pf                = false;
    is_vsync_enabled                 = false;
    pfn_callback_proc                = nullptr;
    unbind_context_request_event     = system_event_create (false); /* manual_reset */
    unbind_context_request_ack_event = system_event_create (false); /* manual_reset */

    ral_callback_buffer_swap_needed = false;
    ral_callback_cs                 = system_critical_section_create();

    backend                           = nullptr;
    backend_type                      = RAL_BACKEND_TYPE_UNKNOWN;
    context_gl                        = nullptr;
    context_window                    = nullptr;
    default_fb_has_depth_attachment   = false;
    default_fb_has_stencil_attachment = false;

    pGLBindFramebuffer = nullptr;
    pGLBlitFramebuffer = nullptr;
    pGLClear           = nullptr;
    pGLClearColor      = nullptr;
    pGLDisable         = nullptr;
    pGLEnable          = nullptr;
    pGLFinish          = nullptr;
    pGLMemoryBarrier   = nullptr;
    pGLScissor         = nullptr;
    pGLViewport        = nullptr;

    handlers = new ral_rendering_handler_custom_wait_event_handler[n_ref_handlers];

    memcpy(handlers,
           ref_handlers,
           n_ref_handlers * sizeof(ral_rendering_handler_custom_wait_event_handler) );

    handlers[RAGL_RENDERING_HANDLER_RENDERING_THREAD_CALLBACK_REQUEST_ID].event = callback_request_event;
    handlers[RAGL_RENDERING_HANDLER_CONTEXT_SHARING_SETUP_REQUEST_ID].event     = unbind_context_request_event;
}


/** TODO */
PRIVATE void _raGL_rendering_handler_context_sharing_setup_request_event_handler(uint32_t ignored,
                                                                                 void*    rendering_handler_raBackend)
{
    _raGL_rendering_handler* rendering_handler_ptr = static_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    /* This event is used for setting up context sharing. In order for it to succeed, we must unbind the context from
     * current thread, then wait for a 'job done' signal, and bind the context back to this thread */
    ogl_context_unbind_from_current_thread(rendering_handler_ptr->context_gl);

    system_event_set        (rendering_handler_ptr->unbind_context_request_ack_event);
    system_event_wait_single(rendering_handler_ptr->bind_context_request_event);

    ogl_context_bind_to_current_thread(rendering_handler_ptr->context_gl);

    system_event_set(rendering_handler_ptr->bind_context_request_ack_event);
}

/** TODO */
PRIVATE system_dag _raGL_rendering_handler_create_dag_from_present_job(_raGL_rendering_handler* rendering_handler_ptr,
                                                                       ral_present_job          present_job)
{
    uint32_t   n_connections   = 0;
    uint32_t   n_present_tasks = 0;
    system_dag result          = nullptr;

    ral_present_job_get_property(present_job,
                                 RAL_PRESENT_JOB_PROPERTY_N_CONNECTIONS,
                                &n_connections);
    ral_present_job_get_property(present_job,
                                 RAL_PRESENT_JOB_PROPERTY_N_PRESENT_TASKS,
                                &n_present_tasks);

    if (n_present_tasks == 0)
    {
        goto end;
    }

    result = system_dag_create();

    for (uint32_t n_present_task = 0;
                  n_present_task < n_present_tasks;
                ++n_present_task)
    {
        ral_present_task      present_task      = nullptr;
        system_dag_node       present_task_node = nullptr;
        ral_present_task_type present_task_type;

        ral_present_job_get_task_at_index(present_job,
                                          n_present_task,
                                         &present_task);

        ASSERT_DEBUG_SYNC(present_task != nullptr,
                          "Null present task reported at index [%d]",
                          n_present_task);

        ral_present_task_get_property(present_task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &present_task_type);

        ASSERT_DEBUG_SYNC(present_task_type != RAL_PRESENT_TASK_TYPE_GROUP,
                          "The present job must be flattened before it is passed for execution.");

        present_task_node = system_dag_add_node(result,
                                                reinterpret_cast<system_dag_node_value>(present_task) );

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(rendering_handler_ptr->dag_present_task_to_node_map,
                                                     reinterpret_cast<system_hash64>(present_task) ),
                          "Present task->DAG node map already holds a DAG node for the newly spawned present task");
        ASSERT_DEBUG_SYNC(present_task_node  != nullptr,
                          "NULL DAG node created for a present task");

        system_hash64map_insert(rendering_handler_ptr->dag_present_task_to_node_map,
                                reinterpret_cast<system_hash64>(present_task),
                                present_task_node ,
                                nullptr,  /* on_removal_callback          */
                                nullptr); /* on_removal_callback_user_arg */
    }

    for (uint32_t n_connection = 0;
                  n_connection < n_connections;
                ++n_connection)
    {
        ral_present_job_connection_id connection_id                = -1;
        ral_present_task              dst_present_task             = nullptr;
        system_dag_node               dst_present_task_dag_node    = nullptr;
        ral_present_task_id           dst_present_task_id          = -1;
        uint32_t                      dst_present_task_input_index = -1;
        ral_present_task_type         dst_present_task_type;
        ral_present_task              src_present_task              = nullptr;
        system_dag_node               src_present_task_dag_node     = nullptr;
        ral_present_task_id           src_present_task_id           = -1;
        uint32_t                      src_present_task_output_index = -1;
        ral_present_task_type         src_present_task_type;

        ral_present_job_get_connection_id_at_index(present_job,
                                                   n_connection,
                                                  &connection_id);
        ral_present_job_get_connection_property   (present_job,
                                                   connection_id,
                                                   RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_ID,
                                                  &dst_present_task_id);
        ral_present_job_get_connection_property   (present_job,
                                                   connection_id,
                                                   RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_INPUT_INDEX,
                                                  &dst_present_task_input_index);
        ral_present_job_get_connection_property   (present_job,
                                                   connection_id,
                                                   RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_ID,
                                                  &src_present_task_id);
        ral_present_job_get_connection_property   (present_job,
                                                   connection_id,
                                                   RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_OUTPUT_INDEX,
                                                  &src_present_task_output_index);

        ral_present_job_get_task_with_id(present_job,
                                         dst_present_task_id,
                                        &dst_present_task);
        ral_present_job_get_task_with_id(present_job,
                                         src_present_task_id,
                                        &src_present_task);

        ral_present_task_get_property(dst_present_task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &dst_present_task_type);
        ral_present_task_get_property(src_present_task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &src_present_task_type);

        system_hash64map_get(rendering_handler_ptr->dag_present_task_to_node_map,
                             reinterpret_cast<system_hash64>(dst_present_task),
                            &dst_present_task_dag_node);
        system_hash64map_get(rendering_handler_ptr->dag_present_task_to_node_map,
                             reinterpret_cast<system_hash64>(src_present_task),
                            &src_present_task_dag_node);

        if (!system_dag_is_connection_defined(result,
                                              src_present_task_dag_node,
                                              dst_present_task_dag_node) )
        {
            system_dag_add_connection(result,
                                      src_present_task_dag_node,
                                      dst_present_task_dag_node);
        }
    }

end:
    system_hash64map_clear(rendering_handler_ptr->dag_present_task_to_node_map);

    return result;
}

/** TODO */
PRIVATE void _raGL_rendering_handler_ral_based_rendering_callback_handler_internal(ogl_context                             context,
                                                                                   _raGL_rendering_handler*                rendering_handler_ptr,
                                                                                   PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                                                   volatile void*                          callback_user_arg)
{
    ral_context     context_ral;
    ral_present_job present_job;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);

    ASSERT_DEBUG_SYNC(context_ral != nullptr,
                      "ogl_context uses a null RAL context");

    raGL_backend_sync();

    present_job = pfn_callback_proc(context_ral,
                                    const_cast<void*>(callback_user_arg),
                                    nullptr); /* frame_data_ptr */

    if (present_job != nullptr)
    {
        raGL_rendering_handler_execute_present_job(reinterpret_cast<ral_rendering_handler>(rendering_handler_ptr),
                                                   present_job);

        ral_present_job_release(present_job);
    }
}

/** TODO */
PRIVATE void _raGL_rendering_handler_ral_based_rendering_callback_handler(ogl_context context,
                                                                          void*       rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    _raGL_rendering_handler_ral_based_rendering_callback_handler_internal(context,
                                                                          rendering_handler_ptr,
                                                                          rendering_handler_ptr->ral_callback_pfn_callback_proc,
                                                                          rendering_handler_ptr->ral_callback_user_arg);

    rendering_handler_ptr->ral_callback_pfn_callback_proc = nullptr;
    rendering_handler_ptr->ral_callback_user_arg          = nullptr;
}

/** TODO */
PRIVATE void _raGL_rendering_handler_rendering_thread_callback_requested_event_handler(uint32_t ignored,
                                                                                       void*    rendering_handler_raBackend)
{
    raGL_dep_tracker         dep_tracker           = nullptr;
    bool                     needs_buffer_swap     = true;
    _raGL_rendering_handler* rendering_handler_ptr = static_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    /* Call-back requested. Sync and handle the request */
    raGL_backend_sync();

    /* Reset dependency tracker and force full cache flush. Expectation is that each frame's contents
     * is going to be rendered by submitting a single present job, and that rendering thread callbacks
     * are exclusive to back-end usage (CPU->GPU transfers, GL object creation & set-up, etc.), so this
     * should not hurt performance too much, but will keep us safe.
     */
    raGL_backend_get_private_property(rendering_handler_ptr->backend,
                                      RAGL_BACKEND_PRIVATE_PROPERTY_DEP_TRACKER,
                                     &dep_tracker);

    raGL_dep_tracker_reset(dep_tracker);

    rendering_handler_ptr->pGLMemoryBarrier (GL_ALL_BARRIER_BITS);

    /* Execute GL commands */
    rendering_handler_ptr->pfn_callback_proc(rendering_handler_ptr->context_gl,
                                             rendering_handler_ptr->callback_request_user_arg);

    if (rendering_handler_ptr->ral_callback_buffer_swap_needed)
    {
#if 0
        uint32_t window_size    [2];
        uint32_t window_x1y1x2y2[4];

        ASSERT_DEBUG_SYNC(!rendering_handler_ptr->is_helper_context,
                          "Buffer swaps are unavailable for helper contexts");

        /* Blit the context FBO's contents to the back buffer */
        system_window_get_property(rendering_handler_ptr->context_window,
                                   SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                                   window_x1y1x2y2);

        window_size[0] = window_x1y1x2y2[2] - window_x1y1x2y2[0];
        window_size[1] = window_x1y1x2y2[3] - window_x1y1x2y2[1];

        rendering_handler_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                  0);
        rendering_handler_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                  rendering_handler_ptr->default_fb_raGL_id);

        rendering_handler_ptr->pGLBlitFramebuffer(0,              /* srcX0 */
                                                  0,              /* srcY0 */
                                                  window_size[0], /* srcX1 */
                                                  window_size[1], /* srcY1 */
                                                  0,              /* dstX0 */
                                                  0,              /* dstY0 */
                                                  window_size[0], /* dstX1 */
                                                  window_size[1], /* dstY1 */
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_NEAREST);

        ogl_context_swap_buffers(rendering_handler_ptr->context_gl);
#else
        /* TODO: is this path still needed? */
        ASSERT_DEBUG_SYNC(false,
                          "TODO");
#endif
    }

    /* Reset callback data */
    rendering_handler_ptr->callback_request_user_arg       = nullptr;
    rendering_handler_ptr->pfn_callback_proc               = nullptr;
    rendering_handler_ptr->ral_callback_buffer_swap_needed = false;

    /* Set ack event */
    system_event_set(rendering_handler_ptr->callback_request_ack_event);
}


/** TODO */
PUBLIC void* raGL_rendering_handler_create(ral_rendering_handler rendering_handler_ral)
{
    _raGL_rendering_handler* new_handler_ptr = nullptr;

    new_handler_ptr = new (std::nothrow) _raGL_rendering_handler(rendering_handler_ral);

    return new_handler_ptr;
}

/* TODO */
PUBLIC void raGL_rendering_handler_enumerate_custom_wait_event_handlers(void*                                                   rendering_handler_backend,
                                                                        uint32_t*                                               opt_out_n_custom_wait_event_handlers_ptr,
                                                                        const ral_rendering_handler_custom_wait_event_handler** opt_out_custom_wait_event_handlers_ptr)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_backend);

    if (opt_out_n_custom_wait_event_handlers_ptr != nullptr)
    {
        *opt_out_n_custom_wait_event_handlers_ptr = n_ref_handlers;
    }

    if (opt_out_custom_wait_event_handlers_ptr != nullptr)
    {
        *opt_out_custom_wait_event_handlers_ptr = rendering_handler_ptr->handlers;
    }
}

/** Please see header for specification */
PUBLIC void raGL_rendering_handler_execute_present_job(void*           rendering_handler_raGL,
                                                       ral_present_job present_job)
{
    raGL_dep_tracker         dep_tracker             = nullptr;
    uint32_t                 n_ordered_present_tasks = 0;
    system_dag               present_job_dag         = nullptr;
    _raGL_rendering_handler* rendering_handler_ptr   = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raGL);

    raGL_backend_get_private_property(rendering_handler_ptr->backend,
                                      RAGL_BACKEND_PRIVATE_PROPERTY_DEP_TRACKER,
                                     &dep_tracker);

    /* Create a DAG from the present job. */
    present_job_dag = _raGL_rendering_handler_create_dag_from_present_job(rendering_handler_ptr,
                                                                          present_job);

    if (!system_dag_solve(present_job_dag))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not solve a DAG created from the submitted present job");

        goto end;
    }

    if (!system_dag_get_topologically_sorted_node_values(present_job_dag,
                                                         rendering_handler_ptr->ordered_present_tasks) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve topologically sorted present tasks");

        goto end;
    }

    /* Present tasks need to be executed one-after-another. We theoretically could distribute these to
     * separate worker rendering threads, but context sharing is defined pretty loosely and GL implementations
     * are pretty crap at supporting it.
     */
    system_resizable_vector_get_property(rendering_handler_ptr->ordered_present_tasks,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_ordered_present_tasks);

    for (uint32_t n_present_task = 0;
                  n_present_task < n_ordered_present_tasks;
                ++n_present_task)
    {
        ral_present_task      task                 = nullptr;
        raGL_command_buffer   task_cmd_buffer_raGL = nullptr;
        ral_command_buffer    task_cmd_buffer_ral  = nullptr;
        ral_present_task_type task_type;

        system_resizable_vector_get_element_at(rendering_handler_ptr->ordered_present_tasks,
                                               n_present_task,
                                              &task);

        ral_present_task_get_property(task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &task_type);

        switch (task_type)
        {
            case RAL_PRESENT_TASK_TYPE_CPU_TASK:
            {
                void*                            cpu_callback_user_arg;
                PFNRALPRESENTTASKCPUCALLBACKPROC pfn_cpu_callback_proc;

                ral_present_task_get_property(task,
                                              RAL_PRESENT_TASK_PROPERTY_CPU_CALLBACK_PROC,
                                             &pfn_cpu_callback_proc);
                ral_present_task_get_property(task,
                                              RAL_PRESENT_TASK_PROPERTY_CPU_CALLBACK_USER_ARG,
                                             &cpu_callback_user_arg);

                pfn_cpu_callback_proc(cpu_callback_user_arg);

                break;
            }

            case RAL_PRESENT_TASK_TYPE_GPU_TASK:
            {
                ral_present_task_get_property(task,
                                              RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER,
                                             &task_cmd_buffer_ral);

                raGL_backend_get_command_buffer(rendering_handler_ptr->backend,
                                                task_cmd_buffer_ral,
                                               &task_cmd_buffer_raGL);

                raGL_command_buffer_execute(task_cmd_buffer_raGL,
                                            dep_tracker);

                break;
            }

            case RAL_PRESENT_TASK_TYPE_GROUP:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "RAL group present task should never reach this place!");

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported RAL present task type encountered.");
            }
        }
    }

    ral_present_job_get_property(present_job,
                                 RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED,
                                &rendering_handler_ptr->ral_callback_buffer_swap_needed);

    if (rendering_handler_ptr->call_passthrough_mode)
    {
        ASSERT_DEBUG_SYNC(!rendering_handler_ptr->ral_callback_buffer_swap_needed,
                          "Cannot swap buffers");
    }

end:
    system_resizable_vector_clear(rendering_handler_ptr->ordered_present_tasks);

    if (present_job_dag != nullptr)
    {
        system_dag_release(present_job_dag);

        present_job_dag = nullptr;
    }
}

/* TODO */
PUBLIC void raGL_rendering_handler_init_from_rendering_thread(ral_context           context_ral,
                                                              ral_rendering_handler rendering_handler_ral,
                                                              void*                 rendering_handler_raGL)
{
    system_window            context_window                = nullptr;
    unsigned char            context_window_n_depth_bits   = 0;
    unsigned char            context_window_n_stencil_bits = 0;
    unsigned char            context_window_n_samples      = 0;
    system_pixel_format      context_window_pf             = nullptr;
    _raGL_rendering_handler* rendering_handler_ptr         = static_cast<_raGL_rendering_handler*>(rendering_handler_raGL);

    ral_context_get_property (context_ral,
                              RAL_CONTEXT_PROPERTY_BACKEND,
                             &rendering_handler_ptr->backend);

    /* Spin until rendering context becomes available. It will be set up shortly in the same thread
     * that created the window. */
    while (rendering_handler_ptr->context_gl == nullptr)
    {
        raGL_backend_get_property(rendering_handler_ptr->backend,
                                  RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                                 &rendering_handler_ptr->context_gl);

        #ifdef _WIN32
        {
            Sleep(0);
        }
        #else
        {
            sched_yield();
        }
        #endif
    }

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context_gl != nullptr,
                      "raGL_context instance is NULL");

    ogl_context_wait_till_inited(rendering_handler_ptr->context_gl);

    ogl_context_get_property(rendering_handler_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &rendering_handler_ptr->backend_type);
    ogl_context_get_property(rendering_handler_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_IS_HELPER_CONTEXT,
                            &rendering_handler_ptr->is_helper_context);
    ogl_context_get_property(rendering_handler_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &context_window);

    ASSERT_DEBUG_SYNC(context_window != nullptr,
                      "No window associated with the GL context");

    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &context_window_pf);
    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_IS_VSYNC_ENABLED,
                              &rendering_handler_ptr->is_vsync_enabled);

    system_pixel_format_get_property(context_window_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_N_SAMPLES,
                                    &context_window_n_samples);
    system_pixel_format_get_property(context_window_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                    &context_window_n_depth_bits);
    system_pixel_format_get_property(context_window_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_STENCIL_BITS,
                                    &context_window_n_stencil_bits);

    rendering_handler_ptr->context_window                    = context_window;
    rendering_handler_ptr->default_fb_has_depth_attachment   = (context_window_n_depth_bits   >  0);
    rendering_handler_ptr->default_fb_has_stencil_attachment = (context_window_n_stencil_bits >  0);
    rendering_handler_ptr->is_multisample_pf                 = (context_window_n_samples      >  1);

    ogl_context_bind_to_current_thread(rendering_handler_ptr->context_gl);

    if (rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entrypoints_ptr = nullptr;

        ogl_context_get_property(rendering_handler_ptr->context_gl,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entrypoints_ptr);

        rendering_handler_ptr->pGLBindFramebuffer = entrypoints_ptr->pGLBindFramebuffer;
        rendering_handler_ptr->pGLBlitFramebuffer = entrypoints_ptr->pGLBlitFramebuffer;
        rendering_handler_ptr->pGLClear           = entrypoints_ptr->pGLClear;
        rendering_handler_ptr->pGLClearColor      = entrypoints_ptr->pGLClearColor;
        rendering_handler_ptr->pGLDisable         = entrypoints_ptr->pGLDisable;
        rendering_handler_ptr->pGLEnable          = entrypoints_ptr->pGLEnable;
        rendering_handler_ptr->pGLFinish          = entrypoints_ptr->pGLFinish;
        rendering_handler_ptr->pGLMemoryBarrier   = entrypoints_ptr->pGLMemoryBarrier;
        rendering_handler_ptr->pGLScissor         = entrypoints_ptr->pGLScissor;
        rendering_handler_ptr->pGLViewport        = entrypoints_ptr->pGLViewport;
    }
    else
    {
        const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;

        ASSERT_DEBUG_SYNC(rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_GL,
                          "Unrecognized backend type");

        ogl_context_get_property(rendering_handler_ptr->context_gl,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);

        rendering_handler_ptr->pGLBindFramebuffer = entrypoints_ptr->pGLBindFramebuffer;
        rendering_handler_ptr->pGLBlitFramebuffer = entrypoints_ptr->pGLBlitFramebuffer;
        rendering_handler_ptr->pGLClear           = entrypoints_ptr->pGLClear;
        rendering_handler_ptr->pGLClearColor      = entrypoints_ptr->pGLClearColor;
        rendering_handler_ptr->pGLDisable         = entrypoints_ptr->pGLDisable;
        rendering_handler_ptr->pGLEnable          = entrypoints_ptr->pGLEnable;
        rendering_handler_ptr->pGLFinish          = entrypoints_ptr->pGLFinish;
        rendering_handler_ptr->pGLMemoryBarrier   = entrypoints_ptr->pGLMemoryBarrier;
        rendering_handler_ptr->pGLScissor         = entrypoints_ptr->pGLScissor;
        rendering_handler_ptr->pGLViewport        = entrypoints_ptr->pGLViewport;
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_lock_bound_context(raGL_rendering_handler rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    if (!ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler_ptr->rendering_handler_ral) )
    {
        system_event_set        (rendering_handler_ptr->unbind_context_request_event);
        system_event_wait_single(rendering_handler_ptr->unbind_context_request_ack_event);

        ogl_context_bind_to_current_thread(rendering_handler_ptr->context_gl);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_post_draw_frame(void*           rendering_handler_raBackend,
                                                   ral_present_job present_job)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    if (present_job != nullptr)
    {
        raGL_dep_tracker         dep_tracker                     = nullptr;
        ral_context_object_type  presentable_output_object_type;
        ral_present_task         presentable_output_task         = nullptr;
        uint32_t                 presentable_output_task_index   = -1;
        uint32_t                 presentable_output_task_io      = -1;
        ral_present_task_io_type presentable_output_task_io_type;
        raGL_texture             presentable_texture_raGL         = nullptr;
        ral_texture              presentable_texture_ral          = nullptr;
        ral_texture_view         presentable_texture_view         = nullptr;
        uint32_t                 presentable_texture_view_size[2] = {0};

        raGL_backend_get_private_property(rendering_handler_ptr->backend,
                                          RAGL_BACKEND_PRIVATE_PROPERTY_DEP_TRACKER,
                                         &dep_tracker);

        ral_present_job_get_property(present_job,
                                     RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID,
                                    &presentable_output_task_index);
        ral_present_job_get_property(present_job,
                                     RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_INDEX,
                                    &presentable_output_task_io);
        ral_present_job_get_property(present_job,
                                     RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_TYPE,
                                    &presentable_output_task_io_type);

        ral_present_job_get_task_with_id(present_job,
                                         presentable_output_task_index,
                                        &presentable_output_task);

        ral_present_task_get_io_property(presentable_output_task,
                                         presentable_output_task_io_type,
                                         presentable_output_task_io,
                                         RAL_PRESENT_TASK_IO_PROPERTY_OBJECT,
                                         reinterpret_cast<void**>(&presentable_texture_view) );

        #ifdef _DEBUG
        {
            ral_present_task_get_io_property(presentable_output_task,
                                             presentable_output_task_io_type,
                                             presentable_output_task_io,
                                             RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                             reinterpret_cast<void**>(&presentable_output_object_type) );

            ASSERT_DEBUG_SYNC(presentable_output_object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                              "Invalid presentable output object's type");
        }
        #endif

        ral_texture_view_get_property(presentable_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &presentable_texture_ral);

        raGL_backend_get_texture(rendering_handler_ptr->backend,
                                 presentable_texture_ral,
                                &presentable_texture_raGL);

        ASSERT_DEBUG_SYNC(presentable_texture_raGL != nullptr,
                          "raGL instance of the presentable texture is null");

        ral_texture_view_get_mipmap_property(presentable_texture_view,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                             presentable_texture_view_size + 0);
        ral_texture_view_get_mipmap_property(presentable_texture_view,
                                             0, /* n_layer  */
                                             0, /* n_mipmap */
                                             RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                             presentable_texture_view_size + 1);

        /* If the presentable texture is marked as dirty in the dep tracker, make sure to flush it
         * before we use it as a blit source */
        raGL_framebuffers fbos            = nullptr;
        raGL_framebuffer  read_fb_raGL    = nullptr;
        GLuint            read_fb_raGL_id = -1;

        raGL_backend_get_private_property(rendering_handler_ptr->backend,
                                          RAGL_BACKEND_PRIVATE_PROPERTY_FBOS,
                                         &fbos);

        raGL_framebuffers_get_framebuffer(fbos,
                                          1, /* in_n_attachments */
                                         &presentable_texture_view,
                                          nullptr, /* in_opt_ds_attachment */
                                         &read_fb_raGL);
        raGL_framebuffer_get_property    (read_fb_raGL,
                                          RAGL_FRAMEBUFFER_PROPERTY_ID,
                                         &read_fb_raGL_id);

        rendering_handler_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                  0);
        rendering_handler_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                  read_fb_raGL_id);

        if (raGL_dep_tracker_is_dirty(dep_tracker,
                                      presentable_texture_raGL,
                                      GL_FRAMEBUFFER_BARRIER_BIT) )
        {
            rendering_handler_ptr->pGLMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
        }

        /* Blit the context FBO's contents to the back buffer */
        uint32_t window_size[2];

        system_window_get_property(rendering_handler_ptr->context_window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        rendering_handler_ptr->pGLDisable        (GL_SCISSOR_TEST);
        rendering_handler_ptr->pGLBlitFramebuffer(0,                                /* srcX0 */
                                                  0,                                /* srcY0 */
                                                  presentable_texture_view_size[0], /* srcX1 */
                                                  presentable_texture_view_size[1], /* srcY1 */
                                                  0,                                /* dstX0 */
                                                  0,                                /* dstY0 */
                                                  window_size[0],                   /* dstX1 */
                                                  window_size[1],                   /* dstY1 */
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_LINEAR);
    }

    if (rendering_handler_ptr->is_multisample_pf                   &&
        rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_GL)
    {
        rendering_handler_ptr->pGLDisable(GL_MULTISAMPLE);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_pre_draw_frame(void* rendering_handler_raBackend)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    ASSERT_DEBUG_SYNC(!rendering_handler_ptr->is_helper_context,
                      "Playback is unavailable for helper contexts");

    /* Sync with other contexts.. */
    raGL_backend_sync();

    /* Enable multisampling if needed */
    if (rendering_handler_ptr->is_multisample_pf && rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_GL)
    {
        rendering_handler_ptr->pGLEnable(GL_MULTISAMPLE);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_present_frame(void*                   rendering_handler_raBackend,
                                                 system_critical_section rendering_cs)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    ogl_context_swap_buffers(rendering_handler_ptr->context_gl);

    /* If vertical sync is enabled, force a full pipeline flush + GPU-side execution
     * of the enqueued commands. This helps fighting the tearing A LOT for scenes
     * that take more time to render (eg. the test "greets" scene which slides
     * from right to left), since the frame misses are less frequent. It also puts
     * less pressure on the CPU, since we're no longer spinning.
     */
    if (rendering_handler_ptr->is_vsync_enabled)
    {
        system_critical_section_leave(rendering_cs);
        {
            rendering_handler_ptr->pGLFinish();
        }
        system_critical_section_enter(rendering_cs);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_release(void* rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    delete rendering_handler_ptr;
}

/** Please see header for specification */
PUBLIC bool raGL_rendering_handler_request_callback_for_ral_rendering_handler(void*                                   rendering_handler_backend,
                                                                              PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                                              void*                                   user_arg,
                                                                              bool                                    present_after_executed,
                                                                              ral_rendering_handler_execution_mode    execution_mode)
{
    bool                     dispatch_to_rendering_thread = false;
    _raGL_rendering_handler* rendering_handler_ptr        = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_backend);
    bool                     result;


    dispatch_to_rendering_thread = !(ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler_ptr->rendering_handler_ral) ||
                                     rendering_handler_ptr->call_passthrough_mode);

    if (dispatch_to_rendering_thread)
    {
        system_critical_section_enter(rendering_handler_ptr->ral_callback_cs);
        {
            bool should_continue = false;

            rendering_handler_ptr->ral_callback_buffer_swap_needed = present_after_executed;
            rendering_handler_ptr->ral_callback_pfn_callback_proc  = pfn_callback_proc;
            rendering_handler_ptr->ral_callback_user_arg           = user_arg;

            if (execution_mode != RAL_RENDERING_HANDLER_EXECUTION_MODE_ONLY_IF_IDLE_BLOCK_TILL_FINISHED)
            {
                system_critical_section_enter(rendering_handler_ptr->callback_request_cs);

                should_continue = true;
            }
            else
            {
                should_continue = system_critical_section_try_enter(rendering_handler_ptr->callback_request_cs);
            }

            if (should_continue)
            {
                while (rendering_handler_ptr->callback_request_user_arg != nullptr ||
                       rendering_handler_ptr->pfn_callback_proc         != nullptr)
                {
                    /* Spin until we can cache a new call-back request */
                }

                rendering_handler_ptr->callback_request_user_arg = rendering_handler_ptr;
                rendering_handler_ptr->pfn_callback_proc         = _raGL_rendering_handler_ral_based_rendering_callback_handler;

                system_event_set(rendering_handler_ptr->callback_request_event);

                if (execution_mode != RAL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_DONT_BLOCK)
                {
                    system_event_wait_single(rendering_handler_ptr->callback_request_ack_event);
                }

                system_critical_section_leave(rendering_handler_ptr->callback_request_cs);
            }

            result = should_continue;
        }
        system_critical_section_leave(rendering_handler_ptr->ral_callback_cs);
    }
    else
    {
        volatile void*                          cached_callback_user_arg = nullptr;
        ogl_context                             cached_context           = nullptr;
        PFNRALRENDERINGHANDLERRENDERINGCALLBACK cached_pfn_callback_proc = nullptr;

        cached_callback_user_arg = user_arg;
        cached_pfn_callback_proc = pfn_callback_proc;

        if (rendering_handler_ptr->call_passthrough_mode)
        {
            cached_context = rendering_handler_ptr->call_passthrough_context;

            ASSERT_DEBUG_SYNC(cached_context != nullptr,
                              "No ogl_context assigned to RAL context");
        }
        else
        {
            cached_context = rendering_handler_ptr->context_gl;
        }

        _raGL_rendering_handler_ral_based_rendering_callback_handler_internal(cached_context,
                                                                              rendering_handler_ptr,
                                                                              cached_pfn_callback_proc,
                                                                              cached_callback_user_arg);

        result = true;
    }

    return result;
}

/** Please see header for specification */
PUBLIC void raGL_rendering_handler_set_property(raGL_rendering_handler           rendering_handler,
                                                raGL_rendering_handler_property  property,
                                                const void*                      value)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    switch (property)
    {
        case RAGL_RENDERING_HANDLER_PROPERTY_CALL_PASSTHROUGH_CONTEXT:
        {
            rendering_handler_ptr->call_passthrough_context = *reinterpret_cast<const ogl_context*>(value);

            break;
        }

        case RAGL_RENDERING_HANDLER_PROPERTY_CALL_PASSTHROUGH_MODE:
        {
            const bool new_status = *reinterpret_cast<const bool*>(value);

            rendering_handler_ptr->call_passthrough_mode = new_status;

            ogl_context_set_passthrough_context(new_status ? rendering_handler_ptr->context_gl
                                                           : nullptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_rendering_handler_property value");
        }
    }

}

/** TODO */
PUBLIC void raGL_rendering_handler_unlock_bound_context(raGL_rendering_handler rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    if (!ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler_ptr->rendering_handler_ral) )
    {
        ogl_context_unbind_from_current_thread(rendering_handler_ptr->context_gl);

        system_event_set        (rendering_handler_ptr->bind_context_request_event);
        system_event_wait_single(rendering_handler_ptr->bind_context_request_ack_event);
    }
}
