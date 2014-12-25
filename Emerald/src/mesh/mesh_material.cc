/**
 *
 * Emerald (kbi/elude @2014)
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
    mesh_material_shading     shading;
    _mesh_material_property   shading_properties[MESH_MATERIAL_SHADING_PROPERTY_COUNT];
    scene_material            source_scene_material;
    system_variant            temp_variant_float;
    ogl_uber                  uber;

    system_hashed_ansi_string uv_map_name; /* NULL by default, needs to be manually set */
    float                     vertex_smoothing_angle;

    _mesh_material()
    {
        callback_manager       = NULL;
        context                = NULL;
        dirty                  = true;
        name                   = NULL;
        source_scene_material  = NULL;
        temp_variant_float     = system_variant_create(SYSTEM_VARIANT_FLOAT);
        uber                   = NULL;
        uv_map_name            = NULL;
        vertex_smoothing_angle = 0.0f;
    }

    REFCOUNT_INSERT_VARIABLES
} _mesh_material;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh_material, mesh_material, _mesh_material);


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
PUBLIC EMERALD_API mesh_material mesh_material_create(__in __notnull system_hashed_ansi_string name,
                                                      __in __notnull ogl_context               context)
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

        new_material->callback_manager = system_callback_manager_create( (_callback_id) MESH_MATERIAL_CALLBACK_ID_COUNT);
        new_material->context          = context;
        new_material->name             = name;
        new_material->uv_map_name      = NULL;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_material,
                                                       _mesh_material_release,
                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Mesh Materials\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
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
        const char* name_strings[] =
        {
            "\\Mesh Materials\\",
            system_hashed_ansi_string_get_buffer(name),
        };
        unsigned int n_name_strings = sizeof(name_strings) / sizeof(name_strings[0]);

        /* Copy relevant fields */
        new_material_ptr->dirty   = src_material_ptr->dirty;
        new_material_ptr->name    = src_material_ptr->name;
        new_material_ptr->shading = src_material_ptr->shading;
        new_material_ptr->uber    = src_material_ptr->uber;

        memcpy(new_material_ptr->shading_properties,
               src_material_ptr->shading_properties,
               sizeof(src_material_ptr->shading_properties) );

        /* Retain assets that are reference-counted */
        for (unsigned int n_shading_properties = 0;
                          n_shading_properties < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                        ++n_shading_properties)
        {
            _mesh_material_property& shading_property = new_material_ptr->shading_properties[n_shading_properties];

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
                                                       system_hashed_ansi_string_create_by_merging_strings(n_name_strings,
                                                                                                           name_strings) );
    }

    return (mesh_material) new_material_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create_from_scene_material(__in     __notnull scene_material src_material,
                                                                          __in_opt           ogl_context    context)
{
    /* Create a new mesh_material instance */
    ogl_textures              context_textures  = NULL;
    mesh_material             result_material   = NULL;
    system_hashed_ansi_string src_material_name = NULL;

    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_NAME,
                               &src_material_name);

    result_material = mesh_material_create(src_material_name,
                                           context);

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

    /* Throw an assertion failures for textures that are not yet supported on mesh_material
     * back-end.
     */
    ASSERT_DEBUG_SYNC(luminance_texture_file_name == NULL                                    ||
                      system_hashed_ansi_string_get_length(luminance_texture_file_name) == 0,
                      "mesh_material does not support luminance textures");

    ASSERT_DEBUG_SYNC(normal_texture_file_name == NULL                                       ||
                      system_hashed_ansi_string_get_length(normal_texture_file_name) == 0,
                      "mesh_material does not support normal textures");

    ASSERT_DEBUG_SYNC(reflection_texture_file_name == NULL                                   ||
                      system_hashed_ansi_string_get_length(reflection_texture_file_name) == 0,
                      "mesh_material does not support reflection textures");

    ASSERT_DEBUG_SYNC(specular_texture_file_name == NULL                                     ||
                      system_hashed_ansi_string_get_length(specular_texture_file_name) == 0,
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
                        ASSERT_ALWAYS_SYNC(false,
                                           "Texture [%s] unavailable in the rendering context",
                                           system_hashed_ansi_string_get_buffer(color_texture_file_name) );
                    }
                    else
                    {
                        mesh_material_set_shading_property_to_texture(result_material,
                                                                      config.shading_property,
                                                                      texture,
                                                                      0, /* mipmap_level */
                                                                      config.texture_mag_filter,
                                                                      config.texture_min_filter);
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
PUBLIC EMERALD_API system_hashed_ansi_string mesh_material_get_name(__in __notnull mesh_material material)
{
    return ((_mesh_material*) material)->name;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_uber mesh_material_get_ogl_uber(__in     __notnull mesh_material material,
                                                       __in_opt           scene         scene)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    if (material_ptr->dirty)
    {
        ogl_materials materials = NULL;

        ogl_context_get_property(material_ptr->context,
                                 OGL_CONTEXT_PROPERTY_MATERIALS,
                                &materials);

        LOG_INFO("Material is dirty - baking..");

        if (material_ptr->uber == NULL)
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
        }

        material_ptr->dirty = false;
        material_ptr->uber  = ogl_materials_get_uber(materials,
                                                     material,
                                                     scene);
    }

    return material_ptr->uber;
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
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_vec3(__in                     __notnull mesh_material                  material,
                                                                                      __in                               mesh_material_shading_property property,
                                                                                      __in                               system_timeline_time           time,
                                                                                      __out_ecount_full_opt(3)           float*                         out_vec3_value)
{
    _mesh_material*          material_ptr = ((_mesh_material*) material);
    _mesh_material_property* property_ptr = material_ptr->shading_properties + property;

    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
                      "Requested shading property is not using a curve container float attachment");

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

#if 0

TODO: REMOVE

/* Please see header for specification */
PUBLIC mesh_material mesh_material_load(__in __notnull system_file_serializer serializer,
                                        __in __notnull ogl_context            context,
                                        __in __notnull system_hash64map       texture_id_to_ogl_texture_map)
{
    system_hashed_ansi_string material_name = NULL;
    mesh_material             new_material  = NULL;
    bool                      result        = true;

    result &= system_file_serializer_read_hashed_ansi_string(serializer,
                                                            &material_name);

    if (!result)
    {
        result = false;

        goto end_error;
    }

    /* Create the material instance */
    new_material = mesh_material_create(material_name,
                                        context);

    ASSERT_ALWAYS_SYNC(new_material != NULL,
                       "Out of memory");

    if (new_material == NULL)
    {
        goto end_error;
    }

    /* Set the VSA */
    float vertex_smoothing_angle = 0.0f;

    result &= system_file_serializer_read(serializer,
                                          sizeof(vertex_smoothing_angle),
                                         &vertex_smoothing_angle);

    mesh_material_set_property(new_material,
                               MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                              &vertex_smoothing_angle);

    /* Set the material shading type */
    mesh_material_shading shading_type = MESH_MATERIAL_SHADING_UNKNOWN;

    result &= system_file_serializer_read(serializer,
                                          sizeof(shading_type),
                                         &shading_type);

    if (!result)
    {
        goto end_error;
    }

    mesh_material_set_property(new_material,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type);

    /* Iterate over all properties */
    for (mesh_material_shading_property property = MESH_MATERIAL_SHADING_PROPERTY_FIRST;
                                        property < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                                ++(int&)property)
    {
        curve_container                        attachment_curve_container_data[3] = {NULL};
        float                                  attachment_float_data;
        mesh_material_input_fragment_attribute attachment_input_fragment_attribute_data;
        _mesh_material_property_texture        attachment_texture_data;
        mesh_material_property_attachment      attachment_type = MESH_MATERIAL_PROPERTY_ATTACHMENT_UNKNOWN;
        float                                  attachment_vec4_data[4];

        /* Read attachment type */
        result &= system_file_serializer_read(serializer,
                                              sizeof(attachment_type),
                                             &attachment_type);
        if (!result)
        {
            goto end_error;
        }

        /* Read attachment properties */
        switch (attachment_type)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
            {
                /* Nothing to serialize */
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
            {
                const unsigned int n_components = (attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT) ? 1 : 3;

                for (unsigned int n_component = 0;
                                  n_component < n_components;
                                ++n_component)
                {
                    result &= system_file_serializer_read_curve_container(serializer,
                                                                          attachment_curve_container_data + n_component);
                }

                if (!result)
                {
                    goto end_error;
                }

                if (n_components == 1)
                {
                    mesh_material_set_shading_property_to_curve_container_float(new_material,
                                                                                property,
                                                                                attachment_curve_container_data[0]);
                }
                else
                {
                    mesh_material_set_shading_property_to_curve_container_vec3(new_material,
                                                                                property,
                                                                                attachment_curve_container_data);
                }

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
            {
                result &= system_file_serializer_read(serializer,
                                                      sizeof(attachment_float_data),
                                                     &attachment_float_data);

                if (!result)
                {
                    goto end_error;
                }

                mesh_material_set_shading_property_to_float(new_material,
                                                            property,
                                                            attachment_float_data);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE:
            {
                result &= system_file_serializer_read(serializer,
                                                      sizeof(attachment_input_fragment_attribute_data),
                                                     &attachment_input_fragment_attribute_data);

                if (!result)
                {
                    goto end_error;
                }

                mesh_material_set_shading_property_to_input_fragment_attribute(new_material,
                                                                               property,
                                                                               attachment_input_fragment_attribute_data);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                GLuint      texture_gl_id    = 0;
                ogl_texture texture_instance = NULL;

                /* Retrieve ID of the texture and find the ogl_texture instance corresponding
                 * to that identifier.
                 */
                result &= system_file_serializer_read(serializer,
                                                       sizeof(texture_gl_id),
                                                      &texture_gl_id);

                if (!system_hash64map_get(texture_id_to_ogl_texture_map,
                                          (system_hash64) texture_gl_id,
                                         &texture_instance) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not find a ogl_texture instance corresponding to texture GL ID");

                    goto end_error;
                }

                /* Retrieve texture properties */
                result &= system_file_serializer_read(serializer,
                                                      sizeof(attachment_texture_data.mag_filter),
                                                     &attachment_texture_data.mag_filter);
                result &= system_file_serializer_read(serializer,
                                                      sizeof(attachment_texture_data.min_filter),
                                                     &attachment_texture_data.min_filter);
                result &= system_file_serializer_read(serializer,
                                                      sizeof(attachment_texture_data.mipmap_level),
                                                     &attachment_texture_data.mipmap_level);

                if (!result)
                {
                    goto end_error;
                }

                mesh_material_set_shading_property_to_texture(new_material,
                                                              property,
                                                              texture_instance,
                                                              attachment_texture_data.mipmap_level,
                                                              attachment_texture_data.mag_filter,
                                                              attachment_texture_data.min_filter);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
            {
                result &= system_file_serializer_read(serializer,
                                                      sizeof(attachment_vec4_data),
                                                      attachment_vec4_data);

                if (!result)
                {
                    goto end_error;
                }

                mesh_material_set_shading_property_to_vec4(new_material,
                                                           property,
                                                           attachment_vec4_data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized mesh material shading property attachment");

                goto end_error;
            }
        } /* switch (attachment_type) */
    }

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false, "Material serialization failed");

    if (new_material != NULL)
    {
        mesh_material_release(new_material);

        new_material = NULL;
    }

end:
    return new_material;
}
#endif

#if 0

TODO: REMOVE

/* Please see header for specification */
PUBLIC bool mesh_material_save(__in __notnull system_file_serializer serializer,
                               __in __notnull mesh_material          material)
{
    _mesh_material* material_ptr = (_mesh_material*) material;
    bool            result       = true;

    /* Store general properties */
    result &= system_file_serializer_write_hashed_ansi_string(serializer,
                                                              material_ptr->name);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(material_ptr->vertex_smoothing_angle),
                                                             &material_ptr->vertex_smoothing_angle);
    result &= system_file_serializer_write                   (serializer,
                                                              sizeof(material_ptr->shading),
                                                             &material_ptr->shading);

    /* Iterate over all properties */
    for (mesh_material_shading_property property = MESH_MATERIAL_SHADING_PROPERTY_FIRST;
                                        property < MESH_MATERIAL_SHADING_PROPERTY_COUNT;
                                ++(int&)property)
    {
        const _mesh_material_property& property_data = material_ptr->shading_properties[property];

        /* Store attachment type */
        result &= system_file_serializer_write(serializer,
                                               sizeof(property_data.attachment),
                                              &property_data.attachment);

        /* Each attachment type needs to be serialized differently */
        switch (property_data.attachment)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
            {
                /* Nothing to serialize */
                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
            {
                const unsigned int n_components = (property_data.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT) ? 1 : 3;

                for (unsigned int n_component = 0;
                                  n_component < n_components;
                                ++n_component)
                {
                    result &= system_file_serializer_write_curve_container(serializer,
                                                                           property_data.curve_container_data[n_component]);
                }

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
            {
                result &= system_file_serializer_write(serializer,
                                                       sizeof(property_data.float_data),
                                                      &property_data.float_data);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE:
            {
                result &= system_file_serializer_write(serializer,
                                                       sizeof(property_data.input_fragment_attribute_data),
                                                      &property_data.input_fragment_attribute_data);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                GLuint texture_id = NULL;

                ogl_texture_get_property(property_data.texture_data.texture,
                                         OGL_TEXTURE_PROPERTY_ID,
                                        &texture_id);

                ASSERT_DEBUG_SYNC(texture_id != 0,
                                  "Texture ID is 0");

                /* Serialize the properties */
                result &= system_file_serializer_write(serializer,
                                                       sizeof(texture_id),
                                                      &texture_id);
                result &= system_file_serializer_write(serializer,
                                                       sizeof(property_data.texture_data.mag_filter),
                                                      &property_data.texture_data.mag_filter);
                result &= system_file_serializer_write(serializer,
                                                       sizeof(property_data.texture_data.min_filter),
                                                      &property_data.texture_data.min_filter);
                result &= system_file_serializer_write(serializer,
                                                       sizeof(property_data.texture_data.mipmap_level),
                                                      &property_data.texture_data.mipmap_level);

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
            {
                result &= system_file_serializer_write(serializer,
                                                       sizeof(property_data.vec4_data),
                                                      &property_data.vec4_data);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized material property attachment type");
            }
        } /* switch (property_data.attachment) */
    } /* for (all shading properties) */

    return result;
}
#endif

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_property(__in __notnull mesh_material          material,
                                                   __in           mesh_material_property property,
                                                   __in __notnull const void*            data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    switch (property)
    {
        case MESH_MATERIAL_PROPERTY_SHADING:
        {
            material_ptr->dirty   = true;
            material_ptr->shading = *(mesh_material_shading*) data;

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

    material_ptr->dirty                                                = true;
    material_ptr->shading_properties[property].attachment              = MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT;
    material_ptr->shading_properties[property].curve_container_data[0] = data;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_vec3(__in           __notnull mesh_material                  material,
                                                                                   __in                     mesh_material_shading_property property,
                                                                                   __in_ecount(3)           curve_container*               data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    material_ptr->dirty                                                = true;
    material_ptr->shading_properties[property].attachment              = MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3;
    material_ptr->shading_properties[property].curve_container_data[0] = data[0];
    material_ptr->shading_properties[property].curve_container_data[1] = data[1];
    material_ptr->shading_properties[property].curve_container_data[2] = data[2];
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_float(__in __notnull mesh_material                  material,
                                                                    __in           mesh_material_shading_property property,
                                                                    __in           float                          data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

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
