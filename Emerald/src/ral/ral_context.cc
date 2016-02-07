/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_texture.h" /* TEMP TEMP TEMP */
#include "raGL/raGL_types.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_program.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_pool.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"

typedef bool (*PFNRALBACKENDGETFRAMEBUFFERPROC)(void*                      backend,
                                                ral_framebuffer            framebuffer_ral,
                                                void**                     out_framebuffer_backend_ptr);
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
    PFNRALBACKENDGETFRAMEBUFFERPROC    pfn_backend_get_framebuffer_proc;
    PFNRALBACKENDGETPROPERTYPROC       pfn_backend_get_property_proc;
    PFNRALBACKENDRELEASEPROC           pfn_backend_release_proc;

    system_callback_manager callback_manager;

    system_resizable_vector buffers; /* owns ral_buffer instances */
    system_critical_section buffers_cs;
    uint32_t                n_buffers_created;

    system_resizable_vector framebuffers; /* owns ral_framebuffer instances */
    system_critical_section framebuffers_cs;
    uint32_t                n_framebuffers_created;

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

    ral_texture_pool        texture_pool;
    system_hash64map        textures_by_filename_map; /* does NOT own the ral_texture values */
    system_hash64map        textures_by_name_map;     /* does NOT own the ral_texture values */
    system_critical_section textures_cs;
    uint32_t                n_textures_created;

    system_critical_section object_to_refcount_cs;
    system_hash64map        object_to_refcount_map;

    system_hashed_ansi_string name;
    ogl_rendering_handler     rendering_handler;
    demo_window               window_demo;
    system_window             window_system;

    REFCOUNT_INSERT_VARIABLES;


    _ral_context(system_hashed_ansi_string in_name,
                 demo_window               in_window)
    {
        backend                  = NULL;
        backend_type             = RAL_BACKEND_TYPE_UNKNOWN;
        buffers                  = system_resizable_vector_create(sizeof(ral_buffer) );
        buffers_cs               = system_critical_section_create();
        callback_manager         = system_callback_manager_create((_callback_id) RAL_CONTEXT_CALLBACK_ID_COUNT);
        framebuffers             = system_resizable_vector_create(sizeof(ral_framebuffer) );
        framebuffers_cs          = system_critical_section_create();
        n_buffers_created        = 0;
        n_framebuffers_created   = 0;
        n_samplers_created       = 0;
        n_textures_created       = 0;
        name                     = in_name;
        object_to_refcount_cs    = system_critical_section_create();
        object_to_refcount_map   = system_hash64map_create       (sizeof(uint32_t) );
        rendering_handler        = NULL;
        programs                 = system_resizable_vector_create(sizeof(ral_program) );
        programs_by_name_map     = system_hash64map_create       (sizeof(ral_program) );
        programs_cs              = system_critical_section_create();
        samplers                 = system_resizable_vector_create(sizeof(ral_sampler) );
        samplers_cs              = system_critical_section_create();
        shaders                  = system_resizable_vector_create(sizeof(ral_shader) );
        shaders_by_name_map      = system_hash64map_create       (sizeof(ral_shader) );
        shaders_cs               = system_critical_section_create();
        texture_pool             = ral_texture_pool_create       ();
        textures_by_filename_map = system_hash64map_create       (sizeof(ral_texture) );
        textures_by_name_map     = system_hash64map_create       (sizeof(ral_texture) );
        textures_cs              = system_critical_section_create();
        window_demo              = in_window;
        window_system            = NULL;

        pfn_backend_get_property_proc = NULL;

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
            &framebuffers_cs,
            &object_to_refcount_cs,
            &programs_cs,
            &samplers_cs,
            &shaders_cs
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
            &framebuffers,
            &programs,
            &samplers,
            &shaders
        };

        const uint32_t n_cses_to_release     = sizeof(cses_to_release)     / sizeof(cses_to_release    [0]);
        const uint32_t n_hashmaps_to_release = sizeof(hashmaps_to_release) / sizeof(hashmaps_to_release[0]);
        const uint32_t n_vectors_to_release  = sizeof(vectors_to_release)  / sizeof(vectors_to_release [0]);

        for (uint32_t n_cs_to_release = 0;
                      n_cs_to_release < n_cses_to_release;
                    ++n_cs_to_release)
        {
            system_critical_section* current_cs_ptr = cses_to_release[n_cs_to_release];

            if (*current_cs_ptr != NULL)
            {
                system_critical_section_release(*current_cs_ptr);

                *current_cs_ptr = NULL;
            }
        }

        for (uint32_t n_hashmap_to_release = 0;
                      n_hashmap_to_release < n_hashmaps_to_release;
                    ++n_hashmap_to_release)
        {
            system_hash64map* current_hashmap_ptr = hashmaps_to_release[n_hashmap_to_release];

            if (*current_hashmap_ptr != NULL)
            {
                system_hash64map_release(*current_hashmap_ptr);

                *current_hashmap_ptr = NULL;
            }
        }

        for (uint32_t n_vector_to_release = 0;
                      n_vector_to_release < n_vectors_to_release;
                    ++n_vector_to_release)
        {
            system_resizable_vector* current_vector_ptr = vectors_to_release[n_vector_to_release];
            uint32_t                 n_objects          = 0;

            if (*current_vector_ptr != NULL)
            {
                system_resizable_vector_get_property(*current_vector_ptr,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_objects);

                ASSERT_DEBUG_SYNC(n_objects == 0,
                                  "Memory leak detected");

                system_resizable_vector_release(*current_vector_ptr);
                *current_vector_ptr = NULL;
            }
        }

        if (texture_pool != NULL)
        {
            ral_texture_pool_release(texture_pool);

            texture_pool = NULL;
        } /* if (texture_pool != NULL) */

        /* Callback manager needs to be deleted at the end. */
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */
    }
} _ral_context;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_context,
                               ral_context,
                              _ral_context);

