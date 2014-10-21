
/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_dag.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_time.h"
#include "system/system_threads.h"
#include "system/system_variant.h"

/* Private declarations */
typedef enum
{
    /* NOTE: For serialization compatibility, always make sure to add
     *       new node types right BEFORE the very last item.
     */
    NODE_TYPE_ROOT,
    NODE_TYPE_GENERAL,
    NODE_TYPE_STATIC_MATRIX4X4,
    NODE_TYPE_TRANSLATION_DYNAMIC,
    NODE_TYPE_ROTATION_DYNAMIC,

    /* Always last */
    NODE_TYPE_UNKNOWN
} _node_type;

typedef system_matrix4x4 (*PFNUPDATEMATRIXPROC)(void*                data,
                                                system_matrix4x4     current_matrix,
                                                system_timeline_time time);

typedef struct _scene_graph_node_matrix4x4_static
{
    system_matrix4x4     matrix;
    scene_graph_node_tag tag;

    explicit _scene_graph_node_matrix4x4_static(system_matrix4x4     in_matrix,
                                                scene_graph_node_tag in_tag)
    {
        matrix = system_matrix4x4_create();
        tag    = in_tag;

        system_matrix4x4_set_from_matrix4x4(matrix, in_matrix);
    }

    ~_scene_graph_node_matrix4x4_static()
    {
        if (matrix != NULL)
        {
            system_matrix4x4_release(matrix);

            matrix = NULL;
        }
    }
} _scene_graph_node_matrix4x4_static;

typedef struct _scene_graph_node_rotation_dynamic
{
    curve_container      curves[4]; /* angle, axis xyz */
    scene_graph_node_tag tag;
    system_variant       variant_float;

    explicit _scene_graph_node_rotation_dynamic(__in_ecount(4) __notnull curve_container*     in_curves,
                                                __in                     scene_graph_node_tag in_tag)
    {
        tag = in_tag;

        for (uint32_t n_curve = 0;
                      n_curve < 4;
                    ++n_curve)
        {
            curves[n_curve] = in_curves[n_curve];

            curve_container_retain(curves[n_curve]);
        }

        variant_float = system_variant_create(SYSTEM_VARIANT_FLOAT);
    }

    ~_scene_graph_node_rotation_dynamic()
    {
        for (uint32_t n_curve = 0;
                      n_curve < 4;
                    ++n_curve)
        {
            if (curves[n_curve] != NULL)
            {
                curve_container_release(curves[n_curve]);

                curves[n_curve] = NULL;
            }
        }

        if (variant_float != NULL)
        {
            system_variant_release(variant_float);

            variant_float = NULL;
        }
    }
} _scene_graph_node_rotation_dynamic;

typedef struct _scene_graph_node_translation_dynamic
{
    curve_container      curves[3];
    scene_graph_node_tag tag;
    system_variant       variant_float;

    explicit _scene_graph_node_translation_dynamic(__in_ecount(3) __notnull curve_container*     in_curves,
                                                   __in                     scene_graph_node_tag in_tag)
    {
        tag = in_tag;

        for (uint32_t n_curve = 0;
                      n_curve < 3;
                    ++n_curve)
        {
            curves[n_curve] = in_curves[n_curve];

            curve_container_retain(curves[n_curve]);
        }

        variant_float = system_variant_create(SYSTEM_VARIANT_FLOAT);
    }

    ~_scene_graph_node_translation_dynamic()
    {
        for (uint32_t n_curve = 0;
                      n_curve < 3;
                    ++n_curve)
        {
            if (curves[n_curve] != NULL)
            {
                curve_container_release(curves[n_curve]);

                curves[n_curve] = NULL;
            }
        }

        if (variant_float != NULL)
        {
            system_variant_release(variant_float);

            variant_float = NULL;
        }
    }
} _scene_graph_node_translation_dynamic;

typedef struct _scene_graph_node
{
    system_resizable_vector attached_cameras;
    system_resizable_vector attached_lights;
    system_resizable_vector attached_meshes;
    system_dag_node         dag_node;
    void*                   data;
    system_timeline_time    last_update_time;
    _scene_graph_node*      parent_node;
    PFNUPDATEMATRIXPROC     pUpdateMatrix;
    scene_graph_node_tag    tag;
    system_matrix4x4        transformation_matrix;
    int                     transformation_matrix_index;
    _node_type              type;

    _scene_graph_node(__in_opt __maybenull _scene_graph_node* in_parent_node)
    {
        attached_cameras            = system_resizable_vector_create(4 /* capacity */, sizeof(void*) );
        attached_lights             = system_resizable_vector_create(4 /* capacity */, sizeof(void*) );
        attached_meshes             = system_resizable_vector_create(4 /* capacity */, sizeof(void*) );
        dag_node                    = NULL;
        last_update_time            = -1;
        parent_node                 = in_parent_node;
        pUpdateMatrix               = NULL;
        tag                         = SCENE_GRAPH_NODE_TAG_UNDEFINED;
        transformation_matrix       = NULL;
        transformation_matrix_index = -1;
        type                        = NODE_TYPE_UNKNOWN;
    }

    ~_scene_graph_node()
    {
        if (attached_cameras != NULL)
        {
            system_resizable_vector_release(attached_cameras);

            attached_cameras = NULL;
        }

        if (attached_lights != NULL)
        {
            system_resizable_vector_release(attached_lights);

            attached_lights = NULL;
        }

        if (attached_meshes != NULL)
        {
            system_resizable_vector_release(attached_meshes);

            attached_meshes = NULL;
        }

        if (data != NULL)
        {
            switch (type)
            {
                case NODE_TYPE_ROOT:
                {
                    /* Nothing to do here */
                    break;
                }

                case NODE_TYPE_STATIC_MATRIX4X4:
                {
                    delete (_scene_graph_node_matrix4x4_static*) data;

                    data = NULL;
                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false, "Unrecognized node type");
                }
            } /* switch (type) */
        } /* if (data != NULL) */
    }
} _scene_graph_node;

typedef struct _scene_graph
{
    system_dag              dag;
    bool                    dirty;
    system_timeline_time    dirty_time;
    system_timeline_time    last_compute_time;
    system_resizable_vector nodes;
    _scene_graph_node*      root_node_ptr;

    system_critical_section node_compute_cs;
    system_resizable_vector node_compute_vector;

    system_resizable_vector sorted_nodes; /* filled by compute() */
    system_read_write_mutex sorted_nodes_rw_mutex;

    system_critical_section compute_lock_cs;

    _scene_graph()
    {
        compute_lock_cs       = system_critical_section_create();
        dag                   = NULL;
        dirty                 = true;
        dirty_time            = 0;
        last_compute_time     = -1;
        node_compute_cs       = system_critical_section_create();
        node_compute_vector   = NULL;
        nodes                 = NULL;
        root_node_ptr         = NULL;
        sorted_nodes          = NULL;
        sorted_nodes_rw_mutex = NULL;
    }

    ~_scene_graph()
    {
        if (compute_lock_cs != NULL)
        {
            system_critical_section_release(compute_lock_cs);

            compute_lock_cs = NULL;
        }

        if (dag != NULL)
        {
            system_dag_release(dag);

            dag = NULL;
        }

        if (node_compute_cs != NULL)
        {
            system_critical_section_release(node_compute_cs);

            node_compute_cs = NULL;
        }

        if (node_compute_vector != NULL)
        {
            system_resizable_vector_release(node_compute_vector);

            node_compute_vector = NULL;
        }

        if (sorted_nodes != NULL)
        {
            system_resizable_vector_release(sorted_nodes);

            sorted_nodes = NULL;
        }

        if (sorted_nodes_rw_mutex != NULL)
        {
            system_read_write_mutex_release(sorted_nodes_rw_mutex);

            sorted_nodes_rw_mutex = NULL;
        }
    }
} _scene_graph;


