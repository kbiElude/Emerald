
/**
 *
 * Emerald (kbi/elude @2014-2016)
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

    bool is_lw10_collada_file;

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
        animations                     = system_resizable_vector_create(4 /* capacity */);
        cameras                        = system_resizable_vector_create(4 /* capacity */);
        cameras_by_id_map              = system_hash64map_create       (sizeof(collada_data_camera) );
        effects                        = system_resizable_vector_create(4 /* capacity */);
        effects_by_id_map              = system_hash64map_create       (sizeof(collada_data_effect) );
        geometries                     = system_resizable_vector_create(4      /* capacity              */,
                                                                        true); /* should_be_thread_safe */
        geometries_by_id_map           = system_hash64map_create       (sizeof(collada_data_geometry),
                                                                        true); /* should_be_thread_safe */
        images                         = system_resizable_vector_create(4 /* capacity */);
        images_by_id_map               = system_hash64map_create       (sizeof(collada_data_image) );
        is_lw10_collada_file           = false;
        lights                         = system_resizable_vector_create(1 /* capacity */);
        lights_by_id_map               = system_hash64map_create       (sizeof(collada_data_light) );
        materials                      = system_resizable_vector_create(4 /* capacity */);
        materials_by_id_map            = system_hash64map_create       (sizeof(collada_data_material) );
        max_animation_duration         = 0.0f;
        nodes_by_id_map                = system_hash64map_create       (sizeof(_collada_data_node*) );
        object_to_animation_vector_map = system_hash64map_create       (sizeof(system_resizable_vector) );
        scenes                         = system_resizable_vector_create(1 /* capacity */);
        scenes_by_id_map               = system_hash64map_create       (sizeof(collada_data_scene) );
        serializer                     = nullptr;
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
    if (animations != nullptr)
    {
        collada_data_animation animation = nullptr;

        while (system_resizable_vector_pop(animations,
                                          &animation) )
        {
            collada_data_animation_release(animation);

            animation = nullptr;
        }

        system_resizable_vector_release(animations);
        animations = nullptr;
    }

    if (object_to_animation_vector_map != nullptr)
    {
        system_hash64           temp_hash   = 0;
        system_resizable_vector temp_vector = nullptr;

        while (system_hash64map_get_element_at(object_to_animation_vector_map,
                                               0, /* index */
                                               &temp_vector,
                                               &temp_hash) )
        {
            system_resizable_vector_release(temp_vector);
            temp_vector = nullptr;

            system_hash64map_remove(object_to_animation_vector_map,
                                    temp_hash);
        }

        system_hash64map_release(object_to_animation_vector_map);
        object_to_animation_vector_map = nullptr;
    }

    if (cameras != nullptr)
    {
        collada_data_camera camera = nullptr;

        while (system_resizable_vector_pop(cameras,
                                          &camera) )
        {
            collada_data_camera_release(camera);

            camera = nullptr;
        }

        system_resizable_vector_release(cameras);
        cameras = nullptr;
    }

    if (cameras_by_id_map != nullptr)
    {
        system_hash64map_release(cameras_by_id_map);
        cameras_by_id_map = nullptr;
    }

    if (effects != nullptr)
    {
        collada_data_effect effect = nullptr;

        while (system_resizable_vector_pop(effects,
                                          &effect) )
        {
            collada_data_effect_release(effect);

            effect = nullptr;
        }

        system_resizable_vector_release(effects);
        effects = nullptr;
    }

    if (effects_by_id_map != nullptr)
    {
        system_hash64map_release(effects_by_id_map);
        effects_by_id_map = nullptr;
    }

    if (geometries != nullptr)
    {
        collada_data_geometry geometry = nullptr;

        while (system_resizable_vector_pop(geometries,
                                          &geometry) )
        {
            collada_data_geometry_release(geometry);

            geometry = nullptr;
        }

        system_resizable_vector_release(geometries);
        geometries = nullptr;
    }

    if (geometries_by_id_map != nullptr)
    {
        system_hash64map_release(geometries_by_id_map);
        geometries_by_id_map = nullptr;
    }

    if (images != nullptr)
    {
        collada_data_image image = nullptr;

        while (system_resizable_vector_pop(images,
                                          &image) )
        {
            collada_data_image_release(image);

            image = nullptr;
        }

        system_resizable_vector_release(images);
        images = nullptr;
    }

    if (lights != nullptr)
    {
        collada_data_light light = nullptr;

        while (system_resizable_vector_pop(lights,
                                          &light) )
        {
            collada_data_light_release(light);

            light = nullptr;
        }

        system_resizable_vector_release(lights);
        lights = nullptr;
    }

    if (lights_by_id_map != nullptr)
    {
        system_hash64map_release(lights_by_id_map);
        lights_by_id_map = nullptr;
    }

    if (materials != nullptr)
    {
        collada_data_material material = nullptr;

        while (system_resizable_vector_pop(materials,
                                          &material) )
        {
            collada_data_material_release(material);

            material = nullptr;
        }

        system_resizable_vector_release(materials);
        materials = nullptr;
    }

    if (materials_by_id_map != nullptr)
    {
        system_hash64map_release(materials_by_id_map);

        materials_by_id_map = nullptr;
    }

    if (nodes_by_id_map != nullptr)
    {
        system_hash64map_release(nodes_by_id_map);
        nodes_by_id_map = nullptr;
    }

    if (scenes != nullptr)
    {
        collada_data_scene scene = nullptr;

        while (system_resizable_vector_pop(scenes,
                                          &scene) )
        {
            collada_data_scene_release(scene);

            scene = nullptr;
        }

        system_resizable_vector_release(scenes);
        scenes = nullptr;
    }

    if (scenes_by_id_map != nullptr)
    {
        system_hash64map_release(scenes_by_id_map);
        scenes_by_id_map = nullptr;
    }

    if (serializer != nullptr)
    {
        system_file_serializer_release(serializer);

        serializer = nullptr;
    }
}


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(collada_data, collada_data, _collada_data);

