/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_animation.h"
#include "collada/collada_data_camera.h"
#include "collada/collada_data_image.h"
#include "collada/collada_data_input.h"
#include "collada/collada_data_light.h"
#include "collada/collada_data_scene_graph_node_camera_instance.h"
#include "collada/collada_data_scene_graph_node_geometry_instance.h"
#include "collada/collada_data_scene_graph_node_light_instance.h"
#include "collada/collada_data_scene.h"
#include "collada/collada_data_source.h"
#include "collada/collada_data_transformation.h"
#include "collada/collada_data_sampler.h"
#include "collada/collada_scene_generator.h"
#include "collada/collada_value.h"
#include "curve/curve_container.h"
#include "gfx/gfx_image.h"
#include "ogl/ogl_texture.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_texture.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "system/system_variant.h"
#include <sstream>

/* Forward declarations */
PRIVATE void             _collada_scene_generator_process_collada_data_node          (__in __notnull collada_data                  data,
                                                                                      __in __notnull collada_data_scene_graph_node node,
                                                                                      __in __notnull scene                         scene,
                                                                                      __in __notnull scene_graph_node              parent_node,
                                                                                      __in __notnull scene_graph                   graph,
                                                                                      __in __notnull system_hash64map              scene_to_dag_node_map,
                                                                                      __in __notnull ogl_context                   context);
PRIVATE void             _collada_scene_generator_process_camera_instance_node_item  (__in __notnull collada_data                  data,
                                                                                      __in __notnull scene                         scene,
                                                                                      __in __notnull scene_graph                   graph,
                                                                                      __in __notnull scene_graph_node              parent_node,
                                                                                      __in __notnull void*                         node_item_data);
PRIVATE void             _collada_scene_generator_process_geometry_instance_node_item(__in __notnull collada_data                  data,
                                                                                      __in __notnull scene                         scene,
                                                                                      __in __notnull scene_graph                   graph,
                                                                                      __in __notnull scene_graph_node              parent_node,
                                                                                      __in __notnull void*                         node_item_data,
                                                                                      __in __notnull ogl_context                   context);
PRIVATE void             _collada_scene_generator_process_light_instance_node_item   (__in __notnull collada_data                  data,
                                                                                      __in __notnull scene                         scene,
                                                                                      __in __notnull scene_graph                   graph,
                                                                                      __in __notnull scene_graph_node              parent_node,
                                                                                      __in __notnull void*                         node_item_data);
PRIVATE scene_graph_node _collada_scene_generator_process_transformation_node_item   (__in __notnull scene_graph                   graph,
                                                                                      __in __notnull scene_graph_node              parent_node,
                                                                                      __in __notnull void*                         node_item_data,
                                                                                      __in __notnull system_hashed_ansi_string     node_item_sid);

