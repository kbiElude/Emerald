#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_common.h"
#include "plugin_curves.h"
#include "plugin_materials.h"
#include "plugin_meshes.h"
#include "plugin_ui.h"
#include "plugin_vmaps.h"

#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "scene/scene.h"
#include "scene/scene_material.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_thread_pool.h"
#include "system/system_variant.h"
#include <sstream>

typedef struct _mesh_instance
{
    system_hashed_ansi_string filename;
    bool                      is_shadow_caster;
    bool                      is_shadow_receiver;
    LWItemID                  lw_item_id;
    mesh                      mesh_gpu;
    _mesh_instance*           mesh_gpu_provider_ptr;
    system_hashed_ansi_string name;
    void*                     object_id;
    LWItemID                  parent_item_id;
    _mesh_instance*           parent_mesh_ptr;
    float                     pivot      [3];
    curve_container           rotation   [3]; /* H, P, B */
    curve_container           translation[3];

    unsigned int n_points;
    unsigned int n_polygons;

    system_hash64map material_to_polygon_instance_vector_map;

    explicit _mesh_instance()
    {
        filename              = NULL;
        is_shadow_caster      = false;
        is_shadow_receiver    = false;
        lw_item_id            = 0;
        mesh_gpu              = NULL;
        mesh_gpu_provider_ptr = NULL;
        name                  = NULL;
        n_points              = 0;
        n_polygons            = 0;
        object_id             = NULL;
        parent_item_id        = NULL;
        parent_mesh_ptr       = NULL;

        material_to_polygon_instance_vector_map = system_hash64map_create(sizeof(system_resizable_vector) );

        memset(pivot,
               0,
               sizeof(pivot) );
        memset(rotation,
               0,
               sizeof(rotation) );
        memset(translation,
               0,
               sizeof(translation) );
    }

    ~_mesh_instance()
    {
        if (material_to_polygon_instance_vector_map != NULL)
        {
            const uint32_t n_vectors = system_hash64map_get_amount_of_elements(material_to_polygon_instance_vector_map);

            for (uint32_t n_vector = 0;
                          n_vector < n_vectors;
                        ++n_vector)
            {
                system_resizable_vector current_vector = NULL;

                if (!system_hash64map_get_element_at(material_to_polygon_instance_vector_map,
                                                     n_vector,
                                                    &current_vector,
                                                     NULL) ) /* outHash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve [%d]-th polygon instance vector.",
                                      n_vector);

                    continue;
                }

                system_resizable_vector_release(current_vector);
                current_vector = NULL;
            } /* for (all stored vectors) */

            system_hash64map_release(material_to_polygon_instance_vector_map);

            material_to_polygon_instance_vector_map = NULL;
        } /* if (material_to_polygon_instance_vector_map != NULL) */
    }
} _mesh_instance;

typedef struct _bake_mesh_gl_blob_worker_arg
{
    _mesh_instance* instance_ptr;
    mesh            new_mesh;
    scene           mesh_scene;

    explicit _bake_mesh_gl_blob_worker_arg(__in __notnull _mesh_instance* in_instance_ptr,
                                           __in __notnull mesh            in_new_mesh,
                                           __in __notnull scene           in_scene)
    {
        instance_ptr = in_instance_ptr;
        new_mesh     = in_new_mesh;
        mesh_scene   = in_scene;
    }
} _bake_mesh_gl_blob_worker_arg;

typedef struct _polygon_instance
{
    scene_material material;
    float          uv_data    [2*3];
    bool           uv_data_defined;
    float          vertex_data[3*3];

    _polygon_instance()
    {
        material        = NULL;
        uv_data_defined = false;

        memset(uv_data,
               0,
               sizeof(uv_data) );
        memset(vertex_data,
               0,
               sizeof(vertex_data) );
    }
} _polygon_instance;

/* Forward declarations */
PRIVATE  void  AddMeshToScene                       (__in __notnull  scene                       in_scene,
                                                     __in __notnull  mesh                        new_mesh,
                                                     __in __notnull  _mesh_instance*             instance_ptr);
volatile void  BakeMeshGLBlobWorkerThreadEntryPoint (__in __notnull  void*                       arg);
PRIVATE  void  ExtractMeshData                      (__in __notnull  _mesh_instance*             instance_ptr,
                                                     __in            int                         lw_object_id);
volatile void  ExtractMeshDataWorkerThreadEntryPoint(__in __notnull  void*                       n_object_raw);
PRIVATE  void  ExtractPointData                     (__in            LWPolID                     polygon_id,
                                                     __in            LWPntID                     point_id,
                                                     __in            LWMeshInfoID                layer_mesh_info_id,
                                                     __in  __notnull LWMeshInfo*                 layer_mesh_info_ptr,
                                                     __out __notnull float*                      out_point_vertex_data,
                                                     __out __notnull float*                      out_point_uv_data,
                                                     __out __notnull bool*                       out_uv_data_extracted);
PRIVATE  void* GenerateDataStreamData               (__in  __notnull system_resizable_vector     polygon_instance_vector,
                                                     __in            mesh_layer_data_stream_type stream_type);

/* Local variables */
PRIVATE system_hash64map        filename_to_mesh_instance_map      = NULL;
PRIVATE system_hash64map        item_id_to_mesh_instance_map       = NULL;
PRIVATE system_hash64map        mesh_instance_to_scene_mesh_map    = NULL;
PRIVATE system_hash64map        polygon_id_to_polygon_instance_map = system_hash64map_create(sizeof(_polygon_instance*),
                                                                                             true);
PRIVATE system_resource_pool    polygon_instance_resource_pool     = NULL;
PRIVATE system_resizable_vector objects                            = NULL;
PRIVATE system_hash64map        scene_mesh_to_mesh_instance_map    = NULL;

system_critical_section global_cs = system_critical_section_create();

/** TODO */
PRIVATE void AddMeshToScene(__in __notnull scene           in_scene,
                            __in __notnull mesh            new_mesh,
                            __in __notnull _mesh_instance* instance_ptr)
{
    /* Store the mesh */
    scene_mesh new_scene_mesh = NULL;

    scene_add_mesh_instance(in_scene,
                            new_mesh,
                            instance_ptr->name);

    new_scene_mesh = scene_get_mesh_instance_by_name(in_scene,
                                                     instance_ptr->name);
    ASSERT_DEBUG_SYNC(new_scene_mesh != NULL,
                      "Could not retrieve a scene_mesh instance by name");

    system_hash64map_insert(scene_mesh_to_mesh_instance_map,
                            (system_hash64) new_scene_mesh,
                            instance_ptr,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */
    system_hash64map_insert(mesh_instance_to_scene_mesh_map,
                            (system_hash64) instance_ptr,
                            new_scene_mesh,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */
}

/** TODO */
volatile void BakeMeshGLBlobWorkerThreadEntryPoint(__in __notnull void* arg)
{
    _bake_mesh_gl_blob_worker_arg* arg_ptr     = (_bake_mesh_gl_blob_worker_arg*) arg;
    const uint32_t                 n_materials = system_hash64map_get_amount_of_elements(arg_ptr->instance_ptr->material_to_polygon_instance_vector_map);
    char                           text_buffer[1024];

    ASSERT_DEBUG_SYNC(n_materials != 0,
                      "No materials defined for a mesh instance");

    /* Each layer pass corresponds to the set of polygons that uses
     * a specific material */
    mesh_layer_id new_mesh_layer = mesh_add_layer(arg_ptr->new_mesh);

    for (uint32_t n_material = 0;
                  n_material < n_materials;
                ++n_material)
    {
        scene_material            current_material        = NULL;
        system_hash64             current_material_hash   = 0;
        system_hashed_ansi_string current_material_name   = NULL;
        uint32_t                  n_polygon_instances     = 0;
        system_resizable_vector   polygon_instance_vector = NULL;

        /* Carry on */
        if (!system_hash64map_get_element_at(arg_ptr->instance_ptr->material_to_polygon_instance_vector_map,
                                             n_material,
                                            &polygon_instance_vector,
                                            &current_material_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve polygon data for [%d]-th material.",
                              n_material);

            continue;
        }

        current_material    = (scene_material) current_material_hash;
        n_polygon_instances = system_resizable_vector_get_amount_of_elements(polygon_instance_vector);

        scene_material_get_property(current_material,
                                    SCENE_MATERIAL_PROPERTY_NAME,
                                   &current_material_name);

        /* Update status in the UI */
        sprintf_s(text_buffer,
                  sizeof(text_buffer),
                  "GL blob baking: Forming data streams for mesh [%s] material [%s] (%d/%d)",
                  system_hashed_ansi_string_get_buffer(arg_ptr->instance_ptr->name),
                  system_hashed_ansi_string_get_buffer(current_material_name),
                  n_material + 1,
                  n_materials);

        SetActivityDescription(text_buffer);

        /* NOTE: We do not provide index data here. The index data will have to be calculated for all
         *       streams we feed the object anyway.
         */
        mesh_layer_pass_id current_layer_pass = mesh_add_layer_pass(arg_ptr->new_mesh,
                                                                    new_mesh_layer,
                                                                    mesh_material_create_from_scene_material(current_material,
                                                                                                             NULL),
                                                                    n_polygon_instances * 3);

        /* Vertex data. */
        float* raw_vertex_data = (float*) GenerateDataStreamData(polygon_instance_vector,
                                                                 MESH_LAYER_DATA_STREAM_TYPE_VERTICES);

        mesh_add_layer_data_stream(arg_ptr->new_mesh,
                                   new_mesh_layer,
                                   MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                   n_polygon_instances * 3, /* vertices per triangle */
                                   raw_vertex_data);

        delete [] raw_vertex_data;
        raw_vertex_data = NULL;

        /* UV data */
        float* raw_uv_data = (float*) GenerateDataStreamData(polygon_instance_vector,
                                                             MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS);

        if (raw_uv_data != NULL)
        {
            mesh_add_layer_data_stream(arg_ptr->new_mesh,
                                       new_mesh_layer,
                                       MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                       n_polygon_instances * 2, /* components per UV */
                                       raw_uv_data);

            delete [] raw_uv_data;
            raw_uv_data = NULL;
        }
    } /* for (all materials) */

    /* Generate per-vertex normal data */
    sprintf_s(text_buffer,
              sizeof(text_buffer),
              "GL blob baking: Generating normal data for mesh [%s]..",
              system_hashed_ansi_string_get_buffer(arg_ptr->instance_ptr->name) );

    SetActivityDescription(text_buffer);

    mesh_generate_normal_data(arg_ptr->new_mesh);

    /* Bake the blob data */
    sprintf_s(text_buffer,
              sizeof(text_buffer),
              "GL blob baking: Baking for mesh [%s]..",
              system_hashed_ansi_string_get_buffer(arg_ptr->instance_ptr->name) );

    SetActivityDescription(text_buffer);

    mesh_create_single_indexed_representation(arg_ptr->new_mesh);

    /* Add the mesh to the scene */
    AddMeshToScene(arg_ptr->mesh_scene,
                   arg_ptr->new_mesh,
                   arg_ptr->instance_ptr);

    SetActivityDescription("Inactive");

    /* We're OK to delete the arg descriptor at this point */
    delete arg_ptr;

    arg_ptr = NULL;
}

/** TODO */
PRIVATE void ExtractMeshData(__in __notnull _mesh_instance*  instance_ptr,
                             __in           int              lw_object_id)
{
    /* NOTE: This used to implement a parallelized mesh data extraction but turned out to not to work correctly,
     *       owing to undocumented weird Lightwave's objections to parallelized API calls. Feel free to pick
     *       this up and parallelize this function in a way that will work correctly with LW. (TODO?)
     *
     *       For the time being, if you try to run this in multiple threads at once for different objects,
     *       LW will start reporting odd materials for polygons, which causes a crash in the later GL blob
     *       baking stage. However, when ran one mesh at a time, things work just fine.
     */
    system_timeline_time    last_update_time                    = 0;
    system_hash64map        lw_surface_id_to_scene_material_map = GetLWSurfaceIDToSceneMaterialMap();
    const uint32_t          n_lw_mesh_layers                    = object_funcs_ptr->maxLayers     (lw_object_id);
    system_resizable_vector layer_polygon_ids_vector            = system_resizable_vector_create  (64,
                                                                                                   sizeof(LWPolID) );

    /* Iterate over all LW mesh layers */
    bool layer_data_extracted = false;

    ASSERT_DEBUG_SYNC(n_lw_mesh_layers != 0,
                      "No layers defined for a base mesh?");

    for (uint32_t n_lw_mesh_layer = 0;
                  n_lw_mesh_layer < n_lw_mesh_layers;
                ++n_lw_mesh_layer)
    {
        void* active_vmap = NULL;

        LWMeshInfo*  layer_mesh_info_ptr = object_funcs_ptr->layerMesh(lw_object_id,
                                                                       n_lw_mesh_layer);
        LWMeshInfoID layer_mesh_info_id  = (LWMeshInfoID) layer_mesh_info_ptr;

        if (object_funcs_ptr->layerExists(lw_object_id,
                                          n_lw_mesh_layer) == 1)
        {
            system_resizable_vector_clear(layer_polygon_ids_vector);

            system_critical_section_enter(global_cs);
            {
                LWMeshIteratorID layer_mesh_iterator_id         = layer_mesh_info_ptr->createMeshIterator(layer_mesh_info_id,
                                                                                                          LWMESHITER_POLYGON);
                LWPolID          layer_mesh_iterator_polygon_id = NULL;

                while ( (layer_mesh_iterator_polygon_id = (LWPolID) layer_mesh_info_ptr->iterateMesh(layer_mesh_info_id,
                                                                                                     layer_mesh_iterator_id)) != NULL)
                {
                    system_resizable_vector_push(layer_polygon_ids_vector,
                                                 layer_mesh_iterator_polygon_id);
                }

                /* Safe to release the iterator */
                layer_mesh_info_ptr->destroyMeshIterator(layer_mesh_info_id,
                                                         layer_mesh_iterator_id);
            }
            system_critical_section_leave(global_cs);

            const unsigned int n_layer_points   = layer_mesh_info_ptr->numPoints  (layer_mesh_info_id);
            const unsigned int n_layer_polygons = layer_mesh_info_ptr->numPolygons(layer_mesh_info_id);

            ASSERT_DEBUG_SYNC(n_layer_polygons != 0,
                              "No polygons defined for a layer?");
            ASSERT_DEBUG_SYNC(n_layer_polygons == system_resizable_vector_get_amount_of_elements(layer_polygon_ids_vector),
                              "Something's wrong");

            /* Iterate over all polygons */
            unsigned int n_polygons_processed = 0;

            for (unsigned int n_polygon = 0;
                              n_polygon < n_layer_polygons;
                            ++n_polygon)
            {
                _polygon_instance* current_polygon_ptr = (_polygon_instance*) system_resource_pool_get_from_pool(polygon_instance_resource_pool);
                LWPolID            current_polygon_id  = NULL;

                system_resizable_vector_get_element_at(layer_polygon_ids_vector,
                                                       n_polygon,
                                                      &current_polygon_id);

                const unsigned int current_polygon_n_vertices   = layer_mesh_info_ptr->polSize(layer_mesh_info_id,
                                                                                               current_polygon_id);
                const char*        current_polygon_surface_name = layer_mesh_info_ptr->polTag (layer_mesh_info_id,
                                                                                               current_polygon_id,
                                                                                               LWPTAG_SURF);
                LWID               current_polygon_type         = layer_mesh_info_ptr->polType(layer_mesh_info_id,
                                                                                               current_polygon_id);

                layer_data_extracted = true;

                /* Update UI if it makes sense */
                system_timeline_time time_now = system_time_now();

                if (last_update_time            == 0                                           ||
                    time_now - last_update_time > system_time_get_timeline_time_for_msec(1000) )
                {
                    char text_buffer[1024];

                    memset(text_buffer,
                           0,
                           sizeof(text_buffer) );

                    sprintf_s(text_buffer,
                              sizeof(text_buffer),
                              "Extracting mesh data of [%s] (%d / %d): %d / %d polygons processed (layer %d/%d)",
                              system_hashed_ansi_string_get_buffer(instance_ptr->name),
                              lw_object_id + 1,
                              system_resizable_vector_get_amount_of_elements(objects),
                              n_polygons_processed,
                              n_layer_polygons,
                              n_lw_mesh_layer,
                              n_lw_mesh_layers
                             );

                    SetActivityDescription(text_buffer);
                    last_update_time = time_now;
                }

                /* Carry on with actual task */
                if (current_polygon_type != LWPOLTYPE_FACE)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unsupported polygon type");

                    continue;
                }

                /* Make sure this is a triangle */
                if (current_polygon_n_vertices != 3)
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Non-triangle faces are not supported");

                    continue;
                }

                /* Iterate over all points defined for the polygon */
                bool all_vertices_hold_uv_data = false;

                for (uint32_t current_polygon_n_vertex = 0;
                              current_polygon_n_vertex < current_polygon_n_vertices;
                            ++current_polygon_n_vertex)
                {
                    bool    current_point_defines_uv = false;
                    LWPntID current_point_id         = layer_mesh_info_ptr->polVertex(layer_mesh_info_id,
                                                                                      current_polygon_id,
                                                                                      current_polygon_n_vertex);

                    ExtractPointData(current_polygon_id,
                                     current_point_id,
                                     layer_mesh_info_id,
                                     layer_mesh_info_ptr,
                                     current_polygon_ptr->vertex_data + current_polygon_n_vertex * 3 /* xyz */,
                                     current_polygon_ptr->uv_data     + current_polygon_n_vertex * 2 /* st */,
                                    &current_point_defines_uv);

                    ASSERT_DEBUG_SYNC(!all_vertices_hold_uv_data ||
                                       all_vertices_hold_uv_data && current_point_defines_uv,
                                       "Not all vertices for a given polygon define UV coords?");
                } /* for (all current polygon's vertices) */

                current_polygon_ptr->uv_data_defined = all_vertices_hold_uv_data;

                /* Assign the surface to the polygon */
                if (current_polygon_surface_name == NULL)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Nameless surface assigned to a polygon?");

                    current_polygon_ptr->material = NULL;
                }
                else
                {
                    system_critical_section_enter(global_cs);
                    {
                        LWSurfaceID* object_polygon_surfaces = surface_funcs_ptr->byName(current_polygon_surface_name,
                                                                                         system_hashed_ansi_string_get_buffer(instance_ptr->filename) );

                        if (object_polygon_surfaces[0] == 0)
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "LW did not recognize a valid polygon surface name?");

                            current_polygon_ptr->material = NULL;
                        }
                        else
                        {
                            /* Look up the corresponding scene_material instance for LW-provided
                             * surface ID */
                            if (!system_hash64map_get(lw_surface_id_to_scene_material_map,
                                                      (system_hash64) object_polygon_surfaces[0],
                                                     &current_polygon_ptr->material) )
                            {
                                ASSERT_ALWAYS_SYNC(false,
                                                   "Could not retrieve a scene_material instance for LW-specified surface ID");
                            }
                        }
                    }
                    system_critical_section_leave(global_cs);
                }

                /* Store the polygon */
                system_resizable_vector material_polygon_vector = NULL;

                system_hash64map_insert(polygon_id_to_polygon_instance_map,
                                        (system_hash64) current_polygon_id,
                                        current_polygon_ptr,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */

                if (!system_hash64map_get(instance_ptr->material_to_polygon_instance_vector_map,
                                          (system_hash64) current_polygon_ptr->material,
                                         &material_polygon_vector) )
                {
                    material_polygon_vector = system_resizable_vector_create(64, /* capacity */
                                                                             sizeof(_polygon_instance*),
                                                                             true);

                    system_hash64map_insert(instance_ptr->material_to_polygon_instance_vector_map,
                                            (system_hash64) current_polygon_ptr->material,
                                            material_polygon_vector,
                                            NULL,  /* on_remove_callback */
                                            NULL); /* on_remove_callback_user_arg */
                }

                system_resizable_vector_push(material_polygon_vector,
                                             current_polygon_ptr);

                n_polygons_processed++;
            } /* while (polygon iterator works) */

            ASSERT_DEBUG_SYNC(n_polygons_processed == n_layer_polygons,
                              "Not all polygons were processed!");
        } /* if (LW mesh layer exists) */
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "LW mesh layer data does not exist?");
        }
    } /* for (all LW mesh layers) */

    if (layer_polygon_ids_vector != NULL)
    {
        system_resizable_vector_release(layer_polygon_ids_vector);

        layer_polygon_ids_vector = NULL;
    }

    ASSERT_DEBUG_SYNC(layer_data_extracted,
                  "No layer data found");
}

