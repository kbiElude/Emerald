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
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader_constructor.h"
#include "ogl/ogl_uber.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "scene/scene.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
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
    GLint ambient_color_ub_offset;

    GLint current_light_attenuations_ub_offset;
    GLint current_light_cone_angle_ub_offset; /* cone angle: 1/2 total light cone. used for spot lights */
    GLint current_light_diffuse_ub_offset;
    GLint current_light_direction_ub_offset;
    GLint current_light_edge_angle_ub_offset; /* angular width of the soft edge. used for spot lights */
    GLint current_light_far_near_diff_ub_offset;
    GLint current_light_location_ub_offset;
    GLint current_light_near_plane_ub_offset;
    GLint current_light_projection_ub_offset;
    GLint current_light_range_ub_offset;
    GLint current_light_shadow_map_vsm_cutoff_ub_offset;
    GLint current_light_shadow_map_vsm_min_variance_ub_offset;
    GLint current_light_view_ub_offset;

    ral_texture current_light_shadow_map_texture_color;
    GLuint      current_light_shadow_map_texture_color_sampler_location;
    ral_texture current_light_shadow_map_texture_depth;
    GLuint      current_light_shadow_map_texture_depth_sampler_location;

    _ogl_uber_fragment_shader_item()
    {
        ambient_color_ub_offset                                 = -1;
        current_light_attenuations_ub_offset                    = -1;
        current_light_cone_angle_ub_offset                      = -1;
        current_light_diffuse_ub_offset                         = -1;
        current_light_direction_ub_offset                       = -1;
        current_light_edge_angle_ub_offset                      = -1;
        current_light_far_near_diff_ub_offset                   = -1;
        current_light_location_ub_offset                        = -1;
        current_light_near_plane_ub_offset                      = -1;
        current_light_projection_ub_offset                      = -1;
        current_light_range_ub_offset                           = -1;
        current_light_shadow_map_texture_color_sampler_location = -1;
        current_light_shadow_map_texture_depth_sampler_location = -1;
        current_light_shadow_map_vsm_cutoff_ub_offset           = -1;
        current_light_shadow_map_vsm_min_variance_ub_offset     = -1;
        current_light_view_ub_offset                            = -1;

        current_light_shadow_map_texture_color = NULL;
        current_light_shadow_map_texture_depth = NULL;
    }
} _ogl_uber_fragment_shader_item;

/* Holds all properties that may be used for a vertex shader item */
typedef struct _ogl_uber_vertex_shader_item
{
    GLint current_light_depth_vp_ub_offset; /* row-major */

    _ogl_uber_light_sh_data current_light_sh_data;
    GLint                   current_light_sh_data_ub_offset;

    _ogl_uber_vertex_shader_item()
    {
        current_light_depth_vp_ub_offset = -1;
    }

} _ogl_uber_vertex_shader_item;

/* A single item added to ogl_uber used to construct a program object managed by ogl_uber */
typedef struct _ogl_uber_item
{
    scene_light_falloff                         falloff;
    shaders_fragment_uber_item_id               fs_item_id;
    _ogl_uber_fragment_shader_item              fragment_shader_item;
    bool                                        is_shadow_caster;
    scene_light_shadow_map_algorithm            shadow_map_algorithm;
    scene_light_shadow_map_bias                 shadow_map_bias;
    scene_light_shadow_map_pointlight_algorithm shadow_map_pointlight_algorithm;
    _ogl_uber_vertex_shader_item                vertex_shader_item;
    shaders_vertex_uber_item_id                 vs_item_id;

    _ogl_uber_item_type type;

    _ogl_uber_item()
    {
        falloff                         = SCENE_LIGHT_FALLOFF_UNKNOWN;
        fs_item_id                      = -1;
        is_shadow_caster                = false;
        shadow_map_algorithm            = SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN;
        shadow_map_bias                 = SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN;
        shadow_map_pointlight_algorithm = SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_UNKNOWN;
        type                            = OGL_UBER_ITEM_UNKNOWN;
        vs_item_id                      = -1;
    }
} _ogl_uber_item;

typedef enum
{
    /* Created with ogl_uber_create() */
    OGL_UBER_TYPE_REGULAR,

    /* Created with ogl_uber_create_from_ogl_program() */
    OGL_UBER_TYPE_OGL_PROGRAM_DRIVEN
} _ogl_uber_type;

/* A single item that defines a VAO configured for a particular mesh and current
 * ogl_uber instance.
 */
typedef struct __ogl_uber_vao
{
    system_time mesh_modification_timestamp;
    GLuint      vao_id;

    __ogl_uber_vao()
    {
        mesh_modification_timestamp = 0;
        vao_id                      = 0;
    }
} _ogl_uber_vao;

typedef struct _ogl_uber
{
    ral_context               context;
    system_hashed_ansi_string name;

    ral_program               program;
    shaders_fragment_uber     shader_fragment;
    uint32_t                  shader_fragment_n_items;
    shaders_vertex_uber       shader_vertex;

    uint32_t                  ambient_material_sampler_uniform_location;
    uint32_t                  diffuse_material_sampler_uniform_location;
    uint32_t                  luminosity_material_sampler_uniform_location;
    uint32_t                  mesh_sh3_uniform_location;             /* TODO: clean-up */
    uint32_t                  mesh_sh3_data_offset_uniform_location; /* TODO: clean-up */
    uint32_t                  mesh_sh4_uniform_location;             /* TODO: clean-up */
    uint32_t                  mesh_sh4_data_offset_uniform_location; /* TODO: clean-up */
    uint32_t                  object_normal_attribute_location;
    uint32_t                  object_uv_attribute_location;
    uint32_t                  object_vertex_attribute_location;
    uint32_t                  shininess_material_sampler_uniform_location;
    uint32_t                  specular_material_sampler_uniform_location;

    /* These are stored in UBs so we need to store UB offsets instead of locations */
    uint32_t                  ambient_material_ub_offset;
    uint32_t                  diffuse_material_ub_offset;
    uint32_t                  far_near_plane_diff_ub_offset;
    uint32_t                  flip_z_ub_offset;
    uint32_t                  luminosity_material_ub_offset;
    uint32_t                  max_variance_ub_offset;
    uint32_t                  model_ub_offset;         /* column major ordering! */
    uint32_t                  near_plane_ub_offset;
    uint32_t                  normal_matrix_ub_offset; /* column-major ordering! */
    uint32_t                  shininess_material_ub_offset;
    uint32_t                  specular_material_ub_offset;
    uint32_t                  world_camera_ub_offset;
    uint32_t                  vp_ub_offset;

    bool                      is_rendering;
    uint32_t                  n_texture_units_assigned;
    ogl_program_ub            ub_fs;
    ral_buffer                ub_fs_bo;
    GLuint                    ub_fs_bo_size;
    ogl_program_ub            ub_vs;
    ral_buffer                ub_vs_bo;
    GLuint                    ub_vs_bo_size;

    system_matrix4x4          current_vp;
    float                     current_vsm_max_variance;

    system_resizable_vector   added_items; /* holds _ogl_uber_item instances */
    bool                      dirty;

    system_matrix4x4          graph_rendering_current_matrix;
    _ogl_uber_type            type;
    system_variant            variant_float;

    system_hash64map mesh_to_vao_descriptor_map;

    REFCOUNT_INSERT_VARIABLES

    explicit _ogl_uber(ral_context               context,
                       system_hashed_ansi_string name,
                       _ogl_uber_type            type);
} _ogl_uber;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_uber,
                               ogl_uber,
                              _ogl_uber);

/** Forward declarations */
PRIVATE void _ogl_uber_add_item_shaders_fragment_callback_handler(_shaders_fragment_uber_parent_callback_type type,
                                                                  void*                                       data,
                                                                  void*                                       uber);
PRIVATE void _ogl_uber_link_renderer_callback                    (ogl_context                                 context,
                                                                  void*                                       arg);
PRIVATE void _ogl_uber_release                                   (void*                                       uber);
PRIVATE void _ogl_uber_release_renderer_callback                 (ogl_context                                 context,
                                                                  void*                                       arg);
PRIVATE void _ogl_uber_reset_attribute_uniform_locations         (_ogl_uber*                                  uber_ptr);


/** Internal variables */

/** TODO */
_ogl_uber::_ogl_uber(ral_context               in_context,
                     system_hashed_ansi_string in_name,
                     _ogl_uber_type            in_type)
{
    added_items                    = system_resizable_vector_create(4 /* capacity */);
    context                        = in_context;    /* DO NOT retain, or face circular dependencies! */
    current_vp                     = system_matrix4x4_create();
    dirty                          = true;
    graph_rendering_current_matrix = system_matrix4x4_create();
    is_rendering                   = false;
    mesh_to_vao_descriptor_map     = system_hash64map_create(sizeof(_ogl_uber_vao*),
                                                             false);
    name                           = in_name;
    n_texture_units_assigned       = 0;
    program                        = NULL;
    shader_fragment                = NULL;
    shader_vertex                  = NULL;
    type                           = in_type;
    ub_fs                          = NULL;
    ub_vs                          = NULL;

    _ogl_uber_reset_attribute_uniform_locations(this);
}


