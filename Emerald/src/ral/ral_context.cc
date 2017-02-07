/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_program.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_pool.h"
#include "ral/ral_texture_view.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"

typedef void (*PFNRALBACKENDGETPROPERTYPROC)   (void*                      backend,
                                                ral_context_property       property,
                                                void*                      out_result_ptr);
typedef void (*PFNRALBACKENDRELEASEPROC)       (void*                      backend);


typedef struct _ral_context
{
    /* Rendering back-end instance:
     *
     * OpenGL context:    raGL_backend instance
     * OpenGL ES context: raGL_backend instance
     **/
    void*                              backend;
    ral_backend_type                   backend_type;
    PFNRALBACKENDGETPROPERTYPROC       pfn_backend_get_property_proc;
    PFNRALBACKENDRELEASEPROC           pfn_backend_release_proc;

    system_callback_manager callback_manager;

    system_resizable_vector buffers; /* owns ral_buffer instances */
    system_critical_section buffers_cs;
    uint32_t                n_buffers_created;

    system_resizable_vector command_buffers; /* owns ral_command_buffer instances */
    system_critical_section command_buffers_cs;
    uint32_t                n_command_buffers_created;

    system_resizable_vector gfx_states; /* owns ral_gfx_state instances */
    system_critical_section gfx_states_cs;
    uint32_t                n_gfx_states_created;

    system_resizable_vector programs; /* owns ral_framebuffer instances */
    system_critical_section programs_cs;
    system_hash64map        programs_by_name_map; /* does NOT own the ral_program values */
    uint32_t                n_programs_created;

    system_resizable_vector samplers; /* owns ral_sampler instances */
    system_critical_section samplers_cs;
    uint32_t                n_samplers_created;

    system_resizable_vector shaders;             /* owns ral_shader instances          */
    system_hash64map        shaders_by_name_map; /* does NOT own the ral_shader values */
    system_critical_section shaders_cs;
    uint32_t                n_shaders_created;

    ral_texture_pool        texture_pool;             /* NOT owned - this instance is provided & owned by the backend! */
    system_hash64map        textures_by_filename_map; /* does NOT own the ral_texture values */
    system_hash64map        textures_by_name_map;     /* does NOT own the ral_texture values */
    system_critical_section textures_cs;
    uint32_t                n_textures_created;

    system_resizable_vector texture_views;        /* owns ral_texture_view instances */
    system_critical_section texture_views_cs;
    uint32_t                n_texture_views_created;

    system_critical_section object_to_refcount_cs;
    system_hash64map        object_to_refcount_map;

    system_hashed_ansi_string name;
    ral_rendering_handler     rendering_handler;
    demo_window               window_demo;
    system_window             window_system;

    REFCOUNT_INSERT_VARIABLES;


    _ral_context(system_hashed_ansi_string in_name,
                 demo_window               in_window)
    {
        backend                   = nullptr;
        backend_type              = RAL_BACKEND_TYPE_UNKNOWN;
        buffers                   = system_resizable_vector_create(sizeof(ral_buffer) );
        buffers_cs                = system_critical_section_create();
        callback_manager          = system_callback_manager_create((_callback_id) RAL_CONTEXT_CALLBACK_ID_COUNT);
        command_buffers           = system_resizable_vector_create(sizeof(ral_command_buffer) );
        command_buffers_cs        = system_critical_section_create();
        gfx_states                = system_resizable_vector_create(sizeof(ral_gfx_state) );
        gfx_states_cs             = system_critical_section_create();
        n_buffers_created         = 0;
        n_command_buffers_created = 0;
        n_gfx_states_created      = 0;
        n_samplers_created        = 0;
        n_textures_created        = 0;
        n_texture_views_created   = 0;
        name                      = in_name;
        object_to_refcount_cs     = system_critical_section_create();
        object_to_refcount_map    = system_hash64map_create       (sizeof(uint32_t) );
        rendering_handler         = nullptr;
        programs                  = system_resizable_vector_create(sizeof(ral_program) );
        programs_by_name_map      = system_hash64map_create       (sizeof(ral_program) );
        programs_cs               = system_critical_section_create();
        samplers                  = system_resizable_vector_create(sizeof(ral_sampler) );
        samplers_cs               = system_critical_section_create();
        shaders                   = system_resizable_vector_create(sizeof(ral_shader) );
        shaders_by_name_map       = system_hash64map_create       (sizeof(ral_shader) );
        shaders_cs                = system_critical_section_create();
        texture_views             = system_resizable_vector_create(sizeof(ral_texture_view) );
        texture_views_cs          = system_critical_section_create();
        texture_pool              = nullptr;
        textures_by_filename_map  = system_hash64map_create       (sizeof(ral_texture) );
        textures_by_name_map      = system_hash64map_create       (sizeof(ral_texture) );
        textures_cs               = system_critical_section_create();
        window_demo               = in_window;
        window_system             = nullptr;

        pfn_backend_get_property_proc = nullptr;

        demo_window_get_private_property(in_window,
                                         DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW,
                                        &window_system);
        system_window_get_property      (window_system,
                                         SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                        &rendering_handler);
    }

    ~_ral_context()
    {
        system_critical_section* cses_to_release[] =
        {
            &buffers_cs,
            &command_buffers_cs,
            &gfx_states_cs,
            &object_to_refcount_cs,
            &programs_cs,
            &samplers_cs,
            &shaders_cs,
            &texture_views_cs
        };
        system_hash64map* hashmaps_to_release[] =
        {
            &object_to_refcount_map,
            &programs_by_name_map,
            &shaders_by_name_map,
            &textures_by_filename_map,
            &textures_by_name_map
        };
        system_resizable_vector* vectors_to_release[] =
        {
            &buffers,
            &command_buffers,
            &gfx_states,
            &programs,
            &samplers,
            &shaders,
            &texture_views
        };

        const uint32_t n_cses_to_release     = sizeof(cses_to_release)     / sizeof(cses_to_release    [0]);
        const uint32_t n_hashmaps_to_release = sizeof(hashmaps_to_release) / sizeof(hashmaps_to_release[0]);
        const uint32_t n_vectors_to_release  = sizeof(vectors_to_release)  / sizeof(vectors_to_release [0]);

        for (uint32_t n_cs_to_release = 0;
                      n_cs_to_release < n_cses_to_release;
                    ++n_cs_to_release)
        {
            system_critical_section* current_cs_ptr = cses_to_release[n_cs_to_release];

            if (*current_cs_ptr != nullptr)
            {
                system_critical_section_release(*current_cs_ptr);

                *current_cs_ptr = nullptr;
            }
        }

        for (uint32_t n_hashmap_to_release = 0;
                      n_hashmap_to_release < n_hashmaps_to_release;
                    ++n_hashmap_to_release)
        {
            system_hash64map* current_hashmap_ptr = hashmaps_to_release[n_hashmap_to_release];

            if (*current_hashmap_ptr != nullptr)
            {
                system_hash64map_release(*current_hashmap_ptr);

                *current_hashmap_ptr = nullptr;
            }
        }

        for (uint32_t n_vector_to_release = 0;
                      n_vector_to_release < n_vectors_to_release;
                    ++n_vector_to_release)
        {
            system_resizable_vector* current_vector_ptr = vectors_to_release[n_vector_to_release];
            uint32_t                 n_objects          = 0;

            if (*current_vector_ptr != nullptr)
            {
                system_resizable_vector_get_property(*current_vector_ptr,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_objects);

                ASSERT_DEBUG_SYNC(n_objects == 0,
                                  "Memory leak detected");

                system_resizable_vector_release(*current_vector_ptr);
                *current_vector_ptr = nullptr;
            }
        }

        /* Callback manager needs to be deleted at the end. */
        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }
    }
} _ral_context;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_context,
                               ral_context,
                              _ral_context);