PRIVATE void                                 _collada_data_apply_animation_data_to_transformation(collada_data_transformation           transformation,
                                                                                                  collada_data_animation                animation,
                                                                                                  collada_data_channel                  channel,
                                                                                                  collada_data_channel_target_component animation_target_component);
PRIVATE void                                 _collada_data_init                                  (_collada_data*                        collada_ptr,
                                                                                                  system_hashed_ansi_string             filename,
                                                                                                  bool                                  should_generate_cache_blobs);
PRIVATE void                                 _collada_data_init_animations                       (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_animations,
                                                                                                  system_hash64map                      result_object_to_animation_vector_map,
                                                                                                  collada_data                          data);
PRIVATE void                                 _collada_data_init_cameras                          (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_cameras,
                                                                                                  system_hash64map                      result_cameras_by_id_map);
PRIVATE void                                 _collada_data_init_effects                          (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_effects,
                                                                                                  system_hash64map                      result_effects_by_id_map,
                                                                                                  system_hash64map                      images_by_id_map);
PRIVATE void                                 _collada_data_init_general                          (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  _collada_data*                        collada_data_ptr);
PRIVATE void                                 _collada_data_init_geometries                       (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_geometries,
                                                                                                  system_hash64map                      result_geometries_by_id_map,
                                                                                                  collada_data                          collada_data);
PRIVATE void                                 _collada_data_init_images                           (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_images,
                                                                                                  system_hash64map                      result_images_by_id_map);
PRIVATE void                                 _collada_data_init_lights                           (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_lights,
                                                                                                  system_hash64map                      result_lights_by_id_map);
PRIVATE void                                 _collada_data_init_materials                        (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               materials,
                                                                                                  system_hash64map                      materials_by_id_map,
                                                                                                  system_hash64map                      effects_by_id_map);
PRIVATE void                                 _collada_data_init_scenes                           (tinyxml2::XMLDocument*                xml_document_ptr,
                                                                                                  system_resizable_vector               result_scenes,
                                                                                                  system_hash64map                      result_scenes_by_id_map,
                                                                                                  system_hash64map                      result_nodes_by_id_map,
                                                                                                  _collada_data*                        loader_ptr);
PRIVATE void                                 _collada_data_release                               (void*                                 arg);


/** TODO */
PRIVATE void _collada_data_apply_animation_data(_collada_data* data_ptr)
{
    uint32_t n_animations = 0;
    
    system_resizable_vector_get_property(data_ptr->animations,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_animations);

    /* Iterate over all collada_data_animation instances */
    for (uint32_t n_animation = 0;
                  n_animation < n_animations;
                ++n_animation)
    {
        collada_data_animation current_animation = nullptr;

        if (system_resizable_vector_get_element_at(data_ptr->animations,
                                                   n_animation,
                                                  &current_animation) )
        {
            /* Retrieve channel & sampler instances */
            collada_data_channel animation_channel = nullptr;
            collada_data_sampler animation_sampler = nullptr;

            collada_data_animation_get_property(current_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL,
                                               &animation_channel);
            collada_data_animation_get_property(current_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER,
                                               &animation_sampler);

            /* Retrieve target details */
            void*                                 animation_target           = nullptr;
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
                    ASSERT_DEBUG_SYNC(false,
                                      "TODO");

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
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not get a collada_data_animation instance at index [%d]",
                              n_animation);
        }
    }
}

/** TODO */
PRIVATE void _collada_data_apply_animation_data_to_transformation(collada_data_transformation           transformation,
                                                                  collada_data_animation                animation,
                                                                  collada_data_channel                  channel,
                                                                  collada_data_channel_target_component animation_target_component)
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
            ASSERT_DEBUG_SYNC(false,
                              "TODO");

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE:
        {
            collada_value axis_vector[3] = {nullptr};
            collada_value angle          =  nullptr;

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
            }

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_SCALE:
        {
            collada_value scale_vector[3] = {nullptr};

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
            }

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE:
        {
            collada_value translate_vector[3] = {nullptr};

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
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA transformation type");
        }
    }
}

/** TODO */
PRIVATE system_hashed_ansi_string _collada_data_get_emerald_mesh_blob_file_name(system_hashed_ansi_string geometry_name)
{
    const char* strings[] =
    {
        "collada_mesh_gl_blob_",
        system_hashed_ansi_string_get_buffer(geometry_name)
    };
    const unsigned int n_strings = sizeof(strings) / sizeof(strings[0]);

    return system_hashed_ansi_string_create_by_merging_strings(n_strings,
                                                               strings);
}

/** TODO */
PRIVATE void _collada_data_init(_collada_data*            collada_ptr,
                                system_hashed_ansi_string filename,
                                bool                      should_generate_cache_blobs)
{
    collada_ptr->file_name                   = filename;
    collada_ptr->use_cache_binary_blobs_mode = should_generate_cache_blobs;

    /* Read in and parse COLLADA XML file */
    system_file_serializer serializer      = system_file_serializer_create_for_reading(filename);
    const char*            serialized_data = nullptr;

    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                       &serialized_data);

    if (serialized_data != nullptr)
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
    }

    system_file_serializer_release(serializer);
}

