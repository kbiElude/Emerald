/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_sampler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shader_constructor.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_math_srgb.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_variant.h"
#include <sstream>

#define FRAGMENT_SHADER_PROPERTIES_UBO_BINDING_ID (0)
#define VERTEX_SHADER_PROPERTIES_UBO_BINDING_ID   (1)


/** Internal type definitions */

/* Holds all properties that may be used for a fragment shader item */
typedef struct _ogl_uber_fragment_shader_item
{
    float ambient_color_linear[3]; /* used by renderer  */
    float ambient_color_sRGB  [3]; /* provided by users */
    bool  ambient_color_dirty;
    GLint ambient_color_ub_offset;

    float current_light_attenuations[3];
    bool  current_light_attenuations_dirty;
    GLint current_light_attenuations_ub_offset;

    float current_light_diffuse_linear[4]; /* used by renderer */
    float current_light_diffuse_sRGB  [4]; /* provided by users */
    bool  current_light_diffuse_dirty;
    GLint current_light_diffuse_ub_offset;

    float current_light_direction[3];
    bool  current_light_direction_dirty;
    GLint current_light_direction_ub_offset;

    float current_light_location[4];
    bool  current_light_location_dirty;
    GLint current_light_location_ub_offset;

    ogl_texture current_light_shadow_map_texture;
    GLuint      current_light_shadow_map_texture_sampler_location;

    _ogl_uber_fragment_shader_item()
    {
        ambient_color_ub_offset                           = -1;
        current_light_attenuations_ub_offset              = -1;
        current_light_diffuse_ub_offset                   = -1;
        current_light_direction_ub_offset                 = -1;
        current_light_location_ub_offset                  = -1;
        current_light_shadow_map_texture_sampler_location = -1;

        current_light_shadow_map_texture = NULL;

        ambient_color_dirty              = true;
        current_light_attenuations_dirty = true;
        current_light_diffuse_dirty      = true;
        current_light_direction_dirty    = true;
        current_light_location_dirty     = true;
    }
} _ogl_uber_fragment_shader_item;

/* Holds all properties that may be used for a vertex shader item */
typedef struct _ogl_uber_vertex_shader_item
{
    float current_light_depth_vp[16]; /* row-major */
    bool  current_light_depth_vp_dirty;
    GLint current_light_depth_vp_ub_offset;

    _ogl_uber_light_sh_data current_light_sh_data;
    GLint                   current_light_sh_data_ub_offset;

    _ogl_uber_vertex_shader_item()
    {
        current_light_depth_vp_ub_offset = -1;
        current_light_depth_vp_dirty     = true;
    }

} _ogl_uber_vertex_shader_item;

/* A single item added to ogl_uber used to construct a program object managed by ogl_uber */
typedef struct _ogl_uber_item
{
    scene_light_falloff            falloff;
    shaders_fragment_uber_item_id  fs_item_id;
    _ogl_uber_fragment_shader_item fragment_shader_item;
    bool                           is_shadow_caster;
    scene_light_shadow_map_bias    shadow_map_bias;
    _ogl_uber_vertex_shader_item   vertex_shader_item;
    shaders_vertex_uber_item_id    vs_item_id;

    _ogl_uber_item_type type;

    _ogl_uber_item()
    {
        falloff          = SCENE_LIGHT_FALLOFF_UNKNOWN;
        fs_item_id       = -1;
        is_shadow_caster = false;
        shadow_map_bias  = SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN;
        type             = OGL_UBER_ITEM_UNKNOWN;
        vs_item_id       = -1;
    }
} _ogl_uber_item;

/* A single item that defines a VAO configured for a particular mesh and current
 * ogl_uber instance.
 */
typedef struct __ogl_uber_vao
{
    system_timeline_time mesh_modification_timestamp;
    GLuint               vao_id;

    __ogl_uber_vao()
    {
        mesh_modification_timestamp = 0;
        vao_id                      = 0;
    }
} _ogl_uber_vao;

typedef struct
{
    ogl_context               context;
    system_hashed_ansi_string name;

    ogl_program               program;
    shaders_fragment_uber     shader_fragment;
    uint32_t                  shader_fragment_n_items;
    shaders_vertex_uber       shader_vertex;

    uint32_t                  ambient_material_sampler_uniform_location;
    uint32_t                  ambient_material_vec4_uniform_location;
    uint32_t                  diffuse_material_sampler_uniform_location;
    uint32_t                  diffuse_material_vec4_uniform_location;
    uint32_t                  glosiness_uniform_location;
    uint32_t                  luminosity_material_sampler_uniform_location;
    uint32_t                  luminosity_material_float_uniform_location;
    uint32_t                  mesh_sh3_uniform_location;
    uint32_t                  mesh_sh3_data_offset_uniform_location;
    uint32_t                  mesh_sh4_uniform_location;
    uint32_t                  mesh_sh4_data_offset_uniform_location;
    uint32_t                  model_uniform_location;         /* column major ordering! */
    uint32_t                  normal_matrix_uniform_location; /* column-major ordering! */
    uint32_t                  object_normal_attribute_location;
    uint32_t                  object_uv_attribute_location;
    uint32_t                  object_vertex_attribute_location;
    uint32_t                  shininess_material_sampler_uniform_location;
    uint32_t                  shininess_material_float_uniform_location;
    uint32_t                  specular_material_sampler_uniform_location;
    uint32_t                  specular_material_float_uniform_location;

    /* These are stored in UBs so we need to store UB offsets instead of locations */
    uint32_t                     vp_ub_offset;
    uint32_t                     world_camera_ub_offset;

    void*                        bo_data;
    uint32_t                     bo_data_size;
    uint32_t                     bo_data_vertex_offset;
    GLuint                       bo_id;
    bool                         is_rendering;
    uint32_t                     n_texture_units_assigned;
    ogl_program_uniform_block_id ub_fs_id;
    ogl_program_uniform_block_id ub_vs_id;

    float                     current_camera_location[4];
    bool                      current_camera_location_dirty;
    system_matrix4x4          current_vp;
    bool                      current_vp_dirty;

    system_resizable_vector   added_items; /* holds _ogl_uber_item instances */
    bool                      dirty;

    system_matrix4x4          graph_rendering_current_matrix;
    system_variant            variant_float;

    system_hash64map mesh_to_vao_descriptor_map;

    REFCOUNT_INSERT_VARIABLES
} _ogl_uber;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_uber, ogl_uber, _ogl_uber);

/** Forward declarations */
PRIVATE void _ogl_uber_add_item_shaders_fragment_callback_handler(                                 _shaders_fragment_uber_parent_callback_type type,
                                                                  __in_opt                         void*                                       data,
                                                                  __in                             void*                                       uber);
PRIVATE void _ogl_uber_create_renderer_callback                  (                                 ogl_context                                 context,
                                                                                                   void*                                       arg);
PRIVATE void _ogl_uber_release                                   (__in __notnull __deallocate(mem) void*                                       uber);
PRIVATE void _ogl_uber_release_renderer_callback                 (                                 ogl_context                                 context,
                                                                                                   void*                                       arg);
PRIVATE void _ogl_uber_relink_renderer_callback                  (                                 ogl_context                                 context,
                                                                                                   void*                                       arg);

#ifdef _DEBUG
    PRIVATE void _ogl_uber_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_uber_verify_context_type(x)
#endif

/** Internal variables */

