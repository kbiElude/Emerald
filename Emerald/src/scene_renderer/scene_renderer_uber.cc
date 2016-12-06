/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * Responsible for preparing a command buffer which renders a single mesh, as well as a CPU
 * present task which updates all uniform buffers, before the draw commands are issued.
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer_uber.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_math_srgb.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_variant.h"
#include <sstream>


/** Internal type definitions */
static const char* _scene_renderer_uber_attribute_name_object_normal             = "object_normal";
static const char* _scene_renderer_uber_attribute_name_object_uv                 = "object_uv";
static const char* _scene_renderer_uber_attribute_name_object_vertex             = "object_vertex";
static const char* _scene_renderer_uber_block_name_ub_fs                         = "FragmentShaderProperties";
static const char* _scene_renderer_uber_block_name_ub_vs                         = "VertexShaderProperties";
static const char* _scene_renderer_uber_uniform_name_ambient_material_sampler    = "ambient_material_sampler";
static const char* _scene_renderer_uber_uniform_name_diffuse_material_sampler    = "diffuse_material_sampler";
static const char* _scene_renderer_uber_uniform_name_emission_material_sampler   = "emission_material_sampler";
static const char* _scene_renderer_uber_uniform_name_luminosity_material_sampler = "luminosity_material_sampler";
static const char* _scene_renderer_uber_uniform_name_mesh_sh3                    = "mesh_sh3";
static const char* _scene_renderer_uber_uniform_name_mesh_sh3_data_offset        = "mesh_sh3_data_offset";
static const char* _scene_renderer_uber_uniform_name_mesh_sh4                    = "mesh_sh4";
static const char* _scene_renderer_uber_uniform_name_mesh_sh4_data_offset        = "mesh_sh4_data_offset";
static const char* _scene_renderer_uber_uniform_name_shininess_material_sampler  = "shininess_material_sampler";
static const char* _scene_renderer_uber_uniform_name_specular_material_sampler   = "specular_material_sampler";


/* Holds all properties that may be used for a fragment shader item */
typedef struct _scene_renderer_uber_fragment_shader_item
{
    uint32_t ambient_color_ub_offset;

    uint32_t                  current_light_attenuations_ub_offset;
    uint32_t                  current_light_cone_angle_ub_offset; /* cone angle: 1/2 total light cone. used for spot lights */
    uint32_t                  current_light_diffuse_ub_offset;
    uint32_t                  current_light_direction_ub_offset;
    uint32_t                  current_light_edge_angle_ub_offset; /* angular width of the soft edge. used for spot lights */
    uint32_t                  current_light_far_near_diff_ub_offset;
    uint32_t                  current_light_location_ub_offset;
    uint32_t                  current_light_near_plane_ub_offset;
    uint32_t                  current_light_projection_ub_offset;
    uint32_t                  current_light_range_ub_offset;
    system_hashed_ansi_string current_light_shadow_map_color_uniform_name;
    system_hashed_ansi_string current_light_shadow_map_depth_uniform_name;
    uint32_t                  current_light_shadow_map_vsm_cutoff_ub_offset;
    uint32_t                  current_light_shadow_map_vsm_min_variance_ub_offset;
    uint32_t                  current_light_view_ub_offset;

    ral_texture_view current_light_shadow_map_texture_view_color;
    ral_texture_view current_light_shadow_map_texture_view_depth;

    _scene_renderer_uber_fragment_shader_item()
    {
        ambient_color_ub_offset                             = -1;
        current_light_attenuations_ub_offset                = -1;
        current_light_cone_angle_ub_offset                  = -1;
        current_light_diffuse_ub_offset                     = -1;
        current_light_direction_ub_offset                   = -1;
        current_light_edge_angle_ub_offset                  = -1;
        current_light_far_near_diff_ub_offset               = -1;
        current_light_location_ub_offset                    = -1;
        current_light_near_plane_ub_offset                  = -1;
        current_light_projection_ub_offset                  = -1;
        current_light_range_ub_offset                       = -1;
        current_light_shadow_map_color_uniform_name         = nullptr;
        current_light_shadow_map_depth_uniform_name         = nullptr;
        current_light_shadow_map_vsm_cutoff_ub_offset       = -1;
        current_light_shadow_map_vsm_min_variance_ub_offset = -1;
        current_light_view_ub_offset                        = -1;

        current_light_shadow_map_texture_view_color = nullptr;
        current_light_shadow_map_texture_view_depth = nullptr;
    }
} _scene_renderer_uber_fragment_shader_item;

/* Holds all properties that may be used for a vertex shader item */
typedef struct _scene_renderer_uber_vertex_shader_item
{
    GLint current_light_depth_vp_ub_offset; /* row-major */

    scene_renderer_uber_light_sh_data current_light_sh_data;
    GLint                             current_light_sh_data_ub_offset;

    _scene_renderer_uber_vertex_shader_item()
    {
        current_light_depth_vp_ub_offset = -1;
    }

} _scene_renderer_uber_vertex_shader_item;

/* A single item added to scene_renderer_uber used to construct a program object managed by scene_renderer_uber */
typedef struct _scene_renderer_uber_item
{
    scene_light_falloff                         falloff;
    shaders_fragment_uber_item_id               fs_item_id;
    _scene_renderer_uber_fragment_shader_item   fragment_shader_item;
    bool                                        is_shadow_caster;
    scene_light_shadow_map_algorithm            shadow_map_algorithm;
    scene_light_shadow_map_bias                 shadow_map_bias;
    scene_light_shadow_map_pointlight_algorithm shadow_map_pointlight_algorithm;
    _scene_renderer_uber_vertex_shader_item     vertex_shader_item;
    shaders_vertex_uber_item_id                 vs_item_id;

    scene_renderer_uber_item_type type;

    _scene_renderer_uber_item()
    {
        falloff                         = SCENE_LIGHT_FALLOFF_UNKNOWN;
        fs_item_id                      = -1;
        is_shadow_caster                = false;
        shadow_map_algorithm            = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
        shadow_map_bias                 = SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN;
        shadow_map_pointlight_algorithm = SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_UNKNOWN;
        type                            = SCENE_RENDERER_UBER_ITEM_UNKNOWN;
        vs_item_id                      = -1;
    }
} _scene_renderer_uber_item;

typedef struct _scene_renderer_uber_mesh_data
{
    ral_gfx_state    gfx_state;
    system_hash64map material_to_mesh_data_material_map; /* maos "mesh_material" to a _scene_renderer_uber_mesh_data_material instance */
    mesh             mesh_instance; /* do NOT release */

    ral_buffer   normals_stream_buffer_ral;
    unsigned int normals_stream_offset;
    unsigned int normals_stream_stride;
    ral_buffer   texcoords_stream_buffer_ral;
    unsigned int texcoords_stream_offset;
    unsigned int texcoords_stream_stride;
    ral_buffer   vertex_stream_buffer_ral;
    unsigned int vertex_stream_offset;
    unsigned int vertex_stream_stride;

    _scene_renderer_uber_mesh_data(ral_context context);
    ~_scene_renderer_uber_mesh_data();

} _scene_renderer_uber_mesh_data;

typedef struct _scene_renderer_uber_mesh_data_material
{
    ral_command_buffer              command_buffer;
    ral_context                     context;
    mesh_material                   material;
    _scene_renderer_uber_mesh_data* mesh_data_ptr;
    system_time                     mesh_modification_timestamp;
    struct _scene_renderer_uber*    uber_ptr;

    ral_texture_view color_rt;
    ral_texture_view depth_rt;

    explicit _scene_renderer_uber_mesh_data_material(ral_context                     in_context,
                                                     mesh_material                   in_material,
                                                     _scene_renderer_uber_mesh_data* in_mesh_data_ptr,
                                                     _scene_renderer_uber*           in_uber_ptr)
    {
        ral_command_buffer_create_info command_buffer_create_info;

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        command_buffer_create_info.is_executable                           = true;
        command_buffer_create_info.is_invokable_from_other_command_buffers = true;
        command_buffer_create_info.is_resettable                           = true;
        command_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(in_context,
                                           1, /* n_command_buffers */
                                           &command_buffer_create_info,
                                           &command_buffer);

        color_rt                    = nullptr;
        context                     = in_context;
        depth_rt                    = nullptr;
        material                    = in_material;
        mesh_data_ptr               = in_mesh_data_ptr;
        mesh_modification_timestamp = 0;
        uber_ptr                    = in_uber_ptr;
    }

    ~_scene_renderer_uber_mesh_data_material()
    {
        if (command_buffer != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&command_buffer) );

            command_buffer = nullptr;
        }
    }
} _scene_renderer_uber_mesh_data_material;

typedef enum
{
    /* Created with scene_renderer_uber_create() */
    SCENE_RENDERER_UBER_TYPE_REGULAR,

    /* Created with scene_renderer_uber_create_from_ral_program() */
    SCENE_RENDERER_UBER_TYPE_RAL_PROGRAM_DRIVEN
} _scene_renderer_uber_type;

typedef struct _scene_renderer_uber
{
    ral_context               context;
    system_hashed_ansi_string name;

    ral_program               program;
    shaders_fragment_uber     shader_fragment;
    uint32_t                  shader_fragment_n_items;
    shaders_vertex_uber       shader_vertex;

    ral_texture_view active_color_rt;
    ral_texture_view active_depth_rt;

    /* These are stored in UBs so we need to store UB offsets instead of locations */
    uint32_t ambient_material_ub_offset;
    uint32_t diffuse_material_ub_offset;
    uint32_t far_near_plane_diff_ub_offset;
    uint32_t flip_z_ub_offset;
    uint32_t luminosity_material_ub_offset;
    uint32_t max_variance_ub_offset;
    uint32_t model_ub_offset;         /* column major ordering! */
    uint32_t near_plane_ub_offset;
    uint32_t normal_matrix_ub_offset; /* column-major ordering! */
    uint32_t shininess_material_ub_offset;
    uint32_t specular_material_ub_offset;
    uint32_t world_camera_ub_offset;
    uint32_t vp_ub_offset;

    bool                      is_rendering;
    ral_program_block_buffer  ub_fs;
    GLuint                    ub_fs_bo_size;
    ral_program_block_buffer  ub_vs;
    GLuint                    ub_vs_bo_size;

    system_matrix4x4          current_vp;
    float                     current_vsm_max_variance;

    system_resizable_vector   added_items; /* holds _scene_renderer_uber_item instances */
    bool                      dirty;

    system_matrix4x4          graph_rendering_current_matrix;
    _scene_renderer_uber_type type;
    system_variant            variant_float;

    system_hash64map        mesh_to_mesh_data_map;
    ral_command_buffer      preamble_command_buffer;
    system_resizable_vector scheduled_mesh_buffers;
    system_resizable_vector scheduled_mesh_command_buffers;

    REFCOUNT_INSERT_VARIABLES

    explicit _scene_renderer_uber(ral_context               context,
                                  system_hashed_ansi_string name,
                                  _scene_renderer_uber_type type);
} _scene_renderer_uber;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scene_renderer_uber,
                               scene_renderer_uber,
                              _scene_renderer_uber);

/** Forward declarations */
PRIVATE void _scene_renderer_uber_release              (void*                 uber);
PRIVATE void _scene_renderer_uber_reset_uniform_offsets(_scene_renderer_uber* uber_ptr);


/** Internal variables */

/** TODO */
_scene_renderer_uber::_scene_renderer_uber(ral_context               in_context,
                                           system_hashed_ansi_string in_name,
                                           _scene_renderer_uber_type in_type)
{
    added_items                    = system_resizable_vector_create(4 /* capacity */);
    context                        = in_context;    /* DO NOT retain, or face circular dependencies! */
    current_vp                     = system_matrix4x4_create();
    dirty                          = true;
    graph_rendering_current_matrix = system_matrix4x4_create();
    is_rendering                   = false;
    mesh_to_mesh_data_map          = system_hash64map_create(sizeof(_scene_renderer_uber_mesh_data*),
                                                             false);
    name                           = in_name;
    program                        = nullptr;
    scheduled_mesh_buffers         = system_resizable_vector_create(16); /* capacity */
    scheduled_mesh_command_buffers = system_resizable_vector_create(16); /* capacity */
    shader_fragment                = nullptr;
    shader_vertex                  = nullptr;
    type                           = in_type;
    ub_fs                          = nullptr;
    ub_vs                          = nullptr;

    _scene_renderer_uber_reset_uniform_offsets(this);

    /* Instantiate the preamble command buffer */
    ral_command_buffer_create_info preamble_cmd_buffer_create_info;

    preamble_cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    preamble_cmd_buffer_create_info.is_executable                           = false;
    preamble_cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    preamble_cmd_buffer_create_info.is_resettable                           = true;
    preamble_cmd_buffer_create_info.is_transient                            = true;

    ral_context_create_command_buffers(context,
                                       1, /* n_command_buffers */
                                       &preamble_cmd_buffer_create_info,
                                       &preamble_command_buffer);
}

/** TODO */
_scene_renderer_uber_mesh_data::_scene_renderer_uber_mesh_data(ral_context context)
{
    gfx_state                          = nullptr;
    material_to_mesh_data_material_map = system_hash64map_create(sizeof(void*) );
    mesh_instance                      = nullptr;

    normals_stream_buffer_ral   = nullptr;
    normals_stream_offset       = 0;
    normals_stream_stride       = 0;
    texcoords_stream_buffer_ral = nullptr;
    texcoords_stream_offset     = 0;
    texcoords_stream_stride     = 0;
    vertex_stream_buffer_ral    = nullptr;
    vertex_stream_offset        = 0;
    vertex_stream_stride        = 0;
}

/** TODO */
_scene_renderer_uber_mesh_data::~_scene_renderer_uber_mesh_data()
{
    if (gfx_state != nullptr)
    {
        ral_context gfx_state_context = nullptr;

        ral_gfx_state_get_property(gfx_state,
                                   RAL_GFX_STATE_PROPERTY_CONTEXT,
                                  &gfx_state_context);
        ral_context_delete_objects(gfx_state_context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&gfx_state) );

        gfx_state = nullptr;
    }

    if (material_to_mesh_data_material_map != nullptr)
    {
        _scene_renderer_uber_mesh_data_material* material_data_ptr = nullptr;
        system_hash64                            material_hash;

        while (system_hash64map_get_element_at(material_to_mesh_data_material_map,
                                               0, /* n_element */
                                               &material_data_ptr,
                                               &material_hash))
        {
            delete material_data_ptr;

            system_hash64map_remove(material_to_mesh_data_material_map,
                                    material_hash);
        }

        system_hash64map_release(material_to_mesh_data_material_map);
        material_to_mesh_data_material_map = nullptr;
    }
}