/* Forward declarations */
PRIVATE void _ral_context_delete_textures_from_texture_hashmaps(_ral_context*           context_ptr,
                                                                uint32_t                n_textures,
                                                                ral_texture*            textures);
PRIVATE void _ral_context_notify_backend_about_new_object      (ral_context             context,
                                                                void*                   result_object,
                                                                ral_context_object_type object_type);


/* TODO */
PRIVATE void _ral_context_add_programs_to_program_hashmap(_ral_context* context_ptr,
                                                          uint32_t      n_programs,
                                                          ral_program*  new_programs)
{
    system_critical_section_enter(context_ptr->programs_cs);
    {
        for (uint32_t n_program = 0;
                      n_program < n_programs;
                    ++n_program)
        {
            system_hashed_ansi_string program_name      = nullptr;
            system_hash64             program_name_hash = -1;

            ral_program_get_property(new_programs[n_program],
                                     RAL_PROGRAM_PROPERTY_NAME,
                                    &program_name);

            ASSERT_DEBUG_SYNC(program_name                                       != nullptr &&
                              system_hashed_ansi_string_get_length(program_name) > 0,
                              "Invalid ral_program name property value.");

            program_name_hash = system_hashed_ansi_string_get_hash(program_name);

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->programs_by_name_map,
                                                         program_name_hash),
                              "A ral_program instance already exists for name [%s]",
                              system_hashed_ansi_string_get_buffer(program_name) );

            system_hash64map_insert(context_ptr->programs_by_name_map,
                                    program_name_hash,
                                    new_programs[n_program],
                                    nullptr,  /* on_remove_callback          */
                                    nullptr); /* on_remove_callback_user_arg */
        }
    }
    system_critical_section_leave(context_ptr->programs_cs);
}

/* TODO */
PRIVATE void _ral_context_add_shaders_to_shader_hashmap(_ral_context* context_ptr,
                                                        uint32_t      n_shaders,
                                                        ral_shader*   new_shaders)
{
    system_critical_section_enter(context_ptr->shaders_cs);
    {
        for (uint32_t n_shader = 0;
                      n_shader < n_shaders;
                    ++n_shader)
        {
            system_hashed_ansi_string shader_name      = nullptr;
            system_hash64             shader_name_hash = -1;

            ral_shader_get_property(new_shaders[n_shader],
                                    RAL_SHADER_PROPERTY_NAME,
                                   &shader_name);

            ASSERT_DEBUG_SYNC(shader_name                                       != nullptr &&
                              system_hashed_ansi_string_get_length(shader_name) > 0,
                              "Invalid ral_shader name property value.");

            shader_name_hash = system_hashed_ansi_string_get_hash(shader_name);

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->shaders_by_name_map,
                                                         shader_name_hash),
                              "A ral_shader instance already exists for name [%s]",
                              system_hashed_ansi_string_get_buffer(shader_name) );

            system_hash64map_insert(context_ptr->shaders_by_name_map,
                                    shader_name_hash,
                                    new_shaders[n_shader],
                                    nullptr,  /* on_remove_callback          */
                                    nullptr); /* on_remove_callback_user_arg */
        }
    }
    system_critical_section_leave(context_ptr->shaders_cs);
}

/* TODO */
PRIVATE void _ral_context_add_textures_to_texture_hashmaps(_ral_context* context_ptr,
                                                           uint32_t      n_textures,
                                                           ral_texture*  new_textures)
{
    system_critical_section_enter(context_ptr->textures_cs);
    {
        for (uint32_t n_texture = 0;
                      n_texture < n_textures;
                    ++n_texture)
        {
            system_hashed_ansi_string image_filename      = nullptr;
            system_hash64             image_filename_hash = -1;
            system_hashed_ansi_string image_name          = nullptr;
            system_hash64             image_name_hash     = -1;

            ral_texture_get_property(new_textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_FILENAME,
                                    &image_filename);
            ral_texture_get_property(new_textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_UNIQUE_NAME,
                                    &image_name);

            if (image_filename                                       != nullptr &&
                system_hashed_ansi_string_get_length(image_filename) >  0)
            {
                image_filename_hash = system_hashed_ansi_string_get_hash(image_filename);

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->textures_by_filename_map,
                                                             image_filename_hash),
                                   "A ral_texture instance already exists for filename [%s]",
                                   system_hashed_ansi_string_get_buffer(image_filename) );

                system_hash64map_insert(context_ptr->textures_by_filename_map,
                                        image_filename_hash,
                                        new_textures[n_texture],
                                        nullptr,  /* on_remove_callback */
                                        nullptr); /* on_remove_callback_user_arg */
            }

            if (image_name != nullptr)
            {
                ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(image_name) > 0,
                                  "Invalid gfx_image name property value.");

                image_name_hash = system_hashed_ansi_string_get_hash(image_name);

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->textures_by_name_map,
                                                             image_name_hash),
                                  "A ral_texture instance already exists for name [%s]",
                                  system_hashed_ansi_string_get_buffer(image_name) );

                system_hash64map_insert(context_ptr->textures_by_name_map,
                                        image_name_hash,
                                        new_textures[n_texture],
                                        nullptr,  /* on_remove_callback          */
                                        nullptr); /* on_remove_callback_user_arg */
            }
        }
    }
    system_critical_section_leave(context_ptr->textures_cs);
}