/** TODO */
PRIVATE void _collada_data_init_animations(tinyxml2::XMLDocument*  xml_document_ptr,
                                           system_resizable_vector result_animations,
                                           system_hash64map        result_object_to_animation_vector_map,
                                           collada_data            data)
{
    tinyxml2::XMLElement* collada_element_ptr            = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_animation_element_ptr  = nullptr;
    _collada_data*        data_ptr                       = reinterpret_cast<_collada_data*>(data);
    tinyxml2::XMLElement* library_animations_element_ptr = nullptr;

    if (collada_element_ptr == nullptr)
    {
        goto end;
    }

    library_animations_element_ptr = collada_element_ptr->FirstChildElement("library_animations");

    if (library_animations_element_ptr == nullptr)
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
    current_animation_element_ptr = library_animations_element_ptr->FirstChildElement("animation");

    while (current_animation_element_ptr != nullptr)
    {
        collada_data_animation new_animation = collada_data_animation_create(current_animation_element_ptr,
                                                                             data);

        ASSERT_ALWAYS_SYNC(new_animation != nullptr,
                           "Could not create COLLADA animation descriptor")

        if (new_animation != nullptr)
        {
            /* Store the new animation in a vector*/
            system_hashed_ansi_string animation_id = nullptr;
            system_hash64             entry_hash   = 0;

            collada_data_animation_get_property(new_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_ID,
                                               &animation_id);

            entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(animation_id),
                                                 system_hashed_ansi_string_get_length(animation_id) );

            system_resizable_vector_push(result_animations,
                                         new_animation);

            /* Now add the animation to a helper map */
            collada_data_channel    new_animation_channel = nullptr;
            void*                   new_animation_target  = nullptr;
            system_resizable_vector target_vector         = nullptr;

            collada_data_animation_get_property(new_animation,
                                                COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL,
                                               &new_animation_channel);
            collada_data_channel_get_property  (new_animation_channel,
                                                COLLADA_DATA_CHANNEL_PROPERTY_TARGET,
                                               &new_animation_target);

            ASSERT_DEBUG_SYNC(new_animation_target != nullptr,
                              "Animation target is nullptr");

            if (!system_hash64map_get(result_object_to_animation_vector_map,
                                      (system_hash64) new_animation_target,
                                      &target_vector) )
            {
                /* Spawn a new vector and store it under the target's address */
                target_vector = system_resizable_vector_create(4 /* capacity */);

                system_hash64map_insert(result_object_to_animation_vector_map,
                                        (system_hash64) new_animation_target,
                                        target_vector,
                                        nullptr,  /* on_remove_callback */
                                        nullptr); /* on_remove_callback_user_arg */
            }

            system_resizable_vector_push(target_vector,
                                         new_animation);

            /* Finally, take the very last node of the input (of input type) - this tells the length of
             * the curve */
            const float*             new_animation_input_data          = nullptr;
            uint32_t                 new_animation_input_data_n_values = 0;
            collada_data_float_array new_animation_input_float_array   = nullptr;
            collada_data_source      new_animation_input_source        = nullptr;
            collada_data_sampler     new_animation_sampler             = nullptr;

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
        }

        /* Move on */
        current_animation_element_ptr = current_animation_element_ptr->NextSiblingElement("animation");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_cameras(tinyxml2::XMLDocument*  xml_document_ptr,
                                        system_resizable_vector result_cameras,
                                        system_hash64map        result_cameras_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr         = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_camera_element_ptr  = nullptr;
    tinyxml2::XMLElement* library_cameras_element_ptr = nullptr;

    if (collada_element_ptr == nullptr)
    {
        goto end;
    }

    library_cameras_element_ptr = collada_element_ptr->FirstChildElement("library_cameras");

    if (library_cameras_element_ptr == nullptr)
    {
        goto end;
    }

    /* Iterate through all defined cameras */
    current_camera_element_ptr = library_cameras_element_ptr->FirstChildElement("camera");

    while (current_camera_element_ptr != nullptr)
    {
        collada_data_camera new_camera = collada_data_camera_create(current_camera_element_ptr);

        ASSERT_ALWAYS_SYNC(new_camera != nullptr,
                           "Could not create COLLADA camera descriptor")

        if (new_camera != nullptr)
        {
            /* Store the new camera */
            system_hashed_ansi_string camera_id = nullptr;

            collada_data_camera_get_property(new_camera,
                                             COLLADA_DATA_CAMERA_PROPERTY_ID,
                                            &camera_id);

            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(camera_id),
                                                               system_hashed_ansi_string_get_length(camera_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_cameras_by_id_map, entry_hash),
                              "Item already added to a hash-map");

            system_resizable_vector_push(result_cameras, new_camera);
            system_hash64map_insert     (result_cameras_by_id_map,
                                         entry_hash,
                                         new_camera,
                                         nullptr,  /* no remove call-back needed */
                                         nullptr); /* no remove call-back needed */
        }

        /* Move on */
        current_camera_element_ptr = current_camera_element_ptr->NextSiblingElement("camera");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_effects(tinyxml2::XMLDocument*  xml_document_ptr,
                                        system_resizable_vector result_effects,
                                        system_hash64map        result_effects_by_id_map,
                                        system_hash64map        images_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr         = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_effect_element_ptr  = nullptr;
    tinyxml2::XMLElement* library_effects_element_ptr = nullptr;

    if (collada_element_ptr == nullptr)
    {
        goto end;
    }

    library_effects_element_ptr = collada_element_ptr->FirstChildElement("library_effects");

    if (library_effects_element_ptr == nullptr)
    {
        goto end;
    }

    /* Iterate through all defined effects */
    current_effect_element_ptr = library_effects_element_ptr->FirstChildElement("effect");

    while (current_effect_element_ptr != nullptr)
    {
        collada_data_effect new_effect = collada_data_effect_create(current_effect_element_ptr,
                                                                    images_by_id_map);

        ASSERT_DEBUG_SYNC(new_effect != nullptr,
                          "Could not create COLLADA effect descriptor")

        if (new_effect != nullptr)
        {
            system_hashed_ansi_string effect_id = nullptr;

            collada_data_effect_get_property(new_effect,
                                             COLLADA_DATA_EFFECT_PROPERTY_ID,
                                             &effect_id);

            /* Store the new effect */
            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(effect_id),
                                                               system_hashed_ansi_string_get_length(effect_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_effects_by_id_map, entry_hash),
                              "Item already added to a hash-map");

            system_resizable_vector_push(result_effects, new_effect);
            system_hash64map_insert     (result_effects_by_id_map,
                                         entry_hash,
                                         new_effect,
                                         nullptr,  /* no remove call-back needed */
                                         nullptr); /* no remove call-back needed */
        }

        /* Move on */
        current_effect_element_ptr = current_effect_element_ptr->NextSiblingElement("effect");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_general(tinyxml2::XMLDocument*  xml_document_ptr,
                                        _collada_data*          collada_data_ptr)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* asset_element_ptr       = nullptr;
    tinyxml2::XMLElement* collada_element_ptr     = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* contributor_element_ptr = nullptr;
    tinyxml2::XMLElement* up_axis_element_ptr     = nullptr;
    system_hashed_ansi_string up_axis_has         = nullptr;

    if (collada_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate asset subnode */
    asset_element_ptr = collada_element_ptr->FirstChildElement("asset");

    if (asset_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find <asset> sub-node");

        ASSERT_DEBUG_SYNC(false,
                          "Could not find required <asset> sub-node");

        goto end;
    }

    /* Locate contributor/authoring_tool subnode. */
    contributor_element_ptr = asset_element_ptr->FirstChildElement("contributor");

    if (contributor_element_ptr != nullptr)
    {
        tinyxml2::XMLElement* authoring_tool_element_ptr = contributor_element_ptr->FirstChildElement("authoring_tool");

        if (authoring_tool_element_ptr != nullptr)
        {
            if (strcmp(authoring_tool_element_ptr->GetText(),
                       "Newtek LightWave CORE v1.0") == 0)
            {
                collada_data_ptr->is_lw10_collada_file = true;
            }
        }
    }

    /* Locate up_axis subnode. Note that it is optional, so assume Y_UP orientation as per spec if
     * the element is not available.
     */
    collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_Y_UP;

    up_axis_element_ptr = asset_element_ptr->FirstChildElement("up_axis");

    if (up_axis_element_ptr == nullptr)
    {
        /* That's fine */
        goto end;
    }

    /* Parse the contents */
    up_axis_has = system_hashed_ansi_string_create(up_axis_element_ptr->GetText() );

    if (system_hashed_ansi_string_is_equal_to_raw_string(up_axis_has,
                                                         "X_UP") )
    {
        collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_X_UP;

        ASSERT_ALWAYS_SYNC(false,
                           "Up axis of 'X up' type is not supported");

        goto end;
    }
    else
    if (system_hashed_ansi_string_is_equal_to_raw_string(up_axis_has,
                                                         "Y_UP") )
    {
        collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_Y_UP;

        goto end;
    }
    else
    if (system_hashed_ansi_string_is_equal_to_raw_string(up_axis_has,
                                                         "Z_UP") )
    {
        collada_data_ptr->up_axis = COLLADA_DATA_UP_AXIS_Z_UP;

        LOG_ERROR("Current implementation of 'Z up' up axis support is likely not to work correctly with animations!");

        goto end;
    }
    else
    {
        LOG_FATAL("Unrecognized up_axis node contents. Transformations will be messed up!");

        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized up_axis node contents");

        goto end;
    }

end:
    return ;
}

/** TODO */
PRIVATE void _collada_data_init_geometries(tinyxml2::XMLDocument*  xml_document_ptr,
                                           system_resizable_vector result_geometries,
                                           system_hash64map        result_geometries_by_id_map,
                                           collada_data            collada_data)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr           = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_geometry_element_ptr  = nullptr;
    tinyxml2::XMLElement* geometries_element_ptr        = nullptr;
    system_event          geometry_processed_event      = nullptr;
    unsigned int          n_geometry_elements           = 0;
    volatile unsigned int n_geometry_elements_processed = 0; /* this counter will be accessed from multiple threads at the same time - only use interlocked accesses */

    if (collada_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate library_geometries node */
    geometries_element_ptr = collada_element_ptr->FirstChildElement("library_geometries");

    if (geometries_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find library_geometries node");

        goto end;
    }

    /* Large scenes contain lots of geometry that takes time to load. Distribute the load to separate threads
     * to speed the process up.
     */
    current_geometry_element_ptr = geometries_element_ptr->FirstChildElement("geometry");
    geometry_processed_event     = system_event_create                      (true); /* manual_reset */

    ASSERT_ALWAYS_SYNC(geometry_processed_event != nullptr,
                       "Could not generate an event");

    if (geometry_processed_event == nullptr)
    {
        goto end;
    }

    while (current_geometry_element_ptr != nullptr)
    {
        n_geometry_elements++;

        /* Move to next geometry instance */
        current_geometry_element_ptr = current_geometry_element_ptr->NextSiblingElement("geometry");
    }

    /* Iterate through geometry instances (which describe meshes that are of our interest) */
    current_geometry_element_ptr = geometries_element_ptr->FirstChildElement("geometry");

    while (current_geometry_element_ptr != nullptr)
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
    system_event_wait_single(geometry_processed_event);
    system_event_release    (geometry_processed_event);

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_images(tinyxml2::XMLDocument*  xml_document_ptr,
                                       system_resizable_vector result_images,
                                       system_hash64map        result_images_by_id_map)
{
    tinyxml2::XMLElement*     collada_element_ptr        = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement*     current_image_element_ptr  = nullptr;
    tinyxml2::XMLElement*     library_images_element_ptr = nullptr;
    collada_data_image        new_image                  = nullptr;
    system_hashed_ansi_string new_image_id               = nullptr;

    if (collada_element_ptr == nullptr)
    {
        goto end;
    }

    library_images_element_ptr = collada_element_ptr->FirstChildElement("library_images");

    if (library_images_element_ptr == nullptr)
    {
        goto end;
    }

    /* Iterate through all defined images */
    current_image_element_ptr = library_images_element_ptr->FirstChildElement("image");

    while (current_image_element_ptr != nullptr)
    {
        new_image    = collada_data_image_create(current_image_element_ptr);
        new_image_id = nullptr;

        if (new_image != nullptr)
        {
            collada_data_image_get_property(new_image,
                                            COLLADA_DATA_IMAGE_PROPERTY_ID,
                                            &new_image_id);

            /* Store the new image */
            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(new_image_id),
                                                               system_hashed_ansi_string_get_length(new_image_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_images_by_id_map, entry_hash),
                              "Item already added to a hash-map");

            system_resizable_vector_push(result_images,
                                         new_image);
            system_hash64map_insert     (result_images_by_id_map,
                                         entry_hash,
                                         new_image,
                                         nullptr,  /* no remove call-back needed */
                                         nullptr); /* no remove call-back needed */
        }

        /* Move on */
        current_image_element_ptr = current_image_element_ptr->NextSiblingElement("image");
    }

end: ;
}

/* TODO */
PRIVATE void _collada_data_init_lights(tinyxml2::XMLDocument*  xml_document_ptr,
                                       system_resizable_vector result_lights,
                                       system_hash64map        result_lights_by_id_map)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr       = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_light_element_ptr = nullptr;
    tinyxml2::XMLElement* lights_element_ptr        = nullptr;

    if (collada_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate library_lights node */
    lights_element_ptr = collada_element_ptr->FirstChildElement("library_lights");

    if (lights_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find library_lights node");

        goto end;
    }

    /* Iterate through all lights */
    current_light_element_ptr = lights_element_ptr->FirstChildElement("light");

    while (current_light_element_ptr != nullptr)
    {
        collada_data_light new_light = collada_data_light_create(current_light_element_ptr);

        ASSERT_DEBUG_SYNC(new_light != nullptr,
                          "Out of memory");

        if (new_light == nullptr)
        {
            goto end;
        }

        /* Store the light descriptor */
        system_hashed_ansi_string light_id = nullptr;

        collada_data_light_get_property(new_light,
                                        COLLADA_DATA_LIGHT_PROPERTY_ID,
                                       &light_id);

        system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(light_id),
                                                           system_hashed_ansi_string_get_length(light_id) );

        ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_lights_by_id_map, entry_hash),
                          "Item already added to a hash-map");

        system_resizable_vector_push(result_lights,
                                     new_light);
        system_hash64map_insert     (result_lights_by_id_map,
                                     entry_hash,
                                     new_light,
                                     nullptr,  /* no remove call-back needed */
                                     nullptr); /* no remove call-back needed */

        /* Move on */
        current_light_element_ptr = current_light_element_ptr->NextSiblingElement("light");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_materials(tinyxml2::XMLDocument*  xml_document_ptr,
                                          system_resizable_vector materials,
                                          system_hash64map        materials_by_id_map,
                                          system_hash64map        effects_by_id_map)
{
    tinyxml2::XMLElement* collada_element_ptr           = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_material_element_ptr  = nullptr;
    tinyxml2::XMLElement* library_materials_element_ptr = nullptr;

    if (collada_element_ptr == nullptr)
    {
        goto end;
    }

    library_materials_element_ptr = collada_element_ptr->FirstChildElement("library_materials");

    if (library_materials_element_ptr == nullptr)
    {
        goto end;
    }

    /* Iterate through all defined images */
    current_material_element_ptr = library_materials_element_ptr->FirstChildElement("material");

    while (current_material_element_ptr != nullptr)
    {
        system_hashed_ansi_string new_material_id = nullptr;
        collada_data_material     new_material    = collada_data_material_create(current_material_element_ptr,
                                                                                 effects_by_id_map,
                                                                                 materials_by_id_map);

        ASSERT_ALWAYS_SYNC(new_material != nullptr,
                           "Could not create COLLADA material descriptor")

        if (new_material != nullptr)
        {
            collada_data_material_get_property(new_material,
                                               COLLADA_DATA_MATERIAL_PROPERTY_ID,
                                               &new_material_id);

            /* Store the new material */
            system_hash64 entry_hash = system_hash64_calculate(system_hashed_ansi_string_get_buffer(new_material_id),
                                                               system_hashed_ansi_string_get_length(new_material_id) );

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(materials_by_id_map,
                                                         entry_hash),
                              "Item already added to a hash-map");

            system_resizable_vector_push(materials,
                                         new_material);
            system_hash64map_insert     (materials_by_id_map,
                                         entry_hash,
                                         new_material,
                                         nullptr,  /* no remove call-back needed */
                                         nullptr); /* no remove call-back needed */
        }

        /* Move on */
        current_material_element_ptr = current_material_element_ptr->NextSiblingElement("material");
    }

end: ;
}

/** TODO */
PRIVATE void _collada_data_init_scenes(tinyxml2::XMLDocument*  xml_document_ptr,
                                       system_resizable_vector result_scenes,
                                       system_hash64map        result_scenes_by_id_map,
                                       system_hash64map        result_nodes_by_id_map,
                                       _collada_data*          collada_data_ptr)
{
    /* Locate COLLADA node */
    tinyxml2::XMLElement* collada_element_ptr       = xml_document_ptr->FirstChildElement("COLLADA");
    tinyxml2::XMLElement* current_scene_element_ptr = nullptr;
    tinyxml2::XMLElement* visual_scenes_element_ptr = nullptr;

    if (collada_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find COLLADA root node");

        goto end;
    }

    /* Locate library_visual_scenes node */
    visual_scenes_element_ptr = collada_element_ptr->FirstChildElement("library_visual_scenes");

    if (visual_scenes_element_ptr == nullptr)
    {
        LOG_ERROR("Could not find library_visual_scenes node");

        goto end;
    }

    /* Iterate through all defined scenes */
    current_scene_element_ptr = visual_scenes_element_ptr->FirstChildElement("visual_scene");

    while (current_scene_element_ptr != nullptr)
    {
        /* Allocate a new descriptor */
        collada_data_scene new_scene = collada_data_scene_create(current_scene_element_ptr,
                                                                 (collada_data) collada_data_ptr);

        /* Store new scene */
        system_resizable_vector_push(result_scenes,
                                     new_scene);

        /* Move on */
        current_scene_element_ptr = current_scene_element_ptr->NextSiblingElement("visual_scene");
    }
end: ;
}

/** TODO */
PRIVATE void _collada_data_release(void* arg)
{
    /* Stub */
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_get_property(collada_data          data,
                                                  collada_data_property property,
                                                  void*                 out_result_ptr)
{
    _collada_data* collada_data_ptr = (_collada_data*) data;

    switch (property)
    {
        case COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = collada_data_ptr->use_cache_binary_blobs_mode;

            break;
        }

        case COLLADA_DATA_PROPERTY_CAMERAS_BY_ID_MAP:
        {
            *reinterpret_cast<system_hash64map*>(out_result_ptr) = collada_data_ptr->cameras_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_FILE_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = collada_data_ptr->file_name;

            break;
        }

        case COLLADA_DATA_PROPERTY_GEOMETRIES_BY_ID_MAP:
        {
            *reinterpret_cast<system_hash64map*>(out_result_ptr) = collada_data_ptr->geometries_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_IS_LW10_COLLADA_FILE:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = collada_data_ptr->is_lw10_collada_file;

            break;
        }

        case COLLADA_DATA_PROPERTY_LIGHTS_BY_ID_MAP:
        {
            *reinterpret_cast<system_hash64map*>(out_result_ptr) = collada_data_ptr->lights_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_MATERIALS_BY_ID_MAP:
        {
            *reinterpret_cast<system_hash64map*>(out_result_ptr) = collada_data_ptr->materials_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_MAX_ANIMATION_DURATION:
        {
            *reinterpret_cast<float*>(out_result_ptr) = collada_data_ptr->max_animation_duration;

            break;
        }

        case COLLADA_DATA_PROPERTY_NODES_BY_ID_MAP:
        {
            *reinterpret_cast<system_hash64map*>(out_result_ptr) = collada_data_ptr->nodes_by_id_map;

            break;
        }

        case COLLADA_DATA_PROPERTY_N_CAMERAS:
        {
            system_resizable_vector_get_property(collada_data_ptr->cameras,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_EFFECTS:
        {
            system_resizable_vector_get_property(collada_data_ptr->effects,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_IMAGES:
        {
            system_resizable_vector_get_property(collada_data_ptr->images,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_MATERIALS:
        {
            system_resizable_vector_get_property(collada_data_ptr->materials,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_MESHES:
        {
            system_resizable_vector_get_property(collada_data_ptr->geometries,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_PROPERTY_N_SCENES:
        {
            system_resizable_vector_get_property(collada_data_ptr->scenes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_PROPERTY_OBJECT_TO_ANIMATION_VECTOR_MAP:
        {
            *reinterpret_cast<system_hash64map*>(out_result_ptr) = collada_data_ptr->object_to_animation_vector_map;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA data property");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API collada_data_camera collada_data_get_camera_by_name(collada_data              data,
                                                                       system_hashed_ansi_string name)
{
    _collada_data*      data_ptr  = reinterpret_cast<_collada_data*>(data);
    unsigned int        n_cameras = 0;
    collada_data_camera result    = nullptr;

    LOG_INFO("collada_data_get_camera_by_name(): Slow code-path called");

    system_resizable_vector_get_property(data_ptr->cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_cameras);

    for (unsigned int n_camera = 0;
                      n_camera < n_cameras;
                    ++n_camera)
    {
        collada_data_camera       camera    = nullptr;
        system_hashed_ansi_string camera_id = nullptr;

        if (system_resizable_vector_get_element_at(data_ptr->cameras,
                                                   n_camera,
                                                  &camera) )
        {
            collada_data_camera_get_property(camera,
                                             COLLADA_DATA_CAMERA_PROPERTY_ID,
                                            &camera_id);

            if (system_hashed_ansi_string_is_equal_to_hash_string(camera_id,
                                                                  name) )
            {
                result = camera;

                break;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve camera at index [%d]",
                              n_camera);
        }
    }

    return result;
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_get_effect(collada_data         data,
                                                unsigned int         n_effect,
                                                collada_data_effect* out_effect_ptr)
{
    _collada_data* data_ptr = reinterpret_cast<_collada_data*>(data);

    system_resizable_vector_get_element_at(data_ptr->effects,
                                           n_effect,
                                           out_effect_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh(collada_data data,
                                                      ral_context  context,
                                                      unsigned int n_mesh)
{
    _collada_data*  collada_data_ptr            = reinterpret_cast<_collada_data*>(data);
    bool            has_loaded_blob             = false;
    unsigned int    n_geometries                = 0;
    bool            running_in_cache_blobs_mode = nullptr;
    mesh            result                      = nullptr;

    collada_data_get_property           (data,
                                         COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
                                         &running_in_cache_blobs_mode);
    system_resizable_vector_get_property(collada_data_ptr->geometries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_geometries);

    if (n_geometries > n_mesh)
    {
        collada_data_geometry geometry = nullptr;

        system_resizable_vector_get_element_at(collada_data_ptr->geometries,
                                               n_mesh,
                                              &geometry);

        if (geometry != nullptr)
        {
            collada_data_geometry_get_property(geometry,
                                               COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH,
                                               &result);

            if (result == nullptr)
            {
                system_hashed_ansi_string blob_file_name = nullptr;
                system_hashed_ansi_string geometry_id    = nullptr;

                collada_data_geometry_get_property(geometry,
                                                   COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                                   &geometry_id);

                /* Must be first-time request for the mesh. Convert COLLADA representation to an
                 * Emerald instance of the mesh */
                ASSERT_DEBUG_SYNC(geometry_id != nullptr                                                 &&
                                  geometry_id != system_hashed_ansi_string_get_default_empty_string(),
                                  "Geometry name is nullptr");

                blob_file_name = _collada_data_get_emerald_mesh_blob_file_name(geometry_id);
                result         = collada_mesh_generator_create                (context,
                                                                               data,
                                                                               n_mesh);

                collada_data_geometry_set_property(geometry,
                                                   COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH,
                                                   &result);

                ASSERT_ALWAYS_SYNC(result != nullptr,
                                   "Could not convert COLLADA geometry data to Emerald mesh");

                /* Cache blob data IF we're running in cache blobs mode. More info below */
                if (running_in_cache_blobs_mode)
                {
                    /* Is the blob available already? */
                    system_file_serializer serializer      = system_file_serializer_create_for_reading(blob_file_name,
                                                                                                       false); /* async_read */
                    const void*            serializer_data = nullptr;

                    system_file_serializer_get_property(serializer,
                                                        SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                                       &serializer_data);

                    if (serializer_data != nullptr)
                    {
                        const void*      blob_data                 = nullptr;
                        _mesh_index_type index_type                = MESH_INDEX_TYPE_UNKNOWN;
                        uint32_t         n_blob_data_bytes         = 0;
                        uint32_t         serializer_current_offset = 0;
                        const char*      serializer_raw_storage    = nullptr;

                        system_file_serializer_read(serializer,
                                                    sizeof(index_type),
                                                    &index_type);
                        system_file_serializer_read(serializer,
                                                    sizeof(n_blob_data_bytes),
                                                   &n_blob_data_bytes);

                        system_file_serializer_get_property(serializer,
                                                            SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                                           &serializer_raw_storage);
                        system_file_serializer_get_property(serializer,
                                                            SYSTEM_FILE_SERIALIZER_PROPERTY_CURRENT_OFFSET,
                                                            &serializer_current_offset);

                        blob_data = serializer_raw_storage + serializer_current_offset;

                        mesh_set_single_indexed_representation(result,
                                                               index_type,
                                                               n_blob_data_bytes,
                                                               blob_data);

                        system_file_serializer_read(serializer,
                                                    n_blob_data_bytes,
                                                    nullptr);

                        /* Also read unique stream data offsets */
                        unsigned int n_serialized_max_stream_types = 0;

                        system_file_serializer_read(serializer,
                                                    sizeof(n_serialized_max_stream_types),
                                                   &n_serialized_max_stream_types);

                        ASSERT_DEBUG_SYNC(n_serialized_max_stream_types == MESH_LAYER_DATA_STREAM_TYPE_COUNT,
                                          "COLLADA mesh data uses an unsupported number of data stream types");

                        for (mesh_layer_data_stream_type stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                                         stream_type < static_cast<int32_t>(n_serialized_max_stream_types);
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

                            n_result_layer_passes = mesh_get_number_of_layer_passes(result,
                                                                                    n_layer);

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
                                                    MESH_LAYER_PROPERTY_MODEL_AABB_MAX,
                                                    aabb_max_data);
                            mesh_set_layer_property(result,
                                                    n_layer,
                                                    0, /* n_pass - OK to use zero, this state does not work on pass level */
                                                    MESH_LAYER_PROPERTY_MODEL_AABB_MIN,
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
                            }
                        }

                        /* All done */
                        has_loaded_blob = true;
                    }
                    else
                    {
                        /* The file was not found */
                    }

                    system_file_serializer_release(serializer);
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

                    mesh_get_property(result, MESH_PROPERTY_GL_INDEX_TYPE,
                                     &gl_index_type);
                    mesh_get_property(result, MESH_PROPERTY_GL_PROCESSED_DATA,
                                     &gl_data);
                    mesh_get_property(result, MESH_PROPERTY_GL_PROCESSED_DATA_SIZE,
                                     &gl_data_size);
                    mesh_get_property(result, MESH_PROPERTY_GL_STRIDE,
                                     &gl_data_stride);
                    mesh_get_property(result, MESH_PROPERTY_GL_TOTAL_ELEMENTS,
                                     &gl_data_total_elements);

                    system_file_serializer_write(serializer,
                                                 sizeof(gl_index_type),
                                                &gl_index_type);
                    system_file_serializer_write(serializer,
                                                 sizeof(gl_data_size),
                                                &gl_data_size);
                    system_file_serializer_write(serializer,
                                                 gl_data_size,
                                                 gl_data);

                    /* Also store unique stream offsets */
                    const unsigned int n_max_stream_types = MESH_LAYER_DATA_STREAM_TYPE_COUNT;

                    system_file_serializer_write(serializer,
                                                 sizeof(n_max_stream_types),
                                                &n_max_stream_types);

                    for (mesh_layer_data_stream_type stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                                     stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                             ++(int&)stream_type)
                    {
                        unsigned int stream_offset = 0;

                        mesh_get_layer_data_stream_property(result,
                                                            0, /* layer_id - irrelevant for regular meshes */
                                                            stream_type,
                                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                                           &stream_offset);

                        system_file_serializer_write(serializer,
                                                     sizeof(stream_offset),
                                                    &stream_offset);
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
                        const float* aabb_max_data = nullptr;
                        const float* aabb_min_data = nullptr;

                        n_layer_passes = mesh_get_number_of_layer_passes(result,
                                                                         n_layer);

                        mesh_get_layer_pass_property(result,
                                                     n_layer,
                                                     0, /* n_pass - zero is fine, this state does not depend on layer passes */
                                                     MESH_LAYER_PROPERTY_MODEL_AABB_MAX,
                                                    &aabb_max_data);
                        mesh_get_layer_pass_property(result,
                                                     n_layer,
                                                     0, /* n_pass - zero is fine, this state does not depend on layer passes */
                                                     MESH_LAYER_PROPERTY_MODEL_AABB_MIN,
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
                        }
                    }

                    /* All done */
                    system_file_serializer_release(serializer);
                }

                /* Fill mesh's GL buffer with data */
                mesh_fill_gl_buffers(result,
                                     context,
                                     false /* async */);
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve geometry descriptor at index [%u]",
                      n_mesh);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh_by_name(collada_data              data,
                                                              ral_context               context,
                                                              system_hashed_ansi_string name)
{
    LOG_INFO("Invoked collada_data_get_emerald_mesh_by_name() is a slow code path!");

    _collada_data* collada_data_ptr = reinterpret_cast<_collada_data*>(data);
    unsigned int   n_found_mesh     = -1;
    unsigned int   n_meshes         = 0;
    mesh           result           = nullptr;

    system_resizable_vector_get_property(collada_data_ptr->geometries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_meshes);

    for (unsigned int n_mesh = 0;
                      n_mesh < n_meshes;
                    ++n_mesh)
    {
        collada_data_geometry geometry = nullptr;

        system_resizable_vector_get_element_at(collada_data_ptr->geometries,
                                               n_mesh,
                                              &geometry);

        if (geometry != nullptr)
        {
            system_hashed_ansi_string geometry_id = nullptr;

            collada_data_geometry_get_property(geometry,
                                               COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                               &geometry_id);

            if (system_hashed_ansi_string_is_equal_to_hash_string(geometry_id,
                name) )
            {
                n_found_mesh = n_mesh;

                break;
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve geometry descriptor at index [%u]",
                      n_mesh);
        }
    }

    if (n_found_mesh != -1)
    {
        result = collada_data_get_emerald_mesh(data,
                                               context,
                                               n_found_mesh);
    }
    else
    {
        LOG_ERROR("Requested mesh [%s] was not found",
                  system_hashed_ansi_string_get_buffer(name) );
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene collada_data_get_emerald_scene(collada_data data,
                                                        ral_context  context,
                                                        unsigned int n_scene)
{
    _collada_data* collada_data_ptr = reinterpret_cast<_collada_data*>(data);
    unsigned int   n_scenes         = 0;
    scene          result           = nullptr;

    system_resizable_vector_get_property(collada_data_ptr->scenes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_scenes);

    if (n_scenes > n_scene)
    {
        collada_data_scene scene = nullptr;

        system_resizable_vector_get_element_at(collada_data_ptr->scenes,
                                               n_scene,
                                              &scene);

        if (scene != nullptr)
        {
            collada_data_scene_get_property(scene,
                                            COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE,
                                           &result);

            if (result == nullptr)
            {
                system_hashed_ansi_string scene_name = nullptr;

                collada_data_scene_get_property(scene,
                                                COLLADA_DATA_SCENE_PROPERTY_NAME,
                                               &scene_name);

                /* Must be first-time request for the scene. Convert COLLADA representation to an
                 * Emerald instance of the scene */
                ASSERT_DEBUG_SYNC(scene_name != nullptr                                                 &&
                                  scene_name != system_hashed_ansi_string_get_default_empty_string(),
                                  "Scene name is nullptr");

                result = collada_scene_generator_create(data,
                                                        context,
                                                        n_scene);

                ASSERT_ALWAYS_SYNC(result != nullptr,
                                   "Could not convert COLLADA scene data to Emerald scene");

                collada_data_scene_set_property(scene,
                                                COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE,
                                                &result);
            }
        }
        else
        {
            LOG_ERROR("Could not retrieve scene descriptor at index [%u]",
                      n_scene);
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_get_geometry(collada_data           data,
                                                  unsigned int           n_geometry,
                                                  collada_data_geometry* out_geometry_ptr)
{
    _collada_data* data_ptr = reinterpret_cast<_collada_data*>(data);

    system_resizable_vector_get_element_at(data_ptr->geometries,
                                           n_geometry,
                                           out_geometry_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_get_image(collada_data        data,
                                               unsigned int        n_image,
                                               collada_data_image* out_image_ptr)
{
    _collada_data* data_ptr = reinterpret_cast<_collada_data*>(data);

    system_resizable_vector_get_element_at(data_ptr->images,
                                           n_image,
                                           out_image_ptr);
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_get_material(collada_data           data,
                                                  unsigned int           n_material,
                                                  collada_data_material* out_material_ptr)
{
    _collada_data* data_ptr = reinterpret_cast<_collada_data*>(data);

    system_resizable_vector_get_element_at(data_ptr->materials,
                                           n_material,
                                           out_material_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_get_scene(collada_data        data,
                                               unsigned int        n_scene,
                                               collada_data_scene* out_scene_ptr)
{
    _collada_data* data_ptr = reinterpret_cast<_collada_data*>(data);

    if (!system_resizable_vector_get_element_at(data_ptr->scenes,
                                                n_scene,
                                                out_scene_ptr) )
    {
        LOG_FATAL("Could not retrieve COLLADA scene at index [%u]",
                  n_scene);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API collada_data collada_data_load(system_hashed_ansi_string filename,
                                                  system_hashed_ansi_string name,
                                                  bool                      should_generate_cache_blobs)
{
    _collada_data* new_collada_data = new (std::nothrow) _collada_data;

    ASSERT_DEBUG_SYNC(new_collada_data != nullptr,
                      "Out of memory");

    if (new_collada_data != nullptr)
    {
        _collada_data_init(new_collada_data,
                           filename,
                           should_generate_cache_blobs);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_collada_data,
                                                       _collada_data_release,
                                                       OBJECT_TYPE_COLLADA_DATA,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\COLLADA Data\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (collada_data) new_collada_data;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_set_property(collada_data          data,
                                                  collada_data_property property,
                                                  void*                 in_data)
{
    _collada_data* data_ptr = reinterpret_cast<_collada_data*>(data);

    switch (property)
    {
        case COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE:
        {
            data_ptr->use_cache_binary_blobs_mode = *((bool*) in_data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA data property requested");
        }
    }
}