/** TODO */
PRIVATE void _scene_renderer_uber_add_item_shaders_fragment_callback_handler(_shaders_fragment_uber_parent_callback_type type,
                                                                             void*                                       data,
                                                                             void*                                       uber)
{
    switch (type)
    {
        case SHADERS_FRAGMENT_UBER_PARENT_CALLBACK_NEW_FRAGMENT_INPUT:
        {
            const _shaders_fragment_uber_new_fragment_input_callback* callback_data = reinterpret_cast<const _shaders_fragment_uber_new_fragment_input_callback*>(data);

            ASSERT_DEBUG_SYNC(callback_data != nullptr,
                              "Callback data pointer is nullptr");

            /* Fragment shader uses an input attribute. Update the vertex shader so that
             * the data is taken in vertex shader stage and patched through to the fragment
             * shader.
             */
            shaders_vertex_uber_add_passthrough_input_attribute( reinterpret_cast<_scene_renderer_uber*>(uber)->shader_vertex,
                                                                callback_data->vs_attribute_name,
                                                                callback_data->fs_attribute_type,
                                                                callback_data->fs_attribute_name);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized callback from shaders_fragment_uber object");
        }
    }
}

/** TODO */
PRIVATE void _scene_renderer_uber_bake_mesh_data(_scene_renderer_uber*            uber_ptr,
                                                 mesh                             mesh,
                                                 _scene_renderer_uber_mesh_data*  mesh_data_ptr,
                                                 const ral_gfx_state_create_info* ref_gfx_state_create_info_ptr)
{
    const ral_program_attribute* mesh_normal_attribute_ral_ptr    = nullptr;
    const ral_program_attribute* mesh_texcoords_attribute_ral_ptr = nullptr;
    unsigned int                 mesh_texcoords_bo_n_components   = 2;
    const ral_program_attribute* mesh_vertex_attribute_ral_ptr    = nullptr;
    unsigned int                 mesh_vertex_bo_n_components      = 3;

    mesh_type                                   mesh_instance_type;
    ral_primitive_type                          mesh_primitive_type = RAL_PRIMITIVE_TYPE_UNKNOWN;
    std::vector<ral_gfx_state_vertex_attribute> mesh_vas(8);
    uint32_t                                    n_layers            = 0;

    LOG_INFO("Performance warning: _scene_renderer_uber_bake_mesh_data() invoked");

    /* Sanity checks */
    mesh_get_property(mesh,
                      MESH_PROPERTY_TYPE,
                     &mesh_instance_type);

    if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
    {
        unsigned int n_mesh_layers = 0;

        mesh_get_property(mesh,
                          MESH_PROPERTY_N_LAYERS,
                         &n_mesh_layers);

        ASSERT_DEBUG_SYNC(n_mesh_layers <= 1,
                          "TODO");
    }

    /* Retrieve buffer object storage properties. */
    if (mesh_instance_type == MESH_TYPE_REGULAR)
    {
        /* Regular meshes use the same stride and BO ID for all streams */
        mesh_get_property(mesh,
                          MESH_PROPERTY_BO_RAL,
                         &mesh_data_ptr->texcoords_stream_buffer_ral);
        mesh_get_property(mesh,
                          MESH_PROPERTY_BO_STRIDE,
                         &mesh_data_ptr->texcoords_stream_stride);

        mesh_data_ptr->normals_stream_buffer_ral = mesh_data_ptr->texcoords_stream_buffer_ral;
        mesh_data_ptr->normals_stream_stride     = mesh_data_ptr->texcoords_stream_stride;
        mesh_data_ptr->vertex_stream_buffer_ral  = mesh_data_ptr->texcoords_stream_buffer_ral;
        mesh_data_ptr->vertex_stream_stride      = mesh_data_ptr->texcoords_stream_stride;
    }
    else
    {
        mesh_data_ptr->normals_stream_buffer_ral   = nullptr;
        mesh_data_ptr->normals_stream_stride       = -1;
        mesh_data_ptr->texcoords_stream_buffer_ral = nullptr;
        mesh_data_ptr->texcoords_stream_stride     = -1;
        mesh_data_ptr->vertex_stream_buffer_ral    = nullptr;
        mesh_data_ptr->vertex_stream_stride        = -1;
    }

    if (ral_program_get_vertex_attribute_by_name(uber_ptr->program,
                                                 system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_normal),
                                                 &mesh_normal_attribute_ral_ptr) )
    {
        ral_gfx_state_vertex_attribute normal_data_va;

        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_data_ptr->normals_stream_offset);

        if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
        {
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,
                                               &mesh_data_ptr->normals_stream_buffer_ral);
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL_STRIDE,
                                               &mesh_data_ptr->normals_stream_stride);
        }

        normal_data_va.format     = RAL_FORMAT_RGB32_FLOAT;
        normal_data_va.input_rate = RAL_VERTEX_INPUT_RATE_PER_VERTEX;
        normal_data_va.name       = system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_normal);
        normal_data_va.offset     = mesh_data_ptr->normals_stream_offset;
        normal_data_va.stride     = mesh_data_ptr->normals_stream_stride;

        mesh_vas.at(mesh_normal_attribute_ral_ptr->location) = normal_data_va;
    }

    if (ral_program_get_vertex_attribute_by_name(uber_ptr->program,
                                                 system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv),
                                                 &mesh_texcoords_attribute_ral_ptr) )
    {
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_data_ptr->texcoords_stream_offset);

        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,
                                           &mesh_texcoords_bo_n_components);

        if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
        {
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,
                                               &mesh_data_ptr->texcoords_stream_buffer_ral);
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL_STRIDE,
                                               &mesh_data_ptr->texcoords_stream_stride);
        }
    }

    if (ral_program_get_vertex_attribute_by_name(uber_ptr->program,
                                                 system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_vertex),
                                                 &mesh_vertex_attribute_ral_ptr) )
    {
        ral_gfx_state_vertex_attribute vertex_data_va;

        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_data_ptr->vertex_stream_offset);
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,
                                           &mesh_vertex_bo_n_components);

        ASSERT_DEBUG_SYNC(mesh_vertex_bo_n_components >= 3,
                          "Format specified for vertex data in vertex_data_va below is invalid");

        if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
        {
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,
                                               &mesh_data_ptr->vertex_stream_buffer_ral);
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL_STRIDE,
                                               &mesh_data_ptr->vertex_stream_stride);
        }

        vertex_data_va.format     = (mesh_vertex_bo_n_components == 3) ? RAL_FORMAT_RGB32_FLOAT
                                                                       : RAL_FORMAT_RGBA32_FLOAT;
        vertex_data_va.input_rate = RAL_VERTEX_INPUT_RATE_PER_VERTEX;
        vertex_data_va.name       = system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_vertex);
        vertex_data_va.offset     = mesh_data_ptr->vertex_stream_offset;
        vertex_data_va.stride     = mesh_data_ptr->vertex_stream_stride;

        mesh_vas.at(mesh_vertex_attribute_ral_ptr->location) = vertex_data_va;
    }

    /* A few sanity checks never hurt anyone.. */
    ASSERT_ALWAYS_SYNC(mesh_data_ptr->vertex_stream_buffer_ral != nullptr,
                      "Mesh BO is nullptr");

    /* Iterate over all layers.. */
    mesh_get_property(mesh,
                      MESH_PROPERTY_N_LAYERS,
                     &n_layers);

    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        const uint32_t n_layer_passes = mesh_get_number_of_layer_passes(mesh,
                                                                        n_layer);

        /* Set up attribute data */
        struct _attribute_item
        {
            system_hashed_ansi_string attribute_name;
            uint32_t                  offset;
            int                       size;
        } attribute_items[] =
        {
            {
                system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv),
                mesh_data_ptr->texcoords_stream_offset,
                2
            },
        };
        const unsigned int n_attribute_items = sizeof(attribute_items) / sizeof(attribute_items[0]);

        for (unsigned int n_attribute_item = 0;
                          n_attribute_item < n_attribute_items;
                        ++n_attribute_item)
        {
            _attribute_item& item = attribute_items[n_attribute_item];

            if (item.attribute_name                        != nullptr                                              &&
                item.attribute_name                        != system_hashed_ansi_string_get_default_empty_string() &&
                mesh_texcoords_attribute_ral_ptr           != nullptr                                              &&
                mesh_data_ptr->texcoords_stream_buffer_ral != nullptr)
            {
                ral_gfx_state_vertex_attribute uv_data_va;

                ASSERT_DEBUG_SYNC(item.size == 2,
                                  "Format used by uv_data_va is invalid.");

                uv_data_va.format     = RAL_FORMAT_RG32_FLOAT;
                uv_data_va.input_rate = RAL_VERTEX_INPUT_RATE_PER_VERTEX;
                uv_data_va.name       = item.attribute_name;
                uv_data_va.offset     = item.offset;
                uv_data_va.stride     = mesh_data_ptr->texcoords_stream_stride;

                mesh_vas.at(mesh_texcoords_attribute_ral_ptr->location) = uv_data_va;
            }
        }

        /* Iterate over all layer passes */
        for (uint32_t n_layer_pass = 0;
                      n_layer_pass < n_layer_passes;
                    ++n_layer_pass)
        {
            mesh_draw_call_arguments layer_pass_draw_call_args;
            mesh_material            layer_pass_material = nullptr;

            /* Retrieve layer pass properties */
            if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
            {
                mesh_get_layer_pass_property(mesh,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_DRAW_CALL_ARGUMENTS,
                                            &layer_pass_draw_call_args);

                ASSERT_DEBUG_SYNC(mesh_primitive_type == RAL_PRIMITIVE_TYPE_UNKNOWN                ||
                                  mesh_primitive_type == layer_pass_draw_call_args.primitive_type,
                                  "All layer passes need to use the same primitive type for RAL to work correctly.");

                mesh_primitive_type = (layer_pass_draw_call_args.primitive_type == RAL_PRIMITIVE_TYPE_UNKNOWN) ? RAL_PRIMITIVE_TYPE_TRIANGLES
                                                                                                               : layer_pass_draw_call_args.primitive_type;
            }
            else
            {
                mesh_primitive_type = RAL_PRIMITIVE_TYPE_TRIANGLES;
            }

            mesh_get_layer_pass_property(mesh,
                                         n_layer,
                                         n_layer_pass,
                                         MESH_LAYER_PROPERTY_MATERIAL,
                                        &layer_pass_material);

            /* Bind shading data for each supported shading property */
            struct _attachment
            {
                mesh_material_shading_property property;
                system_hashed_ansi_string      shader_uv_attribute_name;
            } attachments[] =
            {
                {
                    MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                    system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv)
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                    system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv)
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                    system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv)
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                    system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv)
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                    system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv)
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
                        if (attachment.shader_uv_attribute_name != nullptr &&
                            mesh_texcoords_attribute_ral_ptr    != nullptr)
                        {
                            ral_gfx_state_vertex_attribute uv_data_va;

                            ASSERT_DEBUG_SYNC(mesh_data_ptr->texcoords_stream_buffer_ral != nullptr,
                                              "Material requires texture coordinates, but none are provided by the mesh.");
                            ASSERT_DEBUG_SYNC(mesh_texcoords_bo_n_components == 2,
                                              "Format used by uv_data_va is invalid.");

                            uv_data_va.format     = RAL_FORMAT_RG32_FLOAT;
                            uv_data_va.input_rate = RAL_VERTEX_INPUT_RATE_PER_VERTEX;
                            uv_data_va.name       = attachment.shader_uv_attribute_name;
                            uv_data_va.offset     = mesh_data_ptr->texcoords_stream_offset;
                            uv_data_va.stride     = mesh_data_ptr->texcoords_stream_stride;

                            mesh_vas.at(mesh_texcoords_attribute_ral_ptr->location) = uv_data_va;
                        }

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized material property attachment");
                    }
                }
            }
        }
    }

    /* Clone user-specified gfx state and update mesh-specific fields */
    mesh_vertex_ordering       vertex_ordering;
    ral_gfx_state_create_info* new_gfx_state_create_info_ptr = ral_gfx_state_create_info::clone(ref_gfx_state_create_info_ptr,
                                                                                                true,   /* should_include_static_scissor_box_data */
                                                                                                true,   /* should_include_static_viewport_data    */
                                                                                                false); /* should_include_va_data                 */

    mesh_get_property(mesh,
                      MESH_PROPERTY_VERTEX_ORDERING,
                     &vertex_ordering);

    new_gfx_state_create_info_ptr->culling               = true;
    new_gfx_state_create_info_ptr->front_face            = (vertex_ordering == MESH_VERTEX_ORDERING_CCW) ? RAL_FRONT_FACE_CCW
                                                                                                         : RAL_FRONT_FACE_CW;
    new_gfx_state_create_info_ptr->n_vertex_attributes   = static_cast<uint32_t>(mesh_vas.size());
    new_gfx_state_create_info_ptr->primitive_type        = mesh_primitive_type;
    new_gfx_state_create_info_ptr->vertex_attribute_ptrs = (new_gfx_state_create_info_ptr->n_vertex_attributes > 0) ? new ral_gfx_state_vertex_attribute[new_gfx_state_create_info_ptr->n_vertex_attributes]
                                                                                                                    : nullptr;

    if (new_gfx_state_create_info_ptr->n_vertex_attributes > 0)
    {
        memcpy(new_gfx_state_create_info_ptr->vertex_attribute_ptrs,
              &mesh_vas[0],
               sizeof(*new_gfx_state_create_info_ptr->vertex_attribute_ptrs) * new_gfx_state_create_info_ptr->n_vertex_attributes);
    }

    /* Cache the new gfx state and leave. */
    if (mesh_data_ptr->gfx_state != nullptr)
    {
        ral_context_delete_objects(uber_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&mesh_data_ptr->gfx_state) );

        mesh_data_ptr->gfx_state = nullptr;
    }

    ral_context_create_gfx_states(uber_ptr->context,
                                  1, /* n_create_info_items */
                                  new_gfx_state_create_info_ptr,
                                 &mesh_data_ptr->gfx_state);

    mesh_data_ptr->mesh_instance = mesh;

    ral_gfx_state_create_info::release(new_gfx_state_create_info_ptr);
    new_gfx_state_create_info_ptr = nullptr;
}