/** TODO */
PRIVATE void _ogl_uber_add_item_shaders_fragment_callback_handler(         _shaders_fragment_uber_parent_callback_type type,
                                                                  __in_opt void*                                       data,
                                                                  __in     void*                                       uber)
{
    switch (type)
    {
        case SHADERS_FRAGMENT_UBER_PARENT_CALLBACK_NEW_FRAGMENT_INPUT:
        {
            const _shaders_fragment_uber_new_fragment_input_callback* callback_data = (const _shaders_fragment_uber_new_fragment_input_callback*) data;

            ASSERT_DEBUG_SYNC(callback_data != NULL, "Callback data pointer is NULL");

            /* Fragment shader uses an input attribute. Update the vertex shader so that
             * the data is taken in vertex shader stage and patched through to the fragment
             * shader.
             */
            shaders_vertex_uber_add_passthrough_input_attribute( ((_ogl_uber*) uber)->shader_vertex,
                                                                callback_data->vs_attribute_name,
                                                                callback_data->fs_attribute_type,
                                                                callback_data->fs_attribute_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized callback from shaders_fragment_uber object");
        }
    } /* switch (type) */
}

/** TODO */
PRIVATE void _ogl_uber_bake_mesh_vao(__in __notnull _ogl_uber* uber_ptr,
                                     __in __notnull mesh       mesh)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;
    const ogl_context_gl_limits*                              limits          = NULL;

    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);
    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits);

    /* Create a VAO if one is needed */
    _ogl_uber_vao* vao_ptr = NULL;

    if (!system_hash64map_get(uber_ptr->mesh_to_vao_descriptor_map,
                              (system_hash64) mesh,
                             &vao_ptr) )
    {
        vao_ptr = new (std::nothrow) _ogl_uber_vao;

        ASSERT_ALWAYS_SYNC(vao_ptr != NULL, "Out of memory");
        if (vao_ptr == NULL)
        {
            goto end;
        }

        entrypoints->pGLGenVertexArrays(1, &vao_ptr->vao_id);
    }

    /* Retrieve buffer object storage data. All data streams are stored inside the BO */
    GLint        mesh_bo_id            = 0;
    unsigned int mesh_stride           = 0;
    uint32_t     mesh_texcoords_offset = 0;

    mesh_get_layer_data_stream_property(mesh,
                                        MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &mesh_texcoords_offset);

    mesh_get_property(mesh,
                      MESH_PROPERTY_GL_STRIDE,
                     &mesh_stride);
    mesh_get_property(mesh,
                      MESH_PROPERTY_GL_BO_ID,
                     &mesh_bo_id);

    if (uber_ptr->object_normal_attribute_location != -1)
    {
        uint32_t mesh_normals_offset = 0;

        mesh_get_layer_data_stream_property(mesh,
                                            MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_normals_offset);

        dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                             mesh_bo_id,
                                                             uber_ptr->object_normal_attribute_location,
                                                             3,
                                                             GL_FLOAT,
                                                             GL_FALSE,
                                                             mesh_stride,
                                                             mesh_normals_offset);

        dsa_entrypoints->pGLEnableVertexArrayAttribEXT(vao_ptr->vao_id,
                                                       uber_ptr->object_normal_attribute_location);
    }

    if (uber_ptr->object_vertex_attribute_location != -1)
    {
        uint32_t mesh_vertices_offset = 0;

        mesh_get_layer_data_stream_property(mesh,
                                            MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_vertices_offset);

        dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                             mesh_bo_id,
                                                             uber_ptr->object_vertex_attribute_location,
                                                             3,
                                                             GL_FLOAT,
                                                             GL_FALSE,
                                                             mesh_stride,
                                                             mesh_vertices_offset);

        dsa_entrypoints->pGLEnableVertexArrayAttribEXT(vao_ptr->vao_id,
                                                      uber_ptr->object_vertex_attribute_location);
    }

    /* Bind the BO as a data source for the indexed draw calls */
    entrypoints->pGLBindVertexArray(vao_ptr->vao_id);
    entrypoints->pGLBindBuffer     (GL_ELEMENT_ARRAY_BUFFER,
                                    mesh_bo_id);

    /* Iterate over all layers.. */
    uint32_t n_layers = 0;

    mesh_get_property(mesh,
                      MESH_PROPERTY_N_LAYERS,
                     &n_layers);

    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        const uint32_t n_layer_passes = mesh_get_amount_of_layer_passes(mesh, n_layer);

        /* Set up attribute data */
        struct _attribute_item
        {
            GLint         attribute_location;
            const GLvoid* offset;
            int           size;
        } attribute_items[] =
        {
            {
                uber_ptr->object_uv_attribute_location,
                (const GLvoid*) mesh_texcoords_offset,
                2
            },
        };
        const unsigned int n_attribute_items = sizeof(attribute_items) / sizeof(attribute_items[0]);

        for (unsigned int n_attribute_item = 0;
                          n_attribute_item < n_attribute_items;
                        ++n_attribute_item)
        {
            _attribute_item& item = attribute_items[n_attribute_item];

            if (item.attribute_location != -1)
            {
                dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                                     mesh_bo_id,
                                                                     item.attribute_location,
                                                                     item.size,
                                                                     GL_FLOAT,
                                                                     GL_FALSE,
                                                                     mesh_stride,
                                                                     (GLintptr) item.offset);
                dsa_entrypoints->pGLEnableVertexArrayAttribEXT      (vao_ptr->vao_id,
                                                                     item.attribute_location);
            }
        } /* for (all attribute items) */

        /* Iterate over all layer passes */
        for (uint32_t n_layer_pass = 0; n_layer_pass < n_layer_passes; ++n_layer_pass)
        {
            mesh_material layer_pass_material = NULL;

            /* Retrieve layer pass properties */
            mesh_get_layer_pass_property(mesh,
                                         n_layer,
                                         n_layer_pass,
                                         MESH_LAYER_PROPERTY_MATERIAL,
                                        &layer_pass_material);

            /* Bind shading data for each supported shading property */
            struct _attachment
            {
                mesh_material_shading_property property;
                int32_t                        shader_sampler_uniform_location;
                int32_t                        shader_vec4_location;
                int32_t                        shader_uv_attribute_location;
            } attachments[] =
            {
                {
                    MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                    uber_ptr->ambient_material_sampler_uniform_location,
                    uber_ptr->ambient_material_vec4_uniform_location,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                    uber_ptr->diffuse_material_sampler_uniform_location,
                    uber_ptr->diffuse_material_vec4_uniform_location,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                    uber_ptr->luminosity_material_sampler_uniform_location,
                    uber_ptr->luminosity_material_float_uniform_location,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                    uber_ptr->shininess_material_sampler_uniform_location,
                    uber_ptr->shininess_material_float_uniform_location,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                    uber_ptr->specular_material_sampler_uniform_location,
                    uber_ptr->specular_material_float_uniform_location,
                    uber_ptr->object_uv_attribute_location
                },
            };
            const uint32_t n_attachments = sizeof(attachments) / sizeof(attachments[0]);

            for (uint32_t n_attachment = 0; n_attachment < n_attachments; ++n_attachment)
            {
                const _attachment&                      attachment      = attachments[n_attachment];
                const mesh_material_property_attachment attachment_type = mesh_material_get_shading_property_attachment_type(layer_pass_material,
                                                                                                                             attachment.property);

                switch (attachment_type)
                {
                    case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
                    case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
                    case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
                    case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
                    case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                    {
                        /* Nothing to be done here */
                        break;
                    }

                    case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                    {
                        /* Set up UV attribute data */
                        if (attachment.shader_uv_attribute_location != -1)
                        {
                            dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                                                 mesh_bo_id,
                                                                                 attachment.shader_uv_attribute_location,
                                                                                 2, /* size */
                                                                                 GL_FLOAT,
                                                                                 GL_FALSE,
                                                                                 mesh_stride,
                                                                                 (GLintptr) mesh_texcoords_offset);
                            dsa_entrypoints->pGLEnableVertexArrayAttribEXT      (vao_ptr->vao_id,
                                                                                 attachment.shader_uv_attribute_location);
                        }

                        break;
                    } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE: */

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false, "Unrecognized material property attachment");
                    }
                } /* switch (attachment_type) */
            } /* for (all attachments) */
        } /* for (all mesh layer passes) */
    } /* for (all mesh layers) */

    /* Set the modification timestamp */
    mesh_get_property(mesh,
                      MESH_PROPERTY_TIMESTAMP_MODIFICATION,
                      &vao_ptr->mesh_modification_timestamp);

    /* Add the VAO descriptor to the map */
    if (!system_hash64map_contains(uber_ptr->mesh_to_vao_descriptor_map,
                                   (system_hash64) mesh) )
    {
        system_hash64map_insert(uber_ptr->mesh_to_vao_descriptor_map,
                                (system_hash64) mesh,
                                vao_ptr,
                                NULL,  /* on_remove_callback_proc */
                                NULL); /* on_remove_callback_proc_user_arg */
    }

end:
    ;
}

/** TODO */
PRIVATE void _ogl_uber_create_renderer_callback(ogl_context context,
                                                void*       arg)
{
    _ogl_uber*                                                uber_ptr         = (_ogl_uber*) arg;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Generate buffer object we will use for feeding the data to the uber program */
    entry_points->pGLGenBuffers(1, &uber_ptr->bo_id);
}

/** TODO */
PRIVATE void _ogl_uber_release(__in __notnull __deallocate(mem) void* uber)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    if (uber_ptr != NULL)
    {
        if (uber_ptr->added_items != NULL)
        {
            _ogl_uber_item* item_ptr = NULL;

            while (system_resizable_vector_pop(uber_ptr->added_items,
                                              &item_ptr) )
            {
                delete item_ptr;

                item_ptr = NULL;
            }

            system_resizable_vector_release(uber_ptr->added_items);
            uber_ptr->added_items = NULL;
        }

        if (uber_ptr->current_vp != NULL)
        {
            system_matrix4x4_release(uber_ptr->current_vp);

            uber_ptr->current_vp = NULL;
        }

        if (uber_ptr->graph_rendering_current_matrix != NULL)
        {
            system_matrix4x4_release(uber_ptr->graph_rendering_current_matrix);

            uber_ptr->graph_rendering_current_matrix = NULL;
        }

        if (uber_ptr->shader_fragment != NULL)
        {
            shaders_fragment_uber_release(uber_ptr->shader_fragment);
        }

        if (uber_ptr->shader_vertex != NULL)
        {
            shaders_vertex_uber_release(uber_ptr->shader_vertex);
        }

        if (uber_ptr->program != NULL)
        {
            ogl_program_release(uber_ptr->program);
        }

        if (uber_ptr->bo_data != NULL)
        {
            delete [] uber_ptr->bo_data;

            uber_ptr->bo_data = NULL;
        }

        if (uber_ptr->mesh_to_vao_descriptor_map != NULL)
        {
            ASSERT_DEBUG_SYNC(uber_ptr->context != NULL,
                              "Rendering context is NULL");

            ogl_context_request_callback_from_context_thread(uber_ptr->context,
                                                             _ogl_uber_release_renderer_callback,
                                                             uber_ptr);

            system_hash64  vao_hash = 0;
            _ogl_uber_vao* vao_ptr  = NULL;

            while (system_hash64map_get_element_at(uber_ptr->mesh_to_vao_descriptor_map,
                                                   0,
                                                  &vao_ptr,
                                                  &vao_hash) )
            {
                delete vao_ptr;

                /* Move on */
                system_hash64map_remove(uber_ptr->mesh_to_vao_descriptor_map,
                                        vao_hash);
            }
        }

        if (uber_ptr->variant_float != NULL)
        {
            system_variant_release(uber_ptr->variant_float);

            uber_ptr->variant_float = NULL;
        }
    }
}

