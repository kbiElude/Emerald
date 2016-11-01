/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_sync.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_scheduler.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


typedef struct _raGL_buffer
{
    raGL_backend          backend;
    ogl_context           context;       /* NOT owned */
    GLint                 id;            /* NOT owned */
    system_memory_manager memory_manager;
    _raGL_buffer*         parent_buffer_ptr;
    uint32_t              size;
    uint32_t              start_offset;

    ogl_context_gl_entrypoints* entrypoints_ptr;

    _raGL_buffer(_raGL_buffer*         in_parent_buffer_ptr,
                 ogl_context           in_context,
                 GLint                 in_id,
                 system_memory_manager in_memory_manager,
                 uint32_t              in_start_offset,
                 uint32_t              in_size)
    {
        context           = in_context;
        id                = in_id;
        memory_manager    = in_memory_manager;
        parent_buffer_ptr = in_parent_buffer_ptr;
        size              = in_size;
        start_offset      = in_start_offset;

        ogl_context_get_property(in_context,
                                 OGL_CONTEXT_PROPERTY_BACKEND,
                                &backend);

        /* NOTE: Only GL is supported at the moment. */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "TODO");
    }

    ~_raGL_buffer()
    {
        /* Do not release the GL buffer. Object life-time is handled by raGL_buffers. */
    }
} _raGL_buffer;

typedef struct
{
    raGL_buffer                         buffer;
    const ral_buffer_clear_region_info* clear_ops;
    uint32_t                            n_clear_ops;
    bool                                sync_other_contexts;
} _raGL_buffer_clear_region_request_arg;

typedef struct _raGL_buffer_client_memory_sourced_update_request_arg
{
    raGL_buffer                                                          buffer;
    bool                                                                 should_delete;
    bool                                                                 sync_other_contexts;
    std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > updates;
} _raGL_buffer_client_memory_sourced_update_request_arg;

typedef struct
{
    const ral_buffer_copy_to_buffer_info* copy_ops;
    raGL_buffer                           dst_buffer;
    uint32_t                              n_copy_ops;
    raGL_buffer                           src_buffer;
    bool                                  sync_other_contexts;
} _raGL_buffer_copy_buffer_to_buffer_request_arg;