/** TODO */
PRIVATE void _scene_renderer_uber_release(void* uber)
{
    _scene_renderer_uber* uber_ptr = reinterpret_cast<_scene_renderer_uber*>(uber);

    if (uber_ptr != nullptr)
    {
        if (uber_ptr->added_items != nullptr)
        {
            _scene_renderer_uber_item* item_ptr = nullptr;

            while (system_resizable_vector_pop(uber_ptr->added_items,
                                              &item_ptr) )
            {
                delete item_ptr;

                item_ptr = nullptr;
            }

            system_resizable_vector_release(uber_ptr->added_items);
            uber_ptr->added_items = nullptr;
        }

        if (uber_ptr->current_vp != nullptr)
        {
            system_matrix4x4_release(uber_ptr->current_vp);

            uber_ptr->current_vp = nullptr;
        }

        if (uber_ptr->graph_rendering_current_matrix != nullptr)
        {
            system_matrix4x4_release(uber_ptr->graph_rendering_current_matrix);

            uber_ptr->graph_rendering_current_matrix = nullptr;
        }

        if (uber_ptr->scheduled_mesh_buffers != nullptr)
        {
            #ifdef _DEBUG
            {
                uint32_t n_buffers = 0;

                system_resizable_vector_get_property(uber_ptr->scheduled_mesh_buffers,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_buffers);

                ASSERT_DEBUG_SYNC(n_buffers == 0,
                                  "Number of scheduled buffers > 0 at destruction time.");
            }
            #endif

            system_resizable_vector_release(uber_ptr->scheduled_mesh_buffers);

            uber_ptr->scheduled_mesh_buffers = nullptr;
        }

        if (uber_ptr->scheduled_mesh_command_buffers != nullptr)
        {
            #ifdef _DEBUG
            {
                uint32_t n_cmd_buffers = 0;

                system_resizable_vector_get_property(uber_ptr->scheduled_mesh_command_buffers,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_cmd_buffers);

                ASSERT_DEBUG_SYNC(n_cmd_buffers == 0,
                                  "Number of scheduled cmd buffers > 0 at destruction time.");
            }
            #endif

            system_resizable_vector_release(uber_ptr->scheduled_mesh_command_buffers);

            uber_ptr->scheduled_mesh_command_buffers = nullptr;
        }

        if (uber_ptr->shader_fragment != nullptr)
        {
            shaders_fragment_uber_release(uber_ptr->shader_fragment);

            uber_ptr->shader_fragment = nullptr;
        }

        if (uber_ptr->shader_vertex != nullptr)
        {
            shaders_vertex_uber_release(uber_ptr->shader_vertex);

            uber_ptr->shader_vertex = nullptr;
        }

        if (uber_ptr->preamble_command_buffer != nullptr)
        {
            ral_context_delete_objects(uber_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&uber_ptr->preamble_command_buffer) );

            uber_ptr->preamble_command_buffer = nullptr;
        }

        if (uber_ptr->program != nullptr)
        {
            ral_context_delete_objects(uber_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&uber_ptr->program) );

            uber_ptr->program = nullptr;
        }

        if (uber_ptr->ub_fs != nullptr)
        {
            ral_program_block_buffer_release(uber_ptr->ub_fs);

            uber_ptr->ub_fs = nullptr;
        }

        if (uber_ptr->ub_vs != nullptr)
        {
            ral_program_block_buffer_release(uber_ptr->ub_vs);

            uber_ptr->ub_vs = nullptr;
        }

        if (uber_ptr->mesh_to_mesh_data_map != nullptr)
        {
            system_hash64                   mesh_data_hash;
            _scene_renderer_uber_mesh_data* mesh_data_ptr = nullptr;

            while (system_hash64map_get_element_at(uber_ptr->mesh_to_mesh_data_map,
                                                   0,
                                                  &mesh_data_ptr,
                                                  &mesh_data_hash) )
            {
                delete mesh_data_ptr;

                /* Move on */
                system_hash64map_remove(uber_ptr->mesh_to_mesh_data_map,
                                        mesh_data_hash);
            }

            system_hash64map_release(uber_ptr->mesh_to_mesh_data_map);
            uber_ptr->mesh_to_mesh_data_map = nullptr;
        }

        if (uber_ptr->variant_float != nullptr)
        {
            system_variant_release(uber_ptr->variant_float);

            uber_ptr->variant_float = nullptr;
        }
    }
}

/** TODO */
PRIVATE void _scene_renderer_uber_reset_uniform_offsets(_scene_renderer_uber* uber_ptr)
{
    uber_ptr->ambient_material_ub_offset    = -1;
    uber_ptr->diffuse_material_ub_offset    = -1;
    uber_ptr->far_near_plane_diff_ub_offset = -1;
    uber_ptr->flip_z_ub_offset              = -1;
    uber_ptr->luminosity_material_ub_offset = -1;
    uber_ptr->max_variance_ub_offset        = -1;
    uber_ptr->model_ub_offset               = -1;
    uber_ptr->near_plane_ub_offset          = -1;
    uber_ptr->normal_matrix_ub_offset       = -1;
    uber_ptr->shininess_material_ub_offset  = -1;
    uber_ptr->specular_material_ub_offset   = -1;
    uber_ptr->vp_ub_offset                  = -1;
    uber_ptr->world_camera_ub_offset        = -1;
}

/** TODO */
PRIVATE void _scene_renderer_uber_start_rendering_cpu_task_callback(void* uber_raw_ptr)
{
    _scene_renderer_uber* uber_ptr = reinterpret_cast<_scene_renderer_uber*>(uber_raw_ptr);

    if (uber_ptr->max_variance_ub_offset != -1)
    {
        ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                               uber_ptr->max_variance_ub_offset,
                                                              &uber_ptr->current_vsm_max_variance,
                                                               sizeof(float) );
    }

    /* If any part of the SH data comes from a BO, copy it now
     *
     * TODO: SH support has become deprecated.
     */
    #if 0
        for (unsigned int n_item = 0;
                          n_item < n_items;
                        ++n_item)
        {
            _scene_renderer_uber_item* item_ptr = nullptr;

            if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                       n_item,
                                                      &item_ptr) )
            {
                switch (item_ptr->type)
                {
                    case SCENE_RENDERER_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE:
                    {
                        /* Not relevant */
                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_LIGHT:
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
                                ASSERT_DEBUG_SYNC(false,
                                                  "TODO");
                                const unsigned int sh_data_size = (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS) ? 4 * sizeof(float) * 9
                                                                                                                       : 4 * sizeof(float) * 12;

                                dsa_entry_points->pGLNamedCopyBufferSubDataEXT(item_ptr->vertex_shader_item.current_light_sh_data.bo_id,
                                                                               uber_ptr->ubo_id,
                                                                               item_ptr->vertex_shader_item.current_light_sh_data.bo_offset,
                                                                               uber_ptr->ubo_start_offset + uber_ptr->ubo_data_vertex_offset + item_ptr->vertex_shader_item.current_light_sh_data_ub_offset,
                                                                               sh_data_size);

                                break;
                            }

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Unrecognized light type at index [%d]",
                                                  n_item);
                            }
                        }

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized vertex shader item type");
                    }
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve uber item descriptor at index [%d]",
                                  n_item);
            }
        }
    #endif

    /* Sync the UBOs */
    if (uber_ptr->ub_fs != nullptr)
    {
        ral_program_block_buffer_sync_immediately(uber_ptr->ub_fs);
    }
}


/* Please see header for specification */
PUBLIC scene_renderer_uber_item_id scene_renderer_uber_add_input_fragment_attribute_item(scene_renderer_uber                          uber,
                                                                                         scene_renderer_uber_input_fragment_attribute input_attribute)
{
    shaders_fragment_uber_input_attribute_type fs_input_attribute = UBER_INPUT_ATTRIBUTE_UNKNOWN;
    shaders_fragment_uber_item_id              fs_item_id         = -1;
    scene_renderer_uber_item_id                result             = -1;
    _scene_renderer_uber*                      uber_ptr           = reinterpret_cast<_scene_renderer_uber*>(uber);

    ASSERT_DEBUG_SYNC(uber_ptr->type == SCENE_RENDERER_UBER_TYPE_REGULAR,
                      "scene_renderer_uber_add_input_fragment_attribute_item() is only supported for regular scene_renderer_uber instances.");

    switch (input_attribute)
    {
        case SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL:
        {
            fs_input_attribute = UBER_INPUT_ATTRIBUTE_NORMAL;

            break;
        }

        case SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD:
        {
            fs_input_attribute = UBER_INPUT_ATTRIBUTE_TEXCOORD;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized input attribute");
        }
    }

    /* Update fragment shader instance */
    fs_item_id = shaders_fragment_uber_add_input_attribute_contribution(uber_ptr->shader_fragment,
                                                                        fs_input_attribute,
                                                                        _scene_renderer_uber_add_item_shaders_fragment_callback_handler,
                                                                        uber);

    /* Spawn a new descriptor */
    _scene_renderer_uber_item* new_item_ptr = new (std::nothrow) _scene_renderer_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != nullptr,
                       "Out of memory");

    if (new_item_ptr == nullptr)
    {
        goto end;
    }

    new_item_ptr->type = SCENE_RENDERER_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result);

    /* Add the descriptor to the added items vector */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    /* Mark uber instance as dirty */
    uber_ptr->dirty = true;

end:
    return result;
}

/* Please see header for specification */
PUBLIC scene_renderer_uber_item_id scene_renderer_uber_add_light_item(scene_renderer_uber              uber,
                                                                      scene_light                      light_instance,
                                                                      shaders_fragment_uber_light_type light_type,
                                                                      bool                             is_shadow_caster,
                                                                      unsigned int                     n_light_properties,
                                                                      void*                            light_property_values)
{
    _scene_renderer_uber*       uber_ptr     = reinterpret_cast<_scene_renderer_uber*>(uber);
    _scene_renderer_uber_item*  new_item_ptr = nullptr;
    scene_renderer_uber_item_id result       = -1;

    ASSERT_DEBUG_SYNC(uber_ptr->type == SCENE_RENDERER_UBER_TYPE_REGULAR,
                      "scene_renderer_uber_add_light_item() is only supported for regular scene_renderer_uber instances.");

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
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT:
        {
            fs_item_id = shaders_fragment_uber_add_light(uber_ptr->shader_fragment,
                                                         light_type,
                                                         light_instance,
                                                         is_shadow_caster,
                                                         n_light_properties,
                                                         light_property_values,
                                                         _scene_renderer_uber_add_item_shaders_fragment_callback_handler,
                                                         uber);
            vs_item_id = shaders_vertex_uber_add_light  (uber_ptr->shader_vertex,
                                                         SHADERS_VERTEX_UBER_LIGHT_NONE,
                                                         light_type,
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
                                                         light_instance,
                                                         is_shadow_caster,
                                                         n_light_properties,
                                                         light_property_values,
                                                         nullptr, /* callback proc - not used */
                                                         nullptr  /* callback proc user arg - not used */);
            vs_item_id = shaders_vertex_uber_add_light  (uber_ptr->shader_vertex,
                                                         vs_light,
                                                         light_type,
                                                         is_shadow_caster);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized uber light type");
        }
    }

    /* Spawn the descriptor */
    new_item_ptr = new (std::nothrow) _scene_renderer_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != nullptr,
                       "Out of memory");

    if (new_item_ptr == nullptr)
    {
        goto end;
    }

    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS,
                            &new_item_ptr->shadow_map_bias);
    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,
                            &new_item_ptr->shadow_map_algorithm);
    scene_light_get_property(light_instance,
                             SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                            &new_item_ptr->shadow_map_pointlight_algorithm);

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT   ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        scene_light_get_property(light_instance,
                                 SCENE_LIGHT_PROPERTY_FALLOFF,
                                &new_item_ptr->falloff);
    }

    new_item_ptr->fs_item_id       = fs_item_id;
    new_item_ptr->is_shadow_caster = is_shadow_caster;
    new_item_ptr->type             = SCENE_RENDERER_UBER_ITEM_LIGHT;
    new_item_ptr->vs_item_id       = vs_item_id;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result);

    /* Add the descriptor to the added items vector */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    /* Mark uber instance as dirty */
    uber_ptr->dirty = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC scene_renderer_uber scene_renderer_uber_create(ral_context                context,
                                                      system_hashed_ansi_string  name)
{
    _scene_renderer_uber* result_ptr = new (std::nothrow) _scene_renderer_uber(context,
                                                                               name,
                                                                               SCENE_RENDERER_UBER_TYPE_REGULAR);

    ASSERT_DEBUG_SYNC(result_ptr != nullptr,
                      "Out of memory");

    if (result_ptr != nullptr)
    {
        result_ptr->shader_fragment = shaders_fragment_uber_create(context,
                                                                   name);
        result_ptr->shader_vertex   = shaders_vertex_uber_create  (context,
                                                                   name);
        result_ptr->type            = SCENE_RENDERER_UBER_TYPE_REGULAR;
        result_ptr->variant_float   = system_variant_create       (SYSTEM_VARIANT_FLOAT);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                       _scene_renderer_uber_release,
                                                       OBJECT_TYPE_SCENE_RENDERER_UBER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Renderer Ubers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        /* Create a program with the shaders we were provided */
        ral_program_create_info program_create_info;

        program_create_info.active_shader_stages = RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX;
        program_create_info.name                 = name;

        ral_context_create_programs(context,
                                    1, /* n_create_info_items */
                                   &program_create_info,
                                   &result_ptr->program);

        ASSERT_ALWAYS_SYNC(result_ptr->program != nullptr,
                           "Cannot instantiate uber program");

        if (result_ptr->program != nullptr)
        {
            if (!ral_program_attach_shader(result_ptr->program,
                                           shaders_fragment_uber_get_shader(result_ptr->shader_fragment),
                                           true /* async */) ||
                !ral_program_attach_shader(result_ptr->program,
                                           shaders_vertex_uber_get_shader(result_ptr->shader_vertex),
                                           true /* async */) )
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Cannot attach shader(s) to uber program");
            }
        }
    }

    return reinterpret_cast<scene_renderer_uber>(result_ptr);
}

