/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_mesh_dataset.h"
#include "mesh/mesh.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definitions */
typedef struct _lw_mesh_dataset_mesh
{
    system_hashed_ansi_string filename;
    bool                      is_shadow_caster;
    bool                      is_shadow_receiver;
    system_hashed_ansi_string name;

    explicit _lw_mesh_dataset_mesh(__in __notnull system_hashed_ansi_string in_name)
    {
        filename           = NULL;
        is_shadow_caster   = false;
        is_shadow_receiver = false;
        name               = in_name;
    }
} _lw_mesh_dataset_mesh;

typedef struct
{
    system_resizable_vector meshes;

    REFCOUNT_INSERT_VARIABLES
} _lw_mesh_dataset;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_mesh_dataset, lw_mesh_dataset, _lw_mesh_dataset);


/** TODO */
PRIVATE void _lw_mesh_dataset_get_collada_mesh_name(__in_opt  system_hashed_ansi_string  in_name,
                                                    __in      uint32_t                   n_iteration,
                                                    __out_opt uint32_t*                  out_n_iterations,
                                                    __out_opt system_hashed_ansi_string* out_result)
{
    if (out_n_iterations != NULL)
    {
        *out_n_iterations = 2;
    }

    if (out_result != NULL)
    {
        const char* in_name_raw      = system_hashed_ansi_string_get_buffer(in_name);
        uint32_t    in_name_length   = strlen(in_name_raw);
        uint32_t    temp_buffer_size = in_name_length + 1 + ((n_iteration == 1) ? 2 : 0);
        char*       temp_buffer      = new char[temp_buffer_size];

        memset(temp_buffer,
               0,
               temp_buffer_size);

        memcpy(temp_buffer,
               in_name_raw,
               in_name_length);

        /* Replace spaces with underlines */
        for (uint32_t n_character = 0;
                      n_character < temp_buffer_size - 1;
                    ++n_character)
        {
            if (temp_buffer[n_character] == ' ')
            {
                temp_buffer[n_character] = '_';
            }
        }

        if (n_iteration == 1)
        {
            temp_buffer[in_name_length    ] = '_';
            temp_buffer[in_name_length + 1] = '1';
        }

        /* Form the result */
        *out_result = system_hashed_ansi_string_create(temp_buffer);

        delete [] temp_buffer;
        temp_buffer = NULL;
    }
}

/** TODO */
PRIVATE system_hash64map _lw_mesh_dataset_get_filename_to_mesh_name_map(__in __notnull lw_mesh_dataset dataset)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;
    const uint32_t    n_meshes    = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);
    system_hash64map  result      = system_hash64map_create                       (sizeof(_lw_mesh_dataset_mesh*) );

    for (uint32_t n_mesh = 0;
                  n_mesh < n_meshes;
                ++n_mesh)
    {
        system_hash64          mesh_filename_hash = 0;
        _lw_mesh_dataset_mesh* mesh_ptr           = NULL;

        if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                    n_mesh,
                                                   &mesh_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh descriptor");

            continue;
        }

        mesh_filename_hash = system_hashed_ansi_string_get_hash(mesh_ptr->filename);

        if (!system_hash64map_contains(result,
                                       mesh_filename_hash) )
        {
            system_hash64map_insert(result,
                                    mesh_filename_hash,
                                    mesh_ptr->name,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }
    } /* for (all meshes) */

    return result;
}

/** TODO */
PRIVATE system_hash64map _lw_mesh_dataset_get_filename_to_vector_of_meshes_map(__in __notnull lw_mesh_dataset dataset)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;
    const uint32_t    n_meshes    = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);
    system_hash64map  result      = system_hash64map_create(sizeof(system_resizable_vector) );

    for (uint32_t n_mesh = 0;
                  n_mesh < n_meshes;
                ++n_mesh)
    {
        system_hash64           mesh_filename_hash = 0;
        _lw_mesh_dataset_mesh*  mesh_ptr           = NULL;
        system_resizable_vector mesh_vector        = NULL;

        if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                    n_mesh,
                                                   &mesh_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot retrieve mesh descriptor");

            continue;
        }

        mesh_filename_hash = system_hashed_ansi_string_get_hash(mesh_ptr->filename);

        if (!system_hash64map_contains(result,
                                       mesh_filename_hash) )
        {
            system_resizable_vector new_vector = system_resizable_vector_create(4, /* capacity */
                                                                                sizeof(_lw_mesh_dataset_mesh*) );

            system_hash64map_insert(result,
                                    mesh_filename_hash,
                                    new_vector,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }

        if (!system_hash64map_get(result,
                                  mesh_filename_hash,
                                 &mesh_vector)        ||
            mesh_vector == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot retrieve mesh vector");

            continue;
        }

        system_resizable_vector_push(mesh_vector,
                                     mesh_ptr);
    } /* for (all meshes) */

    /* All done */
    return result;
}

