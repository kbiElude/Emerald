/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "demo/demo_app.h"
#include "demo/demo_loader.h"
#include "demo/demo_window.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_shader.h"
#include "scene/scene.h"
#include "scene/scene_multiloader.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"

typedef struct _demo_loader_op
{
    union
    {
        demo_loader_op_build_program                     build_program;
        demo_loader_op_configure_timeline                configure_timeline;
        demo_loader_op_load_audio_stream                 load_audio_stream;
        demo_loader_op_load_scenes                       load_scenes;
        demo_loader_op_render_animation                  render_animation;
        demo_loader_op_render_frame                      render_frame;
        demo_loader_op_request_rendering_thread_callback request_rendering_thread_callback;
    } data;

    demo_loader_op type;


    _demo_loader_op(demo_loader_op_build_program* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        if (op_ptr->cs.body != nullptr)
        {
            ASSERT_DEBUG_SYNC(op_ptr->fs.body == nullptr &&
                              op_ptr->gs.body == nullptr &&
                              op_ptr->tc.body == nullptr &&
                              op_ptr->te.body == nullptr &&
                              op_ptr->vs.body == nullptr,
                              "If compute shader body is defined, bodies for all other shader stages must be NULL");
        }
        else
        {
            ASSERT_DEBUG_SYNC(op_ptr->fs.body != nullptr ||
                              op_ptr->gs.body != nullptr ||
                              op_ptr->tc.body != nullptr ||
                              op_ptr->te.body != nullptr ||
                              op_ptr->vs.body != nullptr,
                              "No body defined for any of the shader stages");
        }

        type = DEMO_LOADER_OP_BUILD_PROGRAM;
    }

    _demo_loader_op(demo_loader_op_configure_timeline* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        type = DEMO_LOADER_OP_CONFIGURE_TIMELINE;
    }

    _demo_loader_op(demo_loader_op_load_audio_stream* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        type = DEMO_LOADER_OP_LOAD_AUDIO_STREAM;
    }

    _demo_loader_op(demo_loader_op_load_scenes* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        type = DEMO_LOADER_OP_LOAD_SCENES;
    }

    _demo_loader_op(demo_loader_op_render_animation* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        type = DEMO_LOADER_OP_RENDER_ANIMATION;
    }

    _demo_loader_op(demo_loader_op_render_frame* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        type = DEMO_LOADER_OP_RENDER_FRAME;
    }

    _demo_loader_op(demo_loader_op_request_rendering_thread_callback* op_ptr)
    {
        memcpy(&data,
               op_ptr,
               sizeof(*op_ptr) );

        type = DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK;
    }
} _demo_loader_op;