/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_root_node(__in __notnull void*                data,
                                                        __in __notnull system_matrix4x4     current_matrix,
                                                        __in           system_timeline_time time)
{
    /* Ignore current matrix, just return an identity matrix */
    system_matrix4x4 result = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(result);
    return result;
}

/** TODO */
PRIVATE void _scene_graph_compute_node_transformation_matrix(__in __notnull _scene_graph_node*   node_ptr,
                                                             __in           system_timeline_time time)
{
    if (node_ptr->transformation_matrix != NULL)
    {
        system_matrix4x4_release(node_ptr->transformation_matrix);

        node_ptr->transformation_matrix = NULL;
    }

    if (node_ptr->type != NODE_TYPE_ROOT)
    {
        ASSERT_DEBUG_SYNC(node_ptr->parent_node->last_update_time == time,
                          "Parent node's update time does not match the computation time!");

        node_ptr->transformation_matrix = node_ptr->pUpdateMatrix(node_ptr->data,
                                                                  node_ptr->parent_node->transformation_matrix,
                                                                  time);
    }
    else
    {
        node_ptr->transformation_matrix = node_ptr->pUpdateMatrix(node_ptr->data,
                                                                  NULL,
                                                                  time);
    }

    node_ptr->last_update_time = time;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_general(__in __notnull void*                data,
                                                      __in __notnull system_matrix4x4     current_matrix,
                                                      __in           system_timeline_time time)
{
    _scene_graph_node_matrix4x4_static* node_data_ptr = (_scene_graph_node_matrix4x4_static*) data;
    system_matrix4x4                    new_matrix    = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4(new_matrix, current_matrix);

    return new_matrix;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_rotation_dynamic(__in __notnull void*                data,
                                                               __in __notnull system_matrix4x4     current_matrix,
                                                               __in           system_timeline_time time)
{
    _scene_graph_node_rotation_dynamic* node_data_ptr = (_scene_graph_node_rotation_dynamic*) data;
    system_matrix4x4                    new_matrix    = system_matrix4x4_create();
    system_matrix4x4                    result        = NULL;
    float                               rotation[4];

    for (uint32_t n_component = 0;
                  n_component < 4;
                ++n_component)
    {
        if (!curve_container_get_value(node_data_ptr->curves[n_component],
                                       time,
                                       false, /* should_force */
                                       node_data_ptr->variant_float) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "curve_container_get_value() failed.");
        }

        system_variant_get_float(node_data_ptr->variant_float,
                                 rotation + n_component);
    } /* for (three components) */

    system_matrix4x4_set_to_identity(new_matrix);
    system_matrix4x4_rotate         (new_matrix, DEG_TO_RAD(rotation[0]), rotation + 1);

    result = system_matrix4x4_create_by_mul(current_matrix, new_matrix);
    system_matrix4x4_release(new_matrix);

    return result;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_static_matrix4x4(__in __notnull void*                data,
                                                               __in __notnull system_matrix4x4     current_matrix,
                                                               __in           system_timeline_time time)
{
    _scene_graph_node_matrix4x4_static* node_data_ptr = (_scene_graph_node_matrix4x4_static*) data;

    return system_matrix4x4_create_by_mul(current_matrix, node_data_ptr->matrix);
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_translation_dynamic(__in __notnull void*                data,
                                                                  __in __notnull system_matrix4x4     current_matrix,
                                                                  __in           system_timeline_time time)
{
    _scene_graph_node_translation_dynamic* node_data_ptr = (_scene_graph_node_translation_dynamic*) data;
    system_matrix4x4                       new_matrix    = system_matrix4x4_create();
    system_matrix4x4                       result        = NULL;
    float                                  translation[3];

    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        if (!curve_container_get_value(node_data_ptr->curves[n_component],
                                       time,
                                       false, /* should_force */
                                       node_data_ptr->variant_float) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "curve_container_get_value() failed.");
        }

        system_variant_get_float(node_data_ptr->variant_float,
                                 translation + n_component);
    } /* for (three components) */

    system_matrix4x4_set_to_identity(new_matrix);
    system_matrix4x4_translate(new_matrix, translation);

    result = system_matrix4x4_create_by_mul(current_matrix, new_matrix);
    system_matrix4x4_release(new_matrix);

    return result;
}

/** TODO */
PRIVATE float _scene_graph_get_float_time_from_timeline_time(system_timeline_time time)
{
    float    time_float = 0.0f;
    uint32_t time_msec  = 0;

    system_time_get_msec_for_timeline_time(time, &time_msec);
    time_float = float(time_msec) / 1000.0f;

    return time_float;
}

/** TODO */
PRIVATE system_hash64map _scene_graph_get_node_hashmap(__in __notnull _scene_graph* graph_ptr)
{
    system_hash64map result = NULL;

    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        const unsigned int n_nodes = system_resizable_vector_get_amount_of_elements(graph_ptr->sorted_nodes);

        result = system_hash64map_create(sizeof(unsigned int) );

        ASSERT_DEBUG_SYNC(result != NULL, "Could not create a hash map");
        if (result == NULL)
        {
            goto end;
        }

        for (unsigned int n_node = 0;
                          n_node < n_nodes;
                        ++n_node)
        {
            const _scene_graph_node* node_ptr = NULL;

            if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                        n_node,
                                                       &node_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve node descriptor at index [%d]",
                                  n_node);
            }

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result, (system_hash64) node_ptr),
                              "Hash map already recognizes a node at index [%d]",
                              n_node);

            system_hash64map_insert(result,
                                    (system_hash64) node_ptr,
                                    (void*) n_node,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        } /* for (all graph nodes) */
    }

end:
    system_read_write_mutex_unlock(graph_ptr->sorted_nodes_rw_mutex,
                                   ACCESS_READ);

    /* All done */
    return result;
}

/** TODO */
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_matrix4x4_static(__in __notnull system_file_serializer serializer,
                                                                             __in __notnull scene_graph            result_graph,
                                                                             __in __notnull scene_graph_node       parent_node)
{
    scene_graph_node     result_node       = NULL;
    system_matrix4x4     serialized_matrix = NULL;
    scene_graph_node_tag serialized_tag    = SCENE_GRAPH_NODE_TAG_UNDEFINED;

    if (system_file_serializer_read_matrix4x4(serializer,                         &serialized_matrix) &&
        system_file_serializer_read          (serializer, sizeof(serialized_tag), &serialized_tag) )
    {
        result_node = scene_graph_add_static_matrix4x4_transformation_node(result_graph,
                                                                           parent_node,
                                                                           serialized_matrix,
                                                                           serialized_tag);

        ASSERT_DEBUG_SYNC(result_node != NULL,
                          "Static matrix4x4 transformation node serialization failed.");

        system_matrix4x4_release(serialized_matrix);
        serialized_matrix = NULL;
    }

    return result_node;
}

