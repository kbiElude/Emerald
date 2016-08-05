/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "procedural/procedural_mesh_box.h"
#include "system/system_log.h"

/** RESULT BUFFER DATA STRUCTURE:
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
    uint32_t                       n_horizontal;
    uint32_t                       n_vertical;
    system_hashed_ansi_string      name;

    ral_buffer  nonindexed_bo;
    GLuint      nonindexed_bo_normal_data_offset;

    ral_buffer   indexed_bo;
    unsigned int indexed_bo_index_data_offset;
    unsigned int indexed_bo_normal_data_offset;

    GLuint n_points;    /* indexed data-specific    */
    GLuint n_triangles; /* nonindexed data-specific */
    GLuint primitive_restart_index;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_box;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_box,
                               procedural_mesh_box,
                              _procedural_mesh_box);


/* Private functions */
PRIVATE void _procedural_mesh_box_init(_procedural_mesh_box* box_ptr)
{
    /* Prepare vertex data. Points are organised, starting from top plane to bottom (Y axis corresponds to height). 2 points
     * correspond to borders. */
    uint32_t n_points_per_plane         = (box_ptr->n_horizontal + 2) * (box_ptr->n_vertical + 2);
    uint32_t n_points                   = n_points_per_plane * 6;
    float*   nonindexed_point_data_ptr  = new (std::nothrow) float[n_points * 3];
    float*   nonindexed_normal_data_ptr = new (std::nothrow) float[n_points * 3];

    ASSERT_ALWAYS_SYNC(nonindexed_point_data_ptr  != nullptr &&
                       nonindexed_normal_data_ptr != nullptr,
                       "Could not generate buffers for %d points",
                       n_points);

    if (nonindexed_point_data_ptr  != nullptr &&
        nonindexed_normal_data_ptr != nullptr)
    {
        float* nonindexed_normals_traveller = nonindexed_normal_data_ptr;
        float* nonindexed_points_traveller  = nonindexed_point_data_ptr;

        /* Bottom plane:
         *
         * B-----C       +---->x
         * |     |       |
         * |     |       |
         * A-----D       v z
         */
        for (uint32_t z = 0;
                      z < box_ptr->n_vertical + 2;
                    ++z)
        {
            for (uint32_t x = 0;
                          x < box_ptr->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(box_ptr->n_horizontal + 1);
                float pz = float(z) / float(box_ptr->n_vertical   + 1);

                *nonindexed_points_traveller = px; nonindexed_points_traveller ++;
                *nonindexed_points_traveller = 0;  nonindexed_points_traveller ++;
                *nonindexed_points_traveller = pz; nonindexed_points_traveller ++;

                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = -1; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
            }
        }

        /* Top plane:
         *
         * F----G       +--->x
         * |    |       |
         * |    |       |
         * E----H       v z
         */
        for (uint32_t z = 0;
                      z < box_ptr->n_vertical + 2;
                    ++z)
        {
            for (uint32_t x = 0;
                          x < box_ptr->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(box_ptr->n_horizontal + 1);
                float pz = float(z) / float(box_ptr->n_vertical   + 1);

                *nonindexed_points_traveller = px; nonindexed_points_traveller ++;
                *nonindexed_points_traveller = 1;  nonindexed_points_traveller ++;
                *nonindexed_points_traveller = pz; nonindexed_points_traveller ++;

                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 1;  nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
            }
        }

        /* Left plane:
         *
         * A-----B     +---->z
         * |     |     |
         * |     |     |
         * E-----F     v y
         */
        for (uint32_t y = 0;
                      y < box_ptr->n_vertical + 2;
                    ++y)
        {
            for (uint32_t z = 0;
                          z < box_ptr->n_horizontal + 2;
                        ++z)
            {
                float pz = float(z) / float(box_ptr->n_horizontal + 1);
                float py = float(y) / float(box_ptr->n_vertical   + 1);

                *nonindexed_points_traveller = 0;  nonindexed_points_traveller++;
                *nonindexed_points_traveller = py; nonindexed_points_traveller++;
                *nonindexed_points_traveller = pz; nonindexed_points_traveller++;

                *nonindexed_normals_traveller = -1; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
            }
        }

        /* Right plane:
         *
         * D-----C  +----->z
         * |     |  |
         * |     |  |
         * H-----G  v y
         */
        for (uint32_t y = 0;
                      y < box_ptr->n_vertical + 2;
                    ++y)
        {
            for (uint32_t z = 0;
                          z < box_ptr->n_horizontal + 2;
                        ++z)
            {
                float pz = float(z) / float(box_ptr->n_horizontal + 1);
                float py = float(y) / float(box_ptr->n_vertical   + 1);

                *nonindexed_points_traveller = 1;  nonindexed_points_traveller++;
                *nonindexed_points_traveller = py; nonindexed_points_traveller++;
                *nonindexed_points_traveller = pz; nonindexed_points_traveller++;

                *nonindexed_normals_traveller = 1; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0; nonindexed_normals_traveller ++;
            }
        }

        /* Front plane:
         *
         * A-----D  +---->x
         * |     |  |
         * |     |  |
         * E-----H  v y
         */
        for (uint32_t y = 0;
                      y < box_ptr->n_vertical + 2;
                    ++y)
        {
            for (uint32_t x = 0;
                          x < box_ptr->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(box_ptr->n_horizontal + 1);
                float py = float(y) / float(box_ptr->n_vertical   + 1);

                *nonindexed_points_traveller = px; nonindexed_points_traveller++;
                *nonindexed_points_traveller = py; nonindexed_points_traveller++;
                *nonindexed_points_traveller = 0;  nonindexed_points_traveller++;

                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0;  nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = -1; nonindexed_normals_traveller ++;

            }
        }

        /* Rear plane:
         *
         * B-----C  +---->x
         * |     |  |
         * |     |  |
         * F-----G  v y
         */
        for (uint32_t y = 0;
                      y < box_ptr->n_vertical + 2;
                    ++y)
        {
            for (uint32_t x = 0;
                          x < box_ptr->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(box_ptr->n_horizontal + 1);
                float py = float(y) / float(box_ptr->n_vertical   + 1);

                *nonindexed_points_traveller = px; nonindexed_points_traveller++;
                *nonindexed_points_traveller = py; nonindexed_points_traveller++;
                *nonindexed_points_traveller = 1;  nonindexed_points_traveller++;

                *nonindexed_normals_traveller = 0; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 1; nonindexed_normals_traveller ++;
            }
        }

        /* Set other fields */
        box_ptr->n_points                = n_points;
        box_ptr->primitive_restart_index = n_points + 1;
    }

    /* Prepare index data. This is pretty straightforward - the order is above, we're using triangle strips
     * so we need to use primitive restart index to separate planes. 
     *
     * The order for:
     * 
     * A--B--C      3  0  4   1   5   2
     * |\ |\ |  is: D, A, E | B | F | C
     * D-\E-\F
     */
    ASSERT_ALWAYS_SYNC(n_points + 1 /* restart index */ <= 65535,
                       "Too many points for BO to hold!");

    uint32_t  n_ordered_indexes                 = 6 * (box_ptr->n_vertical + 1) * (box_ptr->n_horizontal + 2) * 2 +
                                                  6 * (box_ptr->n_vertical + 1); /* vertical restarts */
    GLushort* ordered_index_data_ptr            = new (std::nothrow) GLushort[n_ordered_indexes];
    GLushort* ordered_index_data_traveller_ptr  = ordered_index_data_ptr;
    GLfloat*  ordered_normal_data_ptr           = new (std::nothrow) GLfloat [n_ordered_indexes * 3];
    GLfloat*  ordered_normal_data_traveller_ptr = ordered_normal_data_ptr;

    ASSERT_ALWAYS_SYNC(ordered_index_data_ptr != nullptr,
                       "Out of memory while allocating index array");
    ASSERT_ALWAYS_SYNC(ordered_normal_data_ptr != nullptr,
                       "Out of memory whiel allocating normals array");

    if (ordered_index_data_ptr != nullptr &&
        ordered_normal_data_ptr != nullptr)
    {
        uint32_t assertion_check_cnt = 0;

        for (int n_plane = 0;
                 n_plane < 6;
               ++n_plane)
        {
            for (uint32_t y = 0;
                          y < box_ptr->n_vertical + 1;
                        ++y)
            {
                int n_horizontal_indexes = (box_ptr->n_horizontal + 2);

                for (uint32_t n_patch = 0;
                              n_patch < box_ptr->n_horizontal + 2;
                            ++n_patch)
                {
                    *ordered_index_data_traveller_ptr = y * n_horizontal_indexes + n_patch + n_plane * n_points_per_plane + n_horizontal_indexes; ordered_index_data_traveller_ptr++;
                    *ordered_index_data_traveller_ptr = y * n_horizontal_indexes + n_patch + n_plane * n_points_per_plane;                        ordered_index_data_traveller_ptr++;

                    assertion_check_cnt += 2;

                    switch (n_plane)
                    {
                        case 0:
                        {
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = 1; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = 1; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;

                            break;
                        }

                        case 1:
                        {
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = -1; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = -1; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;

                            break;
                        }

                        case 2:
                        {
                            ordered_normal_data_traveller_ptr[0] = -1;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;
                            ordered_normal_data_traveller_ptr[0] = -1;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;

                            break;
                        }

                        case 3:
                        {
                            ordered_normal_data_traveller_ptr[0] = 1;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;
                            ordered_normal_data_traveller_ptr[0] = 1;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = 0;  ordered_normal_data_traveller_ptr += 3;

                            break;
                        }

                        case 4:
                        {
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = -1;  ordered_normal_data_traveller_ptr += 3;
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = -1;  ordered_normal_data_traveller_ptr += 3;

                            break;
                        }

                        case 5:
                        {
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = 1;  ordered_normal_data_traveller_ptr += 3;
                            ordered_normal_data_traveller_ptr[0] = 0;  ordered_normal_data_traveller_ptr[1] = 0; ordered_normal_data_traveller_ptr[2] = 1;  ordered_normal_data_traveller_ptr += 3;

                            break;
                        }

                        default:ASSERT_DEBUG_SYNC(false,
                                                  "");
                    }
                }

                *ordered_index_data_traveller_ptr = box_ptr->primitive_restart_index; 

                ordered_index_data_traveller_ptr++;
                ordered_normal_data_traveller_ptr += 3;

                assertion_check_cnt++;
            }
        }

        ASSERT_DEBUG_SYNC(assertion_check_cnt == n_ordered_indexes,
                          "Element indices counter is wrong!");
    }

    if (box_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_NONINDEXED_BIT)
    {
        /* Primitive restart doesn't make sense for array-based calls. We need to convert the data we have to triangles */
        GLfloat* normal_data_ptr             = new (std::nothrow) GLfloat[n_ordered_indexes * 9];
        GLfloat* normal_data_traveller_ptr   = normal_data_ptr;
        GLfloat* triangle_data_ptr           = new (std::nothrow) GLfloat[n_ordered_indexes * 9];
        GLfloat* triangle_data_traveller_ptr = triangle_data_ptr;

        uint32_t n_triangles         = 0;
        float    local_vertexes[9]   = {0};
        float    local_normals [9]   = {0};
        uint32_t n_vertexes          = 0;

        if (normal_data_ptr   != nullptr &&
            triangle_data_ptr != nullptr)
        {
            for (uint32_t n_index = 0;
                          n_index < n_ordered_indexes;
                        ++n_index)
            {
                if (ordered_index_data_ptr[n_index] == box_ptr->primitive_restart_index)
                {
                    /* New triangle strip starts */
                    n_vertexes = 0;
                }
                else
                {
                    uint32_t new_index = ordered_index_data_ptr[n_index];

                    if (n_vertexes < 2) /* 0 or 1 vertex is available - can't form a triangle */
                    {
                        local_vertexes[3*n_vertexes  ] = nonindexed_point_data_ptr[new_index*3  ];
                        local_vertexes[3*n_vertexes+1] = nonindexed_point_data_ptr[new_index*3+1];
                        local_vertexes[3*n_vertexes+2] = nonindexed_point_data_ptr[new_index*3+2];

                        local_normals [3*n_vertexes  ] = nonindexed_normal_data_ptr[new_index*3  ];
                        local_normals [3*n_vertexes+1] = nonindexed_normal_data_ptr[new_index*3+1];
                        local_normals [3*n_vertexes+2] = nonindexed_normal_data_ptr[new_index*3+2];

                        n_vertexes++;
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(n_vertexes == 2,
                                          "");

                        /* Insert new vertex */
                        local_vertexes[6] = nonindexed_point_data_ptr[new_index*3  ];
                        local_vertexes[7] = nonindexed_point_data_ptr[new_index*3+1];
                        local_vertexes[8] = nonindexed_point_data_ptr[new_index*3+2];

                        local_normals[6] = nonindexed_normal_data_ptr[new_index*3  ];
                        local_normals[7] = nonindexed_normal_data_ptr[new_index*3+1];
                        local_normals[8] = nonindexed_normal_data_ptr[new_index*3+2];

                        /* Append a triangle */
                        memcpy(triangle_data_traveller_ptr,
                               local_vertexes,
                               9 * sizeof(GLfloat) );
                        memcpy(normal_data_traveller_ptr,
                               local_normals,
                               9 * sizeof(GLfloat) );

                        triangle_data_traveller_ptr += 9;
                        normal_data_traveller_ptr   += 9;
                        n_triangles                 ++;

                        /* Drop the oldest vertex to make space for the new one */
                        for (int n = 3;
                                 n < 9;
                               ++n)
                        {
                            local_vertexes[n - 3] = local_vertexes[n];
                            local_normals [n - 3] = local_normals [n];
                        }
                    }
                }
            }

            /* Store the data. */
            ral_buffer_create_info                 nonindexed_bo_create_info;
            ral_buffer_client_sourced_update_info  nonindexed_bo_normals_update_info;
            ral_buffer_client_sourced_update_info  nonindexed_bo_triangles_update_info;

            box_ptr->nonindexed_bo_normal_data_offset = n_triangles * 9 * sizeof(GLfloat);
            box_ptr->n_triangles                      = n_triangles;

            nonindexed_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
            nonindexed_bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
            nonindexed_bo_create_info.size             = box_ptr->nonindexed_bo_normal_data_offset * 2;
            nonindexed_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            nonindexed_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

            nonindexed_bo_normals_update_info.data         = normal_data_ptr;
            nonindexed_bo_normals_update_info.data_size    = box_ptr->nonindexed_bo_normal_data_offset;
            nonindexed_bo_normals_update_info.start_offset = box_ptr->nonindexed_bo_normal_data_offset;

            nonindexed_bo_triangles_update_info.data         = triangle_data_ptr;
            nonindexed_bo_triangles_update_info.data_size    = box_ptr->nonindexed_bo_normal_data_offset;
            nonindexed_bo_triangles_update_info.start_offset = 0;

            std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > arrays_bo_updates;

            arrays_bo_updates.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&nonindexed_bo_triangles_update_info,
                                                                                               NullDeleter<ral_buffer_client_sourced_update_info>() ));
            arrays_bo_updates.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&nonindexed_bo_normals_update_info,
                                                                                               NullDeleter<ral_buffer_client_sourced_update_info>() ));

            ral_context_create_buffers(box_ptr->context,
                                       1, /* n_buffers */
                                      &nonindexed_bo_create_info,
                                      &box_ptr->nonindexed_bo);

            ral_buffer_set_data_from_client_memory(box_ptr->nonindexed_bo,
                                                   arrays_bo_updates,
                                                   false, /* async               */
                                                   true   /* sync_other_contexts */);

            /* Fine to release the buffers now */
            delete [] normal_data_ptr;
            delete [] triangle_data_ptr;
        }
    }

    if (box_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_INDEXED_BIT)
    {
        uint32_t ordered_normals_size = n_ordered_indexes * 3 * sizeof(GLfloat);

        /* Set offsets. */
        ral_buffer_create_info                                                indexed_data_bo_create_info;
        ral_buffer_client_sourced_update_info                                 indexed_data_bo_update_info[3];
        std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info > > indexed_data_bo_update_info_ptrs;

        indexed_data_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
        indexed_data_bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
        indexed_data_bo_create_info.size             = box_ptr->indexed_bo_normal_data_offset + ordered_normals_size;
        indexed_data_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_INDEX_BUFFER_BIT;
        indexed_data_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

        ral_context_create_buffers(box_ptr->context,
                                   1, /* n_buffers */
                                   &indexed_data_bo_create_info,
                                   &box_ptr->indexed_bo);

        box_ptr->indexed_bo_index_data_offset  =                                         n_points      * 3 * sizeof(GLfloat);
        box_ptr->indexed_bo_normal_data_offset = box_ptr->indexed_bo_index_data_offset + n_ordered_indexes * sizeof(GLushort);

        /* Set buffer object contents */
        indexed_data_bo_update_info[0].data         = nonindexed_point_data_ptr;
        indexed_data_bo_update_info[0].data_size    = box_ptr->indexed_bo_index_data_offset;
        indexed_data_bo_update_info[0].start_offset = 0;

        indexed_data_bo_update_info[1].data         = ordered_index_data_ptr;
        indexed_data_bo_update_info[1].data_size    = box_ptr->indexed_bo_normal_data_offset - box_ptr->indexed_bo_index_data_offset;
        indexed_data_bo_update_info[1].start_offset = box_ptr->indexed_bo_index_data_offset;

        indexed_data_bo_update_info[2].data         = ordered_normal_data_ptr;
        indexed_data_bo_update_info[2].data_size    = ordered_normals_size;
        indexed_data_bo_update_info[2].start_offset = box_ptr->indexed_bo_normal_data_offset;

        indexed_data_bo_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(indexed_data_bo_update_info + 0,
                                                                                                          NullDeleter<ral_buffer_client_sourced_update_info>() ));
        indexed_data_bo_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(indexed_data_bo_update_info + 1,
                                                                                                          NullDeleter<ral_buffer_client_sourced_update_info>() ));
        indexed_data_bo_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(indexed_data_bo_update_info + 2,
                                                                                                          NullDeleter<ral_buffer_client_sourced_update_info>() ));

        ral_buffer_set_data_from_client_memory(box_ptr->indexed_bo,
                                               indexed_data_bo_update_info_ptrs,
                                               false, /* async               */
                                               true   /* sync_other_contexts */);
    }

    /* Update "number of points" to a value that will make sense to end-user */
    if (box_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_INDEXED_BIT)
    {
        box_ptr->n_points = n_ordered_indexes;
    }
    else
    {
        box_ptr->n_points = box_ptr->n_triangles * 3;
    }

    /* Okay, we're cool to release the buffers now */
    delete [] nonindexed_point_data_ptr;
    delete [] nonindexed_normal_data_ptr;
    delete [] ordered_normal_data_ptr;
    delete [] ordered_index_data_ptr;
}

