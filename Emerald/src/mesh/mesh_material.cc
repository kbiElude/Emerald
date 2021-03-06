/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "demo/demo_app.h"
#include "demo/demo_materials.h"
#include "mesh/mesh_material.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_light.h"
#include "scene/scene_material.h"
#include "scene_renderer/scene_renderer_uber.h"
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
    struct _mesh_material_property* parent_property_ptr;
    ral_sampler                     sampler;
    ral_texture_view                texture_view;

    _mesh_material_property_texture(_mesh_material_property* in_parent_property_ptr)
        :parent_property_ptr(in_parent_property_ptr)
    {
        mag_filter   = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
        min_filter   = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
        sampler      = nullptr;
        texture_view = nullptr;
    }

    ~_mesh_material_property_texture();
} _mesh_material_property_texture;

/** TODO */
typedef struct _mesh_material_property
{
    mesh_material_property_attachment attachment;
    ral_context                       context;

    curve_container                        curve_container_data[3];
    mesh_material_data_vector              data_vector_data;
    float                                  float_data;
    mesh_material_input_fragment_attribute input_fragment_attribute_data;
    _mesh_material_property_texture        texture_data;
    float                                  vec4_data[4];

    _mesh_material_property()
        :texture_data(this)
    {
        attachment                    = MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE;
        context                       = nullptr; /* configured by _mesh_material */
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

typedef enum
{
    MESH_MATERIAL_SHADER_STAGE_FRAGMENT,
    MESH_MATERIAL_SHADER_STAGE_GEOMETRY,
    MESH_MATERIAL_SHADER_STAGE_TESSELLATION_CONTROL,
    MESH_MATERIAL_SHADER_STAGE_TESSELLATION_EVALUATION,
    MESH_MATERIAL_SHADER_STAGE_VERTEX,

    /* Always last */
    MESH_MATERIAL_SHADER_STAGE_COUNT
} _mesh_material_shader_stage;

/** TODO */
typedef struct _mesh_material
{
    system_callback_manager   callback_manager;
    ral_context               context;
    bool                      dirty;
    system_hashed_ansi_string name;
    system_hashed_ansi_string object_manager_path;
    scene                     owner_scene;
    mesh_material_type        type;

    /* MESH_MATERIAL_TYPE_GENERAL-specific properties */
    mesh_material_shading     shading;
    _mesh_material_property   shading_properties[MESH_MATERIAL_SHADING_PROPERTY_COUNT];
    scene_material            source_scene_material; /* only used if source == MESH_MATERIAL_SOURCE_OGL_UBER */
    system_variant            temp_variant_float;
    scene_renderer_uber       uber_non_sm;            /*         uses shadow maps for per-light visibility calculation */
    scene_renderer_uber       uber_sm;                /* does not use shadow maps for per-light visibility calculation */
    system_hashed_ansi_string uv_map_name;            /* nullptr by default, needs to be manually set */
    float                     vertex_smoothing_angle;

    /* MESH_MATERIAL_TYPE_SHADER_BODIES-specific properties */
    ral_program program;
    ral_shader  shaders[MESH_MATERIAL_SHADER_STAGE_COUNT];

    _mesh_material(ral_context in_context)
    {
        ASSERT_DEBUG_SYNC(in_context != nullptr,
                          "Input RAL context is nullptr");

        callback_manager       = nullptr;
        context                = in_context;
        dirty                  = true;
        name                   = nullptr;
        object_manager_path    = nullptr;
        owner_scene            = nullptr;
        program                = nullptr;
        shading                = MESH_MATERIAL_SHADING_PHONG;
        source_scene_material  = nullptr;
        temp_variant_float     = system_variant_create(SYSTEM_VARIANT_FLOAT);
        type                   = MESH_MATERIAL_TYPE_UNDEFINED;
        uber_non_sm            = nullptr;
        uber_sm                = nullptr;
        uv_map_name            = nullptr;
        vertex_smoothing_angle = 0.0f;

        memset(shaders,
               0,
               sizeof(shaders) );

        for (uint32_t n_shading_property = 0;
                      n_shading_property < sizeof(shading_properties) / sizeof(shading_properties[0]);
                    ++n_shading_property)
        {
            shading_properties[n_shading_property].context = in_context;
        }
    }

    REFCOUNT_INSERT_VARIABLES
} _mesh_material;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh_material,
                               mesh_material,
                              _mesh_material);


/** Forward declarations */
PRIVATE void _mesh_material_subscribe_for_notifications(_mesh_material* material_ptr,
                                                        bool            should_subscribe);


/** TODO */
_mesh_material_property_texture::~_mesh_material_property_texture()
{
    if (sampler != nullptr)
    {
        ral_texture parent_texture = nullptr;

        ral_texture_view_get_property(texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &parent_texture);

        ral_context_delete_objects(parent_property_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&sampler) );
        ral_context_delete_objects(parent_property_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&parent_texture) );

        sampler      = nullptr;
        texture_view = nullptr;
    }
}

/** TODO */
PRIVATE void _mesh_material_deinit_shading_property_attachment(_mesh_material_property& material_property)
{
    if (material_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT ||
        material_property.attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3)
    {
        for (unsigned int n_component = 0;
                          n_component < sizeof(material_property.curve_container_data) / sizeof(material_property.curve_container_data[0]);
                        ++n_component)
        {
            if (material_property.curve_container_data[n_component] != nullptr)
            {
                curve_container_release(material_property.curve_container_data[n_component]);

                material_property.curve_container_data[n_component] = nullptr;
            }
        }
    }
}


/** TODO */
PRIVATE void _mesh_material_on_material_invalidation_needed(const void* not_used,
                                                            void*       material)
{
    ASSERT_DEBUG_SYNC( reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                       "Redundant callback detected.");

    /* We will need a new ogl_uber instance for the re-configured scene. */
    reinterpret_cast<_mesh_material*>(material)->dirty = true;
}

/** TODO */
PRIVATE void _mesh_material_on_scene_about_to_be_deleted(const void* scene_to_be_dealloced,
                                                         void*       material)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    _mesh_material_subscribe_for_notifications(material_ptr,
                                               false); /* should_subscribe */

    material_ptr->owner_scene = nullptr;
}

