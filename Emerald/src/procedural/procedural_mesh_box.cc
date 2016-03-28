/**
 *
 * Emerald (kbi/elude @2012-2015)
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
    ral_context                   context;
    _procedural_mesh_data_bitmask data;
    uint32_t                      n_horizontal;
    uint32_t                      n_vertical;
    system_hashed_ansi_string     name;

    ral_buffer  arrays_bo;
    GLuint      arrays_bo_normals_offset; /* does NOT include start offset */

    ral_buffer   elements_bo;
    unsigned int elements_bo_indexes_offset; /* does NOT include start offset */
    unsigned int elements_bo_normals_offset; /* does NOT include start offset */

    GLuint n_points;    /* elements only */
    GLuint n_triangles; /* arrays only */
    GLuint primitive_restart_index;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_box;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_box,
                               procedural_mesh_box,
                              _procedural_mesh_box);


/* Private functions */
PRIVATE void _procedural_mesh_box_create_renderer_callback(ogl_context context,
                                                           void*       arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* entry_points_dsa = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    _procedural_mesh_box*                                     mesh_box         = (_procedural_mesh_box*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entry_points_dsa);

    /* Prepare point data. Points are organised, starting from top plane to bottom (Y axis corresponds to height). 2 points
     * correspond to borders. */
    uint32_t n_points_per_plane = (mesh_box->n_horizontal + 2) * (mesh_box->n_vertical + 2);
    uint32_t n_points           = n_points_per_plane * 6;
    float*   nonindexed_points  = new (std::nothrow) float[n_points * 3];
    float*   nonindexed_normals = new (std::nothrow) float[n_points * 3];

    ASSERT_ALWAYS_SYNC(nonindexed_points  != NULL &&
                       nonindexed_normals != NULL,
                       "Could not generate buffers for %d points",
                       n_points);

    if (nonindexed_points != NULL &&
        nonindexed_normals != NULL)
    {
        float* nonindexed_normals_traveller = nonindexed_normals;
        float* nonindexed_points_traveller  = nonindexed_points;

        /* Bottom plane:
         *
         * B-----C       +---->x
         * |     |       |
         * |     |       |
         * A-----D       v z
         */
        for (uint32_t z = 0;
                      z < mesh_box->n_vertical + 2;
                    ++z)
        {
            for (uint32_t x = 0;
                          x < mesh_box->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(mesh_box->n_horizontal + 1);
                float pz = float(z) / float(mesh_box->n_vertical   + 1);

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
                      z < mesh_box->n_vertical + 2;
                    ++z)
        {
            for (uint32_t x = 0;
                          x < mesh_box->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(mesh_box->n_horizontal + 1);
                float pz = float(z) / float(mesh_box->n_vertical   + 1);

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
                      y < mesh_box->n_vertical + 2;
                    ++y)
        {
            for (uint32_t z = 0;
                          z < mesh_box->n_horizontal + 2;
                        ++z)
            {
                float pz = float(z) / float(mesh_box->n_horizontal + 1);
                float py = float(y) / float(mesh_box->n_vertical   + 1);

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
                      y < mesh_box->n_vertical + 2;
                    ++y)
        {
            for (uint32_t z = 0;
                          z < mesh_box->n_horizontal + 2;
                        ++z)
            {
                float pz = float(z) / float(mesh_box->n_horizontal + 1);
                float py = float(y) / float(mesh_box->n_vertical   + 1);

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
                      y < mesh_box->n_vertical + 2;
                    ++y)
        {
            for (uint32_t x = 0;
                          x < mesh_box->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(mesh_box->n_horizontal + 1);
                float py = float(y) / float(mesh_box->n_vertical   + 1);

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
                      y < mesh_box->n_vertical + 2;
                    ++y)
        {
            for (uint32_t x = 0;
                          x < mesh_box->n_horizontal + 2;
                        ++x)
            {
                float px = float(x) / float(mesh_box->n_horizontal + 1);
                float py = float(y) / float(mesh_box->n_vertical   + 1);

                *nonindexed_points_traveller = px; nonindexed_points_traveller++;
                *nonindexed_points_traveller = py; nonindexed_points_traveller++;
                *nonindexed_points_traveller = 1;  nonindexed_points_traveller++;

                *nonindexed_normals_traveller = 0; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 0; nonindexed_normals_traveller ++;
                *nonindexed_normals_traveller = 1; nonindexed_normals_traveller ++;
            }
        }

        /* Set other fields */
        mesh_box->n_points                = n_points;
        mesh_box->primitive_restart_index = n_points + 1;
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

    uint32_t  n_ordered_indexes         = 6 * (mesh_box->n_vertical + 1) * (mesh_box->n_horizontal + 2) * 2 +
                                          6 * (mesh_box->n_vertical + 1); /* vertical restarts */
    GLushort* ordered_indexes           = new (std::nothrow) GLushort[n_ordered_indexes];
    GLushort* ordered_indexes_traveller = ordered_indexes;
    GLfloat*  ordered_normals           = new (std::nothrow) GLfloat [n_ordered_indexes * 3];
    GLfloat*  ordered_normals_traveller = ordered_normals;

    ASSERT_ALWAYS_SYNC(ordered_indexes != NULL,
                       "Out of memory while allocating index array");
    ASSERT_ALWAYS_SYNC(ordered_normals != NULL,
                       "Out of memory whiel allocating normals array");

    if (ordered_indexes != NULL &&
        ordered_normals != NULL)
    {
        uint32_t assertion_check_cnt = 0;

        for (int n_plane = 0;
                 n_plane < 6;
               ++n_plane)
        {
            for (uint32_t y = 0;
                          y < mesh_box->n_vertical + 1;
                        ++y)
            {
                int n_horizontal_indexes = (mesh_box->n_horizontal + 2);

                for (uint32_t n_patch = 0;
                              n_patch < mesh_box->n_horizontal + 2;
                            ++n_patch)
                {
                    *ordered_indexes_traveller = y * n_horizontal_indexes + n_patch + n_plane * n_points_per_plane + n_horizontal_indexes; ordered_indexes_traveller++;
                    *ordered_indexes_traveller = y * n_horizontal_indexes + n_patch + n_plane * n_points_per_plane;                        ordered_indexes_traveller++;

                    assertion_check_cnt += 2;

                    switch (n_plane)
                    {
                        case 0:
                        {
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = 1; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = 1; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;

                            break;
                        }

                        case 1:
                        {
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = -1; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = -1; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;

                            break;
                        }

                        case 2:
                        {
                            ordered_normals_traveller[0] = -1;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;
                            ordered_normals_traveller[0] = -1;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;

                            break;
                        }

                        case 3:
                        {
                            ordered_normals_traveller[0] = 1;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;
                            ordered_normals_traveller[0] = 1;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = 0;  ordered_normals_traveller += 3;

                            break;
                        }

                        case 4:
                        {
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = -1;  ordered_normals_traveller += 3;
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = -1;  ordered_normals_traveller += 3;

                            break;
                        }

                        case 5:
                        {
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = 1;  ordered_normals_traveller += 3;
                            ordered_normals_traveller[0] = 0;  ordered_normals_traveller[1] = 0; ordered_normals_traveller[2] = 1;  ordered_normals_traveller += 3;

                            break;
                        }

                        default:ASSERT_DEBUG_SYNC(false,
                                                  "");
                    }
                }

                *ordered_indexes_traveller = mesh_box->primitive_restart_index; 

                ordered_indexes_traveller++;
                ordered_normals_traveller += 3;

                assertion_check_cnt++;
            }
        } /* for(n_plane) */

        ASSERT_DEBUG_SYNC(assertion_check_cnt == n_ordered_indexes,
                          "Element indices counter is wrong!");
    }

    if (mesh_box->data & DATA_BO_ARRAYS)
    {
        /* Primitive restart doesn't make sense for array-based calls. We need to convert the data we have to triangles */
        GLfloat* normals             = new (std::nothrow) GLfloat[n_ordered_indexes * 9];
        GLfloat* normals_traveller   = normals;
        GLfloat* triangles           = new (std::nothrow) GLfloat[n_ordered_indexes * 9];
        GLfloat* triangles_traveller = triangles;
        uint32_t n_triangles         = 0;
        float    local_vertexes[9]   = {0};
        float    local_normals [9]   = {0};
        uint32_t n_vertexes          = 0;

        if (normals   != NULL &&
            triangles != NULL)
        {
            for (uint32_t n_index = 0;
                          n_index < n_ordered_indexes;
                        ++n_index)
            {
                if (ordered_indexes[n_index] == mesh_box->primitive_restart_index)
                {
                    /* New triangle strip starts */
                    n_vertexes = 0;
                }
                else
                {
                    uint32_t new_index = ordered_indexes[n_index];

                    if (n_vertexes < 2) /* 0 or 1 vertex is available - can't form a triangle */
                    {
                        local_vertexes[3*n_vertexes  ] = nonindexed_points[new_index*3  ];
                        local_vertexes[3*n_vertexes+1] = nonindexed_points[new_index*3+1];
                        local_vertexes[3*n_vertexes+2] = nonindexed_points[new_index*3+2];

                        local_normals [3*n_vertexes  ] = nonindexed_normals[new_index*3  ];
                        local_normals [3*n_vertexes+1] = nonindexed_normals[new_index*3+1];
                        local_normals [3*n_vertexes+2] = nonindexed_normals[new_index*3+2];

                        n_vertexes++;
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(n_vertexes == 2,
                                          "");

                        /* Insert new vertex */
                        local_vertexes[6] = nonindexed_points[new_index*3  ];
                        local_vertexes[7] = nonindexed_points[new_index*3+1];
                        local_vertexes[8] = nonindexed_points[new_index*3+2];

                        local_normals[6] = nonindexed_normals[new_index*3  ];
                        local_normals[7] = nonindexed_normals[new_index*3+1];
                        local_normals[8] = nonindexed_normals[new_index*3+2];

                        /* Append a triangle */
                        memcpy(triangles_traveller,
                               local_vertexes,
                               9 * sizeof(GLfloat) );
                        memcpy(normals_traveller,
                               local_normals,
                               9 * sizeof(GLfloat) );

                        triangles_traveller += 9;
                        normals_traveller   += 9;
                        n_triangles         ++;

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
            } /* for (uint32_t n_index = 0; n_index < n_ordered_indexes; ++n_index)*/

            /* Store the data. */
            ral_buffer_create_info                 arrays_bo_create_info;
            ral_buffer_client_sourced_update_info  arrays_bo_normals_update_info;
            ral_buffer_client_sourced_update_info  arrays_bo_triangles_update_info;

            mesh_box->arrays_bo_normals_offset = n_triangles * 9 * sizeof(GLfloat);
            mesh_box->n_triangles              = n_triangles;

            arrays_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
            arrays_bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
            arrays_bo_create_info.size             = mesh_box->arrays_bo_normals_offset * 2;
            arrays_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            arrays_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

            arrays_bo_normals_update_info.data         = normals;
            arrays_bo_normals_update_info.data_size    = mesh_box->arrays_bo_normals_offset;
            arrays_bo_normals_update_info.start_offset = mesh_box->arrays_bo_normals_offset;

            arrays_bo_triangles_update_info.data         = triangles;
            arrays_bo_triangles_update_info.data_size    = mesh_box->arrays_bo_normals_offset;
            arrays_bo_triangles_update_info.start_offset = 0;

            std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > arrays_bo_updates;

            arrays_bo_updates.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&arrays_bo_triangles_update_info,
                                                                                               NullDeleter<ral_buffer_client_sourced_update_info>() ));
            arrays_bo_updates.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&arrays_bo_normals_update_info,
                                                                                               NullDeleter<ral_buffer_client_sourced_update_info>() ));

            ral_context_create_buffers(mesh_box->context,
                                       1, /* n_buffers */
                                      &arrays_bo_create_info,
                                      &mesh_box->arrays_bo);

            ral_buffer_set_data_from_client_memory(mesh_box->arrays_bo,
                                                   arrays_bo_updates,
                                                   false, /* async               */
                                                   true   /* sync_other_contexts */);

            /* Fine to release the buffers now */
            delete [] normals;
            delete [] triangles;
        } /* if (normals != NULL && triangles != NULL) */
    } /* if (mesh_box->data & DATA_BO_ARRAYS) */

    if (mesh_box->data & DATA_BO_ELEMENTS)
    {
        uint32_t ordered_normals_size = n_ordered_indexes * 3 * sizeof(GLfloat);

        /* Set offsets. */
        ral_buffer_create_info                                                elements_create_info;
        ral_buffer_client_sourced_update_info                                 elements_update_info[3];
        std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info > > elements_update_info_ptrs;

        elements_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
        elements_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
        elements_create_info.size             = mesh_box->elements_bo_normals_offset + ordered_normals_size;
        elements_create_info.usage_bits       = RAL_BUFFER_USAGE_INDEX_BUFFER_BIT;
        elements_create_info.user_queue_bits  = 0xFFFFFFFF;

        ral_context_create_buffers(mesh_box->context,
                                   1, /* n_buffers */
                                   &elements_create_info,
                                   &mesh_box->elements_bo);

        mesh_box->elements_bo_indexes_offset =                                        n_points      * 3 * sizeof(GLfloat);
        mesh_box->elements_bo_normals_offset = mesh_box->elements_bo_indexes_offset + n_ordered_indexes * sizeof(GLushort);

        /* Set buffer object contents */
        elements_update_info[0].data         = nonindexed_points;
        elements_update_info[0].data_size    = mesh_box->elements_bo_indexes_offset;
        elements_update_info[0].start_offset = 0;

        elements_update_info[1].data         = ordered_indexes;
        elements_update_info[1].data_size    = (mesh_box->elements_bo_normals_offset - mesh_box->elements_bo_indexes_offset);
        elements_update_info[1].start_offset = mesh_box->elements_bo_indexes_offset;

        elements_update_info[2].data         = ordered_normals;
        elements_update_info[2].data_size    = ordered_normals_size;
        elements_update_info[2].start_offset = mesh_box->elements_bo_normals_offset;

        elements_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(elements_update_info + 0,
                                                                                                   NullDeleter<ral_buffer_client_sourced_update_info>() ));
        elements_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(elements_update_info + 1,
                                                                                                   NullDeleter<ral_buffer_client_sourced_update_info>() ));
        elements_update_info_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(elements_update_info + 2,
                                                                                                   NullDeleter<ral_buffer_client_sourced_update_info>() ));

        ral_buffer_set_data_from_client_memory(mesh_box->elements_bo,
                                               elements_update_info_ptrs,
                                               false, /* async               */
                                               true   /* sync_other_contexts */);
    } /* if (mesh_box->data & DATA_BO_ELEMENTS) */

    /* Update "number of points" to a value that will make sense to end-user */
    if (mesh_box->data & DATA_BO_ELEMENTS)
    {
        mesh_box->n_points = n_ordered_indexes;
    }
    else
    {
        mesh_box->n_points = mesh_box->n_triangles * 3;
    }

    /* Okay, we're cool to release the buffers now */
    delete [] nonindexed_points;
    delete [] nonindexed_normals;
    delete [] ordered_normals;
    delete [] ordered_indexes;
}

/** TODO */
PRIVATE void _procedural_mesh_box_release_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _procedural_mesh_box*             mesh_box     = (_procedural_mesh_box*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (mesh_box->data & DATA_BO_ARRAYS)
    {
        ral_context_delete_objects(mesh_box->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   (const void**) &mesh_box->arrays_bo);

        mesh_box->arrays_bo                = NULL;
        mesh_box->arrays_bo_normals_offset = -1;
    } /* if (mesh_box->data & DATA_BO_ARRAYS) */

    if (mesh_box->data & DATA_BO_ELEMENTS)
    {
        ral_context_delete_objects(mesh_box->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   (const void**) &mesh_box->elements_bo);

        mesh_box->elements_bo                = NULL;
        mesh_box->elements_bo_indexes_offset = -1;
        mesh_box->elements_bo_normals_offset = -1;
    } /* if (mesh_box->data & DATA_BO_ELEMENTS) */
}

/** TODO */
PRIVATE void _procedural_mesh_box_release(void* arg)
{
    _procedural_mesh_box* instance = (_procedural_mesh_box*) arg;

    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(instance->context),
                                                     _procedural_mesh_box_release_renderer_callback,
                                                     instance);
}

