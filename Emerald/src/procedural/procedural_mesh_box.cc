/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "procedural/procedural_mesh_box.h"
#include "system/system_log.h"

/** BUFFER OBJECT STRUCTURE:
 *
 * 1) Vertex model-space locations
 * 2) Indexes
 * 3) Normals
 */
/* Internal type definitions */
typedef struct
{
    ogl_context                   context;
    _procedural_mesh_data_bitmask data;
    uint32_t                      n_horizontal;
    uint32_t                      n_vertical;
    system_hashed_ansi_string     name;

    GLuint arrays_bo_id;
    GLuint elements_bo_id;
    GLuint n_points; /* elements only */
    GLuint n_triangles; /* arrays only */
    GLuint primitive_restart_index;

    GLuint arrays_normals_offset;

    GLuint elements_indexes_offset;
    GLuint elements_normals_offset;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_box;

#ifdef _DEBUG
    PRIVATE void _procedural_mesh_box_verify_context_type(__in __notnull ogl_context);
#else
    #define _procedural_mesh_box_verify_context_type(x)
#endif

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_box, procedural_mesh_box, _procedural_mesh_box);


/* Private functions */
PRIVATE void _procedural_mesh_box_create_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _procedural_mesh_box*             mesh_box     = (_procedural_mesh_box*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Generate a buffer object to hold all data */
    entry_points->pGLGenBuffers(1, &mesh_box->elements_bo_id);

    /* Prepare point data. Points are organised, starting from top plane to bottom (Y axis corresponds to height). 2 points
     * correspond to borders. */
    uint32_t n_points_per_plane = (mesh_box->n_horizontal + 2) * (mesh_box->n_vertical + 2);
    uint32_t n_points           = n_points_per_plane * 6;
    float*   nonindexed_points  = new (std::nothrow) float[n_points * 3];
    float*   nonindexed_normals = new (std::nothrow) float[n_points * 3];

    ASSERT_ALWAYS_SYNC(nonindexed_points != NULL && nonindexed_normals != NULL, "Could not generate buffers for %d points", n_points);
    if (nonindexed_points != NULL && nonindexed_normals != NULL)
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
        for (uint32_t z = 0; z < mesh_box->n_vertical + 2; ++z)
        {
            for (uint32_t x = 0; x < mesh_box->n_horizontal + 2; ++x)
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
        for (uint32_t z = 0; z < mesh_box->n_vertical + 2; ++z)
        {
            for (uint32_t x = 0; x < mesh_box->n_horizontal + 2; ++x)
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
        for (uint32_t y = 0; y < mesh_box->n_vertical + 2; ++y)
        {
            for (uint32_t z = 0; z < mesh_box->n_horizontal + 2; ++z)
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
        for (uint32_t y = 0; y < mesh_box->n_vertical + 2; ++y)
        {
            for (uint32_t z = 0; z < mesh_box->n_horizontal + 2; ++z)
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
        for (uint32_t y = 0; y < mesh_box->n_vertical + 2; ++y)
        {
            for (uint32_t x = 0; x < mesh_box->n_horizontal + 2; ++x)
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
        for (uint32_t y = 0; y < mesh_box->n_vertical + 2; ++y)
        {
            for (uint32_t x = 0; x < mesh_box->n_horizontal + 2; ++x)
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
    ASSERT_ALWAYS_SYNC(n_points + 1 /* restart index */ <= 65535, "Too many points for BO to hold!");

    uint32_t  n_ordered_indexes         = 6 * (mesh_box->n_vertical + 1) * (mesh_box->n_horizontal + 2) * 2 + 6 * (mesh_box->n_vertical + 1); /* vertical restarts */
    GLushort* ordered_indexes           = new (std::nothrow) GLushort[n_ordered_indexes];
    GLushort* ordered_indexes_traveller = ordered_indexes;
    GLfloat*  ordered_normals           = new (std::nothrow) GLfloat [n_ordered_indexes * 3];
    GLfloat*  ordered_normals_traveller = ordered_normals;

    ASSERT_ALWAYS_SYNC(ordered_indexes != NULL, "Out of memory while allocating index array");
    ASSERT_ALWAYS_SYNC(ordered_normals != NULL, "Out of memory whiel allocating normals array");
    if (ordered_indexes != NULL && ordered_normals != NULL)
    {
        uint32_t assertion_check_cnt = 0;

        for (int n_plane = 0; n_plane < 6; ++n_plane)
        {
            for (uint32_t y = 0; y < mesh_box->n_vertical + 1; ++y)
            {
                int n_horizontal_indexes = (mesh_box->n_horizontal + 2);

                for (uint32_t n_patch = 0; n_patch < mesh_box->n_horizontal + 2; ++n_patch)
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

                        default:ASSERT_DEBUG_SYNC(false, "");
                    }
                }
                
                *ordered_indexes_traveller = mesh_box->primitive_restart_index; 

                ordered_indexes_traveller++;
                ordered_normals_traveller += 3;
                
                assertion_check_cnt++;
            }

        } /* for(n_plane) */

        ASSERT_DEBUG_SYNC(assertion_check_cnt == n_ordered_indexes, "Element indices counter is wrong!");
    }

    if (mesh_box->data & DATA_ARRAYS)
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

        if (triangles != NULL)
        {
            for (uint32_t n_index = 0; n_index < n_ordered_indexes; ++n_index)
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
                        ASSERT_DEBUG_SYNC(n_vertexes == 2, "");

                        /* Insert new vertex */
                        local_vertexes[6] = nonindexed_points[new_index*3  ];
                        local_vertexes[7] = nonindexed_points[new_index*3+1];
                        local_vertexes[8] = nonindexed_points[new_index*3+2];

                        local_normals[6] = nonindexed_normals[new_index*3  ];
                        local_normals[7] = nonindexed_normals[new_index*3+1];
                        local_normals[8] = nonindexed_normals[new_index*3+2];

                        /* Append a triangle */
                        memcpy(triangles_traveller, local_vertexes, 9 * sizeof(GLfloat) );
                        memcpy(normals_traveller,   local_normals,  9 * sizeof(GLfloat) );

                        triangles_traveller += 9;
                        normals_traveller   += 9;
                        n_triangles         ++;

                        /* Drop the oldest vertex to make space for the new one */
                        for (int n = 3; n < 9; ++n)
                        {
                            local_vertexes[n - 3] = local_vertexes[n];
                            local_normals [n - 3] = local_normals [n];
                        }
                    }                   
                }
            } /* for (uint32_t n_index = 0; n_index < n_ordered_indexes; ++n_index)*/

            /* Store the data. */
            mesh_box->arrays_normals_offset = n_triangles * 9 * sizeof(GLfloat);
            mesh_box->n_triangles           = n_triangles;

            /* Set buffer object contents */
            entry_points->pGLBindBuffer(GL_ARRAY_BUFFER, mesh_box->arrays_bo_id);
            entry_points->pGLBufferData(GL_ARRAY_BUFFER, mesh_box->arrays_normals_offset * 2, NULL, GL_STATIC_DRAW);

            entry_points->pGLBufferSubData(GL_ARRAY_BUFFER, 0,                               mesh_box->arrays_normals_offset, triangles);
            entry_points->pGLBufferSubData(GL_ARRAY_BUFFER, mesh_box->arrays_normals_offset, mesh_box->arrays_normals_offset, normals);

            /* Fine to release the buffers now */
            delete [] normals;
            delete [] triangles;
        }
    }

    if (mesh_box->data & DATA_ELEMENTS)
    {
        /* Set offsets. */
        mesh_box->elements_indexes_offset =                                     n_points      * 3 * sizeof(GLfloat);
        mesh_box->elements_normals_offset = mesh_box->elements_indexes_offset + n_ordered_indexes * sizeof(GLushort);

        /* Set buffer object contents */
        uint32_t ordered_normals_size = n_ordered_indexes * 3 * sizeof(GLfloat);

        entry_points->pGLBindBuffer   (GL_ARRAY_BUFFER, mesh_box->elements_bo_id);
        entry_points->pGLBufferData   (GL_ARRAY_BUFFER, mesh_box->elements_normals_offset + ordered_normals_size, NULL, GL_STATIC_DRAW);

        entry_points->pGLBufferSubData(GL_ARRAY_BUFFER, 0,                                  mesh_box->elements_indexes_offset,                                      nonindexed_points);
        entry_points->pGLBufferSubData(GL_ARRAY_BUFFER, mesh_box->elements_indexes_offset, (mesh_box->elements_normals_offset - mesh_box->elements_indexes_offset), ordered_indexes);
        entry_points->pGLBufferSubData(GL_ARRAY_BUFFER, mesh_box->elements_normals_offset,  ordered_normals_size,                                                   ordered_normals);
    }

    /* Update "number of points" to a value that will make sense to end-user */
    mesh_box->n_points = n_ordered_indexes;

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

    if (mesh_box->data & DATA_ARRAYS)
    {
        entry_points->pGLDeleteBuffers(1, &mesh_box->arrays_bo_id);
    }

    if (mesh_box->data & DATA_ELEMENTS)
    {
        entry_points->pGLDeleteBuffers(1, &mesh_box->elements_bo_id);
    }
}