/* Please see header for specification */
PUBLIC scene_renderer_uber scene_renderer_uber_create_from_ral_program(ral_context               context,
                                                                       system_hashed_ansi_string name,
                                                                       ral_program               program)
{
    _scene_renderer_uber* result_ptr = new (std::nothrow) _scene_renderer_uber(context,
                                                                               name,
                                                                               SCENE_RENDERER_UBER_TYPE_RAL_PROGRAM_DRIVEN);

    ASSERT_DEBUG_SYNC(result_ptr != nullptr,
                      "Out of memory");

    if (result_ptr != nullptr)
    {
        /* Cache the input program */
        ASSERT_DEBUG_SYNC(program != nullptr,
                          "Input program is nullptr");

        result_ptr->program = program;
        result_ptr->type    = SCENE_RENDERER_UBER_TYPE_RAL_PROGRAM_DRIVEN;

        ral_context_retain_object(context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  program);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                       _scene_renderer_uber_release,
                                                       OBJECT_TYPE_SCENE_RENDERER_UBER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scene Renderer Ubers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scene_renderer_uber) result_ptr;
}

/* Please see header for specification */
PUBLIC void scene_renderer_uber_get_shader_general_property(const scene_renderer_uber            uber,
                                                            scene_renderer_uber_general_property property,
                                                            void*                                out_result_ptr)
{
    const _scene_renderer_uber* uber_ptr = reinterpret_cast<const _scene_renderer_uber*>(uber);

    switch (property)
    {
        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = uber_ptr->name;

            break;
        }

        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_N_ITEMS:
        {
            ASSERT_DEBUG_SYNC(uber_ptr->type == SCENE_RENDERER_UBER_TYPE_REGULAR,
                              "SCENE_RENDERER_UBER_GENERAL_PROPERTY_N_ITEMS query is only supported for regular scene_renderer_uber instances.");

            system_resizable_vector_get_property(uber_ptr->added_items,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized general scene_renderer_uber property");
        }
    }
}

/* Please see header for specification */
PUBLIC void scene_renderer_uber_get_shader_item_property(const scene_renderer_uber         uber,
                                                         scene_renderer_uber_item_id       item_id,
                                                         scene_renderer_uber_item_property property,
                                                         void*                             out_result_ptr)
{
    _scene_renderer_uber_item*  item_ptr = nullptr;
    const _scene_renderer_uber* uber_ptr = reinterpret_cast<const _scene_renderer_uber*>(uber);

    ASSERT_DEBUG_SYNC(uber_ptr->type == SCENE_RENDERER_UBER_TYPE_REGULAR,
                      "scene_renderer_uber_get_shader_item_property() is only supported for regular scene_renderer_uber instances.");

    if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                               item_id,
                                              &item_ptr) )
    {
        switch (property)
        {
            case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_FALLOFF:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == SCENE_RENDERER_UBER_ITEM_LIGHT,
                                  "Invalid SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_FALLOFF request");

                *reinterpret_cast<scene_light_falloff*>(out_result_ptr) = item_ptr->falloff;

                break;
            }

            case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == SCENE_RENDERER_UBER_ITEM_LIGHT,
                                  "Invalid SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM request");

                *reinterpret_cast<scene_light_shadow_map_algorithm*>(out_result_ptr) = item_ptr->shadow_map_algorithm;

                break;
            }

            case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == SCENE_RENDERER_UBER_ITEM_LIGHT,
                                  "Invalid SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS request");

                *reinterpret_cast<scene_light_shadow_map_bias*>(out_result_ptr) = item_ptr->shadow_map_bias;

                break;
            }

            case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == SCENE_RENDERER_UBER_ITEM_LIGHT,
                                  "Invalid SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM request");

                *reinterpret_cast<scene_light_shadow_map_pointlight_algorithm*>(out_result_ptr) = item_ptr->shadow_map_pointlight_algorithm;

                break;
            }

            case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == SCENE_RENDERER_UBER_ITEM_LIGHT,
                                  "Invalid SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP request");

                *reinterpret_cast<bool*>(out_result_ptr) = item_ptr->is_shadow_caster;

                break;
            }

            case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_TYPE:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == SCENE_RENDERER_UBER_ITEM_LIGHT,
                                  "Invalid SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_TYPE request");

                shaders_fragment_uber_get_light_item_properties(uber_ptr->shader_fragment,
                                                                item_ptr->fs_item_id,
                                                                reinterpret_cast<shaders_fragment_uber_light_type*>(out_result_ptr) );

                break;
            }

            case SCENE_RENDERER_UBER_ITEM_PROPERTY_TYPE:
            {
                *reinterpret_cast<scene_renderer_uber_item_type*>(out_result_ptr) = item_ptr->type;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized uber item property requested");
            }
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve descriptor of uber shader item at index [%d]",
                          item_id);
    }
}

/** TODO */
PUBLIC void scene_renderer_uber_link(scene_renderer_uber uber)
{
    const ral_program_variable* ambient_material_uniform_ral_ptr     = nullptr;
    const ral_program_variable* diffuse_material_uniform_ral_ptr     = nullptr;
    const ral_program_variable* emission_material_uniform_ral_ptr    = nullptr;
    const ral_program_variable* far_near_plane_diff_uniform_ral_ptr  = nullptr;
    const ral_program_variable* flip_z_uniform_ral_ptr               = nullptr;
    const ral_program_variable* luminosity_material_uniform_ral_ptr  = nullptr;
    const ral_program_variable* max_variance_uniform_ral_ptr         = nullptr;
    const ral_program_variable* model_uniform_ral_ptr                = nullptr;
    const ral_program_variable* near_plane_uniform_ral_ptr           = nullptr;
    unsigned int                n_items                              = 0;
    const ral_program_variable* normal_matrix_uniform_ral_ptr        = nullptr;
    raGL_program                program_raGL                         = nullptr;
    const ral_program_variable* shininess_material_uniform_ral_ptr   = nullptr;
    const ral_program_variable* specular_material_uniform_ral_ptr    = nullptr;
    _scene_renderer_uber*       uber_ptr                             = reinterpret_cast<_scene_renderer_uber*>(uber);
    const ral_program_variable* vp_uniform_ral_ptr                   = nullptr;
    const ral_program_variable* world_camera_uniform_ral_ptr         = nullptr;

    const system_hashed_ansi_string ub_fs_block_name = system_hashed_ansi_string_create(_scene_renderer_uber_block_name_ub_fs);
    const system_hashed_ansi_string ub_vs_block_name = system_hashed_ansi_string_create(_scene_renderer_uber_block_name_ub_vs);

    /* Bail out if no need to link */
    if (!uber_ptr->dirty)
    {
        goto end;
    }

    /* Recompile shaders if needed */
    if (uber_ptr->type == SCENE_RENDERER_UBER_TYPE_REGULAR)
    {
        const bool is_fs_dirty = shaders_fragment_uber_is_dirty(uber_ptr->shader_fragment);
        const bool is_vs_dirty = shaders_vertex_uber_is_dirty  (uber_ptr->shader_vertex);

        if (is_fs_dirty)
        {
            shaders_fragment_uber_recompile(uber_ptr->shader_fragment);
        }

        if (is_vs_dirty)
        {
            shaders_vertex_uber_recompile(uber_ptr->shader_vertex);
        }
    }


    /* Retrieve uniform offsets */
    _scene_renderer_uber_reset_uniform_offsets(uber_ptr);

    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("ambient_material"),
                                          &ambient_material_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("diffuse_material"),
                                          &diffuse_material_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("emission_material"),
                                          &emission_material_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("far_near_plane_diff"),
                                          &far_near_plane_diff_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("flip_z"),
                                          &flip_z_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("luminosity_material"),
                                          &luminosity_material_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("max_variance"),
                                          &max_variance_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("model"),
                                          &model_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("near_plane"),
                                          &near_plane_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("normal_matrix"),
                                          &normal_matrix_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("shininess_material"),
                                          &shininess_material_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_fs_block_name,
                                           system_hashed_ansi_string_create("specular_material"),
                                          &specular_material_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("world_camera"),
                                          &world_camera_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(uber_ptr->program,
                                           ub_vs_block_name,
                                           system_hashed_ansi_string_create("vp"),
                                          &vp_uniform_ral_ptr);

    if (ambient_material_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(ambient_material_uniform_ral_ptr->block_offset != -1,
                          "Ambient material UB offset is -1");

        uber_ptr->ambient_material_ub_offset = ambient_material_uniform_ral_ptr->block_offset;
    }

    if (diffuse_material_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(diffuse_material_uniform_ral_ptr->block_offset != -1,
                          "Diffuse material UB offset is -1");

        uber_ptr->diffuse_material_ub_offset = diffuse_material_uniform_ral_ptr->block_offset;
    }

    if (far_near_plane_diff_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(far_near_plane_diff_uniform_ral_ptr->block_offset != -1,
                          "Far/near plane diff UB offset is -1");

        uber_ptr->far_near_plane_diff_ub_offset = far_near_plane_diff_uniform_ral_ptr->block_offset;
    }

    if (flip_z_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(flip_z_uniform_ral_ptr->block_offset != -1,
                          "Flip Z UB offset is -1");

        uber_ptr->flip_z_ub_offset = flip_z_uniform_ral_ptr->block_offset;
    }

    if (luminosity_material_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(luminosity_material_uniform_ral_ptr->block_offset != -1,
                          "Luminosity material UB offset is -1.");

        uber_ptr->luminosity_material_ub_offset = luminosity_material_uniform_ral_ptr->block_offset;
    }

    if (max_variance_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(max_variance_uniform_ral_ptr->block_offset != -1,
                          "Max variance UB offset is -1");

        uber_ptr->max_variance_ub_offset = max_variance_uniform_ral_ptr->block_offset;
    }

    if (model_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(model_uniform_ral_ptr->block_offset != -1,
                          "Model matrix UB offset is -1");

        uber_ptr->model_ub_offset = model_uniform_ral_ptr->block_offset;
    }

    if (near_plane_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(near_plane_uniform_ral_ptr->block_offset != -1,
                          "Near plane UB offset is -1");

        uber_ptr->near_plane_ub_offset = near_plane_uniform_ral_ptr->block_offset;
    }

    if (normal_matrix_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(normal_matrix_uniform_ral_ptr->block_offset != -1,
                          "Normal matrix UB offset is -1");

        uber_ptr->normal_matrix_ub_offset = normal_matrix_uniform_ral_ptr->block_offset;
    }

    if (shininess_material_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(shininess_material_uniform_ral_ptr->block_offset != -1,
                          "Shininess material UB offset is -1");

        uber_ptr->shininess_material_ub_offset = shininess_material_uniform_ral_ptr->block_offset;
    }

    if (specular_material_uniform_ral_ptr != nullptr)
    {
        uber_ptr->specular_material_ub_offset = specular_material_uniform_ral_ptr->block_offset;
    }

    if (vp_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(vp_uniform_ral_ptr->block_offset != -1,
                          "VP UB offset is -1");

        uber_ptr->vp_ub_offset = vp_uniform_ral_ptr->block_offset;
    }

    if (world_camera_uniform_ral_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(world_camera_uniform_ral_ptr->block_offset != -1,
                          "World camera UB offset is -1");

        uber_ptr->world_camera_ub_offset = world_camera_uniform_ral_ptr->block_offset;
    }

    /* Retrieve uniform block IDs and their properties*/
    if (uber_ptr->ub_fs != nullptr)
    {
        ral_program_block_buffer_release(uber_ptr->ub_fs);

        uber_ptr->ub_fs = nullptr;
    }

    if (uber_ptr->ub_vs != nullptr)
    {
        ral_program_block_buffer_release(uber_ptr->ub_vs);

        uber_ptr->ub_vs = nullptr;
    }

    uber_ptr->ub_fs = ral_program_block_buffer_create(uber_ptr->context,
                                                      uber_ptr->program,
                                                      ub_fs_block_name);
    uber_ptr->ub_vs = ral_program_block_buffer_create(uber_ptr->context,
                                                      uber_ptr->program,
                                                      ub_vs_block_name);

    if (uber_ptr->ub_fs != nullptr)
    {
        ral_buffer buffer_ral = nullptr;

        ral_program_block_buffer_get_property(uber_ptr->ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &buffer_ral);
        ral_buffer_get_property              (buffer_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &uber_ptr->ub_fs_bo_size);
    }
    else
    {
        uber_ptr->ub_fs_bo_size = 0;
    }

    if (uber_ptr->ub_vs != nullptr)
    {
        ral_buffer buffer_ral = nullptr;

        ral_program_block_buffer_get_property(uber_ptr->ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &buffer_ral);
        ral_buffer_get_property              (buffer_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &uber_ptr->ub_vs_bo_size);
    }
    else
    {
        uber_ptr->ub_vs_bo_size = 0;
    }

    /* Create internal representation of uber shader items */
    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_items);

    for (unsigned int n_item = 0;
                      n_item < n_items;
                    ++n_item)
    {
        _scene_renderer_uber_item* item_ptr = nullptr;

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
            case SCENE_RENDERER_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE:
            {
                /* UB not used */
                break;
            }

            case SCENE_RENDERER_UBER_ITEM_LIGHT:
            {
                /* Fragment shader stuff */
                const ral_program_variable* light_ambient_color_uniform_ral_ptr               = nullptr;
                const ral_program_variable* light_attenuations_uniform_ral_ptr                = nullptr;
                const ral_program_variable* light_cone_angle_uniform_ral_ptr                  = nullptr;
                const ral_program_variable* light_diffuse_uniform_ral_ptr                     = nullptr;
                const ral_program_variable* light_direction_uniform_ral_ptr                   = nullptr;
                const ral_program_variable* light_edge_angle_uniform_ral_ptr                  = nullptr;
                const ral_program_variable* light_far_near_diff_uniform_ral_ptr               = nullptr;
                const ral_program_variable* light_location_uniform_ral_ptr                    = nullptr;
                const ral_program_variable* light_near_plane_uniform_ral_ptr                  = nullptr;
                const ral_program_variable* light_projection_uniform_ral_ptr                  = nullptr;
                const ral_program_variable* light_range_uniform_ral_ptr                       = nullptr;
                const ral_program_variable* light_shadow_map_vsm_cutoff_uniform_ral_ptr       = nullptr;
                const ral_program_variable* light_shadow_map_vsm_min_variance_uniform_ral_ptr = nullptr;
                const ral_program_variable* light_view_uniform_ral_ptr                        = nullptr;

                std::stringstream  light_attenuations_uniform_name_sstream;
                std::stringstream  light_cone_angle_uniform_name_sstream;
                std::stringstream  light_diffuse_uniform_name_sstream;
                std::stringstream  light_direction_uniform_name_sstream;
                std::stringstream  light_edge_angle_uniform_name_sstream;
                std::stringstream  light_far_near_diff_uniform_name_sstream;
                std::stringstream  light_location_uniform_name_sstream;
                std::stringstream  light_near_plane_uniform_name_sstream;
                std::stringstream  light_projection_uniform_name_sstream;
                std::stringstream  light_range_uniform_name_sstream;
                std::stringstream  light_shadow_map_color_uniform_name_sstream;
                std::stringstream  light_shadow_map_depth_uniform_name_sstream;
                std::stringstream  light_shadow_map_vsm_cutoff_uniform_name_sstream;
                std::stringstream  light_shadow_map_vsm_min_variance_uniform_name_sstream;
                std::stringstream  light_view_uniform_name_sstream;

                light_attenuations_uniform_name_sstream                << "light"
                                                                       << n_item
                                                                       << "_attenuations";
                light_cone_angle_uniform_name_sstream                  << "light"
                                                                       << n_item
                                                                       << "_cone_angle";
                light_diffuse_uniform_name_sstream                     << "light"
                                                                       << n_item
                                                                       << "_diffuse";
                light_direction_uniform_name_sstream                   << "light"
                                                                       << n_item
                                                                       << "_direction";
                light_edge_angle_uniform_name_sstream                  << "light"
                                                                       << n_item
                                                                       << "_edge_angle";
                light_far_near_diff_uniform_name_sstream               << "light"
                                                                       << n_item
                                                                       << "_far_near_diff";
                light_location_uniform_name_sstream                    << "light"
                                                                       << n_item
                                                                       << "_world_pos";
                light_near_plane_uniform_name_sstream                  << "light"
                                                                       << n_item
                                                                       << "_near";
                light_projection_uniform_name_sstream                  << "light"
                                                                       << n_item
                                                                       << "_projection";
                light_range_uniform_name_sstream                       << "light"
                                                                       << n_item
                                                                       << "_range";
                light_shadow_map_color_uniform_name_sstream            << "light"
                                                                       << n_item
                                                                       << "_shadow_map_color";
                light_shadow_map_depth_uniform_name_sstream            << "light"
                                                                       << n_item
                                                                       << "_shadow_map_depth";
                light_shadow_map_vsm_cutoff_uniform_name_sstream       << "light"
                                                                       << n_item
                                                                       << "_shadow_map_vsm_cutoff";
                light_shadow_map_vsm_min_variance_uniform_name_sstream << "light"
                                                                       << n_item
                                                                       << "_shadow_map_vsm_min_variance";
                light_view_uniform_name_sstream                        << "light"
                                                                       << n_item
                                                                       << "_view";

                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create("ambient_color"),
                                                      &light_ambient_color_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_attenuations_uniform_name_sstream.str().c_str() ),
                                                      &light_attenuations_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_cone_angle_uniform_name_sstream.str().c_str() ),
                                                      &light_cone_angle_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_diffuse_uniform_name_sstream.str().c_str() ),
                                                      &light_diffuse_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_direction_uniform_name_sstream.str().c_str() ),
                                                      &light_direction_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_edge_angle_uniform_name_sstream.str().c_str() ),
                                                      &light_edge_angle_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_far_near_diff_uniform_name_sstream.str().c_str() ),
                                                      &light_far_near_diff_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_location_uniform_name_sstream.str().c_str()  ),
                                                      &light_location_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_near_plane_uniform_name_sstream.str().c_str() ),
                                                      &light_near_plane_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_projection_uniform_name_sstream.str().c_str() ),
                                                      &light_projection_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_range_uniform_name_sstream.str().c_str()  ),
                                                      &light_range_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_view_uniform_name_sstream.str().c_str() ),
                                                      &light_view_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_shadow_map_vsm_cutoff_uniform_name_sstream.str().c_str() ),
                                                      &light_shadow_map_vsm_cutoff_uniform_ral_ptr);
                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_fs_block_name,
                                                       system_hashed_ansi_string_create(light_shadow_map_vsm_min_variance_uniform_name_sstream.str().c_str() ),
                                                      &light_shadow_map_vsm_min_variance_uniform_ral_ptr);

                if (ral_program_get_block_variable_by_name(uber_ptr->program,
                                                           system_hashed_ansi_string_get_default_empty_string(),
                                                           system_hashed_ansi_string_create(light_shadow_map_color_uniform_name_sstream.str().c_str() ),
                                                           nullptr) ) /* out_variable_ptr_ptr */
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_color_uniform_name = system_hashed_ansi_string_create(light_shadow_map_color_uniform_name_sstream.str().c_str() );
                }
                else
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_color_uniform_name= nullptr;
                }

                if (ral_program_get_block_variable_by_name(uber_ptr->program,
                                                           system_hashed_ansi_string_get_default_empty_string(),
                                                           system_hashed_ansi_string_create(light_shadow_map_depth_uniform_name_sstream.str().c_str() ),
                                                           nullptr) ) /* out_variable_ptr_ptr */
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_depth_uniform_name = system_hashed_ansi_string_create(light_shadow_map_depth_uniform_name_sstream.str().c_str() );
                }
                else
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_depth_uniform_name = nullptr;
                }

                if (light_ambient_color_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.ambient_color_ub_offset = light_ambient_color_uniform_ral_ptr->block_offset;
                }

                if (light_attenuations_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_attenuations_ub_offset = light_attenuations_uniform_ral_ptr->block_offset;
                }

                if (light_cone_angle_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset = light_cone_angle_uniform_ral_ptr->block_offset;
                }

                if (light_direction_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_direction_ub_offset = light_direction_uniform_ral_ptr->block_offset;
                }

                if (light_diffuse_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_diffuse_ub_offset  = light_diffuse_uniform_ral_ptr->block_offset;
                }

                if (light_edge_angle_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset = light_edge_angle_uniform_ral_ptr->block_offset;
                }

                if (light_far_near_diff_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset = light_far_near_diff_uniform_ral_ptr->block_offset;
                }

                if (light_location_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_location_ub_offset = light_location_uniform_ral_ptr->block_offset;
                }

                if (light_near_plane_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_near_plane_ub_offset = light_near_plane_uniform_ral_ptr->block_offset;
                }

                if (light_projection_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_projection_ub_offset = light_projection_uniform_ral_ptr->block_offset;
                }

                if (light_range_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_range_ub_offset = light_range_uniform_ral_ptr->block_offset;
                }

                if (light_view_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_view_ub_offset = light_view_uniform_ral_ptr->block_offset;
                }

                if (light_shadow_map_vsm_cutoff_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset = light_shadow_map_vsm_cutoff_uniform_ral_ptr->block_offset;
                }

                if (light_shadow_map_vsm_min_variance_uniform_ral_ptr != nullptr)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset = light_shadow_map_vsm_min_variance_uniform_ral_ptr->block_offset;
                }

                /* Vertex shader stuff */
                std::stringstream           light_depth_vb_uniform_name_sstream;
                const ral_program_variable* light_depth_vb_uniform_ral_ptr = nullptr;
                shaders_vertex_uber_light   light_type                     = SHADERS_VERTEX_UBER_LIGHT_NONE;

                light_depth_vb_uniform_name_sstream << "light"
                                                    << n_item
                                                    << "_depth_vp";

                ral_program_get_block_variable_by_name(uber_ptr->program,
                                                       ub_vs_block_name,
                                                       system_hashed_ansi_string_create(light_depth_vb_uniform_name_sstream.str().c_str() ),
                                                      &light_depth_vb_uniform_ral_ptr);

                if (light_depth_vb_uniform_ral_ptr != nullptr)
                {
                    item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset = light_depth_vb_uniform_ral_ptr->block_offset;
                }
                else
                {
                    item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset = -1;
                }

                /* Outdated SH stuff */
                shaders_vertex_uber_get_light_type(uber_ptr->shader_vertex,
                                                   n_item,
                                                  &light_type);

                if (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS ||
                    light_type == SHADERS_VERTEX_UBER_LIGHT_SH_4_BANDS)
                {
                    GLint                       sh_data_uniform_location = -1;
                    std::stringstream           sh_data_uniform_name_sstream;
                    const ral_program_variable* sh_data_uniform_ral_ptr  = nullptr;

                    if (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS)
                    {
                        sh_data_uniform_name_sstream << "light" << n_item << "_sh3[0]";
                    }
                    else
                    {
                        sh_data_uniform_name_sstream << "light" << n_item << "_sh4[0]";
                    }

                    ral_program_get_block_variable_by_name(uber_ptr->program,
                                                           ub_vs_block_name,
                                                           system_hashed_ansi_string_create(sh_data_uniform_name_sstream.str().c_str()),
                                                          &sh_data_uniform_ral_ptr);

                    ASSERT_DEBUG_SYNC(sh_data_uniform_ral_ptr != nullptr,
                                      "Could not retrieve SH data uniform descriptor");
                    ASSERT_DEBUG_SYNC(sh_data_uniform_ral_ptr->block_offset != -1,
                                      "UB offset for SH data is -1");

                    item_ptr->vertex_shader_item.current_light_sh_data_ub_offset = sh_data_uniform_ral_ptr->block_offset;
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized uber item type");
            }
        }
    }

    /* All done - object no longer dirty */
    uber_ptr->dirty = false;

