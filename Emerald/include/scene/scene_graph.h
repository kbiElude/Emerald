/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SCENE_GRAPH_H
#define SCENE_GRAPH_H

#include "curve/curve_types.h"
#include "scene/scene_types.h"
#include "system/system_types.h"

typedef void (*PFNINSERTCAMERAPROC)           (__in __notnull scene_camera     camera,
                                               __in_opt       void*            user_arg);
typedef void (*PFNINSERTLIGHTPROC)            (__in __notnull scene_light      light,
                                               __in_opt       void*            user_arg);
typedef void (*PFNINSERTMESHPROC)             (__in __notnull scene_mesh       mesh,
                                               __in_opt       void*            user_arg);
typedef void (*PFNNEWTRANSFORMATIONMATRIXPROC)(__in __notnull system_matrix4x4 transformation_matrix,
                                               __in_opt       void*            user_arg);

typedef enum
{
    SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX /* not settable, system_matrix4x4 */
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

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_general_node(__in __notnull scene_graph      graph,
                                                                 __in __notnull scene_graph_node parent_node);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_rotation_dynamic_node(__in           __notnull scene_graph      graph,
                                                                          __in           __notnull scene_graph_node parent_node,
                                                                          __in_ecount(4) __notnull curve_container* rotation_vector_curves);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_static_matrix4x4_transformation_node(__in __notnull scene_graph      graph,
                                                                                         __in __notnull scene_graph_node parent_node,
                                                                                         __in __notnull system_matrix4x4 matrix);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_translation_dynamic_node(__in           __notnull scene_graph      graph,
                                                                             __in           __notnull scene_graph_node parent_node,
                                                                             __in_ecount(3) __notnull curve_container* translation_vector_curves);

/** TODO */
PUBLIC EMERALD_API void scene_graph_attach_object_to_node(__in __notnull scene_graph        graph,
                                                          __in __notnull scene_graph_node   graph_node,
                                                          __in           _scene_object_type object_type,
                                                          __in __notnull void*              instance);

/** TODO */
PUBLIC EMERALD_API void scene_graph_compute(__in __notnull scene_graph          graph,
                                            __in           system_timeline_time time);

/** TODO. */
PUBLIC EMERALD_API scene_graph scene_graph_create();

/** TODO.
 **/
PUBLIC EMERALD_API scene_graph_node scene_graph_get_node_for_object(__in __notnull scene_graph        graph,
                                                                    __in           _scene_object_type object_type,
                                                                    __in __notnull void*              object);

/** TODO */
PUBLIC EMERALD_API scene_graph_node scene_graph_get_root_node(__in __notnull scene_graph graph);

/** TODO
 *
 *  NOTE: For internal use only.
 *
 **/
PUBLIC scene_graph scene_graph_load(__in __notnull system_file_serializer  serializer,
                                    __in __notnull system_resizable_vector serialized_scene_cameras,
                                    __in __notnull system_resizable_vector serialized_scene_lights,
                                    __in __notnull system_resizable_vector serialized_scene_mesh_instances);

/** TODO */
PUBLIC EMERALD_API void scene_graph_node_get_property(__in  __notnull scene_graph_node          node,
                                                      __in            scene_graph_node_property property,
                                                      __out __notnull void*                     out_result);

/** TODO */
PUBLIC EMERALD_API void scene_graph_release(__in __notnull __post_invalid scene_graph graph);

/** TODO.
 *
 *  NOTE: For internal use only.
 **/
PUBLIC bool scene_graph_save(__in __notnull system_file_serializer serializer,
                             __in __notnull scene_graph            graph,
                             __in __notnull system_hash64map       camera_ptr_to_id_map,
                             __in __notnull system_hash64map       light_ptr_to_id_map,
                             __in __notnull system_hash64map       mesh_instance_ptr_to_id_map);

/** TODO.
 *
 *  It is NOT guaranteed camera/light call-backs will precede geometry call-backs. Callers
 *  must always take this restriction into account.
 **/
PUBLIC EMERALD_API void scene_graph_traverse(__in __notnull scene_graph                    graph,
                                             __in_opt       PFNNEWTRANSFORMATIONMATRIXPROC on_new_transformation_matrix_proc,
                                             __in_opt       PFNINSERTCAMERAPROC            insert_camera_proc,
                                             __in_opt       PFNINSERTLIGHTPROC             insert_light_proc,
                                             __in_opt       PFNINSERTMESHPROC              insert_mesh_proc,
                                             __in_opt       void*                          user_arg,
                                             __in           system_timeline_time           frame_time);

#endif /* SCENE_GRAPH_H */
