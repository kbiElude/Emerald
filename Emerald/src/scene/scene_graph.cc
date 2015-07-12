
/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_curve.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_mesh.h"
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
typedef system_matrix4x4 (*PFNUPDATEMATRIXPROC)(void*            data,
                                                system_matrix4x4 current_matrix,
                                                system_time      prev_keyframe_time,
                                                system_time      next_keyframe_time,
                                                float            lerp_factor);

/* Forward declarations */
PRIVATE void             _scene_graph_align_time_to_fps                        (scene_graph                                   graph,
                                                                                system_time                                   time,
                                                                                system_time*                                  out_prev_keyframe_time_ptr,
                                                                                system_time*                                  out_next_keyframe_time_ptr);
PRIVATE system_matrix4x4 _scene_graph_compute_root_node                        (void*                                         data,
                                                                                system_matrix4x4                              current_matrix,
                                                                                system_time                                   prev_keyframe_time,
                                                                                system_time                                   next_keyframe_time,
                                                                                float                                         lerp_factor);
PRIVATE void             _scene_graph_compute_node_transformation_matrix       (scene_graph                                   graph,
                                                                                struct _scene_graph_node*                     node_ptr,
                                                                                system_time                                   time);
PRIVATE system_matrix4x4 _scene_graph_compute_general                          (void*                                         data,
                                                                                system_matrix4x4                              current_matrix,
                                                                                system_time                                   prev_keyframe_time,
                                                                                system_time                                   next_keyframe_time,
                                                                                float                                         lerp_factor);
PRIVATE system_matrix4x4 _scene_graph_compute_rotation_dynamic                 (void*                                         data,
                                                                                system_matrix4x4                              current_matrix,
                                                                                system_time                                   prev_keyframe_time,
                                                                                system_time                                   next_keyframe_time,
                                                                                float                                         lerp_factor);
PRIVATE system_matrix4x4 _scene_graph_compute_scale_dynamic                    (void*                                         data,
                                                                                system_matrix4x4                              current_matrix,
                                                                                system_time                                   prev_keyframe_time,
                                                                                system_time                                   next_keyframe_time,
                                                                                float                                         lerp_factor);
PRIVATE system_matrix4x4 _scene_graph_compute_static_matrix4x4                 (void*                                         data,
                                                                                system_matrix4x4                              current_matrix,
                                                                                system_time                                   prev_keyframe_time,
                                                                                system_time                                   next_keyframe_time,
                                                                                float                                         lerp_factor);
PRIVATE system_matrix4x4 _scene_graph_compute_translation_dynamic              (void*                                         data,
                                                                                system_matrix4x4                              current_matrix,
                                                                                system_time                                   prev_keyframe_time,
                                                                                system_time                                   next_keyframe_time,
                                                                                float                                         lerp_factor);
PRIVATE float            _scene_graph_get_float_time_from_timeline_time        (system_time                                   time);
PRIVATE system_hash64map _scene_graph_get_node_hashmap                         (struct _scene_graph*                          graph_ptr);
PRIVATE bool             _scene_graph_load_node                                (system_file_serializer                        serializer,
                                                                                scene_graph                                   result_graph,
                                                                                system_resizable_vector                       serialized_nodes,
                                                                                system_resizable_vector                       scene_cameras_vector,
                                                                                system_resizable_vector                       scene_lights_vector,
                                                                                system_resizable_vector                       scene_mesh_instances_vector,
                                                                                scene                                         owner_scene);
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_matrix4x4_static   (system_file_serializer                        serializer,
                                                                                scene_graph                                   result_graph,
                                                                                scene_graph_node                              parent_node);
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_rotation_dynamic   (system_file_serializer                        serializer,
                                                                                scene_graph                                   result_graph,
                                                                                scene_graph_node                              parent_node,
                                                                                scene                                         owner_scene);
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_scale_dynamic      (system_file_serializer                        serializer,
                                                                                scene_graph                                   result_graph,
                                                                                scene_graph_node                              parent_node,
                                                                                scene                                         owner_scene);
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_translation_dynamic(system_file_serializer                        serializer,
                                                                                scene_graph                                   result_graph,
                                                                                scene_graph_node                              parent_node,
                                                                                scene                                         owner_scene);
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_translation_static (system_file_serializer                        serializer,
                                                                                scene_graph                                   result_graph,
                                                                                scene_graph_node                              parent_node);
PRIVATE bool             _scene_graph_save_scene_graph_node_matrix4x4_static   (system_file_serializer                        serializer,
                                                                                struct _scene_graph_node_matrix4x4_static*    data_ptr);
PRIVATE bool             _scene_graph_save_scene_graph_node_rotation_dynamic   (system_file_serializer                        serializer,
                                                                                struct _scene_graph_node_rotation_dynamic*    data_ptr,
                                                                                scene                                         owner_scene);
PRIVATE bool             _scene_graph_save_scene_graph_node_scale_dynamic      (system_file_serializer                        serializer,
                                                                                struct _scene_graph_node_scale_dynamic*       data_ptr,
                                                                                scene                                         owner_scene);
PRIVATE bool             _scene_graph_save_scene_graph_node_translation_dynamic(system_file_serializer                        serializer,
                                                                                struct _scene_graph_node_translation_dynamic* data_ptr,
                                                                                scene                                         owner_scene);
PRIVATE bool             _scene_graph_save_scene_graph_node_translation_static (system_file_serializer                        serializer,
                                                                                struct _scene_graph_node_translation_static*  data_ptr);
PRIVATE void             _scene_graph_node_release_data                        (void*                                         data,
                                                                                scene_graph_node_type                         type);
PRIVATE bool             _scene_graph_save_curve                               (scene                                         owner_scene,
                                                                                curve_container                               in_curve,
                                                                                system_file_serializer                        serializer);
PRIVATE bool             _scene_graph_save_node                                (system_file_serializer                        serializer,
                                                                                const _scene_graph_node*                      node_ptr,
                                                                                system_hash64map                              node_ptr_to_id_map,
                                                                                system_hash64map                              camera_ptr_to_id_map,
                                                                                system_hash64map                              light_ptr_to_id_map,
                                                                                system_hash64map                              mesh_instance_ptr_to_id_map,
                                                                                scene                                         owner_scene);