/** TODO */
PRIVATE curve_container _collada_scene_generator_create_curve_container_from_collada_value(__in __notnull collada_value value)
{
    curve_container           result     = NULL;
    system_hashed_ansi_string value_id   = NULL;
    collada_value_type        value_type = COLLADA_VALUE_TYPE_UNKNOWN;

    /* Retrieve value type */
    collada_value_get_property(value,
                               COLLADA_VALUE_PROPERTY_TYPE,
                              &value_type);

    /* Generate an unique ID for the curve.
     *
     * This is an easy task for collada_value's that hold animation data.
     *
     * For float values, this is not trivial and would involve merging
     * COLLADA node paths, which is not really that much useful, given the
     * fact we're talking static values here.
     *
     * TODO: Improve if necessary.
     */
    static int        curve_counter = 0;
    std::stringstream curve_name_sstream;

    if (value_type == COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION)
    {
        collada_data_animation    value_animation    = NULL;
        system_hashed_ansi_string value_animation_id = NULL;

        collada_value_get_property(value,
                                   COLLADA_VALUE_PROPERTY_COLLADA_DATA_ANIMATION,
                                  &value_animation);

        ASSERT_DEBUG_SYNC(value_animation != NULL,
                          "COLLADA value holds a NULL animation instance");

        collada_data_animation_get_property(value_animation,
                                            COLLADA_DATA_ANIMATION_PROPERTY_ID,
                                           &value_animation_id);

        curve_name_sstream << system_hashed_ansi_string_get_buffer(value_animation_id);
    }
    else
    {
        /* Fall-back path */
        curve_name_sstream << "Curve " << (curve_counter++);
    }

    value_id = system_hashed_ansi_string_create(curve_name_sstream.str().c_str() );

    /* Spawn the new container */
    result = curve_container_create(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(value_id),
                                                                                            " curve container"),
                                    SYSTEM_VARIANT_FLOAT); /* n_dimension */

    /* If this is a static value, things are simple */
    if (value_type == COLLADA_VALUE_TYPE_FLOAT)
    {
        /* Set the default value for the curve */
        system_variant float_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);
        {
            float value_float = 0.0f;

            collada_value_get_property(value,
                                       COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
                                       &value_float);

            system_variant_set_float         (float_variant,
                                              value_float);
            curve_container_set_default_value(result,
                                              float_variant);
        }
        system_variant_release(float_variant);

        /* Done! */
        goto end;
    }

    /* Means we need to initialize the curve container for a collada_data_animation instance. */
    ASSERT_DEBUG_SYNC(value_type == COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                      "Unrecognized COLLADA value type");

    /* Retrieve source instances, as well as interpolation data. */
    collada_data_animation   animation                       = NULL;
    collada_data_sampler     animation_sampler               = NULL;
    collada_data_float_array input_float_array               = NULL;
    const float*             input_float_array_data          = NULL;
    uint32_t                 input_float_array_n_components  = 0;
    uint32_t                 input_float_array_n_values      = 0;
    collada_data_source      input_source                    = NULL;
    system_resizable_vector  interpolation_vector            = NULL;
    uint32_t                 interpolation_vector_n_values   = 0;
    collada_data_float_array output_float_array              = NULL;
    const float*             output_float_array_data         = NULL;
    uint32_t                 output_float_array_n_components = 0;
    uint32_t                 output_float_array_n_values     = 0;
    collada_data_source      output_source                   = NULL;

    collada_value_get_property             (value,
                                            COLLADA_VALUE_PROPERTY_COLLADA_DATA_ANIMATION,
                                           &animation);
    collada_data_animation_get_property    (animation,
                                            COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER,
                                           &animation_sampler);
    collada_data_sampler_get_input_property(animation_sampler,
                                            COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INPUT,
                                            COLLADA_DATA_SAMPLER_INPUT_PROPERTY_SOURCE,
                                           &input_source);
    collada_data_sampler_get_input_property(animation_sampler,
                                            COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_OUTPUT,
                                            COLLADA_DATA_SAMPLER_INPUT_PROPERTY_SOURCE,
                                           &output_source);
    collada_data_sampler_get_input_property(animation_sampler,
                                            COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INTERPOLATION,
                                            COLLADA_DATA_SAMPLER_INPUT_PROPERTY_INTERPOLATION_DATA,
                                           &interpolation_vector);

    collada_data_source_get_source_float_data(input_source,
                                             &input_float_array);
    collada_data_source_get_source_float_data(output_source,
                                             &output_float_array);

    collada_data_float_array_get_property(input_float_array,
                                          COLLADA_DATA_FLOAT_ARRAY_PROPERTY_DATA,
                                         &input_float_array_data);
    collada_data_float_array_get_property(output_float_array,
                                          COLLADA_DATA_FLOAT_ARRAY_PROPERTY_DATA,
                                         &output_float_array_data);

    /* Sanity checks.. */
    collada_data_float_array_get_property(input_float_array,
                                          COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_COMPONENTS,
                                         &input_float_array_n_components);
    collada_data_float_array_get_property(input_float_array,
                                          COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_VALUES,
                                         &input_float_array_n_values);
    collada_data_float_array_get_property(output_float_array,
                                          COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_COMPONENTS,
                                         &output_float_array_n_components);
    collada_data_float_array_get_property(output_float_array,
                                          COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_VALUES,
                                         &output_float_array_n_values);

    interpolation_vector_n_values = system_resizable_vector_get_amount_of_elements(interpolation_vector);

    ASSERT_DEBUG_SYNC(input_float_array_n_values == output_float_array_n_values,
                      "Input float array defines [%d] values, whereas output float array defines [%d] values",
                      input_float_array_n_values,
                      output_float_array_n_values);
    ASSERT_DEBUG_SYNC(input_float_array_n_values == interpolation_vector_n_values,
                      "Input float array defines [%d] values, whereas interpolation vector defines [%d] values.",
                      input_float_array_n_values,
                      interpolation_vector_n_values);

    ASSERT_DEBUG_SYNC(input_float_array_n_components == 1,
                      "Input float array uses more than 1 component");
    ASSERT_DEBUG_SYNC(output_float_array_n_components == 1,
                      "Output float array uses more than 1 component");

    /* Iterate over all segments */
    system_variant end_value_variant   = system_variant_create(SYSTEM_VARIANT_FLOAT);
    system_variant start_value_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    for (uint32_t n_segment = 0;
                  n_segment < interpolation_vector_n_values - 1;
                ++n_segment)
    {
        float start_time  = input_float_array_data [n_segment];
        float start_value = output_float_array_data[n_segment];
        float end_time    = input_float_array_data [n_segment + 1];
        float end_value   = output_float_array_data[n_segment + 1];

        uint32_t start_time_msec = system_time_get_timeline_time_for_msec( (uint32_t) (start_time * 1000 /* ms in s */) );
        uint32_t end_time_msec   = system_time_get_timeline_time_for_msec( (uint32_t) (end_time   * 1000 /* ms in s */) );

        system_variant_set_float(start_value_variant, start_value);
        system_variant_set_float(end_value_variant,   end_value);

        /* Sanity checks */
        ASSERT_DEBUG_SYNC(start_time <= end_time,
                          "Start time happens later than the curve segment's end time");

        /* Add a new curve segment.
         *
         * TODO: We currently only support linear segments. Add support for the other types when necessary.
         **/
        collada_data_sampler_input_interpolation interpolation = COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_UNKNOWN;

        system_resizable_vector_get_element_at(interpolation_vector,
                                               n_segment,
                                              &interpolation);

        switch (interpolation)
        {
            case COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_LINEAR:
            {
                curve_container_add_lerp_segment(result,
                                                 start_time_msec,
                                                 end_time_msec,
                                                 start_value_variant,
                                                 end_value_variant,
                                                 NULL); /* curve_segment_id */

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported interpolation type encountered");
            }
        } /* switch (interpolation) */
    }

    if (start_value_variant != NULL)
    {
        system_variant_release(start_value_variant);

        start_value_variant = NULL;
    }

    if (end_value_variant != NULL)
    {
        system_variant_release(end_value_variant);

        end_value_variant = NULL;
    }

end:
    return result;
}

/** TODO */
PRIVATE void _collada_scene_generator_create_scene_graph(__in __notnull collada_data       data,
                                                         __in __notnull collada_data_scene collada_scene,
                                                         __in __notnull scene              result_scene,
                                                         __in __notnull ogl_context        context)
{
    scene_graph scene_graph = scene_graph_create();

    ASSERT_DEBUG_SYNC(scene_graph != NULL, "Could not create a scene graph instance");
    if (scene_graph == NULL)
    {
        goto end;
    }

    /* Iterate through collada data nodes and add them to a nodes_to_process vector.
     * We'll process all the nodes*/
    system_hashed_ansi_string     scene_name            = NULL;
    collada_data_scene_graph_node scene_root_node       = NULL;
    system_hash64map              scene_to_dag_node_map = system_hash64map_create(sizeof(system_dag_node) );

    collada_data_scene_get_property(collada_scene,
                                    COLLADA_DATA_SCENE_PROPERTY_NAME,
                                   &scene_name);
    collada_data_scene_get_property(collada_scene,
                                    COLLADA_DATA_SCENE_PROPERTY_ROOT_NODE,
                                   &scene_root_node);

    _collada_scene_generator_process_collada_data_node(data,
                                                       scene_root_node,
                                                       result_scene,
                                                       scene_graph_get_root_node(scene_graph),
                                                       scene_graph,
                                                       scene_to_dag_node_map,
                                                       context);

    /* Pass the graph over to the scene container */
    scene_set_graph(result_scene, scene_graph);

end:
    if (scene_to_dag_node_map != NULL)
    {
        system_hash64map_release(scene_to_dag_node_map);

        scene_to_dag_node_map = NULL;
    }
}


