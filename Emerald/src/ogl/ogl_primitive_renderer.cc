/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_primitive_renderer.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include <sstream>

#define VS_UB_BINDING_ID      (0)
#define VS_VERTEX_DATA_VAA_ID (0)
#define VS_COLOR_DATA_VAA_ID  (1)

static const char* fs_body     = "#version 430 core\n"
                                 "\n"
                                 "flat in  vec4 fp_color;\n"
                                 "     out vec4 result;\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    result = fp_color;\n"
                                 "}\n";
static const char* vs_preamble = "#version 430 core\n"
                                 "\n";
static const char* vs_body     = "layout(std140, binding = 0) uniform UB\n"
                                 "{\n"
                                 "    layout(row_major) mat4 mvp;\n"
                                 "} data;\n"
                                 "\n"
                                 "layout(location = 0) in vec3 vertex;\n"
                                 "layout(location = 1) in vec4 color;\n"
                                 "\n"
                                 "flat out vec4 fp_color;\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    fp_color    = color;\n"
                                 "    gl_Position = data.mvp * vec4(vertex, 1.0);\n"
                                 "}\n";


/** Internal types */
typedef struct _ogl_primitive_renderer_dataset
{
    GLfloat            color_data[4];
    unsigned int       draw_first;
    unsigned int       instance_id;
    ral_primitive_type primitive_type;
    unsigned int       n_vertices;
    unsigned int       n_vertices_allocated;
    GLfloat*           vertex_data;

    _ogl_primitive_renderer_dataset()
    {
        draw_first           = -1;
        instance_id          = -1;
        n_vertices           = 0;
        n_vertices_allocated = 0;
        primitive_type       = RAL_PRIMITIVE_TYPE_UNKNOWN;
        vertex_data          = NULL;

        memset(color_data,
               0,
               sizeof(color_data) );
    }

    ~_ogl_primitive_renderer_dataset()
    {
        if (vertex_data != NULL)
        {
            delete [] vertex_data;

            vertex_data = NULL;
        }
    }

} _ogl_primitive_renderer_dataset;

typedef struct
{
    /* Data BO. The BO consists of:
     *
     * mat4x4                       - MVP matrix
     * [n_datasets] * vec4          - color data for subsequent datasets.
     * [n_datasets] * [vertex data] - vertex data for subsequent datasets.
     **/
    ral_buffer       bo;
    GLuint           bo_color_offset;
    unsigned char*   bo_data;
    unsigned int     bo_data_size;
    system_matrix4x4 bo_mvp;
    GLuint           bo_mvp_offset;
    GLuint           bo_storage_size;
    unsigned int     bo_vertex_offset;

    /* Tells if the BO storage needs to be re-uploaded */
    bool dirty;

    /* Locked whenever there's an ongoing draw request. */
    system_critical_section draw_cs;

    /* Draw call arguments */
    ogl_primitive_renderer_dataset_id* draw_dataset_ids;
    system_matrix4x4                   draw_mvp;
    unsigned int                       draw_n_dataset_ids;

    /* VAO id */
    GLuint vao_id;

    /* Submitted data sets. For simplicity we use a vector. If any dataset is released,
     * its descriptor will be released, but the pointer at a corresponding index in the
     * vector will be set to NULL. This is required to guarantee coherence of the IDs
     * across the application's lifetime.
     */
    system_resizable_vector datasets;

    /* Rendering program */
    ogl_program program;

    /* Rendering context */
    raGL_backend backend;
    ral_context  context;

    /* Cached object name */
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES

} _ogl_primitive_renderer;

/* Forward declarations */
PRIVATE void _ogl_primitive_renderer_draw_rendering_thread_callback    (ogl_context              context,
                                                                        void*                    user_arg);
PRIVATE void _ogl_primitive_renderer_init_program                      (_ogl_primitive_renderer* renderer_ptr);
PRIVATE void _ogl_primitive_renderer_init_vao                          (_ogl_primitive_renderer* renderer_ptr);
PRIVATE void _ogl_primitive_renderer_init_vao_rendering_thread_callback(ogl_context              context,
                                                                        void*                    user_arg);