/** TODO */
PRIVATE void _mesh_material_release(void* data_ptr)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(data_ptr);

    /* Release any textures that may have been attached to the material */
    LOG_INFO("Releasing material [%s]",
             system_hashed_ansi_string_get_buffer(material_ptr->name) );

    if (material_ptr->type == MESH_MATERIAL_TYPE_GENERAL)
    {
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

                        material_ptr->shading_properties[current_property].curve_container_data[n_component] = nullptr;
                    }

                    break;
                }

                case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                {
                    ral_texture parent_texture         = nullptr;
                    ral_context parent_texture_context = nullptr;

                    ral_texture_view_get_property(material_ptr->shading_properties[current_property].texture_data.texture_view,
                                                  RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                                 &parent_texture_context);
                    ral_texture_view_get_property(material_ptr->shading_properties[current_property].texture_data.texture_view,
                                                  RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                                 &parent_texture);

                    ral_context_delete_objects(material_ptr->context,
                                               RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                               1, /* n_objects */
                                               reinterpret_cast<void* const*>(&material_ptr->shading_properties[current_property].texture_data.sampler) );
                    ral_context_delete_objects(parent_texture_context,
                                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                               1, /* n_objects */
                                               reinterpret_cast<void* const*>(&parent_texture) );

                    material_ptr->shading_properties[current_property].texture_data.sampler      = nullptr;
                    material_ptr->shading_properties[current_property].texture_data.texture_view = nullptr;

                    break;
                }
            }
        }

        if (material_ptr->temp_variant_float != nullptr)
        {
            system_variant_release(material_ptr->temp_variant_float);

            material_ptr->temp_variant_float = nullptr;
        }
    }

    if (material_ptr->type == MESH_MATERIAL_TYPE_PROGRAM)
    {
        for (unsigned int n_shader_stage = 0;
                          n_shader_stage < (unsigned int) MESH_MATERIAL_SHADER_STAGE_COUNT;
                        ++n_shader_stage)
        {
            if (material_ptr->shaders[n_shader_stage] != nullptr)
            {
                ral_context_delete_objects(material_ptr->context,
                                           RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                           1, /* n_objects */
                                           reinterpret_cast<void* const*>(&material_ptr->shaders[n_shader_stage]) );

                material_ptr->shaders[n_shader_stage] = nullptr;
            }
        }

        if (material_ptr->program != nullptr)
        {
            ral_context_delete_objects(material_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&material_ptr->program) );

            material_ptr->program = nullptr;
        }
    }

    /* Unsubscribe from notifications */
    _mesh_material_subscribe_for_notifications(material_ptr,
                                               false /* should_subscribe */);

    /* Release callback manager */
    if (material_ptr->callback_manager != nullptr)
    {
        system_callback_manager_release(material_ptr->callback_manager);

        material_ptr->callback_manager = nullptr;
    }
}

/** TODO */
PRIVATE void _mesh_material_subscribe_for_notifications(_mesh_material* material_ptr,
                                                        bool            should_subscribe)
{
    unsigned int            n_scene_lights         = 0;
    system_callback_manager scene_callback_manager = nullptr;

    if (material_ptr->owner_scene == nullptr)
    {
        goto end;
    }

    scene_get_property(material_ptr->owner_scene,
                       SCENE_PROPERTY_CALLBACK_MANAGER,
                      &scene_callback_manager);
    scene_get_property(material_ptr->owner_scene,
                       SCENE_PROPERTY_N_LIGHTS,
                      &n_scene_lights);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                        SCENE_CALLBACK_ID_ABOUT_TO_BE_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _mesh_material_on_scene_about_to_be_deleted,
                                                        material_ptr);
        system_callback_manager_subscribe_for_callbacks(scene_callback_manager,
                                                        SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _mesh_material_on_material_invalidation_needed,
                                                        material_ptr);

        for (unsigned int n_scene_light = 0;
                          n_scene_light < n_scene_lights;
                        ++n_scene_light)
        {
            scene_light             current_light                  = scene_get_light_by_index(material_ptr->owner_scene,
                                                                                              n_scene_light);
            system_callback_manager current_light_callback_manager = nullptr;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_CALLBACK_MANAGER,
                                    &current_light_callback_manager);

            system_callback_manager_subscribe_for_callbacks(current_light_callback_manager,
                                                            SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_ALGORITHM_CHANGED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _mesh_material_on_material_invalidation_needed,
                                                            material_ptr);
            system_callback_manager_subscribe_for_callbacks(current_light_callback_manager,
                                                            SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_BIAS_CHANGED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _mesh_material_on_material_invalidation_needed,
                                                            material_ptr);
            system_callback_manager_subscribe_for_callbacks(current_light_callback_manager,
                                                            SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_FILTERING_CHANGED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _mesh_material_on_material_invalidation_needed,
                                                            material_ptr);
            system_callback_manager_subscribe_for_callbacks(current_light_callback_manager,
                                                            SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_POINTLIGHT_ALGORITHM_CHANGED,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _mesh_material_on_material_invalidation_needed,
                                                            material_ptr);
        }
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(scene_callback_manager,
                                                           SCENE_CALLBACK_ID_ABOUT_TO_BE_DELETED,
                                                           _mesh_material_on_scene_about_to_be_deleted,
                                                           material_ptr,
                                                           true); /* allowed_deferred_execution */
        system_callback_manager_unsubscribe_from_callbacks(scene_callback_manager,
                                                           SCENE_CALLBACK_ID_LIGHT_ADDED,
                                                           _mesh_material_on_material_invalidation_needed,
                                                           material_ptr,
                                                           true); /* allowed_deferred_execution */

        for (unsigned int n_scene_light = 0;
                          n_scene_light < n_scene_lights;
                        ++n_scene_light)
        {
            scene_light             current_light                  = scene_get_light_by_index(material_ptr->owner_scene,
                                                                                              n_scene_light);
            system_callback_manager current_light_callback_manager = nullptr;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_CALLBACK_MANAGER,
                                    &current_light_callback_manager);

            system_callback_manager_unsubscribe_from_callbacks(current_light_callback_manager,
                                                               SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_ALGORITHM_CHANGED,
                                                               _mesh_material_on_material_invalidation_needed,
                                                               material_ptr,
                                                               true); /* allowed_deferred_execution */
            system_callback_manager_unsubscribe_from_callbacks(current_light_callback_manager,
                                                               SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_BIAS_CHANGED,
                                                               _mesh_material_on_material_invalidation_needed,
                                                               material_ptr,
                                                               true); /* allowed_deferred_execution */
            system_callback_manager_unsubscribe_from_callbacks(current_light_callback_manager,
                                                               SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_FILTERING_CHANGED,
                                                               _mesh_material_on_material_invalidation_needed,
                                                               material_ptr,
                                                               true); /* allowed_deferred_execution */
            system_callback_manager_unsubscribe_from_callbacks(current_light_callback_manager,
                                                               SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_POINTLIGHT_ALGORITHM_CHANGED,
                                                               _mesh_material_on_material_invalidation_needed,
                                                               material_ptr,
                                                               true); /* allowed_deferred_execution */
        }
    }