end:
    ;
}

/** Please see header for spec */
PUBLIC void scene_renderer_uber_render_mesh(mesh                             mesh_gpu,
                                            system_matrix4x4                 model,
                                            system_matrix4x4                 normal_matrix,
                                            scene_renderer_uber              uber,
                                            mesh_material                    material,
                                            system_time                      time,
                                            const ral_gfx_state_create_info* ref_gfx_state_create_info_ptr)
{
    _scene_renderer_uber* uber_ptr = reinterpret_cast<_scene_renderer_uber*>(uber);

    ASSERT_DEBUG_SYNC(mesh_gpu != nullptr,
                      "Null mesh instance was specified");
    ASSERT_DEBUG_SYNC(material != nullptr,
                      "Material must not be null");

    /* If the mesh is instantiated, retrieve the mesh instance we should be using
     * for the rendering */
    mesh mesh_instantiation_parent_gpu = nullptr;

    mesh_get_property(mesh_gpu,
                      MESH_PROPERTY_INSTANTIATION_PARENT,
                     &mesh_instantiation_parent_gpu);

    if (mesh_instantiation_parent_gpu == nullptr)
    {
        mesh_instantiation_parent_gpu = mesh_gpu;
    }

    /* Need to bake new mesh data material instance? Do it now */
    _scene_renderer_uber_mesh_data_material* mesh_data_material_ptr       = nullptr;
    _scene_renderer_uber_mesh_data*          mesh_data_ptr                = nullptr;
    system_time                              mesh_modification_timestamp  = 0;
    bool                                     should_record_command_buffer = true;

    mesh_get_property(mesh_instantiation_parent_gpu,
                      MESH_PROPERTY_TIMESTAMP_MODIFICATION,
                     &mesh_modification_timestamp);

    if (!system_hash64map_get(uber_ptr->mesh_to_mesh_data_map,
                              reinterpret_cast<system_hash64>(mesh_gpu),
                             &mesh_data_ptr) )
    {
        mesh_data_ptr = new _scene_renderer_uber_mesh_data(uber_ptr->context);

        _scene_renderer_uber_bake_mesh_data(uber_ptr,
                                            mesh_instantiation_parent_gpu,
                                            mesh_data_ptr,
                                            ref_gfx_state_create_info_ptr);

        system_hash64map_insert(uber_ptr->mesh_to_mesh_data_map,
                                reinterpret_cast<system_hash64>(mesh_gpu),
                                mesh_data_ptr,
                                nullptr,  /* on_removal_callback          */
                                nullptr); /* on_removal_callback_user_arg */
    }

    ASSERT_DEBUG_SYNC(mesh_data_ptr != nullptr,
                      "Mesh data instance is null");

    if (!system_hash64map_get(mesh_data_ptr->material_to_mesh_data_material_map,
                              reinterpret_cast<system_hash64>(material),
                             &mesh_data_material_ptr) )
    {
        mesh_data_material_ptr = new _scene_renderer_uber_mesh_data_material(uber_ptr->context,
                                                                             material,
                                                                             mesh_data_ptr,
                                                                             uber_ptr);

        system_hash64map_insert(mesh_data_ptr->material_to_mesh_data_material_map,
                                reinterpret_cast<system_hash64>(material),
                                mesh_data_material_ptr,
                                nullptr,  /* on_removal_callback          */
                                nullptr); /* on_removal_callback_user_arg */
    }
    else
    {
        ASSERT_DEBUG_SYNC(mesh_data_material_ptr != nullptr,
                          "Mesh material data instance is null");

        if (mesh_data_material_ptr->mesh_modification_timestamp == mesh_modification_timestamp &&
            mesh_data_material_ptr->color_rt                    == uber_ptr->active_color_rt   &&
            mesh_data_material_ptr->depth_rt                    == uber_ptr->active_depth_rt)
        {
            /* No need to re-record commands */
            should_record_command_buffer = true;
        }
    }

    if (should_record_command_buffer)
    {
        mesh_data_material_ptr->mesh_modification_timestamp = mesh_modification_timestamp;
        mesh_data_material_ptr->color_rt                    = uber_ptr->active_color_rt;
        mesh_data_material_ptr->depth_rt                    = uber_ptr->active_depth_rt;

        /* Start recording the command buffer and append the preamble */
        ral_command_buffer_start_recording(mesh_data_material_ptr->command_buffer);
        {
            uint32_t n_preamble_commands = 0;

            ral_command_buffer_get_property(uber_ptr->preamble_command_buffer,
                                            RAL_COMMAND_BUFFER_PROPERTY_N_RECORDED_COMMANDS,
                                           &n_preamble_commands);

            if (n_preamble_commands > 0)
            {
                ral_command_buffer_append_commands_from_command_buffer(mesh_data_material_ptr->command_buffer,
                                                                       uber_ptr->preamble_command_buffer,
                                                                       0, /* n_start_command */
                                                                       n_preamble_commands);
            }

            ral_command_buffer_record_set_gfx_state(mesh_data_material_ptr->command_buffer,
                                                    mesh_data_ptr->gfx_state);

            /* Set up rendertarget bindings */
            if (uber_ptr->active_color_rt != nullptr)
            {
                ral_command_buffer_set_binding_command_info            rt_binding;
                ral_command_buffer_set_color_rendertarget_command_info rt_info    = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();

                /* Make sure color writes are enabled. */
                ASSERT_DEBUG_SYNC(!ref_gfx_state_create_info_ptr->rasterizer_discard,
                                  "Color rendertarget specified even though rasterizer discard mode is enabled.");

                /* Configure the bindings */
                rt_binding.binding_type                  = RAL_BINDING_TYPE_RENDERTARGET;
                rt_binding.name                          = system_hashed_ansi_string_create("result_fragment");
                rt_binding.rendertarget_binding.rt_index = 0;

                rt_info.rendertarget_index = 0;
                rt_info.texture_view       = uber_ptr->active_color_rt;

                ral_command_buffer_record_set_color_rendertargets(mesh_data_material_ptr->command_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &rt_info);
                ral_command_buffer_record_set_bindings           (mesh_data_material_ptr->command_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &rt_binding);
            }

            if (uber_ptr->active_depth_rt != nullptr)
            {
                ral_command_buffer_record_set_depth_rendertarget(mesh_data_material_ptr->command_buffer,
                                                                 uber_ptr->active_depth_rt);
            }
            else
            {
                /* Make sure depth writes are disabled */
                ASSERT_DEBUG_SYNC(!ref_gfx_state_create_info_ptr->depth_writes,
                                  "Depth writes enabled despite no depth rendertarget being specified.");
            }

            /* Set up vertex buffers */
            {
                bool                                              is_normals_input_attribute_present;
                bool                                              is_uv_input_attribute_present;
                bool                                              is_vertex_input_attribute_present;
                uint32_t                                          n_vertex_buffers              = 0;
                const system_hashed_ansi_string                   normals_attribute_name_has    = system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_normal);
                const system_hashed_ansi_string                   uv_attribute_name_has         = system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_uv);
                const system_hashed_ansi_string                   vertex_attribute_name_has     = system_hashed_ansi_string_create(_scene_renderer_uber_attribute_name_object_vertex);
                ral_command_buffer_set_vertex_buffer_command_info vertex_buffers[3];

                is_normals_input_attribute_present = ral_program_get_vertex_attribute_by_name(mesh_data_material_ptr->uber_ptr->program,
                                                                                              normals_attribute_name_has,
                                                                                              nullptr);
                is_uv_input_attribute_present      = ral_program_get_vertex_attribute_by_name(mesh_data_material_ptr->uber_ptr->program,
                                                                                              uv_attribute_name_has,
                                                                                              nullptr);
                is_vertex_input_attribute_present  = ral_program_get_vertex_attribute_by_name(mesh_data_material_ptr->uber_ptr->program,
                                                                                              vertex_attribute_name_has,
                                                                                              nullptr);

                if (mesh_data_ptr->normals_stream_buffer_ral != nullptr &&
                    is_normals_input_attribute_present)
                {
                    ral_command_buffer_set_vertex_buffer_command_info* vb_ptr = vertex_buffers + n_vertex_buffers;

                    vb_ptr->buffer       = mesh_data_ptr->normals_stream_buffer_ral;
                    vb_ptr->name         = normals_attribute_name_has;
                    vb_ptr->start_offset = mesh_data_ptr->normals_stream_offset;

                    ++n_vertex_buffers;
                }

                if (mesh_data_ptr->texcoords_stream_buffer_ral != nullptr &&
                    is_uv_input_attribute_present)
                {
                    ral_command_buffer_set_vertex_buffer_command_info* vb_ptr = vertex_buffers + n_vertex_buffers;

                    vb_ptr->buffer       = mesh_data_ptr->texcoords_stream_buffer_ral;
                    vb_ptr->name         = uv_attribute_name_has;
                    vb_ptr->start_offset = mesh_data_ptr->texcoords_stream_offset;

                    ++n_vertex_buffers;
                }

                if (mesh_data_ptr->vertex_stream_buffer_ral != nullptr &&
                    is_vertex_input_attribute_present)
                {
                    ral_command_buffer_set_vertex_buffer_command_info* vb_ptr = vertex_buffers + n_vertex_buffers;

                    vb_ptr->buffer       = mesh_data_ptr->vertex_stream_buffer_ral;
                    vb_ptr->name         = vertex_attribute_name_has;
                    vb_ptr->start_offset = mesh_data_ptr->vertex_stream_offset;

                    ++n_vertex_buffers;
                }

                ral_command_buffer_record_set_vertex_buffers(mesh_data_material_ptr->command_buffer,
                                                             n_vertex_buffers,
                                                             vertex_buffers);
            }

            /* Update model matrix */
            ASSERT_DEBUG_SYNC(uber_ptr->model_ub_offset != -1,
                              "No model matrix uniform found");

            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                   uber_ptr->model_ub_offset,
                                                                   system_matrix4x4_get_row_major_data(model),
                                                                   sizeof(float) * 16);

            /* Update normal matrix */
            if (uber_ptr->normal_matrix_ub_offset != -1)
            {
                ASSERT_DEBUG_SYNC(normal_matrix != nullptr,
                                  "Normal matrix is nullptr but is required.");

                ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                       uber_ptr->normal_matrix_ub_offset,
                                                                       system_matrix4x4_get_row_major_data(normal_matrix),
                                                                       sizeof(float) * 16);
            }

            /* Iterate over all layers.. */
            mesh_type  instance_type;
            ral_buffer mesh_data_bo = nullptr;
            uint32_t   n_layers     = 0;

            mesh_get_property(mesh_instantiation_parent_gpu,
                              MESH_PROPERTY_BO_RAL,
                             &mesh_data_bo);
            mesh_get_property(mesh_instantiation_parent_gpu,
                              MESH_PROPERTY_N_LAYERS,
                             &n_layers);
            mesh_get_property(mesh_instantiation_parent_gpu,
                              MESH_PROPERTY_TYPE,
                             &instance_type);

            for (uint32_t n_layer = 0;
                          n_layer < n_layers;
                        ++n_layer)
            {
                const uint32_t n_layer_passes = mesh_get_number_of_layer_passes(mesh_instantiation_parent_gpu,
                                                                                n_layer);

                /* Iterate over all layer passes */
                for (uint32_t n_layer_pass = 0;
                              n_layer_pass < n_layer_passes;
                            ++n_layer_pass)
                {
                    mesh_material layer_pass_material = nullptr;

                    /* Retrieve layer pass properties */
                    mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                 n_layer,
                                                 n_layer_pass,
                                                 MESH_LAYER_PROPERTY_MATERIAL,
                                                &layer_pass_material);

                    if (layer_pass_material              != mesh_data_material_ptr->material &&
                        mesh_data_material_ptr->material != nullptr)
                    {
                        continue;
                    }

                    /* Bind shading data for each supported shading property */
                    struct _attachment
                    {
                        mesh_material_shading_property property;
                        const char*                    shader_sampler_name;
                        uint32_t                       shader_scalar_ub_offset;
                        const char*                    shader_uv_attribute_name;
                        bool                           convert_to_linear;
                    } attachments[] =
                    {
                        {
                            MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                            _scene_renderer_uber_uniform_name_ambient_material_sampler,
                            mesh_data_material_ptr->uber_ptr->ambient_material_ub_offset,
                            _scene_renderer_uber_attribute_name_object_uv,
                            true,
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                            _scene_renderer_uber_uniform_name_diffuse_material_sampler,
                            mesh_data_material_ptr->uber_ptr->diffuse_material_ub_offset,
                            _scene_renderer_uber_attribute_name_object_uv,
                            true,
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                            _scene_renderer_uber_uniform_name_luminosity_material_sampler,
                            mesh_data_material_ptr->uber_ptr->luminosity_material_ub_offset,
                            _scene_renderer_uber_attribute_name_object_uv,
                            false
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                            _scene_renderer_uber_uniform_name_shininess_material_sampler,
                            mesh_data_material_ptr->uber_ptr->shininess_material_ub_offset,
                            _scene_renderer_uber_attribute_name_object_uv,
                            false
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                            _scene_renderer_uber_uniform_name_specular_material_sampler,
                            mesh_data_material_ptr->uber_ptr->specular_material_ub_offset,
                            _scene_renderer_uber_attribute_name_object_uv,
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
                                float        data_vec3[3] = {0};
                                unsigned int n_components = 1;

                                if (attachment.shader_scalar_ub_offset == -1)
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
                                }

                                ral_program_block_buffer_set_nonarrayed_variable_value(mesh_data_material_ptr->uber_ptr->ub_fs,
                                                                                       attachment.shader_scalar_ub_offset,
                                                                                       data_vec3,
                                                                                       sizeof(float) * n_components);

                                break;
                            }

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                            {
                                ral_sampler                                 attachment_sampler      = nullptr;
                                ral_command_buffer_set_binding_command_info attachment_texture_binding;
                                ral_texture_view                            attachment_texture_view = nullptr;

                                mesh_material_get_shading_property_value_texture_view(layer_pass_material,
                                                                                      attachment.property,
                                                                                     &attachment_texture_view,
                                                                                     &attachment_sampler);

                                attachment_texture_binding.binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
                                attachment_texture_binding.name                               = system_hashed_ansi_string_create(attachment.shader_sampler_name);
                                attachment_texture_binding.sampled_image_binding.sampler      = attachment_sampler;
                                attachment_texture_binding.sampled_image_binding.texture_view = attachment_texture_view;

                                ral_command_buffer_record_set_bindings(mesh_data_material_ptr->command_buffer,
                                                                       1, /* n_bindings */
                                                                      &attachment_texture_binding);

                                break;
                            }

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                            {
                                float data_vec4[4] = {0};

                                if (attachment.shader_scalar_ub_offset == -1)
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
                                }

                                ral_program_block_buffer_set_nonarrayed_variable_value(mesh_data_material_ptr->uber_ptr->ub_fs,
                                                                                       attachment.shader_scalar_ub_offset,
                                                                                       data_vec4,
                                                                                       sizeof(float) * 4);

                                break;
                            }

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Unrecognized material property attachment");
                            }
                        }
                    }

                    if (mesh_data_material_ptr->uber_ptr->ub_fs != nullptr)
                    {
                        ral_program_block_buffer_sync_via_command_buffer(mesh_data_material_ptr->uber_ptr->ub_fs,
                                                                         mesh_data_material_ptr->command_buffer);
                    }

                    if (mesh_data_material_ptr->uber_ptr->ub_vs != nullptr)
                    {
                        ral_program_block_buffer_sync_via_command_buffer(mesh_data_material_ptr->uber_ptr->ub_vs,
                                                                         mesh_data_material_ptr->command_buffer);
                    }

                    /* Issue the draw call. We need to handle two separate cases here:
                     *
                     * 1) We're dealing with a regular mesh:    need to do an indexed draw call.
                     * 2) We're dealing with a GPU stream mesh: trickier! need to check what kind of draw call
                     *                                          we need to make and act accordingly.
                     */
                    if (instance_type == MESH_TYPE_REGULAR)
                    {
                        /* Retrieve mesh index type */
                        _mesh_index_type index_type = MESH_INDEX_TYPE_UNKNOWN;

                        mesh_get_property(mesh_instantiation_parent_gpu,
                                          MESH_PROPERTY_BO_INDEX_TYPE,
                                         &index_type);

                        /* Proceed with the actual draw call */
                        ral_command_buffer_draw_call_indexed_command_info draw_call_info;
                        uint32_t                                          layer_pass_index_data_offset = 0;
                        uint32_t                                          layer_pass_index_max_value   = 0;
                        uint32_t                                          layer_pass_index_min_value   = 0;
                        uint32_t                                          layer_pass_n_indices         = 0;

                        mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                     n_layer,
                                                     n_layer_pass,
                                                     MESH_LAYER_PROPERTY_BO_ELEMENTS_OFFSET,
                                                    &layer_pass_index_data_offset);
                        mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                     n_layer,
                                                     n_layer_pass,
                                                     MESH_LAYER_PROPERTY_BO_ELEMENTS_DATA_MAX_INDEX,
                                                    &layer_pass_index_max_value);
                        mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                     n_layer,
                                                     n_layer_pass,
                                                     MESH_LAYER_PROPERTY_BO_ELEMENTS_DATA_MIN_INDEX,
                                                    &layer_pass_index_min_value);
                        mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                     n_layer,
                                                     n_layer_pass,
                                                     MESH_LAYER_PROPERTY_N_ELEMENTS,
                                                    &layer_pass_n_indices);

                        switch (index_type)
                        {
                            case MESH_INDEX_TYPE_UNSIGNED_CHAR:
                            {
                                draw_call_info.first_index = layer_pass_index_data_offset;
                                draw_call_info.index_type  = RAL_INDEX_TYPE_8BIT;

                                break;
                            }

                            case MESH_INDEX_TYPE_UNSIGNED_SHORT:
                            {
                                ASSERT_DEBUG_SYNC((layer_pass_index_data_offset % sizeof(uint16_t)) == 0,
                                                  "Invalid index data offset");

                                draw_call_info.first_index = layer_pass_index_data_offset / sizeof(uint16_t);
                                draw_call_info.index_type  = RAL_INDEX_TYPE_16BIT;

                                break;
                            }

                            case MESH_INDEX_TYPE_UNSIGNED_INT:
                            {
                                ASSERT_DEBUG_SYNC((layer_pass_index_data_offset % sizeof(uint32_t)) == 0,
                                                  "Invalid index data offset");

                                draw_call_info.first_index = layer_pass_index_data_offset / sizeof(uint32_t);
                                draw_call_info.index_type  = RAL_INDEX_TYPE_32BIT;

                                break;
                            }

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Unrecognized mesh index type");
                            }
                        }

                        draw_call_info.base_instance = 0;
                        draw_call_info.base_vertex   = 0;
                        draw_call_info.index_buffer  = mesh_data_bo;
                        draw_call_info.n_indices     = layer_pass_n_indices;
                        draw_call_info.n_instances   = 1;

                        ral_command_buffer_record_draw_call_indexed(mesh_data_material_ptr->command_buffer,
                                                                    1, /* n_draw_calls */
                                                                   &draw_call_info);
                    }
                    else
                    {
                        mesh_draw_call_arguments draw_call_arguments;
                        mesh_draw_call_type      draw_call_type = MESH_DRAW_CALL_TYPE_UNKNOWN;

                        ASSERT_DEBUG_SYNC(instance_type == MESH_TYPE_GPU_STREAM,
                                          "Unrecognized mesh type encountered.");

                        mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                     n_layer,
                                                     n_layer_pass,
                                                     MESH_LAYER_PROPERTY_DRAW_CALL_ARGUMENTS,
                                                    &draw_call_arguments);
                        mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                                     n_layer,
                                                     n_layer_pass,
                                                     MESH_LAYER_PROPERTY_DRAW_CALL_TYPE,
                                                    &draw_call_type);

                        switch (draw_call_type)
                        {
                            case MESH_DRAW_CALL_TYPE_NONINDEXED:
                            {
                                ral_command_buffer_draw_call_regular_command_info draw_call_info;

                                draw_call_info.base_instance = 0;
                                draw_call_info.base_vertex   = draw_call_arguments.n_base_vertex;
                                draw_call_info.n_instances   = 1;
                                draw_call_info.n_vertices    = draw_call_arguments.n_vertices;

                                ral_command_buffer_record_draw_call_regular(mesh_data_material_ptr->command_buffer,
                                                                            1, /* n_draw_calls */
                                                                           &draw_call_info);

                                break;
                            }

                            case MESH_DRAW_CALL_TYPE_NONINDEXED_INDIRECT:
                            {
                                ral_command_buffer_draw_call_indirect_command_info draw_call_info;

                                draw_call_info.index_buffer    = nullptr;
                                draw_call_info.index_type      = RAL_INDEX_TYPE_COUNT;
                                draw_call_info.indirect_buffer = draw_call_arguments.draw_indirect_bo;
                                draw_call_info.offset          = draw_call_arguments.indirect_offset;
                                draw_call_info.stride          = 0;

                                ral_command_buffer_record_draw_call_indirect_regular(mesh_data_material_ptr->command_buffer,
                                                                                     1, /* n_draw_calls */
                                                                                    &draw_call_info);

                                break;
                            }

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Unrecognized draw call type requested for a GPU stream mesh.");
                            }
                        }
                    }
                }
            }
        }
        ral_command_buffer_stop_recording(mesh_data_material_ptr->command_buffer);
    }

    /* Iterate over mesh layers and cache any RAL buffers used by the draw calls.
     * These will have to be exposed as inputs of the result present task.
     */
    uint32_t n_mesh_layers = 0;

    mesh_get_property(mesh_gpu,
                      MESH_PROPERTY_N_LAYERS,
                      &n_mesh_layers);

    for (uint32_t n_mesh_layer = 0;
                  n_mesh_layer < n_mesh_layers;
                ++n_mesh_layer)
    {
        for (mesh_layer_data_stream_type stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                         stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                ++((int&)stream_type) )
        {
            ral_buffer stream_buffers[2] = {nullptr};

            mesh_get_layer_data_stream_property(mesh_gpu,
                                                n_mesh_layer,
                                                stream_type,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,
                                                stream_buffers + 0);
            mesh_get_layer_data_stream_property(mesh_gpu,
                                                n_mesh_layer,
                                                stream_type,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_RAL,
                                                stream_buffers + 1);

            for (uint32_t n_stream_buffer = 0;
                          n_stream_buffer < sizeof(stream_buffers) / sizeof(stream_buffers[0]);
                        ++n_stream_buffer)
            {
                ral_buffer buffer         = stream_buffers[n_stream_buffer];
                ral_buffer buffer_topmost;

                if (buffer == nullptr)
                {
                    continue;
                }

                ral_buffer_get_property(buffer,
                                        RAL_BUFFER_PROPERTY_PARENT_BUFFER_TOPMOST,
                                       &buffer_topmost);

                if (buffer_topmost != nullptr)
                {
                    buffer = buffer_topmost;
                }

                if (system_resizable_vector_find(uber_ptr->scheduled_mesh_buffers,
                                                 buffer) == ITEM_NOT_FOUND)
                {
                    system_resizable_vector_push(uber_ptr->scheduled_mesh_buffers,
                                                 buffer);
                }
            }
        }
    }

