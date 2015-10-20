/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_TIMELINE_VIDEO_SEGMENT_H
#define DEMO_TIMELINE_VIDEO_SEGMENT_H

#include "demo/demo_types.h"
#include "ogl/ogl_types.h"          /* TODO: Remove OGL stuff */
#include "ral/ral_types.h"
#include "system/system_types.h"

typedef PFNOGLPIPELINECALLBACKPROC PFNDEMOTIMELINEVIDEOSEGMENTPASSCALLBACKPROC;
typedef unsigned int               demo_timeline_video_segment_input_id;
typedef unsigned int               demo_timeline_video_segment_output_id;

/* Helper structure used to declare a single pass of a video segment.
 *
 * Only used by demo_timeline_add_video_segment()
 */
typedef struct
{
    /* Name of the pass. Used when presenting performance statistics */
    system_hashed_ansi_string name;

    /* Function to call back to render stage contents. */
    PFNDEMOTIMELINEVIDEOSEGMENTPASSCALLBACKPROC pfn_callback_proc;

    /* User data to pass with the call-back */
    void* user_arg;
} demo_timeline_video_segment_pass;

typedef enum
{
    /* float; settable.
     *
     * Aspect ratio which should be used for the video segment.
     */
    DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_ASPECT_RATIO,

    /* system_hashed_ansi_string; not settable.
     *
     * Name assigned to the video segment at creation time.
     */
    DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_NAME
} demo_timeline_video_segment_property;

typedef enum
{
    /* Video segment which is described by a DAG. Each node is described by a relevant render call-back func ptr,
     * inputs and outputs taken by the node. This allows to order the execution on-the-fly and parallelize the work
     * whenever the dependencies are met.
     **/
    // TODO: DEMO_TIMELINE_VIDEO_SEGMENT_TYPE_GRAPH_BASED

    /* Video segment which is built of rendering passes. These are executed sequentially one after another.
     * No dependency support is present.
     */
    DEMO_TIMELINE_VIDEO_SEGMENT_TYPE_PASS_BASED,
} demo_timeline_video_segment_type;



REFCOUNT_INSERT_DECLARATIONS(demo_timeline_video_segment,
                             demo_timeline_video_segment)


/** Appends a set of passes to the specified video segment.
 *
 *  NOTE: This function may only be called for a pass-based video segment. Assertion failure will
 *        occur otherwise.
 *
 *  @param segment  Video segment to configure.
 *  @param n_passes Number of items available under @param passes. Must be > 0.
 *  @param passes   Array of video segment pass descriptors.
 *
 *  @return true if the operation was successful, false otherwise.
 */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_passes(demo_timeline_video_segment             segment,
                                                               unsigned int                            n_passes,
                                                               const demo_timeline_video_segment_pass* passes);

/** Adds a new texture input to the video segment.
 *
 *  @param segment           Video segment to add a texture input to.
 *  @param input_name        Name of the output.
 *  @param texture_type      Type of the texture that will be accepted on input.
 *  @param texture_format    Format of the texture accepted by the new input.
 *  @param texture_n_layers  Number of layers expected at the input.
 *  @param texture_n_mipmaps Number of mipmaps expected at the input per each layer.
 *  @param texture_n_samples Number of samples the input texture must contain.
 *  @param out_input_id_ptr  Deref will be stored to the ID of the newly created input. Must not be NULL.
 *
 *  @return true if the request was handled successfully, false otherwise.
 */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_texture_input(demo_timeline_video_segment           segment,
                                                                      system_hashed_ansi_string             input_name,
                                                                      ral_texture_type                      texture_type,
                                                                      ral_texture_format                    texture_format,
                                                                      unsigned int                          texture_n_layers,
                                                                      unsigned int                          texture_n_mipmaps,
                                                                      unsigned int                          texture_n_samples,
                                                                      demo_timeline_video_segment_input_id* out_input_id_ptr);

