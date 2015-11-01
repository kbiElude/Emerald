/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_TYPES_H
#define DEMO_TYPES_H

#include "ogl/ogl_types.h" /* TODO: Remove OGL dep */
#include "ral/ral_types.h"
#include "system/system_types.h"

DECLARE_HANDLE(demo_app);
DECLARE_HANDLE(demo_loader);
DECLARE_HANDLE(demo_timeline);
DECLARE_HANDLE(demo_timeline_graph);
DECLARE_HANDLE(demo_timeline_postprocessing_segment);
DECLARE_HANDLE(demo_timeline_segment);
DECLARE_HANDLE(demo_timeline_segment_node);
DECLARE_HANDLE(demo_timeline_segment_node_private);
DECLARE_HANDLE(demo_timeline_video_segment);
DECLARE_HANDLE(demo_timeline_video_segment_node);

typedef struct
{
    // TODO: stuff like n_start_layer, n_start_mipmap, etc. should be added here one day */

    ogl_texture  texture; /* TODO: This should be a RAL type */
} demo_texture_attachment_declaration;

/** Structure used to describe a new texture input. Should only be used at creation time */
typedef struct
{
    /** Name of the output. */
    system_hashed_ansi_string input_name;

    /** Type of the texture that will be accepted on input. */
    ral_texture_type texture_type;

    /** Format of the texture accepted by the new input. */
    ral_texture_format texture_format;

    /** Number of texture data components expected at the input. */
    unsigned int texture_n_components;

    /** Number of layers expected at the input. */
    unsigned int texture_n_layers;

    /** Number of samples the input texture must contain. */
    unsigned int texture_n_samples;

    /** Tells if a texture is required in order for the parent segment/node to render */
    bool is_attachment_required;
} demo_texture_input_declaration;

/* Specifies representation of texture dimensions */
typedef enum
{
    /* Texture size is specified in pixels (uint32_t). This works for all RAL texture types */
    DEMO_TEXTURE_SIZE_MODE_ABSOLUTE,

    /* Texture size is specified with a normalized float, which is multiplied by rendering context's FBO dimensions.
     *
     * This works only for 2D RAL texture type */
    DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL,

    DEMO_TEXTURE_SIZE_MODE_UNKNOWN
} demo_texture_size_mode;

/* Specifies texture size. See demo_texture_size_mode documentation for more details. */
typedef struct
{
    union
    {
        /** Specifies absolute texture size. Will only be used if texture_size_mode is set to absolute. */
        uint32_t absolute_size[3];

        /** Specifies proportional texture size. Will only be used if texture_size_mode is set to proportional. */
        float proportional_size[2];
    };

    /** Specifies which texture size representation should be used. */
    demo_texture_size_mode mode;
} demo_texture_size_declaration;

/** Structure used to describe a new texture output. Should only be used at creation time */
typedef struct
{
    /* Name of the output. */
    system_hashed_ansi_string output_name;

    /* true if storage for the whole mip-map chain should be initialized.
     * false if only the base mip-map will be needed.
     */
    bool texture_needs_full_mipmap_chain;

    /* Type of the texture that will be bound to the output. */
    ral_texture_type texture_type;

    /* Internalformat of the texture that will be bound to the output. The specified
     * internalformat must either match or be compatible with the internalformat of
     *  the bound texture. */
    ral_texture_format texture_format;

    /* Video segment is allowed to update many layers in one go. The first layer
     * which may be updated is at index @param texture_n_start_layer_index, up to
     * (@param texture_n_start_layer_index + @param texture_n_end_layer_index) exclusive. */
    unsigned int texture_n_start_layer_index;

    /* Video segment is allowed to update many layers in one go. The first layer
     * which may be updated is at index @param texture_n_start_layer_index, up to
     * (@param texture_n_start_layer_index + @param texture_n_layers) exclusive. */
    unsigned int texture_n_layers;

    /* Number of samples used by textures that will be bound to the output. */
    unsigned int texture_n_samples;
    /* Number of component enums accessible under @param texture_components. Must be a value between
     * 1 and 4 inclusive. */
    unsigned int texture_n_components;

    /* A texture output must define which components of the exposed texture should be accessed.
     * This is done by passing an array of ral_texture_component values. The array must
     * hold exactly @param texture_n_components items. */
    const ral_texture_component* texture_components;

    /* Tells what size should be used for the base mipmap */
    demo_texture_size_declaration texture_size;

} demo_texture_output_declaration;

typedef enum
{
    // TODO: DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_BUFFER

    /* A texture input */
    DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE,

    DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_UNKNOWN
} demo_timeline_segment_node_interface_type;

typedef unsigned int demo_timeline_segment_node_id;
typedef unsigned int demo_timeline_segment_node_input_id;
typedef unsigned int demo_timeline_segment_node_output_id;

