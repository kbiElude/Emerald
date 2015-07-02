/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef COLLADA_TYPES_H
#define COLLADA_TYPES_H

DECLARE_HANDLE(collada_data);
DECLARE_HANDLE(collada_data_animation);
DECLARE_HANDLE(collada_data_camera);
DECLARE_HANDLE(collada_data_channel);
DECLARE_HANDLE(collada_data_effect);
DECLARE_HANDLE(collada_data_float_array);
DECLARE_HANDLE(collada_data_geometry);
DECLARE_HANDLE(collada_data_geometry_material_binding);
DECLARE_HANDLE(collada_data_geometry_mesh);
DECLARE_HANDLE(collada_data_image);
DECLARE_HANDLE(collada_data_input);
DECLARE_HANDLE(collada_data_input_set);
DECLARE_HANDLE(collada_data_light);
DECLARE_HANDLE(collada_data_material);
DECLARE_HANDLE(collada_data_material_binding);
DECLARE_HANDLE(collada_data_name_array);
DECLARE_HANDLE(collada_data_polylist);
DECLARE_HANDLE(collada_data_sampler);
DECLARE_HANDLE(collada_data_sampler2D);
DECLARE_HANDLE(collada_data_scene);
DECLARE_HANDLE(collada_data_scene_graph_node);
DECLARE_HANDLE(collada_data_scene_graph_node_camera_instance);
DECLARE_HANDLE(collada_data_scene_graph_node_geometry_instance);
DECLARE_HANDLE(collada_data_scene_graph_node_item);
DECLARE_HANDLE(collada_data_scene_graph_node_light_instance);
DECLARE_HANDLE(collada_data_scene_graph_node_material_instance);
DECLARE_HANDLE(collada_data_source);
DECLARE_HANDLE(collada_data_surface);
DECLARE_HANDLE(collada_data_transformation);
DECLARE_HANDLE(collada_value);

/** Describes up vector. */
typedef enum
{
    COLLADA_DATA_UP_AXIS_X_UP,
    COLLADA_DATA_UP_AXIS_Y_UP,
    COLLADA_DATA_UP_AXIS_Z_UP,

    COLLADA_DATA_UP_AXIS_UNDEFINED
} _collada_data_up_axis;

/** Describes shading algorithms supported by COLLADA for COMMON profile */
typedef enum
{
    /* color = <emission> + <ambient>*al + <diffuse>*max(dot(N,L), 0)+<specular>*max(dot(H, N), 0)^shininess */
    COLLADA_DATA_SHADING_BLINN,
    /* color = <emission> + <ambient>*al + <diffuse>*max(dot(N,L), 0) */
    COLLADA_DATA_SHADING_LAMBERT,
    /* color = <emission> + <ambient>*al + <diffuse>*max(dot(N,L), 0)+<specular>*max(dot(R, I), 0)^shininess */
    COLLADA_DATA_SHADING_PHONG,

    /* Always last */
    COLLADA_DATA_SHADING_UNKNOWN
} _collada_data_shading;

/** Describes type of representation used to describe a single shading factor. */
typedef enum
{
    COLLADA_DATA_SHADING_FACTOR_NONE,
    COLLADA_DATA_SHADING_FACTOR_FLOAT,
    COLLADA_DATA_SHADING_FACTOR_TEXTURE,
    COLLADA_DATA_SHADING_FACTOR_VEC4,

    /* Always last */
    COLLADA_DATA_SHADING_FACTOR_UNKNOWN
} collada_data_shading_factor;

/** Describes shading factor item type */
typedef enum
{
    COLLADA_DATA_SHADING_FACTOR_ITEM_AMBIENT    = 1 << 0,
    COLLADA_DATA_SHADING_FACTOR_ITEM_DIFFUSE    = 1 << 1,
    COLLADA_DATA_SHADING_FACTOR_ITEM_LUMINOSITY = 1 << 2,
    COLLADA_DATA_SHADING_FACTOR_ITEM_SHININESS  = 1 << 3,
    COLLADA_DATA_SHADING_FACTOR_ITEM_SPECULAR   = 1 << 4,

    COLLADA_DATA_SHADING_FACTOR_ITEM_COUNT   = 5, /* always equal to +1 off the highest power defined in this enum */
    COLLADA_DATA_SHADING_FACTOR_ITEM_UNKNOWN = COLLADA_DATA_SHADING_FACTOR_ITEM_COUNT
} collada_data_shading_factor_item;

/** Describes common inputs supported by COLLADA.
 *
 *  We only support a subset of these at the moment!
 */