/** TODO */
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_rotation_dynamic(__in __notnull system_file_serializer serializer,
                                                                             __in __notnull scene_graph            result_graph,
                                                                             __in __notnull scene_graph_node       parent_node)
{
    scene_graph_node     result_node          = NULL;
    curve_container      serialized_curves[4] = {NULL, NULL, NULL, NULL};
    scene_graph_node_tag serialized_tag       = SCENE_GRAPH_NODE_TAG_UNDEFINED;

    for (unsigned int n = 0;
                      n < 4;
                    ++n)
    {
        if (!system_file_serializer_read_curve_container(serializer,
                                                         serialized_curves + n) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Dynamic rotation transformation node serialization failed.");

            goto end;
        }
    }

    if (!system_file_serializer_read(serializer,
                                     sizeof(serialized_tag),
                                    &serialized_tag) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not read tag info");

        goto end;
    }

    result_node = scene_graph_add_rotation_dynamic_node(result_graph,
                                                        parent_node,
                                                        serialized_curves,
                                                        serialized_tag);

    ASSERT_DEBUG_SYNC(result_node != NULL,
                      "Could not add rotation dynamic node");

end:
    for (unsigned int n = 0;
                      n < 4;
                    ++n)
    {
        if (serialized_curves[n] != NULL)
        {
            curve_container_release(serialized_curves[n]);

            serialized_curves[n] = NULL;
        }
    }

    return result_node;
}

/** TODO */
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_translation_dynamic(__in __notnull system_file_serializer serializer,
                                                                                __in __notnull scene_graph            result_graph,
                                                                                __in __notnull scene_graph_node       parent_node)
{
    scene_graph_node     result_node          = NULL;
    curve_container      serialized_curves[3] = {NULL, NULL, NULL};
    scene_graph_node_tag serialized_tag       = SCENE_GRAPH_NODE_TAG_UNDEFINED;

    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        if (!system_file_serializer_read_curve_container(serializer,
                                                         serialized_curves + n) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Dynamic translatioon transformation node serialization failed.");

            goto end;
        }
    }

    if (!system_file_serializer_read(serializer,
                                     sizeof(serialized_tag),
                                    &serialized_tag) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Node tag serialization failed");

        goto end;
    }

    result_node = scene_graph_add_translation_dynamic_node(result_graph,
                                                           parent_node,
                                                           serialized_curves,
                                                           serialized_tag);

    ASSERT_DEBUG_SYNC(result_node != NULL,
                      "Could not add translation dynamic node");

end:
    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        if (serialized_curves[n] != NULL)
        {
            curve_container_release(serialized_curves[n]);

            serialized_curves[n] = NULL;
        }
    }

    return result_node;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_matrix4x4_static(__in __notnull system_file_serializer              serializer,
                                                                 __in __notnull _scene_graph_node_matrix4x4_static* data_ptr)
{
    bool result = true;

    result &= system_file_serializer_write_matrix4x4(serializer,
                                                     data_ptr->matrix);
    result &= system_file_serializer_write          (serializer,
                                                     sizeof(data_ptr->tag),
                                                    &data_ptr->tag);

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_rotation_dynamic(__in __notnull system_file_serializer              serializer,
                                                                 __in __notnull _scene_graph_node_rotation_dynamic* data_ptr)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(sizeof(data_ptr->curves) / sizeof(data_ptr->curves[0]) == 4, "");

    for (unsigned int n = 0;
                      n < 4;
                    ++n)
    {
        result &= system_file_serializer_write_curve_container(serializer,
                                                               data_ptr->curves[n]);
    }

    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->tag),
                                          &data_ptr->tag);

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_translation_dynamic(__in __notnull system_file_serializer                 serializer,
                                                                    __in __notnull _scene_graph_node_translation_dynamic* data_ptr)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(sizeof(data_ptr->curves) / sizeof(data_ptr->curves[0]) == 3, "");

    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        result &= system_file_serializer_write_curve_container(serializer, data_ptr->curves[n]);
    }

    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->tag),
                                          &data_ptr->tag);

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_load_node(__in __notnull system_file_serializer  serializer,
                                    __in __notnull scene_graph             result_graph,
                                    __in __notnull system_resizable_vector serialized_nodes,
                                    __in __notnull system_resizable_vector scene_cameras_vector,
                                    __in __notnull system_resizable_vector scene_lights_vector,
                                    __in __notnull system_resizable_vector scene_mesh_instances_vector)
{
    const unsigned int n_serialized_nodes = system_resizable_vector_get_amount_of_elements(serialized_nodes);
    bool               result             = true;

    /* Retrieve node type */
    _node_type node_type = NODE_TYPE_UNKNOWN;

    result &= system_file_serializer_read(serializer,
                                          sizeof(node_type),
                                         &node_type);

    if (!result)
    {
        goto end_error;
    }

    /* If this is not a root node, retrieve parent node ID */
    scene_graph_node parent_node    = NULL;
    unsigned int     parent_node_id = 0;

    if (node_type != NODE_TYPE_ROOT)
    {
        result &= system_file_serializer_read(serializer,
                                              sizeof(parent_node_id),
                                             &parent_node_id);

        if (!result                                                  ||
            !system_resizable_vector_get_element_at(serialized_nodes,
                                                    parent_node_id,
                                                   &parent_node) )
        {
            goto end_error;
        }
    } /* if (node_type != NODE_TYPE_ROOT) */
    else
    {
        parent_node = scene_graph_get_root_node(result_graph);
    }

    /* Serialize the new node */
    scene_graph_node new_node = NULL;

    switch (node_type)
    {
        case NODE_TYPE_ROOT:
        {
            /* Nothing to serialize */
            new_node = parent_node;

            break;
        }

        case NODE_TYPE_GENERAL:
        {
            new_node = scene_graph_add_general_node(result_graph,
                                                    parent_node);

            break;
        }

        case NODE_TYPE_STATIC_MATRIX4X4:
        {
            new_node = _scene_graph_load_scene_graph_node_matrix4x4_static(serializer,
                                                                           result_graph,
                                                                           parent_node);

            break;
        }

        case NODE_TYPE_TRANSLATION_DYNAMIC:
        {
            new_node = _scene_graph_load_scene_graph_node_translation_dynamic(serializer,
                                                                              result_graph,
                                                                              parent_node);

            break;
        }

        case NODE_TYPE_ROTATION_DYNAMIC:
        {
            new_node = _scene_graph_load_scene_graph_node_rotation_dynamic(serializer,
                                                                           result_graph,
                                                                           parent_node);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized node type");

            goto end_error;
        }
    } /* switch (node_type) */

    if (new_node == NULL)
    {
        ASSERT_DEBUG_SYNC(false, "Node serialization error");

        goto end_error;
    }

    /* Attach cameras to the node */
    uint32_t n_attached_cameras = 0;

    result &= system_file_serializer_read(serializer,
                                          sizeof(n_attached_cameras),
                                          &n_attached_cameras);

    for (uint32_t n_attached_camera = 0;
                  n_attached_camera < n_attached_cameras;
                ++n_attached_camera)
    {
        /* Retrieve ID of the camera */
        scene_camera attached_camera    = NULL;
        unsigned int attached_camera_id = -1;

        result &= system_file_serializer_read(serializer,
                                              sizeof(attached_camera_id),
                                             &attached_camera_id);

        if (!result)
        {
            goto end_error;
        }

        /* Find the corresponding camera instance */
        if (!system_resizable_vector_get_element_at(scene_cameras_vector,
                                                    attached_camera_id,
                                                   &attached_camera) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find scene camera instance at index [%d]",
                              attached_camera_id);

            goto end_error;
        }

        /* Attach the camera to the node */
        scene_graph_attach_object_to_node(result_graph,
                                          new_node,
                                          SCENE_OBJECT_TYPE_CAMERA,
                                          attached_camera);
    } /* for (all attached cameras) */

    /* Attach lights to the node */
    uint32_t n_attached_lights = 0;

    result &= system_file_serializer_read(serializer,
                                          sizeof(n_attached_lights),
                                          &n_attached_lights);

    for (uint32_t n_attached_light = 0;
                  n_attached_light < n_attached_lights;
                ++n_attached_light)
    {
        /* Retrieve ID of the light */
        scene_light  attached_light    = NULL;
        unsigned int attached_light_id = -1;

        result &= system_file_serializer_read(serializer,
                                              sizeof(attached_light_id),
                                             &attached_light_id);

        if (!result)
        {
            goto end_error;
        }

        /* Find the corresponding light instance */
        if (!system_resizable_vector_get_element_at(scene_lights_vector,
                                                    attached_light_id,
                                                   &attached_light) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find scene light instance at index [%d]",
                              attached_light_id);

            goto end_error;
        }

        /* Attach the light to the node */
        scene_graph_attach_object_to_node(result_graph,
                                          new_node,
                                          SCENE_OBJECT_TYPE_LIGHT,
                                          attached_light);
    } /* for (all attached lights) */

    /* Attach mesh instances to the node */
    uint32_t n_attached_mesh_instances = 0;

    result &= system_file_serializer_read(serializer,
                                          sizeof(n_attached_mesh_instances),
                                          &n_attached_mesh_instances);

    for (uint32_t n_attached_mesh_instance = 0;
                  n_attached_mesh_instance < n_attached_mesh_instances;
                ++n_attached_mesh_instance)
    {
        /* Retrieve ID of the mesh instance */
        scene_mesh   attached_mesh_instance    = NULL;
        unsigned int attached_mesh_instance_id = -1;

        result &= system_file_serializer_read(serializer,
                                              sizeof(attached_mesh_instance_id),
                                             &attached_mesh_instance_id);

        if (!result)
        {
            goto end_error;
        }

        /* Find the corresponding mesh instance */
        if (!system_resizable_vector_get_element_at(scene_mesh_instances_vector,
                                                    attached_mesh_instance_id,
                                                   &attached_mesh_instance) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find scene mesh instance at index [%d]",
                              attached_mesh_instance_id);

            goto end_error;
        }

        /* Attach the light to the node */
        scene_graph_attach_object_to_node(result_graph,
                                          new_node,
                                          SCENE_OBJECT_TYPE_MESH,
                                          attached_mesh_instance);
    } /* for (all attached lights) */

    /* All done */
    system_resizable_vector_push(serialized_nodes, new_node);

    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false, "Node serialization failed");

    result = false;