/** TODO */
PRIVATE void _lw_mesh_dataset_release(void* arg)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) arg;

    if (dataset_ptr->meshes != NULL)
    {
        _lw_mesh_dataset_mesh* mesh_ptr = NULL;

        while (system_resizable_vector_pop(dataset_ptr->meshes,
                                          &mesh_ptr) )
        {
            delete mesh_ptr;

            mesh_ptr = NULL;
        }

        system_resizable_vector_release(dataset_ptr->meshes);
        dataset_ptr->meshes = NULL;
    } /* if (dataset_ptr->meshes != NULL) */
}


/* Please see header for specification */
PUBLIC EMERALD_API lw_mesh_dataset_mesh_id lw_mesh_dataset_add_mesh(__in __notnull lw_mesh_dataset           dataset,
                                                                    __in __notnull system_hashed_ansi_string name)
{
    _lw_mesh_dataset*       dataset_ptr  = (_lw_mesh_dataset*) dataset;
    _lw_mesh_dataset_mesh*  new_mesh_ptr = NULL;
    lw_mesh_dataset_mesh_id result_id    = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);

    new_mesh_ptr = new (std::nothrow) _lw_mesh_dataset_mesh(name);

    ASSERT_ALWAYS_SYNC(new_mesh_ptr != NULL,
                       "Out of memory");
    if (new_mesh_ptr != NULL)
    {
        system_resizable_vector_push(dataset_ptr->meshes,
                                     new_mesh_ptr);
    }
    else
    {
        result_id = -1;
    }

    return result_id;

}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_apply_to_scene(__in __notnull lw_mesh_dataset dataset,
                                                       __in           scene           scene)
{
    _lw_mesh_dataset* dataset_ptr                      = (_lw_mesh_dataset*) dataset;
    uint32_t          n_lw_mesh_getter_name_iterations = 0;
    const uint32_t    n_meshes                         = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);

    _lw_mesh_dataset_get_collada_mesh_name(NULL,
                                           0,
                                          &n_lw_mesh_getter_name_iterations,
                                           NULL);

    /* 1. Objects that share the same vertex/normal/etc. data are represented by the same filename.
     *    Map each file name to one of the objects that uses the data defined by the file.
     *    We will use this information to make all other meshes, which use the same resources,
     *    instantiated.
     */
    system_hash64map filename_to_mesh_name_map = _lw_mesh_dataset_get_filename_to_mesh_name_map(dataset);

    /* 2. Retrieve a map that maps filenames to a vector of meshes. This will be useful to avoid the O(n^2)
     *    complexity in the final step. */
    system_hash64map filename_to_vector_of_meshes_map = _lw_mesh_dataset_get_filename_to_vector_of_meshes_map(dataset);

    /* 3. Iterate over all filenames. For all meshes that have a different name than the one stored
     *    in filename_to_mesh_name_map, drop all the stored data and toggle mesh instantiation, pointing
     *    to the "base" object whose name we know from the filename_to_mesh_name map.
     */
    const uint32_t n_filename_to_vector_of_meshes_map_entries = system_hash64map_get_amount_of_elements(filename_to_vector_of_meshes_map);

    for (uint32_t n_filename = 0;
                  n_filename < n_filename_to_vector_of_meshes_map_entries;
                ++n_filename)
    {
        system_hash64           base_filename_hash         = 0;
        uint32_t                n_vector_of_meshes_entries = 0;
        system_resizable_vector vector_of_meshes           = NULL;

        if (!system_hash64map_get_element_at(filename_to_vector_of_meshes_map,
                                             n_filename,
                                            &vector_of_meshes,
                                            &base_filename_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh vector");

            continue;
        }

        /* Retrieve name of the base mesh, whose data all other meshes built of the same vertex data
         * should use. */
        scene_mesh                base_mesh           = NULL;
        mesh                      base_mesh_gpu       = NULL;
        system_hashed_ansi_string base_mesh_name      = NULL;
        system_hashed_ansi_string base_mesh_name_copy = NULL;

        if (!system_hash64map_get(filename_to_mesh_name_map,
                                  base_filename_hash,
                                 &base_mesh_name_copy) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve base mesh name.");

            continue;
        }

        for (unsigned int n_iteration = 0;
                          n_iteration < n_lw_mesh_getter_name_iterations && base_mesh == NULL;
                        ++n_iteration)
        {
            base_mesh_name = base_mesh_name_copy;

            _lw_mesh_dataset_get_collada_mesh_name(base_mesh_name,
                                                   n_iteration,
                                                   NULL, /* out_n_iterations */
                                                  &base_mesh_name);

            /* Retrieve the scene mesh instance */
            base_mesh = scene_get_mesh_instance_by_name(scene,
                                                        base_mesh_name);
        }

        if (base_mesh == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve base mesh.");

            continue;
        }

        scene_mesh_get_property(base_mesh,
                                SCENE_MESH_PROPERTY_MESH,
                               &base_mesh_gpu);

        /* Iterate over all meshes which use the same source file data */
        n_vector_of_meshes_entries = system_resizable_vector_get_amount_of_elements(vector_of_meshes);

        ASSERT_DEBUG_SYNC(n_vector_of_meshes_entries >= 1,
                          "Mesh vector found but holds no items");

        for (uint32_t n_mesh_entry = 0;
                      n_mesh_entry < n_vector_of_meshes_entries;
                    ++n_mesh_entry)
        {
            _lw_mesh_dataset_mesh* mesh_ptr = NULL;

            if (!system_resizable_vector_get_element_at(vector_of_meshes,
                                                        n_mesh_entry,
                                                       &mesh_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh descriptor");

                break;
            }

            /* Retrieve scene instance of the mesh */
            scene_mesh                mesh_to_be_instantiated      = NULL;
            mesh                      mesh_to_be_instantiated_gpu  = NULL;
            system_hashed_ansi_string mesh_to_be_instantiated_name = NULL;

            for (unsigned int n_iteration = 0;
                              n_iteration < n_lw_mesh_getter_name_iterations && mesh_to_be_instantiated == NULL;
                            ++n_iteration)
            {
                mesh_to_be_instantiated_name = mesh_ptr->name;

                _lw_mesh_dataset_get_collada_mesh_name(mesh_to_be_instantiated_name,
                                                       n_iteration,
                                                       NULL, /* out_n_iterations */
                                                      &mesh_to_be_instantiated_name);

                /* Retrieve the scene mesh instance */
                mesh_to_be_instantiated = scene_get_mesh_instance_by_name(scene,
                                                                          mesh_to_be_instantiated_name);
            }

            ASSERT_DEBUG_SYNC(mesh_to_be_instantiated != NULL,
                              "Could not identify a mesh in the scene container that needs to be instantiated");

            scene_mesh_get_property(mesh_to_be_instantiated,
                                    SCENE_MESH_PROPERTY_MESH,
                                   &mesh_to_be_instantiated_gpu);

            /* Skip base meshes */
            if (system_hashed_ansi_string_is_equal_to_hash_string(mesh_to_be_instantiated_name,
                                                                  base_mesh_name) )
            {
                continue;
            }

            /* Convert the mesh instance to an instantiated version. */
            mesh_set_as_instantiated(mesh_to_be_instantiated_gpu,
                                     base_mesh_gpu);
        } /* for (all mesh entries) */
    } /* for (all file names) */

    /* All done. OK to release the reosurces */
    if (filename_to_mesh_name_map != NULL)
    {
        system_hash64map_release(filename_to_mesh_name_map);

        filename_to_mesh_name_map = NULL;
    }

    if (filename_to_vector_of_meshes_map != NULL)
    {
        system_hash64map_release(filename_to_vector_of_meshes_map);

        filename_to_vector_of_meshes_map = NULL;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_mesh_dataset lw_mesh_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    _lw_mesh_dataset* dataset_ptr = new (std::nothrow) _lw_mesh_dataset;

    ASSERT_ALWAYS_SYNC(dataset_ptr != NULL, "Out of memory");
    if (dataset_ptr != NULL)
    {
        memset(dataset_ptr,
               0,
               sizeof(_lw_mesh_dataset) );

        dataset_ptr->meshes = system_resizable_vector_create(4, /* capacity */
                                                             sizeof(_lw_mesh_dataset_mesh*) );

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(dataset_ptr,
                                                       _lw_mesh_dataset_release,
                                                       OBJECT_TYPE_LW_MESH_DATASET,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave mesh data-sets\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }

    return (lw_mesh_dataset) dataset_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_get_mesh_property(__in  __notnull lw_mesh_dataset               dataset,
                                                          __in            lw_mesh_dataset_mesh_id       mesh_id,
                                                          __in            lw_mesh_dataset_mesh_property property,
                                                          __out __notnull void*                         out_result)
{
    _lw_mesh_dataset*      dataset_ptr = (_lw_mesh_dataset*) dataset;
    _lw_mesh_dataset_mesh* mesh_ptr    = NULL;

    if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                mesh_id,
                                               &mesh_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized mesh ID requested");

        goto end;
    }

    switch (property)
    {
        case LW_MESH_DATASET_MESH_PROPERTY_FILENAME:
        {
            *(system_hashed_ansi_string*) out_result = mesh_ptr->filename;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER:
        {
            *(bool*) out_result = mesh_ptr->is_shadow_caster;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER:
        {
            *(bool*) out_result = mesh_ptr->is_shadow_receiver;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = mesh_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_mesh_dataset_mesh_property value");
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for specification */
PUBLIC EMERALD_API lw_mesh_dataset lw_mesh_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                        __in __notnull system_file_serializer    serializer)
{
    lw_mesh_dataset dataset = lw_mesh_dataset_create(name);

    ASSERT_ALWAYS_SYNC(dataset != NULL,
                       "LW mesh dataset creation failed");
    if (dataset != NULL)
    {
        _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;
        uint32_t          n_meshes    = 0;

        system_file_serializer_read(serializer,
                                    sizeof(n_meshes),
                                   &n_meshes);

        for (uint32_t n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
        {
            system_hashed_ansi_string mesh_filename           = NULL;
            lw_mesh_dataset_mesh_id   mesh_id                 = -1;
            bool                      mesh_is_shadow_caster   = false;
            bool                      mesh_is_shadow_receiver = false;
            system_hashed_ansi_string mesh_name               = NULL;

            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &mesh_filename);
            system_file_serializer_read                   (serializer,
                                                           sizeof(mesh_is_shadow_caster),
                                                          &mesh_is_shadow_caster);
            system_file_serializer_read                   (serializer,
                                                           sizeof(mesh_is_shadow_receiver),
                                                          &mesh_is_shadow_receiver);
            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &mesh_name);

            mesh_id = lw_mesh_dataset_add_mesh(dataset,
                                               mesh_name);

            lw_mesh_dataset_set_mesh_property(dataset,
                                              mesh_id,
                                              LW_MESH_DATASET_MESH_PROPERTY_FILENAME,
                                             &mesh_filename);
            lw_mesh_dataset_set_mesh_property(dataset,
                                              mesh_id,
                                              LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER,
                                             &mesh_is_shadow_caster);
            lw_mesh_dataset_set_mesh_property(dataset,
                                              mesh_id,
                                              LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER,
                                             &mesh_is_shadow_receiver);
        } /* for (all meshes) */
    } /* if (dataset != NULL) */

    return dataset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void lw_mesh_dataset_save(__in __notnull lw_mesh_dataset        dataset,
                                             __in __notnull system_file_serializer serializer)
{
    _lw_mesh_dataset* dataset_ptr = (_lw_mesh_dataset*) dataset;
    const uint32_t    n_meshes    = system_resizable_vector_get_amount_of_elements(dataset_ptr->meshes);

    system_file_serializer_write(serializer,
                                 sizeof(n_meshes),
                                &n_meshes);

    for (uint32_t n_mesh = 0;
                  n_mesh < n_meshes;
                ++n_mesh)
    {
        _lw_mesh_dataset_mesh* mesh_ptr = NULL;

        if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                    n_mesh,
                                                   &mesh_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh descriptor");

            continue;
        }

        system_file_serializer_write_hashed_ansi_string(serializer,
                                                        mesh_ptr->filename);
        system_file_serializer_write                   (serializer,
                                                        sizeof(mesh_ptr->is_shadow_caster),
                                                       &mesh_ptr->is_shadow_caster);
        system_file_serializer_write                   (serializer,
                                                        sizeof(mesh_ptr->is_shadow_receiver),
                                                       &mesh_ptr->is_shadow_receiver);
        system_file_serializer_write_hashed_ansi_string(serializer,
                                                        mesh_ptr->name);
    } /* for (all meshes) */
}

/* Please see header for specification */;
PUBLIC EMERALD_API void lw_mesh_dataset_set_mesh_property(__in __notnull lw_mesh_dataset               dataset,
                                                          __in           lw_mesh_dataset_mesh_id       mesh_id,
                                                          __in           lw_mesh_dataset_mesh_property property,
                                                          __in __notnull const void*                   data)
{
    _lw_mesh_dataset*      dataset_ptr = (_lw_mesh_dataset*) dataset;
    _lw_mesh_dataset_mesh* mesh_ptr    = NULL;

    if (!system_resizable_vector_get_element_at(dataset_ptr->meshes,
                                                mesh_id,
                                               &mesh_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized mesh ID requested");

        goto end;
    }

    switch (property)
    {
        case LW_MESH_DATASET_MESH_PROPERTY_FILENAME:
        {
            mesh_ptr->filename = *(system_hashed_ansi_string*) data;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER:
        {
            mesh_ptr->is_shadow_caster = *(bool*) data;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER:
        {
            mesh_ptr->is_shadow_receiver = *(bool*) data;

            break;
        }

        case LW_MESH_DATASET_MESH_PROPERTY_NAME:
        {
            mesh_ptr->name = *(system_hashed_ansi_string*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized lw_mesh_dataset_mesh_property value");
        }
    } /* switch (property) */

end:
    ;
}