end:
    ral_context_retain_object(uber_ptr->context,
                              RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                              mesh_data_material_ptr->command_buffer);

    system_resizable_vector_push(uber_ptr->scheduled_mesh_command_buffers,
                                 mesh_data_material_ptr->command_buffer);
}

/* Please see header for specification */
PUBLIC void scene_renderer_uber_set_shader_general_property(scene_renderer_uber                  uber,
                                                            scene_renderer_uber_general_property property,
                                                            const void*                          data)
{
    _scene_renderer_uber* uber_ptr = reinterpret_cast<_scene_renderer_uber*>(uber);

    /* All properties below refer to the uniform block defined in uber vertex shader. */
    if (uber_ptr->ub_vs == nullptr)
    {
        goto end;
    }

    switch (property)
    {
        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_CAMERA_LOCATION:
        {
            if (uber_ptr->world_camera_ub_offset != -1)
            {
                const float location[4] =
                {
                    (reinterpret_cast<const float*>(data))[0],
                    (reinterpret_cast<const float*>(data))[1],
                    (reinterpret_cast<const float*>(data))[2],
                    1.0f
                };

                ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                       uber_ptr->world_camera_ub_offset,
                                                                       location,
                                                                       sizeof(float) * 4);
            }

            break;
        }

        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_FAR_NEAR_PLANE_DIFF:
        {
            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                   uber_ptr->far_near_plane_diff_ub_offset,
                                                                   data,
                                                                   sizeof(float) );

            break;
        }

        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_FLIP_Z:
        {
            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                   uber_ptr->flip_z_ub_offset,
                                                                   data,
                                                                   sizeof(float) );

            break;
        }

        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_VSM_MAX_VARIANCE:
        {
            /* max variance is an uniform so it doesn't make much sense to track it, as the program
             * is context-wide and the value might've been changed by another scene_renderer_uber instance. */
            uber_ptr->current_vsm_max_variance = *reinterpret_cast<const float*>(data);

            break;
        }

        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_NEAR_PLANE:
        {
            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                   uber_ptr->near_plane_ub_offset,
                                                                   data,
                                                                   sizeof(float) );

            break;
        }

        case SCENE_RENDERER_UBER_GENERAL_PROPERTY_VP:
        {
            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                   uber_ptr->vp_ub_offset,
                                                                   system_matrix4x4_get_row_major_data( (system_matrix4x4) data),
                                                                   sizeof(float) * 16);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized general uber property [%d]",
                              property);
        }
    }

