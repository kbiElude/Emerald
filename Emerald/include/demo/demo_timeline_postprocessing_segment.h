/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_TIMELINE_POSTPROCESSING_SEGMENT_H
#define DEMO_TIMELINE_POSTPROCESSING_SEGMENT_H

#include "demo/demo_types.h"
#include "ogl/ogl_types.h"   /* TODO: Remove OGL dependency */


REFCOUNT_INSERT_DECLARATIONS(demo_timeline_postprocessing_segment,
                             demo_timeline_postprocessing_segment)


/** Adds a new node to the postprocessing segment.
 *
 *  @param segment             Postprocessing segment to add a new node to.
 *  @param node_type           Type of the node which is to be added. Must not be DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT.
 *  @param name                Name of the new node.
 *  @param out_opt_node_id_ptr If not NULL, deref will be set to ID of the newly created node, assuming the function returns true.
 *                             Otherwise, the pointed memory will not be touched.
 *  @param out_opt_node_ptr    If not NULL, deref will be set to the handle of the newly created node, assuming the function returns
 *                             true. Otherwise, the pointed memory will not be touched.
 *
 *  @return true if the node is added successfuly, false otherwise.
 */
PUBLIC bool demo_timeline_postprocessing_segment_add_node(demo_timeline_postprocessing_segment           segment,
                                                          demo_timeline_postprocessing_segment_node_type node_type,
                                                          system_hashed_ansi_string                      name,
                                                          demo_timeline_segment_node_id*                 out_opt_node_id_ptr,
                                                          demo_timeline_segment_node*                    out_opt_node_ptr);

/** Connects one node's output with another node's input.
 *
 *  @param segment            Timeline segment both source and destination nodes are in.
 *  @param src_node_id        Source node's ID
 *  @param src_node_output_id Source node's output ID
 *  @param dst_node_id        Destination node's ID
 *  @param dst_node_input_id  Destination node's input ID
 *
 *  @return true if the new connection was created successfully, false otherwise. Look at the console log
 *          for more details in case of any failures.
 **/
PUBLIC bool demo_timeline_postprocessing_segment_connect_nodes(demo_timeline_postprocessing_segment segment,
                                                               demo_timeline_segment_node_id        src_node_id,
                                                               demo_timeline_segment_node_output_id src_node_output_id,
                                                               demo_timeline_segment_node_id        dst_node_id,
                                                               demo_timeline_segment_node_output_id dst_node_input_id);

/** TODO */
PUBLIC demo_timeline_postprocessing_segment demo_timeline_postprocessing_segment_create(ogl_context               context,
                                                                                        demo_timeline             owner_timeline,
                                                                                        system_hashed_ansi_string name);

/** Deletes one or more nodes from the postprocessing segment.
 *
 *  The function will fail if any of the specified nodes is connected to any other node. Such connections need to be released
 *  before the node can be deleted.
 *
 *  Output node cannot be deleted.
 *
 *  @param segment  Postprocessing segment to delete a node from.
 *  @param n_nodes  Number of node IDs specified under @param node_ids
 *  @param node_ids Exactly @param n_nodes IDs of nodes to be removed. IDs must be unique or the function will
 *                  cause a crash. Must not be NULL if @param n_nodes != 0.
 *
 *  @return true if the nodes were deleted successfully, false otherwise.
 **/
PUBLIC bool demo_timeline_postprocessing_segment_delete_nodes(demo_timeline_postprocessing_segment segment,
                                                              uint32_t                             n_nodes,
                                                              const demo_timeline_segment_node_id* node_ids);

#endif /* DEMO_TIMELINE_POSTPROCESSING_SEGMENT_H */