end:
    ;
}

/** TODO */
PRIVATE void _mesh_material_get_ral_enums_for_mesh_material_texture_filtering(mesh_material_texture_filtering filtering,
                                                                              ral_texture_filter*             out_texture_filter_ptr,
                                                                              ral_texture_mipmap_mode*        out_texture_mipmap_mode_ptr)
{
    GLenum result = GL_NONE;

    switch (filtering)
    {
        case MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR:
        {
            *out_texture_filter_ptr      = RAL_TEXTURE_FILTER_LINEAR;
            *out_texture_mipmap_mode_ptr = RAL_TEXTURE_MIPMAP_MODE_LINEAR;

            break;
        }

        case MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_NEAREST:
        {
            *out_texture_filter_ptr      = RAL_TEXTURE_FILTER_LINEAR;
            *out_texture_mipmap_mode_ptr = RAL_TEXTURE_MIPMAP_MODE_NEAREST;

            break;
        }

        case MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_LINEAR:
        {
            *out_texture_filter_ptr      = RAL_TEXTURE_FILTER_NEAREST;
            *out_texture_mipmap_mode_ptr = RAL_TEXTURE_MIPMAP_MODE_LINEAR;

            break;
        }

        case MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_NEAREST:
        {
            *out_texture_filter_ptr      = RAL_TEXTURE_FILTER_NEAREST;
            *out_texture_mipmap_mode_ptr = RAL_TEXTURE_MIPMAP_MODE_NEAREST;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized material texture filtering requested [%d]",
                              filtering);
        }
    }
}