/** TODO */
PRIVATE void _ogl_uber_release_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    _ogl_uber*                        uber_ptr    = (_ogl_uber*) arg;
    const unsigned int                n_vaos      = system_hash64map_get_amount_of_elements(uber_ptr->mesh_to_vao_descriptor_map);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    for (unsigned int n_vao = 0;
                      n_vao < n_vaos;
                    ++n_vao)
    {
        _ogl_uber_vao* vao_ptr = NULL;

        if (system_hash64map_get_element_at(uber_ptr->mesh_to_vao_descriptor_map,
                                            n_vao,
                                            &vao_ptr,
                                            NULL) ) /* out_hash */
        {
            if (vao_ptr->vao_id != 0)
            {
                entrypoints->pGLDeleteVertexArrays(1,
                                                  &vao_ptr->vao_id);

                vao_ptr->vao_id = 0;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve VAO descriptor at index [%d]",
                              n_vao);
        }
    } /* for (all VAO descriptors) */
}
/** TODO */
PRIVATE void _ogl_uber_relink_renderer_callback(ogl_context context, void* arg)
{
    _ogl_uber*                                                uber_ptr         = (_ogl_uber*) arg;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    LOG_INFO("Relinking an uber object instance");

    /* Set up UBO bindings */
    uint32_t program_id                             = ogl_program_get_id(uber_ptr->program);
    uint32_t fragment_shader_properties_block_index = entry_points->pGLGetUniformBlockIndex(program_id,
                                                                                            "FragmentShaderProperties");
    uint32_t vertex_shader_properties_block_index   = entry_points->pGLGetUniformBlockIndex(program_id,
                                                                                            "VertexShaderProperties");

    if (fragment_shader_properties_block_index != -1)
    {
        entry_points->pGLUniformBlockBinding(program_id,
                                             fragment_shader_properties_block_index,
                                             FRAGMENT_SHADER_PROPERTIES_UBO_BINDING_ID);
    }
    if (vertex_shader_properties_block_index != -1)
    {
        entry_points->pGLUniformBlockBinding(program_id,
                                             vertex_shader_properties_block_index,
                                             VERTEX_SHADER_PROPERTIES_UBO_BINDING_ID);
    }

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                      "Could not set up uniform block bindings");

    /* Set up storage for buffer object that will be used for feeding the data to the uber program */
    dsa_entry_points->pGLNamedBufferDataEXT(uber_ptr->bo_id,
                                            uber_ptr->bo_data_size,
                                            NULL,
                                            GL_DYNAMIC_DRAW);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                       "Could not generate BO");
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_uber_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_uber is only supported under GL contexts")
    }
#endif


/* Please see header for specification */
PUBLIC EMERALD_API ogl_uber_item_id ogl_uber_add_input_fragment_attribute_item(__in __notnull ogl_uber                           uber,
                                                                               __in __notnull _ogl_uber_input_fragment_attribute input_attribute)
{
    shaders_fragment_uber_input_attribute_type fs_input_attribute = UBER_INPUT_ATTRIBUTE_UNKNOWN;
    shaders_fragment_uber_item_id              fs_item_id         = -1;
    ogl_uber_item_id                           result             = -1;
    _ogl_uber*                                 uber_ptr           = (_ogl_uber*) uber;

    switch (input_attribute)
    {
        case OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL:
        {
            fs_input_attribute = UBER_INPUT_ATTRIBUTE_NORMAL;

            break;
        }

        case OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD:
        {
            fs_input_attribute = UBER_INPUT_ATTRIBUTE_TEXCOORD;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized input attribute");
        }
    } /* switch (input_attribute) */

    /* Update fragment shader instance */
    fs_item_id = shaders_fragment_uber_add_input_attribute_contribution(uber_ptr->shader_fragment,
                                                                        fs_input_attribute,
                                                                        _ogl_uber_add_item_shaders_fragment_callback_handler,
                                                                        uber);

    /* Spawn a new descriptor */
    _ogl_uber_item* new_item_ptr = new (std::nothrow) _ogl_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != NULL, "Out of memory");
    if (new_item_ptr == NULL)
    {
        goto end;
    }

    new_item_ptr->type = OGL_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE;
    result             = system_resizable_vector_get_amount_of_elements(uber_ptr->added_items);

    /* Add the descriptor to the added items vector */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    /* Mark uber instance as dirty */
    uber_ptr->dirty = true;

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_uber_item_id ogl_uber_add_light_item(__in __notnull                        ogl_uber                         uber,
                                                            __in                                  shaders_fragment_uber_light_type light_type,
                                                            __in                                  bool                             is_shadow_caster,
                                                            __in                                  scene_light_shadow_map_bias      shadow_map_bias,
                                                            __in                                  scene_light_falloff              falloff,
                                                            __in __notnull                        unsigned int                     n_light_properties,
                                                            __in_ecount_opt(n_light_properties*2) void*                            light_property_values)
{
    _ogl_uber*       uber_ptr     = (_ogl_uber*) uber;
    _ogl_uber_item*  new_item_ptr = NULL;
    ogl_uber_item_id result       = -1;

    /* Update uber shader instances */
    shaders_fragment_uber_item_id fs_item_id = -1;
    shaders_vertex_uber_item_id   vs_item_id = -1;

    switch (light_type)
    {
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT:
        {
            fs_item_id = shaders_fragment_uber_add_light(uber_ptr->shader_fragment,
                                                         light_type,
                                                         is_shadow_caster,
                                                         shadow_map_bias,
                                                         falloff,
                                                         n_light_properties,
                                                         light_property_values,
                                                         _ogl_uber_add_item_shaders_fragment_callback_handler,
                                                         uber);
            vs_item_id = shaders_vertex_uber_add_light  (uber_ptr->shader_vertex,
                                                         SHADERS_VERTEX_UBER_LIGHT_NONE,
                                                         is_shadow_caster);

            break;
        }

        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE:
        {
            break;
        }

        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH3:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH4:
        {
            shaders_vertex_uber_light vs_light = (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH3) ? SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS :
                                                                                                                   SHADERS_VERTEX_UBER_LIGHT_SH_4_BANDS;

            fs_item_id = shaders_fragment_uber_add_light(uber_ptr->shader_fragment,
                                                         light_type,
                                                         is_shadow_caster,
                                                         shadow_map_bias,
                                                         falloff,
                                                         n_light_properties,
                                                         light_property_values,
                                                         NULL, /* callback proc - not used */
                                                         NULL  /* callback proc user arg - not used */);
            vs_item_id = shaders_vertex_uber_add_light  (uber_ptr->shader_vertex,
                                                         vs_light,
                                                         is_shadow_caster);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized uber light type");
        }
    } /* switch (light_type) */

    /* Spawn the descriptor */
    new_item_ptr = new (std::nothrow) _ogl_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != NULL, "Out of memory");
    if (new_item_ptr == NULL)
    {
        goto end;
    }

    new_item_ptr->fs_item_id       = fs_item_id;
    new_item_ptr->is_shadow_caster = is_shadow_caster;
    new_item_ptr->shadow_map_bias  = shadow_map_bias;
    new_item_ptr->type             = OGL_UBER_ITEM_LIGHT;
    new_item_ptr->vs_item_id       = vs_item_id;
    result                         = system_resizable_vector_get_amount_of_elements(uber_ptr->added_items);

    /* Add the descriptor to the added items vector */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    /* Mark uber instance as dirty */
    uber_ptr->dirty = true;

end:
    return result;
}