/* Forward declarations */
PRIVATE void _ral_context_notify_backend_about_new_object(ral_context             context,
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
            system_hashed_ansi_string program_name      = NULL;
            system_hash64             program_name_hash = -1;

            ral_program_get_property(new_programs[n_program],
                                     RAL_PROGRAM_PROPERTY_NAME,
                                    &program_name);

            ASSERT_DEBUG_SYNC(program_name                                       != NULL &&
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
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
        } /* for (all programs) */
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
            system_hashed_ansi_string shader_name      = NULL;
            system_hash64             shader_name_hash = -1;

            ral_shader_get_property(new_shaders[n_shader],
                                    RAL_SHADER_PROPERTY_NAME,
                                   &shader_name);

            ASSERT_DEBUG_SYNC(shader_name                                       != NULL &&
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
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
        } /* for (all shaders) */
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
            system_hashed_ansi_string image_filename      = NULL;
            system_hash64             image_filename_hash = -1;
            system_hashed_ansi_string image_name          = NULL;
            system_hash64             image_name_hash     = -1;

            ral_texture_get_property(new_textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_FILENAME,
                                    &image_filename);
            ral_texture_get_property(new_textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_NAME,
                                    &image_name);

            if (image_filename                                       != NULL &&
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
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            } /* if (image_filename != NULL) */

            ASSERT_DEBUG_SYNC(image_name                                       != NULL &&
                              system_hashed_ansi_string_get_length(image_name) > 0,
                              "Invalid gfx_image name property value.");

            image_name_hash = system_hashed_ansi_string_get_hash(image_name);

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->textures_by_name_map,
                                                         image_name_hash),
                              "A ral_texture instance already exists for name [%s]",
                              system_hashed_ansi_string_get_buffer(image_name) );

            system_hash64map_insert(context_ptr->textures_by_name_map,
                                    image_name_hash,
                                    new_textures[n_texture],
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
        } /* for (all textures) */
    }
    system_critical_section_leave(context_ptr->textures_cs);
}

