/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_sampler.h"
#include "ogl/ogl_samplers.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_textures.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
#include "scene/scene_material.h"
#include "system/system_callback_manager.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_variant.h"

/** TODO */
typedef struct _mesh_material_property_texture
{
    mesh_material_texture_filtering mag_filter;
    mesh_material_texture_filtering min_filter;
    unsigned int                    mipmap_level;
    ogl_sampler                     sampler;
    ogl_texture                     texture;

    _mesh_material_property_texture()
    {
        mag_filter   = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
        min_filter   = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
        mipmap_level = -1;
        sampler      = NULL;
        texture      = NULL;
    }

    ~_mesh_material_property_texture()
    {
        if (sampler != NULL)
        {
            ogl_sampler_release(sampler);

            sampler = NULL;
        }

        if (texture != NULL)
        {
            ogl_texture_release(texture);

            texture = NULL;
        }
    }
} _mesh_material_property_texture;

/** TODO */
typedef struct _mesh_material_property
{
    mesh_material_property_attachment attachment;

    curve_container                        curve_container_data[3];
    mesh_material_data_vector              data_vector_data;
    float                                  float_data;
    mesh_material_input_fragment_attribute input_fragment_attribute_data;
    _mesh_material_property_texture        texture_data;
    float                                  vec4_data[4];

    _mesh_material_property()
    {
        attachment                    = MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE;
        data_vector_data              = MESH_MATERIAL_DATA_VECTOR_UNKNOWN;
        float_data                    = -1.0f;
        input_fragment_attribute_data = MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN;

        memset(curve_container_data,
               0,
               sizeof(curve_container_data) );
        memset(vec4_data,
               0,
               sizeof(vec4_data) );
    }
} _mesh_material_property;

/** TODO */
typedef struct _mesh_material
{
    system_callback_manager   callback_manager;
    ogl_context               context;
    bool                      dirty;
    system_hashed_ansi_string name;
    system_hashed_ansi_string object_manager_path;
    scene                     owner_scene;
    mesh_material_shading     shading;
    _mesh_material_property   shading_properties[MESH_MATERIAL_SHADING_PROPERTY_COUNT];
    scene_material            source_scene_material;
    system_variant            temp_variant_float;
    ogl_uber                  uber_non_sm; /*         uses shadow maps for per-light visibility calculation */
    ogl_uber                  uber_sm;     /* does not use shadow maps for per-light visibility calculation */

    mesh_material_fs_behavior fs_behavior;
    system_hashed_ansi_string uv_map_name; /* NULL by default, needs to be manually set */
    float                     vertex_smoothing_angle;
    mesh_material_vs_behavior vs_behavior;

    _mesh_material()
    {
        callback_manager       = NULL;
        context                = NULL;
        dirty                  = true;
        fs_behavior            = MESH_MATERIAL_FS_BEHAVIOR_DEFAULT;
        name                   = NULL;
        object_manager_path    = NULL;
        owner_scene            = NULL;
        source_scene_material  = NULL;
        temp_variant_float     = system_variant_create(SYSTEM_VARIANT_FLOAT);
        uber_non_sm            = NULL;
        uber_sm                = NULL;
        uv_map_name            = NULL;
        vertex_smoothing_angle = 0.0f;
        vs_behavior            = MESH_MATERIAL_VS_BEHAVIOR_DEFAULT;
    }

    REFCOUNT_INSERT_VARIABLES
} _mesh_material;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh_material,
                               mesh_material,
                              _mesh_material);


/** TODO */
PRIVATE void _mesh_material_deinit_shading_property_attachment(__in_opt _mesh_material_property& material_property)
{
    if (material_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT ||
        material_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3)
    {
        for (unsigned int n_component = 0;
                          n_component < sizeof(material_property.curve_container_data) / sizeof(material_property.curve_container_data[0]);
                        ++n_component)
        {
            if (material_property.curve_container_data[n_component] != NULL)
            {
                curve_container_release(material_property.curve_container_data[n_component]);

                material_property.curve_container_data[n_component] = NULL;
            }
        } /* for (all components) */
    } /* for (curve container based attachments) */
}


/** TODO */
PRIVATE void _mesh_material_on_lights_added_to_scene(const void* not_used,
                                                     void*       material)
{
    /* We will need a new ogl_uber instance for the re-configured scene. */
    ((_mesh_material*) material)->dirty = true;
}