/** Forward declarations */
PRIVATE void            _raGL_buffer_on_client_memory_sourced_update_request_gpu_scheduler_thread(void*                                                      callback_arg);
PRIVATE ral_present_job _raGL_buffer_on_client_memory_sourced_update_request_rendering_thread    (ral_context                                                context,
                                                                                                  void*                                                      callback_arg,
                                                                                                  const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE ral_present_job _raGL_buffer_on_clear_region_request_rendering_thread                    (ral_context                                                context,
                                                                                                  void*                                                      callback_arg,
                                                                                                  const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE ral_present_job _raGL_buffer_on_copy_buffer_to_buffer_update_request_rendering_thread    (ral_context                                                context,
                                                                                                  void*                                                      callback_arg,
                                                                                                  const ral_rendering_handler_rendering_callback_frame_data* unused);


/** TODO */
PRIVATE void _raGL_buffer_on_client_memory_sourced_update_request_gpu_scheduler_thread(void* callback_arg)
{
    _raGL_buffer_client_memory_sourced_update_request_arg* callback_arg_ptr    = static_cast<_raGL_buffer_client_memory_sourced_update_request_arg*>(callback_arg);
    ogl_context                                            current_context     = ogl_context_get_current_context();
    ral_context                                            current_context_ral = nullptr;

    ogl_context_get_property(current_context,
                              OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                             &current_context_ral);

    _raGL_buffer_on_client_memory_sourced_update_request_rendering_thread(current_context_ral,
                                                                          callback_arg_ptr,
                                                                          nullptr); /* unused */
}

/** TODO */
PRIVATE ral_present_job _raGL_buffer_on_client_memory_sourced_update_request_rendering_thread(ral_context                                                context,
                                                                                              void*                                                      callback_arg,
                                                                                              const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_buffer*                                             buffer_ptr          = nullptr;
    _raGL_buffer_client_memory_sourced_update_request_arg*    callback_arg_ptr    = reinterpret_cast<_raGL_buffer_client_memory_sourced_update_request_arg*>(callback_arg);
    ogl_context                                               context_gl          = nullptr;
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr = nullptr;

    buffer_ptr = reinterpret_cast<_raGL_buffer*>(callback_arg_ptr->buffer);

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    raGL_backend_sync();

    /* Bind the buffer object to ensure the latest BO contents is accessible to this context */
    for (auto current_update_ptr : callback_arg_ptr->updates)
    {
        entrypoints_dsa_ptr->pGLNamedBufferSubDataEXT(buffer_ptr->id,
                                                      buffer_ptr->start_offset + current_update_ptr->start_offset,
                                                      current_update_ptr->data_size,
                                                      current_update_ptr->data);

        if (current_update_ptr->pfn_op_finished_callback_proc != nullptr)
        {
            current_update_ptr->pfn_op_finished_callback_proc(current_update_ptr->op_finished_callback_user_arg);
        }
    }

    if (callback_arg_ptr->sync_other_contexts)
    {
        raGL_backend_enqueue_sync();
    }

    if (callback_arg_ptr->should_delete)
    {
        delete callback_arg_ptr;
    }

    /* We speak GL here. No need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_buffer_on_clear_region_request_rendering_thread(ral_context                                                context,
                                                                              void*                                                      callback_arg,
                                                                              const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_buffer*                          buffer_ptr       = nullptr;
    _raGL_buffer_clear_region_request_arg* callback_arg_ptr = reinterpret_cast<_raGL_buffer_clear_region_request_arg*>(callback_arg);
    ogl_context                            context_gl       = nullptr;
    const ogl_context_gl_entrypoints*      entrypoints_ptr  = nullptr;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    raGL_backend_sync();


    buffer_ptr = reinterpret_cast<_raGL_buffer*>(callback_arg_ptr->buffer);

    entrypoints_ptr->pGLBindBuffer(GL_COPY_WRITE_BUFFER,
                                   buffer_ptr->id);

    for (uint32_t n_clear_op = 0;
                  n_clear_op < callback_arg_ptr->n_clear_ops;
                ++n_clear_op)
    {
        const ral_buffer_clear_region_info& current_clear = callback_arg_ptr->clear_ops[n_clear_op];

        entrypoints_ptr->pGLClearBufferSubData(GL_COPY_WRITE_BUFFER,
                                               GL_R32UI,
                                               current_clear.offset,
                                               current_clear.size,
                                               GL_RED_INTEGER,
                                               GL_UNSIGNED_INT,
                                              &current_clear.clear_value);
    }

    if (callback_arg_ptr->sync_other_contexts)
    {
        raGL_backend_enqueue_sync();
    }

    /* We speak GL here. No need for a present job */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_buffer_on_copy_buffer_to_buffer_update_request_rendering_thread(ral_context                                                context_ral,
                                                                                              void*                                                      callback_arg,
                                                                                              const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_buffer_copy_buffer_to_buffer_request_arg*           callback_arg_ptr    = reinterpret_cast<_raGL_buffer_copy_buffer_to_buffer_request_arg*>(callback_arg);
    ogl_context                                               context_gl          = nullptr;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = nullptr;
    _raGL_buffer*                                             dst_buffer_ptr      = reinterpret_cast<_raGL_buffer*>(callback_arg_ptr->dst_buffer);
    _raGL_buffer*                                             src_buffer_ptr      = reinterpret_cast<_raGL_buffer*>(callback_arg_ptr->src_buffer);

    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);

    raGL_backend_sync();

    for (uint32_t n_copy_op = 0;
                  n_copy_op < callback_arg_ptr->n_copy_ops;
                ++n_copy_op)
    {
        const ral_buffer_copy_to_buffer_info& current_copy = callback_arg_ptr->copy_ops[n_copy_op];

        dsa_entrypoints_ptr->pGLNamedCopyBufferSubDataEXT(src_buffer_ptr->id,                                                         /* readBuffer  */
                                                          dst_buffer_ptr->id,                                                         /* writeBuffer */
                                                          src_buffer_ptr->start_offset + current_copy.src_buffer_region_start_offset, /* readOffset  */
                                                          dst_buffer_ptr->start_offset + current_copy.dst_buffer_region_start_offset, /* writeOffset */
                                                          current_copy.region_size);
    }

    if (callback_arg_ptr->sync_other_contexts)
    {
        raGL_backend_enqueue_sync();
    }

    /* We speak GL here, no need for a present job */
    return nullptr;
}

/** Please see header for specification */
PUBLIC void raGL_buffer_clear_region(raGL_buffer                         buffer_raGL,
                                     uint32_t                            n_clear_ops,
                                     const ral_buffer_clear_region_info* clear_ops,
                                     bool                                sync_other_contexts)
{
    _raGL_buffer*                         buffer_ptr      = reinterpret_cast<_raGL_buffer*>(buffer_raGL);
    _raGL_buffer_clear_region_request_arg callback_arg;
    ogl_context                           context_gl        = ogl_context_get_current_context();
    ral_context                           context_ral       = nullptr;
    ral_rendering_handler                 rendering_handler = nullptr;

    callback_arg.buffer              = buffer_raGL;
    callback_arg.clear_ops           = clear_ops;
    callback_arg.n_clear_ops         = n_clear_ops;
    callback_arg.sync_other_contexts = sync_other_contexts;

    if (buffer_ptr->context != nullptr)
    {
        context_gl = buffer_ptr->context;
    }

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                     _raGL_buffer_on_clear_region_request_rendering_thread,
                                                    &callback_arg,
                                                     false); /* present_after_executed */
}

/** Please see header for specification */
PUBLIC void raGL_buffer_copy_buffer_to_buffer(raGL_buffer                           dst_buffer_raGL,
                                              raGL_buffer                           src_buffer_raGL,
                                              uint32_t                              n_copy_ops,
                                              const ral_buffer_copy_to_buffer_info* copy_ops,
                                              bool                                  sync_other_contexts)
{
    _raGL_buffer_copy_buffer_to_buffer_request_arg callback_arg;
    ogl_context                                    current_context     = ogl_context_get_current_context();
    ral_context                                    current_context_ral = nullptr;
    ral_rendering_handler                          current_context_rh  = nullptr;
    _raGL_buffer*                                  dst_buffer_ptr      = reinterpret_cast<_raGL_buffer*>(dst_buffer_raGL);
    _raGL_buffer*                                  src_buffer_ptr      = reinterpret_cast<_raGL_buffer*>(src_buffer_raGL);

    callback_arg.copy_ops            = copy_ops;
    callback_arg.dst_buffer          = dst_buffer_raGL;
    callback_arg.n_copy_ops          = n_copy_ops;
    callback_arg.src_buffer          = src_buffer_raGL;
    callback_arg.sync_other_contexts = sync_other_contexts;

    if (current_context != dst_buffer_ptr->context)
    {
        current_context = dst_buffer_ptr->context;
    }

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &current_context_ral);
    ral_context_get_property(current_context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                             &current_context_rh);

    ral_rendering_handler_request_rendering_callback(current_context_rh,
                                                     _raGL_buffer_on_copy_buffer_to_buffer_update_request_rendering_thread,
                                                    &callback_arg,
                                                     false); /* present_after_executed */
}