/** TODO */
PRIVATE void _procedural_mesh_box_release(void* arg)
{
    _procedural_mesh_box* instance = (_procedural_mesh_box*) arg;

    ogl_context_request_callback_from_context_thread(instance->context, _procedural_mesh_box_release_renderer_callback, instance);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _procedural_mesh_box_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "procedural_mesh_box is only supported under GL contexts")
    }
#endif


/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(__in __notnull ogl_context                   context,
                                                                  __in           _procedural_mesh_data_bitmask data_bitmask,
                                                                  __in __notnull uint32_t                      n_horizontal,
                                                                  __in __notnull uint32_t                      n_vertical,
                                                                  __in __notnull system_hashed_ansi_string     name)
{
    _procedural_mesh_box_verify_context_type(context);

    /* Create the instance */
    _procedural_mesh_box* new_instance = new (std::nothrow) _procedural_mesh_box;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory while allocating space for box instance [%s]", system_hashed_ansi_string_get_buffer(name) );
    if (new_instance != NULL)
    {
        /* Cache input arguments */
        new_instance->context      = context;
        new_instance->data         = data_bitmask;
        new_instance->n_horizontal = n_horizontal;
        new_instance->n_vertical   = n_vertical;
        new_instance->name         = name;

        new_instance->arrays_bo_id            = -1;
        new_instance->elements_bo_id          = -1;
        new_instance->n_points                = -1;
        new_instance->n_triangles             = -1;
        new_instance->primitive_restart_index = -1;
        new_instance->arrays_normals_offset   = -1;
        new_instance->elements_indexes_offset = -1;
        new_instance->elements_normals_offset = -1;

        /* Call back renderer to continue */
        ogl_context_request_callback_from_context_thread(context, _procedural_mesh_box_create_renderer_callback, new_instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _procedural_mesh_box_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_BOX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (box)\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (procedural_mesh_box) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_arrays_bo_id(__in __notnull procedural_mesh_box mesh_box)
{
    return ((_procedural_mesh_box*) mesh_box)->arrays_bo_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_elements_bo_id(__in __notnull procedural_mesh_box mesh_box)
{
    return ((_procedural_mesh_box*) mesh_box)->elements_bo_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_box_get_arrays_bo_offsets(__in  __notnull procedural_mesh_box mesh_box,
                                                                  __out __notnull GLuint*             out_vertex_data_offset,
                                                                  __out __notnull GLuint*             out_normals_data_offset)
{
    _procedural_mesh_box* mesh_box_ptr = (_procedural_mesh_box*) mesh_box;

    *out_vertex_data_offset  = 0;
    *out_normals_data_offset = mesh_box_ptr->arrays_normals_offset;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_box_get_elements_bo_offsets(__in  __notnull procedural_mesh_box mesh_box,
                                                                    __out __notnull GLuint* out_vertex_data_offset,
                                                                    __out __notnull GLuint* out_elements_data_offset,
                                                                    __out __notnull GLuint* out_normals_data_offset)
{
    _procedural_mesh_box* mesh_box_ptr = (_procedural_mesh_box*) mesh_box;

    *out_vertex_data_offset   = 0;
    *out_elements_data_offset = mesh_box_ptr->elements_indexes_offset;
    *out_normals_data_offset  = mesh_box_ptr->elements_normals_offset;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_number_of_points(__in __notnull procedural_mesh_box box)
{
    _procedural_mesh_box* mesh_box_ptr = (_procedural_mesh_box*) box;

    return mesh_box_ptr->n_points;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_number_of_triangles(__in __notnull procedural_mesh_box box)
{
    _procedural_mesh_box* mesh_box_ptr = (_procedural_mesh_box*) box;

    return mesh_box_ptr->n_triangles;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_restart_index(__in __notnull procedural_mesh_box box)
{
    return ((_procedural_mesh_box*) box)->primitive_restart_index;
}