/** TODO */
PRIVATE void _ogl_uber_relink(__in __notnull ogl_uber uber)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    /* Sanity check */
    ASSERT_DEBUG_SYNC(uber_ptr->dirty, "_ogl_uber_relink() called for a non-dirty uber object");

    /* Recompile shaders if needed */
    if (shaders_fragment_uber_is_dirty(uber_ptr->shader_fragment) )
    {
        shaders_fragment_uber_recompile(uber_ptr->shader_fragment);
    }

    if (shaders_vertex_uber_is_dirty(uber_ptr->shader_vertex) )
    {
        shaders_vertex_uber_recompile(uber_ptr->shader_vertex);
    }

    /* Link the program object */
    if (!ogl_program_link(uber_ptr->program) )
    {
        ASSERT_ALWAYS_SYNC(false, "Cannot link uber program");
    }

    /* Set default attribute & uniform locations */
    uber_ptr->ambient_material_sampler_uniform_location    = -1;
    uber_ptr->ambient_material_vec4_uniform_location       = -1;
    uber_ptr->diffuse_material_sampler_uniform_location    = -1;
    uber_ptr->diffuse_material_vec4_uniform_location       = -1;
    uber_ptr->glosiness_uniform_location                   = -1;
    uber_ptr->luminosity_material_sampler_uniform_location = -1;
    uber_ptr->luminosity_material_float_uniform_location   = -1;
    uber_ptr->mesh_sh3_uniform_location                    = -1;
    uber_ptr->mesh_sh3_data_offset_uniform_location        = -1;
    uber_ptr->mesh_sh4_uniform_location                    = -1;
    uber_ptr->mesh_sh4_data_offset_uniform_location        = -1;
    uber_ptr->model_uniform_location                       = -1;
    uber_ptr->normal_matrix_uniform_location               = -1;
    uber_ptr->object_normal_attribute_location             = -1;
    uber_ptr->object_uv_attribute_location                 = -1;
    uber_ptr->object_vertex_attribute_location             = -1;
    uber_ptr->shininess_material_sampler_uniform_location  = -1;
    uber_ptr->shininess_material_float_uniform_location    = -1;
    uber_ptr->specular_material_sampler_uniform_location   = -1;
    uber_ptr->specular_material_float_uniform_location     = -1;
    uber_ptr->vp_ub_offset                                 = -1;
    uber_ptr->world_camera_ub_offset                       = -1;

    /* Retrieve attribute locations */
    const ogl_program_attribute_descriptor* object_normal_descriptor = NULL;
    const ogl_program_attribute_descriptor* object_uv_descriptor     = NULL;
    const ogl_program_attribute_descriptor* object_vertex_descriptor = NULL;

    ogl_program_get_attribute_by_name(uber_ptr->program,
                                      system_hashed_ansi_string_create("object_normal"),
                                     &object_normal_descriptor);
    ogl_program_get_attribute_by_name(uber_ptr->program,
                                      system_hashed_ansi_string_create("object_uv"),
                                     &object_uv_descriptor);
    ogl_program_get_attribute_by_name(uber_ptr->program,
                                      system_hashed_ansi_string_create("object_vertex"),
                                     &object_vertex_descriptor);

    if (object_normal_descriptor != NULL)
    {
        uber_ptr->object_normal_attribute_location = object_normal_descriptor->location;
    }

    if (object_uv_descriptor != NULL)
    {
        uber_ptr->object_uv_attribute_location = object_uv_descriptor->location;
    }

    if (object_vertex_descriptor != NULL)
    {
        uber_ptr->object_vertex_attribute_location = object_vertex_descriptor->location;
    }

    /* Retrieve uniform locations */
    const ogl_program_uniform_descriptor* ambient_material_sampler_uniform_descriptor    = NULL;
    const ogl_program_uniform_descriptor* ambient_material_vec4_uniform_descriptor       = NULL;
    const ogl_program_uniform_descriptor* diffuse_material_sampler_uniform_descriptor    = NULL;
    const ogl_program_uniform_descriptor* diffuse_material_vec4_uniform_descriptor       = NULL;
    const ogl_program_uniform_descriptor* emission_material_sampler_uniform_descriptor   = NULL;
    const ogl_program_uniform_descriptor* emission_material_vec4_uniform_descriptor      = NULL;
    const ogl_program_uniform_descriptor* glosiness_uniform_descriptor                   = NULL;
    const ogl_program_uniform_descriptor* luminosity_material_sampler_uniform_descriptor = NULL;
    const ogl_program_uniform_descriptor* luminosity_material_float_uniform_descriptor   = NULL;
    const ogl_program_uniform_descriptor* mesh_sh3_uniform_descriptor                    = NULL;
    const ogl_program_uniform_descriptor* mesh_sh3_data_offset_uniform_descriptor        = NULL;
    const ogl_program_uniform_descriptor* mesh_sh4_uniform_descriptor                    = NULL;
    const ogl_program_uniform_descriptor* mesh_sh4_data_offset_uniform_descriptor        = NULL;
    const ogl_program_uniform_descriptor* model_uniform_descriptor                       = NULL;
    const ogl_program_uniform_descriptor* normal_matrix_uniform_descriptor               = NULL;
    const ogl_program_uniform_descriptor* shininess_material_sampler_uniform_descriptor  = NULL;
    const ogl_program_uniform_descriptor* shininess_material_float_uniform_descriptor    = NULL;
    const ogl_program_uniform_descriptor* specular_material_sampler_uniform_descriptor   = NULL;
    const ogl_program_uniform_descriptor* specular_material_float_uniform_descriptor     = NULL;
    const ogl_program_uniform_descriptor* vp_uniform_descriptor                          = NULL;
    const ogl_program_uniform_descriptor* world_camera_uniform_descriptor                = NULL;

    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("ambient_material_sampler"),
                                   &ambient_material_sampler_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("ambient_material"),
                                   &ambient_material_vec4_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("diffuse_material_sampler"),
                                   &diffuse_material_sampler_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("diffuse_material"),
                                   &diffuse_material_vec4_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("emission_material_sampler"),
                                   &emission_material_sampler_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("emission_material"),
                                   &emission_material_vec4_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("glosiness"),
                                   &glosiness_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("luminosity_material_sampler"),
                                   &luminosity_material_sampler_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("luminosity_material"),
                                   &luminosity_material_float_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("mesh_sh3"),
                                   &mesh_sh3_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("mesh_sh3_data_offset"),
                                   &mesh_sh3_data_offset_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("mesh_sh4"),
                                   &mesh_sh4_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("mesh_sh4_data_offset"),
                                   &mesh_sh4_data_offset_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("model"),
                                   &model_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("normal_matrix"),
                                   &normal_matrix_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("shininess_material_sampler"),
                                   &shininess_material_sampler_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("shininess_material"),
                                   &shininess_material_float_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("specular_material_sampler"),
                                   &specular_material_sampler_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("specular_material"),
                                   &specular_material_float_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("vp"),
                                   &vp_uniform_descriptor);
    ogl_program_get_uniform_by_name(uber_ptr->program,
                                    system_hashed_ansi_string_create("world_camera"),
                                   &world_camera_uniform_descriptor);

    if (ambient_material_sampler_uniform_descriptor != NULL)
    {
        uber_ptr->ambient_material_sampler_uniform_location = ambient_material_sampler_uniform_descriptor->location;
    }

    if (ambient_material_vec4_uniform_descriptor != NULL)
    {
        uber_ptr->ambient_material_vec4_uniform_location = ambient_material_vec4_uniform_descriptor->location;
    }

    if (diffuse_material_sampler_uniform_descriptor != NULL)
    {
        uber_ptr->diffuse_material_sampler_uniform_location = diffuse_material_sampler_uniform_descriptor->location;
    }

    if (diffuse_material_vec4_uniform_descriptor != NULL)
    {
        uber_ptr->diffuse_material_vec4_uniform_location = diffuse_material_vec4_uniform_descriptor->location;
    }

    if (glosiness_uniform_descriptor != NULL)
    {
        uber_ptr->glosiness_uniform_location = glosiness_uniform_descriptor->location;
    }

    if (luminosity_material_sampler_uniform_descriptor != NULL)
    {
        uber_ptr->luminosity_material_sampler_uniform_location = luminosity_material_sampler_uniform_descriptor->location;
    }

    if (luminosity_material_float_uniform_descriptor != NULL)
    {
        uber_ptr->luminosity_material_float_uniform_location = luminosity_material_float_uniform_descriptor->location;
    }

    if (mesh_sh3_uniform_descriptor != NULL)
    {
        uber_ptr->mesh_sh3_uniform_location = mesh_sh3_uniform_descriptor->location;
    }
    if (mesh_sh3_data_offset_uniform_descriptor != NULL)
    {
        uber_ptr->mesh_sh3_data_offset_uniform_location = mesh_sh3_data_offset_uniform_descriptor->location;
    }

    if (mesh_sh4_uniform_descriptor != NULL)
    {
        uber_ptr->mesh_sh4_uniform_location = mesh_sh4_uniform_descriptor->location;
    }
    if (mesh_sh4_data_offset_uniform_descriptor != NULL)
    {
        uber_ptr->mesh_sh4_data_offset_uniform_location = mesh_sh4_data_offset_uniform_descriptor->location;
    }

    if (model_uniform_descriptor != NULL)
    {
        uber_ptr->model_uniform_location = model_uniform_descriptor->location;
    }

    if (normal_matrix_uniform_descriptor != NULL)
    {
        uber_ptr->normal_matrix_uniform_location = normal_matrix_uniform_descriptor->location;
    }

    if (shininess_material_sampler_uniform_descriptor != NULL)
    {
        uber_ptr->shininess_material_sampler_uniform_location = shininess_material_sampler_uniform_descriptor->location;
    }

    if (shininess_material_float_uniform_descriptor != NULL)
    {
        uber_ptr->shininess_material_float_uniform_location = shininess_material_float_uniform_descriptor->location;
    }

    if (specular_material_sampler_uniform_descriptor != NULL)
    {
        uber_ptr->specular_material_sampler_uniform_location = specular_material_sampler_uniform_descriptor->location;
    }

    if (specular_material_float_uniform_descriptor != NULL)
    {
        uber_ptr->specular_material_float_uniform_location = specular_material_float_uniform_descriptor->location;
    }

    if (vp_uniform_descriptor != NULL)
    {
        ASSERT_DEBUG_SYNC(vp_uniform_descriptor->ub_offset != -1,
                          "VP UB offset is -1");

        uber_ptr->vp_ub_offset = vp_uniform_descriptor->ub_offset;
    }

    if (world_camera_uniform_descriptor != NULL)
    {
        ASSERT_DEBUG_SYNC(world_camera_uniform_descriptor->ub_id != -1,
                          "World camera UB offset is -1");

        uber_ptr->world_camera_ub_offset = world_camera_uniform_descriptor->ub_offset;
    }

    /* Retrieve uniform block IDs and their properties*/
    bool         fs_ub_size_retrieved = false;
    unsigned int fs_ub_size           = 0;
    bool         vs_ub_size_retrieved = false;
    unsigned int vs_ub_size           = 0;

    uber_ptr->ub_fs_id = -1;
    uber_ptr->ub_vs_id = -1;

    fs_ub_size_retrieved = ogl_program_get_uniform_block_index(uber_ptr->program,
                                                               system_hashed_ansi_string_create("FragmentShaderProperties"),
                                                              &uber_ptr->ub_fs_id);
    vs_ub_size_retrieved = ogl_program_get_uniform_block_index(uber_ptr->program,
                                                               system_hashed_ansi_string_create("VertexShaderProperties"),
                                                              &uber_ptr->ub_vs_id);

    if (fs_ub_size_retrieved)
    {
        ogl_program_get_uniform_block_properties(uber_ptr->program,
                                                 uber_ptr->ub_fs_id,
                                                &fs_ub_size,
                                                 NULL,  /* out_name */
                                                 NULL); /* out_n_members */
    }

    if (vs_ub_size_retrieved)
    {
        ogl_program_get_uniform_block_properties(uber_ptr->program,
                                                 uber_ptr->ub_vs_id,
                                                &vs_ub_size,
                                                 NULL,  /* out_name */
                                                 NULL); /* out_n_members */
    }

    /* Create internal representation of uber shader items */
    const unsigned int n_items = system_resizable_vector_get_amount_of_elements(uber_ptr->added_items);

    for (unsigned int n_item = 0;
                      n_item < n_items;
                    ++n_item)
    {
        _ogl_uber_item* item_ptr = NULL;

        if (!system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                    n_item,
                                                   &item_ptr) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve uber item descriptor at index [%d]",
                               n_item);
        }

        /* Fill relevant fields */
        switch (item_ptr->type)
        {
            case OGL_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE:
            {
                /* UB not used */
                break;
            }

            case OGL_UBER_ITEM_LIGHT:
            {
                /* Fragment shader stuff */
                const ogl_program_uniform_descriptor* light_ambient_color_uniform_ptr = NULL;
                std::stringstream                     light_attenuations_uniform_name_sstream;
                const ogl_program_uniform_descriptor* light_attenuations_uniform_ptr = NULL;
                std::stringstream                     light_diffuse_uniform_name_sstream;
                const ogl_program_uniform_descriptor* light_diffuse_uniform_ptr = NULL;
                std::stringstream                     light_direction_uniform_name_sstream;
                const ogl_program_uniform_descriptor* light_direction_uniform_ptr = NULL;
                std::stringstream                     light_location_uniform_name_sstream;
                const ogl_program_uniform_descriptor* light_location_uniform_ptr = NULL;
                std::stringstream                     light_shadow_map_uniform_name_sstream;
                const ogl_program_uniform_descriptor* light_shadow_map_uniform_ptr = NULL;

                light_attenuations_uniform_name_sstream << "light" << n_item << "_attenuations";
                light_diffuse_uniform_name_sstream      << "light" << n_item << "_diffuse";
                light_direction_uniform_name_sstream    << "light" << n_item << "_direction";
                light_location_uniform_name_sstream     << "light" << n_item << "_world_pos";
                light_shadow_map_uniform_name_sstream   << "light" << n_item << "_shadow_map";

                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create("ambient_color"),
                                               &light_ambient_color_uniform_ptr);
                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create(light_attenuations_uniform_name_sstream.str().c_str() ),
                                               &light_attenuations_uniform_ptr);
                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create(light_diffuse_uniform_name_sstream.str().c_str() ),
                                               &light_diffuse_uniform_ptr);
                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create(light_direction_uniform_name_sstream.str().c_str() ),
                                               &light_direction_uniform_ptr);
                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create(light_location_uniform_name_sstream.str().c_str()  ),
                                               &light_location_uniform_ptr);
                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create(light_shadow_map_uniform_name_sstream.str().c_str() ),
                                               &light_shadow_map_uniform_ptr);

                if (light_ambient_color_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.ambient_color_ub_offset = light_ambient_color_uniform_ptr->ub_offset;
                }

                if (light_attenuations_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_attenuations_ub_offset = light_attenuations_uniform_ptr->ub_offset;
                }

                if (light_direction_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_direction_ub_offset = light_direction_uniform_ptr->ub_offset;
                }

                if (light_diffuse_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_diffuse_ub_offset  = light_diffuse_uniform_ptr->ub_offset;
                }

                if (light_location_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_location_ub_offset = light_location_uniform_ptr->ub_offset;
                }

                if (light_shadow_map_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_texture_sampler_location = light_shadow_map_uniform_ptr->location;
                }

                /* Vertex shader stuff */
                std::stringstream                     light_depth_vb_uniform_name_sstream;
                const ogl_program_uniform_descriptor* light_depth_vb_uniform_ptr = NULL;
                shaders_vertex_uber_light             light_type                 = SHADERS_VERTEX_UBER_LIGHT_NONE;

                light_depth_vb_uniform_name_sstream << "light" << n_item << "_depth_vp";

                ogl_program_get_uniform_by_name(uber_ptr->program,
                                                system_hashed_ansi_string_create(light_depth_vb_uniform_name_sstream.str().c_str() ),
                                               &light_depth_vb_uniform_ptr);

                if (light_depth_vb_uniform_ptr != NULL)
                {
                    item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset = light_depth_vb_uniform_ptr->ub_offset;
                }

                /* Outdated SH stuff */
                shaders_vertex_uber_get_light_type(uber_ptr->shader_vertex,
                                                   n_item,
                                                  &light_type);

                if (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS ||
                    light_type == SHADERS_VERTEX_UBER_LIGHT_SH_4_BANDS)
                {
                    GLint                                 sh_data_uniform_location = -1;
                    std::stringstream                     sh_data_uniform_name_sstream;
                    const ogl_program_uniform_descriptor* sh_data_uniform_ptr      = NULL;

                    if (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS)
                    {
                        sh_data_uniform_name_sstream << "light" << n_item << "_sh3[0]";
                    }
                    else
                    {
                        sh_data_uniform_name_sstream << "light" << n_item << "_sh4[0]";
                    }

                    ogl_program_get_uniform_by_name(uber_ptr->program,
                                                    system_hashed_ansi_string_create(sh_data_uniform_name_sstream.str().c_str()),
                                                   &sh_data_uniform_ptr);

                    ASSERT_DEBUG_SYNC(sh_data_uniform_ptr != NULL,
                                      "Could not retrieve SH data uniform descriptor");
                    ASSERT_DEBUG_SYNC(sh_data_uniform_ptr->ub_offset != -1,
                                      "UB offset for SH data is -1");

                    item_ptr->vertex_shader_item.current_light_sh_data_ub_offset = sh_data_uniform_ptr->ub_offset;
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unrecognized uber item type");
            }
        } /* switch (item_type) */
    } /* for (all uber items) */

    /* Preallocate space for process-side storage of UB data. Since we are going to use one BO for two separate
     * UB bindings, we need to make sure the vetex shader data is aligned as required by underlying GL impl */
    const ogl_context_gl_limits* limits_ptr =NULL;
    uint32_t                     n_bytes    = fs_ub_size;

    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    if (n_bytes % limits_ptr->uniform_buffer_offset_alignment != 0)
    {
        /* Align! */
        n_bytes += (limits_ptr->uniform_buffer_offset_alignment - (n_bytes % limits_ptr->uniform_buffer_offset_alignment) );
    }

    uber_ptr->bo_data_vertex_offset  = n_bytes;
    n_bytes                       += vs_ub_size;
    uber_ptr->bo_data_size           = n_bytes;

    if (uber_ptr->bo_data != NULL)
    {
        delete [] uber_ptr->bo_data;

        uber_ptr->bo_data = NULL;
    }

    uber_ptr->bo_data = new (std::nothrow) char[n_bytes];
    ASSERT_ALWAYS_SYNC(uber_ptr->bo_data != NULL,
                       "Out of memory");

    /* Request renderer thread call-back to do the other part of the initialization */
    ogl_context_request_callback_from_context_thread(uber_ptr->context,
                                                     _ogl_uber_relink_renderer_callback,
                                                     uber_ptr);

    /* All done - object no longer dirty */
    uber_ptr->dirty = false;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_uber ogl_uber_create(__in __notnull ogl_context               context,
                                            __in __notnull system_hashed_ansi_string name)
{
    _ogl_uber_verify_context_type(context);

    _ogl_uber* result = new (std::nothrow) _ogl_uber;

    ASSERT_DEBUG_SYNC(result != NULL, "Out of memory");
    if (result != NULL)
    {
        result->added_items                   = system_resizable_vector_create(4 /* capacity */,
                                                                               sizeof(_ogl_uber_item*) );
        result->bo_data                       = NULL;
        result->bo_data_size                  = -1;
        result->bo_data_vertex_offset         = -1;
        result->context                       = context;    /* DO NOT retain, or face circular dependencies! */
        result->current_camera_location_dirty = true;
        result->current_vp                    = system_matrix4x4_create();
        result->current_vp_dirty              = true;
        result->dirty                         = true;
        result->is_rendering                  = false;
        result->mesh_to_vao_descriptor_map    = system_hash64map_create(sizeof(_ogl_uber_vao*),
                                                                        false);
        result->name                          = name;
        result->n_texture_units_assigned      = 0;
        result->shader_fragment               = shaders_fragment_uber_create(context,
                                                                             name);
        result->shader_vertex                 = shaders_vertex_uber_create  (context,
                                                                             name);
        result->variant_float                 = system_variant_create(SYSTEM_VARIANT_FLOAT);

        result->ambient_material_sampler_uniform_location    = -1;
        result->ambient_material_vec4_uniform_location       = -1;
        result->diffuse_material_sampler_uniform_location    = -1;
        result->diffuse_material_vec4_uniform_location       = -1;
        result->glosiness_uniform_location                   = -1;
        result->luminosity_material_sampler_uniform_location = -1;
        result->luminosity_material_float_uniform_location   = -1;
        result->mesh_sh3_uniform_location                    = -1;
        result->mesh_sh3_data_offset_uniform_location        = -1;
        result->mesh_sh4_uniform_location                    = -1;
        result->mesh_sh4_data_offset_uniform_location        = -1;
        result->model_uniform_location                       = -1;
        result->normal_matrix_uniform_location               = -1;
        result->object_normal_attribute_location             = -1;
        result->object_uv_attribute_location                 = -1;
        result->object_vertex_attribute_location             = -1;
        result->shininess_material_sampler_uniform_location  = -1;
        result->shininess_material_float_uniform_location    = -1;
        result->specular_material_sampler_uniform_location   = -1;
        result->specular_material_float_uniform_location     = -1;
        result->vp_ub_offset                                 = -1;
        result->world_camera_ub_offset                       = -1;

        result->graph_rendering_current_matrix = system_matrix4x4_create();

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _ogl_uber_release,
                                                       OBJECT_TYPE_OGL_UBER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Ubers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        /* Create a program with the shaders we were provided */
        result->program = ogl_program_create(context,
                                             name);

        ASSERT_ALWAYS_SYNC(result->program != NULL, "Cannot instantiate uber program");
        if (result->program != NULL)
        {
            if (!ogl_program_attach_shader(result->program,
                                           shaders_fragment_uber_get_shader(result->shader_fragment)) ||
                !ogl_program_attach_shader(result->program,
                                           shaders_vertex_uber_get_shader  (result->shader_vertex)) )
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Cannot attach shader(s) to uber program");
            }

            /* Request renderer thread call-back to do the other part of the initialization */
            ogl_context_request_callback_from_context_thread(context,
                                                             _ogl_uber_create_renderer_callback,
                                                             result);
        }
    }

    return (ogl_uber) result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_get_shader_general_property(__in  __notnull const ogl_uber             uber,
                                                             __in            _ogl_uber_general_property property,
                                                             __out __notnull void*                      out_result)
{
    const _ogl_uber* uber_ptr = (const _ogl_uber*) uber;

    switch (property)
    {
        case OGL_UBER_GENERAL_PROPERTY_N_ITEMS:
        {
            *( (uint32_t*) out_result) = system_resizable_vector_get_amount_of_elements(uber_ptr->added_items);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized general ogl_uber property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_get_shader_item_property(__in __notnull const ogl_uber          uber,
                                                          __in           ogl_uber_item_id        item_id,
                                                          __in           _ogl_uber_item_property property,
                                                          __out          void*                   result)
{
    const _ogl_uber*      uber_ptr = (const _ogl_uber*) uber;
          _ogl_uber_item* item_ptr = NULL;

    if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                               item_id,
                                              &item_ptr) )
    {
        switch (property)
        {
            case OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR:
            {
                ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.ambient_color_ub_offset != -1,
                                  "OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR requested but the underlying shader does not use the property");

                *((float**) result) = item_ptr->fragment_shader_item.ambient_color_sRGB;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
            {
                ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_attenuations_ub_offset != -1,
                                  "OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS requested but the underlying shader does not use the property");

                *((float**) result) = item_ptr->fragment_shader_item.current_light_attenuations;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
            {
                ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_diffuse_ub_offset != -1,
                                  "OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE requested but the underlying shader does not use the property");

                *((float**) result) = item_ptr->fragment_shader_item.current_light_diffuse_sRGB;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
            {
                ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_direction_ub_offset != -1,
                                  "OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION requested but the underlying shader does not use the property");

                *((float**) result) = item_ptr->fragment_shader_item.current_light_direction;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
            {
                ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_location_ub_offset != -1,
                                  "OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION requested but the underlying shader does not use the property");

                *((float**) result) = item_ptr->fragment_shader_item.current_light_location;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF request");

                *(scene_light_falloff*) result = item_ptr->falloff;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS request");

                *(scene_light_shadow_map_bias*) result = item_ptr->shadow_map_bias;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP request");

                *(bool*) result = item_ptr->is_shadow_caster;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_TYPE:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_TYPE request");

                shaders_fragment_uber_get_light_item_properties(uber_ptr->shader_fragment,
                                                                item_ptr->fs_item_id,
                                                                (shaders_fragment_uber_light_type*) result);

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_TYPE:
            {
                *((_ogl_uber_item_type*) result) = item_ptr->type;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized uber item property requested");
            }
        } /* switch (property) */
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve descriptor of uber shader item at index [%d]",
                          item_id);
    }
}

