/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_texture.h"
#include "procedural/procedural_mesh_sphere.h"
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
    uint32_t                      n_latitude_splices;
    uint32_t                      n_longitude_splices;
    system_hashed_ansi_string     name;

    GLuint      arrays_bo_id;
    ogl_texture arrays_tbo;
    GLuint      elements_bo_id;
    GLuint      n_points; /* elements only */
    GLuint      n_triangles; /* arrays only */
    GLuint      primitive_restart_index;

    GLuint elements_indexes_offset;
    GLuint elements_vertexes_offset;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_sphere;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_sphere, procedural_mesh_sphere, _procedural_mesh_sphere);

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _procedural_mesh_sphere_verify_context_type(__in __notnull ogl_context);
#else
    #define _procedural_mesh_sphere_verify_context_type(x)
#endif

/* Private functions */
PRIVATE void _procedural_mesh_sphere_create_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    _procedural_mesh_sphere*                                  mesh_sphere      = (_procedural_mesh_sphere*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Generate a buffer object to hold all data */
    entry_points->pGLGenBuffers(1, &mesh_sphere->elements_bo_id);

    /* Prepare point data. Normals = points */
    float    theta_degrees_delta = 180.0f / float(mesh_sphere->n_latitude_splices - 1);
    float    phi_degrees_delta   = 360.0f / float(mesh_sphere->n_longitude_splices - 1);
    uint32_t n_theta_iterations  = mesh_sphere->n_latitude_splices;
    uint32_t n_phi_iterations    = mesh_sphere->n_longitude_splices;
    uint32_t n_points            = (n_phi_iterations) * (n_theta_iterations);
    float*   nonindexed_points   = new (std::nothrow) float[n_points * 3];

    ASSERT_ALWAYS_SYNC(nonindexed_points != NULL, "Could not generate buffer for %d points", n_points);
    if (nonindexed_points != NULL)
    {
        float* nonindexed_points_traveller = nonindexed_points;
        
        for (uint32_t n_phi_iteration = 0 ; n_phi_iteration < n_phi_iterations; ++n_phi_iteration)
        {
            float phi = DEG_TO_RAD(phi_degrees_delta) * n_phi_iteration;

            for (uint32_t n_theta_iteration = 0; n_theta_iteration < n_theta_iterations; ++n_theta_iteration)
            {
                float theta = (-3.14152965f * 0.5f) + DEG_TO_RAD(theta_degrees_delta) * n_theta_iteration;
                
                *nonindexed_points_traveller = cos(theta) * cos(phi); nonindexed_points_traveller++;
                *nonindexed_points_traveller = cos(theta) * sin(phi); nonindexed_points_traveller++;
                *nonindexed_points_traveller = sin(theta);            nonindexed_points_traveller++;
            }
        }

        /* Set other fields */
        mesh_sphere->n_points = n_points;
    }

    /* Prepare index data. */
    uint32_t  n_ordered_indexes         = (n_phi_iterations) * (n_theta_iterations) * 6;
    GLushort* ordered_indexes           = new (std::nothrow) GLushort[n_ordered_indexes];
    GLushort* ordered_indexes_traveller = ordered_indexes;

    ASSERT_ALWAYS_SYNC(ordered_indexes != NULL, "Out of memory while allocating index array");
    if (ordered_indexes != NULL)
    {
        for (uint32_t n_phi_iteration = 0 ; n_phi_iteration < n_phi_iterations; ++n_phi_iteration)
        {
            uint32_t next_n_phi_iteration = ((n_phi_iteration + 1) != n_phi_iterations ? (n_phi_iteration + 1) : 0);

            for (uint32_t n_theta_iteration = 0; n_theta_iteration < n_theta_iterations; ++n_theta_iteration)
            {
                /* Form a quad made of 2 triangles */
                uint32_t next_n_theta_iteration = ((n_theta_iteration + 1) != n_theta_iterations ? (n_theta_iteration + 1) : 0);

                *ordered_indexes_traveller = n_phi_iteration      * n_theta_iterations + n_theta_iteration;      ordered_indexes_traveller++;
                *ordered_indexes_traveller = next_n_phi_iteration * n_theta_iterations + next_n_theta_iteration; ordered_indexes_traveller++;
                *ordered_indexes_traveller = next_n_phi_iteration * n_theta_iterations + (n_theta_iteration);    ordered_indexes_traveller++;

                *ordered_indexes_traveller = n_phi_iteration      * n_theta_iterations + n_theta_iteration;      ordered_indexes_traveller++;
                *ordered_indexes_traveller = n_phi_iteration      * n_theta_iterations + next_n_theta_iteration; ordered_indexes_traveller++;
                *ordered_indexes_traveller = next_n_phi_iteration * n_theta_iterations + next_n_theta_iteration; ordered_indexes_traveller++;
            }
        }

        mesh_sphere->n_points = n_ordered_indexes;
    }

    if (mesh_sphere->data & DATA_ARRAYS)
    {
        /* Primitive restart doesn't make sense for array-based calls. We need to convert the data we have to triangles */
        uint32_t n_triangles         = (n_phi_iterations) * (n_theta_iterations) * 2; /* times 2 because we need to make up quads */
        GLfloat* triangles           = new (std::nothrow) GLfloat[n_triangles * 9]; /* 3 vertexes per triangle */
        GLfloat* triangles_traveller = triangles;

        if (triangles != NULL)
        {
            for (uint32_t n_index = 0; n_index < n_triangles * 3; ++n_index)
            {
                uint32_t new_index = ordered_indexes[n_index];

                ASSERT_DEBUG_SYNC(new_index < n_points, "Out of points");

                *triangles_traveller = nonindexed_points[new_index*3  ]; triangles_traveller++;
                *triangles_traveller = nonindexed_points[new_index*3+1]; triangles_traveller++;
                *triangles_traveller = nonindexed_points[new_index*3+2]; triangles_traveller++;
            } /* for (uint32_t n_index = 0; n_index < n_ordered_indexes; ++n_index)*/

            /* Store the data. */
            mesh_sphere->n_triangles = n_triangles;

            /* Set buffer object contents */
            entry_points->pGLGenBuffers(1, &mesh_sphere->arrays_bo_id);
            dsa_entry_points->pGLNamedBufferDataEXT(mesh_sphere->arrays_bo_id, n_triangles * 3 * 3 * sizeof(GLfloat), triangles, GL_STATIC_DRAW);

            /* Fine to release the buffers now */
            delete [] triangles;
        }
    }

    if (mesh_sphere->data & DATA_ELEMENTS)
    {
        /* Seems to be broken */
        ASSERT_DEBUG_SYNC(false, "");

        /* Set offsets. */
        const uint32_t indexes_size  = n_ordered_indexes * 3 * sizeof(unsigned short);
        const uint32_t vertices_size = n_points          * 3 * sizeof(float);

        mesh_sphere->elements_indexes_offset  = 0;
        mesh_sphere->elements_vertexes_offset = n_ordered_indexes * 3 * sizeof(unsigned short);

        /* Set buffer object contents */
        uint32_t ordered_vertexes_size = n_ordered_indexes * 3 * sizeof(GLfloat);

        dsa_entry_points->pGLNamedBufferDataEXT(mesh_sphere->elements_bo_id, indexes_size + vertices_size, NULL, GL_STATIC_DRAW);

        dsa_entry_points->pGLNamedBufferSubDataEXT(mesh_sphere->elements_bo_id, mesh_sphere->elements_indexes_offset,  indexes_size,  ordered_indexes);
        dsa_entry_points->pGLNamedBufferSubDataEXT(mesh_sphere->elements_bo_id, mesh_sphere->elements_vertexes_offset, vertices_size, nonindexed_points);

        /* Update "number of points" to a value that will make sense to end-user */
        mesh_sphere->n_points = n_ordered_indexes;
    }

    /* Okay, we're cool to release the buffers now */
    delete [] nonindexed_points;
    delete [] ordered_indexes;
}