/** Please see header for specification */
PUBLIC raGL_buffer raGL_buffer_create(ogl_context           context,
                                      GLint                 id,
                                      system_memory_manager memory_manager,
                                      uint32_t              start_offset,
                                      uint32_t              size)
{
    _raGL_buffer* new_buffer_ptr = new (std::nothrow) _raGL_buffer(nullptr,    /* in_parent_buffer_ptr */
                                                                   context,
                                                                   id,
                                                                   memory_manager,
                                                                   start_offset,
                                                                   size);

    ASSERT_ALWAYS_SYNC(new_buffer_ptr != nullptr,
                       "Out of memory");

    return (raGL_buffer) new_buffer_ptr;
}

/** Please see header for specification */
PUBLIC raGL_buffer raGL_buffer_create_raGL_buffer_subregion(raGL_buffer parent_buffer,
                                                            uint32_t    start_offset,
                                                            uint32_t    size)
{
    _raGL_buffer* parent_buffer_ptr = reinterpret_cast<_raGL_buffer*>(parent_buffer);
    _raGL_buffer* result_ptr        = nullptr;

    uint32_t new_size         = size;
    uint32_t new_start_offset = 0;

    /* Sanity checks */
    if (parent_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Parent raGL_buffer instance is nullptr");

        goto end;
    }

    if (new_size == 0)
    {
        ASSERT_DEBUG_SYNC(new_size != 0,
                          "raGL_buffer_create_raGL_buffer_subregion() was passed a size of 0.");

        goto end;
    }

    /* Determine topmost parent buffer */
    new_start_offset = start_offset;

    while (parent_buffer_ptr != nullptr)
    {
        new_start_offset += parent_buffer_ptr->start_offset;

        if (parent_buffer_ptr->parent_buffer_ptr != nullptr)
        {
            parent_buffer_ptr = parent_buffer_ptr->parent_buffer_ptr;
        }
        else
        {
            break;
        }
    }

    if (new_start_offset >= parent_buffer_ptr->size)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Start offset exceeds available storage size.");

        goto end;
    }

    if (new_start_offset + new_size > parent_buffer_ptr->size)
    {
        ASSERT_DEBUG_SYNC(false,
                          "End offset exceeds available storage size.");

        goto end;
    }

    /* Create a new descriptor */
    result_ptr = new (std::nothrow) _raGL_buffer(parent_buffer_ptr,
                                                 parent_buffer_ptr->context,
                                                 parent_buffer_ptr->id,
                                                 parent_buffer_ptr->memory_manager,
                                                 new_start_offset,
                                                 new_size);

    if (result_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

end:
    return (raGL_buffer) result_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_buffer_get_property(raGL_buffer          buffer,
                                     raGL_buffer_property property,
                                     void*                out_result_ptr)
{
    _raGL_buffer* buffer_ptr = reinterpret_cast<_raGL_buffer*>(buffer);

    /* Sanity checks */
    if (buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_buffer instance is nullptr");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAGL_BUFFER_PROPERTY_ID:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = buffer_ptr->id;

            break;
        }

        case RAGL_BUFFER_PROPERTY_MEMORY_MANAGER:
        {
            *reinterpret_cast<system_memory_manager*>(out_result_ptr) = buffer_ptr->memory_manager;

            break;
        }

        case RAGL_BUFFER_PROPERTY_SIZE:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = buffer_ptr->size;

            break;
        }

        case RAGL_BUFFER_PROPERTY_START_OFFSET:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = buffer_ptr->start_offset;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_buffer_property value.");
        }
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC void raGL_buffer_release(raGL_buffer buffer)
{
    _raGL_buffer* buffer_ptr = reinterpret_cast<_raGL_buffer*>(buffer);

    ASSERT_DEBUG_SYNC(buffer != nullptr,
                      "Input buffer is nullptr");

    delete reinterpret_cast<_raGL_buffer*>(buffer);
}

