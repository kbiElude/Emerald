/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "procedural/procedural_mesh_sphere.h"
#include "raGL/raGL_buffer.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "system/system_log.h"

/** BUFFER DATA STRUCTURE:
 *
 * 1) Vertex model-space locations
 * 2) Indexes
 * 3) Normals
 */

/* Internal type definitions */
typedef struct
{
    ral_context                   context;
    _procedural_mesh_data_bitmask creation_bitmask;
    uint32_t                      n_latitude_splices;
    uint32_t                      n_longitude_splices;
    system_hashed_ansi_string     name;

    ral_buffer arrays_bo;
    GLuint     n_triangles;
    GLfloat*   raw_arrays_data;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_sphere;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_sphere,
                               procedural_mesh_sphere,
                              _procedural_mesh_sphere);

/* Private functions */
PRIVATE void _procedural_mesh_sphere_create_renderer_callback(ogl_context context,
                                                              void*       arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_limits*                              limits_ptr       = NULL;
    _procedural_mesh_sphere*                                  mesh_sphere      = (_procedural_mesh_sphere*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* Prepare point data. Normals = points */
    float    theta_degrees_delta = 180.0f / float(mesh_sphere->n_latitude_splices - 1);
    float    phi_degrees_delta   = 360.0f / float(mesh_sphere->n_longitude_splices - 1);
    uint32_t n_theta_iterations  = mesh_sphere->n_latitude_splices;
    uint32_t n_phi_iterations    = mesh_sphere->n_longitude_splices;
    uint32_t n_points            = (n_phi_iterations) * (n_theta_iterations);
    float*   nonindexed_points   = new (std::nothrow) float[n_points * 3];

    ASSERT_ALWAYS_SYNC(nonindexed_points != NULL,
                       "Could not generate buffer for %d points",
                       n_points);

    if (nonindexed_points != NULL)
    {
        float* nonindexed_points_traveller = nonindexed_points;

        for (uint32_t n_phi_iteration = 0;
                      n_phi_iteration < n_phi_iterations;
                    ++n_phi_iteration)
        {
            float phi = DEG_TO_RAD(phi_degrees_delta) * n_phi_iteration;

            for (uint32_t n_theta_iteration = 0;
                          n_theta_iteration < n_theta_iterations;
                        ++n_theta_iteration)
            {
                float theta = (-3.14152965f * 0.5f) + DEG_TO_RAD(theta_degrees_delta) * n_theta_iteration;

                *nonindexed_points_traveller = cos(theta) * cos(phi);
                nonindexed_points_traveller++;

                *nonindexed_points_traveller = cos(theta) * sin(phi);
                nonindexed_points_traveller++;

                *nonindexed_points_traveller = sin(theta);
                nonindexed_points_traveller++;
            }
        }
    }

    /* Prepare index data. */
    uint32_t  n_ordered_indexes         = (n_phi_iterations) * (n_theta_iterations) * 6;
    GLushort* ordered_indexes           = new (std::nothrow) GLushort[n_ordered_indexes];
    GLushort* ordered_indexes_traveller = ordered_indexes;

    ASSERT_ALWAYS_SYNC(ordered_indexes != NULL,
                       "Out of memory while allocating index array");

    if (ordered_indexes != NULL)
    {
        for (uint32_t n_phi_iteration = 0;
                      n_phi_iteration < n_phi_iterations;
                    ++n_phi_iteration)
        {
            uint32_t next_n_phi_iteration = ((n_phi_iteration + 1) != n_phi_iterations ? (n_phi_iteration + 1) : 0);

            for (uint32_t n_theta_iteration = 0;
                          n_theta_iteration < n_theta_iterations;
                        ++n_theta_iteration)
            {
                /* Form a quad made of 2 triangles */
                uint32_t next_n_theta_iteration = ((n_theta_iteration + 1) != n_theta_iterations ? (n_theta_iteration + 1) : 0);

                *ordered_indexes_traveller = n_phi_iteration      * n_theta_iterations + n_theta_iteration;
                 ordered_indexes_traveller++;

                *ordered_indexes_traveller = next_n_phi_iteration * n_theta_iterations + next_n_theta_iteration;
                 ordered_indexes_traveller++;
                *ordered_indexes_traveller = next_n_phi_iteration * n_theta_iterations + (n_theta_iteration);
                 ordered_indexes_traveller++;

                *ordered_indexes_traveller = n_phi_iteration      * n_theta_iterations + n_theta_iteration;
                 ordered_indexes_traveller++;
                *ordered_indexes_traveller = n_phi_iteration      * n_theta_iterations + next_n_theta_iteration;
                 ordered_indexes_traveller++;
                *ordered_indexes_traveller = next_n_phi_iteration * n_theta_iterations + next_n_theta_iteration;
                 ordered_indexes_traveller++;
            }
        }
    }

    if (mesh_sphere->creation_bitmask & DATA_BO_ARRAYS)
    {
        /* Primitive restart doesn't make sense for array-based calls. We need to convert the data we have to triangles */
        uint32_t n_triangles         = (n_phi_iterations) * (n_theta_iterations) * 2; /* times 2 because we need to make up quads */
        GLfloat* triangles           = new (std::nothrow) GLfloat[n_triangles * 9]; /* 3 vertexes per triangle */
        GLfloat* triangles_traveller = triangles;

        if (triangles != NULL)
        {
            for (uint32_t n_index = 0;
                          n_index < n_triangles * 3;
                        ++n_index)
            {
                uint32_t new_index = ordered_indexes[n_index];

                ASSERT_DEBUG_SYNC(new_index < n_points,
                                  "Out of points");

                *triangles_traveller = nonindexed_points[new_index*3  ];
                 triangles_traveller++;
                *triangles_traveller = nonindexed_points[new_index*3+1];
                 triangles_traveller++;
                *triangles_traveller = nonindexed_points[new_index*3+2];
                 triangles_traveller++;
            } /* for (uint32_t n_index = 0; n_index < n_ordered_indexes; ++n_index)*/

            /* Store the data. */
            ral_buffer_create_info                arrays_create_info;
            ral_buffer_client_sourced_update_info arrays_update_info;

            mesh_sphere->n_triangles  = n_triangles;

            arrays_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
            arrays_create_info.parent_buffer    = NULL;
            arrays_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
            arrays_create_info.size             = n_triangles * 3 * 3 * sizeof(GLfloat);
            arrays_create_info.start_offset     = 0;
            arrays_create_info.usage_bits       = RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            arrays_create_info.user_queue_bits  = 0xFFFFFFFF;

            arrays_update_info.data         = triangles;
            arrays_update_info.data_size    = arrays_create_info.size;
            arrays_update_info.start_offset = 0;

            ral_context_create_buffers(mesh_sphere->context,
                                       1, /* n_buffers */
                                       &arrays_create_info,
                                      &mesh_sphere->arrays_bo);

            ral_buffer_set_data_from_client_memory(mesh_sphere->arrays_bo,
                                                   1, /* n_updates */
                                                  &arrays_update_info);

            /* Fine to release the buffers now, unless the caller has requested otherwise */
            if (!(mesh_sphere->creation_bitmask & DATA_RAW))
            {
                delete [] triangles;

                triangles = NULL;
            }
            else
            {
                mesh_sphere->raw_arrays_data = triangles;
            }
        }
    }

    /* Okay, we're cool to release the buffers now, unless the caller has requested to retain those. */
    delete [] nonindexed_points;
    delete [] ordered_indexes;

    nonindexed_points = NULL;
    ordered_indexes   = NULL;
}

/** TODO */
PRIVATE void _procedural_mesh_sphere_release_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _procedural_mesh_sphere*          mesh_sphere  = (_procedural_mesh_sphere*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (mesh_sphere->creation_bitmask & DATA_BO_ARRAYS)
    {
        ral_context_delete_objects(mesh_sphere->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   (const void**) &mesh_sphere->arrays_bo);

        mesh_sphere->arrays_bo = NULL;
    }
}

/** TODO */
PRIVATE void _procedural_mesh_sphere_release(void* arg)
{
    _procedural_mesh_sphere* instance = (_procedural_mesh_sphere*) arg;

    if (instance->raw_arrays_data != NULL)
    {
        delete [] instance->raw_arrays_data;

        instance->raw_arrays_data = NULL;
    }

    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(instance->context),
                                                     _procedural_mesh_sphere_release_renderer_callback,
                                                     instance);
}