PRIVATE void _ogl_primitive_renderer_release_rendering_thread_callback (ogl_context              context,
                                                                        void*                    user_arg);
PRIVATE void _ogl_primitive_renderer_release                           (void*                    line_strip_renderer);
PRIVATE void _ogl_primitive_renderer_update_bo_storage                 (ogl_context              context,
                                                                        _ogl_primitive_renderer* renderer_ptr);
PRIVATE void _ogl_primitive_renderer_update_data_buffer                (_ogl_primitive_renderer* renderer_ptr);
PRIVATE void _ogl_primitive_renderer_update_vao                        (ogl_context              context,
                                                                        _ogl_primitive_renderer* renderer_ptr);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_primitive_renderer,
                               ogl_primitive_renderer,
                              _ogl_primitive_renderer);


/** TODO */
PRIVATE void _ogl_primitive_renderer_draw_rendering_thread_callback(ogl_context context,
                                                                    void*       user_arg)
{
    ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    ogl_context_gl_entrypoints*                         entry_points     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* NOTE: draw_cs is locked while this call-back is being handled */
    _ogl_primitive_renderer* renderer_ptr = (_ogl_primitive_renderer*) user_arg;

    if (renderer_ptr->dirty)
    {
        _ogl_primitive_renderer_update_bo_storage(context,
                                                  renderer_ptr);
        _ogl_primitive_renderer_update_vao       (context,
                                                  renderer_ptr);

        ASSERT_DEBUG_SYNC(!renderer_ptr->dirty,
                          "Renderer's BO storage is still marked as dirty");
    }

    /* Update MVP if necessary */
    if (!system_matrix4x4_is_equal(renderer_ptr->bo_mvp,
                                   renderer_ptr->draw_mvp) )
    {
        ral_buffer_client_sourced_update_info bo_update_info;

        bo_update_info.data         = system_matrix4x4_get_row_major_data(renderer_ptr->draw_mvp);
        bo_update_info.data_size    = sizeof(float) * 16;
        bo_update_info.start_offset = renderer_ptr->bo_mvp_offset;

        ral_buffer_set_data_from_client_memory(renderer_ptr->bo,
                                               1, /* n_updates */
                                              &bo_update_info);

        system_matrix4x4_set_from_matrix4x4(renderer_ptr->bo_mvp,
                                            renderer_ptr->draw_mvp);
    }

    /* Draw line strips as requested */
    GLuint      bo_id           = 0;
    raGL_buffer bo_raGL         = NULL;
    GLuint      bo_start_offset = 0;

    raGL_backend_get_buffer(renderer_ptr->backend,
                            renderer_ptr->bo,
                  (void**) &bo_raGL);

    raGL_buffer_get_property(bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &bo_id);
    raGL_buffer_get_property(bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &bo_start_offset);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     VS_UB_BINDING_ID,
                                     bo_id,
                                     bo_start_offset,
                                     renderer_ptr->bo_storage_size);
    entry_points->pGLBindVertexArray(renderer_ptr->vao_id);
    entry_points->pGLUseProgram     (ogl_program_get_id(renderer_ptr->program) );

    /* TODO: Add support for indirect draw calls? */
    for (unsigned int n = 0;
                      n < renderer_ptr->draw_n_dataset_ids;
                    ++n)
    {
        _ogl_primitive_renderer_dataset* dataset_ptr = NULL;

        if (system_resizable_vector_get_element_at(renderer_ptr->datasets,
                                                   renderer_ptr->draw_dataset_ids[n],
                                                  &dataset_ptr) )
        {
            entry_points->pGLDrawArraysInstancedBaseInstance(raGL_utils_get_ogl_primive_type_for_ral_primitive_type(dataset_ptr->primitive_type),
                                                             dataset_ptr->draw_first,
                                                             dataset_ptr->n_vertices,
                                                             1, /* primcount */
                                                             dataset_ptr->instance_id);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve dataset descriptor");
        }
    } /* for (all datasets) */
}