/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(ral_context                   context,
                                                                  _procedural_mesh_data_bitmask data_bitmask,
                                                                  uint32_t                      n_horizontal,
                                                                  uint32_t                      n_vertical,
                                                                  system_hashed_ansi_string     name)
{
    /* Create the instance */
    _procedural_mesh_box* new_instance = new (std::nothrow) _procedural_mesh_box;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory while allocating space for box instance [%s]",
                       system_hashed_ansi_string_get_buffer(name) );

    if (new_instance != NULL)
    {
        /* Cache input arguments */
        new_instance->context      = context;
        new_instance->data         = data_bitmask;
        new_instance->n_horizontal = n_horizontal;
        new_instance->n_vertical   = n_vertical;
        new_instance->name         = name;

        new_instance->arrays_bo                  = NULL;
        new_instance->arrays_bo_normals_offset   = -1;
        new_instance->elements_bo                = NULL;
        new_instance->elements_bo_indexes_offset = -1;
        new_instance->elements_bo_normals_offset = -1;
        new_instance->n_points                   = -1;
        new_instance->n_triangles                = -1;
        new_instance->primitive_restart_index    = -1;

        /* Call back renderer to continue */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                         _procedural_mesh_box_create_renderer_callback,
                                                         new_instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                       _procedural_mesh_box_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_BOX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (box)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (procedural_mesh_box) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_box_get_property(procedural_mesh_box          box,
                                                         procedural_mesh_box_property property,
                                                         void*                        out_result)
{
    _procedural_mesh_box* mesh_box_ptr = (_procedural_mesh_box*) box;

    switch (property)
    {
        case PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_RAL:
        {
            *(ral_buffer*) out_result = mesh_box_ptr->arrays_bo;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_NORMALS_DATA_OFFSET:
        {
            *(unsigned int*) out_result = mesh_box_ptr->arrays_bo_normals_offset;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET:
        {
            *(unsigned int*) out_result = 0;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_INDICES_DATA_OFFSET:
        {
            *(unsigned int*) out_result = mesh_box_ptr->elements_bo_indexes_offset;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_NORMALS_DATA_OFFSET:
        {
            *(unsigned int*) out_result = mesh_box_ptr->elements_bo_normals_offset;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_VERTEX_DATA_OFFSET:
        {
            *(unsigned int*) out_result = 0;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_N_TRIANGLES:
        {
            *(unsigned int*) out_result = mesh_box_ptr->n_triangles;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_N_VERTICES:
        {
            *(unsigned int*) out_result = mesh_box_ptr->n_points;

            break;
        }

        case PROCEDURAL_MESH_BOX_PROPERTY_RESTART_INDEX:
        {
            *(GLuint*) out_result = mesh_box_ptr->primitive_restart_index;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural_mesh_box_property value");
        }
    } /* switch (property) */
}