end:
    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_node(__in __notnull system_file_serializer   serializer,
                                    __in __notnull const _scene_graph_node* node_ptr,
                                    __in __notnull system_hash64map         node_ptr_to_id_map,
                                    __in __notnull system_hash64map         camera_ptr_to_id_map,
                                    __in __notnull system_hash64map         light_ptr_to_id_map,
                                    __in __notnull system_hash64map         mesh_instance_ptr_to_id_map)
{
    bool result = true;

    /* Store node type */
    result &= system_file_serializer_write(serializer,
                                           sizeof(node_ptr->type),
                                          &node_ptr->type);

    /* Store parent node ID */
    unsigned int parent_node_id = 0;

    if (node_ptr->type != NODE_TYPE_ROOT)
    {
        if (!system_hash64map_get(node_ptr_to_id_map,
                                  (system_hash64) node_ptr->parent_node,
                                 &parent_node_id) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve parent node ID");
        }
        else
        {
            system_file_serializer_write(serializer,
                                         sizeof(parent_node_id),
                                        &parent_node_id);
        }
    } /* if (node_ptr->type != NODE_TYPE_ROOT) */

    /* Each node needs to be serialized slightly different, depending on its type.
     * When loading, these arguments will be passed to corresponding create() functions.
     */
    switch (node_ptr->type)
    {
        case NODE_TYPE_ROOT:
        {
            /* Nothing to serialize */
            break;
        }

        case NODE_TYPE_GENERAL:
        {
            /* Parent node already serialized, so nothing more to serialize */
            break;
        }

        case NODE_TYPE_STATIC_MATRIX4X4:
        {
            /* Save matrix data */
            result &= _scene_graph_save_scene_graph_node_matrix4x4_static(serializer,
                                                                          (_scene_graph_node_matrix4x4_static*) node_ptr->data);

            break;
        }

        case NODE_TYPE_TRANSLATION_DYNAMIC:
        {
            /* Save curve container data */
            result &= _scene_graph_save_scene_graph_node_translation_dynamic(serializer,
                                                                             (_scene_graph_node_translation_dynamic*) node_ptr->data);

            break;
        }

        case NODE_TYPE_ROTATION_DYNAMIC:
        {
            /* Save curve container data */
            result &= _scene_graph_save_scene_graph_node_rotation_dynamic(serializer,
                                                                          (_scene_graph_node_rotation_dynamic*) node_ptr->data);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized scene graph node type [%d]",
                               node_ptr->type);
        }
    } /* switch (node_ptr->type) */

    /* Store IDs of attached cameras */
    const uint32_t n_cameras = system_resizable_vector_get_amount_of_elements(node_ptr->attached_cameras);

    result &= system_file_serializer_write(serializer,
                                           sizeof(n_cameras),
                                          &n_cameras);

    for (uint32_t n_camera = 0;
                  n_camera < n_cameras;
                ++n_camera)
    {
        scene_camera current_camera = NULL;

        if (system_resizable_vector_get_element_at(node_ptr->attached_cameras,
                                                   n_camera,
                                                  &current_camera) )
        {
            void* camera_id_ptr = NULL;

            /* Retrieve the ID */
            if (system_hash64map_get(camera_ptr_to_id_map,
                                     (system_hash64) current_camera,
                                    &camera_id_ptr) )
            {
                unsigned int camera_id = (unsigned int) camera_id_ptr;

                result &= system_file_serializer_write(serializer,
                                                       sizeof(camera_id),
                                                      &camera_id);
            }
            else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Failed to retrieve camera ID");
            }
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Failed to retrieve camera descriptor at index [%d]",
                               n_camera);
        }
    } /* for (all attached cameras) */

    /* Store IDs of attached lights */
    const uint32_t n_lights = system_resizable_vector_get_amount_of_elements(node_ptr->attached_lights);

    result &= system_file_serializer_write(serializer,
                                           sizeof(n_lights),
                                          &n_lights);

    for (uint32_t n_light = 0;
                  n_light < n_lights;
                ++n_light)
    {
        scene_light current_light = NULL;

        if (system_resizable_vector_get_element_at(node_ptr->attached_lights,
                                                   n_light,
                                                  &current_light) )
        {
            void* light_id_ptr = NULL;

            /* Retrieve the ID */
            if (system_hash64map_get(light_ptr_to_id_map,
                                     (system_hash64) current_light,
                                    &light_id_ptr) )
            {
                unsigned int light_id = (unsigned int) light_id_ptr;

                result &= system_file_serializer_write(serializer,
                                                       sizeof(light_id),
                                                      &light_id);
            }
            else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Failed to retrieve light ID");
            }
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Failed to retrieve light descriptor at index [%d]",
                               n_light);
        }
    } /* for (all attached lights) */

    /* Store IDs of attached mesh instances */
    const uint32_t n_mesh_instances = system_resizable_vector_get_amount_of_elements(node_ptr->attached_meshes);

    result &= system_file_serializer_write(serializer,
                                           sizeof(n_mesh_instances),
                                          &n_mesh_instances);

    for (uint32_t n_mesh_instance = 0;
                  n_mesh_instance < n_mesh_instances;
                ++n_mesh_instance)
    {
        scene_light current_mesh = NULL;

        if (system_resizable_vector_get_element_at(node_ptr->attached_meshes,
                                                   n_mesh_instance,
                                                  &current_mesh) )
        {
            void* mesh_id_ptr = NULL;

            /* Retrieve the ID */
            if (system_hash64map_get(mesh_instance_ptr_to_id_map,
                                     (system_hash64) current_mesh,
                                    &mesh_id_ptr) )
            {
                unsigned int mesh_id = (unsigned int) mesh_id_ptr;

                result &= system_file_serializer_write(serializer,
                                                       sizeof(mesh_id),
                                                      &mesh_id);
            }
            else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Failed to retrieve mesh instance ID");
            }
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Failed to retrieve mesh instance descriptor at index [%d]",
                               n_mesh_instance);
        }
    } /* for (all attached mesh instances) */

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_update_sorted_nodes(__in __notnull _scene_graph* graph_ptr)
{
    bool getter_result = false;

    if (graph_ptr->dirty)
    {
        /* Solve the DAG */
        if (system_dag_solve(graph_ptr->dag) )
        {
            graph_ptr->dirty = false;

            system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                         ACCESS_WRITE);
            {
                getter_result = system_dag_get_topologically_sorted_node_values(graph_ptr->dag,
                                                                                graph_ptr->sorted_nodes);
            }
            system_read_write_mutex_unlock(graph_ptr->sorted_nodes_rw_mutex,
                                           ACCESS_WRITE);

        }
        else
        {
            LOG_FATAL("Could not compute scene graph - graph is not a DAG?");

            ASSERT_DEBUG_SYNC(false, "");
            goto end;
        }
    }
    else
    {
        getter_result = true;
    }