/** TODO */
PRIVATE void _ogl_primitive_renderer_init_program(_ogl_primitive_renderer* renderer_ptr)
{
    /* Prepare the vertex shader body.
     *
     * NOTE: There used to be a #define here, which is why the break-down.
     */
    const char* vs_parts[] =
    {
        vs_preamble,
        vs_body
    };
    const unsigned int n_vs_parts = sizeof(vs_parts) / sizeof(vs_parts[0]);

    /* Prepare the objects */
    ogl_shader fs = NULL;
    ogl_shader vs = NULL;

    fs = ogl_shader_create(ral_context_get_gl_context(renderer_ptr->context),
                           RAL_SHADER_TYPE_FRAGMENT,
                           system_hashed_ansi_string_create_by_merging_two_strings("Line strip renderer FS ",
                                                                                   system_hashed_ansi_string_get_buffer(renderer_ptr->name) ));
    vs = ogl_shader_create(ral_context_get_gl_context(renderer_ptr->context),
                           RAL_SHADER_TYPE_VERTEX,
                           system_hashed_ansi_string_create_by_merging_two_strings("Line strip renderer VS ",
                                                                                   system_hashed_ansi_string_get_buffer(renderer_ptr->name) ));

    renderer_ptr->program = ogl_program_create(ral_context_get_gl_context(renderer_ptr->context),
                                               system_hashed_ansi_string_create_by_merging_two_strings("Line strip renderer program ",
                                                                                                       system_hashed_ansi_string_get_buffer(renderer_ptr->name) ));

    ogl_shader_set_body(fs,
                        system_hashed_ansi_string_create(fs_body) );
    ogl_shader_set_body(vs,
                        system_hashed_ansi_string_create_by_merging_strings(n_vs_parts,
                                                                            vs_parts) );

    ogl_program_attach_shader(renderer_ptr->program,
                              fs);
    ogl_program_attach_shader(renderer_ptr->program,
                              vs);

    if (!ogl_program_link(renderer_ptr->program) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not link line strip renderer program");
    }

    /* Good to release the shaders at this point */
    ogl_shader_release(fs);
    ogl_shader_release(vs);
}

/** TODO */
PRIVATE void _ogl_primitive_renderer_init_vao(_ogl_primitive_renderer* renderer_ptr)
{
    /* Request a call-back from the rendering trhread */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(renderer_ptr->context),
                                                     _ogl_primitive_renderer_init_vao_rendering_thread_callback,
                                                     renderer_ptr);
}