struct _texture_loader_workload
{
    collada_data              data;
    system_hashed_ansi_string file_name;
    system_hashed_ansi_string file_name_with_path;
    gfx_image                 image;
    unsigned int              n_workload;
    volatile unsigned int*    n_workloads_processed_ptr;
    unsigned int              n_workloads_to_process;
    ogl_texture               texture;
    bool                      texture_has_mipmaps;
    system_event              workloads_processed_event;

    _texture_loader_workload()
    {
        data                      = NULL;
        file_name                 = NULL;
        file_name_with_path       = NULL;
        image                     = NULL;
        n_workload                = 0;
        n_workloads_processed_ptr = NULL;
        n_workloads_to_process    = 0;
        texture                   = NULL;
        texture_has_mipmaps       = false;
        workloads_processed_event = NULL;
    }
};

/** TODO */
volatile void _collada_scene_generator_process_workload(__in __notnull system_thread_pool_callback_argument data)
{
    _texture_loader_workload* workload_ptr = (_texture_loader_workload*) data;

    /* Retrieve image properties */
    system_hashed_ansi_string collada_file_name_with_path     = NULL;
    const char*               collada_file_name_with_path_raw = NULL;
    collada_data_image        image                           = NULL;
    system_hashed_ansi_string image_file_name                 = NULL;
    system_hashed_ansi_string image_file_name_with_path       = NULL;
    system_hashed_ansi_string name                            = NULL;
    collada_data_image        result_image                    = NULL;

    collada_data_get_image           (workload_ptr->data,
                                      workload_ptr->n_workload,
                                     &image);
    collada_data_image_get_properties(image,
                                     &name,
                                     &image_file_name,
                                     &image_file_name_with_path,
                                      NULL); /* out_requires_mipmaps */

    /* Try to create a gfx_image instance of the image */
    if ( (workload_ptr->image = gfx_image_create_from_file(name,
                                                           image_file_name,
                                                           true)) == NULL) /* use_alternative_filename_getter */
    {
        if ( (workload_ptr->image = gfx_image_create_from_file(name,
                                                               image_file_name_with_path,
                                                               true)) == NULL) /* use_alternative_filename_getter */
        {
            /* Extract path from the COLLADA scene file and use it for the image file */
            const char* path_terminator = NULL;

            collada_data_get_property(workload_ptr->data,
                                      COLLADA_DATA_PROPERTY_FILE_NAME,
                                      &collada_file_name_with_path);

            collada_file_name_with_path_raw = system_hashed_ansi_string_get_buffer(collada_file_name_with_path);
            path_terminator                 = strrchr(collada_file_name_with_path_raw, '//');

            if (path_terminator != NULL)
            {
                system_hashed_ansi_string new_path                = system_hashed_ansi_string_create_substring             (collada_file_name_with_path_raw,
                                                                                                                            0, /* offset */
                                                                                                                            (path_terminator + 1) - collada_file_name_with_path_raw);
                system_hashed_ansi_string new_path_with_file_name = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(new_path),
                                                                                                                            system_hashed_ansi_string_get_buffer(image_file_name) );

                workload_ptr->image = gfx_image_create_from_file(name,
                                                                 new_path_with_file_name,
                                                                 true); /* use_alternative_filename_getter */
            } /* if (path_terminator != NULL) */
        } /* if (GFX image could not have been loaded from working directory) */
    } /* if (GFX image could not have been loaded from original location) */

    /* Check if we should signal the event */
    if (::InterlockedIncrement(workload_ptr->n_workloads_processed_ptr) == workload_ptr->n_workloads_to_process)
    {
        system_event_set(workload_ptr->workloads_processed_event);
    }
}

