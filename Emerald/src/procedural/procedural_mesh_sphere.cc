/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "procedural/procedural_mesh_sphere.h"
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
    ral_context                    context;
    procedural_mesh_data_type_bits data_types;
    uint32_t                       n_latitude_splices;
    uint32_t                       n_longitude_splices;
    system_hashed_ansi_string      name;

    uint32_t   n_triangles;
    ral_buffer nonindexed_bo;
    float*     raw_data_ptr;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_sphere;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_sphere,
                               procedural_mesh_sphere,
                              _procedural_mesh_sphere);

/* Private functions */
PRIVATE void _procedural_mesh_sphere_init(_procedural_mesh_sphere* sphere_ptr)
{
    /* Prepare point data. Normals = points */
    const float    theta_degrees_delta       = 180.0f / float(sphere_ptr->n_latitude_splices  - 1);
    const float    phi_degrees_delta         = 360.0f / float(sphere_ptr->n_longitude_splices - 1);
    const uint32_t n_theta_iterations        = sphere_ptr->n_latitude_splices;
    const uint32_t n_phi_iterations          = sphere_ptr->n_longitude_splices;
    const uint32_t n_points                  = (n_phi_iterations) * (n_theta_iterations);
    float*         nonindexed_point_data_ptr = new (std::nothrow) float[n_points * 3];

    ASSERT_ALWAYS_SYNC(nonindexed_point_data_ptr != nullptr,
                       "Could not generate buffer for %d points",
                       n_points);

    if (nonindexed_point_data_ptr != nullptr)
    {
        float* nonindexed_point_data_traveller_ptr = nonindexed_point_data_ptr;

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

                *nonindexed_point_data_traveller_ptr = cos(theta) * cos(phi);
                 nonindexed_point_data_traveller_ptr++;

                *nonindexed_point_data_traveller_ptr = cos(theta) * sin(phi);
                 nonindexed_point_data_traveller_ptr++;

                *nonindexed_point_data_traveller_ptr = sin(theta);
                 nonindexed_point_data_traveller_ptr++;
            }
        }
    }

    /* Prepare index data. */
    const uint32_t  n_ordered_indexes                = (n_phi_iterations) * (n_theta_iterations) * 6;
    uint16_t*       ordered_index_data_ptr           = new (std::nothrow) uint16_t[n_ordered_indexes];
    uint16_t*       ordered_index_data_traveller_ptr = ordered_index_data_ptr;

    ASSERT_ALWAYS_SYNC(ordered_index_data_ptr != nullptr,
                       "Out of memory while allocating index array");

    if (ordered_index_data_ptr != nullptr)
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

                *ordered_index_data_traveller_ptr = n_phi_iteration      * n_theta_iterations + n_theta_iteration;
                 ordered_index_data_traveller_ptr++;

                *ordered_index_data_traveller_ptr = next_n_phi_iteration * n_theta_iterations + next_n_theta_iteration;
                 ordered_index_data_traveller_ptr++;
                *ordered_index_data_traveller_ptr = next_n_phi_iteration * n_theta_iterations + (n_theta_iteration);
                 ordered_index_data_traveller_ptr++;

                *ordered_index_data_traveller_ptr = n_phi_iteration      * n_theta_iterations + n_theta_iteration;
                 ordered_index_data_traveller_ptr++;
                *ordered_index_data_traveller_ptr = n_phi_iteration      * n_theta_iterations + next_n_theta_iteration;
                 ordered_index_data_traveller_ptr++;
                *ordered_index_data_traveller_ptr = next_n_phi_iteration * n_theta_iterations + next_n_theta_iteration;
                 ordered_index_data_traveller_ptr++;
            }
        }
    }

    if (sphere_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_NONINDEXED_BIT)
    {
        /* Primitive restart doesn't make sense for array-based calls. We need to convert the data we have to triangles */
        const uint32_t n_triangles                 = (n_phi_iterations) * (n_theta_iterations) * 2; /* times 2 because we need to make up quads */
        GLfloat*       triangle_data_ptr           = new (std::nothrow) GLfloat[n_triangles * 9];   /* 3 vertices per triangle */
        GLfloat*       triangle_data_traveller_ptr = triangle_data_ptr;

        if (triangle_data_ptr != nullptr)
        {
            for (uint32_t n_index = 0;
                          n_index < n_triangles * 3;
                        ++n_index)
            {
                uint32_t new_index = ordered_index_data_ptr[n_index];

                ASSERT_DEBUG_SYNC(new_index < n_points,
                                  "Out of points");

                *triangle_data_traveller_ptr = nonindexed_point_data_ptr[new_index * 3];
                 triangle_data_traveller_ptr++;
                *triangle_data_traveller_ptr = nonindexed_point_data_ptr[new_index * 3 + 1];
                 triangle_data_traveller_ptr++;
                *triangle_data_traveller_ptr = nonindexed_point_data_ptr[new_index * 3 + 2];
                 triangle_data_traveller_ptr++;
            }

            /* Store the data. */
            ral_buffer_create_info                                               nonindexed_bo_create_info;
            ral_buffer_client_sourced_update_info                                nonindexed_bo_update_info;
            std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > nonindexed_bo_update_info_ptrs;

            sphere_ptr->n_triangles  = n_triangles;

            nonindexed_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
            nonindexed_bo_create_info.parent_buffer    = NULL;
            nonindexed_bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
            nonindexed_bo_create_info.size             = n_triangles * 3 * 3 * sizeof(GLfloat);
            nonindexed_bo_create_info.start_offset     = 0;
            nonindexed_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            nonindexed_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

            nonindexed_bo_update_info.data         = triangle_data_ptr;
            nonindexed_bo_update_info.data_size    = nonindexed_bo_create_info.size;
            nonindexed_bo_update_info.start_offset = 0;

            nonindexed_bo_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&nonindexed_bo_update_info,
                                                                                                            NullDeleter<ral_buffer_client_sourced_update_info>() ));

            ral_context_create_buffers(sphere_ptr->context,
                                       1, /* n_buffers */
                                       &nonindexed_bo_create_info,
                                      &sphere_ptr->nonindexed_bo);

            ral_buffer_set_data_from_client_memory(sphere_ptr->nonindexed_bo,
                                                   nonindexed_bo_update_info_ptrs,
                                                   false, /* async               */
                                                   true   /* sync_other_contexts */);

            /* Fine to release the buffers now, unless the caller has requested otherwise */
            if (!(sphere_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_RAW_BIT))
            {
                delete [] triangle_data_ptr;

                triangle_data_ptr = nullptr;
            }
            else
            {
                sphere_ptr->raw_data_ptr = triangle_data_ptr;
            }
        }
    }

    /* Okay, we're cool to release the buffers now, unless the caller has requested to retain those. */
    delete [] nonindexed_point_data_ptr;
    delete [] ordered_index_data_ptr;
}