end:
    return getter_result;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_rotation_dynamic_node(__in           __notnull scene_graph          graph,
                                                                          __in           __notnull scene_graph_node     parent_node,
                                                                          __in_ecount(4) __notnull curve_container*     rotation_vector_curves,
                                                                          __in                     scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr       = (_scene_graph*) graph;
    _scene_graph_node* parent_node_ptr = (_scene_graph_node*) parent_node;
    _scene_graph_node* new_node_ptr    = new (std::nothrow) _scene_graph_node(parent_node_ptr);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_rotation_dynamic(rotation_vector_curves, tag);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag, new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = NODE_TYPE_ROTATION_DYNAMIC;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_rotation_dynamic;

    if (!system_dag_add_connection(graph_ptr->dag, parent_node_ptr->dag_node, new_node_ptr->dag_node) )
    {
        ASSERT_ALWAYS_SYNC(false, "Could not add connection to DAG");

        /* TODO: This is going to leak. Oh well. */
        goto end;
    }

    graph_ptr->dirty      = true;
    graph_ptr->dirty_time = system_time_now();

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_translation_dynamic_node(__in           __notnull scene_graph          graph,
                                                                             __in           __notnull scene_graph_node     parent_node,
                                                                             __in_ecount(3) __notnull curve_container*     translation_vector_curves,
                                                                             __in                     scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr       = (_scene_graph*) graph;
    _scene_graph_node* parent_node_ptr = (_scene_graph_node*) parent_node;
    _scene_graph_node* new_node_ptr    = new (std::nothrow) _scene_graph_node(parent_node_ptr);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_translation_dynamic(translation_vector_curves, tag);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag, new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = NODE_TYPE_TRANSLATION_DYNAMIC;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_translation_dynamic;

    if (!system_dag_add_connection(graph_ptr->dag, parent_node_ptr->dag_node, new_node_ptr->dag_node) )
    {
        ASSERT_ALWAYS_SYNC(false, "Could not add connection to DAG");

        /* TODO: This is going to leak. Oh well. */
        goto end;
    }

    graph_ptr->dirty      = true;
    graph_ptr->dirty_time = system_time_now();

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_general_node(__in __notnull scene_graph      graph,
                                                                 __in __notnull scene_graph_node parent_node)
{
    _scene_graph*      graph_ptr       = (_scene_graph*) graph;
    _scene_graph_node* parent_node_ptr = (_scene_graph_node*) parent_node;
    _scene_graph_node* new_node_ptr    = new (std::nothrow) _scene_graph_node(parent_node_ptr);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        goto end;
    }

    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag, new_node_ptr);
    new_node_ptr->type          = NODE_TYPE_GENERAL;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_general;

    if (!system_dag_add_connection(graph_ptr->dag, parent_node_ptr->dag_node, new_node_ptr->dag_node) )
    {
        ASSERT_ALWAYS_SYNC(false, "Could not add connection to DAG");

        /* TODO: This is going to leak. Oh well. */
        goto end;
    }

    graph_ptr->dirty      = true;
    graph_ptr->dirty_time = system_time_now();

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_add_static_matrix4x4_transformation_node(__in __notnull scene_graph          graph,
                                                                                         __in __notnull scene_graph_node     parent_node,
                                                                                         __in __notnull system_matrix4x4     matrix,
                                                                                         __in           scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr       = (_scene_graph*) graph;
    _scene_graph_node* parent_node_ptr = (_scene_graph_node*) parent_node;
    _scene_graph_node* new_node_ptr    = new (std::nothrow) _scene_graph_node(parent_node_ptr);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_matrix4x4_static(matrix, tag);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag, new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = NODE_TYPE_STATIC_MATRIX4X4;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_static_matrix4x4;

    if (!system_dag_add_connection(graph_ptr->dag, parent_node_ptr->dag_node, new_node_ptr->dag_node) )
    {
        ASSERT_ALWAYS_SYNC(false, "Could not add connection to DAG");

        /* TODO: This is going to leak. Oh well. */
        goto end;
    }

    graph_ptr->dirty      = true;
    graph_ptr->dirty_time = system_time_now();

end:
    return (scene_graph_node) new_node_ptr;
}


/** Please see header for specification */
PUBLIC EMERALD_API void scene_graph_attach_object_to_node(__in __notnull scene_graph        graph,
                                                          __in __notnull scene_graph_node   graph_node,
                                                          __in           _scene_object_type object_type,
                                                          __in __notnull void*              instance)
{
    _scene_graph*      graph_ptr = (_scene_graph*)      graph;
    _scene_graph_node* node_ptr  = (_scene_graph_node*) graph_node;

    switch (object_type)
    {
        case SCENE_OBJECT_TYPE_CAMERA:
        {
            system_resizable_vector_push(node_ptr->attached_cameras,
                                         instance);

            /* Update the scene_camera object so that it points to the node. */
            scene_camera_set_property( (scene_camera) instance,
                                      SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                      &graph_node);

            break;
        }

        case SCENE_OBJECT_TYPE_LIGHT:
        {
            system_resizable_vector_push(node_ptr->attached_lights,
                                         instance);

            break;
        }

        case SCENE_OBJECT_TYPE_MESH:
        {
            system_resizable_vector_push(node_ptr->attached_meshes,
                                         instance);

            break;
        }

        default:
        {
            LOG_FATAL("Unrecognized object type");
        }
    } /* switch (object_type) */

    graph_ptr->dirty      = true;
    graph_ptr->dirty_time = system_time_now();
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_graph_compute(__in __notnull scene_graph          graph,
                                            __in           system_timeline_time time)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    if (graph_ptr->last_compute_time == time)
    {
        /* No need to re-compute stuff. */
        goto end;
    }

    /* Sanity check */
    ASSERT_DEBUG_SYNC(system_critical_section_get_owner(graph_ptr->compute_lock_cs) == system_threads_get_thread_id(),
                      "Graph not locked");

    /* Retrieve nodes in topological order */
    bool getter_result = _scene_graph_update_sorted_nodes(graph_ptr);

    if (!getter_result)
    {
        LOG_FATAL("Could not retrieve topologically sorted node values");

        ASSERT_DEBUG_SYNC(false, "");
        goto end;
    }

    /* Iterate through all nodes and calculate transformation matrices */
    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        const unsigned int n_sorted_nodes = system_resizable_vector_get_amount_of_elements(graph_ptr->sorted_nodes);

        for (unsigned int n_sorted_node = 0;
                          n_sorted_node < n_sorted_nodes;
                        ++n_sorted_node)
        {
            _scene_graph_node* node_ptr        = NULL;
            _scene_graph_node* parent_node_ptr = NULL;

            if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                        n_sorted_node,
                                                       &node_ptr) )
            {
                LOG_FATAL("Could not retrieve topologically sorted node");

                ASSERT_DEBUG_SYNC(false, "");
                goto end_unlock_sorted_nodes;
            }

            /* Update transformation matrix */
            if (node_ptr->last_update_time != time)
            {
                _scene_graph_compute_node_transformation_matrix(node_ptr, time);
            }

            ASSERT_DEBUG_SYNC(node_ptr->transformation_matrix != NULL,
                              "pUpdateMatrix() returned NULL");
        } /* for (all sorted nodes) */
    }

    graph_ptr->dirty             = false;
    graph_ptr->last_compute_time = time;