/** TODO */
PRIVATE bool _ral_context_create_objects(_ral_context*           context_ptr,
                                         ral_context_object_type object_type,
                                         uint32_t                n_objects,
                                         void**                  object_create_info_ptrs,
                                         void**                  out_result_object_ptrs)
{
    bool                    backend_texture_callback_used = false;
    uint32_t*               object_counter_ptr            = NULL;
    system_critical_section object_storage_cs             = NULL;
    system_resizable_vector object_storage_vector         = NULL;
    const char*             object_type_name              = NULL;
    bool                    result                        = false;
    void**                  result_objects_ptr            = NULL;

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

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            object_counter_ptr    = &context_ptr->n_framebuffers_created;
            object_storage_cs     =  context_ptr->framebuffers_cs;
            object_storage_vector =  context_ptr->framebuffers;
            object_type_name      = "RAL Framebuffer [%d]";

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
            object_counter_ptr    = &context_ptr->n_framebuffers_created;
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
            object_storage_vector = NULL; /* textures are maintained by the texture pool */
            object_type_name      = "RAL Texture [%d]";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized context object type");

            goto end;
        }
    } /* switch (object_type) */

    /* After framebuffer instances are created, we will need to fire a notification,
     * so that the backend can create renderer-specific objects for each RAL framebuffer.
     *
     * The notification's argument takes an array of framebuffer instances. Allocate space
     * for that array. */
    result_objects_ptr = new void*[n_objects];

    if (result_objects_ptr == NULL)
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
        system_hashed_ansi_string name_has = NULL;
        char temp[128];

        snprintf(temp,
                 sizeof(temp),
                 object_type_name,
                 (*object_counter_ptr)++);

        name_has = system_hashed_ansi_string_create(temp);

        switch (object_type)
        {
            case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
            {
                result_objects_ptr[n_object] = ral_buffer_create(name_has,
                                                                 (const ral_buffer_create_info*) (object_create_info_ptrs + n_object) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
            {
                result_objects_ptr[n_object] = ral_framebuffer_create( (ral_context) context_ptr,
                                                                      name_has);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
            {
                result_objects_ptr[n_object] = ral_program_create((ral_context) context_ptr,
                                                                  (const ral_program_create_info*) (object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
            {
                result_objects_ptr[n_object] = ral_sampler_create(name_has,
                                                                  (const ral_sampler_create_info*) (object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SHADER:
            {
                result_objects_ptr[n_object] = ral_shader_create( (const ral_shader_create_info*) (object_create_info_ptrs) + n_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
            {
                const ral_texture_create_info* texture_create_info_ptr = (const ral_texture_create_info*) (object_create_info_ptrs) + n_object;

                if (!ral_texture_pool_get(context_ptr->texture_pool,
                                          texture_create_info_ptr,
                                          (ral_texture*) (result_objects_ptr + n_object) ))
                {
                    backend_texture_callback_used = true;
                    result_objects_ptr[n_object]  = ral_texture_create((ral_context) context_ptr,
                                                                       name_has,
                                                                       texture_create_info_ptr);
                }
                else
                {
                    /* The new texture instance comes from the texture pool, so make sure the filename & name is synced
                     * to what's been requested */
                    const system_hashed_ansi_string empty_string = system_hashed_ansi_string_get_default_empty_string();

                    ral_texture_set_property((ral_texture) result_objects_ptr[n_object],
                                             RAL_TEXTURE_PROPERTY_FILENAME,
                                            &empty_string);
                    ral_texture_set_property((ral_texture) result_objects_ptr[n_object],
                                             RAL_TEXTURE_PROPERTY_NAME,
                                            &texture_create_info_ptr->name);
                }

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
            {
                /* NOTE: We may need the client app to specify the usage pattern in the future */
                result_objects_ptr[n_object] = ral_texture_create_from_file_name((ral_context) context_ptr,
                                                                                 name_has,
                                                                                 *(system_hashed_ansi_string*) (object_create_info_ptrs + n_object),
                                                                                 RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT,
                                                                                 _ral_context_notify_backend_about_new_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
            {
                /* NOTE: We may need the client app to specify the usage pattern in the future */
                system_hashed_ansi_string file_name_has = NULL;

                gfx_image_get_property(*(gfx_image*) (object_create_info_ptrs + n_object),
                                       GFX_IMAGE_PROPERTY_FILENAME,
                                      &file_name_has);
                gfx_image_get_property(*(gfx_image*) (object_create_info_ptrs + n_object),
                                       GFX_IMAGE_PROPERTY_NAME,
                                      &name_has);

                result_objects_ptr[n_object] = ral_texture_create_from_gfx_image((ral_context) context_ptr,
                                                                                 name_has,
                                                                                 *(gfx_image*) (object_create_info_ptrs + n_object),
                                                                                 RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT,
                                                                                 _ral_context_notify_backend_about_new_object);

                ral_texture_set_property((ral_texture) result_objects_ptr[n_object],
                                         RAL_TEXTURE_PROPERTY_FILENAME,
                                        &file_name_has);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL context object type");

                goto end;
            }
        } /* switch (object_type) */

        if (result_objects_ptr[n_object] == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to create a %s at index [%d]",
                              object_type_name,
                              n_object);

            goto end;
        }

        /* Notify the subscribers, if needed */
        if (object_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME &&
            object_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE)
        {
            if (object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE && backend_texture_callback_used ||
                object_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE)
            {
                _ral_context_notify_backend_about_new_object((ral_context) context_ptr,
                                                             result_objects_ptr[n_object],
                                                             object_type);
            }
        }
    } /* for (all objects to create) */

    /* Store the new objects */
    if (object_storage_vector != NULL)
    {
        system_critical_section_enter(object_storage_cs);
        {
            for (uint32_t n_object = 0;
                          n_object < n_objects;
                        ++n_object)
            {
                system_resizable_vector_push(object_storage_vector,
                                             result_objects_ptr[n_object]);
            } /* for (all objects) */
        }
        system_critical_section_leave(object_storage_cs);
    } /* if (object_storage_vector != NULL) */

    /* Store the reference counters */
    system_critical_section_enter(context_ptr->object_to_refcount_cs);
    {
        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->object_to_refcount_map,
                                                         (system_hash64) result_objects_ptr[n_object]),
                              "Reference counter already defined for the newly created object.");

            system_hash64map_insert(context_ptr->object_to_refcount_map,
                                    (system_hash64) result_objects_ptr[n_object],
                                    (void*) 1,
                                    NULL,  /* callback          */
                                    NULL); /* callback_argument */
        }
    }
    system_critical_section_leave(context_ptr->object_to_refcount_cs);

    /* Configure the output variables */
    memcpy(out_result_object_ptrs,
           result_objects_ptr,
           sizeof(void*) * n_objects);

    /* All done */
    result = true;
end:
    if (!result)
    {
        if (result_objects_ptr != NULL)
        {
            for (uint32_t n_object = 0;
                          n_object < n_objects;
                        ++n_object)
            {
                if (result_objects_ptr[n_object] != NULL)
                {
                    switch (object_type)
                    {
                        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:      ral_buffer_release     ( (ral_buffer&)      result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: ral_framebuffer_release( (ral_framebuffer&) result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:     ral_program_release    ( (ral_program&)     result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     ral_sampler_release    ( (ral_sampler&)     result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SHADER:      ral_shader_release     ( (ral_shader&)      result_objects_ptr[n_object]); break;

                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
                        {
                            ral_texture_release( (ral_texture&) result_objects_ptr[n_object]);

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized RAL context object type");
                        }
                    } /* switch (object_type) */

                    result_objects_ptr[n_object] = NULL;
                } /* if (result_objects_ptr[n_object] != NULL) */
            } /* for (all potentially created RAL objects) */
        } /* if (result_objects_ptr != NULL) */
    } /* if (!result) */

    if (result_objects_ptr != NULL)
    {
        delete [] result_objects_ptr;

        result_objects_ptr = NULL;
    }

    return result;
}

/** TODO */
PRIVATE bool _ral_context_delete_objects(_ral_context*           context_ptr,
                                         ral_context_object_type object_type,
                                         uint32_t                in_n_objects,
                                         const void**            in_object_ptrs)
{
    system_critical_section cs            = NULL;
    system_resizable_vector object_vector = NULL;
    bool                    result        = false;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            cs            = context_ptr->buffers_cs;
            object_vector = context_ptr->buffers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            cs            = context_ptr->framebuffers_cs;
            object_vector = context_ptr->framebuffers;

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

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    } /* switch (object_type) */

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

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
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

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    } /* switch (object_type) */

    /* Release the objects */
    system_critical_section_enter(cs);
    {
        for (uint32_t n_object = 0;
                      n_object < in_n_objects;
                    ++n_object)
        {
            const void* object_ptr          = in_object_ptrs[n_object];
            uint32_t    object_vector_index = ITEM_NOT_FOUND;

            if (in_object_ptrs[n_object] == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Object at index [%d] is NULL",
                                  n_object);

                continue;
            } /* if (in_object_ptrs[n_object] == NULL) */

            if (object_vector != NULL)
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
            } /* if (object_vector != NULL) */

            switch (object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_BUFFER:      ral_buffer_release     ( (ral_buffer&)      object_ptr); break;
                case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: ral_framebuffer_release( (ral_framebuffer&) object_ptr); break;
                case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:     ral_program_release    ( (ral_program&)     object_ptr); break;
                case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     ral_sampler_release    ( (ral_sampler&)     object_ptr); break;
                case RAL_CONTEXT_OBJECT_TYPE_SHADER:      ral_shader_release     ( (ral_shader&)      object_ptr); break;
                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:     ral_texture_release    ( (ral_texture&)     object_ptr); break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized RAL context object type");

                    goto end;
                }
            } /* switch (object_type) */
        } /* for (all specified framebuffer instances) */
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
            system_hashed_ansi_string program_name      = NULL;
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
        } /* for (all specified programs) */
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
            system_hashed_ansi_string shader_name      = NULL;
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
        } /* for (all specified shaders) */
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
            system_hashed_ansi_string texture_filename      = NULL;
            system_hash64             texture_filename_hash = -1;
            system_hashed_ansi_string texture_name          = NULL;
            system_hash64             texture_name_hash     = -1;

            ral_texture_get_property(textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_FILENAME,
                                    &texture_filename);
            ral_texture_get_property(textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_NAME,
                                    &texture_name);

            if (texture_filename                                       != NULL &&
                system_hashed_ansi_string_get_length(texture_filename) >  0)
            {
                texture_filename_hash = system_hashed_ansi_string_get_hash(texture_filename);

                ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->textures_by_filename_map,
                                                            texture_filename_hash),
                                  "Texture with filename [%s] is not stored in the filename->texture hashmap.",
                                  system_hashed_ansi_string_get_buffer(texture_filename) );

                system_hash64map_remove(context_ptr->textures_by_filename_map,
                                        texture_filename_hash);
            } /* if (texture_filename != NULL) */

            texture_name_hash = system_hashed_ansi_string_get_hash(texture_name);

            ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->textures_by_name_map,
                                                        texture_name_hash),
                              "Texture [%s] is not stored in the name->texture hashmap.",
                              system_hashed_ansi_string_get_buffer(texture_name) );

            system_hash64map_remove(context_ptr->textures_by_name_map,
                                    texture_name_hash);
        } /* for (all specified textures) */
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
            raGL_backend_init( (raGL_backend) context_ptr->backend);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported backend type.");
        }
    } /* switch (context_ptr->backend_type) */
}