end:
    ;
}

/* Please see header for specification */
PUBLIC void scene_renderer_uber_set_shader_item_property(scene_renderer_uber               uber,
                                                         unsigned int                      item_index,
                                                         scene_renderer_uber_item_property property,
                                                         const void*                       data)
{
    _scene_renderer_uber* uber_ptr = reinterpret_cast<_scene_renderer_uber*>(uber);

    ASSERT_DEBUG_SYNC(uber_ptr->type == SCENE_RENDERER_UBER_TYPE_REGULAR,
                      "scene_renderer_uber_set_shader_item_property() is only supported for regular scene_renderer_uber instances.");

    switch (property)
    {
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_VIEW_COLOR:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_VIEW_DEPTH:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE:
        case SCENE_RENDERER_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP:
        {
            _scene_renderer_uber_item* item_ptr = nullptr;

            if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                       item_index,
                                                      &item_ptr) )
            {
                switch (property)
                {
                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR:
                    {
                        if (item_ptr->fragment_shader_item.ambient_color_ub_offset != -1)
                        {
                            /* We're rendering in linear space, so make sure to convert
                             * sRGB light color to linear space */
                            float        linear_data[3];
                            const float* srgb_data_ptr = reinterpret_cast<const float*>(data);

                            for (unsigned int n_component = 0;
                                              n_component < 3;
                                            ++n_component)
                            {
                                linear_data[n_component] = convert_sRGB_to_linear(srgb_data_ptr[n_component]);
                            }

                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.ambient_color_ub_offset,
                                                                                   linear_data,
                                                                                   sizeof(float) * 3);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
                    {
                        if (item_ptr->fragment_shader_item.current_light_attenuations_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_attenuations_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) * 3);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_diffuse_ub_offset != -1)
                        {
                            /* We're rendering in linear space, so make sure to convert
                             * sRGB light color to linear space */
                            float        linear_data[4];
                            const float* srgb_data_ptr = reinterpret_cast<const float*>(data);

                            for (unsigned int n_component = 0;
                                              n_component < 3;
                                            ++n_component)
                            {
                                linear_data[n_component] = convert_sRGB_to_linear(srgb_data_ptr[n_component]);
                            }

                            linear_data[3] = 1.0f;

                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_diffuse_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) * 4);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
                    {
                        const float* temp = reinterpret_cast<const float*>(data);

                        if (item_ptr->fragment_shader_item.current_light_direction_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_direction_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) * 3);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF:
                    {
                        if (item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
                    {
                        if (item_ptr->fragment_shader_item.current_light_location_ub_offset != -1)
                        {
                            const float location[4] =
                            {
                                reinterpret_cast<const float*>(data)[0],
                                reinterpret_cast<const float*>(data)[1],
                                reinterpret_cast<const float*>(data)[2],
                                1.0f
                            };

                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_location_ub_offset,
                                                                                   location,
                                                                                   sizeof(float) * 4);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_near_plane_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_near_plane_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX:
                    {
                        if (item_ptr->fragment_shader_item.current_light_projection_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_projection_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) * 16);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_range_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_range_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX:
                    {
                        if (item_ptr->fragment_shader_item.current_light_view_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_view_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) * 16);
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_VIEW_COLOR:
                    {
                        item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_color = *reinterpret_cast<const ral_texture_view*>(data);

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_VIEW_DEPTH:
                    {
                        item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_depth = *reinterpret_cast<const ral_texture_view*>(data);

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF:
                    {
                        if (item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_fs,
                                                                                   item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) );
                        }

                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP:
                    {
                        if (item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset != -1)
                        {
                            ral_program_block_buffer_set_nonarrayed_variable_value(uber_ptr->ub_vs,
                                                                                   item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset,
                                                                                   data,
                                                                                   sizeof(float) * 16);
                        }

                        break;
                    }
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve fragment shader item descriptor at index [%d]",
                                  item_index);
            }

            break;
        }

        case SCENE_RENDERER_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA:
        {
            shaders_vertex_uber_light light_type = SHADERS_VERTEX_UBER_LIGHT_NONE;

            if (shaders_vertex_uber_get_light_type(uber_ptr->shader_vertex,
                                                   item_index,
                                                  &light_type) )
            {
                _scene_renderer_uber_item* item_ptr = nullptr;

                if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                           item_index,
                                                          &item_ptr) )
                {
                    memcpy(&item_ptr->vertex_shader_item.current_light_sh_data,
                           data,
                           sizeof(scene_renderer_uber_light_sh_data) );
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve vertex shader item at index [%d]",
                                      item_index);
                }
            }
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
    }
}

/* Please see header for specification */
PUBLIC void scene_renderer_uber_rendering_start(scene_renderer_uber                   uber,
                                                const scene_renderer_uber_start_info* start_info_ptr)
{
    _scene_renderer_uber* uber_ptr = reinterpret_cast<_scene_renderer_uber*>(uber);

    ASSERT_DEBUG_SYNC(!uber_ptr->is_rendering,
                      "Already started");

    /* Validate & cache the information specified in start info */
    ASSERT_DEBUG_SYNC(start_info_ptr != nullptr,
                      "Specified a null start info structure");
    ASSERT_DEBUG_SYNC(start_info_ptr->color_rt != nullptr ||
                      start_info_ptr->depth_rt != nullptr,
                      "All specified rendertargets are null");

    uber_ptr->active_color_rt = start_info_ptr->color_rt;
    uber_ptr->active_depth_rt = start_info_ptr->depth_rt;

    /* Update shaders if the configuration has been changed since the last call */
    if (uber_ptr->dirty)
    {
        scene_renderer_uber_link(uber);

        ASSERT_DEBUG_SYNC(!uber_ptr->dirty,
                          "Linking an scene_renderer_uber instance did not reset the dirty flag");
    }

    /* Set up UB contents & texture samplers */
    unsigned int n_items = 0;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_items);

    ral_command_buffer_start_recording(uber_ptr->preamble_command_buffer);
    {
        /* Activate the uber program */
        ral_command_buffer_record_set_program(uber_ptr->preamble_command_buffer,
                                              uber_ptr->program);

        for (unsigned int n_item = 0;
                          n_item < n_items;
                        ++n_item)
        {
            _scene_renderer_uber_item* item_ptr = nullptr;

            if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                       n_item,
                                                      &item_ptr) )
            {
                switch (item_ptr->type)
                {
                    case SCENE_RENDERER_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE:
                    {
                        /* UB not used */
                        break;
                    }

                    case SCENE_RENDERER_UBER_ITEM_LIGHT:
                    {
                        if (item_ptr->fragment_shader_item.current_light_shadow_map_color_uniform_name != nullptr)
                        {
                            ral_sampler                                 shadow_map_sampler        = nullptr;
                            ral_texture_type                            shadow_map_texture_type   = RAL_TEXTURE_TYPE_UNKNOWN;
                            ral_command_buffer_set_binding_command_info sm_binding_info;

                            /* Bind the shadow map */
                            ASSERT_DEBUG_SYNC(false,
                                              "TODO: Sampler binding set-up");

                            ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_color != nullptr,
                                              "No color shadow map assigned to a light which casts shadows");

                            ral_texture_view_get_property(item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_color,
                                                          RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                                         &shadow_map_texture_type);

                            sm_binding_info.binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
                            sm_binding_info.name                               = item_ptr->fragment_shader_item.current_light_shadow_map_color_uniform_name;
                            sm_binding_info.sampled_image_binding.sampler      = shadow_map_sampler;
                            sm_binding_info.sampled_image_binding.texture_view = item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_color;

                            ral_command_buffer_record_set_bindings(uber_ptr->preamble_command_buffer,
                                                                   1, /* n_bindings */
                                                                  &sm_binding_info);
                        }

                        if (item_ptr->fragment_shader_item.current_light_shadow_map_depth_uniform_name != nullptr)
                        {
                            ral_sampler                                 shadow_map_sampler      = nullptr;
                            ral_texture_type                            shadow_map_texture_type = RAL_TEXTURE_TYPE_UNKNOWN;
                            ral_command_buffer_set_binding_command_info sm_binding_info;

                            /* Bind the shadow map */
                            ASSERT_DEBUG_SYNC(false,
                                              "TODO: Sampler binding set-up");

                            ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_depth != nullptr,
                                              "No depth shadow map assigned to a light which casts shadows");

                            ral_texture_view_get_property(item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_depth,
                                                          RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                                         &shadow_map_texture_type);

                            sm_binding_info.binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
                            sm_binding_info.name                               = item_ptr->fragment_shader_item.current_light_shadow_map_depth_uniform_name;
                            sm_binding_info.sampled_image_binding.sampler      = shadow_map_sampler;
                            sm_binding_info.sampled_image_binding.texture_view = item_ptr->fragment_shader_item.current_light_shadow_map_texture_view_depth;

                            ral_command_buffer_record_set_bindings(uber_ptr->preamble_command_buffer,
                                                                   1, /* n_bindings */
                                                                  &sm_binding_info);
                        }

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized uber item type");
                    }
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve uber item descriptor at index [%d]", n_item);
            }
        }

        /* Configure uniform buffer bindings */
        ral_buffer fs_ub_bo_ral = nullptr;
        ral_buffer vs_ub_bo_ral = nullptr;

        if (uber_ptr->ub_fs != nullptr)
        {
            ral_program_block_buffer_get_property(uber_ptr->ub_fs,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &fs_ub_bo_ral);

            if (uber_ptr->ub_fs_bo_size != 0)
            {
                ral_command_buffer_set_binding_command_info ub_fs_binding_info;

                ub_fs_binding_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
                ub_fs_binding_info.name                          = system_hashed_ansi_string_create(_scene_renderer_uber_block_name_ub_fs);
                ub_fs_binding_info.uniform_buffer_binding.buffer = fs_ub_bo_ral;
                ub_fs_binding_info.uniform_buffer_binding.offset = 0;
                ub_fs_binding_info.uniform_buffer_binding.size   = uber_ptr->ub_fs_bo_size;

                ral_command_buffer_record_set_bindings(uber_ptr->preamble_command_buffer,
                                                       1, /* n_bindings */
                                                      &ub_fs_binding_info);
            }
        }

        if (uber_ptr->ub_vs != nullptr)
        {
            ral_program_block_buffer_get_property(uber_ptr->ub_vs,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &vs_ub_bo_ral);

            if (uber_ptr->ub_vs_bo_size != 0)
            {
                ral_command_buffer_set_binding_command_info ub_vs_binding_info;

                ub_vs_binding_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
                ub_vs_binding_info.name                          = system_hashed_ansi_string_create(_scene_renderer_uber_block_name_ub_vs);
                ub_vs_binding_info.uniform_buffer_binding.buffer = vs_ub_bo_ral;
                ub_vs_binding_info.uniform_buffer_binding.offset = 0;
                ub_vs_binding_info.uniform_buffer_binding.size   = uber_ptr->ub_vs_bo_size;

                ral_command_buffer_record_set_bindings(uber_ptr->preamble_command_buffer,
                                                       1, /* n_bindings */
                                                      &ub_vs_binding_info);
            }
        }
    }
    ral_command_buffer_stop_recording(uber_ptr->preamble_command_buffer);

    /* Mark as 'being rendered' */
    uber_ptr->is_rendering = true;
}