typedef enum
{
    /* not settable; ogl_texture.
     *
     * Tells what texture attachment is bound to the output.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_BOUND_TEXTURE,

    /* not settable; demo_timeline_segment_node_interface_type.
     *
     * Tells what interface the input uses (eg. DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE)
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_INTERFACE_TYPE,

    /* not settable; bool.
     *
     * Tells if the node input must be connected to an output in order to render.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_IS_REQUIRED,

    /* not settable; system_hashed_ansi_string
     *
     * Name of the input.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_NAME,

    /* not settable; ral_texture_type
     *
     * Tells what texture type is accepted by the input.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_TYPE,

    /* not settable; ral_texture_format
     *
     * Tells what texture format is accepted by the input.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_FORMAT,

    /* not settable; uint32_t
     *
     * Tells how many texture components are required by the input.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_COMPONENTS,

    /* not settable; uint32_t
     *
     * Tells how many texture layers are required by the input.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_LAYERS,

    /* not settable; uint32_t
     *
     * Tells how many texture samples are required by the input.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_SAMPLES
} demo_timeline_segment_node_input_property;

typedef enum
{
    /* not settable; ogl_texture.
     *
     * Tells what texture attachment is bound to the output.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_BOUND_TEXTURE,

    /* not settable; demo_timeline_segment_node_interface_type.
     *
     * Tells what interface the output uses (eg. DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE)
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_INTERFACE_TYPE,

    /* not settable; ral_texture_component[TEXTURE_N_COMPONENTS]
     *
     * Tells which components should be used as treated as output from
     * the bound texture.
     *
     * Query only valid for texture interfaes.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_COMPONENTS,

    /* not settable; ral_texture_format.
     *
     * Tells the texture format of the output texture.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_FORMAT,

    /* not settable; uint32_t
     *
     * Tells the number of components the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_COMPONENTS,

    /* not settable; uint32_t
     *
     * Tells the number of layers the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_LAYERS,

    /* not settable; uint32_t
     *
     * Tells the number of samples the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_SAMPLES,

    /* not settable; uint32_t
     *
     * Tells the start layer index the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_START_LAYER_INDEX,

    /* settable;
     *
     * uint32_t[1] for 1D texture types.
     * uint32_t[2] for 1D array / 2D / cube-map texture types.
     * uint32_t[3] for 2D array / 3D texture types.
     *
     * Absolute texture size that should be used for a texture attachment.
     *
     * Property only valid for texture interfaces.
     * Property only valid if DEMO_TIMELINE_SEGMENT_MODE_OUTPUT_PROPERTY_TEXTURE_SIZE_MODE is set to DEMO_TEXTURE_SIZE_MODE_ABSOLUTE.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_ABSOLUTE,

    /* not settable; demo_texture_size_mode
     *
     * Tells how texture's dimensions should be calculated.
     *
     * Query only valid for texture interfaces.
     **/
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_MODE,

    /* settable; float[2] for 2D texture types only.
     *
     * Normalized texture size. To obtain size in pixels, the tuple will be multiple by rendering context's
     * FBO dimensions.
     *
     * Property only valid for texture interfaces.
     * Property only valid if DEMO_TIMELINE_SEGMENT_MODE_OUTPUT_PROPERTY_TEXTURE_SIZE_MODE is set to DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_PROPORTIONAL,

    /* not settable; ral_texture_type
     *
     * Tells the type of the texture the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_TYPE,

    /* not settable; bool
     *
     * Tells if the output exposes full mipmap chain. If not, only one base mipmap
     * is going to be used.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_USES_FULL_MIPMAP_CHAIN,
} demo_timeline_segment_node_output_property;

/* Segment type */
typedef enum
{
    DEMO_TIMELINE_SEGMENT_TYPE_FIRST,

    DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING = DEMO_TIMELINE_SEGMENT_TYPE_FIRST,
    DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,

    /* TODO: DEMO_TIMELINE_SEGMENT_TYPE_AUDIO, */

    /* Always last */
    DEMO_TIMELINE_SEGMENT_TYPE_COUNT
} demo_timeline_segment_type;


/* Post-processing segment objects: */
typedef enum
{
    /* NOTE: Update the _node_data array in demo_timeline_postprocessing_segment.cc, if you move/add/remove any entries
     *       defined by this enum. */
    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT,
    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT,

    /* Always last */
    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_COUNT

} demo_timeline_postprocessing_segment_node_type;

/* Video segment objects: */
typedef enum
{
    /* NOTE: Update the _node_data array in demo_timeline_video_segment.cc, if you move/add/remove any entries
     *       defined by this enum. */
    DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT,

    DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,

    /* Always last */
    DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_COUNT
} demo_timeline_video_segment_node_type;


/** NOTE:
 *
 *  PFNSEGMENTNODEINITCALLBACKPROC: Do not initialize texture objects that you're going to use as attachments for node outputs.
 *                                  These will be provided to the node by demo_timeline_video_segment after init() is called.
 *                                  Do NOT release these textures at deinit() time.
 */
typedef RENDERING_CONTEXT_CALL void                               (*PFNSEGMENTNODEDEINITCALLBACKPROC)     (demo_timeline_segment_node_private node);
typedef                        bool                               (*PFNSEGMENTNODEGETPROPERTYCALLBACKPROC)(demo_timeline_segment_node_private node,
                                                                                                           int                                property,
                                                                                                           void*                              out_result_ptr);
typedef RENDERING_CONTEXT_CALL demo_timeline_segment_node_private (*PFNSEGMENTNODEINITCALLBACKPROC)       (demo_timeline_segment              segment,
                                                                                                           demo_timeline_segment_node         node,
                                                                                                           void*                              user_arg1,
                                                                                                           void*                              user_arg2,
                                                                                                           void*                              user_arg3);
typedef RENDERING_CONTEXT_CALL bool                               (*PFNSEGMENTNODERENDERCALLBACKPROC)     (demo_timeline_segment_node_private node,
                                                                                                           uint32_t                           frame_index,
                                                                                                           system_time                        frame_time,
                                                                                                           const int32_t*                     rendering_area_px_topdown);
typedef                        bool                               (*PFNSEGMENTNODESETPROPERTYCALLBACKPROC)(demo_timeline_segment_node_private node,
                                                                                                           int                                property,
                                                                                                           const void*                        data);
#endif /* DEMO_TYPES_H */