/** TODO */
PRIVATE void _ral_context_notify_backend_about_new_object(ral_context             context,
                                                          void*                   result_object,
                                                          ral_context_object_type object_type)
{
    ral_context_callback_objects_created_callback_arg callback_arg;
    _ral_context*                                     context_ptr  = (_ral_context*) context;

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

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
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
    } /* switch (object_type) */
}

/** TODO */
PRIVATE void _ral_context_on_texture_dropped_from_texture_pool(const void* callback_arg,
                                                                     void* context)
{
    _ral_texture_pool_callback_texture_dropped_arg* callback_arg_ptr = (_ral_texture_pool_callback_texture_dropped_arg*) callback_arg;
    _ral_context*                                   context_ptr      = (_ral_context*)                                   context;
    bool                                            result;

    result = _ral_context_delete_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                         callback_arg_ptr->n_textures,
                                         (const void**) callback_arg_ptr->textures);

    ASSERT_DEBUG_SYNC(result,
                      "Could not physically delete texture objects.");
}

/** TODO */
PRIVATE void _ral_context_release(void* context)
{
    _ral_context* context_ptr = (_ral_context*) context;

    /* Release the texture pool */
    if (context_ptr->texture_pool != NULL)
    {
        ral_texture_pool_release(context_ptr->texture_pool);

        context_ptr->texture_pool = NULL;
    }

    /* Release the back-end */
    if (context_ptr->backend                  != NULL &&
        context_ptr->pfn_backend_release_proc != NULL)
    {
        context_ptr->pfn_backend_release_proc(context_ptr->backend);

        context_ptr->backend = NULL;
    }

    /* Detect any leaking objects */
    uint32_t n_buffers      = 0;
    uint32_t n_framebuffers = 0;
    uint32_t n_programs     = 0;
    uint32_t n_samplers     = 0;
    uint32_t n_shaders      = 0;

    system_resizable_vector_get_property(context_ptr->buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_buffers);
    system_resizable_vector_get_property(context_ptr->framebuffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_framebuffers);
    system_resizable_vector_get_property(context_ptr->programs,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_programs);
    system_resizable_vector_get_property(context_ptr->samplers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_samplers);
    system_resizable_vector_get_property(context_ptr->shaders,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_shaders);

    ASSERT_DEBUG_SYNC(n_buffers == 0,
                      "Buffer object leak detected");
    ASSERT_DEBUG_SYNC(n_framebuffers == 0,
                      "Framebuffer object leak detected");
    ASSERT_DEBUG_SYNC(n_programs == 0,
                      "Program object leak detected");
    ASSERT_DEBUG_SYNC(n_samplers == 0,
                      "Sampler object leak detected");
    ASSERT_DEBUG_SYNC(n_shaders == 0,
                      "Shader object leak detected");
}