/** TODO.
 *
 *  Used by ogl_scene_renderer.
 *
 *  @param material Material to issue draw calls for. Note that this function
 *                  does not change current GL active program state - it merely
 *                  issues draw calls for those layers whose material matches
 *                  the one passed as an argument.
 *                  If @param material is NULL, all layers will be rendered
 *                  using currently bound program.
 **/
PUBLIC void ogl_uber_rendering_render_mesh(__in __notnull mesh                 mesh_gpu,
                                           __in __notnull system_matrix4x4     model,
                                           __in __notnull system_matrix4x4     normal_matrix,
                                           __in __notnull ogl_uber             uber,
                                           __in_opt       mesh_material        material,
                                           __in           system_timeline_time time)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    if (mesh_gpu != NULL)
    {
        _ogl_uber_vao* vao_ptr = NULL;

        /* If the mesh is instantiated, retrieve the mesh instance we should be using
         * for the rendering */
        mesh mesh_instantiation_parent_gpu = NULL;

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_INSTANTIATION_PARENT,
                         &mesh_instantiation_parent_gpu);

        if (mesh_instantiation_parent_gpu == NULL)
        {
            mesh_instantiation_parent_gpu = mesh_gpu;
        }

        /* Retrieve VAO for the user-requested mesh */
        if (!system_hash64map_get(uber_ptr->mesh_to_vao_descriptor_map,
                                  (system_hash64) mesh_instantiation_parent_gpu,
                                  &vao_ptr) )
        {
            /* No VAO initialized? Pity, one should have been created a long time ago.. */
            _ogl_uber_bake_mesh_vao(uber_ptr,
                                    mesh_instantiation_parent_gpu);

            /* Retrieve the new VAO descriptor */
            system_hash64map_get(uber_ptr->mesh_to_vao_descriptor_map,
                                 (system_hash64) mesh_instantiation_parent_gpu,
                                &vao_ptr);

            ASSERT_DEBUG_SYNC(vao_ptr != NULL,
                              "Mesh VAO was not created successfully");
        }
        else
        {
            /* Make sure the VAO we're using needs not be re-initialized */
            system_timeline_time mesh_modification_timestamp = 0;

            mesh_get_property(mesh_instantiation_parent_gpu,
                              MESH_PROPERTY_TIMESTAMP_MODIFICATION,
                              &mesh_modification_timestamp);

            if (mesh_modification_timestamp != vao_ptr->mesh_modification_timestamp)
            {
                system_hashed_ansi_string mesh_name = NULL;

                mesh_get_property(mesh_instantiation_parent_gpu,
                                  MESH_PROPERTY_NAME,
                                 &mesh_name);

                LOG_INFO("Mesh [%s] was updated - need to update ogl_uber VAO representation",
                         system_hashed_ansi_string_get_buffer(mesh_name)
                        );

                _ogl_uber_bake_mesh_vao(uber_ptr,
                                        mesh_instantiation_parent_gpu);
            }
        }

        /* Bind the VAO */
        const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
        const ogl_context_gl_entrypoints*                         entry_points     = NULL;

        ogl_context_get_property(uber_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &dsa_entry_points);
        ogl_context_get_property(uber_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        entry_points->pGLBindVertexArray(vao_ptr->vao_id);

        /* Update model matrix */
        ASSERT_DEBUG_SYNC(uber_ptr->model_uniform_location != -1,
                          "No model matrix uniform found");

        entry_points->pGLProgramUniformMatrix4fv(ogl_program_get_id(uber_ptr->program),
                                                 uber_ptr->model_uniform_location,
                                                 1,       /* count */
                                                 GL_TRUE, /* transpose */
                                                 system_matrix4x4_get_row_major_data(model)
                                                );

        /* Update normal matrix */
        if (uber_ptr->normal_matrix_uniform_location != -1)
        {
            ASSERT_DEBUG_SYNC(normal_matrix != NULL,
                              "Normal matrix is NULL but is required.");

            entry_points->pGLProgramUniformMatrix4fv(ogl_program_get_id(uber_ptr->program),
                                                     uber_ptr->normal_matrix_uniform_location,
                                                     1,       /* count */
                                                     GL_TRUE, /* transpose */
                                                     system_matrix4x4_get_row_major_data(normal_matrix)
                                                    );
        }

        /* Retrieve mesh index type and convert it to GL equivalent */
        GLenum           gl_index_type = GL_NONE;
        _mesh_index_type index_type    = MESH_INDEX_TYPE_UNKNOWN;

        mesh_get_property(mesh_instantiation_parent_gpu,
                          MESH_PROPERTY_GL_INDEX_TYPE,
                         &index_type);

        switch (index_type)
        {
            case MESH_INDEX_TYPE_UNSIGNED_CHAR:  gl_index_type = GL_UNSIGNED_BYTE;  break;
            case MESH_INDEX_TYPE_UNSIGNED_SHORT: gl_index_type = GL_UNSIGNED_SHORT; break;
            case MESH_INDEX_TYPE_UNSIGNED_INT:   gl_index_type = GL_UNSIGNED_INT;   break;

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized mesh index type");
            }
        } /* switch (index_type) */

        /* Iterate over all layers.. */
              uint32_t n_layers = 0;
        const GLuint   po_id    = ogl_program_get_id(uber_ptr->program);

        mesh_get_property(mesh_instantiation_parent_gpu,
                          MESH_PROPERTY_N_LAYERS,
                         &n_layers);

        for (uint32_t n_layer = 0;
                      n_layer < n_layers;
                    ++n_layer)
        {
            const uint32_t n_layer_passes = mesh_get_amount_of_layer_passes(mesh_instantiation_parent_gpu,
                                                                            n_layer);

            /* Iterate over all layer passes */
            for (uint32_t n_layer_pass = 0;
                          n_layer_pass < n_layer_passes;
                        ++n_layer_pass)
            {
                uint32_t      layer_pass_elements_offset = 0;
                uint32_t      layer_pass_index_max_value = 0;
                uint32_t      layer_pass_index_min_value = 0;
                mesh_material layer_pass_material        = NULL;
                uint32_t      layer_pass_n_elements      = 0;
                uint32_t      n_texture_units_used       = uber_ptr->n_texture_units_assigned;

                /* Retrieve layer pass properties */
                mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_MATERIAL,
                                            &layer_pass_material);
                mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET,
                                            &layer_pass_elements_offset);
                mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX,
                                            &layer_pass_index_max_value);
                mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX,
                                            &layer_pass_index_min_value);
                mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_N_ELEMENTS,
                                            &layer_pass_n_elements);

                if (layer_pass_material != material && material != NULL)
                {
                    continue;
                }

                if (material != NULL)
                {
                    /* Bind shading data for each supported shading property */
                    struct _attachment
                    {
                        mesh_material_shading_property property;
                        int32_t                        shader_sampler_uniform_location;
                        int32_t                        shader_scalar_uniform_location;
                        int32_t                        shader_uv_attribute_location;
                        bool                           convert_to_linear;
                    } attachments[] =
                    {
                        {
                            MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                            uber_ptr->ambient_material_sampler_uniform_location,
                            uber_ptr->ambient_material_vec4_uniform_location,
                            uber_ptr->object_uv_attribute_location,
                            true,
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                            uber_ptr->diffuse_material_sampler_uniform_location,
                            uber_ptr->diffuse_material_vec4_uniform_location,
                            uber_ptr->object_uv_attribute_location,
                            true,
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                            uber_ptr->luminosity_material_sampler_uniform_location,
                            uber_ptr->luminosity_material_float_uniform_location,
                            uber_ptr->object_uv_attribute_location,
                            false
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                            uber_ptr->shininess_material_sampler_uniform_location,
                            uber_ptr->shininess_material_float_uniform_location,
                            uber_ptr->object_uv_attribute_location,
                            false
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                            uber_ptr->specular_material_sampler_uniform_location,
                            uber_ptr->specular_material_float_uniform_location,
                            uber_ptr->object_uv_attribute_location,
                            false
                        },
                    };
                    const uint32_t n_attachments = sizeof(attachments) / sizeof(attachments[0]);

                    for (uint32_t n_attachment = 0;
                                  n_attachment < n_attachments;
                                ++n_attachment)
                    {
                        const _attachment&                      attachment      = attachments[n_attachment];
                        const mesh_material_property_attachment attachment_type = mesh_material_get_shading_property_attachment_type(layer_pass_material,
                                                                                                                                     attachment.property);

                        switch (attachment_type)
                        {
                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE:
                            {
                                /* Nothing to be done here */
                                break;
                            }

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT:
                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT:
                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3:
                            {
                                float        data_vec3[3];
                                unsigned int n_components = 1;

                                if (attachment.shader_scalar_uniform_location == -1)
                                {
                                    continue;
                                }

                                if (attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT)
                                {
                                    mesh_material_get_shading_property_value_float(layer_pass_material,
                                                                                   attachment.property,
                                                                                   data_vec3 + 0);
                                }
                                else
                                if (attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT)
                                {
                                    mesh_material_get_shading_property_value_curve_container_float(layer_pass_material,
                                                                                                   attachment.property,
                                                                                                   time,
                                                                                                   data_vec3 + 0);
                                }
                                else
                                {
                                    mesh_material_get_shading_property_value_curve_container_vec3(layer_pass_material,
                                                                                                  attachment.property,
                                                                                                  time,
                                                                                                  data_vec3);

                                    n_components = 3;
                                }

                                if (attachment.convert_to_linear)
                                {
                                    for (unsigned int n_component = 0;
                                                      n_component < n_components;
                                                    ++n_component)
                                    {
                                        data_vec3[n_component] = convert_sRGB_to_linear(data_vec3[n_component]);
                                    }
                                } /* if (attachment.convert_to_linear) */

                                if (attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT                  ||
                                    attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT)
                                {
                                    entry_points->pGLProgramUniform1f(po_id,
                                                                      attachment.shader_scalar_uniform_location,
                                                                      data_vec3[0]);
                                }
                                else
                                {
                                    entry_points->pGLProgramUniform3fv(po_id,
                                                                       attachment.shader_scalar_uniform_location,
                                                                       1, /* count */
                                                                       data_vec3);
                                }

                                break;
                            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT: */

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                            {
                                ogl_sampler  layer_pass_sampler              = NULL;
                                ogl_texture  layer_pass_texture              = NULL;
                                unsigned int layer_pass_texture_mipmap_level = 0;

                                if (attachment.shader_sampler_uniform_location == -1)
                                {
                                    continue;
                                }

                                /* Set up the sampler */
                                mesh_material_get_shading_property_value_texture(layer_pass_material,
                                                                                 attachment.property,
                                                                                &layer_pass_texture,
                                                                                &layer_pass_texture_mipmap_level,
                                                                                &layer_pass_sampler);

                                entry_points->pGLBindSampler(n_texture_units_used,
                                                             ogl_sampler_get_id(layer_pass_sampler) );

                                dsa_entry_points->pGLBindMultiTextureEXT (GL_TEXTURE0 + n_texture_units_used,
                                                                          GL_TEXTURE_2D,
                                                                          layer_pass_texture);
                                entry_points->pGLProgramUniform1i        (po_id,
                                                                          attachment.shader_sampler_uniform_location,
                                                                          n_texture_units_used);

                                n_texture_units_used++;

                                break;
                            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE: */

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                            {
                                float data_vec4[4];

                                if (attachment.shader_scalar_uniform_location == -1)
                                {
                                    continue;
                                }

                                mesh_material_get_shading_property_value_vec4(layer_pass_material,
                                                                              attachment.property,
                                                                              data_vec4);

                                if (attachment.convert_to_linear)
                                {
                                    for (unsigned int n_component = 0;
                                                      n_component < 4;
                                                    ++n_component)
                                    {
                                        data_vec4[n_component] = convert_sRGB_to_linear(data_vec4[n_component]);
                                    }
                                } /* if (attachment.convert_to_linear) */

                                entry_points->pGLProgramUniform4fv(po_id,
                                                                   attachment.shader_scalar_uniform_location,
                                                                   1, /* count */
                                                                   data_vec4);

                                break;
                            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4: */

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false, "Unrecognized material property attachment");
                            }
                        } /* switch (attachment_type) */
                    } /* for (all attachments) */
                } /* if (material != NULL) */

                /* Issue the draw call */
                entry_points->pGLDrawRangeElements(GL_TRIANGLES,
                                                   layer_pass_index_min_value,
                                                   layer_pass_index_max_value,
                                                   layer_pass_n_elements,
                                                   gl_index_type,
                                                  (const GLvoid*) layer_pass_elements_offset);
            } /* for (all mesh layer passes) */
        } /* for (all mesh layers) */
    } /* if (mesh_gpu != NULL) */
}

/* Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_uber_rendering_start(__in __notnull ogl_uber uber)
{
    _ogl_uber*                                                uber_ptr         = (_ogl_uber*) uber;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;

    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ASSERT_DEBUG_SYNC(!uber_ptr->is_rendering,
                      "Already started");

    /* Reset texture unit use counter */
    uber_ptr->n_texture_units_assigned = 0;

    /* Update shaders if the configuration has been changed since the last call */
    if (uber_ptr->dirty)
    {
        _ogl_uber_relink(uber);

        ASSERT_DEBUG_SYNC(!uber_ptr->dirty,
                          "Relinking did not reset the dirty flag");
    }

    /* Set up UB contents & texture samplers */
    bool               has_modified_bo_data = false;
    const unsigned int n_items              = system_resizable_vector_get_amount_of_elements(uber_ptr->added_items);

    for (unsigned int n_item = 0;
                      n_item < n_items;
                    ++n_item)
    {
        _ogl_uber_item* item_ptr = NULL;

        if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                   n_item,
                                                  &item_ptr) )
        {
            switch (item_ptr->type)
            {
                case OGL_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE:
                {
                    /* UB not used */
                    break;
                }

                case OGL_UBER_ITEM_LIGHT:
                {
                    /* Fragment shader part */
                    if (item_ptr->fragment_shader_item.ambient_color_ub_offset != -1 &&
                        item_ptr->fragment_shader_item.ambient_color_dirty)
                    {
                        memcpy((char*) uber_ptr->bo_data + item_ptr->fragment_shader_item.ambient_color_ub_offset,
                                       item_ptr->fragment_shader_item.ambient_color_linear,
                                       sizeof(float) * 3);

                        has_modified_bo_data                               = true;
                        item_ptr->fragment_shader_item.ambient_color_dirty = false;
                    }

                    if (item_ptr->fragment_shader_item.current_light_attenuations_ub_offset != -1 &&
                        item_ptr->fragment_shader_item.current_light_attenuations_dirty)
                    {
                        memcpy((char*) uber_ptr->bo_data + item_ptr->fragment_shader_item.current_light_attenuations_ub_offset,
                                       item_ptr->fragment_shader_item.current_light_attenuations,
                                       sizeof(float) * 3);

                        has_modified_bo_data                                            = true;
                        item_ptr->fragment_shader_item.current_light_attenuations_dirty = false;
                    }

                    if (item_ptr->fragment_shader_item.current_light_diffuse_ub_offset != -1 &&
                        item_ptr->fragment_shader_item.current_light_diffuse_dirty)
                    {
                        memcpy((char*) uber_ptr->bo_data + item_ptr->fragment_shader_item.current_light_diffuse_ub_offset,
                                       item_ptr->fragment_shader_item.current_light_diffuse_sRGB,
                                       sizeof(float) * 4);

                        has_modified_bo_data                                       = true;
                        item_ptr->fragment_shader_item.current_light_diffuse_dirty = false;
                    }

                    if (item_ptr->fragment_shader_item.current_light_direction_ub_offset != -1 &&
                        item_ptr->fragment_shader_item.current_light_direction_dirty)
                    {
                        memcpy((char*) uber_ptr->bo_data + item_ptr->fragment_shader_item.current_light_direction_ub_offset,
                                       item_ptr->fragment_shader_item.current_light_direction,
                                       sizeof(float) * 3);

                        has_modified_bo_data                                         = true;
                        item_ptr->fragment_shader_item.current_light_direction_dirty = false;
                    }

                    if (item_ptr->fragment_shader_item.current_light_location_ub_offset != -1 &&
                        item_ptr->fragment_shader_item.current_light_location_dirty)
                    {
                        memcpy((char*) uber_ptr->bo_data + item_ptr->fragment_shader_item.current_light_location_ub_offset,
                                       item_ptr->fragment_shader_item.current_light_location,
                                       sizeof(float) * 4);

                        has_modified_bo_data                                        = true;
                        item_ptr->fragment_shader_item.current_light_location_dirty = false;
                    }

                    if (item_ptr->fragment_shader_item.current_light_shadow_map_texture_sampler_location != -1)
                    {
                        /* Bind the shadow map */
                        GLuint n_texture_unit = uber_ptr->n_texture_units_assigned++;

                        ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_shadow_map_texture != NULL,
                                          "No shadow map assigned to a light which casts shadows");

                        entry_points->pGLBindSampler            (n_texture_unit,
                                                                 0);            /* TODO: use a sampler instead of SM texture state! */
                        dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0 + n_texture_unit,
                                                                 GL_TEXTURE_2D,
                                                                 item_ptr->fragment_shader_item.current_light_shadow_map_texture);
                        entry_points->pGLProgramUniform1i       (ogl_program_get_id(uber_ptr->program),
                                                                 item_ptr->fragment_shader_item.current_light_shadow_map_texture_sampler_location,
                                                                 n_texture_unit);
                    }

                    /* Vertex shader part */
                    if (item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset != -1 &&
                        item_ptr->vertex_shader_item.current_light_depth_vp_dirty)
                    {
                        memcpy((char*) uber_ptr->bo_data + uber_ptr->bo_data_vertex_offset + item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset,
                               item_ptr->vertex_shader_item.current_light_depth_vp,
                               sizeof(item_ptr->vertex_shader_item.current_light_depth_vp) );

                        has_modified_bo_data                                      = true;
                        item_ptr->vertex_shader_item.current_light_depth_vp_dirty = false;
                    }

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized uber item type");
                }
            } /* switch (item_ptr->type) */
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve uber item descriptor at index [%d]", n_item);
        }
    } /* for (all fragment shader items) */

    if (uber_ptr->vp_ub_offset != -1 &&
        uber_ptr->current_vp_dirty)
    {
        memcpy((char*)uber_ptr->bo_data + uber_ptr->bo_data_vertex_offset + uber_ptr->vp_ub_offset,
               system_matrix4x4_get_row_major_data(uber_ptr->current_vp),
               sizeof(float) * 16);

        has_modified_bo_data       = true;
        uber_ptr->current_vp_dirty = false;
    }

    if (uber_ptr->world_camera_ub_offset != -1 &&
        uber_ptr->current_camera_location_dirty)
    {
        ASSERT_DEBUG_SYNC(uber_ptr->world_camera_ub_offset != -1,
                          "World camera location uniform error");

        memcpy((char*)uber_ptr->bo_data + uber_ptr->bo_data_vertex_offset + uber_ptr->world_camera_ub_offset,
               uber_ptr->current_camera_location,
               sizeof(float) * 4);

        has_modified_bo_data                    = true;
        uber_ptr->current_camera_location_dirty = false;
    }

    /* Copy the data to VRAM *ONLY* if necessary. This call is HEAVY - makes sense given
     * we will follow it with at least one dependent draw call.
     */
    if (has_modified_bo_data)
    {
        dsa_entry_points->pGLNamedBufferSubDataEXT(uber_ptr->bo_id,
                                                   0, /* offset */
                                                   (GLsizeiptr) uber_ptr->bo_data_size,
                                                   uber_ptr->bo_data);
    }

    /* If any part of the SH data comes from a BO, copy it now */
    for (unsigned int n_item = 0;
                      n_item < n_items;
                    ++n_item)
    {
        _ogl_uber_item* item_ptr = NULL;

        if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                   n_item,
                                                  &item_ptr) )
        {
            switch (item_ptr->type)
            {
                case OGL_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE:
                {
                    /* Not relevant */
                    break;
                }

                case OGL_UBER_ITEM_LIGHT:
                {
                    shaders_vertex_uber_light light_type = SHADERS_VERTEX_UBER_LIGHT_NONE;

                    if (!shaders_vertex_uber_get_light_type(uber_ptr->shader_vertex,
                                                            n_item,
                                                           &light_type))
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Cannot determine light type at index [%d]",
                                          n_item);
                    }

                    switch (light_type)
                    {
                        case SHADERS_VERTEX_UBER_LIGHT_NONE:
                        {
                            break;
                        }

                        case SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS:
                        case SHADERS_VERTEX_UBER_LIGHT_SH_4_BANDS:
                        {
                            const unsigned int sh_data_size = (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS) ? 4 * sizeof(float) * 9
                                                                                                                   : 4 * sizeof(float) * 12;

                            dsa_entry_points->pGLNamedCopyBufferSubDataEXT(item_ptr->vertex_shader_item.current_light_sh_data.bo_id,
                                                                           uber_ptr->bo_id,
                                                                           item_ptr->vertex_shader_item.current_light_sh_data.bo_offset,
                                                                           uber_ptr->bo_data_vertex_offset + item_ptr->vertex_shader_item.current_light_sh_data_ub_offset,
                                                                           sh_data_size);
                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false, "Unrecognized light type at index [%d]", n_item);
                        }
                    } /* switch (light_type) */

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false, "Unrecognized vertex shader item type");
                }
            } /* switch (item_ptr->type) */
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve uber item descriptor at index [%d]", n_item);
        }
    } /* for (all vertex shader items) */

    /* Configure uniform buffer bindings */
    uint32_t fs_ub_size = 0;
    uint32_t vs_ub_size = 0;

    if (uber_ptr->ub_fs_id != -1)
    {
        ogl_program_get_uniform_block_properties(uber_ptr->program,
                                                 uber_ptr->ub_fs_id,
                                                &fs_ub_size,
                                                 NULL,
                                                 NULL);

        if (fs_ub_size != 0)
        {
            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             FRAGMENT_SHADER_PROPERTIES_UBO_BINDING_ID,
                                             uber_ptr->bo_id,
                                             0, /* offset */
                                             fs_ub_size);
        }
    } /* if (uber_ptr->ub_fs_id != -1) */

    if (uber_ptr->ub_vs_id != -1)
    {
        ogl_program_get_uniform_block_properties(uber_ptr->program,
                                                 uber_ptr->ub_vs_id,
                                                &vs_ub_size,
                                                 NULL,
                                                 NULL);

        if (vs_ub_size != 0)
        {
            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             VERTEX_SHADER_PROPERTIES_UBO_BINDING_ID,
                                             uber_ptr->bo_id,
                                             uber_ptr->bo_data_vertex_offset,
                                             vs_ub_size);
        }
    }

    /* Activate the uber program */
    GLuint program_id = ogl_program_get_id(uber_ptr->program);

    entry_points->pGLUseProgram(program_id);

    /* Mark as 'being rendered' */
    uber_ptr->is_rendering = true;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_set_shader_general_property(__in __notnull ogl_uber                   uber,
                                                             __in           _ogl_uber_general_property property,
                                                             __in           const void*                data)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    switch (property)
    {
        case OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION:
        {
            if (memcmp(uber_ptr->current_camera_location,
                       data,
                       sizeof(float) * 3) != 0)
            {
                memcpy(uber_ptr->current_camera_location,
                       data,
                       sizeof(float) * 3);

                uber_ptr->current_camera_location[3]    = 1.0f;
                uber_ptr->current_camera_location_dirty = true;
            }

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_VP:
        {
            if (!system_matrix4x4_is_equal(uber_ptr->current_vp,
                                           (system_matrix4x4) data) )
            {
                system_matrix4x4_set_from_matrix4x4(uber_ptr->current_vp,
                                                    (system_matrix4x4) data);

                uber_ptr->current_vp_dirty = true;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized general uber property [%d]",
                              property);
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_set_shader_item_property(__in __notnull ogl_uber                uber,
                                                          __in           unsigned int            item_index,
                                                          __in           _ogl_uber_item_property property,
                                                          __in           const void*             data)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    switch (property)
    {
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
        case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP:
        case OGL_UBER_ITEM_PROPERTY_VERTEX_DEPTH_VP:
        {
            _ogl_uber_item* item_ptr = NULL;

            if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                       item_index,
                                                      &item_ptr) )
            {
                switch (property)
                {
                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR:
                    {
                        if (memcmp(item_ptr->fragment_shader_item.ambient_color_sRGB,
                                   data,
                                   sizeof(float) * 3) != 0)
                        {
                            /* We're rendering in linear space, so make sure to convert
                             * sRGB light color to linear space */
                            float* data_ptr = (float*) data;

                            for (unsigned int n_component = 0;
                                              n_component < 3;
                                            ++n_component)
                            {
                                item_ptr->fragment_shader_item.ambient_color_linear[n_component] = convert_sRGB_to_linear(data_ptr[n_component]);
                            }

                            memcpy(item_ptr->fragment_shader_item.ambient_color_sRGB,
                                   data,
                                   sizeof(float) * 3);

                            item_ptr->fragment_shader_item.ambient_color_dirty = true;
                        }

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
                    {
                        if (memcmp(item_ptr->fragment_shader_item.current_light_attenuations,
                                   data,
                                   sizeof(float) * 3) != 0)
                        {
                            memcpy(item_ptr->fragment_shader_item.current_light_attenuations,
                                   data,
                                   sizeof(float) * 3);

                            item_ptr->fragment_shader_item.current_light_attenuations_dirty = true;
                        }

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
                    {
                        if (memcmp(item_ptr->fragment_shader_item.current_light_diffuse_sRGB,
                                   data,
                                   sizeof(float) * 3) != 0)
                        {
                            /* We're rendering in linear space, so make sure to convert
                             * sRGB light color to linear space */
                            for (unsigned int n_component = 0;
                                              n_component < 3;
                                            ++n_component)
                            {
                                item_ptr->fragment_shader_item.current_light_diffuse_linear[n_component] = convert_sRGB_to_linear(((float*) data)[n_component]);
                            }

                            item_ptr->fragment_shader_item.current_light_diffuse_linear[3] = 1.0f;

                            memcpy(item_ptr->fragment_shader_item.current_light_diffuse_sRGB,
                                   data,
                                   sizeof(float) * 3);

                            item_ptr->fragment_shader_item.current_light_diffuse_dirty = true;
                        }

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
                    {
                        if (memcmp(item_ptr->fragment_shader_item.current_light_direction,
                                   data,
                                   sizeof(float) * 3) != 0)
                        {
                            memcpy(item_ptr->fragment_shader_item.current_light_direction,
                                   data,
                                   sizeof(float) * 3);

                            item_ptr->fragment_shader_item.current_light_direction_dirty = true;
                        }

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
                    {
                        if (memcmp(item_ptr->fragment_shader_item.current_light_location,
                                   data,
                                   sizeof(float) * 3) != 0)
                        {
                            memcpy(item_ptr->fragment_shader_item.current_light_location,
                                   data,
                                   sizeof(float) * 3);

                            item_ptr->fragment_shader_item.current_light_location[3]    = 1.0f;
                            item_ptr->fragment_shader_item.current_light_location_dirty = true;
                        }

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP:
                    {
                        item_ptr->fragment_shader_item.current_light_shadow_map_texture = *(ogl_texture*) data;

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_VERTEX_DEPTH_VP:
                    {
                        if (memcmp(item_ptr->vertex_shader_item.current_light_depth_vp,
                                   data,
                                   sizeof(float) * 16) != 0)
                        {
                            memcpy(item_ptr->vertex_shader_item.current_light_depth_vp,
                                   data,
                                   sizeof(float) * 16);

                            item_ptr->vertex_shader_item.current_light_depth_vp_dirty = true;
                        }

                        break;
                    }
                } /* switch (property) */
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve fragment shader item descriptor at index [%d]",
                                  item_index);
            }

            break;
        }

        case OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA:
        {
            shaders_vertex_uber_light light_type = SHADERS_VERTEX_UBER_LIGHT_NONE;

            if (shaders_vertex_uber_get_light_type(uber_ptr->shader_vertex,
                                                   item_index,
                                                  &light_type) )
            {
                _ogl_uber_item* item_ptr = NULL;

                if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                           item_index,
                                                          &item_ptr) )
                {
                    memcpy(&item_ptr->vertex_shader_item.current_light_sh_data,
                           data,
                           sizeof(_ogl_uber_light_sh_data) );
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve vertex shader item at index [%d]",
                                      item_index);
                }
            } /* if (shaders_vertex_uber_get_light_type(uber_ptr->shader_vertex, item_index, &light_type) ) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Item at index [%d] is not a light",
                                  item_index);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized item property requested [%d]",
                              property);
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_uber_rendering_stop(__in __notnull ogl_uber uber)
{
    _ogl_uber*                        uber_ptr     = (_ogl_uber*) uber;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(uber_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ASSERT_DEBUG_SYNC(uber_ptr->is_rendering,
                      "Not started");

    /* Mark as no longer rendered */
    uber_ptr->is_rendering = false;
}