/** TODO */
PRIVATE void _collada_scene_generator_create_textures(__in __notnull collada_data       data,
                                                      __in __notnull collada_data_scene collada_scene,
                                                      __in __notnull scene              result_scene,
                                                      __in __notnull ogl_context        context)
{
    unsigned int n_images = 0;

    collada_data_get_property(data, COLLADA_DATA_PROPERTY_N_IMAGES, &n_images);

    /* Texture loading process can be distributed across available threads. However, we first need
     * to accumulate unique file names, so that each thread will be guaranteed to work on a different
     * asset.*/
    volatile unsigned int n_workloads_processed     = 0;
    system_hash64map      workloads_map             = system_hash64map_create(sizeof(_texture_loader_workload*), false);
    system_event          workloads_processed_event = system_event_create    (true /* manual_reset */, false /* start_state */);

    for (unsigned int n_image = 0; n_image < n_images; ++n_image)
    {
        collada_data_image        image                     = NULL;
        system_hashed_ansi_string image_file_name           = NULL;
        system_hashed_ansi_string image_file_name_with_path = NULL;

        collada_data_get_image           (data, n_image, &image);
        collada_data_image_get_properties(image,
                                          NULL  /* out_name */,
                                         &image_file_name,
                                         &image_file_name_with_path,
                                          NULL); /* out_requires_mipmaps */

        /* Is this filename already cached? */
        if (!system_hash64map_get(workloads_map,
                                  system_hashed_ansi_string_get_hash(image_file_name),
                                  NULL) )
        {
            _texture_loader_workload* new_workload = new (std::nothrow) _texture_loader_workload;

            ASSERT_ALWAYS_SYNC(new_workload != NULL, "Out of memory");
            if (new_workload != NULL)
            {
                new_workload->data                      = data;
                new_workload->file_name                 = image_file_name;
                new_workload->file_name_with_path       = image_file_name_with_path;
                new_workload->n_workload                = n_image;
                new_workload->n_workloads_processed_ptr = &n_workloads_processed;
                new_workload->workloads_processed_event = workloads_processed_event;

                if (!system_hash64map_insert(workloads_map,
                                             system_hashed_ansi_string_get_hash(image_file_name),
                                             new_workload,
                                             NULL,   /* on_remove_callback */
                                             NULL) ) /* on_remove_callback_user_arg */
                {
                    ASSERT_DEBUG_SYNC(false, "Could not insert new owrkload");
                }
            } /* if (new_workload != NULL) */
        } /* if (workload not defined) */
    } /* for (all COLLADA images) */

    /* Process all workloads */
    const unsigned int n_workloads = system_hash64map_get_amount_of_elements(workloads_map);

    for (unsigned int n_workload = 0;
                      n_workload < n_workloads;
                    ++n_workload)
    {
        _texture_loader_workload* current_workload = NULL;

        if (system_hash64map_get_element_at(workloads_map, n_workload, &current_workload, NULL /* pOutHash */) )
        {
            /* Before we set off, update the descriptor so that it tells how many workloads
             * we will be launching */
            current_workload->n_workloads_to_process = n_workloads;

            system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_IDLE,
                                                                                                             _collada_scene_generator_process_workload,
                                                                                                             current_workload);

            system_thread_pool_submit_single_task(task);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve workload descriptor");
        }
    } /* for (all workloads) */

    /* Wait for the processing to finish */
    system_event_wait_single_infinite(workloads_processed_event);

    /* Release stuff before we continue */
    system_event_release(workloads_processed_event);
    workloads_processed_event = NULL;

    /* Create textures off the images we've loaded */
    for (unsigned int n_image = 0; n_image < n_images; ++n_image)
    {
        system_hashed_ansi_string image_file_name    = NULL;
        _texture_loader_workload* image_workload_ptr = NULL;
        system_hashed_ansi_string name               = NULL;
        bool                      requires_mipmaps   = false;
        collada_data_image        result_image       = NULL;

        /* Retrieve workload corresponding to the image we're processing */
        collada_data_get_image           (data,
                                          n_image,
                                         &result_image);
        collada_data_image_get_properties(result_image,
                                         &name,
                                         &image_file_name,
                                          NULL, /* out_file_name_with_path */
                                         &requires_mipmaps);

        system_hash64map_get(workloads_map,
                             system_hashed_ansi_string_get_hash(image_file_name),
                             &image_workload_ptr);

        if (image_workload_ptr->image == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not load image [%s] from file [%s]",
                               system_hashed_ansi_string_get_buffer(name),
                               system_hashed_ansi_string_get_buffer(image_file_name) );
        } /* if (image_workload_ptr->image == NULL) */
        else
        {
            bool        has_created_texture = false;
            ogl_texture result_ogl_texture  = NULL;

            /* Has an ogl_texture object already been spawned for this image? */
            if (image_workload_ptr->texture == NULL)
            {
                /* Try to spawn ogl_texture object */
                result_ogl_texture = ogl_texture_create_from_gfx_image(context,
                                                                       image_workload_ptr->image,
                                                                       image_file_name);

                if (result_ogl_texture == NULL)
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Could not create ogl_texture [%s] from file [%s]",
                                       system_hashed_ansi_string_get_buffer(name),
                                       system_hashed_ansi_string_get_buffer(image_file_name) );
                } /* if (result_ogl_texture == NULL) */
                else
                {
                    /* Cache the ogl_texture instance */
                    has_created_texture         = true;
                    image_workload_ptr->texture = result_ogl_texture;
                }
            }
            else
            {
                result_ogl_texture = image_workload_ptr->texture;
            }

            /* Spawn scene_texture instance */
            scene_texture result_texture = scene_texture_create(name,
                                                                image_file_name);

            if (result_texture == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not create scene_texture [%s] from file [%s]",
                                   system_hashed_ansi_string_get_buffer(name),
                                   system_hashed_ansi_string_get_buffer(image_file_name) );
            } /* if (result_texture == NULL) */
            else
            {
                /* If mip-maps are required, generate them now */
                if (requires_mipmaps &&
                    !image_workload_ptr->texture_has_mipmaps)
                {
                   LOG_INFO("VRAM usage warning: texture [%s] requires mip-maps - generating..",
                            system_hashed_ansi_string_get_buffer(name) );

                   ogl_texture_generate_mipmaps(result_ogl_texture);

                   image_workload_ptr->texture_has_mipmaps = true;
                }

                scene_texture_set(result_texture,
                                  SCENE_TEXTURE_PROPERTY_OGL_TEXTURE,
                                 &result_ogl_texture);

                /* Bind the new scene texture object to the scene */
                if (!scene_add_texture(result_scene, result_texture) )
                {
                    ASSERT_ALWAYS_SYNC(false, "Could not attach texture to the scene");
                }

                /* We do not need to own the assets anymore */
                if (has_created_texture)
                {
                    ogl_texture_release(result_ogl_texture);
                }

                scene_texture_release(result_texture);
            }
        }
    } /* for (all images) */

    /* Good to release all workload descriptors at this point */
    system_hash64             current_workload_hash = 0;
    _texture_loader_workload* current_workload_ptr  = NULL;

    while (system_hash64map_get_element_at(workloads_map, 0, &current_workload_ptr, &current_workload_hash) )
    {
        gfx_image_release(current_workload_ptr->image);
        current_workload_ptr->image = NULL;

        delete current_workload_ptr;
        current_workload_ptr = NULL;

        system_hash64map_remove(workloads_map, current_workload_hash);
    } /* while (images are cached in hash map) */

    system_hash64map_release(workloads_map);
    workloads_map = NULL;
}