/** TODO */
PRIVATE void _ral_context_subscribe_for_notifications(_ral_context* context_ptr,
                                                      bool          should_subscribe)
{
    system_callback_manager texture_pool_callback_manager = NULL;

    ral_texture_pool_get_property(context_ptr->texture_pool,
                                  RAL_TEXTURE_POOL_PROPERTY_CALLBACK_MANAGER,
                                 &texture_pool_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(texture_pool_callback_manager,
                                                        RAL_TEXTURE_POOL_CALLBACK_ID_TEXTURE_DROPPED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _ral_context_on_texture_dropped_from_texture_pool,
                                                        context_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(texture_pool_callback_manager,
                                                           RAL_TEXTURE_POOL_CALLBACK_ID_TEXTURE_DROPPED,
                                                           _ral_context_on_texture_dropped_from_texture_pool,
                                                           context_ptr);
    }
}

/** Please see header for specification */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      demo_window               window)
{
    _ral_context* new_context_ptr = new (std::nothrow) _ral_context(name,
                                                                    window);

    ASSERT_ALWAYS_SYNC(new_context_ptr != NULL,
                       "Out of memory");
    if (new_context_ptr != NULL)
    {
        /* Instantiate the rendering back-end */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        demo_window_get_property(window,
                                 DEMO_WINDOW_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        switch (backend_type)
        {
            case RAL_BACKEND_TYPE_ES:
            case RAL_BACKEND_TYPE_GL:
            {
                new_context_ptr->backend_type                         = backend_type;
                new_context_ptr->backend                              = (void*) raGL_backend_create( (ral_context) new_context_ptr,
                                                                                                    name,
                                                                                                    backend_type);
                new_context_ptr->pfn_backend_get_framebuffer_proc     = raGL_backend_get_framebuffer;
                new_context_ptr->pfn_backend_get_property_proc        = raGL_backend_get_property;
                new_context_ptr->pfn_backend_release_proc             = raGL_backend_release;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported backend type requested.");
            }
        } /* switch(backend_type) */

        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_context_ptr,
                                                       _ral_context_release,
                                                       OBJECT_TYPE_RAL_CONTEXT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Contexts\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        /* Initialize the context */
        _ral_context_init(new_context_ptr);
    } /* if (new_context_ptr != NULL) */

    return (ral_context) new_context_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_buffers(ral_context                   context,
                                                   uint32_t                      n_buffers,
                                                   const ral_buffer_create_info* buffer_create_info_ptr,
                                                   ral_buffer*                   out_result_buffers_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_buffers == 0)
    {
        goto end;
    }

    if (buffer_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input buffer create info array is NULL");

        goto end;
    }

    if (out_result_buffers_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                         n_buffers,
                                         (void**) buffer_create_info_ptr,
                                         (void**) out_result_buffers_ptr);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_framebuffers(ral_context      context,
                                                        uint32_t         n_framebuffers,
                                                        ral_framebuffer* out_result_framebuffers_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_framebuffers == 0)
    {
        goto end;
    }

    if (out_result_framebuffers_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                                         n_framebuffers,
                                         NULL, /* object_create_info_ptrs */
                                         (void**) out_result_framebuffers_ptr);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_programs(ral_context                    context,
                                                    uint32_t                       n_create_info_items,
                                                    const ral_program_create_info* create_info_ptrs,
                                                    ral_program*                   out_result_program_ptrs)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (out_result_program_ptrs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                         n_create_info_items,
                                         (void**) create_info_ptrs,
                                         (void**) out_result_program_ptrs);

    if (result)
    {
        _ral_context_add_programs_to_program_hashmap(context_ptr,
                                                     n_create_info_items,
                                                     out_result_program_ptrs);
    } /* if (result) */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_samplers(ral_context                    context,
                                                    uint32_t                       n_create_info_items,
                                                    const ral_sampler_create_info* create_info_ptrs,
                                                    ral_sampler*                   out_result_sampler_ptrs)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (out_result_sampler_ptrs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                         n_create_info_items,
                                         (void**) create_info_ptrs,
                                         (void**) out_result_sampler_ptrs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_shaders(ral_context                   context,
                                                   uint32_t                      n_create_info_items,
                                                   const ral_shader_create_info* create_info_ptrs,
                                                   ral_shader*                   out_result_shader_ptrs)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        goto end;
    }

    if (create_info_ptrs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input shader create info array is NULL");

        goto end;
    }

    if (out_result_shader_ptrs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                         n_create_info_items,
                                         (void**) create_info_ptrs,
                                         (void**) out_result_shader_ptrs);

    if (result)
    {
        _ral_context_add_shaders_to_shader_hashmap(context_ptr,
                                                   n_create_info_items,
                                                   out_result_shader_ptrs);
    } /* if (result != NULL) */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_create_textures(ral_context                    context,
                                                    uint32_t                       n_textures,
                                                    const ral_texture_create_info* texture_create_info_ptr,
                                                    ral_texture*                   out_result_textures_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_textures == 0)
    {
        goto end;
    }

    if (texture_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture create info array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                         n_textures,
                                         (void**) texture_create_info_ptr,
                                         (void**) out_result_textures_ptr);

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_textures,
                                                      out_result_textures_ptr);
    } /* if (result != NULL) */