typedef enum
{
    COLLADA_DATA_INPUT_TYPE_BINORMAL,
    COLLADA_DATA_INPUT_TYPE_COLOR,
    COLLADA_DATA_INPUT_TYPE_CONTINUITY,
    COLLADA_DATA_INPUT_TYPE_IMAGE,
    COLLADA_DATA_INPUT_TYPE_INPUT,
    COLLADA_DATA_INPUT_TYPE_IN_TANGENT,
    COLLADA_DATA_INPUT_TYPE_INTERPOLATION,
    COLLADA_DATA_INPUT_TYPE_INV_BIND_MATRIX,
    COLLADA_DATA_INPUT_TYPE_JOINT,
    COLLADA_DATA_INPUT_TYPE_LINEAR_STEPS,
    COLLADA_DATA_INPUT_TYPE_MORPH_TARGET,
    COLLADA_DATA_INPUT_TYPE_MORPH_WEIGHT,
    COLLADA_DATA_INPUT_TYPE_NORMAL,
    COLLADA_DATA_INPUT_TYPE_OUTPUT,
    COLLADA_DATA_INPUT_TYPE_OUT_TANGENT,
    COLLADA_DATA_INPUT_TYPE_POSITION,
    COLLADA_DATA_INPUT_TYPE_TANGENT,
    COLLADA_DATA_INPUT_TYPE_TEXBINORMAL,
    COLLADA_DATA_INPUT_TYPE_TEXCOORD,
    COLLADA_DATA_INPUT_TYPE_TEXTANGENT,
    COLLADA_DATA_INPUT_TYPE_UV,
    COLLADA_DATA_INPUT_TYPE_VERTEX,
    COLLADA_DATA_INPUT_TYPE_WEIGHT,

    /* Always last */
    COLLADA_DATA_INPUT_TYPE_COUNT,

    COLLADA_DATA_INPUT_TYPE_UNDEFINED = COLLADA_DATA_INPUT_TYPE_COUNT
} _collada_data_input_type;

/** Describes texture filtering type */
typedef enum
{
    COLLADA_DATA_SAMPLER_FILTER_NONE,
    COLLADA_DATA_SAMPLER_FILTER_NEAREST,
    COLLADA_DATA_SAMPLER_FILTER_NEAREST_MIPMAP_LINEAR,
    COLLADA_DATA_SAMPLER_FILTER_NEAREST_MIPMAP_NEAREST,
    COLLADA_DATA_SAMPLER_FILTER_LINEAR,
    COLLADA_DATA_SAMPLER_FILTER_LINEAR_MIPMAP_NEAREST,
    COLLADA_DATA_SAMPLER_FILTER_LINEAR_MIPMAP_LINEAR,

    /* Always last */
    COLLADA_DATA_SAMPLER_FILTER_UNKNOWN
} _collada_data_sampler_filter;

/** Describes transformation types supported by COLLADA common profile (v1.4) */
typedef enum
{
    COLLADA_DATA_TRANSFORMATION_TYPE_LOOKAT,
    COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX,
    COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE,
    COLLADA_DATA_TRANSFORMATION_TYPE_SCALE,
    COLLADA_DATA_TRANSFORMATION_TYPE_SKEW,
    COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE,

    /* Always last */
    COLLADA_DATA_TRANSFORMATION_TYPE_COUNT,

    COLLADA_DATA_TRANSFORMATION_TYPE_UNDEFINED = COLLADA_DATA_TRANSFORMATION_TYPE_COUNT
} _collada_data_transformation_type;

/** TODO */
typedef enum
{
    COLLADA_DATA_NODE_ITEM_TYPE_CAMERA_INSTANCE,
    COLLADA_DATA_NODE_ITEM_TYPE_GEOMETRY_INSTANCE,
    COLLADA_DATA_NODE_ITEM_TYPE_LIGHT_INSTANCE,
    COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION,
    COLLADA_DATA_NODE_ITEM_TYPE_NODE,

    COLLADA_DATA_NODE_ITEM_TYPE_UNDEFINED
} _collada_data_node_item_type;

/** Describes node types supported by COLLADA common profile (v1.4) */
typedef enum
{
    COLLADA_DATA_NODE_TYPE_JOINT,
    COLLADA_DATA_NODE_TYPE_NODE,

    /* Special node type which emulates root node, in which all sub-nodes
     * defined in COLLADA container are embedded.
     **/
    COLLADA_DATA_NODE_TYPE_FAKE_ROOT_NODE,

    /* Always last */
    COLLADA_DATA_NODE_TYPE_COUNT,

    COLLADA_DATA_NODE_TYPE_UNDEFINED = COLLADA_DATA_NODE_TYPE_COUNT
} _collada_data_node_type;

#endif /* COLLADA_TYPES_H */