end_unlock_sorted_nodes:
    system_read_write_mutex_unlock(graph_ptr->sorted_nodes_rw_mutex,
                                   ACCESS_READ);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_graph_compute_node(__in __notnull scene_graph          graph,
                                                 __in __notnull scene_graph_node     node,
                                                 __in           system_timeline_time time)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    /* Sanity check */
    ASSERT_DEBUG_SYNC(system_critical_section_get_owner(graph_ptr->compute_lock_cs) == system_threads_get_thread_id(),
                      "Graph not locked");

    /* We take a different approach in this function. We use an internally stored cache of
     * nodes to store a chain of nodes necessary to calculate transformation matrix for
     * each of the nodes. We proceed in the reverse order when calculating the matrices in
     * order to adhere to the parent->child dependencies.
     *
     * This function is significantly faster compared to the full-blown compute() version
     * but it only processes the nodes that are required to come up with the final transformation
     * matrix for the requested node. As such, it would be excruciantigly slow for full graph
     * processing, but is absolutely fine if we need to retrieve transformation matrix for
     * a particular node. (eg. as when we need to compute the path for a given node)
     */

    /* Cache the node chain */
    _scene_graph_node* current_node_ptr = (_scene_graph_node*) node;
    _scene_graph_node* node_ptr         = (_scene_graph_node*) node;

    system_critical_section_enter(graph_ptr->node_compute_cs);

    if (graph_ptr->node_compute_vector == NULL)
    {
        graph_ptr->node_compute_vector = system_resizable_vector_create(4, /* capacity */
                                                                        sizeof(_scene_graph_node*) );

        ASSERT_DEBUG_SYNC(graph_ptr->node_compute_vector != NULL,
                          "Could not create a node compute vector");
        if (graph_ptr->node_compute_vector == NULL)
        {
            goto end;
        }
    }

    while (current_node_ptr != NULL)
    {
        system_resizable_vector_push(graph_ptr->node_compute_vector,
                                     current_node_ptr);

        current_node_ptr = current_node_ptr->parent_node;
    } /* while (current_node_ptr != NULL) */

    /* Compute the transformation matrices for cached nodes */
    const int n_cached_nodes = (int) system_resizable_vector_get_amount_of_elements(graph_ptr->node_compute_vector);

    for (int n_cached_node  = n_cached_nodes - 1;
             n_cached_node >= 0;
           --n_cached_node)
    {
        if (!system_resizable_vector_get_element_at(graph_ptr->node_compute_vector,
                                                    n_cached_node,
                                                   &current_node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve cached node");

            goto end;
        }

        if (current_node_ptr->last_update_time != time)
        {
            _scene_graph_compute_node_transformation_matrix(current_node_ptr,
                                                            time);
        }
    } /* for (all cached nodes) */

    /* Job done! */
end:
    if (graph_ptr->node_compute_vector != NULL)
    {
        system_resizable_vector_clear(graph_ptr->node_compute_vector);
    }

    system_critical_section_leave(graph_ptr->node_compute_cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph scene_graph_create()
{
    _scene_graph* new_graph = new (std::nothrow) _scene_graph;

    if (new_graph == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        goto end;
    }

    /* Initialize the descriptor. */
    new_graph->dag                   = system_dag_create             ();
    new_graph->nodes                 = system_resizable_vector_create(4 /* capacity */,
                                                                      sizeof(_scene_graph_node*) );
    new_graph->sorted_nodes          = system_resizable_vector_create(4 /* capacity */,
                                                                      sizeof(_scene_graph_node*) );
    new_graph->sorted_nodes_rw_mutex = system_read_write_mutex_create();
    new_graph->root_node_ptr         = new (std::nothrow) _scene_graph_node(NULL /* no parent */);

    if (new_graph->dag           == NULL ||
        new_graph->nodes         == NULL ||
        new_graph->root_node_ptr == NULL ||
        new_graph->sorted_nodes  == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        goto end;
    }

    system_resizable_vector_push(new_graph->nodes, new_graph->root_node_ptr);

    /* Set up root node */
    new_graph->root_node_ptr->dag_node      = system_dag_add_node(new_graph->dag, new_graph->root_node_ptr);
    new_graph->root_node_ptr->type          = NODE_TYPE_ROOT;
    new_graph->root_node_ptr->pUpdateMatrix = _scene_graph_compute_root_node;

end:
    return (scene_graph) new_graph;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_get_node_for_object(__in __notnull scene_graph        graph,
                                                                    __in           _scene_object_type object_type,
                                                                    __in __notnull void*              object)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    if (system_resizable_vector_get_amount_of_elements(graph_ptr->sorted_nodes) == 0)
    {
        if (!_scene_graph_update_sorted_nodes(graph_ptr))
        {
            ASSERT_DEBUG_SYNC(false, "Sorted nodes update failed");

            goto end;
        }
    }

    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);

    const unsigned int n_nodes = system_resizable_vector_get_amount_of_elements(graph_ptr->sorted_nodes);
    scene_graph_node   result  = NULL;

    ASSERT_DEBUG_SYNC(n_nodes != 0,
                      "Sorted nodes vector stores 0 items.");

    for (unsigned int n_node = 0;
                      n_node < n_nodes;
                    ++n_node)
    {
        _scene_graph_node* node_ptr = NULL;

        if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                    n_node,
                                                   &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node descriptor");

            goto end;
        }

        /* Retrieve vector holding the requested object type */
        uint32_t                n_objects     = 0;
        system_resizable_vector object_vector = NULL;

        switch (object_type)
        {
            case SCENE_OBJECT_TYPE_CAMERA: object_vector = node_ptr->attached_cameras; break;
            case SCENE_OBJECT_TYPE_LIGHT:  object_vector = node_ptr->attached_lights;  break;
            case SCENE_OBJECT_TYPE_MESH:   object_vector = node_ptr->attached_meshes;  break;

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized object type");

                goto end;
            }
        } /* switch (object_type) */

        n_objects = system_resizable_vector_get_amount_of_elements(object_vector);

        for (unsigned int n_object = 0;
                          n_object < n_objects;
                        ++n_object)
        {
            void* attached_object = NULL;

            if (!system_resizable_vector_get_element_at(object_vector,
                                                        n_object,
                                                       &attached_object) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve attached object");

                goto end;
            }

            if (attached_object == object)
            {
                /* Found the owner node! */
                result = (scene_graph_node) node_ptr;

                goto end;
            }
        } /* for (all attached objects) */
    } /* for (all nodes) */

