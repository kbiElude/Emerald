/**
 *
 * Emerald (kbi/elude @2015)
 *
 * This object represents a single segment node which is the smallest functional unit of
 * a postprocessing or a video segment.
 *
 * Each node can take zero or more inputs, and can expose zero or more outputs. The actual
 * functionality is implemented via call-backs. TODO: These are likely to become backend-specific
 * in longer term.
 *
 * This functionality is considered internal and should not be used by client apps. External interfaces
 * will call this code directly whenever necessary.
 *
 */
#ifndef DEMO_TIMELINE_SEGMENT_NODE_H
#define DEMO_TIMELINE_SEGMENT_NODE_H

#include "demo/demo_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

/* IDs of call-backs used by demo_timeline_video_segment */
typedef enum
{
    /* Node about to be released.
     *
     * @param arg Source demo_timeline_segment_node instance
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_ABOUT_TO_RELEASE,

    /* A texture has been attached to a node input or output.
     *
     * Synchronous call-backs only.
     *
     * @param arg demo_timeline_segment_node_callback_texture_attached_callback_argument instance.
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED,

    /* A texture is about to be detached from node input or output.
     *
     * Synchronous call-backs only.
     *
     * @param arg demo_timeline_segment_node_callback_texture_detached_callback_argument instance.
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,

    /* A new texture input has been added to the node.
     *
     * @param arg New input's ID
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,

    /* An existing texture input has just been deleted.
     *
     * @param arg Former input ID.
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_DELETED,

    /* A new texture output has been added to the node.
     *
     * Only supports synchronous call-backs.
     *
     * @param arg demo_timeline_segment_node_callback_texture_output_added_callback_argument instance.
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,

    /* An existing texture input has just been deleted.
     *
     * Only supports synchronous call-backs.
     *
     * @param arg demo_timeline_segment_node_callback_texture_output_deleted_callback_argument instance.
     */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_DELETED,

    /* Always last */
    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_COUNT
} demo_timeline_segment_node_callback_id;

typedef struct
{

    demo_timeline_segment_node           node;
    demo_timeline_segment_node_output_id output_id;

} demo_timeline_segment_node_callback_texture_output_added_callback_argument;

typedef struct
{

    uint32_t                   id;
    bool                       is_input_id;
    demo_timeline_segment_node node;
    ral_texture                texture;

} demo_timeline_segment_node_callback_texture_attached_callback_argument;

typedef demo_timeline_segment_node_callback_texture_output_added_callback_argument demo_timeline_segment_node_callback_texture_output_deleted_callback_argument;
typedef demo_timeline_segment_node_callback_texture_attached_callback_argument     demo_timeline_segment_node_callback_texture_detached_callback_argument;

typedef enum
{
    /* not settable; system_callback_manager */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER,

    /* not settable; demo_timeline_segment_node_id */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,

    /* not settable; PFNSEGMENTNODEDEINITCALLBACKPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_DEINIT_FUNC_PTR,

    /* not settable; PFNSEGMENTNODEGETPROPERTYCALLBACKPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_PROPERTY_FUNC_PTR,

    /* not settable; PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_TEXTURE_MEMORY_ALLOCATION_DETAILS_FUNC_PTR,

    /* not settable; PFNSEGMENTNODEINITCALLBACKPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_FUNC_PTR,

    /* not settable; PFNSEGMENTNODERENDERCALLBACKPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_RENDER_FUNC_PTR,

    /* not settable; PFNSEGMENTNODESETPROPERTYCALLBACKPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_SET_PROPERTY_FUNC_PTR,

    /* not settable; PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_SET_TEXTURE_MEMORY_ALLOCATION_FUNC_PTR,

    /* not settable; int */
    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_TYPE,

} demo_timeline_segment_node_property;


/** TODO */
PUBLIC bool demo_timeline_segment_node_add_texture_input(demo_timeline_segment_node           node,
                                                         const demo_texture_io_declaration*   new_input_declaration_ptr,
                                                         demo_timeline_segment_node_input_id* out_opt_input_id_ptr);

/** Adds a new texture output to the segment node.
 *
 *  @param segment                   Video segment to add a texture output to.
 *  @param new_output_properties_ptr Properties of the new output. Must not be NULL.
 *  @param out_output_id_ptr         Deref will be stored to the ID of the new output, if the request is successful.
 *                                   Must not be NULL.
 *
 *  @return true if the output was added successfully, false otherwise.
 */