/** TODO */
PRIVATE void _mesh_material_release(void* data_ptr)
{
    _mesh_material* material_ptr = (_mesh_material*) data_ptr;

    /* Release any textures that may have been attached to the material */
    LOG_INFO("Releasing material [%s]",
             system_hashed_ansi_string_get_buffer(material_ptr->name) );

    for (mesh_material_shading_property current_property = MESH_MATERIAL_SHADING_PROPERTY_FIRST;
                                        current_property < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                                ++(int&)current_property)
    {
        switch (material_ptr->shading_properties[current_property].attachment)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
            {
                const unsigned int n_components = (material_ptr->shading_properties[current_property].attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT) ? 1 : 3;

                for (unsigned int n_component = 0;
                                  n_component < n_components;
                                ++n_component)
                {
                    curve_container_release(material_ptr->shading_properties[current_property].curve_container_data[n_component]);

                    material_ptr->shading_properties[current_property].curve_container_data[n_component] = NULL;
                }

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                ogl_sampler_release(material_ptr->shading_properties[current_property].texture_data.sampler);
                ogl_texture_release(material_ptr->shading_properties[current_property].texture_data.texture);

                material_ptr->shading_properties[current_property].texture_data.sampler = NULL;
                material_ptr->shading_properties[current_property].texture_data.texture = NULL;

                break;
            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE: */
        }
    }

    if (material_ptr->temp_variant_float != NULL)
    {
        system_variant_release(material_ptr->temp_variant_float);

        material_ptr->temp_variant_float = NULL;
    }

    /* Release callback manager */
    if (material_ptr->callback_manager != NULL)
    {
        system_callback_manager_release(material_ptr->callback_manager);

        material_ptr->callback_manager = NULL;
    }
}

/* Please see header for specification */
PRIVATE GLenum _mesh_material_get_glenum_for_mesh_material_texture_filtering(__in mesh_material_texture_filtering filtering)
{
    GLenum result = GL_NONE;

    switch (filtering)
    {
        case MESH_MATERIAL_TEXTURE_FILTERING_LINEAR:          result = GL_LINEAR;                 break;
        case MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR:   result = GL_LINEAR_MIPMAP_LINEAR;   break;
        case MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_NEAREST:  result = GL_LINEAR_MIPMAP_NEAREST;  break;
        case MESH_MATERIAL_TEXTURE_FILTERING_NEAREST:         result = GL_NEAREST;                break;
        case MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_LINEAR:  result = GL_NEAREST_MIPMAP_LINEAR;  break;
        case MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_NEAREST: result = GL_NEAREST_MIPMAP_NEAREST; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized material texture filtering requested [%d]", filtering);
        }
    } /* switch (filtering) */

    return result;
}


