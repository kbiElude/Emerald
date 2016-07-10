/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef SCENE_GRAPH_H
#define SCENE_GRAPH_H

#include "curve/curve_types.h"
#include "scene/scene_types.h"
#include "system/system_types.h"

typedef void (*PFNINSERTCAMERAPROC)           (scene_camera     camera,
                                               void*            user_arg);
typedef void (*PFNINSERTLIGHTPROC)            (scene_light      light,
                                               void*            user_arg);
typedef void (*PFNINSERTMESHPROC)             (scene_mesh       mesh,
                                               void*            user_arg);
typedef void (*PFNNEWTRANSFORMATIONMATRIXPROC)(system_matrix4x4 transformation_matrix,
                                               void*            user_arg);

/* Special node tags. These are created from node SIDs and are used to mark
 * recognized transformation types. Later on, when applying curves from
 * lw_curve_dataset, these tags are used to recognize graph nodes that correspond
 * to transformations described by the curves.
 */
typedef enum
{
    SCENE_GRAPH_NODE_TAG_ROTATE_X,
    SCENE_GRAPH_NODE_TAG_ROTATE_Y,
    SCENE_GRAPH_NODE_TAG_ROTATE_Z,
    SCENE_GRAPH_NODE_TAG_SCALE,
    SCENE_GRAPH_NODE_TAG_TRANSLATE,

    SCENE_GRAPH_NODE_TAG_UNDEFINED,
    SCENE_GRAPH_NODE_TAG_COUNT = SCENE_GRAPH_NODE_TAG_UNDEFINED
} scene_graph_node_tag;

typedef enum
{
    /* NOTE: For serialization compatibility, always make sure to add
     *       new node types right BEFORE the very last item.
     */
    SCENE_GRAPH_NODE_TYPE_ROOT,
    SCENE_GRAPH_NODE_TYPE_GENERAL,
    SCENE_GRAPH_NODE_TYPE_ROTATION_DYNAMIC,
    SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC,
    SCENE_GRAPH_NODE_TYPE_MATRIX4X4_STATIC,
    SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC,
    SCENE_GRAPH_NODE_TYPE_TRANSLATION_STATIC,

    /* Always last */
    SCENE_GRAPH_NODE_TYPE_UNKNOWN
} scene_graph_node_type;

typedef enum
{
    /* NOTE: Curve containers are not available for all node types! Trying to set
     *       a curve container for a node that does not support it will result in
     *       an assertion failure.
     */
    SCENE_GRAPH_NODE_PROPERTY_CURVE_X,               /*     settable, curve_container. Releases former curve container
                                                      *               and retains the passed one upon setting. */
    SCENE_GRAPH_NODE_PROPERTY_CURVE_Y,               /*     settable, curve_container. Releases former curve container
                                                      *               and retains the passed one upon setting. */
    SCENE_GRAPH_NODE_PROPERTY_CURVE_Z,               /*     settable, curve_container. Releases former curve container
                                                      *               and retains the passed one upon setting. */
    SCENE_GRAPH_NODE_PROPERTY_CURVE_W,               /*     settable, curve_container. Releases former curve container
                                                      *               and retains the passed one upon setting. */
    SCENE_GRAPH_NODE_PROPERTY_PARENT_NODE,           /* not settable, scene_graph_node */
    SCENE_GRAPH_NODE_PROPERTY_TAG,                   /* not settable, scene_graph_node_tag */
    SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX, /* not settable, system_matrix4x4 */
    SCENE_GRAPH_NODE_PROPERTY_TYPE,                  /* not settable, scene_graph_node_type */

} scene_graph_node_property;

typedef enum
{
    /* NOTE: For serialization compatibility, always make sure to add
     *       new node types right BEFORE the very last item.
     */
    SCENE_OBJECT_TYPE_CAMERA,
    SCENE_OBJECT_TYPE_LIGHT,
    SCENE_OBJECT_TYPE_MESH,

    SCENE_OBJECT_TYPE_UNDEFINED
} _scene_object_type;

/** TODO.
 *
 *  Takes ownership of @param node.
 *
 **/
PUBLIC EMERALD_API void scene_graph_add_node(scene_graph,
                                             scene_graph_node parent_node,
                                             scene_graph_node node);

/** TODO */
PUBLIC EMERALD_API void scene_graph_attach_object_to_node(scene_graph        graph,
                                                          scene_graph_node   graph_node,
                                                          _scene_object_type object_type,
                                                          void*              instance);

/** TODO.
 *
 *  NOTE: Use scene_graph_lock() before calling this function. Otherwise, assertion
 *        failure will occur.
 **/
PUBLIC EMERALD_API void scene_graph_compute(scene_graph graph,
                                            system_time time);