/** TODO */
PRIVATE void _ogl_uber_add_item_shaders_fragment_callback_handler(_shaders_fragment_uber_parent_callback_type type,
                                                                  void*                                       data,
                                                                  void*                                       uber)
{
    switch (type)
    {
        case SHADERS_FRAGMENT_UBER_PARENT_CALLBACK_NEW_FRAGMENT_INPUT:
        {
            const _shaders_fragment_uber_new_fragment_input_callback* callback_data = (const _shaders_fragment_uber_new_fragment_input_callback*) data;

            ASSERT_DEBUG_SYNC(callback_data != NULL,
                              "Callback data pointer is NULL");

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
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized callback from shaders_fragment_uber object");
        }
    } /* switch (type) */
}

/** TODO */
PRIVATE void _ogl_uber_bake_mesh_vao(_ogl_uber* uber_ptr,
                                     mesh       mesh)
{
    /* NOTE: The default n_components values are needed for meshes loaded from serialized blobs.
     *       For efficiency, the data streams are not stored in the files, but they always use
     *       predefined number of components per stream type
     */
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints                = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints                    = NULL;
    const ogl_context_gl_limits*                              limits                         = NULL;
    mesh_type                                                 mesh_instance_type;
    ral_buffer                                                mesh_normals_bo                = 0;
    unsigned int                                              mesh_normals_bo_n_components   = 3;
    unsigned int                                              mesh_normals_bo_offset         = 0;
    unsigned int                                              mesh_normals_bo_stride         = 0;
    ral_buffer                                                mesh_texcoords_bo              = NULL;
    unsigned int                                              mesh_texcoords_bo_n_components = 2;
    unsigned int                                              mesh_texcoords_bo_offset       = 0;
    unsigned int                                              mesh_texcoords_bo_stride       = 0;
    ral_buffer                                                mesh_vertex_bo                 = NULL;
    unsigned int                                              mesh_vertex_bo_n_components    = 3;
    unsigned int                                              mesh_vertex_bo_offset          = 0;
    unsigned int                                              mesh_vertex_bo_stride          = 0;
    uint32_t                                                  n_layers                       = 0;
    _ogl_uber_vao*                                            vao_ptr                        = NULL;

    ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);
    ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits);

    mesh_get_property(mesh,
                      MESH_PROPERTY_TYPE,
                     &mesh_instance_type);

    /* Sanity check: make sure GPU stream meshes only define up to one layer. We'd need one VAO
     *               per GPU stream mesh layer and that does not fit nicely into the current
     *               architecture, so if you ever step upon this assertion failure, I have some
     *               very interesting homework for you.
     *
     * For regular meshes, this is not a problem. A single VAO can be used for all layers just fine.
     */
    if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
    {
        unsigned int n_mesh_layers = 0;

        mesh_get_property(mesh,
                          MESH_PROPERTY_N_LAYERS,
                         &n_mesh_layers);

        ASSERT_DEBUG_SYNC(n_mesh_layers <= 1,
                          "TODO");
    } /* if (mesh_instance_type == MESH_TYPE_GPU_STREAM) */

    /* Create a VAO if one is needed */
    if (!system_hash64map_get(uber_ptr->mesh_to_vao_descriptor_map,
                              (system_hash64) mesh,
                             &vao_ptr) )
    {
        vao_ptr = new (std::nothrow) _ogl_uber_vao;

        ASSERT_ALWAYS_SYNC(vao_ptr != NULL,
                           "Out of memory");

        if (vao_ptr == NULL)
        {
            goto end;
        }

        entrypoints->pGLGenVertexArrays(1,
                                       &vao_ptr->vao_id);
    }

    /* Retrieve buffer object storage properties. */
    if (mesh_instance_type == MESH_TYPE_REGULAR)
    {
        /* Regular meshes use the same stride and BO ID for all streams */
        mesh_get_property(mesh,
                          MESH_PROPERTY_GL_BO_RAL,
                         &mesh_texcoords_bo);
        mesh_get_property(mesh,
                          MESH_PROPERTY_GL_STRIDE,
                         &mesh_texcoords_bo_stride);

        mesh_normals_bo        = mesh_texcoords_bo;
        mesh_normals_bo_stride = mesh_texcoords_bo_stride;
        mesh_vertex_bo         = mesh_texcoords_bo;
        mesh_vertex_bo_stride  = mesh_texcoords_bo_stride;
    }

    if (uber_ptr->object_normal_attribute_location != -1)
    {
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_normals_bo_offset);
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,
                                           &mesh_normals_bo_n_components);

        if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
        {
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL,
                                               &mesh_normals_bo);
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE,
                                               &mesh_normals_bo_stride);
        }

        raGL_buffer mesh_normals_bo_raGL = NULL;
        GLuint      mesh_normals_bo_id    = 0;

        mesh_normals_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                         mesh_normals_bo);

        raGL_buffer_get_property(mesh_normals_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &mesh_normals_bo_id);

        dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                             mesh_normals_bo_id,
                                                             uber_ptr->object_normal_attribute_location,
                                                             mesh_normals_bo_n_components,
                                                             GL_FLOAT,
                                                             GL_FALSE,
                                                             mesh_normals_bo_stride,
                                                             mesh_normals_bo_offset);

        dsa_entrypoints->pGLEnableVertexArrayAttribEXT(vao_ptr->vao_id,
                                                       uber_ptr->object_normal_attribute_location);
    } /* if (uber_ptr->object_normal_attribute_location != -1) */

    if (uber_ptr->object_uv_attribute_location != -1)
    {
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_texcoords_bo_offset);
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
                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL,
                                               &mesh_texcoords_bo);
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE,
                                               &mesh_texcoords_bo_stride);
        }
    } /* if (uber_ptr->object_uv_attribute_location != -1) */

    if (uber_ptr->object_vertex_attribute_location != -1)
    {
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                           &mesh_vertex_bo_offset);
        mesh_get_layer_data_stream_property(mesh,
                                            0, /* layer_id - please see "sanity check" comment above for explanation */
                                            MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,
                                           &mesh_vertex_bo_n_components);

        if (mesh_instance_type == MESH_TYPE_GPU_STREAM)
        {
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL,
                                               &mesh_vertex_bo);
            mesh_get_layer_data_stream_property(mesh,
                                                0, /* layer_id - please see "sanity check" comment above for explanation */
                                                MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE,
                                               &mesh_vertex_bo_stride);
        }

        GLuint      mesh_vertex_bo_id   = 0;
        raGL_buffer mesh_vertex_bo_raGL = NULL;

        mesh_vertex_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                        mesh_vertex_bo);

        raGL_buffer_get_property(mesh_vertex_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &mesh_vertex_bo_id);

        dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                             mesh_vertex_bo_id,
                                                             uber_ptr->object_vertex_attribute_location,
                                                             mesh_vertex_bo_n_components,
                                                             GL_FLOAT,
                                                             GL_FALSE,
                                                             mesh_vertex_bo_stride,
                                                             mesh_vertex_bo_offset);

        dsa_entrypoints->pGLEnableVertexArrayAttribEXT(vao_ptr->vao_id,
                                                       uber_ptr->object_vertex_attribute_location);
    } /* if (uber_ptr->object_vertex_attribute_location != -1) */

    /* A few sanity checks never hurt anyone.. */
    ASSERT_ALWAYS_SYNC(vao_ptr->vao_id != 0,
                       "VAO is 0");
    ASSERT_ALWAYS_SYNC(mesh_vertex_bo != NULL,
                      "Mesh BO is NULL");

    /* Bind the VAO before we move on */
    entrypoints->pGLBindVertexArray(vao_ptr->vao_id);

    /* Bind the BO as a data source for the indexed draw calls, if we're dealing with a regular mesh */
    if (mesh_instance_type == MESH_TYPE_REGULAR)
    {
        GLuint      mesh_vertex_bo_id   = 0;
        raGL_buffer mesh_vertex_bo_raGL = NULL;

        mesh_vertex_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                        mesh_vertex_bo);

        raGL_buffer_get_property(mesh_vertex_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &mesh_vertex_bo_id);

        entrypoints->pGLBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                   mesh_vertex_bo_id);
    }

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
            GLint         attribute_location;
            const GLvoid* offset;
            int           size;
        } attribute_items[] =
        {
            {
                uber_ptr->object_uv_attribute_location,
                (const GLvoid*) (intptr_t) mesh_texcoords_bo_offset,
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
                GLuint      mesh_texcoords_bo_id   = 0;
                raGL_buffer mesh_texcoords_bo_raGL = NULL;

                mesh_texcoords_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                                   mesh_texcoords_bo);

                ASSERT_DEBUG_SYNC(mesh_texcoords_bo != NULL,
                                  "Material requires texture coordinates, but none are provided by the mesh.");

                raGL_buffer_get_property(mesh_texcoords_bo_raGL,
                                         RAGL_BUFFER_PROPERTY_ID,
                                        &mesh_texcoords_bo_id);

                dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                                     mesh_texcoords_bo_id,
                                                                     item.attribute_location,
                                                                     item.size,
                                                                     GL_FLOAT,
                                                                     GL_FALSE,
                                                                     mesh_texcoords_bo_stride,
                                                                     (GLintptr) item.offset);
                dsa_entrypoints->pGLEnableVertexArrayAttribEXT      (vao_ptr->vao_id,
                                                                     item.attribute_location);
            }
        } /* for (all attribute items) */

        /* Iterate over all layer passes */
        for (uint32_t n_layer_pass = 0;
                      n_layer_pass < n_layer_passes;
                    ++n_layer_pass)
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
                int32_t                        shader_uv_attribute_location;
            } attachments[] =
            {
                {
                    MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                    uber_ptr->object_uv_attribute_location
                },

                {
                    MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                    uber_ptr->object_uv_attribute_location
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
                        if (attachment.shader_uv_attribute_location != -1)
                        {
                            GLuint      mesh_texcoords_bo_id   = 0;
                            raGL_buffer mesh_texcoords_bo_raGL = NULL;

                            ASSERT_DEBUG_SYNC(mesh_texcoords_bo != NULL,
                                              "Material requires texture coordinates, but none are provided by the mesh.");

                            mesh_texcoords_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                                               mesh_texcoords_bo);

                            raGL_buffer_get_property(mesh_texcoords_bo_raGL,
                                                     RAGL_BUFFER_PROPERTY_ID,
                                                    &mesh_texcoords_bo_id);

                            dsa_entrypoints->pGLVertexArrayVertexAttribOffsetEXT(vao_ptr->vao_id,
                                                                                 mesh_texcoords_bo_id,
                                                                                 attachment.shader_uv_attribute_location,
                                                                                 mesh_texcoords_bo_n_components,
                                                                                 GL_FLOAT,
                                                                                 GL_FALSE,
                                                                                 mesh_texcoords_bo_stride,
                                                                                 (GLintptr) mesh_texcoords_bo_offset);
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
PRIVATE void _ogl_uber_link_renderer_callback(ogl_context context,
                                              void*       arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_limits*                              limits_ptr       = NULL;
    _ogl_uber*                                                uber_ptr         = (_ogl_uber*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    LOG_INFO("Relinking an uber object instance");

    /* Set up UBO bindings */
    const raGL_program program_raGL                           = ral_context_get_program_gl(uber_ptr->context,
                                                                                           uber_ptr->program);
    uint32_t           program_raGL_id                        = 0;
    uint32_t           fragment_shader_properties_block_index = 0;
    uint32_t           vertex_shader_properties_block_index   = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);


    fragment_shader_properties_block_index = entry_points->pGLGetUniformBlockIndex(program_raGL_id,
                                                                                   "FragmentShaderProperties");
    vertex_shader_properties_block_index   = entry_points->pGLGetUniformBlockIndex(program_raGL_id,
                                                                                   "VertexShaderProperties");


    if (fragment_shader_properties_block_index != -1)
    {
        entry_points->pGLUniformBlockBinding(program_raGL_id,
                                             fragment_shader_properties_block_index,
                                             FRAGMENT_SHADER_PROPERTIES_UBO_BINDING_ID);
    }
    if (vertex_shader_properties_block_index != -1)
    {
        entry_points->pGLUniformBlockBinding(program_raGL_id,
                                             vertex_shader_properties_block_index,
                                             VERTEX_SHADER_PROPERTIES_UBO_BINDING_ID);
    }
}

/** TODO */
PRIVATE void _ogl_uber_release(void* uber)
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

            uber_ptr->shader_fragment = NULL;
        }

        if (uber_ptr->shader_vertex != NULL)
        {
            shaders_vertex_uber_release(uber_ptr->shader_vertex);

            uber_ptr->shader_vertex = NULL;
        }

        if (uber_ptr->program != NULL)
        {
            ral_context_delete_objects(uber_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                       1, /* n_objects */
                                       (const void**) &uber_ptr->program);

            uber_ptr->program = NULL;
        }

        if (uber_ptr->mesh_to_vao_descriptor_map != NULL)
        {
            ASSERT_DEBUG_SYNC(uber_ptr->context != NULL,
                              "Rendering context is NULL");

            ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(uber_ptr->context),
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
PRIVATE void _ogl_uber_release_renderer_callback(ogl_context context,
                                                 void*       arg)
{
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    _ogl_uber*                        uber_ptr    = (_ogl_uber*) arg;
    unsigned int                      n_vaos      = 0;

    system_hash64map_get_property(uber_ptr->mesh_to_vao_descriptor_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_vaos);

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
PRIVATE void _ogl_uber_reset_attribute_uniform_locations(_ogl_uber* uber_ptr)
{
    uber_ptr->ambient_material_sampler_uniform_location    = -1;
    uber_ptr->ambient_material_ub_offset                   = -1;
    uber_ptr->diffuse_material_sampler_uniform_location    = -1;
    uber_ptr->diffuse_material_ub_offset                   = -1;
    uber_ptr->far_near_plane_diff_ub_offset                = -1;
    uber_ptr->flip_z_ub_offset                             = -1;
    uber_ptr->luminosity_material_sampler_uniform_location = -1;
    uber_ptr->luminosity_material_ub_offset                = -1;
    uber_ptr->max_variance_ub_offset                       = -1;
    uber_ptr->mesh_sh3_uniform_location                    = -1;
    uber_ptr->mesh_sh3_data_offset_uniform_location        = -1;
    uber_ptr->mesh_sh4_uniform_location                    = -1;
    uber_ptr->mesh_sh4_data_offset_uniform_location        = -1;
    uber_ptr->model_ub_offset                              = -1;
    uber_ptr->near_plane_ub_offset                         = -1;
    uber_ptr->normal_matrix_ub_offset                      = -1;
    uber_ptr->object_normal_attribute_location             = -1;
    uber_ptr->object_uv_attribute_location                 = -1;
    uber_ptr->object_vertex_attribute_location             = -1;
    uber_ptr->shininess_material_sampler_uniform_location  = -1;
    uber_ptr->shininess_material_ub_offset                 = -1;
    uber_ptr->specular_material_sampler_uniform_location   = -1;
    uber_ptr->specular_material_ub_offset                  = -1;
    uber_ptr->vp_ub_offset                                 = -1;
    uber_ptr->world_camera_ub_offset                       = -1;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_uber_item_id ogl_uber_add_input_fragment_attribute_item(ogl_uber                           uber,
                                                                               _ogl_uber_input_fragment_attribute input_attribute)
{
    shaders_fragment_uber_input_attribute_type fs_input_attribute = UBER_INPUT_ATTRIBUTE_UNKNOWN;
    shaders_fragment_uber_item_id              fs_item_id         = -1;
    ogl_uber_item_id                           result             = -1;
    _ogl_uber*                                 uber_ptr           = (_ogl_uber*) uber;

    ASSERT_DEBUG_SYNC(uber_ptr->type == OGL_UBER_TYPE_REGULAR,
                      "ogl_uber_add_input_fragment_attribute_item() is only supported for regular ogl_uber instances.");

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
PUBLIC EMERALD_API ogl_uber_item_id ogl_uber_add_light_item(ogl_uber                         uber,
                                                            scene_light                      light_instance,
                                                            shaders_fragment_uber_light_type light_type,
                                                            bool                             is_shadow_caster,
                                                            unsigned int                     n_light_properties,
                                                            void*                            light_property_values)
{
    _ogl_uber*       uber_ptr     = (_ogl_uber*) uber;
    _ogl_uber_item*  new_item_ptr = NULL;
    ogl_uber_item_id result       = -1;

    ASSERT_DEBUG_SYNC(uber_ptr->type == OGL_UBER_TYPE_REGULAR,
                      "ogl_uber_add_light_item() is only supported for regular ogl_uber instances.");

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
                                                         _ogl_uber_add_item_shaders_fragment_callback_handler,
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
                                                         NULL, /* callback proc - not used */
                                                         NULL  /* callback proc user arg - not used */);
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
    } /* switch (light_type) */

    /* Spawn the descriptor */
    new_item_ptr = new (std::nothrow) _ogl_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != NULL, "Out of memory");
    if (new_item_ptr == NULL)
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
    new_item_ptr->type             = OGL_UBER_ITEM_LIGHT;
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
PUBLIC EMERALD_API ogl_uber ogl_uber_create(ral_context                context,
                                            system_hashed_ansi_string  name)
{
    _ogl_uber* result_ptr = new (std::nothrow) _ogl_uber(context,
                                                         name,
                                                         OGL_UBER_TYPE_REGULAR);

    ASSERT_DEBUG_SYNC(result_ptr != NULL,
                      "Out of memory");

    if (result_ptr != NULL)
    {
        result_ptr->shader_fragment = shaders_fragment_uber_create(context,
                                                                   name);
        result_ptr->shader_vertex   = shaders_vertex_uber_create  (context,
                                                                   name);
        result_ptr->type            = OGL_UBER_TYPE_REGULAR;
        result_ptr->variant_float   = system_variant_create       (SYSTEM_VARIANT_FLOAT);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                       _ogl_uber_release,
                                                       OBJECT_TYPE_OGL_UBER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Ubers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        /* Create a program with the shaders we were provided */
        ral_program_create_info program_create_info;

        program_create_info.active_shader_stages = RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX;
        program_create_info.name                 = name;

        ral_context_create_programs(context,
                                    1, /* n_create_info_items */
                                   &program_create_info,
                                   &result_ptr->program);

        ASSERT_ALWAYS_SYNC(result_ptr->program != NULL,
                           "Cannot instantiate uber program");

        if (result_ptr->program != NULL)
        {
            if (!ral_program_attach_shader(result_ptr->program,
                                           shaders_fragment_uber_get_shader(result_ptr->shader_fragment) ) ||
                !ral_program_attach_shader(result_ptr->program,
                                           shaders_vertex_uber_get_shader(result_ptr->shader_vertex)) )
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Cannot attach shader(s) to uber program");
            }
        } /* if (result->program != NULL) */
    } /* if (result != NULL) */

    return (ogl_uber) result_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_uber ogl_uber_create_from_ral_program(ral_context               context,
                                                             system_hashed_ansi_string name,
                                                             ral_program               program)
{
    _ogl_uber* result_ptr = new (std::nothrow) _ogl_uber(context,
                                                         name,
                                                         OGL_UBER_TYPE_OGL_PROGRAM_DRIVEN);

    ASSERT_DEBUG_SYNC(result_ptr != NULL,
                      "Out of memory");

    if (result_ptr != NULL)
    {
        /* Cache the input program */
        ASSERT_DEBUG_SYNC(program != NULL,
                          "Input program is NULL");

        result_ptr->program = program;
        result_ptr->type    = OGL_UBER_TYPE_OGL_PROGRAM_DRIVEN;

        ral_context_retain_object(context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  program);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                       _ogl_uber_release,
                                                       OBJECT_TYPE_OGL_UBER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Ubers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (result_ptr != NULL) */

    return (ogl_uber) result_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_get_shader_general_property(const ogl_uber             uber,
                                                             _ogl_uber_general_property property,
                                                             void*                      out_result)
{
    const _ogl_uber* uber_ptr = (const _ogl_uber*) uber;

    switch (property)
    {
        case OGL_UBER_GENERAL_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = uber_ptr->name;

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_N_ITEMS:
        {
            ASSERT_DEBUG_SYNC(uber_ptr->type == OGL_UBER_TYPE_REGULAR,
                              "OGL_UBER_GENERAL_PROPERTY_N_ITEMS query is only supported for regular ogl_uber instances.");

            system_resizable_vector_get_property(uber_ptr->added_items,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result);

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
PUBLIC EMERALD_API void ogl_uber_get_shader_item_property(const ogl_uber          uber,
                                                          ogl_uber_item_id        item_id,
                                                          _ogl_uber_item_property property,
                                                          void*                   result)
{
    const _ogl_uber*      uber_ptr = (const _ogl_uber*) uber;
          _ogl_uber_item* item_ptr = NULL;

    ASSERT_DEBUG_SYNC(uber_ptr->type == OGL_UBER_TYPE_REGULAR,
                      "ogl_uber_get_shader_item_property() is only supported for regular ogl_uber instances.");

    if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                               item_id,
                                              &item_ptr) )
    {
        switch (property)
        {
            case OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF request");

                *(scene_light_falloff*) result = item_ptr->falloff;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM request");

                *(scene_light_shadow_map_algorithm*) result = item_ptr->shadow_map_algorithm;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS request");

                *(scene_light_shadow_map_bias*) result = item_ptr->shadow_map_bias;

                break;
            }

            case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM:
            {
                ASSERT_DEBUG_SYNC(item_ptr->type == OGL_UBER_ITEM_LIGHT,
                                  "Invalid OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM request");

                *(scene_light_shadow_map_pointlight_algorithm*) result = item_ptr->shadow_map_pointlight_algorithm;

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

/** TODO */
PUBLIC EMERALD_API void ogl_uber_link(ogl_uber uber)
{
    const ral_program_variable*  ambient_material_sampler_uniform_ptr    = NULL;
    const ral_program_variable*  ambient_material_uniform_ptr            = NULL;
    const ral_program_variable*  diffuse_material_sampler_uniform_ptr    = NULL;
    const ral_program_variable*  diffuse_material_uniform_ptr            = NULL;
    const ral_program_variable*  emission_material_sampler_uniform_ptr   = NULL;
    const ral_program_variable*  emission_material_uniform_ptr           = NULL;
    const ral_program_variable*  far_near_plane_diff_uniform_ptr         = NULL;
    const ral_program_variable*  flip_z_uniform_ptr                      = NULL;
    const ral_program_variable*  glosiness_uniform_ptr                   = NULL;
    const ral_program_variable*  luminosity_material_sampler_uniform_ptr = NULL;
    const ral_program_variable*  luminosity_material_uniform_ptr         = NULL;
    const ral_program_variable*  max_variance_uniform_ptr                = NULL;
    const ral_program_variable*  mesh_sh3_uniform_ptr                    = NULL;
    const ral_program_variable*  mesh_sh3_data_offset_uniform_ptr        = NULL;
    const ral_program_variable*  mesh_sh4_uniform_ptr                    = NULL;
    const ral_program_variable*  mesh_sh4_data_offset_uniform_ptr        = NULL;
    const ral_program_variable*  model_uniform_ptr                       = NULL;
    const ral_program_variable*  near_plane_uniform_ptr                  = NULL;
    unsigned int                 n_items                                 = 0;
    const ral_program_variable*  normal_matrix_uniform_ptr               = NULL;
    const ral_program_attribute* object_normal_ptr                       = NULL;
    const ral_program_attribute* object_uv_ptr                           = NULL;
    const ral_program_attribute* object_vertex_ptr                       = NULL;
    raGL_program                 program_raGL                            = NULL;
    const ral_program_variable*  shininess_material_sampler_uniform_ptr  = NULL;
    const ral_program_variable*  shininess_material_uniform_ptr          = NULL;
    const ral_program_variable*  specular_material_sampler_uniform_ptr   = NULL;
    const ral_program_variable*  specular_material_uniform_ptr           = NULL;
    _ogl_uber*                   uber_ptr                                = (_ogl_uber*) uber;
    const ral_program_variable*  vp_uniform_ptr                          = NULL;
    const ral_program_variable*  world_camera_uniform_ptr                = NULL;

    /* Bail out if no need to link */
    if (!uber_ptr->dirty)
    {
        goto end;
    }

    /* Recompile shaders if needed */
    if (uber_ptr->type == OGL_UBER_TYPE_REGULAR)
    {
        if (shaders_fragment_uber_is_dirty(uber_ptr->shader_fragment) )
        {
            shaders_fragment_uber_recompile(uber_ptr->shader_fragment);
        }

        if (shaders_vertex_uber_is_dirty(uber_ptr->shader_vertex) )
        {
            shaders_vertex_uber_recompile(uber_ptr->shader_vertex);
        }
    } /* if (uber_ptr->type == OGL_UBER_TYPE_REGULAR) */

    /* Set default attribute & uniform locations */
    _ogl_uber_reset_attribute_uniform_locations(uber_ptr);

    /* Retrieve attribute locations */
    program_raGL = ral_context_get_program_gl(uber_ptr->context,
                                              uber_ptr->program);

    raGL_program_get_vertex_attribute_by_name(program_raGL,
                                              system_hashed_ansi_string_create("object_normal"),
                                             &object_normal_ptr);
    raGL_program_get_vertex_attribute_by_name(program_raGL,
                                              system_hashed_ansi_string_create("object_uv"),
                                             &object_uv_ptr);
    raGL_program_get_vertex_attribute_by_name(program_raGL,
                                              system_hashed_ansi_string_create("object_vertex"),
                                             &object_vertex_ptr);

    if (object_normal_ptr != NULL)
    {
        uber_ptr->object_normal_attribute_location = object_normal_ptr->location;
    }

    if (object_uv_ptr != NULL)
    {
        uber_ptr->object_uv_attribute_location = object_uv_ptr->location;
    }

    if (object_vertex_ptr != NULL)
    {
        uber_ptr->object_vertex_attribute_location = object_vertex_ptr->location;
    }

    /* Retrieve uniform locations */
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("ambient_material_sampler"),
                                    &ambient_material_sampler_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("ambient_material"),
                                    &ambient_material_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("diffuse_material_sampler"),
                                    &diffuse_material_sampler_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("diffuse_material"),
                                    &diffuse_material_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("emission_material_sampler"),
                                    &emission_material_sampler_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("emission_material"),
                                    &emission_material_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("far_near_plane_diff"),
                                    &far_near_plane_diff_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("flip_z"),
                                    &flip_z_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("glosiness"),
                                    &glosiness_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("luminosity_material_sampler"),
                                    &luminosity_material_sampler_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("luminosity_material"),
                                    &luminosity_material_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("max_variance"),
                                    &max_variance_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("mesh_sh3"),
                                    &mesh_sh3_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("mesh_sh3_data_offset"),
                                    &mesh_sh3_data_offset_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("mesh_sh4"),
                                    &mesh_sh4_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("mesh_sh4_data_offset"),
                                    &mesh_sh4_data_offset_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("model"),
                                    &model_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("near_plane"),
                                    &near_plane_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("normal_matrix"),
                                    &normal_matrix_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("shininess_material_sampler"),
                                    &shininess_material_sampler_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("shininess_material"),
                                    &shininess_material_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("specular_material_sampler"),
                                    &specular_material_sampler_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("specular_material"),
                                    &specular_material_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("vp"),
                                    &vp_uniform_ptr);
    raGL_program_get_uniform_by_name(program_raGL,
                                     system_hashed_ansi_string_create("world_camera"),
                                    &world_camera_uniform_ptr);

    if (ambient_material_sampler_uniform_ptr != NULL)
    {
        uber_ptr->ambient_material_sampler_uniform_location = ambient_material_sampler_uniform_ptr->location;
    }

    if (ambient_material_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(ambient_material_uniform_ptr->block_offset != -1,
                          "Ambient material UB offset is -1");

        uber_ptr->ambient_material_ub_offset = ambient_material_uniform_ptr->block_offset;
    }

    if (diffuse_material_sampler_uniform_ptr != NULL)
    {
        uber_ptr->diffuse_material_sampler_uniform_location = diffuse_material_sampler_uniform_ptr->location;
    }

    if (diffuse_material_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(diffuse_material_uniform_ptr->block_offset != -1,
                          "Diffuse material UB offset is -1");

        uber_ptr->diffuse_material_ub_offset = diffuse_material_uniform_ptr->block_offset;
    }

    if (far_near_plane_diff_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(far_near_plane_diff_uniform_ptr->block_offset != -1,
                          "Far/near plane diff UB offset is -1");

        uber_ptr->far_near_plane_diff_ub_offset = far_near_plane_diff_uniform_ptr->block_offset;
    }

    if (flip_z_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(flip_z_uniform_ptr->block_offset != -1,
                          "Flip Z UB offset is -1");

        uber_ptr->flip_z_ub_offset = flip_z_uniform_ptr->block_offset;
    }

    if (luminosity_material_sampler_uniform_ptr != NULL)
    {
        uber_ptr->luminosity_material_sampler_uniform_location = luminosity_material_sampler_uniform_ptr->location;
    }

    if (luminosity_material_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(luminosity_material_uniform_ptr->block_offset != -1,
                          "Luminosity material UB offset is -1.");

        uber_ptr->luminosity_material_ub_offset = luminosity_material_uniform_ptr->block_offset;
    }

    if (max_variance_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(max_variance_uniform_ptr->block_offset != -1,
                          "Max variance UB offset is -1");

        uber_ptr->max_variance_ub_offset = max_variance_uniform_ptr->block_offset;
    }

    if (mesh_sh3_uniform_ptr != NULL)
    {
        uber_ptr->mesh_sh3_uniform_location = mesh_sh3_uniform_ptr->location;
    }
    if (mesh_sh3_data_offset_uniform_ptr != NULL)
    {
        uber_ptr->mesh_sh3_data_offset_uniform_location = mesh_sh3_data_offset_uniform_ptr->location;
    }

    if (mesh_sh4_uniform_ptr != NULL)
    {
        uber_ptr->mesh_sh4_uniform_location = mesh_sh4_uniform_ptr->location;
    }
    if (mesh_sh4_data_offset_uniform_ptr != NULL)
    {
        uber_ptr->mesh_sh4_data_offset_uniform_location = mesh_sh4_data_offset_uniform_ptr->location;
    }

    if (model_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(model_uniform_ptr->block_offset != -1,
                          "Model matrix UB offset is -1");

        uber_ptr->model_ub_offset = model_uniform_ptr->block_offset;
    }

    if (near_plane_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(near_plane_uniform_ptr->block_offset != -1,
                          "Near plane UB offset is -1");

        uber_ptr->near_plane_ub_offset = near_plane_uniform_ptr->block_offset;
    }

    if (normal_matrix_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(normal_matrix_uniform_ptr->block_offset != -1,
                          "Normal matrix UB offset is -1");

        uber_ptr->normal_matrix_ub_offset = normal_matrix_uniform_ptr->block_offset;
    }

    if (shininess_material_sampler_uniform_ptr != NULL)
    {
        uber_ptr->shininess_material_sampler_uniform_location = shininess_material_sampler_uniform_ptr->location;
    }

    if (shininess_material_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(shininess_material_uniform_ptr->block_offset != -1,
                          "Shininess material UB offset is -1");

        uber_ptr->shininess_material_ub_offset = shininess_material_uniform_ptr->block_offset;
    }

    if (specular_material_sampler_uniform_ptr != NULL)
    {
        uber_ptr->specular_material_sampler_uniform_location = specular_material_sampler_uniform_ptr->location;
    }

    if (specular_material_uniform_ptr != NULL)
    {
        uber_ptr->specular_material_ub_offset = specular_material_uniform_ptr->block_offset;
    }

    if (vp_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(vp_uniform_ptr->block_offset != -1,
                          "VP UB offset is -1");

        uber_ptr->vp_ub_offset = vp_uniform_ptr->block_offset;
    }

    if (world_camera_uniform_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(world_camera_uniform_ptr->block_index != -1,
                          "World camera UB offset is -1");

        uber_ptr->world_camera_ub_offset = world_camera_uniform_ptr->block_offset;
    }

    /* Retrieve uniform block IDs and their properties*/
    uber_ptr->ub_fs = NULL;
    uber_ptr->ub_vs = NULL;

    raGL_program_get_uniform_block_by_name(program_raGL,
                                           system_hashed_ansi_string_create("FragmentShaderProperties"),
                                          &uber_ptr->ub_fs);
    raGL_program_get_uniform_block_by_name(program_raGL,
                                           system_hashed_ansi_string_create("VertexShaderProperties"),
                                          &uber_ptr->ub_vs);

    if (uber_ptr->ub_fs != NULL)
    {
        ogl_program_ub_get_property(uber_ptr->ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &uber_ptr->ub_fs_bo_size);
        ogl_program_ub_get_property(uber_ptr->ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &uber_ptr->ub_fs_bo);
    }
    else
    {
        uber_ptr->ub_fs_bo      = NULL;
        uber_ptr->ub_fs_bo_size = 0;
    }

    if (uber_ptr->ub_vs != NULL)
    {
        ogl_program_ub_get_property(uber_ptr->ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &uber_ptr->ub_vs_bo_size);
        ogl_program_ub_get_property(uber_ptr->ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &uber_ptr->ub_vs_bo);
    }
    else
    {
        uber_ptr->ub_vs_bo      = NULL;
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
                const ral_program_variable* light_ambient_color_uniform_ptr               = NULL;
                const ral_program_variable* light_attenuations_uniform_ptr                = NULL;
                const ral_program_variable* light_cone_angle_uniform_ptr                  = NULL;
                const ral_program_variable* light_diffuse_uniform_ptr                     = NULL;
                const ral_program_variable* light_direction_uniform_ptr                   = NULL;
                const ral_program_variable* light_edge_angle_uniform_ptr                  = NULL;
                const ral_program_variable* light_far_near_diff_uniform_ptr               = NULL;
                const ral_program_variable* light_location_uniform_ptr                    = NULL;
                const ral_program_variable* light_near_plane_uniform_ptr                  = NULL;
                const ral_program_variable* light_projection_uniform_ptr                  = NULL;
                const ral_program_variable* light_range_uniform_ptr                       = NULL;
                const ral_program_variable* light_shadow_map_color_uniform_ptr            = NULL;
                const ral_program_variable* light_shadow_map_depth_uniform_ptr            = NULL;
                const ral_program_variable* light_shadow_map_vsm_cutoff_uniform_ptr       = NULL;
                const ral_program_variable* light_shadow_map_vsm_min_variance_uniform_ptr = NULL;
                const ral_program_variable* light_view_uniform_ptr                        = NULL;

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

                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create("ambient_color"),
                                                &light_ambient_color_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_attenuations_uniform_name_sstream.str().c_str() ),
                                                &light_attenuations_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_cone_angle_uniform_name_sstream.str().c_str() ),
                                                &light_cone_angle_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_diffuse_uniform_name_sstream.str().c_str() ),
                                                &light_diffuse_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_direction_uniform_name_sstream.str().c_str() ),
                                                &light_direction_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_edge_angle_uniform_name_sstream.str().c_str() ),
                                                &light_edge_angle_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_far_near_diff_uniform_name_sstream.str().c_str() ),
                                                &light_far_near_diff_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_location_uniform_name_sstream.str().c_str()  ),
                                                &light_location_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_near_plane_uniform_name_sstream.str().c_str() ),
                                                &light_near_plane_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_projection_uniform_name_sstream.str().c_str() ),
                                                &light_projection_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_range_uniform_name_sstream.str().c_str()  ),
                                                &light_range_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_shadow_map_color_uniform_name_sstream.str().c_str() ),
                                                &light_shadow_map_color_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_shadow_map_depth_uniform_name_sstream.str().c_str() ),
                                                &light_shadow_map_depth_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_view_uniform_name_sstream.str().c_str() ),
                                                &light_view_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_shadow_map_vsm_cutoff_uniform_name_sstream.str().c_str() ),
                                                &light_shadow_map_vsm_cutoff_uniform_ptr);
                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_shadow_map_vsm_min_variance_uniform_name_sstream.str().c_str() ),
                                                &light_shadow_map_vsm_min_variance_uniform_ptr);

                if (light_ambient_color_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.ambient_color_ub_offset = light_ambient_color_uniform_ptr->block_offset;
                }

                if (light_attenuations_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_attenuations_ub_offset = light_attenuations_uniform_ptr->block_offset;
                }

                if (light_cone_angle_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset = light_cone_angle_uniform_ptr->block_offset;
                }

                if (light_direction_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_direction_ub_offset = light_direction_uniform_ptr->block_offset;
                }

                if (light_diffuse_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_diffuse_ub_offset  = light_diffuse_uniform_ptr->block_offset;
                }

                if (light_edge_angle_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset = light_edge_angle_uniform_ptr->block_offset;
                }

                if (light_far_near_diff_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset = light_far_near_diff_uniform_ptr->block_offset;
                }

                if (light_location_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_location_ub_offset = light_location_uniform_ptr->block_offset;
                }

                if (light_near_plane_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_near_plane_ub_offset = light_near_plane_uniform_ptr->block_offset;
                }

                if (light_projection_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_projection_ub_offset = light_projection_uniform_ptr->block_offset;
                }

                if (light_range_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_range_ub_offset = light_range_uniform_ptr->block_offset;
                }

                if (light_shadow_map_color_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_texture_color_sampler_location = light_shadow_map_color_uniform_ptr->location;
                }

                if (light_shadow_map_depth_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth_sampler_location = light_shadow_map_depth_uniform_ptr->location;
                }

                if (light_view_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_view_ub_offset = light_view_uniform_ptr->block_offset;
                }

                if (light_shadow_map_vsm_cutoff_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset = light_shadow_map_vsm_cutoff_uniform_ptr->block_offset;
                }

                if (light_shadow_map_vsm_min_variance_uniform_ptr != NULL)
                {
                    item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset = light_shadow_map_vsm_min_variance_uniform_ptr->block_offset;
                }

                /* Vertex shader stuff */
                std::stringstream           light_depth_vb_uniform_name_sstream;
                const ral_program_variable* light_depth_vb_uniform_ptr = NULL;
                shaders_vertex_uber_light   light_type                 = SHADERS_VERTEX_UBER_LIGHT_NONE;

                light_depth_vb_uniform_name_sstream << "light"
                                                    << n_item
                                                    << "_depth_vp";

                raGL_program_get_uniform_by_name(program_raGL,
                                                 system_hashed_ansi_string_create(light_depth_vb_uniform_name_sstream.str().c_str() ),
                                                &light_depth_vb_uniform_ptr);

                if (light_depth_vb_uniform_ptr != NULL)
                {
                    item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset = light_depth_vb_uniform_ptr->block_offset;
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
                    const ral_program_variable* sh_data_uniform_ptr      = NULL;

                    if (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS)
                    {
                        sh_data_uniform_name_sstream << "light" << n_item << "_sh3[0]";
                    }
                    else
                    {
                        sh_data_uniform_name_sstream << "light" << n_item << "_sh4[0]";
                    }

                    raGL_program_get_uniform_by_name(program_raGL,
                                                     system_hashed_ansi_string_create(sh_data_uniform_name_sstream.str().c_str()),
                                                    &sh_data_uniform_ptr);

                    ASSERT_DEBUG_SYNC(sh_data_uniform_ptr != NULL,
                                      "Could not retrieve SH data uniform descriptor");
                    ASSERT_DEBUG_SYNC(sh_data_uniform_ptr->block_offset != -1,
                                      "UB offset for SH data is -1");

                    item_ptr->vertex_shader_item.current_light_sh_data_ub_offset = sh_data_uniform_ptr->block_offset;
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unrecognized uber item type");
            }
        } /* switch (item_type) */
    } /* for (all uber items) */

    /* Request renderer thread call-back to do the other part of the initialization */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(uber_ptr->context),
                                                     _ogl_uber_link_renderer_callback,
                                                     uber_ptr);

    /* All done - object no longer dirty */
    uber_ptr->dirty = false;