end:
    system_read_write_mutex_unlock(graph_ptr->sorted_nodes_rw_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_get_root_node(__in __notnull scene_graph graph)
{
    return (scene_graph_node) ((_scene_graph*) graph)->root_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_graph_release(__in __notnull __post_invalid scene_graph graph)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    if (graph_ptr->dag != NULL)
    {
        system_dag_release(graph_ptr->dag);

        graph_ptr->dag = NULL;
    }

    if (graph_ptr->nodes != NULL)
    {
        _scene_graph_node* node_ptr = NULL;

        while (system_resizable_vector_pop(graph_ptr->nodes, &node_ptr) )
        {
            delete node_ptr;

            node_ptr = NULL;
        }

        system_resizable_vector_release(graph_ptr->nodes);
        graph_ptr->nodes = NULL;
    }

    /* Root node is not deallocated since it's a part of nodes container */
}

/* Please see header for specification */
PUBLIC scene_graph scene_graph_load(__in __notnull system_file_serializer  serializer,
                                    __in __notnull system_resizable_vector serialized_scene_cameras,
                                    __in __notnull system_resizable_vector serialized_scene_lights,
                                    __in __notnull system_resizable_vector serialized_scene_mesh_instances)
{
    scene_graph result = scene_graph_create();

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not create a scene graph instance");

    /* Serialize all nodes. The _save() function stores the nodes in a sorted order,
     * so we should be just fine to add the nodes in the order defined in the file.
     **/
    unsigned int            n_nodes          = 0;
    system_resizable_vector serialized_nodes = system_resizable_vector_create(4, /* capacity */
                                                                              sizeof(scene_graph_node) );

    if (!system_file_serializer_read(serializer,
                                     sizeof(n_nodes),
                                     &n_nodes) )
    {
        goto end_error;
    }

    for (unsigned int n_node = 0;
                      n_node < n_nodes;
                    ++n_node)
    {
        if (!_scene_graph_load_node(serializer,
                                    result,
                                    serialized_nodes,
                                    serialized_scene_cameras,
                                    serialized_scene_lights,
                                    serialized_scene_mesh_instances) )
        {
            goto end_error;
        }
    } /* for (all nodes available for serialization) */

    /* All done */
    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false, "Scene graph serialization failed");

    if (result != NULL)
    {
        scene_graph_release(result);

        result = NULL;
    }

end:
    if (serialized_nodes != NULL)
    {
        system_resizable_vector_release(serialized_nodes);

        serialized_nodes = NULL;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_graph_lock(__in __notnull scene_graph graph)
{
    system_critical_section_enter( ((_scene_graph*) graph)->compute_lock_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_graph_node_get_property(__in  __notnull scene_graph_node          node,
                                                      __in            scene_graph_node_property property,
                                                      __out __notnull void*                     out_result)
{
    _scene_graph_node* node_ptr = (_scene_graph_node*) node;

    switch (property)
    {
        case SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX:
        {
            *(system_matrix4x4*) out_result = node_ptr->transformation_matrix;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scene_graph_node property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC bool scene_graph_save(__in __notnull system_file_serializer serializer,
                             __in __notnull const scene_graph      graph,
                             __in __notnull system_hash64map       camera_ptr_to_id_map,
                             __in __notnull system_hash64map       light_ptr_to_id_map,
                             __in __notnull system_hash64map       mesh_instance_ptr_to_id_map)
{
    const _scene_graph* graph_ptr    = (const _scene_graph*) graph;
    system_hash64map    node_hashmap = NULL;
    bool                result       = true;

    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        const unsigned int n_nodes = system_resizable_vector_get_amount_of_elements(graph_ptr->sorted_nodes);

        ASSERT_DEBUG_SYNC(n_nodes != 0,
                          "No nodes defined for a graph to be serialized.");

        /* Prepare a hash map that maps node pointers to IDs corresponding to node descriptor locations
         * in graph_ptr->nodes.
         */
        node_hashmap = _scene_graph_get_node_hashmap( (_scene_graph*) graph);

        /* Store all nodes. While we're on it, determine which of these nodes is considered a root node */
        system_file_serializer_write(serializer,
                                     sizeof(n_nodes),
                                    &n_nodes);

        for (unsigned int n_node = 0;
                          n_node < n_nodes;
                        ++n_node)
        {
            _scene_graph_node* node_ptr = NULL;

            if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                        n_node,
                                                       &node_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve node descriptor at index [%d]",
                                  n_node);

                result = false;
                goto end;
            }

            if (n_node == 0)
            {
                ASSERT_DEBUG_SYNC(node_ptr->parent_node == NULL,
                                  "Zeroth node is not a root node.");
            }

            /* Serialize the node */
            result &= _scene_graph_save_node(serializer,
                                             node_ptr,
                                             node_hashmap,
                                             camera_ptr_to_id_map,
                                             light_ptr_to_id_map,
                                             mesh_instance_ptr_to_id_map);
        } /* for (all nodes) */
    }

    /* All done */
end:
    system_read_write_mutex_unlock(graph_ptr->sorted_nodes_rw_mutex,
                                   ACCESS_READ);

    if (node_hashmap != NULL)
    {
        system_hash64map_release(node_hashmap);

        node_hashmap = NULL;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_graph_traverse(__in __notnull scene_graph                    graph,
                                             __in_opt       PFNNEWTRANSFORMATIONMATRIXPROC on_new_transformation_matrix_proc,
                                             __in_opt       PFNINSERTCAMERAPROC            insert_camera_proc,
                                             __in_opt       PFNINSERTLIGHTPROC             insert_light_proc,
                                             __in_opt       PFNINSERTMESHPROC              insert_mesh_proc,
                                             __in_opt       void*                          user_arg,
                                             __in           system_timeline_time           frame_time)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    scene_graph_lock(graph);

    if (graph_ptr->dirty                           ||
        graph_ptr->last_compute_time != frame_time)
    {
        scene_graph_compute(graph,
                            frame_time);
    }

    /* Iterate through the sorted nodes. Mark all transformation matrices as not reported. This is important
     * because we'll later need to know which matrices have already fired 'on new transformation' event */
    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        const unsigned int n_sorted_nodes = system_resizable_vector_get_amount_of_elements(graph_ptr->sorted_nodes);

        for (unsigned int n_sorted_node = 0;
                          n_sorted_node < n_sorted_nodes;
                        ++n_sorted_node)
        {
            _scene_graph_node* node_ptr = NULL;

            if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                        n_sorted_node,
                                                       &node_ptr) )
            {
                ASSERT_ALWAYS_SYNC(false, "Could not retrieve sorted node at index [%d]", n_sorted_node);

                goto end;
            }

            node_ptr->transformation_matrix_index = -1;
        } /* for (all sorted nodes) */

        /* Iterate through the sorted nodes. Fire the 'on new transformation matrix' only for the first object
         * associated with the node, and then continue with object-specific events.
         **/
        int indices_used = 0;

        for (unsigned int n_sorted_node = 0;
                          n_sorted_node < n_sorted_nodes;
                        ++n_sorted_node)
        {
            _scene_graph_node* node_ptr           = NULL;
            unsigned int       n_attached_cameras = 0;
            unsigned int       n_attached_lights  = 0;
            unsigned int       n_attached_meshes  = 0;

            if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                        n_sorted_node,
                                                       &node_ptr) )
            {
                ASSERT_ALWAYS_SYNC(false, "Could not retrieve sorted node at index [%d]", n_sorted_node);

                goto end;
            }

            /* Update the matrix */
            if (node_ptr->last_update_time != frame_time)
            {
                _scene_graph_compute_node_transformation_matrix(node_ptr,
                                                                frame_time);
            }

            /* Any cameras / meshes attached? */
            n_attached_cameras = system_resizable_vector_get_amount_of_elements(node_ptr->attached_cameras);
            n_attached_lights  = system_resizable_vector_get_amount_of_elements(node_ptr->attached_lights);
            n_attached_meshes  = system_resizable_vector_get_amount_of_elements(node_ptr->attached_meshes);

            if (n_attached_cameras != 0 ||
                n_attached_lights  != 0 ||
                n_attached_meshes  != 0)
            {
                /* Ah-hah. Has this transformation matrix been already fired? */
                if (node_ptr->transformation_matrix_index == -1)
                {
                    /* Nope, issue a notification */
                    if (on_new_transformation_matrix_proc != NULL)
                    {
                        on_new_transformation_matrix_proc(node_ptr->transformation_matrix, user_arg);
                    }

                    node_ptr->transformation_matrix_index = indices_used;
                    indices_used++;
                }

                if (insert_camera_proc != NULL ||
                    insert_mesh_proc   != NULL ||
                    insert_light_proc  != NULL)
                {
                    for (unsigned int n_iteration = 0;
                                      n_iteration < 3 /* cameras/meshes/lights */;
                                    ++n_iteration)
                    {
                        const unsigned int      n_objects = (n_iteration == 0) ? n_attached_cameras         :
                                                            (n_iteration == 1) ? n_attached_meshes          :
                                                                                 n_attached_lights;
                        system_resizable_vector objects   = (n_iteration == 0) ? node_ptr->attached_cameras :
                                                            (n_iteration == 1) ? node_ptr->attached_meshes  :
                                                                                 node_ptr->attached_lights;

                        for (unsigned int n_object = 0; n_object < n_objects; ++n_object)
                        {
                            void* object = NULL;

                            if (system_resizable_vector_get_element_at(objects, n_object, &object) )
                            {
                                if (n_iteration == 0 && insert_camera_proc != NULL)
                                {
                                    insert_camera_proc( (scene_camera) object, user_arg);
                                }
                                else
                                if (n_iteration == 1 && insert_mesh_proc != NULL)
                                {
                                    insert_mesh_proc( (scene_mesh) object, user_arg);
                                }
                                else
                                if (n_iteration == 2 && insert_light_proc != NULL)
                                {
                                    insert_light_proc( (scene_light) object, user_arg);
                                }
                            }
                            else
                            {
                                ASSERT_ALWAYS_SYNC(false,
                                                   "Could not retrieve attached object at index [%d], iteration [%d]",
                                                   n_object,
                                                   n_iteration);

                                goto end;
                            }
                        } /* for (all attached objects) */
                    } /* for (all three iterations) */
                } /* if (it makes sense to fire event call-backs) */
            } /* if (n_attached_objects != 0) */
        } /* for (all sorted nodes) */
    } /* "sorted nodes" lock */

end:
    scene_graph_unlock(graph);

    system_read_write_mutex_unlock(graph_ptr->sorted_nodes_rw_mutex,
                                   ACCESS_READ);
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_graph_unlock(__in __notnull scene_graph graph)
{
    system_critical_section_leave( ((_scene_graph*) graph)->compute_lock_cs);
}