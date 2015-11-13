/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "procedural/procedural_mesh_circle.h"
#include "system/system_log.h"

/* Internal type definitions */
typedef struct
{
    ogl_buffers                   buffers; /* owned by ogl_context */
    ogl_context                   context;
    uint32_t                      n_segments;
    system_hashed_ansi_string     name;

    GLuint       arrays_bo_id;    /* owned by ogl_buffers - do NOT release with glDeleteBuffers() */
    unsigned int arrays_bo_size;
    unsigned int arrays_bo_start_offset;
    GLuint       n_vertices;

    REFCOUNT_INSERT_VARIABLES
} _procedural_mesh_circle;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_mesh_circle,
                               procedural_mesh_circle,
                              _procedural_mesh_circle);


/* Private functions */
PRIVATE void _procedural_mesh_circle_create_renderer_callback(ogl_context context,
                                                              void*       arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_limits*                              limits_ptr       = NULL;
    _procedural_mesh_circle*                                  mesh_circle_ptr  = (_procedural_mesh_circle*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* Generate the data. Since we want a triangle fan, the first vertex lands in (0, 0),
     * and then we iterate over vertices, uniformly distributed over the perimeter. We ensure
     * the very last vertex covers the first one we generated right after the one in the centre,
     * so that there's no visible crack in the generated mesh.
     */
    const unsigned int n_vertices         = (1 /* centre */ + mesh_circle_ptr->n_segments);
    const unsigned int data_size          = sizeof(float) * n_vertices * 2 /* x, y*/;
    float*             data_ptr           = new float[data_size / sizeof(float) ];
    float*             data_traveller_ptr = data_ptr;
    float              step               = 2.0f * 3.14152965f / float(mesh_circle_ptr->n_segments - 1);

    ASSERT_DEBUG_SYNC(data_ptr != NULL,
                      "Out of memory");

    *data_traveller_ptr = 0.0f; ++data_traveller_ptr;
    *data_traveller_ptr = 0.0f; ++data_traveller_ptr;

    for (unsigned int n_segment = 0;
                      n_segment < mesh_circle_ptr->n_segments;
                    ++n_segment)
    {
        *data_traveller_ptr = cos(float(n_segment) * step); ++data_traveller_ptr;
        *data_traveller_ptr = sin(float(n_segment) * step); ++data_traveller_ptr;
    } /* for (all segments) */

    ASSERT_DEBUG_SYNC( ((char*) data_traveller_ptr - (char*) data_ptr) == data_size,
                      "Buffer underflow/overflow detected");

    mesh_circle_ptr->arrays_bo_size = data_size;
    mesh_circle_ptr->n_vertices     = n_vertices;

    /* Store the data in buffer memory */
    ogl_buffers_allocate_buffer_memory(mesh_circle_ptr->buffers,
                                       mesh_circle_ptr->arrays_bo_size,
                                       RAL_BUFFER_MAPPABILITY_NONE,
                                       RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                       OGL_BUFFERS_FLAGS_NONE,
                                       &mesh_circle_ptr->arrays_bo_id,
                                       &mesh_circle_ptr->arrays_bo_start_offset);

    dsa_entry_points->pGLNamedBufferSubDataEXT(mesh_circle_ptr->arrays_bo_id,
                                               mesh_circle_ptr->arrays_bo_start_offset,
                                               mesh_circle_ptr->arrays_bo_size,
                                               data_ptr);

    /* Safe to release the data buffer at this point */
    delete [] data_ptr;
    data_ptr = NULL;
}

/** TODO */
PRIVATE void _procedural_mesh_circle_release_renderer_callback(ogl_context context, void* arg)
{
    _procedural_mesh_circle* mesh_circle_ptr = (_procedural_mesh_circle*) arg;

    if (mesh_circle_ptr->arrays_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(mesh_circle_ptr->buffers,
                                       mesh_circle_ptr->arrays_bo_id,
                                       mesh_circle_ptr->arrays_bo_start_offset);
    }
}

/** TODO */
PRIVATE void _procedural_mesh_circle_release(void* arg)
{
    _procedural_mesh_circle* instance_ptr = (_procedural_mesh_circle*) arg;

    ogl_context_request_callback_from_context_thread(instance_ptr->context,
                                                     _procedural_mesh_circle_release_renderer_callback,
                                                     instance_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API procedural_mesh_circle procedural_mesh_circle_create(ogl_context                   context,
                                                                        _procedural_mesh_data_bitmask data_bitmask,
                                                                        uint32_t                      n_segments,
                                                                        system_hashed_ansi_string     name)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(data_bitmask == DATA_BO_ARRAYS,
                      "data_bitmask argument must be set to DATA_BO_ARRAYS");
    ASSERT_DEBUG_SYNC(n_segments >= 4,
                      "Number of circle segments must be at least 4.");

    /* Create the instance */
    _procedural_mesh_circle* new_circle_ptr = new (std::nothrow) _procedural_mesh_circle;

    ASSERT_ALWAYS_SYNC(new_circle_ptr != NULL,
                       "Out of memory while allocating space for a procedural_mesh_circle instance [%s]",
                       system_hashed_ansi_string_get_buffer(name) );

    if (new_circle_ptr != NULL)
    {
        /* Cache input arguments */
        new_circle_ptr->arrays_bo_id           = -1;
        new_circle_ptr->arrays_bo_start_offset = -1;
        new_circle_ptr->context                = context;
        new_circle_ptr->n_segments             = n_segments;
        new_circle_ptr->name                   = name;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &new_circle_ptr->buffers);

        /* Call back renderer to continue */
        ogl_context_request_callback_from_context_thread(context,
                                                         _procedural_mesh_circle_create_renderer_callback,
                                                         new_circle_ptr);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_circle_ptr,
                                                       _procedural_mesh_circle_release,
                                                       OBJECT_TYPE_PROCEDURAL_MESH_CIRCLE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural meshes (circle)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (procedural_mesh_circle) new_circle_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void procedural_mesh_circle_get_property(procedural_mesh_circle           circle,
                                                            _procedural_mesh_circle_property property,
                                                            void*                            out_result)
{
    const _procedural_mesh_circle* circle_ptr = (_procedural_mesh_circle*) circle;

    switch (property)
    {
        case PROCEDURAL_MESH_CIRCLE_PROPERTY_ARRAYS_BO_ID:
        {
            *(GLuint*) out_result = circle_ptr->arrays_bo_id;

            break;
        }

        case PROCEDURAL_MESH_CIRCLE_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET:
        {
            *(GLuint*) out_result = circle_ptr->arrays_bo_start_offset;

            break;
        }

        case PROCEDURAL_MESH_CIRCLE_PROPERTY_N_VERTICES:
        {
            *(unsigned int*) out_result = circle_ptr->n_vertices;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _procedural_mesh_circle_property value");
        }
    } /* switch (property) */
}