/** TODO */
volatile void ExtractMeshDataWorkerThreadEntryPoint(__in __notnull void* n_object_raw)
{
    unsigned int n_object        = (unsigned int) n_object_raw;
    const char*  object_filename = object_funcs_ptr->filename(n_object);

    if (object_filename == NULL)
    {
        goto end;
    }
    
    /* LW objects are identified by an index. Match current index
     * with a descriptor of a mesh instance that acts as a GPU data
     * source. (that is: will host a "mesh" instance)
     */
    system_hashed_ansi_string object_filename_has      = system_hashed_ansi_string_create(object_filename);
    _mesh_instance*           object_mesh_instance_ptr = NULL;

    ASSERT_DEBUG_SYNC(object_filename != NULL,
                      "Object filename is NULL");

    if (!system_hash64map_get(filename_to_mesh_instance_map,
                              (system_hash64) object_filename_has,
                             &object_mesh_instance_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve mesh descriptor for object filename [%s]",
                          object_filename);

        goto end;
    }

    /* Go on.. */
    ExtractMeshData(object_mesh_instance_ptr,
                    n_object);

end:
    SetActivityDescription("Inactive");
}

/** TODO */
PRIVATE void ExtractPointData(__in            LWPolID          polygon_id,
                              __in            LWPntID          point_id,
                              __in            LWMeshInfoID     layer_mesh_info_id,
                              __in  __notnull LWMeshInfo*      layer_mesh_info_ptr,
                              __out __notnull float*           out_point_vertex_data,
                              __out __notnull float*           out_point_uv_data,
                              __out __notnull bool*            out_uv_data_extracted)
{
    /* Extract vertex data.
     *
     * LW uses left-handed system, so convert Z coords to a right-handed system.
     */
    layer_mesh_info_ptr->pntBasePos(layer_mesh_info_id,
                                    point_id,
                                    out_point_vertex_data);

    /* Extract UV for the point */
    unsigned int n_uv_vmaps   = 0;
    bool         uv_extracted = false;

    GetGlobalVMapProperty(VMAP_PROPERTY_N_UV_VMAPS,
                         &n_uv_vmaps);

    for (unsigned int n_uv_vmap = 0;
                      n_uv_vmap < n_uv_vmaps;
                    ++n_uv_vmap)
    {
        system_hashed_ansi_string current_vmap_name = NULL;

        GetVMapProperty(VMAP_PROPERTY_NAME,
                        VMAP_TYPE_UVMAP,
                        n_uv_vmap,
                       &current_vmap_name);

        void* active_vmap_id = layer_mesh_info_ptr->pntVIDLookup(layer_mesh_info_id,
                                                                 LWVMAP_TXUV,
                                                                 system_hashed_ansi_string_get_buffer(current_vmap_name) );

        if (active_vmap_id != 0)
        {
            int result = layer_mesh_info_ptr->pntVPIDGet(layer_mesh_info_id,
                                                         point_id,
                                                         polygon_id,
                                                         out_point_uv_data,
                                                         active_vmap_id);

            if (!result)
            {
                result = layer_mesh_info_ptr->pntVIDGet(layer_mesh_info_id,
                                                        point_id,
                                                        out_point_uv_data,
                                                        active_vmap_id);
            }

            if (result)
            {
                /* UV found, bail out */
                uv_extracted = true;

                break;
            }
        } /* if (active_vmap_id != 0) */
    } /* for (all UV vmaps) */

    *out_uv_data_extracted = uv_extracted;
}