/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_sphere procedural_mesh_sphere_create(ral_context                   context,
                                                                        _procedural_mesh_data_bitmask data_bitmask,
                                                                        uint32_t                      n_latitude_splices, /* number of latitude splices */
                                                                        uint32_t                      n_longitude_splices, /* number of longitude splices */
                                                                        system_hashed_ansi_string     name)
{
    /* Create the instance */
    _procedural_mesh_sphere* new_instance = new (std::nothrow) _procedural_mesh_sphere;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory while allocating space for sphere instance [%s]",
                       system_hashed_ansi_string_get_buffer(name) );

    if (new_instance != NULL)
    {
        /* Cache input arguments */
        new_instance->arrays_bo           = NULL;
        new_instance->context             = context;
        new_instance->creation_bitmask    = data_bitmask;
        new_instance->n_latitude_splices  = n_latitude_splices;
        new_instance->n_longitude_splices = n_longitude_splices;
        new_instance->name                = name;
        new_instance->n_triangles         = -1;
        new_instance->raw_arrays_data     = NULL;

        /* Call back renderer to continue */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                         _procedural_mesh_sphere_create_renderer_callback,
                                                         new_instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                       _procedural_mesh_sphere_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_SPHERE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (sphere)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (procedural_mesh_sphere) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_property(procedural_mesh_sphere           sphere,
                                                            _procedural_mesh_sphere_property prop,
                                                            void*                            out_result)
{
    _procedural_mesh_sphere* sphere_ptr = (_procedural_mesh_sphere*) sphere;

    switch (prop)
    {
        case PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_RAL:
        {
            *(ral_buffer*) out_result = sphere_ptr->arrays_bo;

            break;
        }

        case PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_NORMALS_DATA_OFFSET:
        case PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET:
        {
            raGL_buffer arrays_bo_raGL = ral_context_get_buffer_gl(sphere_ptr->context,
                                                                   sphere_ptr->arrays_bo);

            raGL_buffer_get_property(arrays_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                     out_result);

            break;
        }

        case PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_RAW_DATA:
        {
            ASSERT_DEBUG_SYNC((sphere_ptr->creation_bitmask & DATA_RAW) != 0,
                              "PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_RAW_DATA request but the procedural_mesh_sphere was"
                              " not created with DATA_RAW flag");

            *(GLfloat**) out_result = sphere_ptr->raw_arrays_data;

            break;
        }

        case PROCEDURAL_MESH_SPHERE_PROPERTY_N_TRIANGLES:
        {
            *(unsigned int*) out_result = sphere_ptr->n_triangles;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _procedural_mesh_sphere_property value");
        }
    } /* switch (prop) */
}