end:
    return result;
}

/** TODO */
PUBLIC EMERALD_API bool ral_context_create_textures_from_file_names(ral_context                      context,
                                                                    uint32_t                         n_file_names,
                                                                    const system_hashed_ansi_string* file_names_ptr,
                                                                    ral_texture*                     out_result_textures_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_file_names == 0)
    {
        goto end;
    }

    if (file_names_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input file name array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == NULL)
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
        } /* while (n_current_file_name != n_file_names) */

        if (n_file_names_to_handle > 0)
        {
            system_hashed_ansi_string* file_names_to_handle = NULL;
            ral_texture*               result_textures      = new (std::nothrow) ral_texture[n_file_names_to_handle];

            ASSERT_ALWAYS_SYNC(result_textures != NULL,
                               "Out of memory");

            system_resizable_vector_get_property(file_names_to_handle_vector,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                                &file_names_to_handle);

            result = _ral_context_create_objects(context_ptr,
                                                 RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME,
                                                 n_file_names_to_handle,
                                                 (void**) file_names_to_handle,
                                                 (void**) result_textures);

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
                } /* for (all file names specified on input) */

                ASSERT_DEBUG_SYNC(n_returned_textures == n_file_names_to_handle,
                                  "Internal error");

                _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                              n_file_names_to_handle,
                                                              result_textures);
            } /* if (result != NULL) */

            delete [] result_textures;
            result_textures = NULL;
        } /* if (n_file_names_to_handle > 0) */
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
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_images == 0)
    {
        goto end;
    }

    if (images == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input image array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE,
                                         n_images,
                                         (void**) images,
                                         (void**) out_result_textures_ptr);

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_images,
                                                      out_result_textures_ptr);

    } /* if (result != NULL) */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_context_delete_objects(ral_context             context,
                                                   ral_context_object_type object_type,
                                                   uint32_t                in_n_objects,
                                                   const void**            in_objects)
{
    _ral_context* context_ptr = (_ral_context*) context;
    const void*   object_ptrs[16];
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    } /* if (context == NULL) */

    if (in_n_objects == 0)
    {
        goto end;
    }

    if (in_objects == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input object array is NULL");

        goto end;
    }

    /* Decrement the reference counter for the object. We will only proceed with the release process
     * for those objects, whose refcounter drops to zero */
    uint32_t n_objects = 0;

    ASSERT_DEBUG_SYNC(in_n_objects < sizeof(object_ptrs) / sizeof(object_ptrs[0]),
                      "TODO");

    system_critical_section_enter(context_ptr->object_to_refcount_cs);
    {
        for (uint32_t n_object = 0;
                      n_object < in_n_objects;
                    ++n_object)
        {
            uint32_t object_ref_counter = 0;

            if (in_objects[n_object] == NULL)
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

            if (--object_ref_counter == 0)
            {
                object_ptrs[n_objects++] = in_objects[n_object];
            }

            system_hash64map_remove(context_ptr->object_to_refcount_map,
                                    (system_hash64) in_objects[n_object]);

            if (object_ref_counter != 0)
            {
                system_hash64map_insert(context_ptr->object_to_refcount_map,
                                        (system_hash64) in_objects[n_object],
                                        (void*) object_ref_counter,
                                        NULL, /* calback           */
                                        NULL  /* callback_argument */);
            }
        } /* for (all input objects) */
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
        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            /* Nothing to do for these object types */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            _ral_context_delete_programs_from_program_hashmap(context_ptr,
                                                              n_objects,
                                                              (ral_program*) object_ptrs);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            _ral_context_delete_shaders_from_shader_hashmap(context_ptr,
                                                            n_objects,
                                                            (ral_shader*) object_ptrs);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            _ral_context_delete_textures_from_texture_hashmaps(context_ptr,
                                                               n_objects,
                                                               (ral_texture*) object_ptrs);

            /* Note: Instead of deleting the textures, we stash them back in the texture pool so that
             *       they can be re-used.. unless the texture pool is no longer available, in which
             *       case we actually do release them. :) */
            if (context_ptr->texture_pool != NULL)
            {
                for (uint32_t n_texture = 0;
                              n_texture < n_objects;
                            ++n_texture)
                {
                    ral_texture_pool_add(context_ptr->texture_pool,
                                         (ral_texture) object_ptrs[n_texture]);
                }
            }
            else
            {
                _ral_texture_pool_callback_texture_dropped_arg fake_callback_arg;

                fake_callback_arg.n_textures = n_objects;
                fake_callback_arg.textures   = (ral_texture*) object_ptrs;

                _ral_context_on_texture_dropped_from_texture_pool(&fake_callback_arg,
                                                                  context_ptr);
            }

            result = true;
            goto end;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL object type");
        }
    } /* switch (object_type) */

    /* Release the objects */
    result = _ral_context_delete_objects(context_ptr,
                                         object_type,
                                         n_objects,
                                         object_ptrs);

