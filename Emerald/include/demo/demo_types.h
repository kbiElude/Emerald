/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef DEMO_TYPES_H
#define DEMO_TYPES_H

#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

DECLARE_HANDLE(demo_app);
DECLARE_HANDLE(demo_flyby);
DECLARE_HANDLE(demo_loader);
DECLARE_HANDLE(demo_timeline);
DECLARE_HANDLE(demo_timeline_segment);
DECLARE_HANDLE(demo_timeline_segment_graph);
DECLARE_HANDLE(demo_timeline_segment_node);
DECLARE_HANDLE(demo_timeline_segment_node_private);
DECLARE_HANDLE(demo_window);

typedef struct
{
    // TODO: stuff like n_start_layer, n_start_mipmap, etc. should be added here one day */

    ral_texture  texture_ral;
} demo_texture_attachment_declaration;

/** Structure used to describe a new texture input. Should only be used at creation time */
typedef struct
{
    /** Name of the IO. */
    system_hashed_ansi_string name;

    /** Type of the texture that will be accepted by IO. */
    ral_texture_type texture_type;

    /** Format of the texture accepted by the new IO. */
    ral_format texture_format;

    /** Number of texture data components expected at the input, or exposed by the output. */
    unsigned int texture_n_components;

    /** Number of layers expected at the input, or exposed by the output. */
    unsigned int texture_n_layers;

    /** Number of samples the input texture must contain (input IO),
     *  or number of samples the output texture will contain (output IO). */
    unsigned int texture_n_samples;

    /** For input IOs:  tells if a texture is required in order for the parent segment/node to render.
     *  For output IOs: ignored */
    bool is_attachment_required;
} demo_texture_io_declaration;

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
        /** Specifies absolute texture size. Will only be used if absolute mode is requested. */
        uint32_t absolute_size[3];

        /** Specifies proportional texture size. Will only be used if proportional mode is requested. */
        float proportional_size[2];
    };

    /** Specifies which texture size representation should be used. */
    demo_texture_size_mode mode;
} demo_texture_size_declaration;

/* Specifies texture memory allocation. */
typedef struct
{
    ral_texture                   bound_texture;
    ral_texture_component         components[4];
    ral_format                    format;
    uint32_t                      n_layers;
    uint32_t                      n_samples;
    system_hashed_ansi_string     name;
    bool                          needs_full_mipmap_chain;
    demo_texture_size_declaration size;
    ral_texture_type              type;
} demo_texture_memory_allocation;

typedef enum
{
    // TODO: DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_BUFFER

    /* A texture input */
    DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE,

    DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_UNKNOWN
} demo_timeline_segment_node_interface_type;

typedef unsigned int demo_timeline_segment_id;
typedef unsigned int demo_timeline_segment_node_id;
typedef unsigned int demo_timeline_segment_node_input_id;
typedef unsigned int demo_timeline_segment_node_output_id;

typedef struct
{
    bool                          is_input;
    demo_timeline_segment_node_id node_id;
    uint32_t                      node_io_id;
} demo_timeline_segment_node_io;


typedef enum
{
    /* not settable; ral_texture.
     *
     * Tells what texture attachment is bound to the output.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,

    /* not settable; demo_timeline_segment_node_interface_type.
     *
     * Tells what interface the output uses (eg. DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE)
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_INTERFACE_TYPE,

    /* not settable; bool. */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_IS_REQUIRED,

    /* not settable; system_hashed_ansi_string */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_NAME,

    /* not settable; ral_texture_format.
     *
     * Tells the texture format of the output texture.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_FORMAT,

    /* not settable; uint32_t
     *
     * Tells the number of components the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_COMPONENTS,

    /* not settable; uint32_t
     *
     * Tells the number of layers the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_LAYERS,

    /* not settable; uint32_t
     *
     * Tells the number of samples the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_SAMPLES,

    /* not settable; ral_texture_type
     *
     * Tells the type of the texture the output exposes.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_TYPE,

    /* not settable; bool
     *
     * Tells if the output exposes full mipmap chain. If not, only one base mipmap
     * is going to be used.
     *
     * Query only valid for texture interfaces.
     */
    DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_USES_FULL_MIPMAP_CHAIN,
} demo_timeline_segment_node_io_property;

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


/* Video segment objects: */
typedef enum
{
    /*** VIDEO SEGMENT NODES: ***/

    /* NOTE: Update the node_data array in demo_timeline_video_segment.cc, if you move/add/remove any entries
     *       defined by this enum. */
    DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,


    /*** VIDEO SEGMENT NODES: ***/
    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT,
    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT,

    /* Always last */
    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_COUNT
} demo_timeline_segment_node_type;


/** NOTE:
 *
 *  PFNSEGMENTNODEINITCALLBACKPROC: Do not initialize texture objects that you're going to use as attachments for node outputs.
 *                                  These will be provided to the node by demo_timeline_video_segment after init() is called.
 *                                  Do NOT release these textures at deinit() time.
 */
typedef void                               (*PFNSEGMENTNODEDEINITCALLBACKPROC)                   (demo_timeline_segment_node_private node);
typedef bool                               (*PFNSEGMENTNODEGETPROPERTYCALLBACKPROC)              (demo_timeline_segment_node_private node,
                                                                                                  int                                property,
                                                                                                  void*                              out_result_ptr);
typedef bool                               (*PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC)(demo_timeline_segment_node_private node,
                                                                                                  uint32_t                           n_allocation,
                                                                                                  demo_texture_memory_allocation*    out_memory_allocation_data_ptr);
typedef demo_timeline_segment_node_private (*PFNSEGMENTNODEINITCALLBACKPROC)                     (demo_timeline_segment              segment,
                                                                                                  demo_timeline_segment_node         node,
                                                                                                  ral_context                        context);
typedef bool                               (*PFNSEGMENTNODERENDERCALLBACKPROC)                   (demo_timeline_segment_node_private node,
                                                                                                  uint32_t                           frame_index,
                                                                                                  system_time                        frame_time,
                                                                                                  const int32_t*                     rendering_area_px_topdown);
typedef bool                               (*PFNSEGMENTNODESETPROPERTYCALLBACKPROC)              (demo_timeline_segment_node_private node,
                                                                                                  int                                property,
                                                                                                  const void*                        data);
typedef void                               (*PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC)       (demo_timeline_segment_node_private node,
                                                                                                  uint32_t                           n_allocation,
                                                                                                  ral_texture                        texture);

#endif /* DEMO_TYPES_H */