typedef struct _demo_loader
{
    ral_context             context;      /* DO NOT retain or release */
    system_resizable_vector enqueued_ops; /* each item is a _demo_loader_op instance */
    bool                    has_been_launched;
    system_resizable_vector loaded_objects[DEMO_LOADER_OBJECT_TYPE_COUNT];

    explicit _demo_loader(ral_context in_context)
    {
        context           = in_context;
        enqueued_ops      = system_resizable_vector_create(16 /* capacity */);
        has_been_launched = false;

        for (uint32_t object_type_index = 0;
                      object_type_index < (uint32_t) DEMO_LOADER_OBJECT_TYPE_COUNT;
                    ++object_type_index)
        {
            loaded_objects[object_type_index] = system_resizable_vector_create(4 /* capacity */);
        }
    }

    ~_demo_loader()
    {
        if (enqueued_ops != nullptr)
        {
            _demo_loader_op* enqueued_op_ptr = nullptr;

            /* Release the operations in reversed order. Each operation can have a teardown func ptr associated.
             * If one is present, we need to call it now so that related assets, objects, etc. can be released.
             */
            while (system_resizable_vector_pop(enqueued_ops,
                                              &enqueued_op_ptr) )
            {
                switch (enqueued_op_ptr->type)
                {
                    case DEMO_LOADER_OP_BUILD_PROGRAM:
                    {
                        if (enqueued_op_ptr->data.build_program.pfn_teardown_func_ptr != nullptr)
                        {
                            enqueued_op_ptr->data.build_program.pfn_teardown_func_ptr(enqueued_op_ptr->data.build_program.teardown_callback_user_arg);
                        }

                        break;
                    }

                    case DEMO_LOADER_OP_LOAD_AUDIO_STREAM:
                    {
                        if (enqueued_op_ptr->data.load_audio_stream.pfn_teardown_func_ptr != nullptr)
                        {
                            enqueued_op_ptr->data.load_audio_stream.pfn_teardown_func_ptr(enqueued_op_ptr->data.load_audio_stream.teardown_callback_user_arg);
                        }

                        break;
                    }

                    case DEMO_LOADER_OP_CONFIGURE_TIMELINE:
                    case DEMO_LOADER_OP_RENDER_ANIMATION:
                    case DEMO_LOADER_OP_RENDER_FRAME:
                    {
                        /* No tear-down func ptr stored for the ops. */
                        break;
                    }

                    case DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK:
                    {
                        if (enqueued_op_ptr->data.request_rendering_thread_callback.pfn_teardown_func_ptr != nullptr)
                        {
                            enqueued_op_ptr->data.request_rendering_thread_callback.pfn_teardown_func_ptr(enqueued_op_ptr->data.request_rendering_thread_callback.teardown_callback_user_arg);
                        }

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized loader op type");
                    }
                }

                delete enqueued_op_ptr;
                enqueued_op_ptr = nullptr;
            }

            system_resizable_vector_release(enqueued_ops);
            enqueued_ops = nullptr;
        }

        for (uint32_t object_type_index = 0;
                      object_type_index < DEMO_LOADER_OBJECT_TYPE_COUNT;
                    ++object_type_index)
        {
            if (loaded_objects[object_type_index] != nullptr)
            {
                uint32_t n_loaded_objects = 0;

                system_resizable_vector_get_property(loaded_objects[object_type_index],
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_loaded_objects);

                for (uint32_t n_loaded_object = 0;
                              n_loaded_object < n_loaded_objects;
                            ++n_loaded_object)
                {
                    demo_loader_release_object_by_index( (demo_loader)             this,
                                                         (demo_loader_object_type) object_type_index,
                                                        n_loaded_object);
                }

                system_resizable_vector_release(loaded_objects[object_type_index]);
                loaded_objects[object_type_index] = nullptr;
            }
        }
    }
} _demo_loader;