end:
    return result;
}

/** TODO */
PUBLIC EMERALD_API raGL_buffer ral_context_get_buffer_gl(ral_context context,
                                                         ral_buffer  buffer)
{
    raGL_buffer   buffer_raGL = NULL;
    _ral_context* context_ptr = (_ral_context*) context;

    raGL_backend_get_buffer(context_ptr->backend,
                            buffer,
                  (void**) &buffer_raGL);

    return buffer_raGL;
}

/** TODO */
PUBLIC EMERALD_API raGL_framebuffer ral_context_get_framebuffer_gl(ral_context     context,
                                                                   ral_framebuffer framebuffer)
{
    _ral_context*    context_ptr      = (_ral_context*) context;
    raGL_framebuffer framebuffer_raGL = NULL;

    raGL_backend_get_framebuffer(context_ptr->backend,
                                 framebuffer,
                       (void**) &framebuffer_raGL);

    return framebuffer_raGL;
}

/** TODO */
PUBLIC EMERALD_API ogl_context ral_context_get_gl_context(ral_context context)
{
    raGL_backend backend         = (raGL_backend) ((_ral_context*) context)->backend;
    ogl_context  backend_context = NULL;

    raGL_backend_get_property(backend,
                              RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                             &backend_context);

    return backend_context;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_program ral_context_get_program_by_name(ral_context               context,
                                                               system_hashed_ansi_string name)
{
    _ral_context* context_ptr = (_ral_context*) context;
    ral_program   result      = NULL;

    /* Sanity checks..*/
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == NULL)
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
PUBLIC EMERALD_API raGL_program ral_context_get_program_gl(ral_context context,
                                                           ral_program program)
{
    _ral_context* context_ptr  = (_ral_context*) context;
    raGL_program  program_raGL = NULL;

    raGL_backend_get_program(context_ptr->backend,
                             program,
                   (void**) &program_raGL);

    return program_raGL;
}


/** Please see header for specification */
PUBLIC EMERALD_API void ral_context_get_property(ral_context          context,
                                                 ral_context_property property,
                                                 void*                out_result_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    } /* if (context == NULL) */

    /* If this is a backend-specific property, forward the request to the backend instance.
     * Otherwise, try to handle it. */
    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_BACKEND:
        {
            *(raGL_backend*) out_result_ptr = (raGL_backend) context_ptr->backend;

            break;
        }

        case RAL_CONTEXT_PROPERTY_BACKEND_TYPE:
        {
            *(ral_backend_type*) out_result_ptr = context_ptr->backend_type;

            break;
        }

        case RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT:
        case RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
        case RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_ATTACHMENT_TEXTURE_FORMAT:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_SIZE:
        {
            context_ptr->pfn_backend_get_property_proc(context_ptr->backend,
                                                       property,
                                                       out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = context_ptr->callback_manager;

            break;
        }

        case RAL_CONTEXT_PROPERTY_RENDERING_HANDLER:
        {
            *(ogl_rendering_handler*) out_result_ptr = context_ptr->rendering_handler;

            break;
        }

        case RAL_CONTEXT_PROPERTY_WINDOW_DEMO:
        {
            *(demo_window*) out_result_ptr = context_ptr->window_demo;

            break;
        }

        case RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM:
        {
            *(system_window*) out_result_ptr = context_ptr->window_system;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecpognized ral_context_property value.");
        }
    } /* switch (property) */
end:
    ;
}