/** TODO
 *
 *  NOTE: Use scene_graph_lock() before calling this function. Otherwise, assertion
 *        failure will occur.
 **/
PUBLIC EMERALD_API void scene_graph_compute_node(scene_graph      graph,
                                                 scene_graph_node node,
                                                 system_time      time);


/** TODO. */
PUBLIC EMERALD_API scene_graph scene_graph_create(scene                     owner_scene,
                                                  system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_general_node(scene_graph graph);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_rotation_dynamic_node(scene_graph          graph,
                                                                             curve_container*     rotation_vector_curves,
                                                                             bool                 expressed_in_radians,
                                                                             scene_graph_node_tag tag);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_scale_dynamic_node(scene_graph          graph,
                                                                          curve_container*     scale_vector_curves,
                                                                          scene_graph_node_tag tag);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_static_matrix4x4_transformation_node(scene_graph          graph,
                                                                                            system_matrix4x4     matrix,
                                                                                            scene_graph_node_tag tag);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_translation_dynamic_node(scene_graph          graph,
                                                                                curve_container*     translation_vector_curves,
                                                                                const bool*          negate_xyz_vectors,
                                                                                scene_graph_node_tag tag);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_translation_static_node(scene_graph          graph,
                                                                               float*               translation_vector,
                                                                               scene_graph_node_tag tag);

/** TODO. */
PUBLIC EMERALD_API scene_graph_node scene_graph_get_node_for_object(scene_graph        graph,
                                                                    _scene_object_type object_type,
                                                                    void*              object);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_get_root_node(scene_graph graph);

/** TODO
 *
 *  NOTE: For internal use only.
 *
 **/
PUBLIC scene_graph scene_graph_load(scene                     owner_scene,
                                    system_file_serializer    serializer,
                                    system_resizable_vector   serialized_scene_cameras,
                                    system_resizable_vector   serialized_scene_lights,
                                    system_resizable_vector   serialized_scene_mesh_instances,
                                    system_hashed_ansi_string object_manager_path);

/** Locks scene graph's computation operations to the calling thread.
 *
 *  @param scene_graph Graph instance to lock.
 */
PUBLIC EMERALD_API void scene_graph_lock(scene_graph graph);

#ifdef _DEBUG
    /** TODO */
    PUBLIC EMERALD_API void scene_graph_log_hierarchy(scene_graph      graph,
                                                      scene_graph_node node,
                                                      unsigned int     indent_level,
                                                      system_time      time);
#endif

/** TODO */
PUBLIC EMERALD_API void scene_graph_node_get_property(scene_graph_node          node,
                                                      scene_graph_node_property property,
                                                      void*                     out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool scene_graph_node_get_transformation_node(scene_graph_node     node,
                                                                 scene_graph_node_tag tag,
                                                                 scene_graph_node*    out_result_node);

/** TODO.
 *
 *  NOTE: src_node is released inside the function. DO NOT reuse or release after
 *        execution flow returns to the caller.
 *  NOTE: Not *all* properties are copied. Please check the implementation for details.
 *
 **/
PUBLIC EMERALD_API void scene_graph_node_replace(scene_graph      graph,
                                                 scene_graph_node dst_node,
                                                 scene_graph_node src_node);

/** TODO */
PUBLIC void scene_graph_node_release(scene_graph_node node);

/** TODO */
PUBLIC EMERALD_API void scene_graph_release(scene_graph graph);

/** TODO.
 *
 *  NOTE: For internal use only.
 **/
PUBLIC bool scene_graph_save(system_file_serializer serializer,
                             scene_graph            graph,
                             system_hash64map       camera_ptr_to_id_map,
                             system_hash64map       light_ptr_to_id_map,
                             system_hash64map       mesh_instance_ptr_to_id_map,
                             scene                  owner_scene);

/** TODO.
 *
 *  It is NOT guaranteed camera/light call-backs will precede geometry call-backs. Callers
 *  must always take this restriction into account.
 **/
PUBLIC EMERALD_API void scene_graph_traverse(scene_graph                    graph,
                                             PFNNEWTRANSFORMATIONMATRIXPROC on_new_transformation_matrix_proc,
                                             PFNINSERTCAMERAPROC            insert_camera_proc,
                                             PFNINSERTLIGHTPROC             insert_light_proc,
                                             PFNINSERTMESHPROC              insert_mesh_proc,
                                             void*                          user_arg,
                                             system_time                    frame_time);

/** Unlocks scene graph's computation operations. This call must be preceded by
 *  a scene_graph_lock() call, otherwise an assertion failure will occur.
 *
 *  @param scene_graph Graph instance to use for the operation.
 */
PUBLIC EMERALD_API void scene_graph_unlock(scene_graph graph);

#endif /* SCENE_GRAPH_H */