/** TODO */
PRIVATE void _collada_scene_generator_process_collada_data_node(__in __notnull collada_data                  data,
                                                                __in __notnull collada_data_scene_graph_node node,
                                                                __in __notnull scene                         scene,
                                                                __in __notnull scene_graph_node              parent_node,
                                                                __in __notnull scene_graph                   graph,
                                                                __in __notnull system_hash64map              scene_to_dag_node_map,
                                                                __in __notnull ogl_context                   context)
{
    _collada_data_node_type node_type    = COLLADA_DATA_NODE_TYPE_UNDEFINED;
    unsigned int            n_node_items = 0;

    collada_data_scene_graph_node_get_property(node,
                                               COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_TYPE,
                                              &node_type);
    collada_data_scene_graph_node_get_property(node,
                                               COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_N_NODE_ITEMS,
                                              &n_node_items);

    /* Create a corresponding scene graph node */
    scene_graph_node new_scene_graph_node = NULL;

    switch (node_type)
    {
        case COLLADA_DATA_NODE_TYPE_NODE:
        {
            /* This corresponds to a general node in scene graph speak */
            new_scene_graph_node = scene_graph_create_general_node(graph);

            ASSERT_DEBUG_SYNC(new_scene_graph_node != NULL,
                              "Could not create a general scene graph node");

            scene_graph_add_node(graph,
                                 parent_node,
                                 new_scene_graph_node);
            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unsupported COLLADA node type encountered (joint?)");

            goto end;
        }
    } /* switch (node_type) */

    /* Iterate through COLLADA data node's node items. */
    scene_graph_node current_node = new_scene_graph_node;

    for (unsigned int n_node_item = 0;
                      n_node_item < n_node_items;
                    ++n_node_item)
    {
        collada_data_scene_graph_node_item node_item      = NULL;
        void*                              node_item_data = NULL;
        system_hashed_ansi_string          node_item_sid  = NULL;
        _collada_data_node_item_type       node_item_type = COLLADA_DATA_NODE_ITEM_TYPE_UNDEFINED;

        collada_data_scene_graph_node_get_node_item(node,
                                                    n_node_item,
                                                   &node_item);

        collada_data_scene_graph_node_item_get_property(node_item,
                                                        COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_DATA_HANDLE,
                                                       &node_item_data);
        collada_data_scene_graph_node_item_get_property(node_item,
                                                        COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_SID,
                                                       &node_item_sid);
        collada_data_scene_graph_node_item_get_property(node_item,
                                                        COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_TYPE,
                                                       &node_item_type);

        switch (node_item_type)
        {
            case COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION:
            {
                /* NOTE: Subsequent transformations are dependent so current_node must be updated */
                current_node = _collada_scene_generator_process_transformation_node_item(graph,
                                                                                         current_node,
                                                                                         node_item_data,
                                                                                         node_item_sid);

                break;
            } /* case COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION: */

            case COLLADA_DATA_NODE_ITEM_TYPE_CAMERA_INSTANCE:
            {
                _collada_scene_generator_process_camera_instance_node_item(data,
                                                                           scene,
                                                                           graph,
                                                                           current_node,
                                                                           node_item_data);

                break;
            }

            case COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE:
            {
                _collada_scene_generator_process_geometry_instance_node_item(data,
                                                                             scene,
                                                                             graph,
                                                                             current_node,
                                                                             node_item_data,
                                                                             context);

                break;
            } /* case COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE: */

            case COLLADA_DATA_NODE_ITEM_TYPE_LIGHT_INSTANCE:
            {
                _collada_scene_generator_process_light_instance_node_item(data,
                                                                          scene,
                                                                          graph,
                                                                          current_node,
                                                                          node_item_data);

                break;
            }

            case COLLADA_DATA_NODE_ITEM_TYPE_NODE:
            {
                _collada_scene_generator_process_collada_data_node(data,
                                                                   (collada_data_scene_graph_node) node_item_data,
                                                                   scene,
                                                                   current_node,
                                                                   graph,
                                                                   scene_to_dag_node_map,
                                                                   context);

                break;
            } /* case COLLADA_DATA_NODE_ITEM_TYPE_NODE: */

            default:
            {
                ASSERT_ALWAYS_SYNC(false, "Unsupported COLLADA node item type encountered");
            }
        } /* switch (node_item_type) */
    } /* for (all sub-items) */

end:
    ;
}

/** TODO */
PRIVATE void _collada_scene_generator_process_camera_instance_node_item(__in __notnull collada_data     data,
                                                                        __in __notnull scene            scene,
                                                                        __in __notnull scene_graph      graph,
                                                                        __in __notnull scene_graph_node parent_node,
                                                                        __in __notnull void*            node_item_data)
{
    collada_data_camera       collada_camera = NULL;
    system_hashed_ansi_string camera_name    = NULL;
    system_hashed_ansi_string instance_name  = NULL;

    collada_data_scene_graph_node_camera_instance_get_property( (collada_data_scene_graph_node_camera_instance) node_item_data,
                                                                COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_CAMERA,
                                                               &collada_camera);
    collada_data_scene_graph_node_camera_instance_get_property( (collada_data_scene_graph_node_camera_instance) node_item_data,
                                                                COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_NAME,
                                                               &instance_name);

    ASSERT_DEBUG_SYNC(collada_camera != NULL, "Camera is NULL");
    ASSERT_DEBUG_SYNC(instance_name  != NULL, "Camera instance name is NULL");

    /* TODO: For now we assume the properties are static. */
    scene_camera new_camera_instance = scene_camera_create(instance_name);

    /* Set camera instance properties */
    float                     ar          = 0.0f;
    _scene_camera_type        camera_type = SCENE_CAMERA_TYPE_PERSPECTIVE;
    system_hashed_ansi_string name        = NULL;
    float                     xfov        = 0.0f;
    float                     yfov        = 0.0f;
    float                     zfar        = 0.0f;
    float                     znear       = 0.0f;

    scene_graph_attach_object_to_node(graph,
                                      parent_node,
                                      SCENE_OBJECT_TYPE_CAMERA,
                                      new_camera_instance);

    collada_data_camera_get_property(collada_camera,
                                     COLLADA_DATA_CAMERA_PROPERTY_NAME,
                                    &name);
    collada_data_camera_get_property(collada_camera,
                                     COLLADA_DATA_CAMERA_PROPERTY_AR,
                                    &ar);
    collada_data_camera_get_property(collada_camera,
                                     COLLADA_DATA_CAMERA_PROPERTY_XFOV,
                                    &xfov);
    collada_data_camera_get_property(collada_camera,
                                     COLLADA_DATA_CAMERA_PROPERTY_YFOV,
                                    &yfov);
    collada_data_camera_get_property(collada_camera,
                                     COLLADA_DATA_CAMERA_PROPERTY_ZFAR,
                                    &zfar);
    collada_data_camera_get_property(collada_camera,
                                     COLLADA_DATA_CAMERA_PROPERTY_ZNEAR,
                                    &znear);

    yfov = DEG_TO_RAD(yfov);

    scene_camera_set_property(new_camera_instance,
                              SCENE_CAMERA_PROPERTY_ASPECT_RATIO,
                             &ar);
    scene_camera_set_property(new_camera_instance,
                              SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                             &zfar);
    scene_camera_set_property(new_camera_instance,
                              SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                             &znear);
    scene_camera_set_property(new_camera_instance,
                              SCENE_CAMERA_PROPERTY_TYPE,
                              &camera_type);
    scene_camera_set_property(new_camera_instance,
                              SCENE_CAMERA_PROPERTY_VERTICAL_FOV,
                             &yfov);

    bool result = scene_add_camera(scene,
                                   new_camera_instance);
    ASSERT_ALWAYS_SYNC(result, "Could not add a mesh instance");
}