end:
    ;
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
PUBLIC void ogl_uber_rendering_render_mesh(mesh             mesh_gpu,
                                           system_matrix4x4 model,
                                           system_matrix4x4 normal_matrix,
                                           ogl_uber         uber,
                                           mesh_material    material,
                                           system_time      time)
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
            system_time mesh_modification_timestamp = 0;

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
        const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points_ptr = NULL;
        const ogl_context_gl_entrypoints*                         entry_points_ptr     = NULL;

        ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &dsa_entry_points_ptr);
        ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points_ptr);

        entry_points_ptr->pGLBindVertexArray(vao_ptr->vao_id);

        /* Ensure vertex ordering is correct */
        mesh_vertex_ordering vertex_ordering;

        mesh_get_property(mesh_gpu,
                          MESH_PROPERTY_VERTEX_ORDERING,
                         &vertex_ordering);

        entry_points_ptr->pGLFrontFace( (vertex_ordering == MESH_VERTEX_ORDERING_CCW) ? GL_CCW
                                                                                      : GL_CW);

        /* Update model matrix */
        ASSERT_DEBUG_SYNC(uber_ptr->model_ub_offset != -1,
                          "No model matrix uniform found");

        ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                    uber_ptr->model_ub_offset,
                                                    system_matrix4x4_get_row_major_data(model),
                                                    0, /* src_data_flags */
                                                    sizeof(float) * 16);

        /* Update normal matrix */
        if (uber_ptr->normal_matrix_ub_offset != -1)
        {
            ASSERT_DEBUG_SYNC(normal_matrix != NULL,
                              "Normal matrix is NULL but is required.");

            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                        uber_ptr->normal_matrix_ub_offset,
                                                        system_matrix4x4_get_row_major_data(normal_matrix),
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 16);
        }

        /* Make sure the uniform buffer bindings are fine */
        if (uber_ptr->ub_fs != NULL)
        {
            GLuint      ub_fs_bo_id           = 0;
            raGL_buffer ub_fs_bo_raGL         = NULL;
            uint32_t    ub_fs_bo_start_offset = -1;

            ub_fs_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                      uber_ptr->ub_fs_bo);

            raGL_buffer_get_property(ub_fs_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &ub_fs_bo_id);
            raGL_buffer_get_property(ub_fs_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &ub_fs_bo_start_offset);

            entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                                 FRAGMENT_SHADER_PROPERTIES_UBO_BINDING_ID,
                                                 ub_fs_bo_id,
                                                 ub_fs_bo_start_offset,
                                                 uber_ptr->ub_fs_bo_size);
        }

        if (uber_ptr->ub_vs != NULL)
        {
            GLuint      ub_vs_bo_id           = 0;
            raGL_buffer ub_vs_bo_raGL         = NULL;
            uint32_t    ub_vs_bo_start_offset = -1;

            ub_vs_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                      uber_ptr->ub_vs_bo);

            raGL_buffer_get_property(ub_vs_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &ub_vs_bo_id);
            raGL_buffer_get_property(ub_vs_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &ub_vs_bo_start_offset);

            entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                                 VERTEX_SHADER_PROPERTIES_UBO_BINDING_ID,
                                                 ub_vs_bo_id,
                                                 ub_vs_bo_start_offset,
                                                 uber_ptr->ub_vs_bo_size);
        }

        /* Iterate over all layers.. */
        mesh_type          instance_type;
        uint32_t           n_layers   = 0;
        const raGL_program po_raGL    = ral_context_get_program_gl(uber_ptr->context,
                                                             uber_ptr->program);
        GLuint             po_raGL_id = 0;

        raGL_program_get_property(po_raGL,
                                  RAGL_PROGRAM_PROPERTY_ID,
                                 &po_raGL_id);

        mesh_get_property(mesh_instantiation_parent_gpu,
                          MESH_PROPERTY_TYPE,
                         &instance_type);
        mesh_get_property(mesh_instantiation_parent_gpu,
                          MESH_PROPERTY_N_LAYERS,
                         &n_layers);

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
                mesh_material layer_pass_material  = NULL;
                uint32_t      n_texture_units_used = uber_ptr->n_texture_units_assigned;

                /* Retrieve layer pass properties */
                mesh_get_layer_pass_property(mesh_instantiation_parent_gpu,
                                             n_layer,
                                             n_layer_pass,
                                             MESH_LAYER_PROPERTY_MATERIAL,
                                            &layer_pass_material);

                if (layer_pass_material != material &&
                    material            != NULL)
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
                        int32_t                        shader_scalar_ub_offset;
                        int32_t                        shader_uv_attribute_location;
                        bool                           convert_to_linear;
                    } attachments[] =
                    {
                        {
                            MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                            uber_ptr->ambient_material_sampler_uniform_location,
                            uber_ptr->ambient_material_ub_offset,
                            uber_ptr->object_uv_attribute_location,
                            true,
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                            uber_ptr->diffuse_material_sampler_uniform_location,
                            uber_ptr->diffuse_material_ub_offset,
                            uber_ptr->object_uv_attribute_location,
                            true,
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
                            uber_ptr->luminosity_material_sampler_uniform_location,
                            uber_ptr->luminosity_material_ub_offset,
                            uber_ptr->object_uv_attribute_location,
                            false
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                            uber_ptr->shininess_material_sampler_uniform_location,
                            uber_ptr->shininess_material_ub_offset,
                            uber_ptr->object_uv_attribute_location,
                            false
                        },

                        {
                            MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                            uber_ptr->specular_material_sampler_uniform_location,
                            uber_ptr->specular_material_ub_offset,
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
                                } /* if (attachment.convert_to_linear) */

                                if (attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT                  ||
                                    attachment_type == MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT)
                                {
                                    ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                                attachment.shader_scalar_ub_offset,
                                                                                data_vec3,
                                                                                0, /* src_data_flags */
                                                                                sizeof(float) );
                                }
                                else
                                {
                                    ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                                attachment.shader_scalar_ub_offset,
                                                                                data_vec3,
                                                                                0, /* src_data_flags */
                                                                                sizeof(float) * 3);
                                }

                                break;
                            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT: */

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE:
                            {
                                ral_sampler  layer_pass_sampler              = NULL;
                                GLuint       layer_pass_sampler_id           = 0;
                                raGL_sampler layer_pass_sampler_raGL         = NULL;
                                ral_texture  layer_pass_texture              = NULL;
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

                                layer_pass_sampler_raGL = ral_context_get_sampler_gl(uber_ptr->context,
                                                                                     layer_pass_sampler);

                                raGL_sampler_get_property(layer_pass_sampler_raGL,
                                                          RAGL_SAMPLER_PROPERTY_ID,
                                                         &layer_pass_sampler_id);

                                entry_points_ptr->pGLBindSampler(n_texture_units_used,
                                                                 layer_pass_sampler_id);

                                dsa_entry_points_ptr->pGLBindMultiTextureEXT (GL_TEXTURE0 + n_texture_units_used,
                                                                              GL_TEXTURE_2D,
                                                                              ral_context_get_texture_gl_id(uber_ptr->context,
                                                                                                            layer_pass_texture) );
                                entry_points_ptr->pGLProgramUniform1i        (po_raGL_id,
                                                                              attachment.shader_sampler_uniform_location,
                                                                              n_texture_units_used);

                                n_texture_units_used++;

                                break;
                            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE: */

                            case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4:
                            {
                                float data_vec4[4];

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
                                } /* if (attachment.convert_to_linear) */

                                ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                            attachment.shader_scalar_ub_offset,
                                                                            data_vec4,
                                                                            0, /* src_data_flags */
                                                                            sizeof(float) * 4);

                                break;
                            } /* case MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4: */

                            default:
                            {
                                ASSERT_DEBUG_SYNC(false, "Unrecognized material property attachment");
                            }
                        } /* switch (attachment_type) */
                    } /* for (all attachments) */
                } /* if (material != NULL) */

                if (uber_ptr->ub_fs != NULL)
                {
                    ogl_program_ub_sync(uber_ptr->ub_fs);
                }

                if (uber_ptr->ub_vs != NULL)
                {
                    ogl_program_ub_sync(uber_ptr->ub_vs);
                }

                /* Issue the draw call. We need to handle two separate cases here:
                 *
                 * 1) We're dealing with a regular mesh:    need to do an indexed draw call.
                 * 2) We're dealing with a GPU stream mesh: trickier! need to check what kind of draw call
                 *                                          we need to make and act accordingly.
                 */
                if (instance_type == MESH_TYPE_REGULAR)
                {
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

                    /* Proceed with the actual draw call */
                    uint32_t layer_pass_elements_offset = 0;
                    uint32_t layer_pass_index_max_value = 0;
                    uint32_t layer_pass_index_min_value = 0;
                    uint32_t layer_pass_n_elements      = 0;

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

                    entry_points_ptr->pGLDrawRangeElements(GL_TRIANGLES,
                                                           layer_pass_index_min_value,
                                                           layer_pass_index_max_value,
                                                           layer_pass_n_elements,
                                                           gl_index_type,
                                                          (const GLvoid*) (intptr_t) layer_pass_elements_offset);
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

                    if (draw_call_arguments.pre_draw_call_barriers != 0)
                    {
                        entry_points_ptr->pGLMemoryBarrier(draw_call_arguments.pre_draw_call_barriers);
                    }

                    switch (draw_call_type)
                    {
                        case MESH_DRAW_CALL_TYPE_ARRAYS:
                        {
                            entry_points_ptr->pGLDrawArrays(draw_call_arguments.mode,
                                                            draw_call_arguments.first,
                                                            draw_call_arguments.count);

                            break;
                        }

                        case MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT:
                        {
                            entry_points_ptr->pGLBindBuffer(GL_DRAW_INDIRECT_BUFFER,
                                                            draw_call_arguments.draw_indirect_bo_binding);

                            entry_points_ptr->pGLDrawArraysIndirect(draw_call_arguments.mode,
                                                                    draw_call_arguments.indirect);

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized draw call type requested for a GPU stream mesh.");
                        }
                    } /* switch (draw_call_type) */
                }
            } /* for (all mesh layer passes) */
        } /* for (all mesh layers) */
    } /* if (mesh_gpu != NULL) */
}

/* Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_uber_rendering_start(ogl_uber uber)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entry_points_ptr     = NULL;
    raGL_program                                              program_raGL         = NULL;
    GLuint                                                    program_raGL_id      = 0;
    _ogl_uber*                                                uber_ptr             = (_ogl_uber*) uber;

    program_raGL = ral_context_get_program_gl(uber_ptr->context,
                                              uber_ptr->program);

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points_ptr);
    ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points_ptr);

    ASSERT_DEBUG_SYNC(!uber_ptr->is_rendering,
                      "Already started");

    /* Reset texture unit use counter */
    uber_ptr->n_texture_units_assigned = 0;

    /* Update shaders if the configuration has been changed since the last call */
    if (uber_ptr->dirty)
    {
        ogl_uber_link(uber);

        ASSERT_DEBUG_SYNC(!uber_ptr->dirty,
                          "Linking an ogl_uber instance did not reset the dirty flag");
    }

    /* Set up UB contents & texture samplers */
    unsigned int n_items = 0;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_items);

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
                    if (item_ptr->fragment_shader_item.current_light_shadow_map_texture_color_sampler_location != -1)
                    {
                        /* Bind the shadow map */
                        GLuint           n_texture_unit            = uber_ptr->n_texture_units_assigned++;
                        GLenum           shadow_map_texture_target = GL_NONE;
                        ral_texture_type shadow_map_texture_type   = RAL_TEXTURE_TYPE_UNKNOWN;

                        ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_shadow_map_texture_color != NULL,
                                          "No color shadow map assigned to a light which casts shadows");

                        ral_texture_get_property(item_ptr->fragment_shader_item.current_light_shadow_map_texture_color,
                                                 RAL_TEXTURE_PROPERTY_TYPE,
                                                &shadow_map_texture_type);

                        shadow_map_texture_target = raGL_utils_get_ogl_texture_target_for_ral_texture_type(shadow_map_texture_type);

                        entry_points_ptr->pGLBindSampler            (n_texture_unit,
                                                                     0);            /* TODO: use a sampler instead of SM texture state! */
                        dsa_entry_points_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0 + n_texture_unit,
                                                                     shadow_map_texture_target,
                                                                     ral_context_get_texture_gl_id(uber_ptr->context,
                                                                                                   item_ptr->fragment_shader_item.current_light_shadow_map_texture_color) );
                        entry_points_ptr->pGLProgramUniform1i       (program_raGL_id,
                                                                     item_ptr->fragment_shader_item.current_light_shadow_map_texture_color_sampler_location,
                                                                     n_texture_unit);
                    }

                    if (item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth_sampler_location != -1)
                    {
                        /* Bind the shadow map */
                        GLuint           n_texture_unit            = uber_ptr->n_texture_units_assigned++;
                        GLenum           shadow_map_texture_target = GL_NONE;
                        ral_texture_type shadow_map_texture_type   = RAL_TEXTURE_TYPE_UNKNOWN;

                        ASSERT_DEBUG_SYNC(item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth != NULL,
                                          "No depth shadow map assigned to a light which casts shadows");

                        ral_texture_get_property(item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth,
                                                 RAL_TEXTURE_PROPERTY_TYPE,
                                                &shadow_map_texture_type);

                        shadow_map_texture_target = raGL_utils_get_ogl_texture_target_for_ral_texture_type(shadow_map_texture_type);

                        entry_points_ptr->pGLBindSampler            (n_texture_unit,
                                                                     0);            /* TODO: use a sampler instead of SM texture state! */
                        dsa_entry_points_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0 + n_texture_unit,
                                                                     shadow_map_texture_target,
                                                                     ral_context_get_texture_gl_id(uber_ptr->context,
                                                                                                   item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth) );
                        entry_points_ptr->pGLProgramUniform1i       (program_raGL_id,
                                                                     item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth_sampler_location,
                                                                     n_texture_unit);
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
                            /* TODO: This code has become deprecated. */
                            ASSERT_DEBUG_SYNC(false,
                                              "TODO");