/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create(system_hashed_ansi_string name,
                                                      ral_context               context,
                                                      system_hashed_ansi_string object_manager_path)
{
    _mesh_material* new_material_ptr = new (std::nothrow) _mesh_material(context);

    LOG_INFO("Creating material [%s]",
             system_hashed_ansi_string_get_buffer(name) );

    ASSERT_ALWAYS_SYNC(new_material_ptr != nullptr,
                       "Out of memory");

    if (new_material_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(name != nullptr,
                          "Name is nullptr");

        new_material_ptr->callback_manager    = system_callback_manager_create( (_callback_id) MESH_MATERIAL_CALLBACK_ID_COUNT);
        new_material_ptr->context             = context;
        new_material_ptr->name                = name;
        new_material_ptr->object_manager_path = object_manager_path;
        new_material_ptr->type                = MESH_MATERIAL_TYPE_GENERAL;
        new_material_ptr->uv_map_name         = nullptr;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_material_ptr,
                                                       _mesh_material_release,
                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                       GET_OBJECT_PATH(name,
                                                                       OBJECT_TYPE_MESH_MATERIAL,
                                                                       object_manager_path) );
    }

    return (mesh_material) new_material_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create_copy(system_hashed_ansi_string name,
                                                           mesh_material             src_material)
{
    _mesh_material* src_material_ptr = reinterpret_cast<_mesh_material*>(src_material);
    _mesh_material* new_material_ptr = new (std::nothrow) _mesh_material(src_material_ptr->context);

    LOG_INFO("Creating material copy [%s]",
             system_hashed_ansi_string_get_buffer(name) );

    ASSERT_ALWAYS_SYNC(new_material_ptr != nullptr,
                       "Out of memory");

    if (new_material_ptr != nullptr)
    {
        /* Copy relevant fields */
        new_material_ptr->dirty               = src_material_ptr->dirty;
        new_material_ptr->name                = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(src_material_ptr->name),
                                                                                                        " copy");
        new_material_ptr->object_manager_path = src_material_ptr->object_manager_path;
        new_material_ptr->type                = src_material_ptr->type;

        if (new_material_ptr->type == MESH_MATERIAL_TYPE_GENERAL)
        {
            new_material_ptr->shading     = src_material_ptr->shading;
            new_material_ptr->uber_non_sm = src_material_ptr->uber_non_sm;
            new_material_ptr->uber_sm     = src_material_ptr->uber_sm;

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
                    ral_texture parent_texture         = nullptr;
                    ral_context parent_texture_context = nullptr;

                    ral_texture_view_get_property(shading_property.texture_data.texture_view,
                                                  RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                                 &parent_texture_context);
                    ral_texture_view_get_property(shading_property.texture_data.texture_view,
                                                  RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                                 &parent_texture);

                    ral_context_retain_object(src_material_ptr->context,
                                              RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                              shading_property.texture_data.sampler);
                    ral_context_retain_object(parent_texture_context,
                                              RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                              parent_texture);
                }
            }
        }
        else
        if (new_material_ptr->type == MESH_MATERIAL_TYPE_PROGRAM)
        {
            new_material_ptr->shading = MESH_MATERIAL_SHADING_NONE;

            for (unsigned int n_shader_stage = 0;
                              n_shader_stage < MESH_MATERIAL_SHADER_STAGE_COUNT;
                            ++n_shader_stage)
            {
                if (src_material_ptr->shaders[n_shader_stage] != nullptr)
                {
                    new_material_ptr->shaders[n_shader_stage] = src_material_ptr->shaders[n_shader_stage];

                    ral_context_retain_object(new_material_ptr->context,
                                              RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                              new_material_ptr->shaders[n_shader_stage]);
                }
            }

            if (src_material_ptr->program != nullptr)
            {
                new_material_ptr->program = src_material_ptr->program;

                ral_context_retain_object(new_material_ptr->context,
                                          RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                          new_material_ptr->program);
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material instance type");
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
PUBLIC EMERALD_API mesh_material mesh_material_create_from_scene_material(scene_material src_material,
                                                                          ral_context    context_ral)
{
    /* Create a new mesh_material instance */
    curve_container*                color_curves_ptr                 = nullptr;
    system_hashed_ansi_string       color_texture_file_name          = nullptr;
    mesh_material_texture_filtering color_texture_mag_filter         = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
    mesh_material_texture_filtering color_texture_min_filter         = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;
    curve_container                 glosiness                        = nullptr;
    curve_container                 luminance                        = nullptr;
    system_hashed_ansi_string       luminance_texture_file_name      = nullptr;
    system_hashed_ansi_string       normal_texture_file_name         = nullptr;
    system_hashed_ansi_string       reflection_texture_file_name     = nullptr;
    mesh_material                   result_material                  = nullptr;
    const mesh_material_shading     shading_type                     = MESH_MATERIAL_SHADING_PHONG;
    float                           smoothing_angle                  = 0.0f;
    curve_container                 specular_ratio                   = nullptr;
    system_hashed_ansi_string       specular_texture_file_name       = nullptr;
    system_hashed_ansi_string       src_material_object_manager_path = nullptr;
    system_hashed_ansi_string       src_material_name                = nullptr;

    typedef struct _attachment_configuration
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

        _attachment_configuration()
        {
        }

        _attachment_configuration(mesh_material_shading_property    in_shading_property,
                                  mesh_material_property_attachment in_property_attachment,
                                  curve_container*                  in_curve_data,
                                  system_hashed_ansi_string         in_texture_filename,
                                  mesh_material_texture_filtering   in_texture_mag_filter,
                                  mesh_material_texture_filtering   in_texture_min_filter)
        {
            curve_data          = in_curve_data;
            property_attachment = in_property_attachment;
            shading_property    = in_shading_property;
            texture_filename    = in_texture_filename;
            texture_mag_filter  = in_texture_mag_filter;
            texture_min_filter  = in_texture_min_filter;
        }

        _attachment_configuration(mesh_material_shading_property    in_shading_property,
                                  mesh_material_property_attachment in_property_attachment,
                                  curve_container*                  in_curve_data)
        {
            curve_data          = in_curve_data;
            property_attachment = in_property_attachment;
            shading_property    = in_shading_property;
        }

    } _attachment_configuration;

          _attachment_configuration attachment_configs[4]; /* diffuse, glosiness, luminance, specular attachments */
    const unsigned int              n_attachment_configs = sizeof(attachment_configs) /
                                                           sizeof(attachment_configs[0]);


    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_NAME,
                               &src_material_name);
    scene_material_get_property(src_material,
                                SCENE_MATERIAL_PROPERTY_OBJECT_MANAGER_PATH,
                               &src_material_object_manager_path);

    result_material = mesh_material_create(src_material_name,
                                           context_ral,
                                           src_material_object_manager_path);

    ASSERT_DEBUG_SYNC(result_material != nullptr,
                      "mesh_material_create() failed.");

    if (result_material == nullptr)
    {
        goto end;
    }

    /* Update source material */
    reinterpret_cast<_mesh_material*>(result_material)->source_scene_material = src_material;

    /* Force Phong reflection model */
    mesh_material_set_property(result_material,
                               MESH_MATERIAL_PROPERTY_SHADING,
                              &shading_type);

    /* Extract values required to set up the attachment_configs array */
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
                               &color_curves_ptr);
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

    if (color_texture_file_name                                       != nullptr &&
        system_hashed_ansi_string_get_length(color_texture_file_name) == 0)
    {
        color_texture_file_name = nullptr;
    }

    if (luminance_texture_file_name                                       != nullptr &&
        system_hashed_ansi_string_get_length(luminance_texture_file_name) == 0)
    {
        luminance_texture_file_name = nullptr;
    }

    if (normal_texture_file_name                                       != nullptr &&
        system_hashed_ansi_string_get_length(normal_texture_file_name) == 0)
    {
        normal_texture_file_name = nullptr;
    }

    if (reflection_texture_file_name                                       != nullptr &&
        system_hashed_ansi_string_get_length(reflection_texture_file_name) == 0)
    {
        reflection_texture_file_name = nullptr;
    }

    if (specular_texture_file_name                                       != nullptr &&
        system_hashed_ansi_string_get_length(specular_texture_file_name) == 0)
    {
        specular_texture_file_name = nullptr;
    }

    /* Throw an assertion failures for textures that are not yet supported on mesh_material
     * back-end.
     */
    ASSERT_DEBUG_SYNC(luminance_texture_file_name == nullptr,
                      "mesh_material does not support luminance textures");

    ASSERT_DEBUG_SYNC(normal_texture_file_name == nullptr,
                      "mesh_material does not support normal textures");

    ASSERT_DEBUG_SYNC(reflection_texture_file_name == nullptr,
                      "mesh_material does not support reflection textures");

    ASSERT_DEBUG_SYNC(specular_texture_file_name == nullptr,
                      "mesh_material does not support specular textures");

    /* Configure texture/float/vec3 attachments (as described by scene_material) */

    /* 1. Diffuse attachment */
    attachment_configs[0] = _attachment_configuration(
        MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
        (color_texture_file_name != nullptr) ? MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE
                                          : MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3,

        (color_texture_file_name != nullptr) ? nullptr : color_curves_ptr,

        color_texture_file_name,
        color_texture_mag_filter,
        color_texture_min_filter
    );

    /* 2. Glosiness attachment */
    attachment_configs[1] = _attachment_configuration(
        MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
        MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
        &glosiness
    );

    /* 3. Luminance attachment */
    attachment_configs[2] = _attachment_configuration(
        MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
        MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
        &luminance
    );

    /* 4. Specular attachment */
    attachment_configs[3] = _attachment_configuration(
        MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
        MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
       &specular_ratio
    );

    for (unsigned int n_attachment_config = 0;
                      n_attachment_config < n_attachment_configs;
                    ++n_attachment_config)
    {
        const _attachment_configuration& config = attachment_configs[n_attachment_config];

        switch (config.property_attachment)
        {
            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
            {
                ASSERT_DEBUG_SYNC(config.curve_data[0] != nullptr,
                                  "nullptr curve container");

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
                ral_texture texture = nullptr;

                /* Do not attach anything if context_ral is nullptr.
                 *
                 * This won't backfire if the mesh_material is an intermediate object used
                 * for mesh baking. Otherwise context_ral should never be nullptr.
                 */
                if (context_ral == nullptr)
                {
                    break;
                }

                texture = ral_context_get_texture_by_file_name(context_ral,
                                                               config.texture_filename);

                if (texture == nullptr)
                {
                    ral_context_create_textures_from_file_names(context_ral,
                                                                1, /* n_file_names */
                                                               &config.texture_filename,
                                                               &texture);
                }

                ASSERT_ALWAYS_SYNC(texture != nullptr,
                                   "Texture [%s] unavailable in the rendering context",
                                   system_hashed_ansi_string_get_buffer(config.texture_filename) );

                if (texture != nullptr)
                {
                    ral_texture_view             texture_view             = nullptr;
                    ral_texture_view_create_info texture_view_create_info = ral_texture_view_create_info(texture);

                    texture_view = ral_texture_get_view(&texture_view_create_info);

                    mesh_material_set_shading_property_to_texture_view(result_material,
                                                                       config.shading_property,
                                                                       texture_view,
                                                                       config.texture_mag_filter,
                                                                       config.texture_min_filter);

                    /* Generate mip-maps if needed */
                    ral_texture_generate_mipmaps(texture,
                                                 true /* async */);
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized property attachment type");
            }
        }
    }

    /* Configure vertex smoothing angle */
    mesh_material_set_property(result_material,
                               MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                              &smoothing_angle);

end:
    return result_material;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material mesh_material_create_from_shader_bodies(system_hashed_ansi_string name,
                                                                         ral_context               context,
                                                                         system_hashed_ansi_string object_manager_path,
                                                                         system_hashed_ansi_string fs_body,
                                                                         system_hashed_ansi_string gs_body,
                                                                         system_hashed_ansi_string tc_body,
                                                                         system_hashed_ansi_string te_body,
                                                                         system_hashed_ansi_string vs_body)
{
    mesh_material   result_material     = nullptr;
    _mesh_material* result_material_ptr = nullptr;
    ral_program     result_program      = nullptr;
    ral_shader      temp_shader         = nullptr;

    struct _body
    {
        system_hashed_ansi_string body;
        const char*               suffix;
        ral_shader_type           type;
    } bodies[] =
    {
        {fs_body, " FS", RAL_SHADER_TYPE_FRAGMENT},
        {gs_body, " GS", RAL_SHADER_TYPE_GEOMETRY},
        {tc_body, " TC", RAL_SHADER_TYPE_TESSELLATION_CONTROL},
        {te_body, " TE", RAL_SHADER_TYPE_TESSELLATION_EVALUATION},
        {vs_body, " VS", RAL_SHADER_TYPE_VERTEX}
    };
    const unsigned int n_bodies = sizeof(bodies) / sizeof(bodies[0]);

    const ral_program_create_info result_po_create_info =
    {
        ((fs_body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT        : 0) |
        ((gs_body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_GEOMETRY        : 0) |
        ((tc_body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_TESS_CONTROL    : 0) |
        ((te_body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_TESS_EVALUATION : 0) |
        ((vs_body != nullptr) ? RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX          : 0),
        name
    };

    /* Create a new mesh_material instance */
    result_material = mesh_material_create(name,
                                           context,
                                           object_manager_path);

    ASSERT_DEBUG_SYNC(result_material != nullptr,
                      "mesh_material_create() failed.");

    if (result_material == nullptr)
    {
        goto end;
    }

    /* Update the mesh_material instance by changing its type and setting up the program object */
    result_material_ptr = reinterpret_cast<_mesh_material*>(result_material);

    result_material_ptr->type = MESH_MATERIAL_TYPE_PROGRAM;

    if (!ral_context_create_programs(context,
                                     1, /* n_create_info_items */
                                    &result_po_create_info,
                                    &result_program))
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_program_create() call failed.");

        goto end_error;
    }

    for (unsigned int n_body = 0;
                      n_body < n_bodies;
                    ++n_body)
    {
        if (bodies[n_body].body != nullptr)
        {
            const ral_shader_create_info shader_create_info =
            {
                system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                        bodies[n_body].suffix),
                bodies[n_body].type
            };

            if (!ral_context_create_shaders(context,
                                            1, /* n_create_info_items */
                                           &shader_create_info,
                                           &temp_shader) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not create a RAL shader.");

                goto end_error;
            }

            ral_shader_set_property(temp_shader,
                                    RAL_SHADER_PROPERTY_GLSL_BODY,
                                    &bodies[n_body].body);

            /* So far so good! */
            if (!ral_program_attach_shader(result_program,
                                           temp_shader,
                                           true /* async */) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "ogl_program_attach_shader() call failed.");

                goto end_error;
            }

            result_material_ptr->shaders[n_body] = temp_shader;
        }
    }

    result_material_ptr->program = result_program;

    /* All done */
    goto end;

end_error:
    /* Wind down any shader object we may have created */
    for (unsigned int n_shader = 0;
                      n_shader < MESH_MATERIAL_SHADER_STAGE_COUNT;
                    ++n_shader)
    {
        if (result_material_ptr->shaders[n_shader] != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&result_material_ptr->shaders[n_shader]) );

            result_material_ptr->shaders[n_shader] = nullptr;
        }
    }

    if (temp_shader != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&temp_shader) );

        temp_shader = nullptr;
    }

    /* Do the same for the result program object */
    if (result_material_ptr->program != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&result_material_ptr->program) );

        result_material_ptr->program = nullptr;
    }