/** TODO */
PRIVATE void _collada_scene_generator_process_geometry_instance_node_item(__in __notnull collada_data     data,
                                                                          __in __notnull scene            scene,
                                                                          __in __notnull scene_graph      graph,
                                                                          __in __notnull scene_graph_node parent_node,
                                                                          __in __notnull void*            node_item_data,
                                                                          __in __notnull ogl_context      context)
{
    collada_data_geometry     geometry      = NULL;
    system_hashed_ansi_string instance_name = NULL;
    mesh                      mesh          = NULL;
    system_hashed_ansi_string mesh_name     = NULL;

    collada_data_scene_graph_node_geometry_instance_get_property( (collada_data_scene_graph_node_geometry_instance) node_item_data,
                                                                  COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_GEOMETRY,
                                                                 &geometry);
    collada_data_scene_graph_node_geometry_instance_get_property( (collada_data_scene_graph_node_geometry_instance) node_item_data,
                                                                  COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_NAME,
                                                                 &instance_name);

    collada_data_geometry_get_property(geometry,
                                       COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                       &mesh_name);

    mesh = collada_data_get_emerald_mesh_by_name(data, context, mesh_name);
    ASSERT_DEBUG_SYNC(mesh != NULL, "Mesh is NULL");

    bool result = scene_add_mesh_instance(scene, mesh, instance_name);
    ASSERT_ALWAYS_SYNC(result, "Could not add a mesh instance");

    if (result)
    {
        scene_mesh mesh_instance = scene_get_mesh_instance_by_name(scene, instance_name);

        ASSERT_ALWAYS_SYNC(mesh_instance != NULL, "Could not retrieve mesh instance");
        if (mesh_instance != NULL)
        {
            scene_graph_attach_object_to_node(graph,
                                              parent_node,
                                              SCENE_OBJECT_TYPE_MESH,
                                              mesh_instance);
        }
    }
}

/** TODO */
PRIVATE void _collada_scene_generator_process_light_instance_node_item(__in __notnull collada_data     data,
                                                                       __in __notnull scene            scene,
                                                                       __in __notnull scene_graph      graph,
                                                                       __in __notnull scene_graph_node parent_node,
                                                                       __in __notnull void*            node_item_data)
{
    collada_data_light        collada_light = NULL;
    const float*              light_color   = NULL;
    system_hashed_ansi_string light_name    = NULL;
    collada_data_light_type   light_type    = COLLADA_DATA_LIGHT_TYPE_UNKNOWN;
    scene_light               new_light     = NULL;
    system_hashed_ansi_string instance_name = NULL;

    collada_data_scene_graph_node_light_instance_get_property( (collada_data_scene_graph_node_light_instance) node_item_data,
                                                               COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_LIGHT,
                                                              &collada_light);
    collada_data_scene_graph_node_light_instance_get_property( (collada_data_scene_graph_node_light_instance) node_item_data,
                                                               COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_NAME,
                                                              &instance_name);

    ASSERT_DEBUG_SYNC(collada_light != NULL, "Light is NULL");
    ASSERT_DEBUG_SYNC(instance_name != NULL, "Light instance name is NULL");

    /* Convert collada_data_light to scene_light */
    collada_data_light_get_property(collada_light,
                                    COLLADA_DATA_LIGHT_PROPERTY_COLOR,
                                   &light_color);
    collada_data_light_get_property(collada_light,
                                    COLLADA_DATA_LIGHT_PROPERTY_TYPE,
                                   &light_type);

    switch (light_type)
    {
        case COLLADA_DATA_LIGHT_TYPE_DIRECTIONAL:
        {
            new_light = scene_light_create_directional(instance_name);

            scene_light_set_property(new_light,
                                     SCENE_LIGHT_PROPERTY_DIFFUSE,
                                     light_color);

            /* There are no other fields we need to worry about, as far as
             * directional light goes. */
            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unsupported light type");
        }
    } /* switch (light_type) */

    if (new_light != NULL)
    {
        bool result = scene_add_light(scene, new_light);
        ASSERT_ALWAYS_SYNC(result, "Could not add a light instance");

        if (result)
        {
            scene_graph_attach_object_to_node(graph,
                                              parent_node,
                                              SCENE_OBJECT_TYPE_LIGHT,
                                              new_light);
        }
    }
}