/** Please see header for specification */
PUBLIC demo_loader demo_loader_create(ral_context context)
{
    _demo_loader* new_loader_ptr = new (std::nothrow) _demo_loader(context);

    ASSERT_DEBUG_SYNC(new_loader_ptr != nullptr,
                      "Out of memory");

    return (demo_loader) new_loader_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_loader_enqueue_operation(demo_loader    loader,
                                                      demo_loader_op operation,
                                                      void*          operation_arg)
{
    _demo_loader*    loader_ptr = reinterpret_cast<_demo_loader*>(loader);
    _demo_loader_op* new_op_ptr = nullptr;

    ASSERT_DEBUG_SYNC(!loader_ptr->has_been_launched,
                      "Cannot enqueue ops after the loader has been launched!");

    switch (operation)
    {
        case DEMO_LOADER_OP_BUILD_PROGRAM:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_build_program*>(operation_arg) );

            break;
        }

        case DEMO_LOADER_OP_CONFIGURE_TIMELINE:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_configure_timeline*>(operation_arg) );

            break;
        }

        case DEMO_LOADER_OP_LOAD_AUDIO_STREAM:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_load_audio_stream*>(operation_arg) );

            break;
        }

        case DEMO_LOADER_OP_LOAD_SCENES:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_load_scenes*>(operation_arg) );

            break;
        }

        case DEMO_LOADER_OP_RENDER_ANIMATION:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_render_animation*>(operation_arg) );

            break;
        }

        case DEMO_LOADER_OP_RENDER_FRAME:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_render_frame*>(operation_arg) );

            break;
        }

        case DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK:
        {
            new_op_ptr = new (std::nothrow) _demo_loader_op(reinterpret_cast<demo_loader_op_request_rendering_thread_callback*>(operation_arg) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo loader operation");
        }
    }

    ASSERT_DEBUG_SYNC(new_op_ptr != nullptr,
                      "Out of memory");
    if (new_op_ptr != nullptr)
    {
        system_resizable_vector_push(loader_ptr->enqueued_ops,
                                     new_op_ptr);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_loader_get_object_by_index(demo_loader             loader,
                                                        demo_loader_object_type object_type,
                                                        uint32_t                index,
                                                        void*                   out_result_ptr)
{
    _demo_loader* loader_ptr = reinterpret_cast<_demo_loader*>(loader);
    bool          result;

    result = system_resizable_vector_get_element_at(loader_ptr->loaded_objects[object_type],
                                                    index,
                                                    out_result_ptr);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_loader_get_property(demo_loader          loader,
                                                 demo_loader_property property,
                                                 void*                out_data_ptr)
{
    _demo_loader* loader_ptr = reinterpret_cast<_demo_loader*>(loader);

    switch (property)
    {
        case DEMO_LOADER_PROPERTY_N_LOADED_AUDIO_STREAMS:
        {
            system_resizable_vector_get_property(loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM],
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_data_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_loader_property value.");
        }
    }
}

/** Please see header for specification */
PUBLIC void demo_loader_release(demo_loader loader)
{
    delete reinterpret_cast<_demo_loader*>(loader);
}

/** Please see header for specification */
PUBLIC void demo_loader_release_object_by_index(demo_loader             loader,
                                                demo_loader_object_type object_type,
                                                uint32_t                object_index)
{
    _demo_loader* loader_ptr  = reinterpret_cast<_demo_loader*>(loader);
    void*         object      = nullptr;
    const void*   object_null = nullptr;

    if (system_resizable_vector_get_element_at(loader_ptr->loaded_objects[object_type],
                                               object_index,
                                              &object) )
    {
        if (object != nullptr)
        {
            switch (object_type)
            {
                case DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM:
                {
                    audio_stream_release( (audio_stream&) object);

                    break;
                }

                case DEMO_LOADER_OBJECT_TYPE_PROGRAM:
                {
                    ral_program_release( (ral_program&) object);

                    break;
                }

                case DEMO_LOADER_OBJECT_TYPE_SCENE:
                {
                    scene_release( (scene&) object);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized object type");
                }
            }

            system_resizable_vector_set_element_at(loader_ptr->loaded_objects[object_type],
                                                   object_index,
                                                   (void*) object_null);
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve the descriptor of the requested object.");
    }
}

/** Please see header for specification */
PUBLIC void demo_loader_run(demo_loader   loader,
                            demo_timeline timeline,
                            demo_window   window)
{
    _demo_loader* loader_ptr     = reinterpret_cast<_demo_loader*>(loader);
    uint32_t      n_enqueued_ops = 0;
    system_window window_private = nullptr;

    ASSERT_DEBUG_SYNC(!loader_ptr->has_been_launched,
                      "Loader should not be run more than once");

    LOG_INFO("Loader started..");

    demo_window_get_private_property(window,
                                     DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW,
                                    &window_private);

    /* Kick off */
    loader_ptr->has_been_launched = true;

    system_resizable_vector_get_property(loader_ptr->enqueued_ops,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_enqueued_ops);

    for (uint32_t n_enqueued_op = 0;
                  n_enqueued_op < n_enqueued_ops;
                ++n_enqueued_op)
    {
        _demo_loader_op* op_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(loader_ptr->enqueued_ops,
                                                    n_enqueued_op,
                                                   &op_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve an enqueued loader op at index [%d]",
                              n_enqueued_op);

            continue;
        }

        switch (op_ptr->type)
        {
            case DEMO_LOADER_OP_BUILD_PROGRAM:
            {
                ral_context context     = nullptr;
                ral_program new_program = nullptr;

                const ral_program_create_info po_create_info =
                {
                    static_cast<ral_program_shader_stage_bits>(((op_ptr->data.build_program.cs.body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE         : 0) |
                                                               ((op_ptr->data.build_program.fs.body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT        : 0) |
                                                               ((op_ptr->data.build_program.gs.body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_GEOMETRY        : 0) |
                                                               ((op_ptr->data.build_program.tc.body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_TESS_CONTROL    : 0) |
                                                               ((op_ptr->data.build_program.te.body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_TESS_EVALUATION : 0) |
                                                               ((op_ptr->data.build_program.vs.body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX          : 0)),
                    op_ptr->data.build_program.name
                };

                system_window_get_property(window_private,
                                           SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                                          &context);

                if (!ral_context_create_programs(loader_ptr->context,
                                                 1, /* n_create_info_items */
                                                &po_create_info,
                                                &new_program) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not create a new RAL program.");

                    continue;
                }

                struct _shader
                {
                    demo_loader_op_build_program_shader* data_ptr;
                    ral_shader_type                      type;
                } shaders[] =
                {
                    {&op_ptr->data.build_program.cs, RAL_SHADER_TYPE_COMPUTE},
                    {&op_ptr->data.build_program.fs, RAL_SHADER_TYPE_FRAGMENT},
                    {&op_ptr->data.build_program.gs, RAL_SHADER_TYPE_GEOMETRY},
                    {&op_ptr->data.build_program.tc, RAL_SHADER_TYPE_TESSELLATION_CONTROL},
                    {&op_ptr->data.build_program.te, RAL_SHADER_TYPE_TESSELLATION_EVALUATION},
                    {&op_ptr->data.build_program.vs, RAL_SHADER_TYPE_VERTEX}
                };
                const uint32_t n_shaders = sizeof(shaders) / sizeof(shaders[0]);

                for (uint32_t n_shader = 0;
                              n_shader < n_shaders;
                            ++n_shader)
                {
                    const _shader& current_shader = shaders[n_shader];

                    ASSERT_DEBUG_SYNC(current_shader.data_ptr != nullptr,
                                      "Null _shader pointer");

                    if (current_shader.data_ptr->body != nullptr)
                    {
                        ral_shader                      new_shader         = nullptr;
                        const system_hashed_ansi_string shader_body        = system_hashed_ansi_string_create_by_token_replacement(current_shader.data_ptr->body,
                                                                                                                                   current_shader.data_ptr->n_tokens,
                                                                                                                                   current_shader.data_ptr->token_keys,
                                                                                                                                   current_shader.data_ptr->token_values);
                        const ral_shader_create_info    shader_create_info =
                        {
                            current_shader.data_ptr->name,
                            current_shader.type
                        };

                        if (!ral_context_create_shaders(loader_ptr->context,
                                                        1, /* n_create_info_items */
                                                       &shader_create_info,
                                                       &new_shader) )
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Cpuld not create a new RAL shader.");

                            continue;
                        }

                        ral_shader_set_property(new_shader,
                                                RAL_SHADER_PROPERTY_GLSL_BODY,
                                               &shader_body);

                        if (!ral_program_attach_shader(new_program,
                                                       new_shader,
                                                       true /* async */) )
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Failed to attach a RAL shader to a RAL program.");
                        }

                        ral_context_delete_objects(loader_ptr->context,
                                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                                   1, /* n_objects */
                                                   (const void**) &new_shader);
                    }
                }

                if (new_program != nullptr)
                {
                    system_resizable_vector_get_property(loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_PROGRAM],
                                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                         op_ptr->data.build_program.result_program_index_ptr);

                    system_resizable_vector_push(loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_PROGRAM],
                                                 new_program);

                    if (op_ptr->data.build_program.pfn_on_link_finished_callback_proc != nullptr)
                    {
                        op_ptr->data.build_program.pfn_on_link_finished_callback_proc(new_program,
                                                                                      op_ptr->data.build_program.on_link_finished_callback_user_arg);
                    }
                }

                break;
            }

            case DEMO_LOADER_OP_CONFIGURE_TIMELINE:
            {
                op_ptr->data.configure_timeline.pfn_configure_timeline_proc(timeline,
                                                                            op_ptr->data.configure_timeline.user_arg);

                break;
            }

            case DEMO_LOADER_OP_LOAD_AUDIO_STREAM:
            {
                system_file_serializer audio_serializer = nullptr;
                audio_stream           new_audio_stream = nullptr;

                audio_serializer = system_file_serializer_create_for_reading(op_ptr->data.load_audio_stream.audio_file_name);

                ASSERT_ALWAYS_SYNC(audio_serializer != nullptr,
                                   "Could not open [%s]",
                                   system_hashed_ansi_string_get_buffer(op_ptr->data.load_audio_stream.audio_file_name) );

                new_audio_stream = audio_stream_create(audio_device_get_default_device(),
                                                       audio_serializer,
                                                       window);

                ASSERT_ALWAYS_SYNC(new_audio_stream != nullptr,
                                   "Could not create an audio_stream instance.");

                /* NOTE: The audio stream will be bound to the window after the loader part finishes executing.
                 *       This needs to be postponed, because otherwise if the client wanted to start playback
                 *       during the loading process, the music would have also started (prematurely) playing.
                 */

                /* Cache the audio stream */
                if (op_ptr->data.load_audio_stream.result_audio_stream_index_ptr != nullptr)
                {
                    system_resizable_vector_get_property(loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM],
                                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                         op_ptr->data.load_audio_stream.result_audio_stream_index_ptr);
                }

                system_resizable_vector_push(loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM],
                                             new_audio_stream);

                /* Release stuff we no longer need. */
                system_file_serializer_release(audio_serializer);

                break;
            }

            case DEMO_LOADER_OP_LOAD_SCENES:
            {
                scene_multiloader       multiloader       = nullptr;
                system_file_serializer* scene_serializers = nullptr;

                ASSERT_DEBUG_SYNC(op_ptr->data.load_scenes.n_scenes >= 1,
                                  "Invalid number of scenes requested for DEMO_LOADER_OP_LOAD_SCENES operation.");

                multiloader = scene_multiloader_create_from_filenames(loader_ptr->context,
                                                                      op_ptr->data.load_scenes.n_scenes,
                                                                      op_ptr->data.load_scenes.scene_file_names);

                ASSERT_DEBUG_SYNC(multiloader != nullptr,
                                  "Could not spawn a scene_multiloader instance");

                scene_multiloader_load_async         (multiloader);
                scene_multiloader_wait_until_finished(multiloader);

                /* Store the loaded scenes */
                for (uint32_t n_scene = 0;
                              n_scene < op_ptr->data.load_scenes.n_scenes;
                            ++n_scene)
                {
                    scene result_scene = nullptr;

                    scene_multiloader_get_loaded_scene(multiloader,
                                                       n_scene,
                                                      &result_scene);

                    ASSERT_DEBUG_SYNC(result_scene != nullptr,
                                      "scene_multiloader returned a NULL scene handle");

                    system_resizable_vector_get_property(loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_SCENE],
                                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                         op_ptr->data.load_scenes.result_scene_indices_ptr + n_scene);
                    system_resizable_vector_push        (loader_ptr->loaded_objects[DEMO_LOADER_OBJECT_TYPE_SCENE],
                                                         result_scene);
                }

                break;
            }

            case DEMO_LOADER_OP_RENDER_ANIMATION:
            {
                system_event          playback_in_progress_event = nullptr;
                system_event          playback_stopped_event     = nullptr;
                ral_rendering_handler rendering_handler          = nullptr;

                system_window_get_property        (window_private,
                                                   SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                                  &rendering_handler);
                ral_rendering_handler_get_property(rendering_handler,
                                                   RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_IN_PROGRESS_EVENT,
                                                  &playback_in_progress_event);
                ral_rendering_handler_get_property(rendering_handler,
                                                   RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STOPPED_EVENT,
                                                  &playback_stopped_event);

                ral_rendering_handler_set_property(rendering_handler,
                                                   RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                                  &op_ptr->data.render_animation.pfn_rendering_callback_proc);
                ral_rendering_handler_set_property(rendering_handler,
                                                   RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK_USER_ARGUMENT,
                                                  &op_ptr->data.render_animation.user_arg);

                /* Kick off the playback and block until it stops */
                ral_rendering_handler_play(rendering_handler,
                                           0); /* start_time */

                system_event_wait_single(playback_in_progress_event);
                system_event_wait_single(playback_stopped_event);

                break;
            }

            case DEMO_LOADER_OP_RENDER_FRAME:
            {
                ral_rendering_handler rendering_handler = nullptr;

                system_window_get_property(window_private,
                                           SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                          &rendering_handler);

                ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                                 op_ptr->data.render_frame.pfn_rendering_callback_proc,
                                                                 op_ptr->data.render_frame.user_arg,
                                                                 true); /* swap_buffers_afterward */

                break;
            }

            case DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK:
            {
                ral_rendering_handler rendering_handler = nullptr;

                system_window_get_property(window_private,
                                           SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                          &rendering_handler);

                ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                                 op_ptr->data.request_rendering_thread_callback.pfn_rendering_callback_proc,
                                                                 op_ptr->data.request_rendering_thread_callback.user_arg,
                                                                 op_ptr->data.request_rendering_thread_callback.should_swap_buffers);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized loader op type");
            }
        }
    }

    LOG_INFO("Loader finished");
}