PRIVATE bool             _scene_graph_update_sorted_nodes                      (_scene_graph*                                 graph_ptr);


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
    bool                 uses_radians;
    system_variant       variant_float;

    explicit _scene_graph_node_rotation_dynamic(curve_container*     in_curves,
                                                scene_graph_node_tag in_tag,
                                                bool                 in_uses_radians)
    {
        tag          = in_tag;
        uses_radians = in_uses_radians;

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

typedef struct _scene_graph_node_scale_dynamic
{
    curve_container      curves[3]; /* xyz */
    scene_graph_node_tag tag;
    system_variant       variant_float;

    explicit _scene_graph_node_scale_dynamic(curve_container*     in_curves,
                                             scene_graph_node_tag in_tag)
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

    ~_scene_graph_node_scale_dynamic()
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
} _scene_graph_node_scale_dynamic;

typedef struct _scene_graph_node_translation_dynamic
{
    curve_container      curves            [3];
    bool                 negate_xyz_vectors[3];
    scene_graph_node_tag tag;
    system_variant       variant_float;

    explicit _scene_graph_node_translation_dynamic(curve_container*     in_curves,
                                                   scene_graph_node_tag in_tag,
                                                   const bool*          in_negate_xyz_vectors)
    {
        tag = in_tag;

        for (uint32_t n_curve = 0;
                      n_curve < 3;
                    ++n_curve)
        {
            curves            [n_curve] = in_curves            [n_curve];
            negate_xyz_vectors[n_curve] = in_negate_xyz_vectors[n_curve];

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

typedef struct _scene_graph_node_translation_static
{
    float                translation[3];
    scene_graph_node_tag tag;

    explicit _scene_graph_node_translation_static(float*               in_translation,
                                                  scene_graph_node_tag in_tag)
    {
        tag = in_tag;

        memcpy(translation,
               in_translation,
               sizeof(translation) );
    }

    ~_scene_graph_node_translation_static()
    {
        /* Nothing to do here */
    }
} _scene_graph_node_translation_static;

typedef struct _scene_graph_node_transformation_matrix
{
    system_matrix4x4 data;
    bool             has_fired_event;

    _scene_graph_node_transformation_matrix()
    {
        data            = NULL;
        has_fired_event = false;
    }
} _scene_graph_node_transformation_matrix;

typedef struct _scene_graph_node
{
    /* Make sure to update scene_graph_replace_node(), if you add or remove any of the existing fields */
    system_resizable_vector                 attached_cameras;
    system_resizable_vector                 attached_lights;
    system_resizable_vector                 attached_meshes;
    system_dag_node                         dag_node;
    void*                                   data;
    system_time                             last_update_time;
    _scene_graph_node*                      parent_node;
    PFNUPDATEMATRIXPROC                     pUpdateMatrix;
    scene_graph_node_tag                    tag;
    _scene_graph_node_transformation_matrix transformation_matrix;
    scene_graph_node                        transformation_nodes_by_tag[SCENE_GRAPH_NODE_TAG_COUNT];
    scene_graph_node_type                   type;

    _scene_graph_node(_scene_graph_node* in_parent_node)
    {
        attached_cameras = system_resizable_vector_create(4 /* capacity */);
        attached_lights  = system_resizable_vector_create(4 /* capacity */);
        attached_meshes  = system_resizable_vector_create(4 /* capacity */);
        dag_node         = NULL;
        last_update_time = -1;
        parent_node      = in_parent_node;
        pUpdateMatrix    = NULL;
        tag              = SCENE_GRAPH_NODE_TAG_UNDEFINED;
        type             = SCENE_GRAPH_NODE_TYPE_UNKNOWN;

        memset(transformation_nodes_by_tag,
               0,
               sizeof(transformation_nodes_by_tag) );
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

        _scene_graph_node_release_data(data, type);
    }
} _scene_graph_node;

typedef struct _scene_graph
{
    system_dag                dag;
    bool                      dirty;
    system_time               dirty_time;
    system_time               last_compute_time;
    system_resizable_vector   nodes;
    system_hashed_ansi_string object_manager_path;
    scene                     owner_scene;
    _scene_graph_node*        root_node_ptr;

    scene_graph_node          node_by_tag[SCENE_GRAPH_NODE_TAG_COUNT];

    system_critical_section   node_compute_cs;
    system_resizable_vector   node_compute_vector;

    system_resizable_vector   sorted_nodes; /* filled by compute() */
    system_read_write_mutex   sorted_nodes_rw_mutex;

    system_critical_section   compute_lock_cs;

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
        object_manager_path   = NULL;
        root_node_ptr         = NULL;
        sorted_nodes          = NULL;
        sorted_nodes_rw_mutex = NULL;

        memset(node_by_tag,
               0,
               sizeof(node_by_tag) );
    }
} _scene_graph;


/** TODO */
PRIVATE void _scene_graph_align_time_to_fps(scene_graph  graph,
                                            system_time  time,
                                            system_time* out_prev_keyframe_time_ptr,
                                            system_time* out_next_keyframe_time_ptr)
{
    float         fps       = 0.0f;
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    scene_get_property(graph_ptr->owner_scene,
                       SCENE_PROPERTY_FPS,
                      &fps);

    if (fabs(fps) >= 1e-5f)
    {
        /* How many milliseconds per frame? */
        const unsigned int ms_per_frame = (unsigned int) (1000.0f /* ms */ / fps);

        /* Find the aligned time */
        const unsigned int ms_per_frame_boundary       = ms_per_frame / 2;
              unsigned int time_ms                     = 0;
              unsigned int time_ms_modulo_ms_per_frame = 0;

        system_time_get_msec_for_time(time,
                                     &time_ms);

        time_ms_modulo_ms_per_frame = time_ms % ms_per_frame;

        if (out_prev_keyframe_time_ptr != NULL)
        {
            unsigned int temp_aligned_time_ms = time_ms - time_ms_modulo_ms_per_frame;

            *out_prev_keyframe_time_ptr = system_time_get_time_for_msec(temp_aligned_time_ms);
        }

        if (out_next_keyframe_time_ptr != NULL)
        {
            /* Align to the next closest frame */
            unsigned int temp_aligned_time_ms = time_ms + ms_per_frame - time_ms_modulo_ms_per_frame;

            *out_next_keyframe_time_ptr = system_time_get_time_for_msec(temp_aligned_time_ms);
        }
    } /* if (fabs(fps) >= 1e-5f) */
    else
    {
        if (out_prev_keyframe_time_ptr != NULL)
        {
            *out_prev_keyframe_time_ptr = time;
        }

        if (out_next_keyframe_time_ptr != NULL)
        {
            *out_next_keyframe_time_ptr = time;
        }
    }
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_root_node(void*            data,
                                                        system_matrix4x4 current_matrix,
                                                        system_time      prev_keyframe_time,
                                                        system_time      next_keyframe_time,
                                                        float            lerp_factor)
{
    /* Ignore current matrix, just return an identity matrix */
    system_matrix4x4 result = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(result);
    return result;
}

/** TODO */
PRIVATE void _scene_graph_compute_node_transformation_matrix(scene_graph        graph,
                                                             _scene_graph_node* node_ptr,
                                                             system_time        time)
{
    if (node_ptr->transformation_matrix.data != NULL)
    {
        system_matrix4x4_release(node_ptr->transformation_matrix.data);

        node_ptr->transformation_matrix.data = NULL;
    }

    /* Retrieve keyframe data */
    float       lerp_factor        = 0.0f;
    system_time prev_keyframe_time = 0;
    system_time next_keyframe_time = 0;

    _scene_graph_align_time_to_fps(graph,
                                   time,
                                  &prev_keyframe_time,
                                  &next_keyframe_time);

    if (next_keyframe_time != prev_keyframe_time)
    {
        lerp_factor = float(time - prev_keyframe_time) / float(next_keyframe_time - prev_keyframe_time);
    }
    else
    {
        /* This path will be entered if there is no FPS limiter currently enabled */
        lerp_factor = 0.0f;
    }

    /* Calculate new transformation matrix */
    if (node_ptr->type != SCENE_GRAPH_NODE_TYPE_ROOT)
    {
        ASSERT_DEBUG_SYNC(node_ptr->parent_node->last_update_time == time,
                          "Parent node's update time does not match the computation time!");

        node_ptr->transformation_matrix.data = node_ptr->pUpdateMatrix(node_ptr->data,
                                                                       node_ptr->parent_node->transformation_matrix.data,
                                                                       prev_keyframe_time,
                                                                       next_keyframe_time,
                                                                       lerp_factor);
    }
    else
    {
        node_ptr->transformation_matrix.data = node_ptr->pUpdateMatrix(node_ptr->data,
                                                                       NULL,
                                                                       prev_keyframe_time,
                                                                       next_keyframe_time,
                                                                       lerp_factor);
    }

    node_ptr->last_update_time = time;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_general(void*            data,
                                                      system_matrix4x4 current_matrix,
                                                      system_time      prev_keyframe_time,
                                                      system_time      next_keyframe_time,
                                                      float            lerp_factor)
{
    _scene_graph_node_matrix4x4_static* node_data_ptr = (_scene_graph_node_matrix4x4_static*) data;
    system_matrix4x4                    new_matrix    = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4(new_matrix,
                                        current_matrix);

    return new_matrix;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_rotation_dynamic(void*            data,
                                                               system_matrix4x4 current_matrix,
                                                               system_time      prev_keyframe_time,
                                                               system_time      next_keyframe_time,
                                                               float            lerp_factor)
{
    _scene_graph_node_rotation_dynamic* node_data_ptr = (_scene_graph_node_rotation_dynamic*) data;
    system_matrix4x4                    new_matrix    = system_matrix4x4_create();
    system_matrix4x4                    result        = NULL;
    float                               rotation_final        [4];
    float                               rotation_prev_keyframe[4];
    float                               rotation_next_keyframe[4];

    for (uint32_t n_keyframe = 0;
                  n_keyframe < 2; /* prev, next */
                ++n_keyframe)
    {
        float*      result_rotation_ptr = (n_keyframe == 0) ? rotation_prev_keyframe
                                                            : rotation_next_keyframe;
        system_time time                = (n_keyframe == 0) ? prev_keyframe_time
                                                            : next_keyframe_time;

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
                                     result_rotation_ptr + n_component);
        } /* for (all components) */
    } /* for (both keyframes) */

    /* Lerp to calculate the final rotation values */
    ASSERT_DEBUG_SYNC(lerp_factor >= 0.0f && lerp_factor <= 1.0f,
                      "LERP factor is invalid");

    for (uint32_t n_component = 0;
                  n_component < 4;
                ++n_component)
    {
        rotation_final[n_component] = rotation_prev_keyframe[n_component] +
                                      lerp_factor                         *
                                      (rotation_next_keyframe[n_component] - rotation_prev_keyframe[n_component]);
    } /* for (all components) */

    system_matrix4x4_set_to_identity(new_matrix);
    system_matrix4x4_rotate         (new_matrix,
                                     node_data_ptr->uses_radians ? rotation_final[0] : DEG_TO_RAD(rotation_final[0]),
                                     rotation_final + 1);

    result = system_matrix4x4_create_by_mul(current_matrix,
                                            new_matrix);

    system_matrix4x4_release(new_matrix);

    return result;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_scale_dynamic(void*            data,
                                                            system_matrix4x4 current_matrix,
                                                            system_time      prev_keyframe_time,
                                                            system_time      next_keyframe_time,
                                                            float            lerp_factor)
{
    _scene_graph_node_scale_dynamic* node_data_ptr = (_scene_graph_node_scale_dynamic*) data;
    system_matrix4x4                 new_matrix    = system_matrix4x4_create();
    system_matrix4x4                 result        = NULL;
    float                            scale_final        [3];
    float                            scale_prev_keyframe[3];
    float                            scale_next_keyframe[3];

    for (uint32_t n_keyframe = 0;
                  n_keyframe < 2; /* prev, next */
                ++n_keyframe)
    {
        float*      result_scale_ptr = (n_keyframe == 0) ? scale_prev_keyframe
                                                         : scale_next_keyframe;
        system_time time             = (n_keyframe == 0) ? prev_keyframe_time
                                                         : next_keyframe_time;

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
                                     result_scale_ptr + n_component);
        } /* for (all components) */
    } /* for (both keyframes) */

    /* Lerp to calculate the final scale values */
    ASSERT_DEBUG_SYNC(lerp_factor >= 0.0f && lerp_factor <= 1.0f,
                      "LERP factor is invalid");

    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        scale_final[n_component] = scale_prev_keyframe[n_component] +
                                   lerp_factor                      *
                                   (scale_next_keyframe[n_component] - scale_prev_keyframe[n_component]);
    } /* for (all components) */

    system_matrix4x4_set_to_identity(new_matrix);
    system_matrix4x4_scale          (new_matrix,
                                     scale_final);

    result = system_matrix4x4_create_by_mul(current_matrix,
                                            new_matrix);

    system_matrix4x4_release(new_matrix);

    return result;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_static_matrix4x4(void*            data,
                                                               system_matrix4x4 current_matrix,
                                                               system_time      prev_keyframe_time,
                                                               system_time      next_keyframe_time,
                                                               float            lerp_factor)
{
    _scene_graph_node_matrix4x4_static* node_data_ptr = (_scene_graph_node_matrix4x4_static*) data;

    return system_matrix4x4_create_by_mul(current_matrix,
                                          node_data_ptr->matrix);
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_translation_dynamic(void*            data,
                                                                  system_matrix4x4 current_matrix,
                                                                  system_time      prev_keyframe_time,
                                                                  system_time      next_keyframe_time,
                                                                  float            lerp_factor)
{
    _scene_graph_node_translation_dynamic* node_data_ptr = (_scene_graph_node_translation_dynamic*) data;
    system_matrix4x4                       new_matrix    = system_matrix4x4_create();
    system_matrix4x4                       result        = NULL;
    float                                  translation_final        [3];
    float                                  translation_prev_keyframe[3];
    float                                  translation_next_keyframe[3];

    for (uint32_t n_keyframe = 0;
                  n_keyframe < 2; /* prev, next */
                ++n_keyframe)
    {
        float*      result_translation_ptr = (n_keyframe == 0) ? translation_prev_keyframe
                                                               : translation_next_keyframe;
        system_time time                   = (n_keyframe == 0) ? prev_keyframe_time
                                                               : next_keyframe_time;

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
                                     result_translation_ptr + n_component);

            if (node_data_ptr->negate_xyz_vectors[n_component])
            {
                result_translation_ptr[n_component] = -result_translation_ptr[n_component];
            }
        } /* for (all components) */
    } /* for (both keyframes) */

    /* Lerp to calculate the final scale values */
    ASSERT_DEBUG_SYNC(lerp_factor >= 0.0f && lerp_factor <= 1.0f,
                      "LERP factor is invalid");

    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        translation_final[n_component] = translation_prev_keyframe[n_component] +
                                         lerp_factor                            *
                                         (translation_next_keyframe[n_component] - translation_prev_keyframe[n_component]);
    } /* for (all components) */

    system_matrix4x4_set_to_identity(new_matrix);
    system_matrix4x4_translate      (new_matrix,
                                     translation_final);

    result = system_matrix4x4_create_by_mul(current_matrix,
                                            new_matrix);

    system_matrix4x4_release(new_matrix);

    return result;
}

/** TODO */
PRIVATE system_matrix4x4 _scene_graph_compute_translation_static(void*            data,
                                                                 system_matrix4x4 current_matrix,
                                                                 system_time      prev_keyframe_time,
                                                                 system_time      next_keyframe_time,
                                                                 float            lerp_factor)
{
    _scene_graph_node_translation_static* node_data_ptr = (_scene_graph_node_translation_static*) data;
    system_matrix4x4                      result        = system_matrix4x4_create();

    /* No need to do any LERPing - static translation is static by definition */
    system_matrix4x4_set_from_matrix4x4(result,
                                        current_matrix);
    system_matrix4x4_translate         (result,
                                        node_data_ptr->translation);

    return result;
}

/** TODO */
PRIVATE float _scene_graph_get_float_time_from_timeline_time(system_time time)
{
    float    time_float = 0.0f;
    uint32_t time_msec  = 0;

    system_time_get_msec_for_time(time,
                                 &time_msec);

    time_float = float(time_msec) / 1000.0f;

    return time_float;
}

/** TODO */
PRIVATE system_hash64map _scene_graph_get_node_hashmap(_scene_graph* graph_ptr)
{
    system_hash64map result = NULL;

    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        unsigned int n_nodes = 0;

        system_resizable_vector_get_property(graph_ptr->sorted_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_nodes);

        result = system_hash64map_create(sizeof(unsigned int) );

        ASSERT_DEBUG_SYNC(result != NULL,
                          "Could not create a hash map");

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
                                    (void*)         (intptr_t) n_node,
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
PRIVATE bool _scene_graph_load_curve(scene                     owner_scene,
                                     system_hashed_ansi_string object_manager_path,
                                     curve_container*          curve_ptr,
                                     system_file_serializer    serializer)
{
    bool result = true;

    if (owner_scene != NULL)
    {
        scene_curve_id curve_id = 0;

        result &= system_file_serializer_read(serializer,
                                              sizeof(curve_id),
                                             &curve_id);

        if (result)
        {
            scene_curve scene_curve = scene_get_curve_by_id(owner_scene,
                                                            curve_id);

            scene_curve_get(scene_curve,
                            SCENE_CURVE_PROPERTY_INSTANCE,
                            curve_ptr);

            curve_container_retain(*curve_ptr);
        }
    }
    else
    {
        result &= system_file_serializer_read_curve_container(serializer,
                                                              object_manager_path,
                                                              curve_ptr);
    }

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_load_node(system_file_serializer  serializer,
                                    scene_graph             result_graph,
                                    system_resizable_vector serialized_nodes,
                                    system_resizable_vector scene_cameras_vector,
                                    system_resizable_vector scene_lights_vector,
                                    system_resizable_vector scene_mesh_instances_vector,
                                    scene                   owner_scene)
{
    _scene_graph*         graph_ptr                 = NULL;
    uint32_t              n_attached_cameras        = 0;
    uint32_t              n_attached_lights         = 0;
    uint32_t              n_attached_mesh_instances = 0;
    unsigned int          n_serialized_nodes        = 0;
    scene_graph_node      new_node                  = NULL;
    _scene_graph_node*    new_node_ptr              = NULL;
    scene_graph_node_type node_type                 = SCENE_GRAPH_NODE_TYPE_UNKNOWN;
    scene_graph_node      parent_node               = NULL;
    unsigned int          parent_node_id            = 0;
    bool                  result                    = true;

    system_resizable_vector_get_property(serialized_nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_serialized_nodes);

    /* Retrieve node type */
    result &= system_file_serializer_read(serializer,
                                          sizeof(node_type),
                                         &node_type);

    if (!result)
    {
        goto end_error;
    }

    /* If this is not a root node, retrieve parent node ID */
    if (node_type != SCENE_GRAPH_NODE_TYPE_ROOT)
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
    switch (node_type)
    {
        case SCENE_GRAPH_NODE_TYPE_ROOT:
        {
            /* Nothing to serialize */
            new_node = parent_node;

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_GENERAL:
        {
            new_node = scene_graph_create_general_node(result_graph);

            ASSERT_DEBUG_SYNC(new_node != NULL,
                              "Created general node is NULL");

            scene_graph_add_node(result_graph,
                                 parent_node,
                                 new_node);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_ROTATION_DYNAMIC:
        {
            new_node = _scene_graph_load_scene_graph_node_rotation_dynamic(serializer,
                                                                           result_graph,
                                                                           parent_node,
                                                                           owner_scene);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC:
        {
            new_node = _scene_graph_load_scene_graph_node_scale_dynamic(serializer,
                                                                        result_graph,
                                                                        parent_node,
                                                                           owner_scene);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_STATIC_MATRIX4X4:
        {
            new_node = _scene_graph_load_scene_graph_node_matrix4x4_static(serializer,
                                                                           result_graph,
                                                                           parent_node);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC:
        {
            new_node = _scene_graph_load_scene_graph_node_translation_dynamic(serializer,
                                                                              result_graph,
                                                                              parent_node,
                                                                           owner_scene);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_TRANSLATION_STATIC:
        {
            new_node = _scene_graph_load_scene_graph_node_translation_static(serializer,
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
        ASSERT_DEBUG_SYNC(false,
                          "Node serialization error");

        goto end_error;
    }

    /* Attach cameras to the node */
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

    /* If there are any objects attached to this node, cache the transformation nodes. */
    graph_ptr    = (_scene_graph*)      result_graph;
    new_node_ptr = (_scene_graph_node*) new_node;

    if (n_attached_cameras        > 0 ||
        n_attached_lights         > 0 ||
        n_attached_mesh_instances > 0)
    {
        memcpy(new_node_ptr->transformation_nodes_by_tag,
               graph_ptr->node_by_tag,
               sizeof(graph_ptr->node_by_tag) );
    }
    else
    {
        memset(new_node_ptr->transformation_nodes_by_tag,
               0,
               sizeof(new_node_ptr->transformation_nodes_by_tag) );
    }

    /* All done */
    system_resizable_vector_push(serialized_nodes,
                                 new_node);

    goto end;

end_error:
    ASSERT_DEBUG_SYNC(false,
                      "Node serialization failed");

    result = false;

end:
    return result;
}

/** TODO */
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_matrix4x4_static(system_file_serializer serializer,
                                                                             scene_graph            result_graph,
                                                                             scene_graph_node       parent_node)
{
    scene_graph_node     result_node       = NULL;
    system_matrix4x4     serialized_matrix = NULL;
    scene_graph_node_tag serialized_tag    = SCENE_GRAPH_NODE_TAG_UNDEFINED;

    if (system_file_serializer_read_matrix4x4(serializer,
                                             &serialized_matrix) &&
        system_file_serializer_read          (serializer,
                                              sizeof(serialized_tag),
                                             &serialized_tag) )
    {
        result_node = scene_graph_create_static_matrix4x4_transformation_node(result_graph,
                                                                              serialized_matrix,
                                                                              serialized_tag);

        ASSERT_DEBUG_SYNC(result_node != NULL,
                          "Static matrix4x4 transformation node serialization failed.");

        scene_graph_add_node(result_graph,
                             parent_node,
                             result_node);

        system_matrix4x4_release(serialized_matrix);
        serialized_matrix = NULL;
    }

    return result_node;
}

/** TODO */
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_rotation_dynamic(system_file_serializer serializer,
                                                                             scene_graph            result_graph,
                                                                             scene_graph_node       parent_node,
                                                                             scene                  owner_scene)
{
    _scene_graph*        graph_ptr            = (_scene_graph*) result_graph;
    scene_graph_node     result_node          = NULL;
    curve_container      serialized_curves[4] = {NULL, NULL, NULL, NULL};
    scene_graph_node_tag serialized_tag       = SCENE_GRAPH_NODE_TAG_UNDEFINED;
    bool                 uses_radians         = false;

    for (unsigned int n = 0;
                      n < 4;
                    ++n)
    {
        if (!_scene_graph_load_curve(owner_scene,
                                     graph_ptr->object_manager_path,
                                     serialized_curves + n,
                                     serializer) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Dynamic rotation transformation node serialization failed.");

            goto end;
        }
    }

    if (!system_file_serializer_read(serializer,
                                     sizeof(serialized_tag),
                                    &serialized_tag)        ||
        !system_file_serializer_read(serializer,
                                     sizeof(uses_radians),
                                    &uses_radians) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not read node data");

        goto end;
    }

    result_node = scene_graph_create_rotation_dynamic_node(result_graph,
                                                           serialized_curves,
                                                           uses_radians,
                                                           serialized_tag);

    ASSERT_DEBUG_SYNC(result_node != NULL,
                      "Could not add rotation dynamic node");

    scene_graph_add_node(result_graph,
                         parent_node,
                         result_node);

    /* Cache the node by its tag */
    if (serialized_tag < SCENE_GRAPH_NODE_TAG_COUNT)
    {
        graph_ptr->node_by_tag[serialized_tag] = result_node;
    }

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
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_scale_dynamic(system_file_serializer serializer,
                                                                          scene_graph            result_graph,
                                                                          scene_graph_node       parent_node,
                                                                          scene                  owner_scene)
{
    _scene_graph*        graph_ptr            = (_scene_graph*) result_graph;
    scene_graph_node     result_node          = NULL;
    curve_container      serialized_curves[3] = {NULL, NULL, NULL};
    scene_graph_node_tag serialized_tag       = SCENE_GRAPH_NODE_TAG_UNDEFINED;

    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        if (!_scene_graph_load_curve(owner_scene,
                                     graph_ptr->object_manager_path,
                                     serialized_curves + n,
                                     serializer) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Dynamic scale transformation node serialization failed.");

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

    result_node = scene_graph_create_scale_dynamic_node(result_graph,
                                                        serialized_curves,
                                                        serialized_tag);

    ASSERT_DEBUG_SYNC(result_node != NULL,
                      "Could not add scale dynamic node");

    scene_graph_add_node(result_graph,
                         parent_node,
                         result_node);

    /* Cache the node by its tag */
    if (serialized_tag < SCENE_GRAPH_NODE_TAG_COUNT)
    {
        graph_ptr->node_by_tag[serialized_tag] = result_node;
    }

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
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_translation_dynamic(system_file_serializer serializer,
                                                                                scene_graph            result_graph,
                                                                                scene_graph_node       parent_node,
                                                                                scene                  owner_scene)
{
    _scene_graph*        graph_ptr                   = (_scene_graph*) result_graph;
    scene_graph_node     result_node                 = NULL;
    curve_container      serialized_curves       [3] = {NULL, NULL, NULL};
    bool                 serialized_negate_curves[3] = {false, false, false};
    scene_graph_node_tag serialized_tag              = SCENE_GRAPH_NODE_TAG_UNDEFINED;

    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        if (!_scene_graph_load_curve    (owner_scene,
                                         graph_ptr->object_manager_path,
                                         serialized_curves + n,
                                         serializer)                     ||
            !system_file_serializer_read(serializer,
                                         sizeof(bool),
                                         serialized_negate_curves + n) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Dynamic translation transformation node serialization failed.");

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

    result_node = scene_graph_create_translation_dynamic_node(result_graph,
                                                              serialized_curves,
                                                              serialized_negate_curves,
                                                              serialized_tag);

    ASSERT_DEBUG_SYNC(result_node != NULL,
                      "Could not add translation dynamic node");

    scene_graph_add_node(result_graph,
                         parent_node,
                         result_node);

    /* Cache the node by its tag */
    if (serialized_tag < SCENE_GRAPH_NODE_TAG_COUNT)
    {
        graph_ptr->node_by_tag[serialized_tag] = result_node;
    }

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
PRIVATE scene_graph_node _scene_graph_load_scene_graph_node_translation_static(system_file_serializer serializer,
                                                                               scene_graph            result_graph,
                                                                               scene_graph_node       parent_node)
{
    _scene_graph*        graph_ptr                 = (_scene_graph*) result_graph;
    scene_graph_node     result_node               = NULL;
    scene_graph_node_tag serialized_tag            = SCENE_GRAPH_NODE_TAG_UNDEFINED;
    float                serialized_translation[3] = {0.0f};

    if (!system_file_serializer_read(serializer,
                                     sizeof(serialized_translation),
                                     serialized_translation)         ||
        !system_file_serializer_read(serializer,
                                     sizeof(serialized_tag),
                                    &serialized_tag) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Static translation transformation node serialization failed.");

        goto end;
    }

    result_node = scene_graph_create_translation_static_node(result_graph,
                                                             serialized_translation,
                                                             serialized_tag);

    ASSERT_DEBUG_SYNC(result_node != NULL,
                      "Could not add translation static node");

    scene_graph_add_node(result_graph,
                         parent_node,
                         result_node);

    /* Cache the node by its tag */
    if (serialized_tag < SCENE_GRAPH_NODE_TAG_COUNT)
    {
        graph_ptr->node_by_tag[serialized_tag] = result_node;
    }

end:
    return result_node;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_matrix4x4_static(system_file_serializer              serializer,
                                                                 _scene_graph_node_matrix4x4_static* data_ptr)
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
PRIVATE bool _scene_graph_save_scene_graph_node_rotation_dynamic(system_file_serializer              serializer,
                                                                 _scene_graph_node_rotation_dynamic* data_ptr,
                                                                 scene                               owner_scene)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(sizeof(data_ptr->curves) / sizeof(data_ptr->curves[0]) == 4,
                      "");

    for (unsigned int n = 0;
                      n < 4;
                    ++n)
    {
        result &= _scene_graph_save_curve(owner_scene,
                                          data_ptr->curves[n],
                                          serializer);
    }

    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->tag),
                                          &data_ptr->tag);
    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->uses_radians),
                                          &data_ptr->uses_radians);

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_scale_dynamic(system_file_serializer           serializer,
                                                              _scene_graph_node_scale_dynamic* data_ptr,
                                                              scene                            owner_scene)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(sizeof(data_ptr->curves) / sizeof(data_ptr->curves[0]) == 3,
                      "");

    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        result &= _scene_graph_save_curve(owner_scene,
                                          data_ptr->curves[n],
                                          serializer);
    }

    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->tag),
                                          &data_ptr->tag);

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_translation_dynamic(system_file_serializer                 serializer,
                                                                    _scene_graph_node_translation_dynamic* data_ptr,
                                                                    scene                                  owner_scene)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(sizeof(data_ptr->curves) / sizeof(data_ptr->curves[0]) == 3,
                      "");

    for (unsigned int n = 0;
                      n < 3;
                    ++n)
    {
        result &= _scene_graph_save_curve     (owner_scene,
                                               data_ptr->curves[n],
                                               serializer);
        result &= system_file_serializer_write(serializer,
                                               sizeof(data_ptr->negate_xyz_vectors[n]),
                                               data_ptr->negate_xyz_vectors + n);
    }

    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->tag),
                                          &data_ptr->tag);

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_scene_graph_node_translation_static(system_file_serializer                serializer,
                                                                   _scene_graph_node_translation_static* data_ptr)
{
    bool result = true;

    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->translation),
                                           data_ptr->translation);
    result &= system_file_serializer_write(serializer,
                                           sizeof(data_ptr->tag),
                                          &data_ptr->tag);

    return result;
}

/** TODO */
PRIVATE void _scene_graph_node_release_data(void*                 data,
                                            scene_graph_node_type type)
{
    if (data != NULL)
    {
        switch (type)
        {
            case SCENE_GRAPH_NODE_TYPE_GENERAL:
            case SCENE_GRAPH_NODE_TYPE_ROOT:
            {
                /* Nothing to do here */
                break;
            }

            case SCENE_GRAPH_NODE_TYPE_ROTATION_DYNAMIC:
            {
                delete (_scene_graph_node_rotation_dynamic*) data;

                data = NULL;
                break;
            }

            case SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC:
            {
                delete (_scene_graph_node_scale_dynamic*) data;

                data = NULL;
                break;
            }

            case SCENE_GRAPH_NODE_TYPE_STATIC_MATRIX4X4:
            {
                delete (_scene_graph_node_matrix4x4_static*) data;

                data = NULL;
                break;
            }

            case SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC:
            {
                delete (_scene_graph_node_translation_dynamic*) data;

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

/** TODO */
PRIVATE bool _scene_graph_save_curve(scene                  owner_scene,
                                     curve_container        in_curve,
                                     system_file_serializer serializer)
{
    bool result = true;

    if (owner_scene != NULL)
    {
        scene_curve    curve    = scene_get_curve_by_container(owner_scene,
                                                               in_curve);
        scene_curve_id curve_id = 0;

        scene_curve_get(curve,
                        SCENE_CURVE_PROPERTY_ID,
                       &curve_id);

        result &= system_file_serializer_write(serializer,
                                               sizeof(curve_id),
                                              &curve_id);
    }
    else
    {
        result &= system_file_serializer_write_curve_container(serializer,
                                                               in_curve);
    }

    return result;
}

/** TODO */
PRIVATE bool _scene_graph_save_node(system_file_serializer   serializer,
                                    const _scene_graph_node* node_ptr,
                                    system_hash64map         node_ptr_to_id_map,
                                    system_hash64map         camera_ptr_to_id_map,
                                    system_hash64map         light_ptr_to_id_map,
                                    system_hash64map         mesh_instance_ptr_to_id_map,
                                    scene                    owner_scene)
{
    uint32_t     n_cameras        = 0;
    uint32_t     n_lights         = 0;
    uint32_t     n_mesh_instances = 0;
    unsigned int parent_node_id   = 0;
    bool         result           = true;

    /* Store node type */
    result &= system_file_serializer_write(serializer,
                                           sizeof(node_ptr->type),
                                          &node_ptr->type);

    /* Store parent node ID */
    if (node_ptr->type != SCENE_GRAPH_NODE_TYPE_ROOT)
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
        case SCENE_GRAPH_NODE_TYPE_ROOT:
        {
            /* Nothing to serialize */
            break;
        }

        case SCENE_GRAPH_NODE_TYPE_GENERAL:
        {
            /* Parent node already serialized, so nothing more to serialize */
            break;
        }

        case SCENE_GRAPH_NODE_TYPE_ROTATION_DYNAMIC:
        {
            /* Save curve container data */
            result &= _scene_graph_save_scene_graph_node_rotation_dynamic(serializer,
                                                                          (_scene_graph_node_rotation_dynamic*) node_ptr->data,
                                                                          owner_scene);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC:
        {
            /* Save curve container data */
            result &= _scene_graph_save_scene_graph_node_scale_dynamic(serializer,
                                                                       (_scene_graph_node_scale_dynamic*) node_ptr->data,
                                                                          owner_scene);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_STATIC_MATRIX4X4:
        {
            /* Save matrix data */
            result &= _scene_graph_save_scene_graph_node_matrix4x4_static(serializer,
                                                                          (_scene_graph_node_matrix4x4_static*) node_ptr->data);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC:
        {
            /* Save curve container data */
            result &= _scene_graph_save_scene_graph_node_translation_dynamic(serializer,
                                                                             (_scene_graph_node_translation_dynamic*) node_ptr->data,
                                                                             owner_scene);

            break;
        }

        case SCENE_GRAPH_NODE_TYPE_TRANSLATION_STATIC:
        {
            /* Save curve container data */
            result &= _scene_graph_save_scene_graph_node_translation_static(serializer,
                                                                            (_scene_graph_node_translation_static*) node_ptr->data);

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
    system_resizable_vector_get_property(node_ptr->attached_cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_cameras);

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
                unsigned int camera_id = (unsigned int) (intptr_t) camera_id_ptr;

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
    system_resizable_vector_get_property(node_ptr->attached_lights,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_lights);

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
                unsigned int light_id = (unsigned int) (intptr_t) light_id_ptr;

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
    system_resizable_vector_get_property(node_ptr->attached_meshes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_mesh_instances);

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
                unsigned int mesh_id = (unsigned int) (intptr_t) mesh_id_ptr;

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
PRIVATE bool _scene_graph_update_sorted_nodes(_scene_graph* graph_ptr)
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

            ASSERT_DEBUG_SYNC(false,
                              "");

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
PUBLIC EMERALD_API void scene_graph_add_node(scene_graph      graph,
                                             scene_graph_node parent_node,
                                             scene_graph_node node)
{
    _scene_graph*      graph_ptr       = (_scene_graph*)      graph;
    _scene_graph_node* node_ptr        = (_scene_graph_node*) node;
    _scene_graph_node* parent_node_ptr = (_scene_graph_node*) parent_node;

    ASSERT_DEBUG_SYNC(parent_node != NULL,
                      "Parent node is NULL?");
    ASSERT_DEBUG_SYNC(node_ptr->parent_node == NULL,
                      "Node already has a parent");

    node_ptr->parent_node = (_scene_graph_node*) parent_node;

    if (!system_dag_add_connection(graph_ptr->dag,
                                   parent_node_ptr->dag_node,
                                   node_ptr->dag_node) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not add connection to DAG");

        goto end;
    }

    graph_ptr->dirty      = true;
    graph_ptr->dirty_time = system_time_now();

    system_resizable_vector_push(graph_ptr->nodes,
                                 node_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_graph_attach_object_to_node(scene_graph        graph,
                                                          scene_graph_node   graph_node,
                                                          _scene_object_type object_type,
                                                          void*              instance)
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

            scene_light_set_property( (scene_light) instance,
                                      SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE,
                                     &node_ptr);

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
PUBLIC EMERALD_API void scene_graph_compute(scene_graph graph,
                                            system_time time)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    /* Sanity check */
    system_thread_id cs_owner_thread_id = 0;

    system_critical_section_get_property(graph_ptr->compute_lock_cs,
                                         SYSTEM_CRITICAL_SECTION_PROPERTY_OWNER_THREAD_ID,
                                        &cs_owner_thread_id);

    ASSERT_DEBUG_SYNC(cs_owner_thread_id == system_threads_get_thread_id(),
                      "Graph not locked");

    /* Reset tagged nodes */
    memset(graph_ptr->node_by_tag,
           0,
           sizeof(graph_ptr->node_by_tag) );

    /* Retrieve nodes in topological order */
    bool getter_result = _scene_graph_update_sorted_nodes(graph_ptr);

    if (!getter_result)
    {
        LOG_FATAL("Could not retrieve topologically sorted node values");

        ASSERT_DEBUG_SYNC(false,
                          "");

        goto end;
    }

    /* Iterate through all nodes and:
     *
     * 1) calculate transformation matrices
     * 2) Update tagged nodes.
     **/
    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        unsigned int n_sorted_nodes = 0;

        system_resizable_vector_get_property(graph_ptr->sorted_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_sorted_nodes);

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

                ASSERT_DEBUG_SYNC(false,
                                  "");

                goto end_unlock_sorted_nodes;
            }

            /* Update graph's tagged notes */

            /* TODO: This really should be a static assert */
            ASSERT_DEBUG_SYNC(SCENE_GRAPH_NODE_TAG_COUNT == SCENE_GRAPH_NODE_TAG_UNDEFINED,
                              "");

            if (node_ptr->tag < SCENE_GRAPH_NODE_TAG_COUNT)
            {
                graph_ptr->node_by_tag[node_ptr->tag] = (scene_graph_node) node_ptr;
            }

            /* Assign the tagged nodes to the node */
            
            /* TODO: This really should be a static assert */
            ASSERT_DEBUG_SYNC(sizeof(node_ptr->transformation_nodes_by_tag) == sizeof(graph_ptr->node_by_tag),
                              "Size mismatch");

            memcpy(node_ptr->transformation_nodes_by_tag,
                   graph_ptr->node_by_tag,
                   sizeof(node_ptr->transformation_nodes_by_tag) );

            /* Update transformation matrix */
            if (node_ptr->last_update_time != time)
            {
                _scene_graph_compute_node_transformation_matrix(graph,
                                                                node_ptr,
                                                                time);
            }

            ASSERT_DEBUG_SYNC(node_ptr->transformation_matrix.data != NULL,
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
PUBLIC EMERALD_API void scene_graph_compute_node(scene_graph      graph,
                                                 scene_graph_node node,
                                                 system_time      time)
{
    _scene_graph_node* current_node_ptr = (_scene_graph_node*) node;
    _scene_graph*      graph_ptr        = (_scene_graph*)      graph;
    int                n_cached_nodes   = 0;
    _scene_graph_node* node_ptr         = (_scene_graph_node*) node;

    /* Sanity check */
    system_thread_id cs_owner_thread_id = 0;

    system_critical_section_get_property(graph_ptr->compute_lock_cs,
                                         SYSTEM_CRITICAL_SECTION_PROPERTY_OWNER_THREAD_ID,
                                        &cs_owner_thread_id);

    ASSERT_DEBUG_SYNC(cs_owner_thread_id == system_threads_get_thread_id(),
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
    system_critical_section_enter(graph_ptr->node_compute_cs);

    if (graph_ptr->node_compute_vector == NULL)
    {
        graph_ptr->node_compute_vector = system_resizable_vector_create(4 /* capacity */);

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
    system_resizable_vector_get_property(graph_ptr->node_compute_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_cached_nodes);

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
            _scene_graph_compute_node_transformation_matrix(graph,
                                                            current_node_ptr,
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
PUBLIC EMERALD_API scene_graph scene_graph_create(scene                     owner_scene,
                                                  system_hashed_ansi_string object_manager_path)
{
    _scene_graph* new_graph = new (std::nothrow) _scene_graph;

    if (new_graph == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    ASSERT_ALWAYS_SYNC(owner_scene != NULL,
                       "Owner scene is NULL");

    /* Initialize the descriptor. */
    new_graph->dag                   = system_dag_create             ();
    new_graph->nodes                 = system_resizable_vector_create(4 /* capacity */);
    new_graph->object_manager_path   = object_manager_path;
    new_graph->owner_scene           = owner_scene;
    new_graph->sorted_nodes          = system_resizable_vector_create(4 /* capacity */);
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

    system_resizable_vector_push(new_graph->nodes,
                                 new_graph->root_node_ptr);

    /* Set up root node */
    new_graph->root_node_ptr->dag_node      = system_dag_add_node(new_graph->dag,
                                                                  new_graph->root_node_ptr);
    new_graph->root_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_ROOT;
    new_graph->root_node_ptr->pUpdateMatrix = _scene_graph_compute_root_node;

    /* Associate the instance with the owner scene */
    scene_set_graph(owner_scene,
                    (scene_graph) new_graph);

end:
    return (scene_graph) new_graph;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_rotation_dynamic_node(scene_graph          graph,
                                                                             curve_container*     rotation_vector_curves,
                                                                             bool                 expressed_in_radians,
                                                                             scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr    = (_scene_graph*) graph;
    _scene_graph_node* new_node_ptr = new (std::nothrow) _scene_graph_node(NULL);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_rotation_dynamic(rotation_vector_curves,
                                                                                        tag,
                                                                                        expressed_in_radians);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag, new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_ROTATION_DYNAMIC;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_rotation_dynamic;

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_scale_dynamic_node(scene_graph          graph,
                                                                          curve_container*     scale_vector_curves,
                                                                          scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr    = (_scene_graph*) graph;
    _scene_graph_node* new_node_ptr = new (std::nothrow) _scene_graph_node(NULL);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_scale_dynamic(scale_vector_curves,
                                                                                     tag);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag,
                                                      new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_scale_dynamic;

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_translation_dynamic_node(scene_graph          graph,
                                                                                curve_container*     translation_vector_curves,
                                                                                const bool*          negate_xyz_vectors,
                                                                                scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr    = (_scene_graph*) graph;
    _scene_graph_node* new_node_ptr = new (std::nothrow) _scene_graph_node(NULL);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_translation_dynamic(translation_vector_curves,
                                                                                           tag,
                                                                                           negate_xyz_vectors);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag,
                                                      new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_translation_dynamic;

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_translation_static_node(scene_graph          graph,
                                                                               float*               translation_vector,
                                                                               scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr    = (_scene_graph*) graph;
    _scene_graph_node* new_node_ptr = new (std::nothrow) _scene_graph_node(NULL);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_translation_static(translation_vector,
                                                                                          tag);
    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag,
                                                      new_node_ptr);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_TRANSLATION_STATIC;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_translation_static;

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_general_node(scene_graph graph)
{
    _scene_graph*      graph_ptr    = (_scene_graph*) graph;
    _scene_graph_node* new_node_ptr = new (std::nothrow) _scene_graph_node(NULL);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    new_node_ptr->dag_node      = system_dag_add_node(graph_ptr->dag,
                                                      new_node_ptr);
    new_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_GENERAL;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_general;

end:
    return (scene_graph_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_create_static_matrix4x4_transformation_node(scene_graph          graph,
                                                                                            system_matrix4x4     matrix,
                                                                                            scene_graph_node_tag tag)
{
    _scene_graph*      graph_ptr    = (_scene_graph*) graph;
    _scene_graph_node* new_node_ptr = new (std::nothrow) _scene_graph_node(NULL);

    if (new_node_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    new_node_ptr->data          = new (std::nothrow) _scene_graph_node_matrix4x4_static(matrix,
                                                                                        tag);
    new_node_ptr->tag           = tag;
    new_node_ptr->type          = SCENE_GRAPH_NODE_TYPE_STATIC_MATRIX4X4;
    new_node_ptr->pUpdateMatrix = _scene_graph_compute_static_matrix4x4;

    /* NOTE: There's one use case (ogl_flyby wrapped in a scene_graph_node) where we need
     *       transformation_matrix.data to be != NULL.
     *
     *       This matrix will be released back to the pool upon the first graph traversal.
     */
    new_node_ptr->transformation_matrix.data = system_matrix4x4_create();

    if (graph_ptr != NULL)
    {
        new_node_ptr->dag_node = system_dag_add_node(graph_ptr->dag,
                                                     new_node_ptr);
    }
    else
    {
        new_node_ptr->dag_node = NULL;
    }

end:
    return (scene_graph_node) new_node_ptr;
}


/** Please see header for specification */
PUBLIC EMERALD_API scene_graph_node scene_graph_get_node_for_object(scene_graph        graph,
                                                                    _scene_object_type object_type,
                                                                    void*              object)
{
    _scene_graph*    graph_ptr      = (_scene_graph*) graph;
    unsigned int     n_nodes        = 0;
    unsigned int     n_sorted_nodes = 0;
    scene_graph_node result         = NULL;

    system_resizable_vector_get_property(graph_ptr->sorted_nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_sorted_nodes);

    if (n_sorted_nodes == 0)
    {
        if (!_scene_graph_update_sorted_nodes(graph_ptr))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Sorted nodes update failed");

            goto end;
        }
    }

    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);

    system_resizable_vector_get_property(graph_ptr->sorted_nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

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

        system_resizable_vector_get_property(object_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_objects);

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
PUBLIC EMERALD_API scene_graph_node scene_graph_get_root_node(scene_graph graph)
{
    return (scene_graph_node) ((_scene_graph*) graph)->root_node_ptr;
}

/* Please see header for specification */
PUBLIC scene_graph scene_graph_load(scene                     owner_scene,
                                    system_file_serializer    serializer,
                                    system_resizable_vector   serialized_scene_cameras,
                                    system_resizable_vector   serialized_scene_lights,
                                    system_resizable_vector   serialized_scene_mesh_instances,
                                    system_hashed_ansi_string object_manager_path)
{
    scene_graph result = scene_graph_create(owner_scene,
                                            object_manager_path);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not create a scene graph instance");

    /* Serialize all nodes. The _save() function stores the nodes in a sorted order,
     * so we should be just fine to add the nodes in the order defined in the file.
     **/
    unsigned int            n_nodes          = 0;
    system_resizable_vector serialized_nodes = system_resizable_vector_create(4 /* capacity */);

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
                                    serialized_scene_mesh_instances,
                                    owner_scene) )
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
PUBLIC EMERALD_API void scene_graph_lock(scene_graph graph)
{
    system_critical_section_enter( ((_scene_graph*) graph)->compute_lock_cs);
}

#ifdef _DEBUG
    /** TODO */
    PRIVATE system_resizable_vector _scene_graph_get_children(_scene_graph*    graph_ptr,
                                                              scene_graph_node node)
    {
        uint32_t                n_nodes = 0;
        system_resizable_vector result  = system_resizable_vector_create(4 /* capacity */);

        system_resizable_vector_get_property(graph_ptr->nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_nodes);

        for (uint32_t n_node = 0;
                      n_node < n_nodes;
                    ++n_node)
        {
            _scene_graph_node* child_node_ptr = NULL;

            system_resizable_vector_get_element_at(graph_ptr->nodes,
                                                   n_node,
                                                  &child_node_ptr);

            if (child_node_ptr->parent_node == (_scene_graph_node*) node)
            {
                system_resizable_vector_push(result,
                                             child_node_ptr);
            }
        } /* for (all graph nodes) */

        return result;
    }

    /** TODO */
    PRIVATE const char* _scene_graph_get_node_type_string(scene_graph_node_type type)
    {
        const char* result = "[?]";

        switch (type)
        {
            case SCENE_GRAPH_NODE_TYPE_ROOT:
            {
                result = "Root";

                break;
            }

            case SCENE_GRAPH_NODE_TYPE_GENERAL:
            {
                result = "General";

                break;
            }

            case SCENE_GRAPH_NODE_TYPE_ROTATION_DYNAMIC:
            {
                result = "Rotation [dynamic]";

                break;
            }

            case SCENE_GRAPH_NODE_TYPE_SCALE_DYNAMIC:
            {
                result = "Scale [dynamic]";

                break;
            }

            case SCENE_GRAPH_NODE_TYPE_STATIC_MATRIX4X4:
            {
                result = "Matrix4x4 [static]";

                break;
            }

            case SCENE_GRAPH_NODE_TYPE_TRANSLATION_DYNAMIC:
            {
                result = "Translation [dynamic]";

                break;
            }
        } /* switch (type) */

        return result;
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API void scene_graph_log_hierarchy(scene_graph      graph,
                                                      scene_graph_node node,
                                                      unsigned int     indent_level,
                                                      system_time      time)
    {
        _scene_graph*           graph_ptr   = (_scene_graph*) graph;
        _scene_graph_node*      node_ptr    = (_scene_graph_node*) node;

        system_resizable_vector child_nodes   = _scene_graph_get_children(graph_ptr,
                                                                          node);
        uint32_t                n_child_nodes = 0;
        char                    tmp[1024];

        system_resizable_vector_get_property(child_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_child_nodes);

        memset(tmp,
               ' ',
               sizeof(tmp) );

        snprintf(tmp + indent_level,
                 sizeof(tmp) - indent_level,
                 "[%s]",
                 _scene_graph_get_node_type_string(node_ptr->type)
                 );

        LOG_INFO("%s", tmp);

        /* Any attached objects ? */
        uint32_t n_attached_cameras = 0;
        uint32_t n_attached_lights  = 0;
        uint32_t n_attached_meshes  = 0;

        system_resizable_vector_get_property(node_ptr->attached_cameras,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_attached_cameras);
        system_resizable_vector_get_property(node_ptr->attached_lights,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_attached_lights);
        system_resizable_vector_get_property(node_ptr->attached_meshes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_attached_meshes);

        for (uint32_t n_attached_camera = 0;
                      n_attached_camera < n_attached_cameras;
                    ++n_attached_camera)
        {
            scene_camera              camera      = NULL;
            system_hashed_ansi_string camera_name = NULL;

            system_resizable_vector_get_element_at(node_ptr->attached_cameras,
                                                   n_attached_camera,
                                                  &camera);

            scene_camera_get_property(camera,
                                      SCENE_CAMERA_PROPERTY_NAME,
                                      time,
                                     &camera_name);

            memset(tmp,
                   ' ',
                   sizeof(tmp) );

            snprintf(tmp + indent_level,
                     sizeof(tmp) - indent_level,
                     "+ Camera:[%s]",
                     system_hashed_ansi_string_get_buffer(camera_name) );

            LOG_INFO("%s",
                     tmp);
        } /* for (all attached cameras) */

        for (uint32_t n_attached_light = 0;
                      n_attached_light < n_attached_lights;
                    ++n_attached_light)
        {
            scene_light               light      = NULL;
            system_hashed_ansi_string light_name = NULL;

            system_resizable_vector_get_element_at(node_ptr->attached_lights,
                                                   n_attached_light,
                                                  &light);

            scene_light_get_property(light,
                                     SCENE_LIGHT_PROPERTY_NAME,
                                    &light_name);

            memset(tmp,
                   ' ',
                   sizeof(tmp) );

            snprintf(tmp + indent_level,
                     sizeof(tmp) - indent_level,
                     "+ Light:[%s]",
                     system_hashed_ansi_string_get_buffer(light_name) );

            LOG_INFO("%s",
                     tmp);
        } /* for (all attached lights) */

        for (uint32_t n_attached_mesh = 0;
                      n_attached_mesh < n_attached_meshes;
                    ++n_attached_mesh)
        {
            scene_mesh                mesh      = NULL;
            system_hashed_ansi_string mesh_name = NULL;

            system_resizable_vector_get_element_at(node_ptr->attached_meshes,
                                                   n_attached_mesh,
                                                  &mesh);

            scene_mesh_get_property(mesh,
                                    SCENE_MESH_PROPERTY_NAME,
                                   &mesh_name);

            memset(tmp,
                   ' ',
                   sizeof(tmp) );

            snprintf(tmp + indent_level,
                     sizeof(tmp) - indent_level,
                     "+ Mesh:[%s]",
                     system_hashed_ansi_string_get_buffer(mesh_name) );

            LOG_INFO("%s",
                     tmp);
        } /* for (all attached meshes) */

        /* Iterate over children */
        for (uint32_t n_child_node = 0;
                      n_child_node < n_child_nodes;
                    ++n_child_node)
        {
            _scene_graph_node* child_node_ptr = NULL;

            system_resizable_vector_get_element_at(child_nodes,
                                                   n_child_node,
                                                  &child_node_ptr);

            /* Process children */
            scene_graph_log_hierarchy(graph,
                                      (scene_graph_node) child_node_ptr,
                                      indent_level + 1,
                                      time);
        } /* for (all child nodes) */

        /* Done */
        system_resizable_vector_release(child_nodes);

        child_nodes = NULL;
    }

#endif

/* Please see header for specification */
PUBLIC EMERALD_API void scene_graph_node_get_property(scene_graph_node          node,
                                                      scene_graph_node_property property,
                                                      void*                     out_result)
{
    _scene_graph_node* node_ptr = (_scene_graph_node*) node;

    switch (property)
    {
        case SCENE_GRAPH_NODE_PROPERTY_PARENT_NODE:
        {
            *(scene_graph_node*) out_result = (scene_graph_node) node_ptr->parent_node;

            break;
        }

        case SCENE_GRAPH_NODE_PROPERTY_TAG:
        {
            *(scene_graph_node_tag*) out_result = node_ptr->tag;

            break;
        }

        case SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX:
        {
            *(system_matrix4x4*) out_result = node_ptr->transformation_matrix.data;

            break;
        }

        case SCENE_GRAPH_NODE_PROPERTY_TYPE:
        {
            *(scene_graph_node_type*) out_result = node_ptr->type;

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
PUBLIC EMERALD_API bool scene_graph_node_get_transformation_node(scene_graph_node     node,
                                                                 scene_graph_node_tag tag,
                                                                 scene_graph_node*    out_result_node)
{
    _scene_graph_node* node_ptr = (_scene_graph_node*) node;
    bool               result   = true;

    ASSERT_DEBUG_SYNC(tag < SCENE_GRAPH_NODE_TAG_COUNT,
                      "Invalid tag");
    ASSERT_DEBUG_SYNC(out_result_node != NULL,
                      "Output pointer is NULL");

    if (tag < SCENE_GRAPH_NODE_TAG_COUNT)
    {
        ASSERT_DEBUG_SYNC(node_ptr->transformation_nodes_by_tag[tag] != NULL,
                          "Requested transformation node is NULL");

        *out_result_node = node_ptr->transformation_nodes_by_tag[tag];
    } /* if (tag < SCENE_GRAPH_NODE_TAG_COUNT) */
    else
    {
        result = false;
    }

    return result;
}

/* Please see header for specification */
PUBLIC void scene_graph_node_release(scene_graph_node node)
{
    _scene_graph_node* node_ptr = (_scene_graph_node*) node;

    delete node_ptr;
    node_ptr = NULL;
}

/* Please see header for specification */
PUBLIC EMERALD_API void scene_graph_node_replace(scene_graph      graph,
                                                 scene_graph_node dst_node,
                                                 scene_graph_node src_node)
{
    _scene_graph*      graph_ptr    = (_scene_graph*)      graph;
    _scene_graph_node* dst_node_ptr = (_scene_graph_node*) dst_node;
    _scene_graph_node* src_node_ptr = (_scene_graph_node*) src_node;

    /* This function makes several assumptions in order to avoid a deep destruction of the
     * node.
     */
#ifdef _DEBUG
    unsigned int n_dst_cameras = 0;
    unsigned int n_dst_lights  = 0;
    unsigned int n_dst_meshes  = 0;
    unsigned int n_src_cameras = 0;
    unsigned int n_src_lights  = 0;
    unsigned int n_src_meshes  = 0;

    system_resizable_vector_get_property(dst_node_ptr->attached_cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_dst_cameras);
    system_resizable_vector_get_property(dst_node_ptr->attached_lights,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_dst_lights);
    system_resizable_vector_get_property(dst_node_ptr->attached_meshes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_dst_meshes);
    system_resizable_vector_get_property(src_node_ptr->attached_cameras,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_src_cameras);
    system_resizable_vector_get_property(src_node_ptr->attached_lights,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_src_lights);
    system_resizable_vector_get_property(src_node_ptr->attached_meshes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_src_meshes);

    ASSERT_DEBUG_SYNC(n_dst_cameras == 0 &&
                      n_dst_lights  == 0 &&
                      n_dst_meshes  == 0 &&
                      n_src_cameras == 0 &&
                      n_src_lights  == 0 &&
                      n_src_meshes  == 0,
                      "scene_graph_replace_node() does not support replacement for nodes with attach objects.");
#endif

    if (dst_node_ptr->data != NULL)
    {
        _scene_graph_node_release_data(dst_node_ptr->data,
                                       dst_node_ptr->type);
    }

    /* Copy data from the source node */
    dst_node_ptr->data = src_node_ptr->data;
    src_node_ptr->data = NULL;

    dst_node_ptr->pUpdateMatrix = src_node_ptr->pUpdateMatrix;
    dst_node_ptr->type          = src_node_ptr->type;

    /* Release the source node */
    delete src_node_ptr;
    src_node_ptr = NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_graph_release(scene_graph graph)
{
    _scene_graph* graph_ptr = (_scene_graph*) graph;

    if (graph_ptr->compute_lock_cs != NULL)
    {
        system_critical_section_release(graph_ptr->compute_lock_cs);

        graph_ptr->compute_lock_cs = NULL;
    }

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

    if (graph_ptr->node_compute_cs != NULL)
    {
        system_critical_section_release(graph_ptr->node_compute_cs);

        graph_ptr->node_compute_cs = NULL;
    }

    if (graph_ptr->node_compute_vector != NULL)
    {
        system_resizable_vector_release(graph_ptr->node_compute_vector);

        graph_ptr->node_compute_vector = NULL;
    }

    if (graph_ptr->sorted_nodes != NULL)
    {
        system_resizable_vector_release(graph_ptr->sorted_nodes);

        graph_ptr->sorted_nodes = NULL;
    }

    if (graph_ptr->sorted_nodes_rw_mutex != NULL)
    {
        system_read_write_mutex_release(graph_ptr->sorted_nodes_rw_mutex);

        graph_ptr->sorted_nodes_rw_mutex = NULL;
    }

    /* Root node is not deallocated since it's a part of nodes container */
    delete graph_ptr;
}

/* Please see header for specification */
PUBLIC bool scene_graph_save(system_file_serializer serializer,
                             scene_graph            graph,
                             system_hash64map       camera_ptr_to_id_map,
                             system_hash64map       light_ptr_to_id_map,
                             system_hash64map       mesh_instance_ptr_to_id_map,
                             scene                  owner_scene)
{
    _scene_graph*    graph_ptr    = (_scene_graph*) graph;
    system_hash64map node_hashmap = NULL;
    bool             result       = true;

    if (graph_ptr->dirty)
    {
        result &=_scene_graph_update_sorted_nodes(graph_ptr);

        ASSERT_DEBUG_SYNC(result,
                          "Could not update sorted nodes");
    }

    system_read_write_mutex_lock(graph_ptr->sorted_nodes_rw_mutex,
                                 ACCESS_READ);
    {
        unsigned int n_nodes = 0;

        system_resizable_vector_get_property(graph_ptr->sorted_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_nodes);

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
                                             mesh_instance_ptr_to_id_map,
                                             owner_scene);
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
PUBLIC EMERALD_API void scene_graph_traverse(scene_graph                    graph,
                                             PFNNEWTRANSFORMATIONMATRIXPROC on_new_transformation_matrix_proc,
                                             PFNINSERTCAMERAPROC            insert_camera_proc,
                                             PFNINSERTLIGHTPROC             insert_light_proc,
                                             PFNINSERTMESHPROC              insert_mesh_proc,
                                             void*                          user_arg,
                                             system_time                    frame_time)
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
        unsigned int n_sorted_nodes = 0;

        system_resizable_vector_get_property(graph_ptr->sorted_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_sorted_nodes);

        for (unsigned int n_sorted_node = 0;
                          n_sorted_node < n_sorted_nodes;
                        ++n_sorted_node)
        {
            _scene_graph_node* node_ptr = NULL;

            if (!system_resizable_vector_get_element_at(graph_ptr->sorted_nodes,
                                                        n_sorted_node,
                                                       &node_ptr) )
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not retrieve sorted node at index [%d]",
                                   n_sorted_node);

                goto end;
            }

            node_ptr->transformation_matrix.has_fired_event = false;
        } /* for (all sorted nodes) */

        /* Iterate through the sorted nodes. Fire the 'on new transformation matrix' only for the first object
         * associated with the node, and then continue with object-specific events.
         **/
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
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not retrieve sorted node at index [%d]",
                                   n_sorted_node);

                goto end;
            }

            /* Update the matrix */
            if (node_ptr->last_update_time != frame_time)
            {
                _scene_graph_compute_node_transformation_matrix(graph,
                                                                node_ptr,
                                                                frame_time);
            }

            /* Any cameras / meshes attached? */
            n_attached_cameras = 0;
            n_attached_lights  = 0;
            n_attached_meshes  = 0;

            system_resizable_vector_get_property(node_ptr->attached_cameras,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_attached_cameras);
            system_resizable_vector_get_property(node_ptr->attached_lights,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_attached_lights);
            system_resizable_vector_get_property(node_ptr->attached_meshes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_attached_meshes);

            if (n_attached_cameras != 0 ||
                n_attached_lights  != 0 ||
                n_attached_meshes  != 0)
            {
                /* Ah-hah. Has this transformation matrix been already fired? */
                if (!node_ptr->transformation_matrix.has_fired_event)
                {
                    /* Nope, issue a notification */
                    if (on_new_transformation_matrix_proc != NULL)
                    {
                        on_new_transformation_matrix_proc(node_ptr->transformation_matrix.data,
                                                          user_arg);
                    }

                    node_ptr->transformation_matrix.has_fired_event = true;
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

                        for (unsigned int n_object = 0;
                                           n_object < n_objects;
                                         ++n_object)
                        {
                            void* object = NULL;

                            if (system_resizable_vector_get_element_at(objects,
                                                                       n_object,
                                                                      &object) )
                            {
                                if (n_iteration        == 0 &&
                                    insert_camera_proc != NULL)
                                {
                                    insert_camera_proc( (scene_camera) object,
                                                        user_arg);
                                }
                                else
                                if (n_iteration      == 1 &&
                                    insert_mesh_proc != NULL)
                                {
                                    insert_mesh_proc( (scene_mesh) object,
                                                      user_arg);
                                }
                                else
                                if (n_iteration       == 2 &&
                                    insert_light_proc != NULL)
                                {
                                    insert_light_proc( (scene_light) object,
                                                       user_arg);
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
PUBLIC EMERALD_API void scene_graph_unlock(scene_graph graph)
{
    system_critical_section_leave( ((_scene_graph*) graph)->compute_lock_cs);
}