/** TODO */
PUBLIC EMERALD_API raGL_sampler ral_context_get_sampler_gl(ral_context context,
                                                           ral_sampler sampler)
{
    raGL_sampler  sampler_raGL = NULL;
    _ral_context* context_ptr  = (_ral_context*) context;

    raGL_backend_get_sampler(context_ptr->backend,
                             sampler,
                   (void**) &sampler_raGL);

    return sampler_raGL;
}

/** TODO */
PUBLIC EMERALD_API raGL_shader ral_context_get_shader_gl(ral_context context,
                                                         ral_shader  shader)
{
    _ral_context* context_ptr = (_ral_context*) context;
    raGL_shader   shader_raGL = NULL;

    raGL_backend_get_shader(context_ptr->backend,
                            shader,
                  (void**) &shader_raGL);

    return shader_raGL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader ral_context_get_shader_by_name(ral_context               context,
                                                             system_hashed_ansi_string name)
{
    _ral_context* context_ptr = (_ral_context*) context;
    ral_shader    result      = NULL;

    /* Sanity checks..*/
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == NULL)
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
    _ral_context* context_ptr = (_ral_context*) context;
    ral_texture   result      = NULL;

    /* Sanity checks..*/
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (file_name == NULL)
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
    _ral_context* context_ptr = (_ral_context*) context;
    ral_texture   result      = NULL;

    /* Sanity checks..*/
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == NULL)
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
PUBLIC EMERALD_API raGL_texture ral_context_get_texture_gl(ral_context context,
                                                           ral_texture texture)
{
    _ral_context* context_ptr  = (_ral_context*) context;
    raGL_texture  texture_raGL = NULL;

    raGL_backend_get_texture(context_ptr->backend,
                             texture,
                   (void**) &texture_raGL);

    return texture_raGL;
}

PUBLIC GLuint ral_context_get_texture_gl_id(ral_context context,
                                            ral_texture texture)
{
    _ral_context* context_ptr  = (_ral_context*) context;
    GLuint        result       = 0;
    raGL_texture  texture_raGL = NULL;

    raGL_backend_get_texture (context_ptr->backend,
                              texture,
                    (void**) &texture_raGL);
    raGL_texture_get_property(texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &result);

    return result;
}

/** TODO */
PUBLIC EMERALD_API void ral_context_retain_object(ral_context             context,
                                                  ral_context_object_type object_type,
                                                  void*                   object)
{
    _ral_context* context_ptr = (_ral_context*) context;

    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL RAL context specified.");

        goto end;
    }

    system_critical_section_enter(context_ptr->object_to_refcount_cs);
    {
        uint32_t ref_counter = 0;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->object_to_refcount_map,
                                                    (system_hash64) object),
                          "No reference counter associated with the specified object.");

        system_hash64map_get(context_ptr->object_to_refcount_map,
                             (system_hash64) object,
                            &ref_counter);

        ref_counter++;

        system_hash64map_remove(context_ptr->object_to_refcount_map,
                                (system_hash64) object);
        system_hash64map_insert(context_ptr->object_to_refcount_map,
                                (system_hash64) object,
                                (void*) ref_counter,
                                NULL,  /* callback          */
                                NULL); /* callback_argument */
    }
    system_critical_section_leave(context_ptr->object_to_refcount_cs);
end:
    ;
}

/** TODO */
PUBLIC void ral_context_set_property(ral_context          context,
                                     ral_context_property property,
                                     const void*          data)
{
    _ral_context* context_ptr = (_ral_context*) context;

    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_BACKEND:
        {
            ASSERT_DEBUG_SYNC(context_ptr->backend == NULL,
                              "Backend is not NULL");

            context_ptr->backend = *(void**) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_property value.");
        }
    } /* switch (property) */
}