
/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_animation.h"
#include "collada/collada_data_camera.h"
#include "collada/collada_data_channel.h"
#include "collada/collada_data_float_array.h"
#include "collada/collada_data_image.h"
#include "collada/collada_data_light.h"
#include "collada/collada_data_material.h"
#include "collada/collada_data_polylist.h"
#include "collada/collada_data_sampler.h"
#include "collada/collada_data_sampler2D.h"
#include "collada/collada_data_scene.h"
#include "collada/collada_data_scene_graph_node_material_instance.h"
#include "collada/collada_data_source.h"
#include "collada/collada_data_surface.h"
#include "collada/collada_data_transformation.h"
#include "collada/collada_mesh_generator.h"
#include "collada/collada_scene_generator.h"
#include "collada/collada_value.h"
#include "mesh/mesh.h"
#include "scene/scene.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_event.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "tinyxml2.h"

/* Forward declarations */
struct _collada_data_node;

/* Private declarations */

/** Container private type definition */
typedef struct _collada_data
{
    system_resizable_vector animations;
    system_resizable_vector cameras;
    system_resizable_vector effects;
    system_resizable_vector geometries;
    system_resizable_vector images;
    bool                    is_initialized_successfully;
    system_resizable_vector lights;
    system_resizable_vector materials;
    system_resizable_vector scenes;
    system_file_serializer  serializer;

    system_hash64map cameras_by_id_map;
    system_hash64map effects_by_id_map;
    system_hash64map geometries_by_id_map;
    system_hash64map images_by_id_map;
    system_hash64map lights_by_id_map;
    system_hash64map materials_by_id_map;
    system_hash64map nodes_by_id_map;
    system_hash64map scenes_by_id_map;

    /* Maps uint64_t (representing ptr to a collada_data_* object) to a system_resizable_vector,
     * holding collada_data_animation instances.
     */
    system_hash64map object_to_animation_vector_map;

    system_hashed_ansi_string file_name;
    float                     max_animation_duration; /* in seconds */
    _collada_data_up_axis     up_axis;
    bool                      use_cache_binary_blobs_mode;

    /* Constructor */
    _collada_data()
    {
        animations                     = system_resizable_vector_create(4 /* capacity */, sizeof(collada_data_animation) );
        cameras                        = system_resizable_vector_create(4 /* capacity */, sizeof(collada_data_camera) );
        cameras_by_id_map              = system_hash64map_create       (sizeof(collada_data_camera) );
        effects                        = system_resizable_vector_create(4 /* capacity */, sizeof(collada_data_effect) );
        effects_by_id_map              = system_hash64map_create       (sizeof(collada_data_effect) );
        geometries                     = system_resizable_vector_create(4 /* capacity */, sizeof(collada_data_geometry), true);
        geometries_by_id_map           = system_hash64map_create       (sizeof(collada_data_geometry), true);
        images                         = system_resizable_vector_create(4 /* capacity */, sizeof(collada_data_image) );
        images_by_id_map               = system_hash64map_create       (sizeof(collada_data_image) );
        lights                         = system_resizable_vector_create(1 /* capacity */, sizeof(collada_data_light) );
        lights_by_id_map               = system_hash64map_create       (sizeof(collada_data_light) );
        materials                      = system_resizable_vector_create(4 /* capacity */, sizeof(collada_data_material) );
        materials_by_id_map            = system_hash64map_create       (sizeof(collada_data_material) );
        max_animation_duration         = 0.0f;
        nodes_by_id_map                = system_hash64map_create       (sizeof(_collada_data_node*) );
        object_to_animation_vector_map = system_hash64map_create       (sizeof(system_resizable_vector) );
        scenes                         = system_resizable_vector_create(1 /* capacity */, sizeof(collada_data_scene) );
        scenes_by_id_map               = system_hash64map_create       (sizeof(collada_data_scene) );
        serializer                     = NULL;
        up_axis                        = COLLADA_DATA_UP_AXIS_UNDEFINED;
        use_cache_binary_blobs_mode    = false;
        is_initialized_successfully    = false;
    }

    /* Destructor */
    ~_collada_data();

    REFCOUNT_INSERT_VARIABLES
} _collada_data;

/* Destructors for private types */
_collada_data::~_collada_data()
{
    if (animations != NULL)
    {
        collada_data_animation animation = NULL;

        while (system_resizable_vector_pop(animations, &animation) )
        {
            collada_data_animation_release(animation);

            animation = NULL;
        }

        system_resizable_vector_release(animations);
        animations = NULL;
    }

    if (object_to_animation_vector_map != NULL)
    {
        system_hash64           temp_hash   = 0;
        system_resizable_vector temp_vector = NULL;

        while (system_hash64map_get_element_at(object_to_animation_vector_map,
                                               0, /* index */
                                               &temp_vector,
                                               &temp_hash) )
        {
            system_resizable_vector_release(temp_vector);
            temp_vector = NULL;

            system_hash64map_remove(object_to_animation_vector_map,
                                    temp_hash);
        }

        system_hash64map_release(object_to_animation_vector_map);
        object_to_animation_vector_map = NULL;
    }

    if (cameras != NULL)
    {
        collada_data_camera camera = NULL;

        while (system_resizable_vector_pop(cameras, &camera) )
        {
            collada_data_camera_release(camera);

            camera = NULL;
        }

        system_resizable_vector_release(cameras);
        cameras = NULL;
    }

    if (cameras_by_id_map != NULL)
    {
        system_hash64map_release(cameras_by_id_map);
        cameras_by_id_map = NULL;
    }

    if (effects != NULL)
    {
        collada_data_effect effect = NULL;

        while (system_resizable_vector_pop(effects, &effect) )
        {
            collada_data_effect_release(effect);

            effect = NULL;
        }

        system_resizable_vector_release(effects);
        effects = NULL;
    }

    if (effects_by_id_map != NULL)
    {
        system_hash64map_release(effects_by_id_map);
        effects_by_id_map = NULL;
    }

    if (geometries != NULL)
    {
        collada_data_geometry geometry = NULL;

        while (system_resizable_vector_pop(geometries, &geometry) )
        {
            collada_data_geometry_release(geometry);

            geometry = NULL;
        }

        system_resizable_vector_release(geometries);
        geometries = NULL;
    }

    if (geometries_by_id_map != NULL)
    {
        system_hash64map_release(geometries_by_id_map);
        geometries_by_id_map = NULL;
    }

    if (images != NULL)
    {
        collada_data_image image = NULL;

        while (system_resizable_vector_pop(images, &image) )
        {
            collada_data_image_release(image);

            image = NULL;
        }

        system_resizable_vector_release(images);
        images = NULL;
    }

    if (lights != NULL)
    {
        collada_data_light light = NULL;

        while (system_resizable_vector_pop(lights, &light) )
        {
            collada_data_light_release(light);

            light = NULL;
        }

        system_resizable_vector_release(lights);
        lights = NULL;
    }

    if (lights_by_id_map != NULL)
    {
        system_hash64map_release(lights_by_id_map);
        lights_by_id_map = NULL;
    }

    if (materials != NULL)
    {
        collada_data_material material = NULL;

        while (system_resizable_vector_pop(materials, &material) )
        {
            collada_data_material_release(material);

            material = NULL;
        }

        system_resizable_vector_release(materials);
        materials = NULL;
    }

    if (materials_by_id_map != NULL)
    {
        system_hash64map_release(materials_by_id_map);

        materials_by_id_map = NULL;
    }

    if (nodes_by_id_map != NULL)
    {
        system_hash64map_release(nodes_by_id_map);
        nodes_by_id_map = NULL;
    }

    if (scenes != NULL)
    {
        collada_data_scene scene = NULL;

        while (system_resizable_vector_pop(scenes, &scene) )
        {
            collada_data_scene_release(scene);

            scene = NULL;
        }

        system_resizable_vector_release(scenes);
        scenes = NULL;
    }

    if (scenes_by_id_map != NULL)
    {
        system_hash64map_release(scenes_by_id_map);
        scenes_by_id_map = NULL;
    }

    if (serializer != NULL)
    {
        system_file_serializer_release(serializer);

        serializer = NULL;
    }
}


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(collada_data, collada_data, _collada_data);