PUBLIC bool demo_timeline_segment_node_add_texture_output(demo_timeline_segment_node            node,
                                                          const demo_texture_io_declaration*    new_output_declaration_ptr,
                                                          demo_timeline_segment_node_output_id* out_opt_output_id_ptr);

/** Replaces existing attachment of a texture input/output with a new texture, or just detaches existing attachment from
 *  the specified segment node input/output.
 *
 *  @param node                   Segment node to update.
 *  @param is_input_id            True if @param id is an input ID, otherwise @param id should be assumed to be an output ID.
 *  @param id                     ID of an input or output (depending on @param is_inpout_id), whose attachment should be
 *                                updated.
 *  @param texture_attachment_ptr Pointer to demo_texture_attachment_declaration instance, describing the new attachment, OR
 *                                NULL if existing attachment should just be dropped.
 *
 *  @return TODO.
 */
PUBLIC bool demo_timeline_segment_node_attach_texture_to_texture_io(demo_timeline_segment_node                 node,
                                                                    bool                                       is_input_id,
                                                                    uint32_t                                   id,
                                                                    const demo_texture_attachment_declaration* texture_attachment_ptr);

/** TODO */
PUBLIC demo_timeline_segment_node demo_timeline_segment_node_create(ral_context                                         context,
                                                                    demo_timeline_segment_type                          segment_type,
                                                                    demo_timeline_segment_node_type                     node_type,
                                                                    demo_timeline_segment_node_id                       node_id,
                                                                    PFNSEGMENTNODEDEINITCALLBACKPROC                    pfn_deinit_proc,
                                                                    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC               pfn_get_property_proc,
                                                                    PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC pfn_get_texture_memory_allocation_details_proc,
                                                                    PFNSEGMENTNODEINITCALLBACKPROC                      pfn_init_proc,
                                                                    PFNSEGMENTNODERENDERCALLBACKPROC                    pfn_render_proc,
                                                                    PFNSEGMENTNODESETPROPERTYCALLBACKPROC               pfn_set_property_proc,
                                                                    PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC        pfn_set_texture_memory_allocation_proc,
                                                                    system_hashed_ansi_string                           node_name);

/** TODO */
PUBLIC bool demo_timeline_segment_node_delete_texture_input(demo_timeline_segment_node          node,
                                                            demo_timeline_segment_node_input_id input_id);

/** TODO */
PUBLIC bool demo_timeline_segment_node_delete_texture_output(demo_timeline_segment_node           node,
                                                             demo_timeline_segment_node_output_id output_id);

/** TODO */
PUBLIC void demo_timeline_segment_node_get_io_property(demo_timeline_segment_node             node,
                                                       bool                                   is_input_io,
                                                       uint32_t                               io_id,
                                                       demo_timeline_segment_node_io_property property,
                                                       void*                                  out_result_ptr);

/** TODO */
PUBLIC void demo_timeline_segment_node_get_inputs(demo_timeline_segment_node           node,
                                                  uint32_t                             n_input_ids,
                                                  uint32_t*                            out_opt_n_input_ids_ptr,
                                                  demo_timeline_segment_node_input_id* out_opt_input_ids_ptr);


/** TODO */
PUBLIC void demo_timeline_segment_node_get_outputs(demo_timeline_segment_node            node,
                                                   uint32_t                              n_output_ids,
                                                   uint32_t*                             out_opt_n_output_ids_ptr,
                                                   demo_timeline_segment_node_output_id* out_opt_output_ids_ptr);

/** TODO */
PUBLIC void demo_timeline_segment_node_get_property(demo_timeline_segment_node          node,
                                                    demo_timeline_segment_node_property property,
                                                    void*                               out_result_ptr);

/** TODO */
PUBLIC bool demo_timeline_segment_node_get_texture_attachment(demo_timeline_segment_node node,
                                                              bool                       is_input_id,
                                                              uint32_t                   id,
                                                              ral_texture*               out_attached_texture_ptr);

/** TODO */
PUBLIC bool demo_timeline_segment_node_is_node_output_compatible_with_node_input(demo_timeline_segment_node           src_node,
                                                                                 demo_timeline_segment_node_output_id src_node_output_id,
                                                                                 demo_timeline_segment_node           dst_node,
                                                                                 demo_timeline_segment_node_input_id  dst_node_input_id);

/** TODO */
PUBLIC void demo_timeline_segment_node_release(demo_timeline_segment_node node);


#endif /* DEMO_TIMELINE_SEGMENT_NODE_H */