/* TODO */
PRIVATE void _ogl_primitive_renderer_init_vao_rendering_thread_callback(ogl_context context,
                                                                        void*       user_arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _ogl_primitive_renderer*         renderer_ptr = (_ogl_primitive_renderer*) user_arg;

    ogl_context_get_property(ral_context_get_gl_context(renderer_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Generate the VAO. */
    entry_points->pGLGenVertexArrays(1,
                                    &renderer_ptr->vao_id);

    /* We cannot configure the object at this point because the storage is not set up. */
}

/** TODO */
PRIVATE void _ogl_primitive_renderer_release_rendering_thread_callback(ogl_context context,
                                                                       void*       user_arg)
{
    ogl_context_gl_entrypoints* entrypoints  = NULL;
    _ogl_primitive_renderer*    instance_ptr = (_ogl_primitive_renderer*) user_arg;

    ogl_context_get_property(ral_context_get_gl_context(instance_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    if (instance_ptr->bo != NULL)
    {
        ral_context_delete_buffers(instance_ptr->context,
                                   1, /* n_buffers */
                                  &instance_ptr->bo);

        instance_ptr->bo = NULL;
    }

    if (instance_ptr->program != NULL)
    {
        ogl_program_release(instance_ptr->program);

        instance_ptr->program = NULL;
    }

    if (instance_ptr->vao_id != 0)
    {
        entrypoints->pGLDeleteVertexArrays(1,
                                          &instance_ptr->vao_id);

        instance_ptr->vao_id = 0;
    }
}

/** TODO */
PRIVATE void _ogl_primitive_renderer_release(void* line_strip_renderer)
{
    _ogl_primitive_renderer* instance_ptr = (_ogl_primitive_renderer*) line_strip_renderer;

    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(instance_ptr->context),
                                                     _ogl_primitive_renderer_release_rendering_thread_callback,
                                                     instance_ptr);

    if (instance_ptr->datasets != NULL)
    {
        _ogl_primitive_renderer_dataset* dataset_ptr = NULL;

        while (system_resizable_vector_pop(instance_ptr->datasets,
                                          &dataset_ptr) )
        {
            delete dataset_ptr;

            dataset_ptr = NULL;
        }
        system_resizable_vector_release(instance_ptr->datasets);
    } /* if (instance_ptr->datasets != NULL) */

    if (instance_ptr->draw_cs != NULL)
    {
        system_critical_section_release(instance_ptr->draw_cs);

        instance_ptr->draw_cs = NULL;
    }

    if (instance_ptr->bo_mvp != NULL)
    {
        system_matrix4x4_release(instance_ptr->bo_mvp);

        instance_ptr->bo_mvp = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_primitive_renderer_update_bo_storage(ogl_context              context,
                                                       _ogl_primitive_renderer* renderer_ptr)
{
    ral_buffer_client_sourced_update_info               bo_update_info;
    ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    ogl_context_gl_entrypoints*                         entry_points     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* If the data buffer is dirty, we need to update it at this point */
    if (renderer_ptr->dirty)
    {
        LOG_INFO("Performance warning: Flushing data buffer in a rendering thread");

        _ogl_primitive_renderer_update_data_buffer(renderer_ptr);

        ASSERT_DEBUG_SYNC(!renderer_ptr->dirty,
                          "Data buffer is still dirty after flushing");
    }

    /* Make sure we have a BO available and that it is capacious enough. */
    if (renderer_ptr->bo              != NULL &&
        renderer_ptr->bo_storage_size <  renderer_ptr->bo_data_size)
    {
        ral_context_delete_buffers(renderer_ptr->context,
                                   1, /* n_buffers */
                                  &renderer_ptr->bo);

        renderer_ptr->bo = NULL;
    }

    if (renderer_ptr->bo == NULL)
    {
        ral_buffer_create_info bo_create_info;

        LOG_INFO("Performance warning: Need to reallocate data BO storage");

        bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
        bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
        bo_create_info.size             = renderer_ptr->bo_data_size;
        bo_create_info.usage_bits       = RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bo_create_info.user_queue_bits  = 0xFFFFFFFF;

        ral_context_create_buffers(renderer_ptr->context,
                                   1, /* n_buffers */
                                  &bo_create_info,
                                  &renderer_ptr->bo);
    }

    bo_update_info.data         = renderer_ptr->bo_data;
    bo_update_info.data_size    = renderer_ptr->bo_data_size;
    bo_update_info.start_offset = 0;

    ral_buffer_set_data_from_client_memory(renderer_ptr->bo,
                                           1, /* n_updates */
                                          &bo_update_info);

    renderer_ptr->bo_storage_size = renderer_ptr->bo_data_size;

    /* The calls above reset the cached MVP */
    system_matrix4x4_set_to_float(renderer_ptr->bo_mvp,
                                  0.0f);
}

/** Please see header for specification */
PRIVATE void _ogl_primitive_renderer_update_data_buffer(_ogl_primitive_renderer* renderer_ptr)
{
    /* TODO: This is the simplest implementation possible. Consider optimizations */

    /* Determine how much memory we need to allocate */
    unsigned int       n_datasets       = 0;
    unsigned int       color_data_size  = 0;
    const unsigned int mvp_data_size    = sizeof(float) * 16;
    unsigned int       vertex_data_size = 0;

    system_resizable_vector_get_property(renderer_ptr->datasets,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_datasets);

    for (unsigned int n_item = 0;
                      n_item < n_datasets;
                    ++n_item)
    {
        _ogl_primitive_renderer_dataset* dataset_ptr = NULL;

        if (system_resizable_vector_get_element_at(renderer_ptr->datasets,
                                                   n_item,
                                                  &dataset_ptr) &&
            dataset_ptr != NULL)
        {
            /* Color data */
            color_data_size += sizeof(float) * 4;

            /* Vertex data */
            vertex_data_size += dataset_ptr->n_vertices * sizeof(float) * 3 /* components per vertex */;
        }
    } /* for (all dataset items) */

    /* Compute data offsets */
    unsigned int data_mvp_offset    = 0;
    unsigned int data_color_offset  = data_mvp_offset   + mvp_data_size;
    unsigned int data_vertex_offset = data_color_offset + color_data_size;

    renderer_ptr->bo_color_offset  = data_color_offset;
    renderer_ptr->bo_mvp_offset    = data_mvp_offset;
    renderer_ptr->bo_vertex_offset = data_vertex_offset;

    /* Allocate the memory */
    const unsigned int data_size = color_data_size + mvp_data_size + vertex_data_size;

    if (renderer_ptr->bo_data != NULL)
    {
        delete [] renderer_ptr->bo_data;

        renderer_ptr->bo_data = NULL;
    }

    renderer_ptr->bo_data_size = data_size;
    renderer_ptr->bo_data      = new unsigned char[data_size];

    ASSERT_ALWAYS_SYNC(renderer_ptr->bo_data != NULL,
                       "Out of memory");

    /* Cache the data */
    unsigned int current_color_offset  = data_color_offset;
    unsigned int current_vertex_offset = data_vertex_offset;
    unsigned int n_current_item        = 0;

    for (unsigned int n_item = 0;
                      n_item < n_datasets;
                    ++n_item)
    {
        _ogl_primitive_renderer_dataset* dataset_ptr = NULL;

        if (system_resizable_vector_get_element_at(renderer_ptr->datasets,
                                                   n_item,
                                                  &dataset_ptr) &&
            dataset_ptr != NULL)
        {
            dataset_ptr->draw_first  = (current_vertex_offset - data_vertex_offset) / sizeof(float) / 3;
            dataset_ptr->instance_id = n_current_item;

            memcpy((char*) renderer_ptr->bo_data + current_color_offset,
                   dataset_ptr->color_data,
                   sizeof(float) * 4);
            memcpy((char*) renderer_ptr->bo_data + current_vertex_offset,
                   dataset_ptr->vertex_data,
                   sizeof(float) * 3 * dataset_ptr->n_vertices);

            current_color_offset  += sizeof(float) * 4;
            current_vertex_offset += sizeof(float) * 3 * dataset_ptr->n_vertices;
            n_current_item        ++;
        }
    } /* for (all dataset items) */

    renderer_ptr->dirty = false;
}

/** TODO */
PRIVATE void _ogl_primitive_renderer_update_vao(ogl_context               context,
                                                _ogl_primitive_renderer* renderer_ptr)
{
    GLuint                            bo_id           = 0;
    raGL_buffer                       bo_raGL         = NULL;
    uint32_t                          bo_start_offset = -1;
    const ogl_context_gl_entrypoints* entry_points    = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    raGL_backend_get_buffer(renderer_ptr->backend,
                            renderer_ptr->bo,
                  (void**) &bo_raGL);

    raGL_buffer_get_property(bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &bo_id);
    raGL_buffer_get_property(bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &bo_start_offset);

    /* Sanity check */
    ASSERT_DEBUG_SYNC(renderer_ptr->vao_id != 0,
                      "VAO is not generated");

    entry_points->pGLBindBuffer     (GL_ARRAY_BUFFER,
                                     bo_id);
    entry_points->pGLBindVertexArray(renderer_ptr->vao_id);

    entry_points->pGLEnableVertexAttribArray(VS_COLOR_DATA_VAA_ID);
    entry_points->pGLEnableVertexAttribArray(VS_VERTEX_DATA_VAA_ID);

    entry_points->pGLVertexAttribPointer(VS_COLOR_DATA_VAA_ID,
                                         4, /* size */
                                         GL_FLOAT,
                                         GL_FALSE, /* normalized */
                                         0,        /* stride */
                                         (const GLvoid*) (intptr_t) (bo_start_offset + renderer_ptr->bo_color_offset) );
    entry_points->pGLVertexAttribPointer(VS_VERTEX_DATA_VAA_ID,
                                         3, /* size */
                                         GL_FLOAT,
                                         GL_FALSE, /* normalized */
                                         0,        /* stride */
                                         (const GLvoid*) (intptr_t) (bo_start_offset + renderer_ptr->bo_vertex_offset) );

    entry_points->pGLVertexAttribDivisor(VS_COLOR_DATA_VAA_ID,
                                         1);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_primitive_renderer_dataset_id ogl_primitive_renderer_add_dataset(ogl_primitive_renderer renderer,
                                                                                        ral_primitive_type     primitive_type,
                                                                                        unsigned int           n_vertices,
                                                                                        const float*           vertex_data,
                                                                                        const float*           rgb)
{
    _ogl_primitive_renderer*          renderer_ptr = (_ogl_primitive_renderer*) renderer;
    ogl_primitive_renderer_dataset_id result_id    = -1;

    /* Allocate new descriptor */
    _ogl_primitive_renderer_dataset* new_dataset_ptr = new (std::nothrow) _ogl_primitive_renderer_dataset;

    ASSERT_ALWAYS_SYNC(new_dataset_ptr != NULL,
                       "Out of memory");

    if (new_dataset_ptr == NULL)
    {
        goto end;
    }

    /* Fill it. The vertex data offset will be set during BO contents reconstruction */
    new_dataset_ptr->n_vertices_allocated = n_vertices;
    new_dataset_ptr->n_vertices           = n_vertices;
    new_dataset_ptr->primitive_type       = primitive_type;
    new_dataset_ptr->vertex_data          = new (std::nothrow) float[3 * n_vertices];

    ASSERT_ALWAYS_SYNC(new_dataset_ptr->vertex_data != NULL,
                       "Out of memory");

    if (new_dataset_ptr->vertex_data == NULL)
    {
        goto end;
    }

    memcpy(new_dataset_ptr->color_data,
           rgb,
           sizeof(float) * 4);

    memcpy(new_dataset_ptr->vertex_data,
           vertex_data,
           3 * n_vertices * sizeof(float) );

    system_critical_section_enter(renderer_ptr->draw_cs);
    {
        /* Allocate an unique result ID */
        system_resizable_vector_get_property(renderer_ptr->datasets,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &result_id);

        system_resizable_vector_push(renderer_ptr->datasets,
                                     new_dataset_ptr);

        /* Mark the renderer as dirty, so that the BO storage is refilled upon next draw() */
        renderer_ptr->dirty = true;
    }
    system_critical_section_leave(renderer_ptr->draw_cs);

end:
    if (result_id == -1)
    {
        if (new_dataset_ptr != NULL)
        {
            delete new_dataset_ptr;

            new_dataset_ptr = NULL;
        }
    } /* if (result_id == -1) */

    ASSERT_DEBUG_SYNC(result_id != -1,
                      "Function failed");

    return result_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_primitive_renderer_change_dataset_data(ogl_primitive_renderer            renderer,
                                                                   ogl_primitive_renderer_dataset_id dataset_id,
                                                                   unsigned int                      n_vertices,
                                                                   const float*                      vertex_data)
{
    _ogl_primitive_renderer* renderer_ptr = (_ogl_primitive_renderer*) renderer;

    system_critical_section_enter(renderer_ptr->draw_cs);

    /* Retrieve the dataset descriptor */
    _ogl_primitive_renderer_dataset* dataset_ptr = NULL;

    if (!system_resizable_vector_get_element_at(renderer_ptr->datasets,
                                                dataset_id,
                                               &dataset_ptr) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Invalid dataset id");

        goto end;
    }

    if (dataset_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Requested dataset has been deleted");

        goto end;
    }

    /* Update the descriptor */
    if (dataset_ptr->n_vertices_allocated < n_vertices)
    {
        /* Need to reallocate the table.. */
        if (dataset_ptr->vertex_data != NULL)
        {
            delete [] dataset_ptr->vertex_data;

            dataset_ptr->vertex_data = NULL;
        }

        dataset_ptr->vertex_data = new (std::nothrow) GLfloat[n_vertices * 3];

        ASSERT_ALWAYS_SYNC(dataset_ptr->vertex_data != NULL,
                           "Out of memory");

        if (dataset_ptr->vertex_data == NULL)
        {
            goto end;
        }

        dataset_ptr->n_vertices_allocated = n_vertices;
    }

    /* Copy the data */
    dataset_ptr->n_vertices = n_vertices;

    memcpy(dataset_ptr->vertex_data,
           vertex_data,
           sizeof(float) * n_vertices * 3);

    /* Mark the renderer BO as dirty */
    renderer_ptr->dirty = true;

end:
    system_critical_section_leave(renderer_ptr->draw_cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_primitive_renderer ogl_primitive_renderer_create(ral_context               context,
                                                                        system_hashed_ansi_string name)
{
    /* Spawn the new instance */
    _ogl_primitive_renderer* renderer_ptr = new (std::nothrow) _ogl_primitive_renderer;

    ASSERT_ALWAYS_SYNC(renderer_ptr != NULL,
                       "Out of memory while allocating line strip renderer.");

    if (renderer_ptr != NULL)
    {
        memset(renderer_ptr,
               0,
               sizeof(*renderer_ptr) );

        renderer_ptr->bo_mvp   = system_matrix4x4_create       ();
        renderer_ptr->context  = context; /* DO NOT retain the context - otherwise you'll get a circular dependency! */
        renderer_ptr->datasets = system_resizable_vector_create(4 /* capacity */);
        renderer_ptr->dirty    = true;
        renderer_ptr->name     = name;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_BACKEND,
                                &renderer_ptr->backend);

        _ogl_primitive_renderer_init_program(renderer_ptr);
        _ogl_primitive_renderer_init_vao    (renderer_ptr);

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(renderer_ptr,
                                                       _ogl_primitive_renderer_release,
                                                       OBJECT_TYPE_OGL_PRIMITIVE_RENDERER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Primitive Renderers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_primitive_renderer) renderer_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_primitive_renderer_delete_dataset(ogl_primitive_renderer            renderer,
                                                              ogl_primitive_renderer_dataset_id dataset_id)
{
    _ogl_primitive_renderer* renderer_ptr = (_ogl_primitive_renderer*) renderer;
    bool                      result       = true;

    /* Identify the dataset */
    system_critical_section_enter(renderer_ptr->draw_cs);
    {
        _ogl_primitive_renderer_dataset* dataset_ptr = NULL;

        if (!system_resizable_vector_get_element_at(renderer_ptr->datasets,
                                                    dataset_id,
                                                   &dataset_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find dataset with id [%d]",
                              dataset_id);

            result = false;
            goto end;
        }

        /* Deallocate the dataset */
        if (dataset_ptr == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Requested dataset has already been deallocated");

            result = false;
            goto end;
        }

        delete dataset_ptr;
        dataset_ptr = NULL;

        /* Mark the entry as deallocated */
        system_resizable_vector_set_element_at(renderer_ptr->datasets,
                                               dataset_id,
                                               NULL);
    }

end:
    system_critical_section_leave(renderer_ptr->draw_cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_primitive_renderer_draw(ogl_primitive_renderer             renderer,
                                                    system_matrix4x4                   mvp,
                                                    unsigned int                       n_dataset_ids,
                                                    ogl_primitive_renderer_dataset_id* dataset_ids)
{
    /* Store the draw call arguments */
    _ogl_primitive_renderer* renderer_ptr = (_ogl_primitive_renderer*) renderer;

    system_critical_section_enter(renderer_ptr->draw_cs);
    {
        renderer_ptr->draw_dataset_ids   = dataset_ids;
        renderer_ptr->draw_mvp           = mvp;
        renderer_ptr->draw_n_dataset_ids = n_dataset_ids;

        /* Switch to the rendering context */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(renderer_ptr->context),
                                                         _ogl_primitive_renderer_draw_rendering_thread_callback,
                                                         renderer_ptr);
    }
    system_critical_section_leave(renderer_ptr->draw_cs);
}