PRIVATE void                                 _collada_data_apply_animation_data_to_transformation(__in __notnull                collada_data_transformation           transformation,
                                                                                                  __in __notnull                collada_data_animation                animation,
                                                                                                  __in __notnull                collada_data_channel                  channel,
                                                                                                  __in                          collada_data_channel_target_component animation_target_component);
PRIVATE void                                 _collada_data_init                                  (__in __notnull                _collada_data*                        collada_ptr,
                                                                                                  __in                          system_hashed_ansi_string             filename,
                                                                                                  __in                          bool                                  should_generate_cache_blobs);
PRIVATE void                                 _collada_data_init_animations                       (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_animations,
                                                                                                  __in __notnull                system_hash64map                      result_object_to_animation_vector_map,
                                                                                                  __in __notnull                collada_data                          data);
PRIVATE void                                 _collada_data_init_cameras                          (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_cameras,
                                                                                                  __in __notnull                system_hash64map                      result_cameras_by_id_map);
PRIVATE void                                 _collada_data_init_effects                          (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_effects,
                                                                                                  __in __notnull                system_hash64map                      result_effects_by_id_map,
                                                                                                  __in __notnull                system_hash64map                      images_by_id_map);
PRIVATE void                                 _collada_data_init_general                          (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                _collada_data*                        collada_data_ptr);
PRIVATE void                                 _collada_data_init_geometries                       (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_geometries,
                                                                                                  __in __notnull                system_hash64map                      result_geometries_by_id_map,
                                                                                                  __in __notnull                collada_data                          collada_data);
PRIVATE void                                 _collada_data_init_images                           (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_images,
                                                                                                  __in __notnull                system_hash64map                      result_images_by_id_map);
PRIVATE void                                 _collada_data_init_lights                           (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_lights,
                                                                                                  __in __notnull                system_hash64map                      result_lights_by_id_map);
PRIVATE void                                 _collada_data_init_materials                        (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               materials,
                                                                                                  __in __notnull                system_hash64map                      materials_by_id_map,
                                                                                                  __in __notnull                system_hash64map                      effects_by_id_map);
PRIVATE void                                 _collada_data_init_scenes                           (__in __notnull                tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  __in __notnull                system_resizable_vector               result_scenes,
                                                                                                  __in __notnull                system_hash64map                      result_scenes_by_id_map,
                                                                                                  __in __notnull                system_hash64map                      result_nodes_by_id_map,
                                                                                                  __in __notnull                _collada_data*                        loader_ptr);
PRIVATE void                                 _collada_data_release                               (__in __notnull __post_invalid void*                                 arg);


/** TODO */
PRIVATE void _collada_data_apply_animation_data(__in __notnull _collada_data* data_ptr)
{
    const uint32_t n_animations = system_resizable_vector_get_amount_of_elements(data_ptr->animations);

    /* Iterate over all collada_data_animation instances */
    for (uint32_t n_animation = 0;
                  n_animation < n_animations;
                ++n_animation)
    {
        collada_data_animation current_animation = NULL;

        if (system_resizable_vector_get_element_at(data_ptr->animations,
                                                   n_animation,
                                                  &current_animation) )
        {
            /* Retrieve channel & sampler instances */
            collada_data_channel animation_channel = NULL;
            collada_data_sampler animation_sampler = NULL;

            collada_data_animation_get_property(current_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL,
                                               &animation_channel);
            collada_data_animation_get_property(current_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER,
                                               &animation_sampler);

            /* Retrieve target details */
            void*                                 animation_target           = NULL;
            collada_data_channel_target_component animation_target_component = COLLADA_DATA_CHANNEL_TARGET_COMPONENT_UNKNOWN;
            collada_data_channel_target_type      animation_target_type      = COLLADA_DATA_CHANNEL_TARGET_TYPE_UNKNOWN;

            collada_data_channel_get_property(animation_channel,
                                              COLLADA_DATA_CHANNEL_PROPERTY_TARGET,
                                             &animation_target);
            collada_data_channel_get_property(animation_channel,
                                              COLLADA_DATA_CHANNEL_PROPERTY_TARGET_COMPONENT,
                                             &animation_target_component);
            collada_data_channel_get_property(animation_channel,
                                              COLLADA_DATA_CHANNEL_PROPERTY_TARGET_TYPE,
                                             &animation_target_type);

            /* Update the target instance with animation data */
            switch (animation_target_type)
            {
                case COLLADA_DATA_CHANNEL_TARGET_TYPE_CAMERA_INSTANCE:
                case COLLADA_DATA_CHANNEL_TARGET_TYPE_GEOMETRY_INSTANCE:
                case COLLADA_DATA_CHANNEL_TARGET_TYPE_LIGHT_INSTANCE:
                {
                    /* TODO */
                    ASSERT_DEBUG_SYNC(false, "TODO");

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_TYPE_TRANSFORMATION:
                {
                    collada_data_transformation transformation = (collada_data_transformation) animation_target;

                    _collada_data_apply_animation_data_to_transformation(transformation,
                                                                         current_animation,
                                                                         animation_channel,
                                                                         animation_target_component);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized animation target type");
                }
            } /* switch (animation_target_type) */
        } /* if (retrieved current animation) */
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not get a collada_data_animation instance at index [%d]",
                              n_animation);
        }
    } /* for (all animations) */
}