/** TODO */
PRIVATE scene_graph_node _collada_scene_generator_process_transformation_node_item(__in __notnull scene_graph               graph,
                                                                                   __in __notnull scene_graph_node          parent_node,
                                                                                   __in __notnull void*                     node_item_data,
                                                                                   __in __notnull system_hashed_ansi_string node_item_sid)
{
    scene_graph_node                  result              = NULL;
    _collada_data_transformation_type transformation_type = COLLADA_DATA_TRANSFORMATION_TYPE_UNDEFINED;

    collada_data_transformation_get_property( (collada_data_transformation) node_item_data,
                                              COLLADA_DATA_TRANSFORMATION_PROPERTY_TYPE,
                                             &transformation_type);

    switch (transformation_type)
    {
        case COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX:
        {
            system_matrix4x4 transformation_matrix = system_matrix4x4_create();
            {
                collada_value transformation_matrix_collada_value[16] = {NULL};
                float         transformation_matrix_data[16]          = {0};

                /* TODO: If necessary, implement support for animated matrix components */
                collada_data_transformation_get_matrix_properties( (collada_data_transformation) node_item_data,
                                                                  transformation_matrix_collada_value);

                for (uint32_t n = 0;
                              n < 4 * 4;
                            ++n)
                {
                    collada_value_get_property(transformation_matrix_collada_value[n],
                                               COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
                                               transformation_matrix_data + n);
                }

                system_matrix4x4_set_from_row_major_raw(transformation_matrix,
                                                        transformation_matrix_data);

                result = scene_graph_create_static_matrix4x4_transformation_node(graph,
                                                                                 transformation_matrix,
                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

                ASSERT_DEBUG_SYNC(result != NULL, "Could not create a static matrix 4x4 transformation node");

                scene_graph_add_node(graph,
                                     parent_node,
                                     result);
            }
            system_matrix4x4_release(transformation_matrix);

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE:
        {
            float                rotation_angle                 = 0.0f;
            collada_value        rotation_angle_collada_value   = NULL;
            float                rotation_axis[3]               = {0};
            collada_value        rotation_axis_collada_value[3] = {NULL};
            scene_graph_node_tag result_node_tag                = SCENE_GRAPH_NODE_TAG_UNDEFINED;

            /* Convert SID to a node tag.
             *
             * NOTE: LW uses <rotate> nodes for pivot orientation, so we do not want sanity
             *       checks in here.
             */
            if (system_hashed_ansi_string_is_equal_to_raw_string(node_item_sid, "rotateX")) result_node_tag = SCENE_GRAPH_NODE_TAG_ROTATE_X;else
            if (system_hashed_ansi_string_is_equal_to_raw_string(node_item_sid, "rotateY")) result_node_tag = SCENE_GRAPH_NODE_TAG_ROTATE_Y;else
            if (system_hashed_ansi_string_is_equal_to_raw_string(node_item_sid, "rotateZ")) result_node_tag = SCENE_GRAPH_NODE_TAG_ROTATE_Z;

            /* Carry on */
            collada_data_transformation_get_rotate_properties( (collada_data_transformation) node_item_data,
                                                               rotation_axis_collada_value,
                                                              &rotation_angle_collada_value);

            collada_value collada_values[] =
            {
                rotation_angle_collada_value,
                rotation_axis_collada_value[0],
                rotation_axis_collada_value[1],
                rotation_axis_collada_value[2]
            };

            /* If any of the components is defined by a collada_data_animation, we need to use
             * a dynamic rotation matrix4x4 node instead of the faster static one.
             */
            bool is_dynamic = false;

            for (uint32_t n_value = 0;
                          n_value < sizeof(collada_values) / sizeof(collada_values[0]);
                        ++n_value)
            {
                collada_value_type type = COLLADA_VALUE_TYPE_UNKNOWN;

                collada_value_get_property(collada_values[n_value],
                                           COLLADA_VALUE_PROPERTY_TYPE,
                                          &type);

                if (type == COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION)
                {
                    is_dynamic = true;

                    break;
                }
            } /* for (all components) */

            if (is_dynamic)
            {
                curve_container rotation_vector_curves[4] = {NULL};

                for (uint32_t n_component = 0;
                              n_component < sizeof(collada_values) / sizeof(collada_values[0]);
                            ++n_component)
                {
                    rotation_vector_curves[n_component] = _collada_scene_generator_create_curve_container_from_collada_value(collada_values[n_component]);
                } /* for (all components) */

                result = scene_graph_create_rotation_dynamic_node(graph,
                                                                  rotation_vector_curves,
                                                                  result_node_tag);

                ASSERT_DEBUG_SYNC(result != NULL,
                                  "Could not create a scene graph rotation dynamic node");

                scene_graph_add_node(graph,
                                     parent_node,
                                     result);

                /* The former call will retain the curve container array, so we're safe
                 * to release it here */
                for (uint32_t n_component = 0;
                              n_component < sizeof(collada_values) / sizeof(collada_values[0]);
                            ++n_component)
                {
                    curve_container_release(rotation_vector_curves[n_component]);
                } /* for (all components) */
            }
            else
            {
                system_matrix4x4 rotation_matrix = system_matrix4x4_create();

                for (uint32_t n_component = 0;
                              n_component < 3;
                            ++n_component)
                {
                    collada_value_get_property(rotation_axis_collada_value[n_component],
                                               COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
                                               rotation_axis + n_component);
                }
                collada_value_get_property(rotation_angle_collada_value,
                                           COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
                                          &rotation_angle);

                system_matrix4x4_set_to_identity(rotation_matrix);
                system_matrix4x4_rotate         (rotation_matrix, DEG_TO_RAD(rotation_angle), rotation_axis);

                result = scene_graph_create_static_matrix4x4_transformation_node(graph,
                                                                                 rotation_matrix,
                                                                                 result_node_tag);

                ASSERT_DEBUG_SYNC(result != NULL,
                                  "Could not create a scene graph 4x4 matrix transformation node");

                scene_graph_add_node(graph,
                                     parent_node,
                                     result);

                system_matrix4x4_release(rotation_matrix);
            }

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_SCALE:
        {
            scene_graph_node_tag result_node_tag               = SCENE_GRAPH_NODE_TAG_UNDEFINED;
            system_matrix4x4     scale_matrix                  = system_matrix4x4_create();
            collada_value        scale_vector_collada_value[3] = {NULL};
            float                scale_vector[3]               = {0};

            /* Convert SID to a node tag */
            if (system_hashed_ansi_string_is_equal_to_raw_string(node_item_sid, "scale")) result_node_tag = SCENE_GRAPH_NODE_TAG_SCALE;else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized COLLADA scale node SID");
            }

            {
                /* TODO: If necessary, introduce support for collada_data_animation */
                collada_data_transformation_get_scale_properties( (collada_data_transformation) node_item_data,
                                                                  scale_vector_collada_value);

                for (uint32_t n_component = 0;
                              n_component < 3;
                            ++n_component)
                {
                    collada_value_get_property(scale_vector_collada_value[n_component],
                                               COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
                                               scale_vector + n_component);
                } /* for (all components) */

                system_matrix4x4_set_to_identity(scale_matrix);
                system_matrix4x4_scale          (scale_matrix, scale_vector);

                result = scene_graph_create_static_matrix4x4_transformation_node(graph,
                                                                                 scale_matrix,
                                                                                 result_node_tag);

                ASSERT_DEBUG_SYNC(result != NULL,
                                  "Could not create a scene graph 4x4 static matrix transformation node");

                scene_graph_add_node(graph,
                                     parent_node,
                                     result);
            }
            system_matrix4x4_release(scale_matrix);

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE:
        {
            scene_graph_node_tag result_node_tag                     = SCENE_GRAPH_NODE_TAG_UNDEFINED;
            system_matrix4x4     translation_matrix                  = system_matrix4x4_create();
            collada_value        translation_vector_collada_value[3] = {NULL};

            /* Convert SID to a node tag.
             *
             * NOTE: LW uses a number of <translate> nodes to compensate for pivot rotation.
             *       We definitely do not want a sanity check here.
             **/
            if (system_hashed_ansi_string_is_equal_to_raw_string(node_item_sid, "translate"))
            {
                result_node_tag = SCENE_GRAPH_NODE_TAG_TRANSLATE;
            }

            {
                collada_data_transformation_get_translate_properties( (collada_data_transformation) node_item_data,
                                                                      translation_vector_collada_value);

                /* If any of the components is defined by a collada_data_animation, we need to use
                 * a dynamic translation matrix4x4 node instead of the faster static one.
                 */
                bool is_dynamic = false;

                for (uint32_t n_component = 0;
                              n_component < 3;
                            ++n_component)
                {
                    collada_value_type type = COLLADA_VALUE_TYPE_UNKNOWN;

                    collada_value_get_property(translation_vector_collada_value[n_component],
                                               COLLADA_VALUE_PROPERTY_TYPE,
                                              &type);

                    if (type == COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION)
                    {
                        is_dynamic = true;

                        break;
                    }
                } /* for (all components) */

                if (is_dynamic)
                {
                    curve_container translation_vector_curves[3] = {NULL};

                    for (uint32_t n_component = 0;
                                  n_component < 3;
                                ++n_component)
                    {
                        translation_vector_curves[n_component] = _collada_scene_generator_create_curve_container_from_collada_value(translation_vector_collada_value[n_component]);
                    } /* for (all components) */

                    result = scene_graph_create_translation_dynamic_node(graph,
                                                                         translation_vector_curves,
                                                                         result_node_tag);

                    ASSERT_DEBUG_SYNC(result != NULL,
                                      "Could not create a scene graph translation dynamic node");

                    scene_graph_add_node(graph,
                                         parent_node,
                                         result);

                    /* The former call will retain the curve container array, so we're safe
                     * to release it here */
                    for (uint32_t n_component = 0;
                                  n_component < 3;
                                ++n_component)
                    {
                        curve_container_release(translation_vector_curves[n_component]);
                    } /* for (all components) */
                }
                else
                {
                    float translation_vector[3] = {0};

                    for (uint32_t n_component = 0;
                                  n_component < 3;
                                ++n_component)
                    {
                        collada_value_get_property(translation_vector_collada_value[n_component],
                                                   COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
                                                   translation_vector + n_component);
                    }

                    system_matrix4x4_set_to_identity(translation_matrix);
                    system_matrix4x4_translate      (translation_matrix, translation_vector);

                    result = scene_graph_create_static_matrix4x4_transformation_node(graph,
                                                                                     translation_matrix,
                                                                                     result_node_tag);

                    ASSERT_DEBUG_SYNC(result != NULL,
                                      "Could not create a scene graph static matrix4x4 transformation node");

                    scene_graph_add_node(graph,
                                         parent_node,
                                         result);
                }
            }
            system_matrix4x4_release(translation_matrix);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unsupported COLLADA transformation type");
        }
    } /* switch (transformation_type) */

    return result;
}

/** Please see header for specification */
PUBLIC scene collada_scene_generator_create(__in __notnull collada_data data,
                                            __in __notnull ogl_context  context,
                                            __in __notnull unsigned int n_scene)
{
    collada_data_scene collada_scene = NULL;
    scene              result        = NULL;

    collada_data_get_scene(data,
                           n_scene,
                          &collada_scene);

    if (collada_scene == NULL)
    {
        LOG_FATAL("No scene found at index [%d]", n_scene);

        goto end;
    }

    /* Retrieve basic COLLADA scene properties first */
    collada_data_scene_graph_node collada_root_node  = NULL;
    system_hashed_ansi_string     scene_name         = NULL;

    collada_data_scene_get_property(collada_scene,
                                    COLLADA_DATA_SCENE_PROPERTY_NAME,
                                   &scene_name);
    collada_data_scene_get_property(collada_scene,
                                    COLLADA_DATA_SCENE_PROPERTY_ROOT_NODE,
                                   &collada_root_node);

    /* Create the scene instance. */
    result = scene_create(context, scene_name);

    if (result == NULL)
    {
        LOG_FATAL("Could not create a scene instance");

        goto end;
    }

    /* Create scene texture instances */
    _collada_scene_generator_create_textures(data,
                                             collada_scene,
                                             result,
                                             context);

    /* Before we can add any instances, we need to convert the COLLADA scene
     * graph into a DAG. While we trave the graph, we will also add
     * camera/light/geometry instances we encounter.
     **/
    _collada_scene_generator_create_scene_graph(data,
                                                collada_scene,
                                                result,
                                                context);

    /* Take animation time stored in COLLADA container and plug it into the result scene */
    float animation_time = 0.0f;

    collada_data_get_property(data,
                              COLLADA_DATA_PROPERTY_MAX_ANIMATION_DURATION,
                             &animation_time);
    scene_set_property       (result,
                              SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                             &animation_time);

end:
    return result;
}