end:
    return result_material;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_property_attachment_has(mesh_material_property_attachment attachment)
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
    }

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_has(mesh_material_shading shading)
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
    }

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_type_has(mesh_material_type type)
{
    system_hashed_ansi_string result = system_hashed_ansi_string_get_default_empty_string();

    static const char* type_general_name = "General";
    static const char* type_program_name = "Program-based";

    switch (type)
    {
        case MESH_MATERIAL_TYPE_GENERAL:
        {
            result = system_hashed_ansi_string_create(type_general_name);

            break;
        }

        case MESH_MATERIAL_TYPE_PROGRAM:
        {
            result = system_hashed_ansi_string_create(type_program_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_type value.");
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_property_has(mesh_material_shading_property property)
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
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API scene_renderer_uber mesh_material_get_uber(mesh_material material,
                                                              scene         scene,
                                                              bool          use_shadow_maps)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    if (material_ptr->dirty)
    {
        demo_materials materials = nullptr;

        demo_app_get_property(DEMO_APP_PROPERTY_MATERIALS,
                             &materials);

        LOG_INFO("Material is dirty - baking..");

        if (material_ptr->owner_scene == nullptr &&
            material_ptr->type        == MESH_MATERIAL_TYPE_GENERAL)
        {
            /* The material needs to be marked as dirty every time a light is added, or
             * when a significant scene_light property is changed */
            material_ptr->owner_scene = scene;

            _mesh_material_subscribe_for_notifications(material_ptr,
                                                       true /* should_subscribe */);
        }

        material_ptr->dirty = false;

        if (material_ptr->type == MESH_MATERIAL_TYPE_GENERAL)
        {
            material_ptr->uber_sm     = demo_materials_get_uber(materials,
                                                                material,
                                                                scene,
                                                                true); /* use_shadow_maps */

            material_ptr->uber_non_sm = demo_materials_get_uber(materials,
                                                                material,
                                                                scene,
                                                                false); /* use_shadow_maps */
        }
        else
        {
            /* SM does not affect ogl_program-driven materials */
            material_ptr->uber_sm = demo_materials_get_uber(materials,
                                                            material,
                                                            scene,
                                                            true); /* use any value */

            material_ptr->uber_non_sm = material_ptr->uber_sm;
        }

        /* Call back any subscribers & inform them about the event */
        system_callback_manager_call_back(material_ptr->callback_manager,
                                          MESH_MATERIAL_CALLBACK_ID_UBER_UPDATED,
                                          material_ptr);
    }

    return use_shadow_maps ? material_ptr->uber_sm
                           : material_ptr->uber_non_sm;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_property(const mesh_material    material,
                                                   mesh_material_property property,
                                                   void*                  out_result_ptr)
{
    const _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    switch (property)
    {
        case MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = material_ptr->callback_manager;

            break;
        }

        case MESH_MATERIAL_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = material_ptr->context;

            break;
        }

        case MESH_MATERIAL_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = material_ptr->name;

            break;
        }

        case MESH_MATERIAL_PROPERTY_SHADING:
        {
            *reinterpret_cast<mesh_material_shading*>(out_result_ptr) = material_ptr->shading;

            break;
        }

        case MESH_MATERIAL_PROPERTY_SOURCE_RAL_PROGRAM:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_PROGRAM,
                              "MESH_MATERIAL_PROPERTY_SOURCE_OGL_PROGRAM query is invalid for non-program mesh_material instances.");
            ASSERT_DEBUG_SYNC(material_ptr->program != nullptr,
                              "mesh_material instance's RAL program is nullptr.");

            *reinterpret_cast<ral_program*>(out_result_ptr) = material_ptr->program;

            break;
        }

        case MESH_MATERIAL_PROPERTY_SOURCE_SCENE_MATERIAL:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                              "MESH_MATERIAL_PROPERTY_SOURCE_SCENE_MATERIAL query is invalid for customized mesh_material instances.");
            ASSERT_DEBUG_SYNC(material_ptr->source_scene_material != nullptr,
                              "Source scene_material is being queried for but none is set for the queried mesh_material instance");

            *reinterpret_cast<scene_material*>(out_result_ptr) = material_ptr->source_scene_material;

            break;
        }

        case MESH_MATERIAL_PROPERTY_TYPE:
        {
            *reinterpret_cast<mesh_material_type*>(out_result_ptr) = material_ptr->type;

            break;
        }

        case MESH_MATERIAL_PROPERTY_UV_MAP_NAME:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                              "MESH_MATERIAL_PROPERTY_UV_MAP_NAME query is invalid for customized mesh_material instances.");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = material_ptr->uv_map_name;

            break;
        }

        case MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                              "MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE query is invalid for customized mesh_material instances.");

            *reinterpret_cast<float*>(out_result_ptr) = material_ptr->vertex_smoothing_angle;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_material_property value");
        }
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_material_property_attachment mesh_material_get_shading_property_attachment_type(mesh_material                  material,
                                                                                                        mesh_material_shading_property property)
{
    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_attachment_type() call is invalid for customized mesh_material instances.");

    return reinterpret_cast<_mesh_material*>(material)->shading_properties[property].attachment;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_float(mesh_material                  material,
                                                                                       mesh_material_shading_property property,
                                                                                       system_time                    time,
                                                                                       float*                         out_float_value_ptr)
{
    _mesh_material*          material_ptr = reinterpret_cast<_mesh_material*>(material);
    _mesh_material_property* property_ptr = material_ptr->shading_properties + property;

    ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_value_curve_container_float() call is invalid for customized mesh_material instances.");
    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
                      "Requested shading property is not using a curve container float attachment");

    curve_container_get_value(property_ptr->curve_container_data[0],
                              time,
                              false, /* should_force */
                              material_ptr->temp_variant_float);
    system_variant_get_float (material_ptr->temp_variant_float,
                              out_float_value_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_vec3(mesh_material                  material,
                                                                                      mesh_material_shading_property property,
                                                                                      system_time                    time,
                                                                                      float*                         out_vec3_value_ptr)
{
    _mesh_material*          material_ptr = reinterpret_cast<_mesh_material*>(material);
    _mesh_material_property* property_ptr = material_ptr->shading_properties + property;

    ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_value_curve_container_vec3() call is invalid for customized mesh_material instances.");
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
                                  out_vec3_value_ptr + n_component);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_float(mesh_material                  material,
                                                                       mesh_material_shading_property property,
                                                                       float*                         out_float_value_ptr)
{
    _mesh_material_property* property_ptr = reinterpret_cast<_mesh_material*>(material)->shading_properties + property;

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_value_float() call is invalid for customized mesh_material instances.");
    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT,
                      "Requested shading property is not using a float attachment");

    *out_float_value_ptr = property_ptr->float_data;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_input_fragment_attribute(mesh_material                           material,
                                                                                          mesh_material_shading_property          property,
                                                                                          mesh_material_input_fragment_attribute* out_attribute_ptr)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_value_input_fragment_attribute() call is invalid for customized mesh_material instances.");
    ASSERT_DEBUG_SYNC(property == MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,
                      "Invalid property requested");
    ASSERT_DEBUG_SYNC(material_ptr->shading_properties[property].attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE,
                      "Requested shading property does not have an attribute attachment");

    /* Return the requested value */
    if (out_attribute_ptr != nullptr)
    {
        *out_attribute_ptr = material_ptr->shading_properties[property].input_fragment_attribute_data;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_texture_view(mesh_material                    material,
                                                                              mesh_material_shading_property   property,
                                                                              ral_texture_view*                out_texture_view_ptr,
                                                                              ral_sampler*                     out_sampler_ptr)
{
    _mesh_material_property*         property_ptr     = reinterpret_cast<_mesh_material*>(material)->shading_properties + property;
    _mesh_material_property_texture* texture_data_ptr = &property_ptr->texture_data;

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_value_texture() call is invalid for customized mesh_material instances.");
    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE,
                      "Requested shading property is not using a texture attachment");

    if (out_texture_view_ptr != nullptr)
    {
        *out_texture_view_ptr = texture_data_ptr->texture_view;
    }

    if (out_sampler_ptr != nullptr)
    {
        *out_sampler_ptr = texture_data_ptr->sampler;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_vec4(mesh_material                  material,
                                                                      mesh_material_shading_property property,
                                                                      float*                         out_vec4_data_ptr)
{
    _mesh_material_property* property_ptr = reinterpret_cast<_mesh_material*>(material)->shading_properties + property;

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_get_shading_property_value_vec4() call is invalid for customized mesh_material instances.");
    ASSERT_DEBUG_SYNC(property_ptr->attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4,
                      "Requested shading property is not using a vec4 attachment");

    memcpy(out_vec4_data_ptr,
           property_ptr->vec4_data,
           sizeof(float) * 4);
}

/* Please see header for specification */
PUBLIC bool mesh_material_is_a_match_to_mesh_material(mesh_material material_a,
                                                      mesh_material material_b)
{
    mesh_material_shading material_a_shading = MESH_MATERIAL_SHADING_UNKNOWN;
    mesh_material_type    material_a_type    = reinterpret_cast<_mesh_material*>(material_a)->type;
    mesh_material_shading material_b_shading = MESH_MATERIAL_SHADING_UNKNOWN;
    mesh_material_type    material_b_type    = reinterpret_cast<_mesh_material*>(material_b)->type;
    bool                  result             = false;

    ASSERT_DEBUG_SYNC(material_a != nullptr &&
                      material_b != nullptr,
                      "One or both input material arguments are nullptr.");

    /* 1. Type */
    if (material_a_type != material_b_type)
    {
        goto end;
    }

    /* We only check the properties that affect how the shaders are built */
    /* 2. Shading */
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

    /* If this is a 'no shading' material and we've reached this point, we can
     * safely assume both materials are a match. */
    if (material_a_shading == MESH_MATERIAL_SHADING_NONE)
    {
        result = true;

        goto end;
    }

    /* 4. Shading properties */
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

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE:
            {
                mesh_material_input_fragment_attribute material_a_props;
                mesh_material_input_fragment_attribute material_b_props;

                mesh_material_get_shading_property_value_input_fragment_attribute(material_a,
                                                                                  property,
                                                                                 &material_a_props);
                mesh_material_get_shading_property_value_input_fragment_attribute(material_b,
                                                                                  property,
                                                                                 &material_b_props);

                if (material_a_props != material_b_props)
                {
                    goto end;
                }

                break;
            }

            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
            {
                ral_texture_type material_a_texture_type;
                ral_texture_view material_a_texture_view = nullptr;
                ral_texture_type material_b_texture_type;
                ral_texture_view material_b_texture_view = nullptr;

                mesh_material_get_shading_property_value_texture_view(material_a,
                                                                      property,
                                                                     &material_a_texture_view,
                                                                      nullptr);/* out_sampler_ptr - irrelevant */

                mesh_material_get_shading_property_value_texture_view(material_b,
                                                                      property,
                                                                     &material_b_texture_view,
                                                                      nullptr);/* out_sampler_ptr - irrelevant */

                if (material_a_texture_view == nullptr && material_b_texture_view != nullptr ||
                    material_a_texture_view != nullptr && material_b_texture_view == nullptr)
                {
                    goto end;
                }

                if (material_a_texture_view != nullptr &&
                    material_b_texture_view != nullptr)
                {
                    ral_texture_view_get_property(material_a_texture_view,
                                                  RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                                 &material_a_texture_type);
                    ral_texture_view_get_property(material_b_texture_view,
                                                  RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                                 &material_b_texture_type);

                    if (material_a_texture_type != material_b_texture_type)
                    {
                        goto end;
                    }
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
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized material property attachment [%d]",
                                   material_a_attachment);
            }
        }
    }

    /* Done */
    result = true;
end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_property(mesh_material          material,
                                                   mesh_material_property property,
                                                   const void*            data)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    switch (property)
    {
        case MESH_MATERIAL_PROPERTY_SHADING:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                              "MESH_MATERIAL_PROPERTY_SHADING property can only be set for general mesh_material instances.");

            material_ptr->shading = *reinterpret_cast<const mesh_material_shading*>(data);
            material_ptr->dirty   = true;

            break;
        }

        case MESH_MATERIAL_PROPERTY_UV_MAP_NAME:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                              "MESH_MATERIAL_PROPERTY_UV_MAP_NAME property can only be set for general mesh_material instances.");

            material_ptr->uv_map_name = *reinterpret_cast<const system_hashed_ansi_string*>(data);

            break;
        }

        case MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE:
        {
            ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                              "MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE property can only be set for general mesh_material instances.");

            material_ptr->vertex_smoothing_angle = *reinterpret_cast<const float*>(data);

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
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_float(mesh_material                  material,
                                                                                    mesh_material_shading_property property,
                                                                                    curve_container                data)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_set_shading_property_to_curve_container_float() call is invalid for customized mesh_material instances.");

    _mesh_material_deinit_shading_property_attachment(material_ptr->shading_properties[property]);

    material_ptr->dirty                                                = true;
    material_ptr->shading_properties[property].attachment              = MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT;
    material_ptr->shading_properties[property].curve_container_data[0] = data;

    if (data != nullptr)
    {
        curve_container_retain(data);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_vec3(mesh_material                  material,
                                                                                   mesh_material_shading_property property,
                                                                                   curve_container*               data)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_set_shading_property_to_curve_container_vec3() call is invalid for customized mesh_material instances.");

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
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_float(mesh_material                  material,
                                                                    mesh_material_shading_property property,
                                                                    float                          data)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_set_shading_property_to_float() call is invalid for customized mesh_material instances.");

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
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_input_fragment_attribute(mesh_material                          material,
                                                                                       mesh_material_shading_property         property,
                                                                                       mesh_material_input_fragment_attribute attribute)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_set_shading_property_to_input_fragment_attribute() call is invalid for customized mesh_material instances.");
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
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_texture_view(mesh_material                   material,
                                                                           mesh_material_shading_property  property,
                                                                           ral_texture_view                texture_view,
                                                                           mesh_material_texture_filtering mag_filter,
                                                                           mesh_material_texture_filtering min_filter)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    ASSERT_DEBUG_SYNC(reinterpret_cast<_mesh_material*>(material)->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_set_shading_property_to_texture() call is invalid for customized mesh_material instances.");
    ASSERT_DEBUG_SYNC(texture_view != nullptr,
                      "Bound texture view is nullptr");

    if (material_ptr->shading_properties[property].attachment == MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE)
    {
        ral_sampler&      bound_sampler                     = material_ptr->shading_properties[property].texture_data.sampler;
        ral_texture_view& bound_texture_view                = material_ptr->shading_properties[property].texture_data.texture_view;
        ral_texture       bound_texture_view_parent_texture = nullptr;

        ral_texture_view_get_property(bound_texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &bound_texture_view_parent_texture);

        ral_context_delete_objects(material_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                   1, /* n_samplers */
                                   reinterpret_cast<void* const*>(&bound_sampler) );
        ral_context_delete_objects(material_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_samplers */
                                   reinterpret_cast<void* const*>(&bound_texture_view_parent_texture) );

        bound_sampler      = nullptr;
        bound_texture_view = nullptr;
    }

    material_ptr->dirty                                                  = true;
    material_ptr->shading_properties[property].attachment                = MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE;
    material_ptr->shading_properties[property].texture_data.mag_filter   = mag_filter;
    material_ptr->shading_properties[property].texture_data.min_filter   = min_filter;
    material_ptr->shading_properties[property].texture_data.texture_view = texture_view;

    {
        ral_texture parent_texture         = nullptr;
        ral_context parent_texture_context = nullptr;

        ral_texture_view_get_property(texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                     &parent_texture_context);
        ral_texture_view_get_property(texture_view,
                                      RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                     &parent_texture);

        ral_context_retain_object(parent_texture_context,
                                  RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                  parent_texture);
    }

    /* Cache the sampler object we will need to use for the sampling process.
     * Pass nullptr to all irrelevant arguments - we will use default state values
     * for these attributes.
     **/
    ral_texture_filter      mag_filter_ral;
    ral_texture_mipmap_mode mag_mipmap_mode_ral;
    ral_texture_filter      min_filter_ral;
    ral_texture_mipmap_mode min_mipmap_mode_ral;
    ral_sampler_create_info sampler_create_info;

    _mesh_material_get_ral_enums_for_mesh_material_texture_filtering(mag_filter,
                                                                    &mag_filter_ral,
                                                                    &mag_mipmap_mode_ral);
    _mesh_material_get_ral_enums_for_mesh_material_texture_filtering(min_filter,
                                                                    &min_filter_ral,
                                                                    &min_mipmap_mode_ral);

    sampler_create_info.mag_filter  = mag_filter_ral;
    sampler_create_info.min_filter  = min_filter_ral;
    sampler_create_info.mipmap_mode = min_mipmap_mode_ral;

    ral_context_create_samplers(material_ptr->context,
                                1, /* n_create_info_items */
                               &sampler_create_info,
                               &material_ptr->shading_properties[property].texture_data.sampler);
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_vec4(mesh_material                  material,
                                                                   mesh_material_shading_property property,
                                                                   const float*                   data)
{
    _mesh_material* material_ptr = reinterpret_cast<_mesh_material*>(material);

    ASSERT_DEBUG_SYNC(material_ptr->type == MESH_MATERIAL_TYPE_GENERAL,
                      "mesh_material_set_shading_property_to_vec4() call is invalid for customized mesh_material instances.");

    material_ptr->dirty                                   = true;
    material_ptr->shading_properties[property].attachment = MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4;

    memcpy(material_ptr->shading_properties[property].vec4_data,
           data,
           sizeof(float) * 4);
}