/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create(__in     __notnull system_hashed_ansi_string name,
                                                      __in     __notnull ogl_context               context,
                                                      __in_opt           system_hashed_ansi_string object_manager_path)
{
    _mesh_material* new_material = new (std::nothrow) _mesh_material;

    LOG_INFO("Creating material [%s]",
             system_hashed_ansi_string_get_buffer(name) );

    ASSERT_ALWAYS_SYNC(new_material != NULL,
                       "Out of memory");

    if (new_material != NULL)
    {
        ASSERT_DEBUG_SYNC(name != NULL,
                          "Name is NULL");

        new_material->callback_manager    = system_callback_manager_create( (_callback_id) MESH_MATERIAL_CALLBACK_ID_COUNT);
        new_material->context             = context;
        new_material->name                = name;
        new_material->object_manager_path = object_manager_path;
        new_material->uv_map_name         = NULL;
        new_material->vs_behavior         = MESH_MATERIAL_VS_BEHAVIOR_DEFAULT;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_material,
                                                       _mesh_material_release,
                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                                       object_manager_path) );
    }

    return (mesh_material) new_material;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create_copy(__in __notnull system_hashed_ansi_string name,
                                                           __in __notnull mesh_material             src_material)
{
    _mesh_material* new_material_ptr = new (std::nothrow) _mesh_material;
    _mesh_material* src_material_ptr = (_mesh_material*) src_material;

    LOG_INFO("Creating material copy [%s]",
             system_hashed_ansi_string_get_buffer(name) );

    ASSERT_ALWAYS_SYNC(new_material_ptr != NULL,
                       "Out of memory");

    if (new_material_ptr != NULL)
    {
        /* Copy relevant fields */
        new_material_ptr->dirty               = src_material_ptr->dirty;
        new_material_ptr->name                = src_material_ptr->name;
        new_material_ptr->object_manager_path = src_material_ptr->object_manager_path;
        new_material_ptr->shading             = src_material_ptr->shading;
        new_material_ptr->uber_non_sm         = src_material_ptr->uber_non_sm;
        new_material_ptr->uber_sm             = src_material_ptr->uber_sm;
        new_material_ptr->vs_behavior         = src_material_ptr->vs_behavior;

        memcpy(new_material_ptr->shading_properties,
               src_material_ptr->shading_properties,
               sizeof(src_material_ptr->shading_properties) );

        /* Retain assets that are reference-counted */
        for (unsigned int n_shading_properties = 0;
                          n_shading_properties < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                        ++n_shading_properties)
        {
            _mesh_material_property& shading_property = new_material_ptr->shading_properties[n_shading_properties];

            if (shading_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT)
            {
                curve_container_retain(shading_property.curve_container_data[0]);
            }
            else
            if (shading_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3)
            {
                curve_container_retain(shading_property.curve_container_data[0]);
                curve_container_retain(shading_property.curve_container_data[1]);
                curve_container_retain(shading_property.curve_container_data[2]);
            }
            else
            if (shading_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE)
            {
                ogl_sampler_retain(shading_property.texture_data.sampler);
                ogl_texture_retain(shading_property.texture_data.texture);
            }
        }

        /* Set up reference counting for new asset */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_material_ptr,
                                                       _mesh_material_release,
                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                                       src_material_ptr->object_manager_path) );
    }

    return (mesh_material) new_material_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create_from_scene_material(__in     __notnull scene_material src_material,
                                                                          __in_opt           ogl_context    context)
{
    /* Create a new mesh_material instance */
    ogl_textures              context_textures                 = NULL;
    system_hashed_ansi_string src_material_object_manager_path = NULL;
    mesh_material             result_material                  = NULL;
    system_hashed_ansi_string src_material_name                = NULL;

    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_NAME,
                               &src_material_name);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_OBJECT_MANAGER_PATH,
                               &src_material_object_manager_path);

    result_material = mesh_material_create(src_material_name,
                                           context,
                                           src_material_object_manager_path);

    ASSERT_DEBUG_SYNC(result_material != NULL,
                      "mesh_material_create() failed.");
    if (result_material == NULL)
    {
        goto end;
    }

    if (context != NULL)
    {
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TEXTURES,
                                &context_textures);
    }

    /* Update source material */
    ((_mesh_material*) result_material)->source_scene_material = src_material;

    /* Force Phong reflection model */
    const mesh_material_shading shading_type = MESH_MATERIAL_SHADING_PHONG;

    mesh_material_set_property(result_material,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type);

    /* Extract values required to set up the attachment_configs array */
    curve_container*                color                        = NULL;
    system_hashed_ansi_string       color_texture_file_name      = NULL;
    mesh_material_texture_filtering color_texture_mag_filter     = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
    mesh_material_texture_filtering color_texture_min_filter     = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
    curve_container                 glosiness                    = NULL;
    curve_container                 luminance                    = NULL;
    system_hashed_ansi_string       luminance_texture_file_name  = NULL;
    system_hashed_ansi_string       normal_texture_file_name     = NULL;
    system_hashed_ansi_string       reflection_texture_file_name = NULL;
    float                           smoothing_angle              = 0.0f;
    curve_container                 specular_ratio               = NULL;
    system_hashed_ansi_string       specular_texture_file_name   = NULL;

    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_FILE_NAME,
                               &color_texture_file_name);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MAG_FILTER,
                               &color_texture_mag_filter);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_COLOR_TEXTURE_MIN_FILTER,
                               &color_texture_min_filter);

    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_LUMINANCE_TEXTURE_FILE_NAME,
                               &luminance_texture_file_name);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_NORMAL_TEXTURE_FILE_NAME,
                               &normal_texture_file_name);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_REFLECTION_TEXTURE_FILE_NAME,
                               &reflection_texture_file_name);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_SPECULAR_TEXTURE_FILE_NAME,
                               &specular_texture_file_name);

    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_COLOR,
                               &color);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_GLOSINESS,
                               &glosiness);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_LUMINANCE,
                               &luminance);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_SMOOTHING_ANGLE,
                               &smoothing_angle);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_SPECULAR_RATIO,
                               &specular_ratio);

    if (color_texture_file_name                                       != NULL &&
        system_hashed_ansi_string_get_length(color_texture_file_name) == 0)
    {
        color_texture_file_name = NULL;
    }

    if (luminance_texture_file_name                                       != NULL &&
        system_hashed_ansi_string_get_length(luminance_texture_file_name) == 0)
    {
        luminance_texture_file_name = NULL;
    }

    if (normal_texture_file_name                                       != NULL &&
        system_hashed_ansi_string_get_length(normal_texture_file_name) == 0)
    {
        normal_texture_file_name = NULL;
    }

    if (reflection_texture_file_name                                       != NULL &&
        system_hashed_ansi_string_get_length(reflection_texture_file_name) == 0)
    {
        reflection_texture_file_name = NULL;
    }

    if (specular_texture_file_name                                       != NULL &&
        system_hashed_ansi_string_get_length(specular_texture_file_name) == 0)
    {
        specular_texture_file_name = NULL;
    }

    /* Throw an assertion failures for textures that are not yet supported on mesh_material
     * back-end.
     */
    ASSERT_DEBUG_SYNC(luminance_texture_file_name == NULL,
                      "mesh_material does not support luminance textures");

    ASSERT_DEBUG_SYNC(normal_texture_file_name == NULL,
                      "mesh_material does not support normal textures");

    ASSERT_DEBUG_SYNC(reflection_texture_file_name == NULL,
                      "mesh_material does not support reflection textures");

    ASSERT_DEBUG_SYNC(specular_texture_file_name == NULL,
                      "mesh_material does not support specular textures");

    /* Configure texture/float/vec3 attachments (as described by scene_material) */
    typedef struct
    {
        mesh_material_shading_property    shading_property;
        mesh_material_property_attachment property_attachment;

        /* float data */
        float float_data;

        /* curve_container data */
        curve_container* curve_data;

        /* texture data */
        system_hashed_ansi_string       texture_filename;
        mesh_material_texture_filtering texture_mag_filter;
        mesh_material_texture_filtering texture_min_filter;
    } _attachment_configuration;

    const _attachment_configuration attachment_configs[] =
    {
        /* Diffuse attachment */
        {
            MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
            (color_texture_file_name != NULL) ? MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE
                                              : MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3,

            0.0f,

            (color_texture_file_name != NULL) ? NULL : color,

            color_texture_file_name,
            color_texture_mag_filter,
            color_texture_min_filter
        },

        /* Glosiness attachment */
        {
            MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
            MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,

            0.0f,

           &glosiness
        },

        /* Luminance attachment */
        {
            MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
            MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,

            0.0f,

           &luminance,
        },

        /* Specular attachment */
        {
            MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
            MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,

            0.0f,

           &specular_ratio
        },
    };
    const unsigned int n_attachment_configs = sizeof(attachment_configs) /
                                              sizeof(attachment_configs[0]);

    for (unsigned int n_attachment_config = 0;
                      n_attachment_config < n_attachment_configs;
                    ++n_attachment_config)
    {
        const _attachment_configuration& config = attachment_configs[n_attachment_config];

        switch (config.property_attachment)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
            {
                ASSERT_DEBUG_SYNC(config.curve_data[0] != NULL,
                                  "NULL curve container");

                mesh_material_set_shading_property_to_curve_container_float(result_material,
                                                                            config.shading_property,
                                                                            config.curve_data[0]);
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
            {
                mesh_material_set_shading_property_to_curve_container_vec3(result_material,
                                                                           config.shading_property,
                                                                           config.curve_data);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
            {
                mesh_material_set_shading_property_to_float(result_material,
                                                            config.shading_property,
                                                            config.float_data);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                /* Do not attach anything if context is NULL.
                 *
                 * This won't backfire if the mesh_material is an intermediate object used
                 * for mesh baking. Otherwise context should never be NULL.
                 */
                if (context != NULL)
                {
                    ogl_texture texture = NULL;

                    texture = ogl_textures_get_texture_by_filename(context_textures,
                                                                   config.texture_filename);

                    if (texture == NULL)
                    {
                        LOG_INFO("Performance warning: About to call ogl_texture_create() in a scene_multiloader path!");

                        texture = ogl_texture_create(context,
                                                     config.texture_filename,
                                                     config.texture_filename);
                    }

                    ASSERT_ALWAYS_SYNC(texture != NULL,
                                       "Texture [%s] unavailable in the rendering context",
                                       system_hashed_ansi_string_get_buffer(config.texture_filename) );

                    if (texture != NULL)
                    {
                        mesh_material_set_shading_property_to_texture(result_material,
                                                                      config.shading_property,
                                                                      texture,
                                                                      0, /* mipmap_level */
                                                                      config.texture_mag_filter,
                                                                      config.texture_min_filter);

                        /* Generate mip-maps if needed */
                        if (config.texture_min_filter != MESH_MATERIAL_TEXTURE_FILTERING_LINEAR &&
                            config.texture_min_filter != MESH_MATERIAL_TEXTURE_FILTERING_NEAREST)
                        {
                            ogl_texture_generate_mipmaps(texture);
                        }
                    }
                } /* if (context != NULL) */

                break;
            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE: */

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized property attachment type");
            }
        } /* switch (config.property_attachment) */
    } /* for (all attachment configs) */

    /* Configure vertex smoothing angle */
    mesh_material_set_property(result_material,
                               MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                              &smoothing_angle);

end:
    return result_material;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_fs_behavior_has(__in mesh_material_fs_behavior fs_behavior)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* behavior_default_name            = "default";
    static const char* behavior_dual_paraboloid_sm_name = "dual paraboloid SM";

    switch (fs_behavior)
    {
        case MESH_MATERIAL_FS_BEHAVIOR_DEFAULT:
        {
            result = system_hashed_ansi_string_create(behavior_default_name);

            break;
        }

        case MESH_MATERIAL_FS_BEHAVIOR_DUAL_PARABOLOID_SM:
        {
            result = system_hashed_ansi_string_create(behavior_dual_paraboloid_sm_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_fs_behavior value.");
        }
    } /* switch (fs_behavior) */

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_property_attachment_has(__in mesh_material_property_attachment attachment)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* attachment_curve_container_float_name    = "curve container float";
    static const char* attachment_curve_container_vec3_name     = "curve container vec3";
    static const char* attachment_float_name                    = "float";
    static const char* attachment_input_fragment_attribute_name = "input fragment attribute";
    static const char* attachment_none_name                     = "none";
    static const char* attachment_texture_name                  = "texture";
    static const char* attachment_vec4_name                     = "vec4";

    switch (attachment)
    {
        case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
        {
            result = system_hashed_ansi_string_create(attachment_curve_container_float_name);

            break;
        }

        case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
        {
            result = system_hashed_ansi_string_create(attachment_curve_container_vec3_name);

            break;
        }

        case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
        {
            result = system_hashed_ansi_string_create(attachment_float_name);

            break;
        }

        case MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE:
        {
            result = system_hashed_ansi_string_create(attachment_input_fragment_attribute_name);

            break;
        }

        case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
        {
            result = system_hashed_ansi_string_create(attachment_none_name);

            break;
        }

        case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
        {
            result = system_hashed_ansi_string_create(attachment_texture_name);

            break;
        }

        case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
        {
            result = system_hashed_ansi_string_create(attachment_vec4_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_property_attachment value.");
        }
    } /* switch (attachment) */

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_has(__in mesh_material_shading shading)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* shading_input_fragment_attribute_name = "input fragment attribute";
    static const char* shading_lambert_name                  = "lambert";
    static const char* shading_none_name                     = "no shading";
    static const char* shading_phong_name                    = "phong";

    switch (shading)
    {
        case MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE:
        {
            result = system_hashed_ansi_string_create(shading_input_fragment_attribute_name);

            break;
        }

        case MESH_MATERIAL_SHADING_LAMBERT:
        {
            result = system_hashed_ansi_string_create(shading_lambert_name);

            break;
        }

        case MESH_MATERIAL_SHADING_NONE:
        {
            result = system_hashed_ansi_string_create(shading_none_name);

            break;
        }

        case MESH_MATERIAL_SHADING_PHONG:
        {
            result = system_hashed_ansi_string_create(shading_phong_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_shading value.");
        }
    } /* switch (shading) */

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_property_has(__in mesh_material_shading_property property)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* property_ambient_name         = "ambient";
    static const char* property_diffuse_name         = "diffuse";
    static const char* property_input_attribute_name = "input attribute";
    static const char* property_luminosity_name      = "luminosity";
    static const char* property_shininess_name       = "shininess";
    static const char* property_specular_name        = "specular";

    switch (property)
    {
        case MESH_MATERIAL_SHADING_PROPERTY_AMBIENT:
        {
            result = system_hashed_ansi_string_create(property_ambient_name);

            break;
        }

        case MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE:
        {
            result = system_hashed_ansi_string_create(property_diffuse_name);

            break;
        }

        case MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE:
        {
            result = system_hashed_ansi_string_create(property_input_attribute_name);

            break;
        }

        case MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY:
        {
            result = system_hashed_ansi_string_create(property_luminosity_name);

            break;
        }

        case MESH_MATERIAL_SHADING_PROPERTY_SHININESS:
        {
            result = system_hashed_ansi_string_create(property_shininess_name);

            break;
        }

        case MESH_MATERIAL_SHADING_PROPERTY_SPECULAR:
        {
            result = system_hashed_ansi_string_create(property_specular_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_shading_property value.");
        }
    } /* switch (property) */

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_vs_behavior_has(__in mesh_material_vs_behavior vs_behavior)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* behavior_default_name            = "default";
    static const char* behavior_dual_paraboloid_sm_name = "dual paraboloid SM";

    switch (vs_behavior)
    {
        case MESH_MATERIAL_VS_BEHAVIOR_DEFAULT:
        {
            result = system_hashed_ansi_string_create(behavior_default_name);

            break;
        }

        case MESH_MATERIAL_VS_BEHAVIOR_DUAL_PARABOLOID_SM:
        {
            result = system_hashed_ansi_string_create(behavior_dual_paraboloid_sm_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_vs_behavior value.");
        }
    } /* switch (vs_behavior) */

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_uber mesh_material_get_ogl_uber(__in     __notnull mesh_material material,
                                                       __in_opt           scene         scene,
                                                       __in               bool          use_shadow_maps)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    if (material_ptr->dirty)
    {
        ogl_materials materials = NULL;

        ogl_context_get_property(material_ptr->context,
                                 OGL_CONTEXT_PROPERTY_MATERIALS,
                                &materials);

        LOG_INFO("Material is dirty - baking..");

        if (material_ptr->owner_scene == NULL)
        {
            system_callback_manager scene_callback_manager = NULL;

            scene_get_property(scene,
                               SCENE_PROPERTY_CALLBACK_MANAGER,
                              &scene_callback_manager);

            system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                            SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _mesh_material_on_lights_added_to_scene,
                                                            material_ptr);

            material_ptr->owner_scene = scene;
        }

        material_ptr->dirty       = false;
        material_ptr->uber_sm     = ogl_materials_get_uber(materials,
                                                           material,
                                                           scene,
                                                           true); /* use_shadow_maps */
        material_ptr->uber_non_sm = ogl_materials_get_uber(materials,
                                                           material,
                                                           scene,
                                                           false); /* use_shadow_maps */
    }

    return use_shadow_maps ? material_ptr->uber_sm
                           : material_ptr->uber_non_sm;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_property(__in  __notnull mesh_material          material,
                                                   __in            mesh_material_property property,
                                                   __out __notnull void*                  out_result)
{
    const _mesh_material* material_ptr = (_mesh_material*) material;

    switch (property)
    {
        case MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result = material_ptr->callback_manager;

            break;
        }

        case MESH_MATERIAL_PROPERTY_FS_BEHAVIOR:
        {
            *(mesh_material_fs_behavior*) out_result = material_ptr->fs_behavior;

            break;
        }

        case MESH_MATERIAL_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->name;

            break;
        }

        case MESH_MATERIAL_PROPERTY_SHADING:
        {
            *(mesh_material_shading*) out_result = material_ptr->shading;

            break;
        }

        case MESH_MATERIAL_PROPERTY_SOURCE_SCENE_MATERIAL:
        {
            ASSERT_DEBUG_SYNC(material_ptr->source_scene_material != NULL,
                              "Source scene_material is being queried for but none is set for the queried mesh_material instance");

            *(scene_material*) out_result = material_ptr->source_scene_material;

            break;
        }

        case MESH_MATERIAL_PROPERTY_UV_MAP_NAME:
        {
            *(system_hashed_ansi_string*) out_result = material_ptr->uv_map_name;

            break;
        }

        case MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE:
        {
            *(float*) out_result = material_ptr->vertex_smoothing_angle;

            break;
        }

        case MESH_MATERIAL_PROPERTY_VS_BEHAVIOR:
        {
            *(mesh_material_vs_behavior*) out_result = material_ptr->vs_behavior;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_property value");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material_property_attachment mesh_material_get_shading_property_attachment_type(__in __notnull mesh_material                  material,
                                                                                                        __in           mesh_material_shading_property property)
{
    return ((_mesh_material*) material)->shading_properties[property].attachment;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_float(__in      __notnull mesh_material                  material,
                                                                                       __in                mesh_material_shading_property property,
                                                                                       __in                system_timeline_time           time,
                                                                                       __out_opt           float*                         out_float_value)
{
    _mesh_material*          material_ptr = ((_mesh_material*) material);
    _mesh_material_property* property_ptr = material_ptr->shading_properties + property;

    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
                      "Requested shading property is not using a curve container float attachment");

    curve_container_get_value(property_ptr->curve_container_data[0],
                              time,
                              false, /* should_force */
                              material_ptr->temp_variant_float);
    system_variant_get_float (material_ptr->temp_variant_float,
                              out_float_value);
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_vec3(__in      __notnull mesh_material                  material,
                                                                                      __in                mesh_material_shading_property property,
                                                                                      __in                system_timeline_time           time,
                                                                                      __out_ecount_opt(3) float*                         out_vec3_value)
{
    _mesh_material*          material_ptr = ((_mesh_material*) material);
    _mesh_material_property* property_ptr = material_ptr->shading_properties + property;

    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3,
                      "Requested shading property is not using a curve container vec3 attachment");

    for (unsigned int n_component = 0;
                      n_component < 3;
                    ++n_component)
    {
        curve_container_get_value(property_ptr->curve_container_data[n_component],
                                  time,
                                  false, /* should_force */
                                  material_ptr->temp_variant_float);
        system_variant_get_float (material_ptr->temp_variant_float,
                                  out_vec3_value + n_component);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_float(__in      __notnull mesh_material                  material,
                                                                       __in                mesh_material_shading_property property,
                                                                       __out_opt           float*                         out_float_value)
{
    _mesh_material_property* property_ptr = ((_mesh_material*) material)->shading_properties + property;

    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT,
                      "Requested shading property is not using a float attachment");

    *out_float_value = property_ptr->float_data;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_input_fragment_attribute(__in __notnull mesh_material                           material,
                                                                                          __in           mesh_material_shading_property          property,
                                                                                          __out_opt      mesh_material_input_fragment_attribute* out_attribute)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(property == MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                      "Invalid property requested");
    ASSERT_DEBUG_SYNC(material_ptr->shading_properties[property].attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE,
                      "Requested shading property does not have an attribute attachment");

    /* Return the requested value */
    if (out_attribute != NULL)
    {
        *out_attribute = material_ptr->shading_properties[property].input_fragment_attribute_data;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_texture(__in      __notnull mesh_material                    material,
                                                                         __in                mesh_material_shading_property   property,
                                                                         __out_opt           ogl_texture*                     out_texture,
                                                                         __out_opt           unsigned int*                    out_mipmap_level,
                                                                         __out_opt           ogl_sampler*                     out_sampler)
{
    _mesh_material_property*         property_ptr     = ( (_mesh_material*) material)->shading_properties + property;
    _mesh_material_property_texture* texture_data_ptr = &property_ptr->texture_data;

    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE,
                      "Requested shading property is not using a texture attachment");

    if (out_texture != NULL)
    {
        *out_texture = texture_data_ptr->texture;
    }

    if (out_mipmap_level != NULL)
    {
        *out_mipmap_level = texture_data_ptr->mipmap_level;
    }

    if (out_sampler != NULL)
    {
        *out_sampler = texture_data_ptr->sampler;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_vec4(__in           __notnull mesh_material                  material,
                                                                      __in                     mesh_material_shading_property property,
                                                                      __out_ecount(4)          float*                         out_vec4_data)
{
    _mesh_material_property* property_ptr = ((_mesh_material*) material)->shading_properties + property;

    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4,
                      "Requested shading property is not using a vec4 attachment");

    memcpy(out_vec4_data,
           property_ptr->vec4_data,
           sizeof(float) * 4);
}

/* Please see header for specification */
PUBLIC bool mesh_material_is_a_match_to_mesh_material(__in __notnull mesh_material material_a,
                                                      __in __notnull mesh_material material_b)
{
    bool result = false;

    /* We only check the properties that affect how the shaders are built */

    /* 1. Shading */
    mesh_material_shading material_a_shading = MESH_MATERIAL_SHADING_UNKNOWN;
    mesh_material_shading material_b_shading = MESH_MATERIAL_SHADING_UNKNOWN;

    mesh_material_get_property(material_a,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &material_a_shading);
    mesh_material_get_property(material_b,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &material_b_shading);

    if (material_a_shading != material_b_shading)
    {
        goto end;
    }

    /* 2. FS & VS behavior */
    mesh_material_fs_behavior material_a_fs_behavior = MESH_MATERIAL_FS_BEHAVIOR_DEFAULT;
    mesh_material_vs_behavior material_a_vs_behavior = MESH_MATERIAL_VS_BEHAVIOR_DEFAULT;
    mesh_material_fs_behavior material_b_fs_behavior = MESH_MATERIAL_FS_BEHAVIOR_DEFAULT;
    mesh_material_vs_behavior material_b_vs_behavior = MESH_MATERIAL_VS_BEHAVIOR_DEFAULT;

    mesh_material_get_property(material_a,
                               MESH_MATERIAL_PROPERTY_FS_BEHAVIOR,
                              &material_a_fs_behavior);
    mesh_material_get_property(material_a,
                               MESH_MATERIAL_PROPERTY_VS_BEHAVIOR,
                              &material_a_vs_behavior);
    mesh_material_get_property(material_b,
                               MESH_MATERIAL_PROPERTY_FS_BEHAVIOR,
                              &material_b_fs_behavior);
    mesh_material_get_property(material_b,
                               MESH_MATERIAL_PROPERTY_VS_BEHAVIOR,
                              &material_b_vs_behavior);

    if (material_a_fs_behavior != material_b_fs_behavior ||
        material_a_vs_behavior != material_b_vs_behavior)
    {
        goto end;
    }

    /* 3. Shading properties */
    for (unsigned int n_material_property = 0;
                      n_material_property < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                    ++n_material_property)
    {
        mesh_material_shading_property    property              = (mesh_material_shading_property) n_material_property;
        mesh_material_property_attachment material_a_attachment = mesh_material_get_shading_property_attachment_type(material_a,
                                                                                                                     property);
        mesh_material_property_attachment material_b_attachment = mesh_material_get_shading_property_attachment_type(material_b,
                                                                                                                     property);

        if (material_a_attachment != material_b_attachment)
        {
            goto end;
        }

        switch (material_a_attachment)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
            {
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
            {
                /* Single-component floating-point attachments are handled by uniforms so the actual data is irrelevant.
                 * Same goes for curve container based attachments*/
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                ogl_texture_dimensionality material_a_dimensionality;
                ogl_texture                material_a_texture = NULL;
                ogl_texture_dimensionality material_b_dimensionality;
                ogl_texture                material_b_texture = NULL;

                mesh_material_get_shading_property_value_texture(material_a,
                                                                 property,
                                                                &material_a_texture,
                                                                 NULL, /* out_mipmap_level - irrelevant */
                                                                 NULL);/* out_sampler - irrelevant */

                mesh_material_get_shading_property_value_texture(material_b,
                                                                 property,
                                                                &material_b_texture,
                                                                 NULL, /* out_mipmap_level - irrelevant */
                                                                 NULL);/* out_sampler - irrelevant */

                ogl_texture_get_property(material_a_texture,
                                         OGL_TEXTURE_PROPERTY_DIMENSIONALITY,
                                        &material_a_dimensionality);
                ogl_texture_get_property(material_b_texture,
                                         OGL_TEXTURE_PROPERTY_DIMENSIONALITY,
                                        &material_b_dimensionality);

                if (material_a_dimensionality != material_b_dimensionality)
                {
                    goto end;
                }

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
            {
                /* Vec4 attachments are handled by uniforms so the actual data is irrelevant */
                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false, "Unrecognized material property attachment [%d]", material_a_attachment);
            }
        } /* switch (material_a_attachment) */
    } /* for (all material properties) */

    /* Done */
    result = true;
end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_property(__in __notnull mesh_material          material,
                                                   __in           mesh_material_property property,
                                                   __in __notnull const void*            data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    switch (property)
    {
        case MESH_MATERIAL_PROPERTY_FS_BEHAVIOR:
        {
            material_ptr->fs_behavior = *(mesh_material_fs_behavior*) data;
            material_ptr->dirty       = true;

            break;
        }

        case MESH_MATERIAL_PROPERTY_SHADING:
        {
            material_ptr->shading = *(mesh_material_shading*) data;
            material_ptr->dirty   = true;

            break;
        }

        case MESH_MATERIAL_PROPERTY_UV_MAP_NAME:
        {
            material_ptr->uv_map_name = *(system_hashed_ansi_string*) data;

            break;
        }

        case MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE:
        {
            material_ptr->vertex_smoothing_angle = *(float*) data;

            /* Call back registered parties. */
            system_callback_manager_call_back(material_ptr->callback_manager,
                                              MESH_MATERIAL_CALLBACK_ID_VSA_CHANGED,
                                              material);

            break;
        }

        case MESH_MATERIAL_PROPERTY_VS_BEHAVIOR:
        {
            material_ptr->vs_behavior = *(mesh_material_vs_behavior*) data;
            material_ptr->dirty       = true;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_property value");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_float(__in __notnull mesh_material                  material,
                                                                                    __in           mesh_material_shading_property property,
                                                                                    __in           curve_container                data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    _mesh_material_deinit_shading_property_attachment(material_ptr->shading_properties[property]);

    material_ptr->dirty                                                = true;
    material_ptr->shading_properties[property].attachment              = MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT;
    material_ptr->shading_properties[property].curve_container_data[0] = data;

    if (data != NULL)
    {
        curve_container_retain(data);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_vec3(__in           __notnull mesh_material                  material,
                                                                                   __in                     mesh_material_shading_property property,
                                                                                   __in_ecount(3)           curve_container*               data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    _mesh_material_deinit_shading_property_attachment(material_ptr->shading_properties[property]);

    material_ptr->dirty                                                = true;
    material_ptr->shading_properties[property].attachment              = MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3;
    material_ptr->shading_properties[property].curve_container_data[0] = data[0];
    material_ptr->shading_properties[property].curve_container_data[1] = data[1];
    material_ptr->shading_properties[property].curve_container_data[2] = data[2];

    curve_container_retain(data[0]);
    curve_container_retain(data[1]);
    curve_container_retain(data[2]);
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_float(__in __notnull mesh_material                  material,
                                                                    __in           mesh_material_shading_property property,
                                                                    __in           float                          data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    _mesh_material_deinit_shading_property_attachment(material_ptr->shading_properties[property]);

    if (property == MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY ||
        property == MESH_MATERIAL_SHADING_PROPERTY_SHININESS  ||
        property == MESH_MATERIAL_SHADING_PROPERTY_SPECULAR)
    {
        material_ptr->dirty                                   = true;
        material_ptr->shading_properties[property].attachment = MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT;
        material_ptr->shading_properties[property].float_data = data;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unsupported shading property");
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_input_fragment_attribute(__in __notnull mesh_material                          material,
                                                                                       __in           mesh_material_shading_property         property,
                                                                                       __in           mesh_material_input_fragment_attribute attribute)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(material_ptr->shading == MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE,
                      "Invalid mesh material shading");
    ASSERT_DEBUG_SYNC(property == MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                      "Invalid property requested");

    _mesh_material_deinit_shading_property_attachment(material_ptr->shading_properties[property]);

    /* Update property value */
    material_ptr->shading_properties[property].attachment                    = MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE;
    material_ptr->shading_properties[property].input_fragment_attribute_data = attribute;

    /* Mark the material as dirty */
    material_ptr->dirty = true;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_texture(__in __notnull mesh_material                   material,
                                                                      __in           mesh_material_shading_property  property,
                                                                      __in           ogl_texture                     texture,
                                                                      __in           unsigned int                    mipmap_level,
                                                                      __in           mesh_material_texture_filtering mag_filter,
                                                                      __in           mesh_material_texture_filtering min_filter)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    ASSERT_DEBUG_SYNC(texture != NULL, "Bound texture is NULL");

    if (material_ptr->shading_properties[property].attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE)
    {
        ogl_sampler& bound_sampler = material_ptr->shading_properties[property].texture_data.sampler;
        ogl_texture& bound_texture = material_ptr->shading_properties[property].texture_data.texture;

        ogl_sampler_release(bound_sampler);
        ogl_texture_release(bound_texture);

        bound_sampler = NULL;
        bound_texture = NULL;
    }

    material_ptr->dirty                                                  = true;
    material_ptr->shading_properties[property].attachment                = MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE;
    material_ptr->shading_properties[property].texture_data.mag_filter   = mag_filter;
    material_ptr->shading_properties[property].texture_data.min_filter   = min_filter;
    material_ptr->shading_properties[property].texture_data.mipmap_level = mipmap_level;
    material_ptr->shading_properties[property].texture_data.texture      = texture;

    /* Cache the sampler object we will need to use for the sampling process.
     * Pass NULL to all irrelevant arguments - we will use default GL state values
     * for these attributes.
     **/
    GLenum       mag_filter_gl = _mesh_material_get_glenum_for_mesh_material_texture_filtering(mag_filter);
    GLenum       min_filter_gl = _mesh_material_get_glenum_for_mesh_material_texture_filtering(min_filter);
    ogl_samplers samplers      = NULL;

    ogl_context_get_property(material_ptr->context,
                             OGL_CONTEXT_PROPERTY_SAMPLERS,
                            &samplers);

    material_ptr->shading_properties[property].texture_data.sampler = ogl_samplers_get_sampler(samplers,
                                                                                               NULL,  /* border_color */
                                                                                               &mag_filter_gl,
                                                                                               NULL,  /* max_lod_ptr */
                                                                                               &min_filter_gl,
                                                                                               NULL,  /* min_lod_ptr */
                                                                                               NULL,  /* texture_compare_func_ptr */
                                                                                               NULL,  /* texture_compare_mode_ptr */
                                                                                               NULL,  /* wrap_r_ptr */
                                                                                               NULL,  /* wrap_s_ptr */
                                                                                               NULL); /* wrap_t_ptr */

    ogl_sampler_retain(material_ptr->shading_properties[property].texture_data.sampler);
    ogl_texture_retain(texture);
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_vec4(__in __notnull mesh_material                  material,
                                                                   __in           mesh_material_shading_property property,
                                                                   __in_ecount(4) const float*                   data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    material_ptr->dirty                                   = true;
    material_ptr->shading_properties[property].attachment = MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4;

    memcpy(material_ptr->shading_properties[property].vec4_data,
           data,
           sizeof(float) * 4);
}