/** Please see header for specification */
PUBLIC void raGL_buffer_update_regions_with_client_memory(raGL_buffer                                                          buffer,
                                                          std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > updates,
                                                          bool                                                                 async,
                                                          bool                                                                 sync_other_contexts)
{
    _raGL_buffer* buffer_ptr      = reinterpret_cast<_raGL_buffer*>(buffer);
    ogl_context   current_context = ogl_context_get_current_context();

    if (current_context == nullptr)
    {
        current_context = buffer_ptr->context;
    }

    if (async)
    {
        ral_backend_type                                       backend_type;
        _raGL_buffer_client_memory_sourced_update_request_arg* callback_arg_ptr = new _raGL_buffer_client_memory_sourced_update_request_arg();
        ral_scheduler_job_info                                 job_info;
        ral_scheduler                                          scheduler        = nullptr;

        demo_app_get_property   (DEMO_APP_PROPERTY_GPU_SCHEDULER,
                                &scheduler);
        ogl_context_get_property(buffer_ptr->context,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        callback_arg_ptr->buffer              = buffer;
        callback_arg_ptr->should_delete       = true;
        callback_arg_ptr->sync_other_contexts = sync_other_contexts;
        callback_arg_ptr->updates             = updates;

        job_info.job_type                            = RAL_SCHEDULER_JOB_TYPE_CALLBACK;
        job_info.callback_job_args.callback_user_arg = callback_arg_ptr;
        job_info.callback_job_args.pfn_callback_proc = _raGL_buffer_on_client_memory_sourced_update_request_gpu_scheduler_thread;

        ral_scheduler_schedule_job(scheduler,
                                   backend_type,
                                   job_info);
    }
    else
    {
        _raGL_buffer_client_memory_sourced_update_request_arg callback_arg;
        ral_context                                           context_ral  = nullptr;
        ral_rendering_handler                                 context_rh   = nullptr;

        ogl_context_get_property(current_context,
                                 OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                                &context_ral);
        ral_context_get_property(context_ral,
                                 RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                &context_rh);

        callback_arg.buffer              = buffer;
        callback_arg.should_delete       = false;
        callback_arg.sync_other_contexts = sync_other_contexts;
        callback_arg.updates             = updates;

        ral_rendering_handler_request_rendering_callback(context_rh,
                                                         _raGL_buffer_on_client_memory_sourced_update_request_rendering_thread,
                                                        &callback_arg,
                                                         false); /* present_after_executed */
    }
}