/** TODO */
PRIVATE bool _ral_context_create_objects(_ral_context*           context_ptr,
                                         ral_context_object_type object_type,
                                         uint32_t                n_objects,
                                         void* const*            object_create_info_ptrs,
                                         void**                  out_result_object_ptrs)
{
    bool                    cached_object_reused  = false;
    uint32_t*               object_counter_ptr    = nullptr;
    system_critical_section object_storage_cs     = nullptr;
    system_resizable_vector object_storage_vector = nullptr;
    const char*             object_type_name      = nullptr;
    bool                    result                = false;
    void**                  result_objects_ptr    = nullptr;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            object_counter_ptr    = &context_ptr->n_buffers_created;
            object_storage_cs     =  context_ptr->buffers_cs;
            object_storage_vector =  context_ptr->buffers;
            object_type_name      = "RAL Buffer [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER:
        {
            object_counter_ptr    = &context_ptr->n_command_buffers_created;
            object_storage_cs     =  context_ptr->command_buffers_cs;
            object_storage_vector =  context_ptr->command_buffers;
            object_type_name      = "RAL Command Buffer [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:
        {
            object_counter_ptr    = &context_ptr->n_gfx_states_created;
            object_storage_cs     =  context_ptr->gfx_states_cs;
            object_storage_vector =  context_ptr->gfx_states;
            object_type_name      = "RAL GFX State [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            object_counter_ptr    = &context_ptr->n_programs_created;
            object_storage_cs     =  context_ptr->programs_cs;
            object_storage_vector =  context_ptr->programs;
            object_type_name      = "RAL Program [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            object_counter_ptr    = &context_ptr->n_samplers_created;
            object_storage_cs     =  context_ptr->samplers_cs;
            object_storage_vector =  context_ptr->samplers;
            object_type_name      = "RAL Sampler [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            object_counter_ptr    = &context_ptr->n_shaders_created;
            object_storage_cs     =  context_ptr->shaders_cs;
            object_storage_vector =  context_ptr->shaders;
            object_type_name      = "RAL Shader [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
        {
            object_counter_ptr    = &context_ptr->n_textures_created;
            object_storage_cs     =  context_ptr->textures_cs;
            object_storage_vector = nullptr; /* textures are maintained by the texture pool */
            object_type_name      = "RAL Texture [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
        {
            object_counter_ptr    = &context_ptr->n_texture_views_created;
            object_storage_cs     =  context_ptr->texture_views_cs;
            object_storage_vector =  context_ptr->texture_views;
            object_type_name      = "RAL Texture View [%d]";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized context object type");

            goto end;
        }
    }

    /* After framebuffer instances are created, we will need to fire a notification,
     * so that the backend can create renderer-specific objects for each RAL framebuffer.
     *
     * The notification's argument takes an array of framebuffer instances. Allocate space
     * for that array. */
    result_objects_ptr = new void*[n_objects];

    if (result_objects_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    memset(result_objects_ptr,
           0,
           sizeof(void*) * n_objects);

    /* Create the new object instances. */
    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        char temp[128];

        snprintf(temp,
                 sizeof(temp),
                 object_type_name,
                 (*object_counter_ptr)++);

        switch (object_type)
        {
            case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
            {
                result_objects_ptr[n_object] = ral_buffer_create(reinterpret_cast<ral_context>(context_ptr),
                                                                 system_hashed_ansi_string_create(temp),
                                                                 reinterpret_cast<const ral_buffer_create_info*>(object_create_info_ptrs + n_object) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER:
            {
                result_objects_ptr[n_object] = ral_command_buffer_create(reinterpret_cast<ral_context>                          (context_ptr),
                                                                         reinterpret_cast<const ral_command_buffer_create_info*>(object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:
            {
                result_objects_ptr[n_object] = ral_gfx_state_create(reinterpret_cast<ral_context>                     (context_ptr),
                                                                    reinterpret_cast<const ral_gfx_state_create_info*>(object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
            {
                result_objects_ptr[n_object] = ral_program_create(reinterpret_cast<ral_context>                   (context_ptr),
                                                                  reinterpret_cast<const ral_program_create_info*>(object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
            {
                result_objects_ptr[n_object] = ral_sampler_create(reinterpret_cast<ral_context>(context_ptr),
                                                                  system_hashed_ansi_string_create(temp),
                                                                  reinterpret_cast<const ral_sampler_create_info*>(object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SHADER:
            {
                result_objects_ptr[n_object] = ral_shader_create(reinterpret_cast<const ral_shader_create_info*>(object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
            {
                const ral_texture_create_info* texture_create_info_ptr = reinterpret_cast<const ral_texture_create_info*>(object_create_info_ptrs) + n_object;

                if (!ral_texture_pool_get(context_ptr->texture_pool,
                                          texture_create_info_ptr,
                                          reinterpret_cast<ral_texture*>(result_objects_ptr + n_object) ))
                {
                    cached_object_reused         = false;
                    result_objects_ptr[n_object] = ral_texture_create(reinterpret_cast<ral_context>(context_ptr),
                                                                      texture_create_info_ptr);
                }
                else
                {
                    cached_object_reused = true;

                    /* Update texture filename & name */
                    const system_hashed_ansi_string empty_string = system_hashed_ansi_string_get_default_empty_string();

                    ral_texture_set_property(reinterpret_cast<ral_texture>(result_objects_ptr[n_object]),
                                             RAL_TEXTURE_PROPERTY_DESCRIPTION,
                                            &texture_create_info_ptr->description);
                    ral_texture_set_property(reinterpret_cast<ral_texture>(result_objects_ptr[n_object]),
                                             RAL_TEXTURE_PROPERTY_FILENAME,
                                            &empty_string);
                    ral_texture_set_property(reinterpret_cast<ral_texture>(result_objects_ptr[n_object]),
                                             RAL_TEXTURE_PROPERTY_UNIQUE_NAME,
                                            &texture_create_info_ptr->unique_name);
                }

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "TODO");

#if 0
                /* NOTE: We may need the client app to specify the usage pattern in the future */
                result_objects_ptr[n_object] = ral_texture_create_from_file_name(reinterpret_cast<ral_context>(context_ptr),
                                                                                 system_hashed_ansi_string_create(temp),
                                                                                 *reinterpret_cast<const system_hashed_ansi_string*>(object_create_info_ptrs + n_object),
                                                                                 RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT,
                                                                                 true /* async */);
#endif

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
            {
                /* NOTE: We may need the client app to specify the usage pattern in the future */
                system_hashed_ansi_string file_name_has = nullptr;
                system_hashed_ansi_string name_has      = system_hashed_ansi_string_create(temp);

                gfx_image_get_property(*reinterpret_cast<const gfx_image*>(object_create_info_ptrs + n_object),
                                       GFX_IMAGE_PROPERTY_FILENAME,
                                      &file_name_has);
                gfx_image_get_property(*reinterpret_cast<const gfx_image*>(object_create_info_ptrs + n_object),
                                       GFX_IMAGE_PROPERTY_NAME,
                                      &name_has);

                result_objects_ptr[n_object] = ral_texture_create_from_gfx_image(reinterpret_cast<ral_context>(context_ptr),
                                                                                 name_has,
                                                                                 *reinterpret_cast<const gfx_image*>(object_create_info_ptrs + n_object),
                                                                                 RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT);

                ral_texture_set_property(reinterpret_cast<ral_texture>(result_objects_ptr[n_object]),
                                         RAL_TEXTURE_PROPERTY_FILENAME,
                                        &file_name_has);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
            {
                result_objects_ptr[n_object] = ral_texture_view_create(reinterpret_cast<const ral_texture_view_create_info*>(object_create_info_ptrs) + n_object);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL context object type");

                goto end;
            }
        }

        if (result_objects_ptr[n_object] == nullptr)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to create a %s at index [%d]",
                              object_type_name,
                              n_object);

            goto end;
        }

        /* Store the reference counter */
        if (!cached_object_reused)
        {
            system_critical_section_enter(context_ptr->object_to_refcount_cs);
            {
                ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->object_to_refcount_map,
                                                             reinterpret_cast<system_hash64>(result_objects_ptr[n_object])),
                                  "Reference counter already defined for the newly created object.");

                system_hash64map_insert(context_ptr->object_to_refcount_map,
                                        reinterpret_cast<system_hash64>(result_objects_ptr[n_object]),
                                        reinterpret_cast<void*>(1),
                                        nullptr,  /* callback          */
                                        nullptr); /* callback_argument */
            }
            system_critical_section_leave(context_ptr->object_to_refcount_cs);
        }
        else
        {
            uint32_t counter = 0;

            ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->object_to_refcount_map,
                                                        reinterpret_cast<system_hash64>(result_objects_ptr[n_object])),
                              "Reference counter not defined for a cached object.");

            system_hash64map_get(context_ptr->object_to_refcount_map,
                                 reinterpret_cast<system_hash64>(result_objects_ptr[n_object]),
                                &counter);

            counter++;

            system_hash64map_remove(context_ptr->object_to_refcount_map,
                                    reinterpret_cast<system_hash64>(result_objects_ptr[n_object]) );
            system_hash64map_insert(context_ptr->object_to_refcount_map,
                                    reinterpret_cast<system_hash64>(result_objects_ptr[n_object]),
                                    reinterpret_cast<void*>(counter),
                                    nullptr,  /* on_removal_callback          */
                                    nullptr); /* on_removal_callback_user_arg */
        }

        /* Notify the subscribers, if needed */
        if (object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE && !cached_object_reused ||
            object_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE)
        {
            _ral_context_notify_backend_about_new_object(reinterpret_cast<ral_context>(context_ptr),
                                                         result_objects_ptr[n_object],
                                                         object_type);
        }

        if (object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE)
        {
            ral_texture_upload_data_from_gfx_image(reinterpret_cast<ral_texture>(result_objects_ptr[n_object]),
                                                   *reinterpret_cast<const gfx_image*>(object_create_info_ptrs + n_object),
                                                   false); /* async */
        }
    }

    /* Store the new objects */
    if (object_storage_vector != nullptr)
    {
        system_critical_section_enter(object_storage_cs);
        {
            for (uint32_t n_object = 0;
                          n_object < n_objects;
                        ++n_object)
            {
                system_resizable_vector_push(object_storage_vector,
                                             result_objects_ptr[n_object]);
            }
        }
        system_critical_section_leave(object_storage_cs);
    }

    /* Configure the output variables */
    memcpy(out_result_object_ptrs,
           result_objects_ptr,
           sizeof(void*) * n_objects);

    /* All done */
    result = true;
end:
    if (!result)
    {
        if (result_objects_ptr != nullptr)
        {
            for (uint32_t n_object = 0;
                          n_object < n_objects;
                        ++n_object)
            {
                if (result_objects_ptr[n_object] != nullptr)
                {
                    switch (object_type)
                    {
                        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:         ral_buffer_release        (reinterpret_cast<ral_buffer&>        (result_objects_ptr[n_object]) ); break;
                        case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER: ral_command_buffer_release(reinterpret_cast<ral_command_buffer&>(result_objects_ptr[n_object]) ); break;
                        case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:      ral_gfx_state_release     (reinterpret_cast<ral_gfx_state&>     (result_objects_ptr[n_object]) ); break;
                        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:        ral_program_release       (reinterpret_cast<ral_program&>       (result_objects_ptr[n_object]) ); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:        ral_sampler_release       (reinterpret_cast<ral_sampler&>       (result_objects_ptr[n_object]) ); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SHADER:         ral_shader_release        (reinterpret_cast<ral_shader&>        (result_objects_ptr[n_object]) ); break;
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:   ral_texture_view_release  (reinterpret_cast<ral_texture_view&>  (result_objects_ptr[n_object]) ); break;

                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
                        {
                            ral_texture_release(reinterpret_cast<ral_texture&>(result_objects_ptr[n_object]) );

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized RAL context object type");
                        }
                    }

                    result_objects_ptr[n_object] = nullptr;
                }
            }
        }
    }

    if (result_objects_ptr != nullptr)
    {
        delete [] result_objects_ptr;

        result_objects_ptr = nullptr;
    }

    return result;
}

/** Please see header for specification */
PRIVATE bool _ral_context_create_texture_views(ral_context                         context,
                                               uint32_t                            n_create_info_items,
                                               const ral_texture_view_create_info* create_info_ptrs,
                                               ral_texture_view*                   out_result_texture_view_ptrs)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (create_info_ptrs == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture view create info array is NULL");

        goto end;
    }

    if (out_result_texture_view_ptrs == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                         n_create_info_items,
                                         reinterpret_cast<void* const*>(create_info_ptrs),
                                         reinterpret_cast<void**>      (out_result_texture_view_ptrs) );

end:
    return result;
}

/** TODO */
PRIVATE bool _ral_context_delete_objects(_ral_context*           context_ptr,
                                         ral_context_object_type object_type,
                                         uint32_t                in_n_objects,
                                         void* const*            in_object_ptrs)
{
    system_critical_section cs            = nullptr;
    system_resizable_vector object_vector = nullptr;
    bool                    result        = false;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            cs            = context_ptr->buffers_cs;
            object_vector = context_ptr->buffers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER:
        {
            cs            = context_ptr->command_buffers_cs;
            object_vector = context_ptr->command_buffers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:
        {
            cs            = context_ptr->gfx_states_cs;
            object_vector = context_ptr->gfx_states;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            cs            = context_ptr->programs_cs;
            object_vector = context_ptr->programs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            cs            = context_ptr->samplers_cs;
            object_vector = context_ptr->samplers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            cs            = context_ptr->shaders_cs;
            object_vector = context_ptr->shaders;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            /* Textures are stored in the texture pool. */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
        {
            cs            = context_ptr->texture_views_cs;
            object_vector = context_ptr->texture_views;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    }

    /* Notify the subscribers about the event */
    ral_context_callback_objects_deleted_callback_arg callback_arg;

    callback_arg.deleted_objects = in_object_ptrs;
    callback_arg.object_type     = object_type;
    callback_arg.n_objects       = in_n_objects;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_COMMAND_BUFFERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_GFX_STATES_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_PROGRAMS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SHADERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_TEXTURE_VIEWS_DELETED,
                                             &callback_arg);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    }

    /* Release the objects */
    system_critical_section_enter(cs);
    {
        for (uint32_t n_object = 0;
                      n_object < in_n_objects;
                    ++n_object)
        {
            void*    object              = in_object_ptrs[n_object];
            uint32_t object_vector_index = ITEM_NOT_FOUND;

            if (in_object_ptrs[n_object] == nullptr)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Object at index [%d] is NULL",
                                  n_object);

                continue;
            }

            if (object_vector != nullptr)
            {
                object_vector_index = system_resizable_vector_find(object_vector,
                                                                   in_object_ptrs[n_object]);

                if (object_vector_index != ITEM_NOT_FOUND)
                {
                    system_resizable_vector_delete_element_at(object_vector,
                                                              object_vector_index);
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Released object is not tracked by RAL");
                }
            }

            switch (object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_BUFFER:         ral_buffer_release        (reinterpret_cast<ral_buffer&>       (object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER: ral_command_buffer_release(reinterpret_cast<ral_command_buffer>(object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:      ral_gfx_state_release     (reinterpret_cast<ral_gfx_state>     (object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:        ral_program_release       (reinterpret_cast<ral_program&>      (object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:        ral_sampler_release       (reinterpret_cast<ral_sampler&>      (object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_SHADER:         ral_shader_release        (reinterpret_cast<ral_shader&>       (object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:        ral_texture_release       (reinterpret_cast<ral_texture&>      (object) ); break;
                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:   ral_texture_view_release  (reinterpret_cast<ral_texture_view>  (object) ); break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized RAL context object type");

                    goto end;
                }
            }
        }
    }
    system_critical_section_leave(cs);

    result = true;

end:
    return result;
}

/** TODO */
PRIVATE void _ral_context_delete_programs_from_program_hashmap(_ral_context* context_ptr,
                                                               uint32_t      n_programs,
                                                               ral_program*  programs)
{
    system_critical_section_enter(context_ptr->programs_cs);
    {
        for (uint32_t n_program  = 0;
                      n_program  < n_programs;
                    ++n_program)
        {
            system_hashed_ansi_string program_name      = nullptr;
            system_hash64             program_name_hash = -1;

            ral_program_get_property(programs[n_program],
                                     RAL_PROGRAM_PROPERTY_NAME,
                                    &program_name);

            program_name_hash = system_hashed_ansi_string_get_hash(program_name);

            ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->programs_by_name_map,
                                                        program_name_hash),
                              "Program [%s] is not stored in the name->program hashmap.",
                              system_hashed_ansi_string_get_buffer(program_name) );

            system_hash64map_remove(context_ptr->programs_by_name_map,
                                    program_name_hash);
        }
    }
    system_critical_section_leave(context_ptr->programs_cs);
}

/** TODO */
PRIVATE void _ral_context_delete_shaders_from_shader_hashmap(_ral_context* context_ptr,
                                                             uint32_t      n_shaders,
                                                             ral_shader*   shaders)
{
    system_critical_section_enter(context_ptr->shaders_cs);
    {
        for (uint32_t n_shader  = 0;
                      n_shader  < n_shaders;
                    ++n_shader)
        {
            system_hashed_ansi_string shader_name      = nullptr;
            system_hash64             shader_name_hash = -1;

            ral_shader_get_property(shaders[n_shader],
                                    RAL_SHADER_PROPERTY_NAME,
                                   &shader_name);

            shader_name_hash = system_hashed_ansi_string_get_hash(shader_name);

            ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->shaders_by_name_map,
                                                        shader_name_hash),
                              "Shader [%s] is not stored in the name->shader hashmap.",
                              system_hashed_ansi_string_get_buffer(shader_name) );

            system_hash64map_remove(context_ptr->shaders_by_name_map,
                                    shader_name_hash);
        }
    }
    system_critical_section_leave(context_ptr->shaders_cs);
}

/** TODO */
PRIVATE void _ral_context_delete_textures_from_texture_hashmaps(_ral_context* context_ptr,
                                                                uint32_t      n_textures,
                                                                ral_texture*  textures)
{
    system_critical_section_enter(context_ptr->textures_cs);
    {
        for (uint32_t n_texture = 0;
                      n_texture < n_textures;
                    ++n_texture)
        {
            system_hashed_ansi_string texture_filename      = nullptr;
            system_hash64             texture_filename_hash = -1;
            system_hashed_ansi_string texture_name          = nullptr;
            system_hash64             texture_name_hash     = -1;

            ral_texture_get_property(textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_FILENAME,
                                    &texture_filename);
            ral_texture_get_property(textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_UNIQUE_NAME,
                                    &texture_name);

            if (texture_filename                                       != nullptr &&
                system_hashed_ansi_string_get_length(texture_filename) >  0)
            {
                texture_filename_hash = system_hashed_ansi_string_get_hash(texture_filename);

                ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->textures_by_filename_map,
                                                            texture_filename_hash),
                                  "Texture with filename [%s] is not stored in the filename->texture hashmap.",
                                  system_hashed_ansi_string_get_buffer(texture_filename) );

                system_hash64map_remove(context_ptr->textures_by_filename_map,
                                        texture_filename_hash);
            }

            if (texture_name != nullptr)
            {
                texture_name_hash = system_hashed_ansi_string_get_hash(texture_name);

                ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->textures_by_name_map,
                                                            texture_name_hash),
                                  "Texture [%s] is not stored in the name->texture hashmap.",
                                  system_hashed_ansi_string_get_buffer(texture_name) );

                system_hash64map_remove(context_ptr->textures_by_name_map,
                                        texture_name_hash);
            }
        }
    }
    system_critical_section_leave(context_ptr->textures_cs);
}

/** Initializes a RAL context.
 *
 *  TODO
 **/
PUBLIC void _ral_context_init(_ral_context* context_ptr)
{
    switch (context_ptr->backend_type)
    {
        case RAL_BACKEND_TYPE_ES:
        case RAL_BACKEND_TYPE_GL:
        {
            raGL_backend_init(reinterpret_cast<raGL_backend>(context_ptr->backend) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported backend type.");
        }
    }
}

/** TODO */
PRIVATE void _ral_context_notify_backend_about_new_object(ral_context             context,
                                                          void*                   result_object,
                                                          ral_context_object_type object_type)
{
    ral_context_callback_objects_created_callback_arg callback_arg;
    _ral_context*                                     context_ptr  = reinterpret_cast<_ral_context*>(context);

    callback_arg.created_objects = &result_object;
    callback_arg.object_type     = object_type;
    callback_arg.n_objects       = 1;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_COMMAND_BUFFERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_GFX_STATES_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_PROGRAMS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SHADERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
        {
            callback_arg.object_type = RAL_CONTEXT_OBJECT_TYPE_TEXTURE;

            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_TEXTURE_VIEWS_CREATED,
                                             &callback_arg);

            break;
        }
    }
}

/** TODO */
PRIVATE void _ral_context_release(void* context)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);

    system_callback_manager_call_back(context_ptr->callback_manager,
                                      RAL_CONTEXT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                      context_ptr);

    /* Release the back-end */
    if (context_ptr->backend                  != nullptr &&
        context_ptr->pfn_backend_release_proc != nullptr)
    {
        context_ptr->pfn_backend_release_proc(context_ptr->backend);

        context_ptr->backend = nullptr;
    }

    /* Detect any leaking objects */
    uint32_t n_buffers         = 0;
    uint32_t n_command_buffers = 0;
    uint32_t n_gfx_states      = 0;
    uint32_t n_programs        = 0;
    uint32_t n_samplers        = 0;
    uint32_t n_shaders         = 0;
    uint32_t n_texture_views   = 0;

    system_resizable_vector_get_property(context_ptr->buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_buffers);
    system_resizable_vector_get_property(context_ptr->command_buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_command_buffers);
    system_resizable_vector_get_property(context_ptr->gfx_states,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_gfx_states);
    system_resizable_vector_get_property(context_ptr->programs,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_programs);
    system_resizable_vector_get_property(context_ptr->samplers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_samplers);
    system_resizable_vector_get_property(context_ptr->shaders,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_shaders);
    system_resizable_vector_get_property(context_ptr->texture_views,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_texture_views);

    ASSERT_DEBUG_SYNC(n_buffers == 0,
                      "Buffer object leak detected");
    ASSERT_DEBUG_SYNC(n_command_buffers == 0,
                      "Command buffer object leak detected");
    ASSERT_DEBUG_SYNC(n_gfx_states == 0,
                      "GFX state object leak detected");
    ASSERT_DEBUG_SYNC(n_programs == 0,
                      "Program object leak detected");
    ASSERT_DEBUG_SYNC(n_samplers == 0,
                      "Sampler object leak detected");
    ASSERT_DEBUG_SYNC(n_shaders == 0,
                      "Shader object leak detected");
    ASSERT_DEBUG_SYNC(n_texture_views == 0,
                      "Texture view object leak detected");
}

/** Please see header for specification */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      demo_window               window)
{
    _ral_context* new_context_ptr = new (std::nothrow) _ral_context(name,
                                                                    window);

    ASSERT_ALWAYS_SYNC(new_context_ptr != nullptr,
                       "Out of memory");
    if (new_context_ptr != nullptr)
    {
        /* Instantiate the rendering back-end */
        ral_backend_type      backend_type      = RAL_BACKEND_TYPE_UNKNOWN;
        ral_rendering_handler rendering_handler = nullptr;

        demo_window_get_property(window,
                                 DEMO_WINDOW_PROPERTY_BACKEND_TYPE,
                                &backend_type);
        demo_window_get_property(window,
                                 DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                                &rendering_handler);

        switch (backend_type)
        {
            case RAL_BACKEND_TYPE_ES:
            case RAL_BACKEND_TYPE_GL:
            {
                new_context_ptr->backend_type                  = backend_type;
                new_context_ptr->backend                       = reinterpret_cast<void*>(raGL_backend_create(reinterpret_cast<ral_context>(new_context_ptr),
                                                                                                             name,
                                                                                                             backend_type) );
                new_context_ptr->pfn_backend_get_property_proc = raGL_backend_get_property;
                new_context_ptr->pfn_backend_release_proc      = raGL_backend_release;

                raGL_backend_get_private_property(reinterpret_cast<raGL_backend>(new_context_ptr->backend),
                                                  RAGL_BACKEND_PRIVATE_PROPERTY_TEXTURE_POOL,
                                                 &new_context_ptr->texture_pool);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported backend type requested.");
            }
        }

        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_context_ptr,
                                                       _ral_context_release,
                                                       OBJECT_TYPE_RAL_CONTEXT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Contexts\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        /* Initialize the context */
        ral_rendering_handler_bind_to_context(rendering_handler,
                                              reinterpret_cast<ral_context>(new_context_ptr) );

        _ral_context_init(new_context_ptr);
    }

    return reinterpret_cast<ral_context>(new_context_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_buffers(ral_context                   context,
                                                   uint32_t                      n_buffers,
                                                   const ral_buffer_create_info* buffer_create_info_ptr,
                                                   ral_buffer*                   out_result_buffers_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_buffers == 0)
    {
        goto end;
    }

    if (buffer_create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input buffer create info array is NULL");

        goto end;
    }

    if (out_result_buffers_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                         n_buffers,
                                         reinterpret_cast<void* const*>(buffer_create_info_ptr),
                                         reinterpret_cast<void**>      (out_result_buffers_ptr) );

end:
    return result;
}

PUBLIC EMERALD_API bool ral_context_create_command_buffers(ral_context                           context,
                                                           uint32_t                              n_command_buffers,
                                                           const ral_command_buffer_create_info* command_buffer_create_info_ptr,
                                                           ral_command_buffer*                   out_result_command_buffers_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_command_buffers == 0)
    {
        goto end;
    }

    if (command_buffer_create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input command buffer create info array is NULL");

        goto end;
    }

    if (out_result_command_buffers_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                         n_command_buffers,
                                         reinterpret_cast<void* const*>(command_buffer_create_info_ptr) ,
                                         reinterpret_cast<void**>      (out_result_command_buffers_ptr) );

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_gfx_states(ral_context                      context,
                                                      uint32_t                         n_create_info_items,
                                                      const ral_gfx_state_create_info* create_info_ptrs,
                                                      ral_gfx_state*                   out_result_gfx_states_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (out_result_gfx_states_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                         n_create_info_items,
                                         reinterpret_cast<void* const*>(create_info_ptrs),
                                         reinterpret_cast<void**>      (out_result_gfx_states_ptr) );

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_programs(ral_context                    context,
                                                    uint32_t                       n_create_info_items,
                                                    const ral_program_create_info* create_info_ptrs,
                                                    ral_program*                   out_result_program_ptrs)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (out_result_program_ptrs == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                         n_create_info_items,
                                         reinterpret_cast<void* const*>(create_info_ptrs),
                                         reinterpret_cast<void**>      (out_result_program_ptrs) );

    if (result)
    {
        _ral_context_add_programs_to_program_hashmap(context_ptr,
                                                     n_create_info_items,
                                                     out_result_program_ptrs);
    }
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_samplers(ral_context                    context,
                                                    uint32_t                       n_create_info_items,
                                                    const ral_sampler_create_info* create_info_ptrs,
                                                    ral_sampler*                   out_result_sampler_ptrs)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (out_result_sampler_ptrs == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                         n_create_info_items,
                                         reinterpret_cast<void* const*>(create_info_ptrs),
                                         reinterpret_cast<void**>      (out_result_sampler_ptrs) );

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_shaders(ral_context                   context,
                                                   uint32_t                      n_create_info_items,
                                                   const ral_shader_create_info* create_info_ptrs,
                                                   ral_shader*                   out_result_shader_ptrs)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (create_info_ptrs == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input shader create info array is NULL");

        goto end;
    }

    if (out_result_shader_ptrs == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                         n_create_info_items,
                                         reinterpret_cast<void* const*>(create_info_ptrs),
                                         reinterpret_cast<void**>      (out_result_shader_ptrs) );

    if (result)
    {
        _ral_context_add_shaders_to_shader_hashmap(context_ptr,
                                                   n_create_info_items,
                                                   out_result_shader_ptrs);
    }
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_textures(ral_context                    context,
                                                    uint32_t                       n_textures,
                                                    const ral_texture_create_info* texture_create_info_ptr,
                                                    ral_texture*                   out_result_textures_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_textures == 0)
    {
        goto end;
    }

    if (texture_create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture create info array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                         n_textures,
                                         reinterpret_cast<void* const*>(texture_create_info_ptr),
                                         reinterpret_cast<void**>      (out_result_textures_ptr) );

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_textures,
                                                      out_result_textures_ptr);
    }
end:
    return result;
}

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_textures_from_file_names(ral_context                      context,
                                                                    uint32_t                         n_file_names,
                                                                    const system_hashed_ansi_string* file_names_ptr,
                                                                    ral_texture*                     out_result_textures_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_file_names == 0)
    {
        goto end;
    }

    if (file_names_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input file name array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    /* Check if we already have a texture for each specified file name. If any of them is
     * recognized, we can skip them */
    system_critical_section_enter(context_ptr->textures_cs);
    {
        system_resizable_vector file_names_to_handle_vector = system_resizable_vector_create(n_file_names);
        uint32_t                n_current_file_name         = 0;
        uint32_t                n_file_names_to_handle      = 0;

        while (n_current_file_name != n_file_names)
        {
            const system_hash64 file_name_hash = system_hashed_ansi_string_get_hash(file_names_ptr[n_current_file_name]);

            if (!system_hash64map_contains(context_ptr->textures_by_filename_map,
                                           file_name_hash))
            {
                system_resizable_vector_push(file_names_to_handle_vector,
                                             file_names_ptr[n_current_file_name]);

                ++n_file_names_to_handle;
            }
            else
            {
                system_hash64map_get(context_ptr->textures_by_filename_map,
                                     file_name_hash,
                                     out_result_textures_ptr + n_current_file_name);
            }

            ++n_current_file_name;
        }

        if (n_file_names_to_handle > 0)
        {
            system_hashed_ansi_string* file_names_to_handle = nullptr;
            ral_texture*               result_textures      = reinterpret_cast<ral_texture*>(_malloca(sizeof(ral_texture) * n_file_names_to_handle) );

            ASSERT_ALWAYS_SYNC(result_textures != nullptr,
                               "Out of memory");

            system_resizable_vector_get_property(file_names_to_handle_vector,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                                &file_names_to_handle);

            result = _ral_context_create_objects(context_ptr,
                                                 RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME,
                                                 n_file_names_to_handle,
                                                 reinterpret_cast<void* const*>(file_names_to_handle),
                                                 reinterpret_cast<void**>      (result_textures) );

            if (!result)
            {
                ASSERT_DEBUG_SYNC(result,
                                  "Texture creation process failed.");
            }
            else
            {
                uint32_t n_returned_textures = 0;

                for (n_current_file_name = 0;
                     n_current_file_name < n_file_names;
                   ++n_current_file_name)
                {
                    const system_hash64 file_name_hash = system_hashed_ansi_string_get_hash(file_names_ptr[n_current_file_name]);

                    if (!system_hash64map_contains(context_ptr->textures_by_filename_map,
                                                   file_name_hash))
                    {
                        out_result_textures_ptr[n_current_file_name] = result_textures[n_returned_textures++];
                    }
                }

                ASSERT_DEBUG_SYNC(n_returned_textures == n_file_names_to_handle,
                                  "Internal error");

                _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                              n_file_names_to_handle,
                                                              result_textures);
            }

            _freea(result_textures);
        }
        else
        {
            result = true;
        }

        system_resizable_vector_release(file_names_to_handle_vector);
    }
    system_critical_section_leave(context_ptr->textures_cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_textures_from_gfx_images(ral_context      context,
                                                                    uint32_t         n_images,
                                                                    const gfx_image* images,
                                                                    ral_texture*     out_result_textures_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_images == 0)
    {
        goto end;
    }

    if (images == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input image array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE,
                                         n_images,
                                         reinterpret_cast<void* const*>(images),
                                         reinterpret_cast<void**>      (out_result_textures_ptr) );

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_images,
                                                      out_result_textures_ptr);

    }
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_delete_objects(ral_context             context,
                                                   ral_context_object_type object_type,
                                                   uint32_t                in_n_objects,
                                                   void* const*            in_objects)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    void**        object_ptrs;
    bool          result      = false;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (in_n_objects == 0)
    {
        goto end;
    }

    if (in_objects == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input object array is NULL");

        goto end;
    }

    /* Decrement the reference counter for the object. We will only proceed with the release process
     * for those objects, whose refcounter drops to zero (for non-texture objects) or one (otherwise) */
    uint32_t n_objects = 0;

    object_ptrs = static_cast<void**>(_malloca(sizeof(void*) * in_n_objects) );

    ASSERT_DEBUG_SYNC(object_ptrs != nullptr,
                      "TODO");

    system_critical_section_enter(context_ptr->object_to_refcount_cs);
    {
        for (uint32_t n_object = 0;
                      n_object < in_n_objects;
                    ++n_object)
        {
            uint32_t object_ref_counter = 0;

            if (in_objects[n_object] == nullptr)
            {
                continue;
            }

            if (!system_hash64map_get(context_ptr->object_to_refcount_map,
                                      (system_hash64) in_objects[n_object],
                                     &object_ref_counter) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Object [%x] not recognized.",
                                  in_objects[n_object]);

                continue;
            }

            --object_ref_counter;

            if (object_ref_counter == 0 && object_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE ||
                object_ref_counter <= 1 && object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE)
            {
                object_ptrs[n_objects++] = in_objects[n_object];
            }

            system_hash64map_remove(context_ptr->object_to_refcount_map,
                                    reinterpret_cast<system_hash64>(in_objects[n_object]) );

            if (object_ref_counter != 0)
            {
                system_hash64map_insert(context_ptr->object_to_refcount_map,
                                        reinterpret_cast<system_hash64>(in_objects[n_object]),
                                        reinterpret_cast<void*>        (object_ref_counter),
                                        nullptr, /* calback           */
                                        nullptr  /* callback_argument */);
            }
        }
    }
    system_critical_section_leave(context_ptr->object_to_refcount_cs);

    if (n_objects == 0)
    {
        goto end;
    }

    /* A few object types need extra care before we can proceed with calling the actual handler.. */
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        case RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER:
        case RAL_CONTEXT_OBJECT_TYPE_GFX_STATE:
        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
        {
            /* Nothing to do for these object types */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            _ral_context_delete_programs_from_program_hashmap(context_ptr,
                                                              n_objects,
                                                              reinterpret_cast<ral_program*>(object_ptrs) );

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            _ral_context_delete_shaders_from_shader_hashmap(context_ptr,
                                                            n_objects,
                                                            reinterpret_cast<ral_shader*>(object_ptrs) );

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            if (context_ptr->texture_pool != nullptr)
            {
                bool is_pool_being_released = false;

                ral_texture_pool_get_property(context_ptr->texture_pool,
                                              RAL_TEXTURE_POOL_PROPERTY_IS_BEING_RELEASED,
                                             &is_pool_being_released);

                if (!is_pool_being_released)
                {
                    _ral_context_delete_textures_from_texture_hashmaps(context_ptr,
                                                                       n_objects,
                                                                       reinterpret_cast<ral_texture*>(object_ptrs) );

                    for (uint32_t n_texture = 0;
                                  n_texture < n_objects;
                                ++n_texture)
                    {
                        ral_texture_pool_return_texture(context_ptr->texture_pool,
                                                        reinterpret_cast<ral_texture>(object_ptrs[n_texture]) );
                    }

                    result = true;
                }
            }

            if (result)
            {
                goto end;
            }
            else
            {
                break;
            }
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL object type");
        }
    }

    /* Release the objects */
    result = _ral_context_delete_objects(context_ptr,
                                         object_type,
                                         n_objects,
                                         object_ptrs);

end:
    if (object_ptrs != nullptr)
    {
        _freea(object_ptrs);
    }

    return result;
}

/** TODO */
PUBLIC EMERALD_API ogl_context ral_context_get_gl_context(ral_context context)
{
    raGL_backend backend         = reinterpret_cast<raGL_backend>(reinterpret_cast<_ral_context*>(context)->backend);
    ogl_context  backend_context = nullptr;

    raGL_backend_get_property(backend,
                              RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                             &backend_context);

    return backend_context;
}

/** Please see header for specification */
PUBLIC void ral_context_get_private_property(ral_context                  context,
                                             ral_context_private_property property,
                                             void*                        out_result_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);

    switch (property)
    {
        case RAL_CONTEXT_PRIVATE_PROPERTY_CREATE_TEXTURE_VIEW_FUNC_PTR:
        {
            *reinterpret_cast<PFNRALCONTEXTCREATETEXTUREVIEWPROC*>(out_result_ptr) = _ral_context_create_texture_views;

            break;
        }

        case RAL_CONTEXT_PRIVATE_PROPERTY_TEXTURE_POOL:
        {
            *reinterpret_cast<ral_texture_pool*>(out_result_ptr) = context_ptr->texture_pool;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_private_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_program ral_context_get_program_by_name(ral_context               context,
                                                               system_hashed_ansi_string name)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    ral_program   result      = nullptr;

    /* Sanity checks..*/
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input program name is NULL");

        goto end;
    }

    /* Is there a program associated with the specified name? */
    system_critical_section_enter(context_ptr->programs_cs);
    {
        system_hash64map_get(context_ptr->programs_by_name_map,
                             system_hashed_ansi_string_get_hash(name),
                            &result);
    }
    system_critical_section_leave(context_ptr->programs_cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_context_get_property(ral_context          context,
                                                 ral_context_property property,
                                                 void*                out_result_ptr)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    /* If this is a backend-specific property, forward the request to the backend instance.
     * Otherwise, try to handle it. */
    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_BACKEND:
        {
            *reinterpret_cast<raGL_backend*>(out_result_ptr) = reinterpret_cast<raGL_backend>(context_ptr->backend);

            break;
        }

        case RAL_CONTEXT_PROPERTY_BACKEND_TYPE:
        {
            *reinterpret_cast<ral_backend_type*>(out_result_ptr) = context_ptr->backend_type;

            break;
        }

        case RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT:
        case RAL_CONTEXT_PROPERTY_IS_INTEL_DRIVER:
        case RAL_CONTEXT_PROPERTY_IS_NV_DRIVER:
        case RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_COUNT:
        case RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
        case RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE:
        case RAL_CONTEXT_PROPERTY_MAX_UNIFORM_BLOCK_SIZE:
        case RAL_CONTEXT_PROPERTY_STORAGE_BUFFER_ALIGNMENT:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FB_BACK_BUFFER_COLOR_FORMAT:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FB_SIZE:
        case RAL_CONTEXT_PROPERTY_UNIFORM_BUFFER_ALIGNMENT:
        {
            context_ptr->pfn_backend_get_property_proc(context_ptr->backend,
                                                       property,
                                                       out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = context_ptr->callback_manager;

            break;
        }

        case RAL_CONTEXT_PROPERTY_RENDERING_HANDLER:
        {
            *reinterpret_cast<ral_rendering_handler*>(out_result_ptr) = context_ptr->rendering_handler;

            break;
        }

        case RAL_CONTEXT_PROPERTY_WINDOW_DEMO:
        {
            *reinterpret_cast<demo_window*>(out_result_ptr) = context_ptr->window_demo;

            break;
        }

        case RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM:
        {
            *reinterpret_cast<system_window*>(out_result_ptr) = context_ptr->window_system;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecpognized ral_context_property value.");
        }
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader ral_context_get_shader_by_name(ral_context               context,
                                                             system_hashed_ansi_string name)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    ral_shader    result      = nullptr;

    /* Sanity checks..*/
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input shader name is NULL");

        goto end;
    }

    /* Is there a shader associated with the specified name? */
    system_critical_section_enter(context_ptr->shaders_cs);
    {
        system_hash64map_get(context_ptr->shaders_by_name_map,
                             system_hashed_ansi_string_get_hash(name),
                            &result);
    }
    system_critical_section_leave(context_ptr->shaders_cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_texture ral_context_get_texture_by_file_name(ral_context               context,
                                                                    system_hashed_ansi_string file_name)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    ral_texture   result      = nullptr;

    /* Sanity checks..*/
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (file_name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input file name is NULL");

        goto end;
    }

    /* Is there a texture associated with the specified file name? */
    system_critical_section_enter(context_ptr->textures_cs);
    {
        system_hash64map_get(context_ptr->textures_by_filename_map,
                             system_hashed_ansi_string_get_hash(file_name),
                            &result);
    }
    system_critical_section_leave(context_ptr->textures_cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_texture ral_context_get_texture_by_name(ral_context               context,
                                                               system_hashed_ansi_string name)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);
    ral_texture   result      = nullptr;

    /* Sanity checks..*/
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture name is NULL");

        goto end;
    }

    /* Is there a texture associated with the specified name? */
    system_critical_section_enter(context_ptr->textures_cs);
    {
        system_hash64map_get(context_ptr->textures_by_name_map,
                             system_hashed_ansi_string_get_hash(name),
                            &result);
    }
    system_critical_section_leave(context_ptr->textures_cs);

end:
    return result;
}

/** TODO */
PUBLIC EMERALD_API bool ral_context_retain_object(ral_context             context,
                                                  ral_context_object_type object_type,
                                                  void*                   object)
{
    return ral_context_retain_objects(context,
                                      object_type,
                                      1, /* n_objects */
                                     &object);
}

/** TODO */
PUBLIC EMERALD_API bool ral_context_retain_objects(ral_context             context,
                                                   ral_context_object_type object_type,
                                                   uint32_t                n_objects,
                                                   void* const*            objects)
{
    const _ral_context* context_ptr = reinterpret_cast<const _ral_context*>(context);
    bool                result      = false;

    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL RAL context specified.");

        goto end;
    }

    ASSERT_DEBUG_SYNC(object_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                      "Texture views are not retainable");

    system_critical_section_enter(context_ptr->object_to_refcount_cs);
    {
        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            uint32_t ref_counter = 0;

            ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->object_to_refcount_map,
                                                        reinterpret_cast<system_hash64>(objects[n_object]) ),
                              "No reference counter associated with the specified object.");

            system_hash64map_get(context_ptr->object_to_refcount_map,
                                 reinterpret_cast<system_hash64>(objects[n_object]),
                                &ref_counter);

            ref_counter++;

            system_hash64map_remove(context_ptr->object_to_refcount_map,
                                    reinterpret_cast<system_hash64>(objects[n_object]) );
            system_hash64map_insert(context_ptr->object_to_refcount_map,
                                    reinterpret_cast<system_hash64>(objects[n_object]),
                                    reinterpret_cast<void*>        (ref_counter),
                                    nullptr,  /* callback          */
                                    nullptr); /* callback_argument */
        }
    }
    system_critical_section_leave(context_ptr->object_to_refcount_cs);

    result = true;
end:
    return result;
}

/** TODO */
PUBLIC void ral_context_set_property(ral_context          context,
                                     ral_context_property property,
                                     void* const*         data)
{
    _ral_context* context_ptr = reinterpret_cast<_ral_context*>(context);

    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_BACKEND:
        {
            ASSERT_DEBUG_SYNC(context_ptr->backend == nullptr,
                              "Backend is not NULL");

            context_ptr->backend = *data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_property value.");
        }
    }
}