/** Adds a new texture output to the video segment.
 *
 *  It is expected that, as the frame graph is executed, the video segments will bind actual texture instances
 *  to the output during execution. The bound textures' properties must match (or supersede for cases where
 *  this makes sense) the properties provided at add_output() time.
 *
 *  @param segment                      Video segment to add a texture output to.
 *  @param output_name                  Name of the output.
 *  @param texture_type                 Type of the texture that will be bound to the output.
 *  @param texture_internalformat       Internalformat of the texture that will be bound to the output. The specified
 *                                      internalformat must either match or be compatible with the internalformat of
*                                       the bound texture.
 *  @param texture_n_start_layer_index  Video segment is allowed to update many layers in one go. The first layer
 *                                      which may be updated is at index @param texture_n_start_layer_index, up to
 *                                      (@param texture_n_start_layer_index + @param texture_n_end_layer_index) exclusive.
 *  @param texture_n_end_layer_index    Video segment is allowed to update many layers in one go. The first layer
 *                                      which may be updated is at index @param texture_n_start_layer_index, up to
 *                                      (@param texture_n_start_layer_index + @param texture_n_end_layer_index) exclusive.
 *  @param texture_n_start_mipmap_index Video segment can update one or more mipmaps for each layer. Mipmaps from
 *                                      @param texture_n_start_mipmap_index up to @param texture_n_end_mipmap_index may
 *                                      be touched during rendering.
 *  @param texture_n_end_mipmap_index   Video segment can update one or more mipmaps for each layer. Mipmaps from
 *                                      @param texture_n_start_mipmap_index up to @param texture_n_end_mipmap_index may
 *                                      be touched during rendering.
 *  @param texture_n_samples            Number of samples used by textures that will be bound to the output.
 *  @param out_output_id_ptr            Deref will be stored to the ID of the new output, if the request is successful.
 *                                      Must not be NULL.
 *
 *  @return true if the output was added successfully, false otherwise.
 */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_texture_output(demo_timeline_video_segment            segment,
                                                                       system_hashed_ansi_string              output_name,
                                                                       ral_texture_type                       texture_type,
                                                                       ral_texture_format                     texture_format,
                                                                       unsigned int                           texture_n_start_layer_index,
                                                                       unsigned int                           texture_n_end_layer_index,
                                                                       unsigned int                           texture_n_start_mipmap_index,
                                                                       unsigned int                           texture_n_end_mipmap_index,
                                                                       unsigned int                           texture_n_samples,
                                                                       demo_timeline_video_segment_output_id* out_output_id_ptr);

/** TODO */
PUBLIC bool demo_timeline_video_segment_attach_texture_to_texture_input(demo_timeline_video_segment          segment,
                                                                        demo_timeline_video_segment_input_id input_id,
                                                                        ogl_texture                          texture,
                                                                        unsigned int                         n_start_layer,
                                                                        unsigned int                         n_start_mipmap);

/** TODO */
PUBLIC void demo_timeline_video_segment_clear_texture_attachments(demo_timeline_video_segment segment);

/** Creates a new video segment of type @param type.
 *
 *  At creation time, the video segment:
 *  - does not define any dependencies, nodes (graph-based video segment).
 *  - does not define any set of passes or outputs.
 *
 * These need to be added manually after the object is instantiated.
 *
 *  NOTE: This function should only be used by demo_timeline. Use relevant demo_timeline_add_video_segment*()
 *        funcs to instantiate a video segment.
 *
 *  @param context        Rendering context
 *  @param pipeline       Rendering pipeline
 *  @param owner_timeline Timeline instance which is going to control lifetime of the video segment.
 *  @param name           Name of the new video segment.
 *  @param type           Type of the video segment.
 *
 *  @return Requested instance.
 */
PUBLIC demo_timeline_video_segment demo_timeline_video_segment_create(ogl_context                      context,
                                                                      ogl_pipeline                     pipeline,
                                                                      demo_timeline                    owner_timeline,
                                                                      system_hashed_ansi_string        name,
                                                                      demo_timeline_video_segment_type type);

/** TODO */
PUBLIC void demo_timeline_video_segment_get_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     void*                                out_result_ptr);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_video_segment_render(demo_timeline_video_segment segment,
                                                                                  uint32_t                    frame_index,
                                                                                  system_time                 rendering_pipeline_time,
                                                                                  const int*                  rendering_area_px_topdown);

/** TODO */
PUBLIC void demo_timeline_video_segment_set_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     const void*                          data);

#endif /* DEMO_TIMELINE_VIDEO_SEGMENT_H */