/** TODO */
PRIVATE void* GenerateDataStreamData(__in  __notnull system_resizable_vector     polygon_instance_vector,
                                     __in            mesh_layer_data_stream_type stream_type)
{
    const uint32_t n_polygon_instances = system_resizable_vector_get_amount_of_elements(polygon_instance_vector);
    float*         result_data         = NULL;

    switch (stream_type)
    {
        case MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS:
        {
            result_data = new (std::nothrow) float[3 /* vertices */ * 2 /* components */ * n_polygon_instances];

            break;
        }

        case MESH_LAYER_DATA_STREAM_TYPE_VERTICES:
        {
            result_data = new (std::nothrow) float[3 /* vertices */ * 3 /* components */ * n_polygon_instances];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh data stream type");

            goto end;
        }
    } /* switch (stream_type) */

    ASSERT_DEBUG_SYNC(result_data != NULL,
                      "Out of memory");
    if (result_data == NULL)
    {
        goto end;
    }

    /* Iterate over polygons and fill the buffer */
    bool   polygon_defines_uv_coords = false;
    float* result_data_traveller_ptr = result_data;

    for (uint32_t n_polygon = 0;
                  n_polygon < n_polygon_instances;
                ++n_polygon)
    {
        float*             data_ptr          = NULL;
        unsigned int       n_data_components = 0;
        _polygon_instance* polygon_ptr       = NULL;

        if (!system_resizable_vector_get_element_at(polygon_instance_vector,
                                                    n_polygon,
                                                   &polygon_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot retrieve polygon descriptor");

            delete [] result_data;
            result_data = NULL;

            goto end;
        }

        if (stream_type == MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS)
        {
            if (n_polygon == 0)
            {
                if (!polygon_ptr->uv_data_defined)
                {
                    /* No UV data defined - bail out. */
                    delete [] result_data;
                    result_data = NULL;

                    goto end;
                }
                else
                {
                    polygon_defines_uv_coords = true;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(polygon_ptr->uv_data_defined,
                                  "Not all polygons have their UV coords defined!");
            }
        } /* if (stream_type == MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS) */

        switch (stream_type)
        {
            case MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS:
            {
                data_ptr          = polygon_ptr->uv_data;
                n_data_components = 2;

                break;
            }

            case MESH_LAYER_DATA_STREAM_TYPE_VERTICES:
            {
                data_ptr          = polygon_ptr->vertex_data;
                n_data_components = 3;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized mesh data stream type");

                delete [] result_data;
                result_data = NULL;

                goto end;
            }
        } /* switch (stream_type) */

        memcpy(result_data_traveller_ptr,
               data_ptr,
               sizeof(float) * n_data_components * 3 /* veritces per polygon */);

        result_data_traveller_ptr += n_data_components * 3 /* vertices per polygon */;
    } /* for (all polygons) */

end:
    return result_data;
}


/** TODO */
PUBLIC void DeinitMeshData()
{
    /* Release all object descriptors */
    if (filename_to_mesh_instance_map != NULL)
    {
        system_hash64map_release(filename_to_mesh_instance_map);

        filename_to_mesh_instance_map = NULL;
    }

    if (item_id_to_mesh_instance_map != NULL)
    {
        system_hash64map_release(item_id_to_mesh_instance_map);

        item_id_to_mesh_instance_map = NULL;
    }

    if (mesh_instance_to_scene_mesh_map != NULL)
    {
        system_hash64map_release(mesh_instance_to_scene_mesh_map);

        mesh_instance_to_scene_mesh_map = NULL;
    }

    if (objects != NULL)
    {
        _mesh_instance* object_ptr = NULL;

        while (system_resizable_vector_pop(objects,
                                          &object_ptr) )
        {
            delete object_ptr;

            object_ptr = NULL;
        }
        system_resizable_vector_release(objects);
        objects = NULL;
    }

    if (polygon_id_to_polygon_instance_map != NULL)
    {
        system_hash64map_release(polygon_id_to_polygon_instance_map);

        polygon_id_to_polygon_instance_map = NULL;
    }

    if (polygon_instance_resource_pool != NULL)
    {
        system_resource_pool_release(polygon_instance_resource_pool);

        polygon_instance_resource_pool = NULL;
    }

    if (scene_mesh_to_mesh_instance_map != NULL)
    {
        system_hash64map_release(scene_mesh_to_mesh_instance_map);

        scene_mesh_to_mesh_instance_map = NULL;
    }
}

/** TODO */
PUBLIC void FillSceneWithMeshData(__in __notnull scene scene)
{
    system_hash64map        envelope_id_to_curve_container_map = GetEnvelopeIDToCurveContainerHashMap();
    system_resizable_vector job_done_events                    = NULL;

    /* Iterate over all objects */
    LWItemID object_id = item_info_ptr->first(LWI_OBJECT,
                                              LWITEM_NULL);

    /* Extract general mesh data first. This process is fast, so it doesn't really make much sense
     * to parallelize this process.
     */
    while (object_id != 0)
    {
        _mesh_instance* new_instance = new (std::nothrow) _mesh_instance;

        /* Extract general properties */
        const char* filename_raw = object_info_ptr->filename  (object_id);

        ASSERT_DEBUG_SYNC(filename_raw != NULL,
                          "LW reported a NULL file name?");

        new_instance->filename           = system_hashed_ansi_string_create(filename_raw);
        new_instance->is_shadow_caster   =                                 (object_info_ptr->shadowOpts(object_id) & LWOSHAD_CAST)    != 0;
        new_instance->is_shadow_receiver =                                 (object_info_ptr->shadowOpts(object_id) & LWOSHAD_RECEIVE) != 0;
        new_instance->lw_item_id         = object_id;
        new_instance->name               = system_hashed_ansi_string_create(item_info_ptr->name(object_id) );
        new_instance->n_points           = object_info_ptr->numPoints      (object_id);
        new_instance->n_polygons         = object_info_ptr->numPolygons    (object_id);

        /* If this is the first mesh instance that mentions of the specific file name,
         * store it in our nifty internal map */
        if (!system_hash64map_contains(filename_to_mesh_instance_map,
                                       (system_hash64) new_instance->filename) )
        {
            system_hash64map_insert(filename_to_mesh_instance_map,
                                    (system_hash64) new_instance->filename,
                                    new_instance,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
        }

        /* Extract transformation data */
        new_instance->rotation   [0] = GetCurveContainerForProperty    (new_instance->name,
                                                                        ITEM_PROPERTY_ROTATION_H,
                                                                        object_id,
                                                                        envelope_id_to_curve_container_map);
        new_instance->rotation   [1] = GetCurveContainerForProperty    (new_instance->name,
                                                                        ITEM_PROPERTY_ROTATION_P,
                                                                        object_id,
                                                                        envelope_id_to_curve_container_map);
        new_instance->rotation   [2] = GetCurveContainerForProperty    (new_instance->name,
                                                                        ITEM_PROPERTY_ROTATION_B,
                                                                        object_id,
                                                                        envelope_id_to_curve_container_map);
        new_instance->translation[0] = GetCurveContainerForProperty    (new_instance->name,
                                                                        ITEM_PROPERTY_TRANSLATION_X,
                                                                        object_id,
                                                                        envelope_id_to_curve_container_map);
        new_instance->translation[1] = GetCurveContainerForProperty    (new_instance->name,
                                                                        ITEM_PROPERTY_TRANSLATION_Y,
                                                                        object_id,
                                                                        envelope_id_to_curve_container_map);
        new_instance->translation[2] = GetCurveContainerForProperty    (new_instance->name,
                                                                        ITEM_PROPERTY_TRANSLATION_Z,
                                                                        object_id,
                                                                        envelope_id_to_curve_container_map);

        /* Extract pivot data */
        double lw_pivot_data[3];

        item_info_ptr->param(object_id,
                             LWIP_PIVOT,
                             0, /* time */
                             lw_pivot_data);

        new_instance->pivot[0] = (float) lw_pivot_data[0];
        new_instance->pivot[1] = (float) lw_pivot_data[1];
        new_instance->pivot[2] = (float) lw_pivot_data[2];

        /* Extract ID data */
        new_instance->object_id      = object_id;
        new_instance->parent_item_id = item_info_ptr->parent(object_id);

        /* Have we already created a mesh instance descriptor that is described by the
         * same file name? If so, we can re-use GPU mesh representation later on,
         * so that it is calculated only once.
         */
        const uint32_t n_objects_initialized = system_resizable_vector_get_amount_of_elements(objects);

        for (uint32_t n_object = 0;
                      n_object < n_objects_initialized;
                    ++n_object)
        {
            _mesh_instance* created_mesh_ptr = NULL;

            if (!system_resizable_vector_get_element_at(objects,
                                                        n_object,
                                                       &created_mesh_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Cannot retrieve mesh instance descriptor at index [%d]",
                                  n_object);

                continue;
            }

            if (system_hashed_ansi_string_is_equal_to_hash_string(created_mesh_ptr->filename,
                                                                  new_instance->filename) )
            {
                new_instance->mesh_gpu_provider_ptr = created_mesh_ptr;

                break;
            }
        } /* for (all added objects) */

        /* Store the instance */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(item_id_to_mesh_instance_map,
                                                     (system_hash64) object_id),
                          "Mesh ID already recognized");

        system_hash64map_insert     (item_id_to_mesh_instance_map,
                                     (system_hash64) object_id,
                                     new_instance,
                                     NULL,  /* on_remove_callback */
                                     NULL); /* on_remove_callback_user_arg */
        system_resizable_vector_push(objects,
                                     new_instance);

        /* Move to the next object */
        object_id = item_info_ptr->next(object_id);
    } /* while (object_id != 0) */

    /* For each object that acts as a mesh data source (that is: whose mesh_gpu_provider_ptr
     * field is NULL), extract vertex & per-polygon normal data.
     *
     * This data will later serve to prepare a mesh instance.
     *
     * NOTE: I really hate the fact we currently serialize "mesh" instances along with
     *       "scene_mesh" instances, since this mangles CPU/GPU border, but loading
     *       "meshes" works super fast when loading the data. Also, if we had scene_mesh
     *       create "mesh" instances, we would really just be duplicating code required to
     *       set up meshes. I'm putting this on my TO-DO list, though, since it should pay
     *       off in the longer run.
     *
     * NOTE: This process MUST NOT be parallelized owing to Lightwave going nuts.
     *       Please see ExtractMeshDataWorkerThreadEntryPoint() doxygen for more info.
     */
    const uint32_t n_unique_objects = object_funcs_ptr->numObjects();

    for (uint32_t n_object = 0;
                  n_object < n_unique_objects;
                ++n_object)
    {
        system_event job_done_event = system_event_create(true,   /* manual_reset */
                                                          false); /* start_state */

        /* Spawn a worker thread so that we can report the progress. */
        system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_with_event_signal(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                      ExtractMeshDataWorkerThreadEntryPoint,
                                                                                                                      (void*) n_object,
                                                                                                                      job_done_event);

        system_thread_pool_submit_single_task(task);
        system_event_wait_single_infinite    (job_done_event);

        system_event_release(job_done_event);
    }

    /* Update parent mesh pointers.
     *
     * NOTE: Not much benefit in running this in parallel.
     */
    const uint32_t n_objects = system_resizable_vector_get_amount_of_elements(objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        _mesh_instance* instance_ptr = NULL;

        if (!system_resizable_vector_get_element_at(objects,
                                                    n_object,
                                                   &instance_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh instance at index [%d]",
                              n_object);

            continue;
        }

        if (instance_ptr->parent_item_id != NULL)
        {
            _mesh_instance* parent_instance_ptr = NULL;

            if (!system_hash64map_get(item_id_to_mesh_instance_map,
                                      (system_hash64) instance_ptr->parent_item_id,
                                     &instance_ptr->parent_mesh_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve parent mesh instance descriptor");
            }
        } /* if (instance_ptr->parent_item_id != NULL) */
    } /* for (all mesh instances) */

    /* Proceed with creating scene_mesh & mesh instances for each of the meshes
     * we've extracted.
     *
     * NOTE: This heavily benefits from parallelization, as the baking process
     *       is time-consuming.
     */
    job_done_events = system_resizable_vector_create(n_objects,
                                                     sizeof(system_event) );

    for (uint32_t n_iteration = 0;
                  n_iteration < 2;
                ++n_iteration)
    {
        if (n_iteration == 1)
        {
            /* Wait for zeroth iteration's tasks to finish before continuing */
            system_event_wait_multiple_infinite((const system_event*) system_resizable_vector_get_array(job_done_events),
                                                system_resizable_vector_get_amount_of_elements         (job_done_events),
                                                true); /* wait_for_all */

            /* Clean up before continuing */
            system_event event_to_release = NULL;

            while (system_resizable_vector_pop(job_done_events,
                                              &event_to_release) )
            {
                system_event_release(event_to_release);

                event_to_release = NULL;
            }

            system_resizable_vector_release(job_done_events);
            job_done_events = NULL;
        }

        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            _mesh_instance* instance_ptr = NULL;

            if (!system_resizable_vector_get_element_at(objects,
                                                        n_object,
                                                       &instance_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh instance at index [%d]",
                                  n_object);

                continue;
            }

            if (instance_ptr->mesh_gpu_provider_ptr != NULL && n_iteration == 0 ||
                instance_ptr->mesh_gpu_provider_ptr == NULL && n_iteration == 1)
            {
                /* Move on to the next object - this iteration is not expected
                 * to be processing this object. */
                continue;
            }

            /* Instantiate the "GPU mesh" first */
            mesh          new_mesh       = NULL;
            mesh_layer_id new_mesh_layer = NULL;

            new_mesh = mesh_create(MESH_SAVE_SUPPORT,
                                   instance_ptr->name);

            ASSERT_ALWAYS_SYNC(new_mesh != NULL,
                               "mesh_create() failed.");
            if (new_mesh == NULL)
            {
                continue;
            }

            if (n_iteration == 0)
            {
                system_event job_done_event = system_event_create(true,   /* manual_reset */
                                                                  false); /* start_state */

                /* Spawn task argument descriptor */
                _bake_mesh_gl_blob_worker_arg* job_arg_ptr = new (std::nothrow) _bake_mesh_gl_blob_worker_arg(instance_ptr,
                                                                                                              new_mesh,
                                                                                                              scene);

                /* Distribute the task to a worker thread. */
                system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_with_event_signal(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                              BakeMeshGLBlobWorkerThreadEntryPoint,
                                                                                                                              (void*) job_arg_ptr,
                                                                                                                              job_done_event);

                system_thread_pool_submit_single_task(task);

                system_resizable_vector_push(job_done_events,
                                             job_done_event);
            }
            else
            {
                /* Instantiated mesh */
                ASSERT_DEBUG_SYNC(instance_ptr->mesh_gpu_provider_ptr->mesh_gpu != NULL,
                                  "Provider mesh is not a provider");

                mesh_set_as_instantiated(new_mesh,
                                         instance_ptr->mesh_gpu_provider_ptr->mesh_gpu);

                AddMeshToScene(scene,
                               new_mesh,
                               instance_ptr);
            }
        } /* for (all objects) */
    } /* for (both iterations) */

    /* All done! */
    ASSERT_DEBUG_SYNC(job_done_events == NULL,
                      "Clean-up error?");
}

/** TODO */
PUBLIC void GetMeshProperty(__in  __notnull scene_mesh   mesh_instance,
                            __in            MeshProperty property,
                            __out __notnull void*        out_result)
{
    _mesh_instance* mesh_instance_ptr = NULL;

    if (!system_hash64map_get(scene_mesh_to_mesh_instance_map,
                              (system_hash64) mesh_instance,
                             &mesh_instance_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input scene_mesh was not recognized");

        goto end;
    }

    switch (property)
    {
        case MESH_PROPERTY_OBJECT_ID:
        {
            *(void**) out_result = mesh_instance_ptr->object_id;

            break;
        }

        case MESH_PROPERTY_PARENT_OBJECT_ID:
        {
            *(void**) out_result = mesh_instance_ptr->parent_item_id;

            break;
        }

        case MESH_PROPERTY_PARENT_SCENE_MESH:
        {
            _mesh_instance* parent_mesh_instance_ptr = NULL;
            scene_mesh      parent_scene_mesh        = NULL;

            if (mesh_instance_ptr->parent_mesh_ptr != NULL)
            {
                if (!system_hash64map_get(mesh_instance_to_scene_mesh_map,
                                          (system_hash64) mesh_instance_ptr->parent_mesh_ptr,
                                         &parent_scene_mesh) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Privately held parent mesh instance representation could not've been converted to scene_mesh");

                    goto end;
                }
            } /* if (mesh_instance_ptr->parent_mesh_ptr != NULL) */

            *(scene_mesh*) out_result = parent_scene_mesh;

            break;
        }

        case MESH_PROPERTY_ROTATION_HPB_CURVES:
        {
            *(curve_container**) out_result = mesh_instance_ptr->rotation;

            break;
        }

        case MESH_PROPERTY_TRANSLATION_CURVES:
        {
            *(curve_container**) out_result = mesh_instance_ptr->translation;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "MeshProperty was not recognized");
        }
    } /* switch (property) */

end:
    ;
}

/** TODO */
PUBLIC void InitMeshData()
{
    ASSERT_DEBUG_SYNC(item_id_to_mesh_instance_map == NULL &&
                      objects                      == NULL,
                      "Mesh data objects already initialized?");

    filename_to_mesh_instance_map   = system_hash64map_create       (sizeof(scene_mesh),
                                                                     true);                   /* should_be_thread_safe */
    item_id_to_mesh_instance_map    = system_hash64map_create       (sizeof(scene_mesh),
                                                                     true);                   /* should_be_thread_safe */
    objects                         = system_resizable_vector_create(4,                       /* capacity */
                                                                     sizeof(_mesh_instance*),
                                                                     true);                   /* should_be_thread_safe */
    mesh_instance_to_scene_mesh_map = system_hash64map_create       (sizeof(scene_mesh),
                                                                     true);                   /* should_be_thread_safe */
    polygon_instance_resource_pool  = system_resource_pool_create   (sizeof(_polygon_instance),
                                                                     128,                     /* n_elements_to_preallocate */
                                                                     NULL,                    /* init_fn */
                                                                     NULL);                   /* deinit_fn */
    scene_mesh_to_mesh_instance_map = system_hash64map_create       (sizeof(_mesh_instance*),
                                                                     true);                   /* should_be_thread_safe */
}
