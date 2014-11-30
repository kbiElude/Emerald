/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_sampler.h"
#include "ogl/ogl_samplers.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_uber.h"
#include "system/system_callback_manager.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"

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
    ogl_uber                  uber;

    system_hashed_ansi_string uv_map_name; /* NULL by default, needs to be manually set */
    float                     vertex_smoothing_angle;

    _mesh_material()
    {
        callback_manager       = NULL;
        context                = NULL;
        dirty                  = true;
        name                   = NULL;
        uber                   = NULL;
        uv_map_name            = NULL;
        vertex_smoothing_angle = 0.0f;
    }

    REFCOUNT_INSERT_VARIABLES
} _mesh_material;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh_material, mesh_material, _mesh_material);

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
        if (material_ptr->shading_properties[current_property].attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE)
        {
            ogl_sampler_release(material_ptr->shading_properties[current_property].texture_data.sampler);
            ogl_texture_release(material_ptr->shading_properties[current_property].texture_data.texture);

            material_ptr->shading_properties[current_property].texture_data.sampler = NULL;
            material_ptr->shading_properties[current_property].texture_data.texture = NULL;
        }
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
        mesh_material_property_attachment      attachment_type = MESH_MATERIAL_PROPERTY_ATTACHMENT_UNKNOWN;
        mesh_material_input_fragment_attribute attachment_input_fragment_attribute_data;
        _mesh_material_property_texture        attachment_texture_data;
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

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_property(__in __notnull mesh_material          material,
                                                   __in           mesh_material_property property,
                                                   __in __notnull void*                  data)
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
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_float(__in __notnull mesh_material                  material,
                                                                    __in           mesh_material_shading_property property,
                                                                    __in           float                          data)
{
    _mesh_material* material_ptr = (_mesh_material*) material;

    if (property == MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY)
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