/* Please see header for specification */
PUBLIC ral_present_task scene_renderer_uber_rendering_stop(scene_renderer_uber uber)
{
    ral_present_task      result_task = nullptr;
    _scene_renderer_uber* uber_ptr    = reinterpret_cast<_scene_renderer_uber*>(uber);
    ral_buffer            ub_fs_bo    = nullptr;
    ral_buffer            ub_vs_bo    = nullptr;

    ASSERT_DEBUG_SYNC(uber_ptr->is_rendering,
                      "Not started");

    if (uber_ptr->ub_fs != nullptr)
    {
        ral_program_block_buffer_get_property(uber_ptr->ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_fs_bo);
    }

    if (uber_ptr->ub_vs != nullptr)
    {
        ral_program_block_buffer_get_property(uber_ptr->ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_vs_bo);
    }

    /* Form the result present task */
    ral_present_task                     global_cpu_update_task;
    ral_present_task_cpu_create_info     global_cpu_update_task_info;
    ral_present_task_io                  global_cpu_update_task_output;
    uint32_t                             n_render_command_buffers           = 0;
    uint32_t                             n_render_unique_inputs             = 0;
    uint32_t                             n_render_unique_outputs            = 0;
    uint32_t                             n_scheduled_mesh_buffers           = 0;
    ral_present_task*                    render_gpu_tasks                   = nullptr;
    ral_present_task_io*                 render_gpu_task_unique_inputs      = nullptr;
    ral_present_task_io                  render_gpu_task_unique_outputs[2];
    ral_present_task*                    result_present_tasks               = nullptr;
    ral_present_task_group_create_info   result_task_create_info;
    ral_present_task_ingroup_connection* result_task_ingroup_connections    = nullptr;
    ral_present_task_group_mapping*      result_task_unique_input_mappings  = nullptr;
    ral_present_task_group_mapping*      result_task_unique_output_mappings = nullptr;

    system_resizable_vector_get_property(uber_ptr->scheduled_mesh_buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_scheduled_mesh_buffers);
    system_resizable_vector_get_property(uber_ptr->scheduled_mesh_command_buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_render_command_buffers);

    ASSERT_DEBUG_SYNC(n_render_command_buffers > 0,
                      "Zero uber meshes recorded for rendering.");

    global_cpu_update_task_output.buffer      = ub_vs_bo;
    global_cpu_update_task_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    global_cpu_update_task_info.cpu_task_callback_user_arg = uber_ptr;
    global_cpu_update_task_info.n_unique_inputs            = 0;
    global_cpu_update_task_info.n_unique_outputs           = 1;
    global_cpu_update_task_info.pfn_cpu_task_callback_proc = _scene_renderer_uber_start_rendering_cpu_task_callback;
    global_cpu_update_task_info.unique_inputs              = nullptr;
    global_cpu_update_task_info.unique_outputs             = &global_cpu_update_task_output;

    global_cpu_update_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Uber: CPU update"),
                                                        &global_cpu_update_task_info);


    render_gpu_task_unique_inputs    = reinterpret_cast<ral_present_task_io*>(_malloca(sizeof(ral_present_task_io) * (3 /* UB buffers + color rt (potentially) + ds rt (potentially) */ + n_scheduled_mesh_buffers) ));
    render_gpu_task_unique_inputs[0] = global_cpu_update_task_output;
    n_render_unique_inputs           = 1;

    /* Render tasks need to take color/depth RTs as inputs & outputs. If any of the tasks depends on any other input (eg. buffers),
     * we also need to expose it as well. This is especially important for custom meshes which may rely on data generated prior to
     * scene rasterization.
     *
     * TODO: Right now, all render tasks for a given uber will depend on all buffers used by meshes assigned to a given uber.
     *       This is not needed. While it won't matter for OpenGL, making these dependencies explicit could potentially reduce
     *       the number of buffer barriers in Vulkan.
     */
    if (uber_ptr->active_color_rt != nullptr)
    {
        render_gpu_task_unique_outputs[n_render_unique_outputs].texture_view = uber_ptr->active_color_rt;
        render_gpu_task_unique_outputs[n_render_unique_outputs].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        render_gpu_task_unique_inputs [n_render_unique_inputs]               = render_gpu_task_unique_outputs[n_render_unique_outputs];

        ++n_render_unique_inputs;
        ++n_render_unique_outputs;
    }

    if (uber_ptr->active_depth_rt != nullptr)
    {
        render_gpu_task_unique_outputs[n_render_unique_outputs].texture_view = uber_ptr->active_depth_rt;
        render_gpu_task_unique_outputs[n_render_unique_outputs].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        render_gpu_task_unique_inputs [n_render_unique_inputs]               = render_gpu_task_unique_outputs[n_render_unique_outputs];

        ++n_render_unique_inputs;
        ++n_render_unique_outputs;
    }

    for (uint32_t n_scheduled_mesh_buffer = 0;
                  n_scheduled_mesh_buffer < n_scheduled_mesh_buffers;
                ++n_scheduled_mesh_buffer)
    {
        system_resizable_vector_get_element_at(uber_ptr->scheduled_mesh_buffers,
                                               n_scheduled_mesh_buffer,
                                              &render_gpu_task_unique_inputs[n_render_unique_inputs].object);

        render_gpu_task_unique_inputs[n_render_unique_inputs++].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    }

    render_gpu_tasks = reinterpret_cast<ral_present_task*>(_malloca(n_render_command_buffers * sizeof(ral_present_task) ));

    for (uint32_t n_render_command_buffer = 0;
                  n_render_command_buffer < n_render_command_buffers;
                ++n_render_command_buffer)
    {
        ral_command_buffer               render_command_buffer = nullptr;
        ral_present_task_gpu_create_info render_gpu_task_info;

        system_resizable_vector_get_element_at(uber_ptr->scheduled_mesh_command_buffers,
                                               n_render_command_buffer,
                                              &render_command_buffer);

        render_gpu_task_info.command_buffer   = render_command_buffer;
        render_gpu_task_info.n_unique_inputs  = n_render_unique_inputs;
        render_gpu_task_info.n_unique_outputs = n_render_unique_outputs;
        render_gpu_task_info.unique_inputs    = render_gpu_task_unique_inputs;
        render_gpu_task_info.unique_outputs   = render_gpu_task_unique_outputs;

        render_gpu_tasks[n_render_command_buffer] = ral_present_task_create_gpu(system_hashed_ansi_string_create("Uber render command buffer: rasterization"),
                                                                               &render_gpu_task_info);
    }


    /* Result task needs to:
     *
     * 1) expose rendertargets both as input & output
     * 2) expose any buffers required by meshes to render as inputs.
     *
     * Step 2) is simple to solve right now, due to the TODO above, but will become cumbersome
     * as soon as we improve the situation, because that is going to mean we will suddenly need
     * to map result task buffer inputs onto corresponding mesh render tasks' inputs. That could
     * be a lot of work for scenes with large numbers of geometry. Current implementation is undeniably
     * hamfisted and will need to be optimized in the future.
     **/
    result_task_create_info.n_ingroup_connections                    = n_render_command_buffers;
    result_task_create_info.n_present_tasks                          = 1 + n_render_command_buffers;
    result_task_create_info.n_total_unique_inputs                    = (n_render_unique_inputs - 1 /* ub_vs_bo */);
    result_task_create_info.n_total_unique_outputs                   = n_render_unique_outputs;
    result_task_create_info.n_unique_input_to_ingroup_task_mappings  = result_task_create_info.n_total_unique_inputs * n_render_command_buffers;
    result_task_create_info.n_unique_output_to_ingroup_task_mappings = n_render_unique_outputs * n_render_command_buffers;

    result_task_ingroup_connections = reinterpret_cast<ral_present_task_ingroup_connection*>(_malloca(result_task_create_info.n_ingroup_connections * sizeof(ral_present_task_ingroup_connection)) );

    for (uint32_t n_ingroup_connection = 0;
                  n_ingroup_connection < result_task_create_info.n_ingroup_connections;
                ++n_ingroup_connection)
    {
        /* Make sure the UB used by FS is executed after the CPU-side update takes place */
        ral_present_task_ingroup_connection& current_connection = result_task_ingroup_connections[n_ingroup_connection];

        current_connection.input_present_task_index     = 1 + n_ingroup_connection;
        current_connection.input_present_task_io_index  = 0;
        current_connection.output_present_task_index    = 0;
        current_connection.output_present_task_io_index = 0;
    }

    result_present_tasks    = reinterpret_cast<ral_present_task*> (_malloca(result_task_create_info.n_present_tasks * sizeof(ral_present_task) ));
    result_present_tasks[0] = global_cpu_update_task;

    memcpy(result_present_tasks + 1,
           render_gpu_tasks,
           sizeof(ral_present_task) * (result_task_create_info.n_present_tasks - 1));


    result_task_unique_input_mappings  = reinterpret_cast<ral_present_task_group_mapping*>(_malloca(result_task_create_info.n_unique_input_to_ingroup_task_mappings  * sizeof(ral_present_task_group_mapping) ));
    result_task_unique_output_mappings = reinterpret_cast<ral_present_task_group_mapping*>(_malloca(result_task_create_info.n_unique_output_to_ingroup_task_mappings * sizeof(ral_present_task_group_mapping) ));

    for (uint32_t n_mapping = 0;
                  n_mapping < result_task_create_info.n_unique_input_to_ingroup_task_mappings;
                ++n_mapping)
    {
        result_task_unique_input_mappings[n_mapping].group_task_io_index   =     n_mapping % (n_render_unique_inputs - 1);
        result_task_unique_input_mappings[n_mapping].present_task_io_index = 1 + n_mapping % (n_render_unique_inputs - 1);
        result_task_unique_input_mappings[n_mapping].n_present_task        = 1 + n_mapping / (n_render_unique_inputs - 1);
    }

    for (uint32_t n_mapping = 0;
                  n_mapping < result_task_create_info.n_unique_output_to_ingroup_task_mappings;
                ++n_mapping)
    {
        result_task_unique_output_mappings[n_mapping].group_task_io_index   =     n_mapping % n_render_unique_outputs;
        result_task_unique_output_mappings[n_mapping].present_task_io_index =     n_mapping % n_render_unique_outputs;
        result_task_unique_output_mappings[n_mapping].n_present_task        = 1 + n_mapping / n_render_unique_outputs;
    }

    result_task_create_info.ingroup_connections                      = result_task_ingroup_connections;
    result_task_create_info.present_tasks                            = result_present_tasks;
    result_task_create_info.unique_input_to_ingroup_task_mapping     = result_task_unique_input_mappings;
    result_task_create_info.unique_output_to_ingroup_task_mapping    = result_task_unique_output_mappings;

    result_task = ral_present_task_create_group(system_hashed_ansi_string_create("Uber: rasterization"),
                                                &result_task_create_info);

    /* Mark as no longer rendered */
    uber_ptr->is_rendering = false;

    /* Clean up */
    uint32_t            n_scheduled_cmd_buffers = 0;
    ral_command_buffer* scheduled_cmd_buffers   = nullptr;

    for (uint32_t n_present_task = 0;
                  n_present_task < result_task_create_info.n_present_tasks;
                ++n_present_task)
    {
        ral_present_task_release(result_present_tasks[n_present_task]);
    }

    _freea(render_gpu_task_unique_inputs);
    _freea(render_gpu_tasks);
    _freea(result_task_ingroup_connections);
    _freea(result_present_tasks);
    _freea(result_task_unique_input_mappings);
    _freea(result_task_unique_output_mappings);

    system_resizable_vector_get_property(uber_ptr->scheduled_mesh_command_buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_scheduled_cmd_buffers);
    system_resizable_vector_get_property(uber_ptr->scheduled_mesh_command_buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                         &scheduled_cmd_buffers);

    if (n_scheduled_cmd_buffers > 0)
    {
        ral_context_delete_objects(uber_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                   n_scheduled_cmd_buffers,
                                   reinterpret_cast<void* const*>(scheduled_cmd_buffers) );
    }

    system_resizable_vector_clear(uber_ptr->scheduled_mesh_buffers);
    system_resizable_vector_clear(uber_ptr->scheduled_mesh_command_buffers);

    return result_task;
}