#if 0
                            const unsigned int sh_data_size = (light_type == SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS) ? 4 * sizeof(float) * 9
                                                                                                                   : 4 * sizeof(float) * 12;

                            dsa_entry_points->pGLNamedCopyBufferSubDataEXT(item_ptr->vertex_shader_item.current_light_sh_data.bo_id,
                                                                           uber_ptr->ubo_id,
                                                                           item_ptr->vertex_shader_item.current_light_sh_data.bo_offset,
                                                                           uber_ptr->ubo_start_offset + uber_ptr->ubo_data_vertex_offset + item_ptr->vertex_shader_item.current_light_sh_data_ub_offset,
                                                                           sh_data_size);
#endif

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized light type at index [%d]",
                                              n_item);
                        }
                    } /* switch (light_type) */

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized vertex shader item type");
                }
            } /* switch (item_ptr->type) */
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve uber item descriptor at index [%d]",
                              n_item);
        }
    } /* for (all vertex shader items) */

    /* Configure other stuff */
    if (uber_ptr->max_variance_ub_offset != -1)
    {
        ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                    uber_ptr->max_variance_ub_offset,
                                                   &uber_ptr->current_vsm_max_variance,
                                                    0, /* src_data_flags */
                                                    sizeof(float) );
    }

    /* Sync the UBOs */
    if (uber_ptr->ub_fs != NULL)
    {
        ogl_program_ub_sync(uber_ptr->ub_fs);
    }

    if (uber_ptr->ub_vs != NULL)
    {
        ogl_program_ub_sync(uber_ptr->ub_vs);
    }

    /* Configure uniform buffer bindings */
    ral_buffer  fs_ub_bo              =  NULL;
    GLuint      fs_ub_bo_id           =  0;
    raGL_buffer fs_ub_bo_raGL         = NULL;
    GLuint      fs_ub_bo_start_offset = -1;
    uint32_t    fs_ub_size            =  0;
    ral_buffer  vs_ub_bo              =  NULL;
    GLuint      vs_ub_bo_id           =  0;
    raGL_buffer vs_ub_bo_raGL         = NULL;
    GLuint      vs_ub_bo_start_offset = -1;
    uint32_t    vs_ub_size            =  0;

    if (uber_ptr->ub_fs != NULL)
    {
        ogl_program_ub_get_property(uber_ptr->ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &fs_ub_size);
        ogl_program_ub_get_property(uber_ptr->ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &fs_ub_bo);

        if (fs_ub_size != 0)
        {
            fs_ub_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                      fs_ub_bo);

            raGL_buffer_get_property(fs_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &fs_ub_bo_id);
            raGL_buffer_get_property(fs_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &fs_ub_bo_start_offset);

            entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                                 FRAGMENT_SHADER_PROPERTIES_UBO_BINDING_ID,
                                                 fs_ub_bo_id,
                                                 fs_ub_bo_start_offset,
                                                 fs_ub_size);
        }
    } /* if (uber_ptr->ub_fs != NULL) */

    if (uber_ptr->ub_vs != NULL)
    {
        ogl_program_ub_get_property(uber_ptr->ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &vs_ub_size);
        ogl_program_ub_get_property(uber_ptr->ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                   &vs_ub_bo);

        if (vs_ub_size != 0)
        {
            vs_ub_bo_raGL = ral_context_get_buffer_gl(uber_ptr->context,
                                                      vs_ub_bo);

            raGL_buffer_get_property(vs_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &vs_ub_bo_id);
            raGL_buffer_get_property(vs_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &vs_ub_bo_start_offset);

            entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                                 VERTEX_SHADER_PROPERTIES_UBO_BINDING_ID,
                                                 vs_ub_bo_id,
                                                 vs_ub_bo_start_offset,
                                                 vs_ub_size);
        }
    } /* if (uber_ptr->ub_vs != NULL) */

    /* Activate the uber program */
    entry_points_ptr->pGLUseProgram(program_raGL_id);

    /* Mark as 'being rendered' */
    uber_ptr->is_rendering = true;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_set_shader_general_property(ogl_uber                   uber,
                                                             _ogl_uber_general_property property,
                                                             const void*                data)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    /* All properties below refer to the uniform block defined in uber vertex shader. */
    if (uber_ptr->ub_vs == NULL)
    {
        goto end;
    }

    switch (property)
    {
        case OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION:
        {
            if (uber_ptr->world_camera_ub_offset != -1)
            {
                const float location[4] =
                {
                    ((float*) data)[0],
                    ((float*) data)[1],
                    ((float*) data)[2],
                    1.0f
                };

                ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                            uber_ptr->world_camera_ub_offset,
                                                            location,
                                                            0, /* src_data_flags */
                                                            sizeof(float) * 4);
            }

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_FAR_NEAR_PLANE_DIFF:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                        uber_ptr->far_near_plane_diff_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) );

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_FLIP_Z:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                        uber_ptr->flip_z_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) );

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_VSM_MAX_VARIANCE:
        {
            /* max variance is an uniform so it doesn't make much sense to track it, as the program
             * is context-wide and the value might've been changed by another ogl_uber instance. */
            uber_ptr->current_vsm_max_variance = *(float*) data;

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_NEAR_PLANE:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                        uber_ptr->near_plane_ub_offset,
                                                        data,
                                                        0, /* src_data_flags */
                                                        sizeof(float) );

            break;
        }

        case OGL_UBER_GENERAL_PROPERTY_VP:
        {
            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                        uber_ptr->vp_ub_offset,
                                                        system_matrix4x4_get_row_major_data( (system_matrix4x4) data),
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 16);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized general uber property [%d]",
                              property);
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_uber_set_shader_item_property(ogl_uber                uber,
                                                          unsigned int            item_index,
                                                          _ogl_uber_item_property property,
                                                          const void*             data)
{
    _ogl_uber* uber_ptr = (_ogl_uber*) uber;

    ASSERT_DEBUG_SYNC(uber_ptr->type == OGL_UBER_TYPE_REGULAR,
                      "ogl_uber_set_shader_item_property() is only supported for regular ogl_uber instances.");

    switch (property)
    {
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE:
        case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX:
        case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_RAL_COLOR:
        case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_RAL_DEPTH:
        case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF:
        case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE:
        case OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP:
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
                        if (item_ptr->fragment_shader_item.ambient_color_ub_offset != -1)
                        {
                            /* We're rendering in linear space, so make sure to convert
                             * sRGB light color to linear space */
                            float  linear_data[3];
                            float* srgb_data_ptr = (float*) data;

                            for (unsigned int n_component = 0;
                                              n_component < 3;
                                            ++n_component)
                            {
                                linear_data[n_component] = convert_sRGB_to_linear(srgb_data_ptr[n_component]);
                            }

                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.ambient_color_ub_offset,
                                                                        linear_data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 3);
                        } /* if (item_ptr->fragment_shader_item.ambient_color_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS:
                    {
                        if (item_ptr->fragment_shader_item.current_light_attenuations_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_attenuations_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 3);
                        } /* if (item_ptr->fragment_shader_item.current_light_attenuations_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_cone_angle_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_diffuse_ub_offset != -1)
                        {
                            /* We're rendering in linear space, so make sure to convert
                             * sRGB light color to linear space */
                            float  linear_data[4];
                            float* srgb_data_ptr = (float*) data;

                            for (unsigned int n_component = 0;
                                              n_component < 3;
                                            ++n_component)
                            {
                                linear_data[n_component] = convert_sRGB_to_linear(srgb_data_ptr[n_component]);
                            }

                            linear_data[3] = 1.0f;

                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_diffuse_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 4);
                        } /* if (item_ptr->fragment_shader_item.current_light_diffuse_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION:
                    {
                        float* temp = (float*) data;

                        if (item_ptr->fragment_shader_item.current_light_direction_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_direction_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 3);
                        } /* if (item_ptr->fragment_shader_item.current_light_direction_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_edge_angle_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF:
                    {
                        if (item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_far_near_diff_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION:
                    {
                        if (item_ptr->fragment_shader_item.current_light_location_ub_offset != -1)
                        {
                            const float location[4] =
                            {
                                ((float*) data)[0],
                                ((float*) data)[1],
                                ((float*) data)[2],
                                1.0f
                            };

                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_location_ub_offset,
                                                                        location,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 4);
                        } /* if (item_ptr->fragment_shader_item.current_light_location_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_near_plane_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_near_plane_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_near_plane_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX:
                    {
                        if (item_ptr->fragment_shader_item.current_light_projection_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_projection_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 16);
                        } /* if (item_ptr->fragment_shader_item.current_light_projection_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_range_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_range_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_range_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX:
                    {
                        if (item_ptr->fragment_shader_item.current_light_view_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_view_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 16);
                        } /* if (item_ptr->fragment_shader_item.current_light_view_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_RAL_COLOR:
                    {
                        item_ptr->fragment_shader_item.current_light_shadow_map_texture_color = *(ral_texture*) data;

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_RAL_DEPTH:
                    {
                        item_ptr->fragment_shader_item.current_light_shadow_map_texture_depth = *(ral_texture*) data;

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF:
                    {
                        if (item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_shadow_map_vsm_cutoff_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE:
                    {
                        if (item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_fs,
                                                                        item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) );
                        } /* if (item_ptr->fragment_shader_item.current_light_shadow_map_vsm_min_variance_ub_offset != -1) */

                        break;
                    }

                    case OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP:
                    {
                        if (item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset != -1)
                        {
                            ogl_program_ub_set_nonarrayed_uniform_value(uber_ptr->ub_vs,
                                                                        item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset,
                                                                        data,
                                                                        0, /* src_data_flags */
                                                                        sizeof(float) * 16);
                        } /* if (item_ptr->vertex_shader_item.current_light_depth_vp_ub_offset != -1) */

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
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_uber_rendering_stop(ogl_uber uber)
{
    _ogl_uber*                        uber_ptr     = (_ogl_uber*) uber;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(ral_context_get_gl_context(uber_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ASSERT_DEBUG_SYNC(uber_ptr->is_rendering,
                      "Not started");

    /* Mark as no longer rendered */
    uber_ptr->is_rendering = false;
}