/** TODO */
PRIVATE void _procedural_mesh_sphere_get_arrays_tbo_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    _procedural_mesh_sphere*                                  ptr              = (_procedural_mesh_sphere*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ptr->arrays_tbo = ogl_texture_create(context, system_hashed_ansi_string_create_by_merging_two_strings("Procedural mesh sphere texture ",
                                                                                                          system_hashed_ansi_string_get_buffer(ptr->name) ));

    dsa_entry_points->pGLTextureBufferEXT(ptr->arrays_tbo, GL_TEXTURE_BUFFER, GL_RGB32F, ptr->arrays_bo_id);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create a TBO out of sphere BO.");
}

/** TODO */
PRIVATE void _procedural_mesh_sphere_release_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _procedural_mesh_sphere*          mesh_sphere  = (_procedural_mesh_sphere*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (mesh_sphere->data & DATA_ARRAYS)
    {
        entry_points->pGLDeleteBuffers(1, &mesh_sphere->arrays_bo_id);
    }

    if (mesh_sphere->data & DATA_ELEMENTS)
    {
        entry_points->pGLDeleteBuffers(1, &mesh_sphere->elements_bo_id);
    }
}

/** TODO */
PRIVATE void _procedural_mesh_sphere_release(void* arg)
{
    _procedural_mesh_sphere* instance = (_procedural_mesh_sphere*) arg;

    ogl_context_request_callback_from_context_thread(instance->context, _procedural_mesh_sphere_release_renderer_callback, instance);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _procedural_mesh_sphere_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "procedural_mesh_sphere is only supported under GL contexts")
    }