/** TODO */
PRIVATE void _procedural_mesh_sphere_release(void* arg)
{
    _procedural_mesh_sphere* sphere_ptr = reinterpret_cast<_procedural_mesh_sphere*>(arg);

    if (sphere_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_NONINDEXED_BIT)
    {
        ral_context_delete_objects(sphere_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   (const void**) &sphere_ptr->nonindexed_bo);

        sphere_ptr->nonindexed_bo = nullptr;
    }

    if (sphere_ptr->raw_data_ptr != nullptr)
    {
        delete [] sphere_ptr->raw_data_ptr;

        sphere_ptr->raw_data_ptr = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_sphere procedural_mesh_sphere_create(ral_context                    context,
                                                                        procedural_mesh_data_type_bits data_types,
                                                                        uint32_t                       n_latitude_splices,
                                                                        uint32_t                       n_longitude_splices,
                                                                        system_hashed_ansi_string      name)
{
    /* Create the instance */
    _procedural_mesh_sphere* sphere_ptr = new (std::nothrow) _procedural_mesh_sphere;

    ASSERT_ALWAYS_SYNC(sphere_ptr != nullptr,
                       "Out of memory while allocating space for sphere instance [%s]",
                       system_hashed_ansi_string_get_buffer(name) );

    if (sphere_ptr != nullptr)
    {
        /* Cache input arguments */
        sphere_ptr->context             = context;
        sphere_ptr->data_types          = data_types;
        sphere_ptr->n_latitude_splices  = n_latitude_splices;
        sphere_ptr->n_longitude_splices = n_longitude_splices;
        sphere_ptr->name                = name;
        sphere_ptr->nonindexed_bo       = nullptr;
        sphere_ptr->n_triangles         = -1;
        sphere_ptr->raw_data_ptr        = nullptr;

        /* Call back renderer to continue */
        _procedural_mesh_sphere_init(sphere_ptr);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(sphere_ptr,
                                                       _procedural_mesh_sphere_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_SPHERE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (sphere)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return reinterpret_cast<procedural_mesh_sphere>(sphere_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_property(procedural_mesh_sphere           sphere,
                                                            _procedural_mesh_sphere_property prop,
                                                            void*                            out_result)
{
    _procedural_mesh_sphere* sphere_ptr = (_procedural_mesh_sphere*) sphere;

    switch (prop)
    {
        case PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER:
        {
            *(ral_buffer*) out_result = sphere_ptr->nonindexed_bo;

            break;
        }

        case PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER_NORMAL_DATA_OFFSET:
        case PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER_VERTEX_DATA_OFFSET:
        {
            *(uint32_t*) out_result = 0;

            break;
        }

        case PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_RAW_DATA:
        {
            ASSERT_DEBUG_SYNC((sphere_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_RAW_BIT) != 0,
                              "PROCEDURAL_MESH_DATA_TYPE_RAW_BIT request but the procedural_mesh_sphere was"
                              " not created with PROCEDURAL_MESH_DATA_TYPE_RAW_BIT flag");

            *(float**) out_result = sphere_ptr->raw_data_ptr;

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
                              "Unrecognized _rocedural_mesh_sphere_property value");
        }
    }
}