/** TODO */
PRIVATE void _procedural_mesh_box_release(void* arg)
{
    _procedural_mesh_box* box_ptr = reinterpret_cast<_procedural_mesh_box*>(arg);

    if (box_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_NONINDEXED_BIT)
    {
        ral_context_delete_objects(box_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&box_ptr->nonindexed_bo) );

        box_ptr->nonindexed_bo                    = nullptr;
        box_ptr->nonindexed_bo_normal_data_offset = -1;
    }

    if (box_ptr->data_types & PROCEDURAL_MESH_DATA_TYPE_INDEXED_BIT)
    {
        ral_context_delete_objects(box_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   reinterpret_cast<void* const*>(&box_ptr->indexed_bo) );

        box_ptr->indexed_bo                    = nullptr;
        box_ptr->indexed_bo_index_data_offset  = -1;
        box_ptr->indexed_bo_normal_data_offset = -1;
    } 
}

/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(ral_context                    context,
                                                                  procedural_mesh_data_type_bits mesh_data_types,
                                                                  uint32_t                       n_horizontal,
                                                                  uint32_t                       n_vertical,
                                                                  system_hashed_ansi_string      name)
{
    /* Create the instance */
    _procedural_mesh_box* box_ptr = new (std::nothrow) _procedural_mesh_box;

    ASSERT_ALWAYS_SYNC(box_ptr != nullptr,
                       "Out of memory while allocating space for box instance [%s]",
                       system_hashed_ansi_string_get_buffer(name) );

    if (box_ptr != nullptr)
    {
        /* Cache input arguments */
        box_ptr->context      = context;
        box_ptr->data_types   = mesh_data_types;
        box_ptr->n_horizontal = n_horizontal;
        box_ptr->n_vertical   = n_vertical;
        box_ptr->name         = name;

        box_ptr->indexed_bo                       = nullptr;
        box_ptr->indexed_bo_index_data_offset     = -1;
        box_ptr->indexed_bo_normal_data_offset    = -1;
        box_ptr->n_points                         = -1;
        box_ptr->n_triangles                      = -1;
        box_ptr->nonindexed_bo                    = nullptr;
        box_ptr->nonindexed_bo_normal_data_offset = -1;
        box_ptr->primitive_restart_index          = -1;

        /* Initialize */
        _procedural_mesh_box_init(box_ptr);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(box_ptr,
                                                       _procedural_mesh_box_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_BOX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (box)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return reinterpret_cast<procedural_mesh_box>(box_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_box_get_property(procedural_mesh_box          box,
                                                         procedural_mesh_box_property property,
                                                         void*                        out_result_ptr)
{
    _procedural_mesh_box* mesh_box_ptr = reinterpret_cast<_procedural_mesh_box*>(box);

    switch (property)
    {
        case PROCEDURAL_MESH_BOX_PROPERTY_NONINDEXED_BUFFER:
        {
            *reinterpret_cast<ral_buffer*>(out_result_ptr) = mesh_box_ptr->nonindexed_bo;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_NONINDEXED_BUFFER_NORMAL_DATA_OFFSET:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = mesh_box_ptr->nonindexed_bo_normal_data_offset;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_NONINDEXED_BUFFER_VERTEX_DATA_OFFSET:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = 0;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER_INDEX_DATA_OFFSET:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = mesh_box_ptr->indexed_bo_index_data_offset;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER_NORMAL_DATA_OFFSET:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = mesh_box_ptr->indexed_bo_normal_data_offset;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER_VERTEX_DATA_OFFSET:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = 0;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_N_TRIANGLES:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = mesh_box_ptr->n_triangles;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_N_VERTICES:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = mesh_box_ptr->n_points;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_RESTART_INDEX:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = mesh_box_ptr->primitive_restart_index;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural_mesh_box_property value");
        }
    }
}