#endif


/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_sphere procedural_mesh_sphere_create(__in __notnull ogl_context                   context,
                                                                        __in           _procedural_mesh_data_bitmask data_bitmask,
                                                                        __in __notnull uint32_t                      n_latitude_splices, /* number of latitude splices */
                                                                        __in __notnull uint32_t                      n_longitude_splices, /* number of longitude splices */
                                                                        __in __notnull system_hashed_ansi_string     name)
{
    _procedural_mesh_sphere_verify_context_type(context);

    /* Create the instance */
    _procedural_mesh_sphere* new_instance = new (std::nothrow) _procedural_mesh_sphere;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory while allocating space for sphere instance [%s]", system_hashed_ansi_string_get_buffer(name) );
    if (new_instance != NULL)
    {
        /* Cache input arguments */
        new_instance->context             = context;
        new_instance->data                = data_bitmask;
        new_instance->n_latitude_splices  = n_latitude_splices;
        new_instance->n_longitude_splices = n_longitude_splices;
        new_instance->name                = name;

        new_instance->arrays_bo_id             = -1;
        new_instance->arrays_tbo               = 0;
        new_instance->elements_bo_id           = -1;
        new_instance->n_points                 = -1;
        new_instance->n_triangles              = -1;
        new_instance->primitive_restart_index  = -1;
        new_instance->elements_indexes_offset  = -1;
        new_instance->elements_vertexes_offset = -1;

        /* Call back renderer to continue */
        ogl_context_request_callback_from_context_thread(context, _procedural_mesh_sphere_create_renderer_callback, new_instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _procedural_mesh_sphere_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_SPHERE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (sphere)\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (procedural_mesh_sphere) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_arrays_bo_id(__in __notnull procedural_mesh_sphere mesh_sphere)
{
    return ((_procedural_mesh_sphere*) mesh_sphere)->arrays_bo_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture procedural_mesh_sphere_get_arrays_tbo(__in __notnull procedural_mesh_sphere mesh_sphere)
{
    _procedural_mesh_sphere* ptr = (_procedural_mesh_sphere*) mesh_sphere;

    if (ptr->arrays_tbo == 0)
    {
        ogl_context_request_callback_from_context_thread(ptr->context, _procedural_mesh_sphere_get_arrays_tbo_renderer_callback, ptr);
    }

    return ptr->arrays_tbo;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_elements_bo_id(__in __notnull procedural_mesh_sphere mesh_sphere)
{
    return ((_procedural_mesh_sphere*) mesh_sphere)->elements_bo_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_arrays_bo_offsets(__in  __notnull procedural_mesh_sphere mesh_sphere,
                                                                     __out __notnull GLuint*                out_vertex_data_offset,
                                                                     __out __notnull GLuint*                out_normals_data_offset)
{
    _procedural_mesh_sphere* mesh_sphere_ptr = (_procedural_mesh_sphere*) mesh_sphere;

    *out_vertex_data_offset  = 0;
    *out_normals_data_offset = 0;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_elements_bo_offsets(__in  __notnull procedural_mesh_sphere mesh_sphere,
                                                                       __out __notnull GLuint*                out_vertex_data_offset,
                                                                       __out __notnull GLuint*                out_elements_data_offset,
                                                                       __out __notnull GLuint*                out_normals_data_offset)
{
    _procedural_mesh_sphere* mesh_sphere_ptr = (_procedural_mesh_sphere*) mesh_sphere;

    *out_vertex_data_offset   = mesh_sphere_ptr->elements_vertexes_offset;
    *out_elements_data_offset = mesh_sphere_ptr->elements_indexes_offset;
    *out_normals_data_offset  = 0;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_number_of_points(__in __notnull procedural_mesh_sphere sphere)
{
    _procedural_mesh_sphere* mesh_sphere_ptr = (_procedural_mesh_sphere*) sphere;

    return mesh_sphere_ptr->n_points;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_number_of_triangles(__in __notnull procedural_mesh_sphere sphere)
{
    _procedural_mesh_sphere* mesh_sphere_ptr = (_procedural_mesh_sphere*) sphere;

    return mesh_sphere_ptr->n_triangles  ;
}