/** TODO */
PRIVATE void _collada_data_apply_animation_data_to_transformation(__in __notnull collada_data_transformation           transformation,
                                                                  __in __notnull collada_data_animation                animation,
                                                                  __in __notnull collada_data_channel                  channel,
                                                                  __in           collada_data_channel_target_component animation_target_component)
{
    _collada_data_transformation_type transformation_type = COLLADA_DATA_TRANSFORMATION_TYPE_UNDEFINED;

    collada_data_transformation_get_property(transformation,
                                             COLLADA_DATA_TRANSFORMATION_PROPERTY_TYPE,
                                            &transformation_type);

    switch (transformation_type)
    {
        case COLLADA_DATA_TRANSFORMATION_TYPE_LOOKAT:
        case COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX:
        case COLLADA_DATA_TRANSFORMATION_TYPE_SKEW:
        {
            /* TODO */
            ASSERT_DEBUG_SYNC(false, "TODO");

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE:
        {
            collada_value axis_vector[3] = {NULL};
            collada_value angle          =  NULL;

            collada_data_transformation_get_rotate_properties(transformation,
                                                              axis_vector,
                                                             &angle);

            switch (animation_target_component)
            {
                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_ANGLE:
                {
                    collada_value_set(angle,
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_X:
                {
                    collada_value_set(axis_vector[0],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Y:
                {
                    collada_value_set(axis_vector[1],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Z:
                {
                    collada_value_set(axis_vector[2],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized rotation target component");
                }
            } /* switch (channel) */

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_SCALE:
        {
            collada_value scale_vector[3] = {NULL};

            collada_data_transformation_get_scale_properties(transformation,
                                                             scale_vector);

            switch (animation_target_component)
            {
                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_X:
                {
                    collada_value_set(scale_vector[0],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Y:
                {
                    collada_value_set(scale_vector[1],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Z:
                {
                    collada_value_set(scale_vector[2],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized scale target component");
                }
            } /* switch (channel) */

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE:
        {
            collada_value translate_vector[3] = {NULL};

            collada_data_transformation_get_translate_properties(transformation,
                                                                 translate_vector);

            switch (animation_target_component)
            {
                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_X:
                {
                    collada_value_set(translate_vector[0],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Y:
                {
                    collada_value_set(translate_vector[1],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                case COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Z:
                {
                    collada_value_set(translate_vector[2],
                                      COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                                     &animation);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized translate target component");
                }
            } /* switch (channel) */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA transformation type");
        }
    } /* switch (transformation_type) */
}

/** TODO */
PRIVATE system_hashed_ansi_string _collada_data_get_emerald_mesh_blob_file_name(__in __notnull system_hashed_ansi_string geometry_name)
{
    const char* strings[] =
    {
        "collada_mesh_gl_blob_",
        system_hashed_ansi_string_get_buffer(geometry_name)
    };
    const unsigned int n_strings = sizeof(strings) / sizeof(strings[0]);

    return system_hashed_ansi_string_create_by_merging_strings(n_strings, strings);
}

/** TODO */
PRIVATE void _collada_data_init(__in __notnull _collada_data*            collada_ptr,
                                __in           system_hashed_ansi_string filename,
                                __in           bool                      should_generate_cache_blobs)
{
    collada_ptr->file_name                   = filename;
    collada_ptr->use_cache_binary_blobs_mode = should_generate_cache_blobs;

    /* Read in and parse COLLADA XML file */
    system_file_serializer serializer      =               system_file_serializer_create_for_reading(filename);
    const char*            serialized_data = (const char*) system_file_serializer_get_raw_storage   (serializer);

    if (serialized_data != NULL)
    {
        /* Use XML parser to convert the raw text representation into something
         * more meaningful */
        tinyxml2::XMLDocument document;
        int                   loading_result = document.LoadText(serialized_data);

        if (loading_result == tinyxml2::XML_NO_ERROR)
        {
            /* Parse general properties */
            _collada_data_init_general(&document,
                                        collada_ptr);

            /* Parse image data */
            _collada_data_init_images(&document,
                                       collada_ptr->images,
                                       collada_ptr->images_by_id_map);

            /* Parse effect data */
            _collada_data_init_effects(&document,
                                       collada_ptr->effects,
                                       collada_ptr->effects_by_id_map,
                                       collada_ptr->images_by_id_map);

            /* Parse material data */
            _collada_data_init_materials(&document,
                                         collada_ptr->materials,
                                         collada_ptr->materials_by_id_map,
                                         collada_ptr->effects_by_id_map);

            /* Parse camera data */
            _collada_data_init_cameras(&document,
                                        collada_ptr->cameras,
                                        collada_ptr->cameras_by_id_map);

            /* Parse geometry data */
            _collada_data_init_geometries(&document,
                                          collada_ptr->geometries,
                                          collada_ptr->geometries_by_id_map,
                                          (collada_data) collada_ptr);

            /* Parse light data */
            _collada_data_init_lights(&document,
                                       collada_ptr->lights,
                                       collada_ptr->lights_by_id_map);

            /* Parse scene data */
            _collada_data_init_scenes(&document,
                                       collada_ptr->scenes,
                                       collada_ptr->scenes_by_id_map,
                                       collada_ptr->nodes_by_id_map,
                                       collada_ptr);

            /* Parse animation data */
            _collada_data_init_animations(&document,
                                           collada_ptr->animations,
                                           collada_ptr->object_to_animation_vector_map,
                                           (collada_data) collada_ptr);

            /* Now that we have the animation data, we need to update all objects, so that
             * their constant components can be replaced with curve representation.
             */
            _collada_data_apply_animation_data(collada_ptr);

            /* Done */
            collada_ptr->is_initialized_successfully = true;
        }
        else
        {
            LOG_FATAL("Could not parse COLLADA file");
        }
    } /* if (serialized_data != NULL) */

    system_file_serializer_release(serializer);
}

/** TODO */
PRIVATE void _collada_data_init_animations(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                           __in __notnull system_resizable_vector result_animations,
                                           __in __notnull system_hash64map        result_object_to_animation_vector_map,
                                           __in __notnull collada_data            data)
{
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        goto end;
    }

    tinyxml2::XMLElement* library_animations_element_ptr = collada_element_ptr->FirstChildElement("library_animations");

    if (library_animations_element_ptr == NULL)
    {
        goto end;
    }

    /* Iterate through all defined animations. We do a couple of different things in
     * this loop:
     *
     * 1. We create a collada_data_animation instance for each <animation> node we encounter
     *    in the COLLADA file.
     * 2. The instance is then stored in one of the vectors stored in collada_data ..
     * 3. ..as well as in a hash map that connects scene graph nodes to a vector storing animations
     *    that affect some of the properties of that node.
     * 4. Finally, we determine the maximum duration of all animations. Sadly, this information
     *    is not stored in the COLLADA file, so we need to compute it by ourselves.
     *
     **/
    tinyxml2::XMLElement* current_animation_element_ptr = library_animations_element_ptr->FirstChildElement("animation");
    _collada_data*        data_ptr                      = (_collada_data*) data;

    while (current_animation_element_ptr != NULL)
    {
        collada_data_animation new_animation = collada_data_animation_create(current_animation_element_ptr,
                                                                             data);

        ASSERT_ALWAYS_SYNC(new_animation != NULL, "Could not create COLLADA animation descriptor")
        if (new_animation != NULL)
        {
            /* Store the new animation in a vector*/
            system_hashed_ansi_string animation_id = NULL;
            system_hash64             entry_hash   = 0;

            collada_data_animation_get_property(new_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_ID,
                                               &animation_id);

            entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(animation_id),
                                                 system_hashed_ansi_string_get_length(animation_id) );

            system_resizable_vector_push(result_animations,
                                         new_animation);

            /* Now add the animation to a helper map */
            collada_data_channel    new_animation_channel = NULL;
            void*                   new_animation_target  = NULL;
            system_resizable_vector target_vector         = NULL;

            collada_data_animation_get_property(new_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL,
                                               &new_animation_channel);
            collada_data_channel_get_property  (new_animation_channel,
                                                COLLADA_DATA_CHANNEL_PROPERTY_TARGET,
                                               &new_animation_target);

            ASSERT_DEBUG_SYNC(new_animation_target != NULL,
                              "Animation target is NULL");

            if (!system_hash64map_get(result_object_to_animation_vector_map,
                                      (system_hash64) new_animation_target,
                                      &target_vector) )
            {
                /* Spawn a new vector and store it under the target's address */
                target_vector = system_resizable_vector_create(4, /* capacity */
                                                               sizeof(collada_data_animation) );

                system_hash64map_insert(result_object_to_animation_vector_map,
                                        (system_hash64) new_animation_target,
                                        target_vector,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            }

            system_resizable_vector_push(target_vector,
                                         new_animation);

            /* Finally, take the very last node of the input (of input type) - this tells the length of
             * the curve */
            const float*             new_animation_input_data          = NULL;
            uint32_t                 new_animation_input_data_n_values = 0;
            collada_data_float_array new_animation_input_float_array   = NULL;
            collada_data_source      new_animation_input_source        = NULL;
            collada_data_sampler     new_animation_sampler             = NULL;

            collada_data_animation_get_property      (new_animation,
                                                      COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER,
                                                     &new_animation_sampler);
            collada_data_sampler_get_input_property  (new_animation_sampler,
                                                      COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INPUT,
                                                      COLLADA_DATA_SAMPLER_INPUT_PROPERTY_SOURCE,
                                                     &new_animation_input_source);
            collada_data_source_get_source_float_data(new_animation_input_source,
                                                     &new_animation_input_float_array);
            collada_data_float_array_get_property    (new_animation_input_float_array,
                                                      COLLADA_DATA_FLOAT_ARRAY_PROPERTY_DATA,
                                                     &new_animation_input_data);
            collada_data_float_array_get_property    (new_animation_input_float_array,
                                                      COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_VALUES,
                                                     &new_animation_input_data_n_values);

            if (new_animation_input_data[new_animation_input_data_n_values - 1] > data_ptr->max_animation_duration)
            {
                data_ptr->max_animation_duration = new_animation_input_data[new_animation_input_data_n_values - 1];
            }
        } /* if (new_animation_ptr != NULL) */

        /* Move on */
        current_animation_element_ptr = current_animation_element_ptr->NextSiblingElement("animation");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_cameras(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                        __in __notnull system_resizable_vector result_cameras,
                                        __in __notnull system_hash64map        result_cameras_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        goto end;
    }

    tinyxml2::XMLElement* library_cameras_element_ptr = collada_element_ptr->FirstChildElement("library_cameras");

    if (library_cameras_element_ptr == NULL)
    {
        goto end;
    }

    /* Iterate through all defined cameras */
    tinyxml2::XMLElement* current_camera_element_ptr = library_cameras_element_ptr->FirstChildElement("camera");

    while (current_camera_element_ptr != NULL)
    {
        collada_data_camera new_camera = collada_data_camera_create(current_camera_element_ptr);

        ASSERT_ALWAYS_SYNC(new_camera != NULL, "Could not create COLLADA camera descriptor")
        if (new_camera != NULL)
        {
            /* Store the new camera */
            system_hashed_ansi_string camera_id = NULL;

            collada_data_camera_get_property(new_camera,
                                             COLLADA_DATA_CAMERA_PROPERTY_ID,
                                            &camera_id);

            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(camera_id),
                                                               system_hashed_ansi_string_get_length(camera_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_cameras_by_id_map, entry_hash), "Item already added to a hash-map");

            system_resizable_vector_push(result_cameras, new_camera);
            system_hash64map_insert     (result_cameras_by_id_map,
                                         entry_hash,
                                         new_camera,
                                         NULL,  /* no remove call-back needed */
                                         NULL); /* no remove call-back needed */
        } /* if (new_camera_ptr != NULL) */

        /* Move on */
        current_camera_element_ptr = current_camera_element_ptr->NextSiblingElement("camera");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_effects(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                        __in __notnull system_resizable_vector result_effects,
                                        __in __notnull system_hash64map        result_effects_by_id_map,
                                        __in __notnull system_hash64map        images_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        goto end;
    }

    tinyxml2::XMLElement* library_effects_element_ptr = collada_element_ptr->FirstChildElement("library_effects");

    if (library_effects_element_ptr == NULL)
    {
        goto end;
    }

    /* Iterate through all defined effects */
    tinyxml2::XMLElement* current_effect_element_ptr = library_effects_element_ptr->FirstChildElement("effect");

    while (current_effect_element_ptr != NULL)
    {
        collada_data_effect new_effect = collada_data_effect_create(current_effect_element_ptr,
                                                                    images_by_id_map);

        ASSERT_DEBUG_SYNC(new_effect != NULL, "Could not create COLLADA effect descriptor")
        if (new_effect != NULL)
        {
            system_hashed_ansi_string effect_id = NULL;

            collada_data_effect_get_property(new_effect,
                                             COLLADA_DATA_EFFECT_PROPERTY_ID,
                                             &effect_id);

            /* Store the new effect */
            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(effect_id),
                                                               system_hashed_ansi_string_get_length(effect_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_effects_by_id_map, entry_hash), "Item already added to a hash-map");

            system_resizable_vector_push(result_effects, new_effect);
            system_hash64map_insert     (result_effects_by_id_map,
                                         entry_hash,
                                         new_effect,
                                         NULL,  /* no remove call-back needed */
                                         NULL); /* no remove call-back needed */
        } /* if (new_effect_ptr != NULL) */

        /* Move on */
        current_effect_element_ptr = current_effect_element_ptr->NextSiblingElement("effect");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_general(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                        __in __notnull _collada_data*          collada_data_ptr)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate asset subnode */
    tinyxml2::XMLElement* asset_element_ptr = collada_element_ptr->FirstChildElement("asset");

    if (asset_element_ptr == NULL)
    {
        LOG_ERROR("Could not find <asset> sub-node");

        ASSERT_DEBUG_SYNC(false, "Could not find required <asset> sub-node");
        goto end;
    }

    /* Locate up_axis subnode. Note that it is optional, so assume Y_UP orientation as per spec if
     * the element is not available.
     */
    collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_Y_UP;

    tinyxml2::XMLElement* up_axis_element_ptr = asset_element_ptr->FirstChildElement("up_axis");

    if (up_axis_element_ptr == NULL)
    {
        /* That's fine */
        goto end;
    }

    /* Parse the contents */
    system_hashed_ansi_string up_axis_has = system_hashed_ansi_string_create(up_axis_element_ptr->GetText() );

    if (system_hashed_ansi_string_is_equal_to_raw_string(up_axis_has, "X_UP") )
    {
        collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_X_UP;

        ASSERT_ALWAYS_SYNC(false, "Up axis of 'X up' type is not supported");
        goto end;
    }
    else
    if (system_hashed_ansi_string_is_equal_to_raw_string(up_axis_has, "Y_UP") )
    {
        collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_Y_UP;

        goto end;
    }
    else
    if (system_hashed_ansi_string_is_equal_to_raw_string(up_axis_has, "Z_UP") )
    {
        collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_Z_UP;

        LOG_ERROR("Current implementation of 'Z up' up axis support is likely not to work correctly with animations!");
        goto end;
    }
    else
    {
        LOG_FATAL("Unrecognized up_axis node contents. Transformations will be messed up!");

        ASSERT_DEBUG_SYNC(false, "Unrecognized up_axis node contents");
        goto end;
    }

end:
    return ;
}

/** TODO */
PRIVATE void _collada_data_init_geometries(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                           __in __notnull system_resizable_vector result_geometries,
                                           __in __notnull system_hash64map        result_geometries_by_id_map,
                                           __in __notnull collada_data            collada_data)
{
        /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate library_geometries node */
    tinyxml2::XMLElement* geometries_element_ptr = collada_element_ptr->FirstChildElement("library_geometries");

    if (geometries_element_ptr == NULL)
    {
        LOG_ERROR("Could not find library_geometries node");

        goto end;
    }

    /* Large scenes contain lots of geometry that takes time to load. Distribute the load to separate threads
     * to speed the process up.
     */
    tinyxml2::XMLElement* current_geometry_element_ptr  = geometries_element_ptr->FirstChildElement("geometry");
    system_event          geometry_processed_event      = system_event_create(true /* manual_reset */, false /* start_state */);
    unsigned int          n_geometry_elements           = 0;
    volatile unsigned int n_geometry_elements_processed = 0; /* this counter will be accessed from multiple threads at the same time - only use interlocked accesses */

    ASSERT_ALWAYS_SYNC(geometry_processed_event != NULL, "Could not generate an event");
    if (geometry_processed_event == NULL)
    {
        goto end;
    }

    while (current_geometry_element_ptr != NULL)
    {
        n_geometry_elements++;

        /* Move to next geometry instance */
        current_geometry_element_ptr = current_geometry_element_ptr->NextSiblingElement("geometry");
    }

    /* Iterate through geometry instances (which describe meshes that are of our interest) */
    current_geometry_element_ptr = geometries_element_ptr->FirstChildElement("geometry");

    while (current_geometry_element_ptr != NULL)
    {
        volatile collada_data_geometry new_geometry = collada_data_geometry_create_async(current_geometry_element_ptr,
                                                                                         collada_data,
                                                                                         geometry_processed_event,
                                                                                         &n_geometry_elements_processed,
                                                                                         &n_geometry_elements,
                                                                                         result_geometries,
                                                                                         result_geometries_by_id_map);

        /* Move to next geometry instance */
        current_geometry_element_ptr = current_geometry_element_ptr->NextSiblingElement("geometry");
    }

    /* Wait for the processing to finish */
    system_event_wait_single_infinite(geometry_processed_event);
    system_event_release             (geometry_processed_event);

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_images(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                       __in __notnull system_resizable_vector result_images,
                                       __in __notnull system_hash64map        result_images_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        goto end;
    }

    tinyxml2::XMLElement* library_images_element_ptr = collada_element_ptr->FirstChildElement("library_images");

    if (library_images_element_ptr == NULL)
    {
        goto end;
    }

    /* Iterate through all defined images */
    tinyxml2::XMLElement* current_image_element_ptr = library_images_element_ptr->FirstChildElement("image");

    while (current_image_element_ptr != NULL)
    {
        collada_data_image        new_image    = collada_data_image_create(current_image_element_ptr);
        system_hashed_ansi_string new_image_id = NULL;

        if (new_image != NULL)
        {
            collada_data_image_get_property(new_image,
                                            COLLADA_DATA_IMAGE_PROPERTY_ID,
                                            &new_image_id);

            /* Store the new image */
            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(new_image_id),
                                                               system_hashed_ansi_string_get_length(new_image_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_images_by_id_map, entry_hash), "Item already added to a hash-map");

            system_resizable_vector_push(result_images, new_image);
            system_hash64map_insert     (result_images_by_id_map,
                                         entry_hash,
                                         new_image,
                                         NULL,  /* no remove call-back needed */
                                         NULL); /* no remove call-back needed */
        } /* if (new_camera_ptr != NULL) */

        /* Move on */
        current_image_element_ptr = current_image_element_ptr->NextSiblingElement("image");
    }

end: ;
}

/* TODO */
PRIVATE void _collada_data_init_lights(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                       __in __notnull system_resizable_vector result_lights,
                                       __in __notnull system_hash64map        result_lights_by_id_map)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate library_lights node */
    tinyxml2::XMLElement* lights_element_ptr = collada_element_ptr->FirstChildElement("library_lights");

    if (lights_element_ptr == NULL)
    {
        LOG_ERROR("Could not find library_lights node");

        goto end;
    }

    /* Iterate through all lights */
    tinyxml2::XMLElement* current_light_element_ptr = lights_element_ptr->FirstChildElement("light");

    while (current_light_element_ptr != NULL)
    {
        collada_data_light new_light = collada_data_light_create(current_light_element_ptr);

        ASSERT_DEBUG_SYNC(new_light != NULL, "Out of memory");
        if (new_light == NULL)
        {
            goto end;
        }

        /* Store the light descriptor */
        system_hashed_ansi_string light_id = NULL;

        collada_data_light_get_property(new_light,
                                        COLLADA_DATA_LIGHT_PROPERTY_ID,
                                       &light_id);

        system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(light_id),
                                                           system_hashed_ansi_string_get_length(light_id) );

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_lights_by_id_map, entry_hash), "Item already added to a hash-map");

        system_resizable_vector_push(result_lights, new_light);
        system_hash64map_insert     (result_lights_by_id_map,
                                     entry_hash,
                                     new_light,
                                     NULL,  /* no remove call-back needed */
                                     NULL); /* no remove call-back needed */

        /* Move on */
        current_light_element_ptr = current_light_element_ptr->NextSiblingElement("light");
    } /* while (current_light_element_ptr != NULL) */

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_materials(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                          __in __notnull system_resizable_vector materials,
                                          __in __notnull system_hash64map        materials_by_id_map,
                                          __in __notnull system_hash64map        effects_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        goto end;
    }

    tinyxml2::XMLElement* library_materials_element_ptr = collada_element_ptr->FirstChildElement("library_materials");

    if (library_materials_element_ptr == NULL)
    {
        goto end;
    }

    /* Iterate through all defined images */
    tinyxml2::XMLElement* current_material_element_ptr = library_materials_element_ptr->FirstChildElement("material");

    while (current_material_element_ptr != NULL)
    {
        system_hashed_ansi_string new_material_id = NULL;
        collada_data_material     new_material    = collada_data_material_create(current_material_element_ptr,
                                                                                 effects_by_id_map,
                                                                                 materials_by_id_map);

        ASSERT_ALWAYS_SYNC(new_material != NULL, "Could not create COLLADA material descriptor")
        if (new_material != NULL)
        {
            collada_data_material_get_property(new_material,
                                               COLLADA_DATA_MATERIAL_PROPERTY_ID,
                                               &new_material_id);

            /* Store the new material */
            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(new_material_id),
                                                               system_hashed_ansi_string_get_length(new_material_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(materials_by_id_map, entry_hash), "Item already added to a hash-map");

            system_resizable_vector_push(materials, new_material);
            system_hash64map_insert     (materials_by_id_map,
                                         entry_hash,
                                         new_material,
                                         NULL,  /* no remove call-back needed */
                                         NULL); /* no remove call-back needed */
        } /* if (new_material_ptr != NULL) */

        /* Move on */
        current_material_element_ptr = current_material_element_ptr->NextSiblingElement("material");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_scenes(__in __notnull tinyxml2::XMLDocument*  xml_document_ptr,
                                       __in __notnull system_resizable_vector result_scenes,
                                       __in __notnull system_hash64map        result_scenes_by_id_map,
                                       __in __notnull system_hash64map        result_nodes_by_id_map,
                                       __in __notnull _collada_data*          collada_data_ptr)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr = xml_document_ptr->FirstChildElement("COLLADA");

    if (collada_element_ptr == NULL)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate library_visual_scenes node */
    tinyxml2::XMLElement* visual_scenes_element_ptr = collada_element_ptr->FirstChildElement("library_visual_scenes");

    if (visual_scenes_element_ptr == NULL)
    {
        LOG_ERROR("Could not find library_visual_scenes node");

        goto end;
    }

    /* Iterate through all defined scenes */
    tinyxml2::XMLElement* current_scene_element_ptr = visual_scenes_element_ptr->FirstChildElement("visual_scene");

    while (current_scene_element_ptr != NULL)
    {
        /* Allocate a new descriptor */
        collada_data_scene new_scene = collada_data_scene_create(current_scene_element_ptr,
                                                                 (collada_data) collada_data_ptr);

        /* Store new scene */
        system_resizable_vector_push(result_scenes, new_scene);
        /* Move on */
        current_scene_element_ptr = current_scene_element_ptr->NextSiblingElement("visual_scene");
    } /* while (current_scene_element_ptr != NULL) */
end: ;
}

/** TODO */
PRIVATE void _collada_data_release(__in __notnull __post_invalid void* arg)
{
    _collada_data* mesh_collada = (_collada_data*) arg;
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_get_property(__in  __notnull collada_data          data,
                                                  __in            collada_data_property property,
                                                  __out __notnull void*                 out_result_ptr)
{
    _collada_data* collada_data_ptr = (_collada_data*) data;

    switch (property)
    {
        case COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE:
        {
            *( (bool*) out_result_ptr) = collada_data_ptr->use_cache_binary_blobs_mode;

            break;
        }

        case COLLADA_DATA_PROPERTY_CAMERAS_BY_ID_MAP:
        {
            *((system_hash64map*) out_result_ptr) = collada_data_ptr->cameras_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_FILE_NAME:
        {
            *( (system_hashed_ansi_string*) out_result_ptr) = collada_data_ptr->file_name;

            break;
        }

        case COLLADA_DATA_PROPERTY_GEOMETRIES_BY_ID_MAP:
        {
            *((system_hash64map*) out_result_ptr) = collada_data_ptr->geometries_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_LIGHTS_BY_ID_MAP:
        {
            *((system_hash64map*) out_result_ptr) = collada_data_ptr->lights_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_MATERIALS_BY_ID_MAP:
        {
            *((system_hash64map*) out_result_ptr) = collada_data_ptr->materials_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_MAX_ANIMATION_DURATION:
        {
            *(float*) out_result_ptr = collada_data_ptr->max_animation_duration;

            break;
        }

        case COLLADA_DATA_PROPERTY_NODES_BY_ID_MAP:
        {
            *((system_hash64map*) out_result_ptr) = collada_data_ptr->nodes_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_N_CAMERAS:
        {
            *( (unsigned int*) out_result_ptr) = system_resizable_vector_get_amount_of_elements(collada_data_ptr->cameras);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_EFFECTS:
        {
            *( (unsigned int*) out_result_ptr) = system_resizable_vector_get_amount_of_elements(collada_data_ptr->effects);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_IMAGES:
        {
            *( (unsigned int*) out_result_ptr) = system_resizable_vector_get_amount_of_elements(collada_data_ptr->images);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_MATERIALS:
        {
            *( (unsigned int*) out_result_ptr) = system_resizable_vector_get_amount_of_elements(collada_data_ptr->materials);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_MESHES:
        {
            *( (unsigned int*) out_result_ptr) = system_resizable_vector_get_amount_of_elements(collada_data_ptr->geometries);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_SCENES:
        {
            *( (unsigned int*) out_result_ptr) = system_resizable_vector_get_amount_of_elements(collada_data_ptr->scenes);

            break;
        }

        case COLLADA_DATA_PROPERTY_OBJECT_TO_ANIMATION_VECTOR_MAP:
        {
            *( (system_hash64map*) out_result_ptr) = collada_data_ptr->object_to_animation_vector_map;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA data property");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API collada_data_camera collada_data_get_camera_by_name(__in __notnull collada_data              data,
                                                                       __in           system_hashed_ansi_string name)
{
    _collada_data*      data_ptr  = (_collada_data*) data;
    const unsigned int  n_cameras = system_resizable_vector_get_amount_of_elements(data_ptr->cameras);
    collada_data_camera result    = NULL;

    LOG_INFO("collada_data_get_camera_by_name(): Slow code-path called");

    for (unsigned int n_camera = 0; n_camera < n_cameras; ++n_camera)
    {
        collada_data_camera       camera    = NULL;
        system_hashed_ansi_string camera_id = NULL;

        if (system_resizable_vector_get_element_at(data_ptr->cameras, n_camera, &camera) )
        {
            collada_data_camera_get_property(camera,
                                             COLLADA_DATA_CAMERA_PROPERTY_ID,
                                            &camera_id);

            if (system_hashed_ansi_string_is_equal_to_hash_string(camera_id, name) )
            {
                result = camera;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve camera at index [%d]", n_camera);
        }
    }

    return result;
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_get_effect(__in __notnull  collada_data         data,
                                                __in            unsigned int         n_effect,
                                                __out __notnull collada_data_effect* out_effect)
{
    _collada_data* data_ptr = (_collada_data*) data;

    system_resizable_vector_get_element_at(data_ptr->effects, n_effect, out_effect);
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh(collada_data data,
                                                      ogl_context  context,
                                                      unsigned int n_mesh)
{
    _collada_data*            collada_data_ptr            = (_collada_data*) data;
    bool                      has_loaded_blob             = false;
    system_hashed_ansi_string running_in_cache_blobs_mode = false;
    mesh                      result                      = NULL;

    collada_data_get_property(data,
                              COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
                              &running_in_cache_blobs_mode);

    if (system_resizable_vector_get_amount_of_elements(collada_data_ptr->geometries) > n_mesh)
    {
        collada_data_geometry geometry = NULL;

        system_resizable_vector_get_element_at(collada_data_ptr->geometries, n_mesh, &geometry);

        if (geometry != NULL)
        {
            collada_data_geometry_get_property(geometry,
                                               COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH,
                                               &result);

            if (result == NULL)
            {
                system_hashed_ansi_string blob_file_name = NULL;
                system_hashed_ansi_string geometry_id    = NULL;

                collada_data_geometry_get_property(geometry,
                                                   COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                                   &geometry_id);

                /* Must be first-time request for the mesh. Convert COLLADA representation to an
                 * Emerald instance of the mesh */
                ASSERT_DEBUG_SYNC(geometry_id != NULL                                                 &&
                                  geometry_id != system_hashed_ansi_string_get_default_empty_string(),
                                  "Geometry name is NULL");

                blob_file_name = _collada_data_get_emerald_mesh_blob_file_name(geometry_id);
                result         = collada_mesh_generator_create                (context, data, n_mesh);

                collada_data_geometry_set_property(geometry,
                                                   COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH,
                                                   &result);

                ASSERT_ALWAYS_SYNC(result != NULL,
                                   "Could not convert COLLADA geometry data to Emerald mesh");

                /* Cache blob data IF we're running in cache blobs mode. More info below */
                if (running_in_cache_blobs_mode)
                {
                    /* Is the blob available already? */
                    system_file_serializer serializer      = system_file_serializer_create_for_reading(blob_file_name, false);
                    const void*            serializer_data = system_file_serializer_get_raw_storage   (serializer);

                    if (serializer_data != NULL)
                    {
                        const void*      blob_data         = NULL;
                        _mesh_index_type index_type        = MESH_INDEX_TYPE_UNKNOWN;
                        uint32_t         n_blob_data_bytes = 0;

                        system_file_serializer_read(serializer, sizeof(index_type),        &index_type);
                        system_file_serializer_read(serializer, sizeof(n_blob_data_bytes), &n_blob_data_bytes);
                        blob_data = (const char*) system_file_serializer_get_raw_storage(serializer) + system_file_serializer_get_current_offset(serializer);

                        mesh_set_single_indexed_representation(result,
                                                               index_type,
                                                               n_blob_data_bytes,
                                                               blob_data);

                        system_file_serializer_read(serializer,
                                                    n_blob_data_bytes,
                                                    NULL);

                        /* Also read unique stream data offsets */
                        unsigned int n_serialized_max_stream_types = 0;

                        system_file_serializer_read(serializer,
                                                    sizeof(n_serialized_max_stream_types),
                                                   &n_serialized_max_stream_types);

                        ASSERT_DEBUG_SYNC(n_serialized_max_stream_types == MESH_LAYER_DATA_STREAM_TYPE_COUNT,
                                          "COLLADA mesh data uses an unsupported number of data stream types");

                        for (mesh_layer_data_stream_type stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                                         stream_type < n_serialized_max_stream_types;
                                                 ++(int&)stream_type)
                        {
                            unsigned int serialized_stream_offset = 0;

                            system_file_serializer_read(serializer,
                                                        sizeof(serialized_stream_offset),
                                                       &serialized_stream_offset);

                            mesh_set_processed_data_stream_start_offset(result,
                                                                        stream_type,
                                                                        serialized_stream_offset);
                        }

                        /* ..and stride + total number of elements */
                        uint32_t n_total_elements = 0;
                        uint32_t stride           = 0;

                        system_file_serializer_read(serializer,
                                                    sizeof(n_total_elements),
                                                   &n_total_elements);
                        system_file_serializer_read(serializer,
                                                    sizeof(stride),
                                                   &stride);

                        mesh_set_property(result,
                                          MESH_PROPERTY_GL_TOTAL_ELEMENTS,
                                         &n_total_elements);
                        mesh_set_property(result,
                                          MESH_PROPERTY_GL_STRIDE,
                                         &stride);

                        /* Finally, read GL BO index offset data */
                        unsigned int n_read_layer_passes   = 0;
                        unsigned int n_read_layers         = 0;
                        unsigned int n_result_layer_passes = 0;
                        unsigned int n_result_layers       = 0;

                        mesh_get_property(result,
                                          MESH_PROPERTY_N_LAYERS,
                                         &n_result_layers);

                        system_file_serializer_read(serializer,
                                                     sizeof(n_read_layers),
                                                    &n_read_layers);
                        ASSERT_DEBUG_SYNC(n_result_layers == n_read_layers,
                                          "Number of mesh layers mismatch");

                        for (unsigned int n_layer = 0;
                                          n_layer < n_read_layers;
                                        ++n_layer)
                        {
                            float aabb_max_data[3] = {0};
                            float aabb_min_data[3] = {0};

                            n_result_layer_passes = mesh_get_amount_of_layer_passes(result, n_layer);

                            system_file_serializer_read(serializer,
                                                         sizeof(n_read_layer_passes),
                                                        &n_read_layer_passes);
                            system_file_serializer_read(serializer,
                                                        sizeof(float) * 3,
                                                        aabb_max_data);
                            system_file_serializer_read(serializer,
                                                        sizeof(float) * 3,
                                                        aabb_min_data);

                            mesh_set_layer_property(result,
                                                    n_layer,
                                                    0, /* n_pass - OK to use zero, this state does not work on pass level */
                                                    MESH_LAYER_PROPERTY_AABB_MAX,
                                                    aabb_max_data);
                            mesh_set_layer_property(result,
                                                    n_layer,
                                                    0, /* n_pass - OK to use zero, this state does not work on pass level */
                                                    MESH_LAYER_PROPERTY_AABB_MIN,
                                                    aabb_min_data);

                            ASSERT_DEBUG_SYNC(n_result_layer_passes == n_read_layer_passes,
                                              "Number of mesh layer passes mismatch");

                            for (unsigned int n_layer_pass = 0;
                                              n_layer_pass < n_read_layer_passes;
                                            ++n_layer_pass)
                            {
                                uint32_t index_offset    = 0;
                                uint32_t index_max_value = 0;
                                uint32_t index_min_value = 0;

                                system_file_serializer_read(serializer,
                                                             sizeof(index_offset),
                                                            &index_offset);
                                system_file_serializer_read(serializer,
                                                             sizeof(index_max_value),
                                                            &index_max_value);
                                system_file_serializer_read(serializer,
                                                             sizeof(index_min_value),
                                                            &index_min_value);

                                mesh_set_layer_property(result,
                                                        n_layer,
                                                        n_layer_pass,
                                                        MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET,
                                                       &index_offset);
                                mesh_set_layer_property(result,
                                                        n_layer,
                                                        n_layer_pass,
                                                        MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX,
                                                       &index_max_value);
                                mesh_set_layer_property(result,
                                                        n_layer,
                                                        n_layer_pass,
                                                        MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX,
                                                       &index_min_value);
                            } /* for (all layer passes) */
                        } /* for (all layers) */

                        /* All done */
                        has_loaded_blob = true;
                    }
                    else
                    {
                        /* The file was not found */
                    }

                    system_file_serializer_release(serializer);
                }

                if (system_hashed_ansi_string_contains(geometry_id, system_hashed_ansi_string_create("blok6__7__lib") ))
                {
                    int a = 1; a++;
                }

                if (!has_loaded_blob)
                {
                    /* Need to generate indexed data */
                    mesh_create_single_indexed_representation(result);
                }

                /* If 'blob cache' mode was activated for the COLLADA data container, cache the
                 * mesh data. Indexed representation takes time to load and for development purposes
                 * it is useful to avoid regeneration of that data.
                 * For authoring purposes, it is expected the mesh data will simply be serialized
                 * and later re-loaded.
                 */
                if (running_in_cache_blobs_mode && !has_loaded_blob)
                {
                    void*                  gl_data                = 0;
                    uint32_t               gl_data_size           = 0;
                    uint32_t               gl_data_total_elements = 0;
                    uint32_t               gl_data_stride         = 0;
                    _mesh_index_type       gl_index_type          = MESH_INDEX_TYPE_UNKNOWN;
                    system_file_serializer serializer             = system_file_serializer_create_for_writing(blob_file_name);

                    mesh_get_property(result, MESH_PROPERTY_GL_INDEX_TYPE,          &gl_index_type);
                    mesh_get_property(result, MESH_PROPERTY_GL_PROCESSED_DATA,      &gl_data);
                    mesh_get_property(result, MESH_PROPERTY_GL_PROCESSED_DATA_SIZE, &gl_data_size);
                    mesh_get_property(result, MESH_PROPERTY_GL_STRIDE,              &gl_data_stride);
                    mesh_get_property(result, MESH_PROPERTY_GL_TOTAL_ELEMENTS,      &gl_data_total_elements);

                    system_file_serializer_write(serializer, sizeof(gl_index_type), &gl_index_type);
                    system_file_serializer_write(serializer, sizeof(gl_data_size),  &gl_data_size);
                    system_file_serializer_write(serializer, gl_data_size,           gl_data);

                    /* Also store unique stream offsets */
                    const unsigned int n_max_stream_types = MESH_LAYER_DATA_STREAM_TYPE_COUNT;

                    system_file_serializer_write(serializer, sizeof(n_max_stream_types), &n_max_stream_types);

                    for (mesh_layer_data_stream_type stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                                     stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                             ++(int&)stream_type)
                    {
                        unsigned int stream_offset = 0;

                        mesh_get_layer_data_stream_property(result,
                                                            stream_type,
                                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                                           &stream_offset);

                        system_file_serializer_write(serializer, sizeof(stream_offset), &stream_offset);
                    }

                    /* and stride + total number of elements */
                    system_file_serializer_write(serializer,
                                                 sizeof(gl_data_total_elements),
                                                &gl_data_total_elements);
                    system_file_serializer_write(serializer,
                                                 sizeof(gl_data_stride),
                                                &gl_data_stride);

                    /* Don't forget about layer pass data*/
                    unsigned int n_layer_passes = 0;
                    unsigned int n_layers       = 0;

                    mesh_get_property(result,
                                      MESH_PROPERTY_N_LAYERS,
                                     &n_layers);

                    system_file_serializer_write(serializer,
                                                 sizeof(n_layers),
                                                &n_layers);

                    for (unsigned int n_layer = 0;
                                      n_layer < n_layers;
                                    ++n_layer)
                    {
                        const float* aabb_max_data = NULL;
                        const float* aabb_min_data = NULL;

                        n_layer_passes = mesh_get_amount_of_layer_passes(result, n_layer);

                        mesh_get_layer_pass_property(result,
                                                     n_layer,
                                                     0, /* n_pass - zero is fine, this state does not depend on layer passes */
                                                     MESH_LAYER_PROPERTY_AABB_MAX,
                                                    &aabb_max_data);
                        mesh_get_layer_pass_property(result,
                                                     n_layer,
                                                     0, /* n_pass - zero is fine, this state does not depend on layer passes */
                                                     MESH_LAYER_PROPERTY_AABB_MIN,
                                                    &aabb_min_data);

                        system_file_serializer_write(serializer,
                                                     sizeof(n_layer_passes),
                                                    &n_layer_passes);
                        system_file_serializer_write(serializer,
                                                     sizeof(float) * 3,
                                                     aabb_max_data);
                        system_file_serializer_write(serializer,
                                                     sizeof(float) * 3,
                                                     aabb_min_data);

                        for (unsigned int n_layer_pass = 0;
                                          n_layer_pass < n_layer_passes;
                                        ++n_layer_pass)
                        {
                            uint32_t index_offset    = 0;
                            uint32_t index_max_value = 0;
                            uint32_t index_min_value = 0;

                            mesh_get_layer_pass_property(result,
                                                         n_layer,
                                                         n_layer_pass,
                                                         MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET,
                                                        &index_offset);
                            mesh_get_layer_pass_property(result,
                                                         n_layer,
                                                         n_layer_pass,
                                                         MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX,
                                                        &index_max_value);
                            mesh_get_layer_pass_property(result,
                                                         n_layer,
                                                         n_layer_pass,
                                                         MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX,
                                                        &index_min_value);

                            system_file_serializer_write(serializer,
                                                         sizeof(index_offset),
                                                        &index_offset);
                            system_file_serializer_write(serializer,
                                                         sizeof(index_max_value),
                                                        &index_max_value);
                            system_file_serializer_write(serializer,
                                                         sizeof(index_min_value),
                                                        &index_min_value);
                        } /* for (all layer passes) */
                    } /* for (all layers) */

                    /* All done */
                    system_file_serializer_release(serializer);
                }

                /* Fill mesh's GL buffer with data */
                mesh_fill_gl_buffers(result, context);
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve geometry descriptor at index [%d]", n_mesh);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh_by_name(__in __notnull collada_data              data,
                                                              __in __notnull ogl_context               context,
                                                              __in           system_hashed_ansi_string name)
{
    LOG_INFO("Invoked collada_data_get_emerald_mesh_by_name() is a slow code path!");

    _collada_data*     collada_data_ptr = (_collada_data*) data;
    unsigned int       n_found_mesh     = -1;
    const unsigned int n_meshes         = system_resizable_vector_get_amount_of_elements(collada_data_ptr->geometries);
    mesh               result           = NULL;

    for (unsigned int n_mesh = 0; n_mesh < n_meshes; ++n_mesh)
    {
        collada_data_geometry geometry = NULL;

        system_resizable_vector_get_element_at(collada_data_ptr->geometries, n_mesh, &geometry);

        if (geometry != NULL)
        {
            system_hashed_ansi_string geometry_id = NULL;

            collada_data_geometry_get_property(geometry,
                                               COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                               &geometry_id);

            if (system_hashed_ansi_string_is_equal_to_hash_string(geometry_id, name) )
            {
                n_found_mesh = n_mesh;

                break;
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve geometry descriptor at index [%d]", n_mesh);
        }
    }

    if (n_found_mesh != -1)
    {
        result = collada_data_get_emerald_mesh(data, context, n_found_mesh);
    }
    else
    {
        LOG_ERROR("Requested mesh [%s] was not found", system_hashed_ansi_string_get_buffer(name) );
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene collada_data_get_emerald_scene(__in __notnull collada_data data,
                                                        __in __notnull ogl_context  context,
                                                                       unsigned int n_scene)
{
    _collada_data* collada_data_ptr = (_collada_data*) data;
    scene          result           = NULL;

    if (system_resizable_vector_get_amount_of_elements(collada_data_ptr->scenes) > n_scene)
    {
        collada_data_scene scene = NULL;

        system_resizable_vector_get_element_at(collada_data_ptr->scenes, n_scene, &scene);

        if (scene != NULL)
        {
            collada_data_scene_get_property(scene,
                                            COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE,
                                           &result);

            if (result == NULL)
            {
                system_hashed_ansi_string scene_name = NULL;

                collada_data_scene_get_property(scene,
                                                COLLADA_DATA_SCENE_PROPERTY_NAME,
                                               &scene_name);

                /* Must be first-time request for the scene. Convert COLLADA representation to an
                 * Emerald instance of the scene */
                ASSERT_DEBUG_SYNC(scene_name != NULL                                                 &&
                                  scene_name != system_hashed_ansi_string_get_default_empty_string(),
                                  "Scene name is NULL");

                result = collada_scene_generator_create(data, context, n_scene);
                ASSERT_ALWAYS_SYNC(result != NULL,
                                   "Could not convert COLLADA scene data to Emerald scene");

                collada_data_scene_set_property(scene,
                                                COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE,
                                                &result);
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve scene descriptor at index [%d]", n_scene);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_get_geometry(__in __notnull  collada_data           data,
                                                  __in            unsigned int           n_geometry,
                                                  __out __notnull collada_data_geometry* out_geometry)
{
    _collada_data* data_ptr = (_collada_data*) data;

    system_resizable_vector_get_element_at(data_ptr->geometries, n_geometry, out_geometry);
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_get_image(__in __notnull  collada_data        data,
                                               __in            unsigned int        n_image,
                                               __out __notnull collada_data_image* out_image)
{
    _collada_data* data_ptr = (_collada_data*) data;

    system_resizable_vector_get_element_at(data_ptr->images, n_image, out_image);
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_get_material(__in __notnull  collada_data           data,
                                                  __in            unsigned int           n_material,
                                                  __out __notnull collada_data_material* out_material)
{
    _collada_data* data_ptr = (_collada_data*) data;

    system_resizable_vector_get_element_at(data_ptr->materials, n_material, out_material);
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_get_scene(__in  __notnull collada_data        data,
                                               __in            unsigned int        n_scene,
                                               __out __notnull collada_data_scene* out_scene)
{
    _collada_data* data_ptr = (_collada_data*) data;

    if (!system_resizable_vector_get_element_at(data_ptr->scenes, n_scene, out_scene) )
    {
        LOG_FATAL("Could not retrieve COLLADA scene at index [%d]", n_scene);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API collada_data collada_data_load(__in __notnull system_hashed_ansi_string filename,
                                                  __in __notnull system_hashed_ansi_string name,
                                                  __in           bool                      should_generate_cache_blobs)
{
    _collada_data* new_collada_data = new (std::nothrow) _collada_data;

    ASSERT_DEBUG_SYNC(new_collada_data != NULL, "Out of memory");
    if (new_collada_data != NULL)
    {
        _collada_data_init(new_collada_data, filename, should_generate_cache_blobs);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_collada_data,
                                                       _collada_data_release,
                                                       OBJECT_TYPE_COLLADA_DATA,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\COLLADA Data\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (collada_data) new_collada_data;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_set_property(__in __notnull collada_data          data,
                                                  __in           collada_data_property property,
                                                  __in __notnull void*                 in_data)
{
    _collada_data* data_ptr = (_collada_data*) data;

    switch (property)
    {
        case COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE:
        {
            data_ptr->use_cache_binary_blobs_mode = *((bool*) in_data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA data property requested");
        }
    } /* switch (property) */
}
