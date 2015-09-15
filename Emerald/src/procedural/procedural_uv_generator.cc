/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ssb.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_shader.h"
#include "procedural/procedural_uv_generator.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definition */
typedef struct _procedural_uv_generator
{
    ogl_context                  context;
    procedural_uv_generator_type type;

    ogl_shader      generator_cs;
    ogl_program     generator_po;
    ogl_program_ssb input_data_ssb;
    GLuint          input_data_ssb_indexed_bp;
    ogl_program_ub  input_params_ub;
    GLuint          input_params_ub_bo_id;
    unsigned int    input_params_ub_bo_size;
    unsigned int    input_params_ub_bo_start_offset;
    GLint           input_params_ub_normal_data_stride_block_offset;
    GLint           input_params_ub_start_offset_block_offset;
    ogl_program_ub  n_normals_ub;
    GLuint          n_normals_ub_bo_id;
    unsigned int    n_normals_ub_bo_start_offset;
    ogl_program_ssb output_data_ssb;
    GLuint          output_data_ssb_indexed_bp;
    unsigned int    wg_local_size[3];

    system_resizable_vector objects;

            ~_procedural_uv_generator();
    explicit _procedural_uv_generator(ogl_context                  in_context,
                                      procedural_uv_generator_type in_type);

    REFCOUNT_INSERT_VARIABLES
} _procedural_uv_generator;

typedef struct _procedural_uv_generator_object
{
    procedural_uv_generator_object_id object_id;

    unsigned int uv_bo_id;
    unsigned int uv_bo_size;
    unsigned int uv_bo_start_offset;

    mesh          owned_mesh;
    mesh_layer_id owned_mesh_layer_id;

    _procedural_uv_generator* owning_generator_ptr;

    explicit _procedural_uv_generator_object(_procedural_uv_generator*         in_owning_generator_ptr,
                                             mesh                              in_owned_mesh,
                                             mesh_layer_id                     in_owned_mesh_layer_id,
                                             procedural_uv_generator_object_id in_object_id);

    ~_procedural_uv_generator_object();
} _procedural_uv_generator_object;


/** Forward declarations */
PRIVATE system_hashed_ansi_string _procedural_uv_generator_get_generator_name           (procedural_uv_generator_type     type);
PRIVATE ogl_program               _procedural_uv_generator_init_generator_po            (_procedural_uv_generator*        generator_ptr);
PRIVATE void                      _procedural_uv_generator_release                      (void*                            ptr);
PRIVATE void                      _procedural_uv_generator_verify_result_buffer_capacity(_procedural_uv_generator*        generator_ptr,
                                                                                         _procedural_uv_generator_object* object_ptr);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_uv_generator,
                               procedural_uv_generator,
                              _procedural_uv_generator);


/** TODO */
_procedural_uv_generator::_procedural_uv_generator(ogl_context                  in_context,
                                                   procedural_uv_generator_type in_type)
{
    ogl_buffers  buffers             = NULL;
    unsigned int texcoords_data_size = 0;

    /* Perform actual initialization. */
    context                      = in_context;
    generator_cs                 = NULL;
    generator_po                 = NULL;
    input_data_ssb               = NULL;
    input_data_ssb_indexed_bp    = -1;
    input_params_ub              = NULL;
    n_normals_ub                 = NULL;
    n_normals_ub_bo_id           = 0;
    n_normals_ub_bo_start_offset = -1;
    objects                      = system_resizable_vector_create(4 /* capacity */);
    output_data_ssb              = NULL;
    output_data_ssb_indexed_bp   = -1;
    type                         = in_type;

    memset(wg_local_size,
           0,
           sizeof(wg_local_size) );

    ogl_context_retain(in_context);

    _procedural_uv_generator_init_generator_po(this);
}

/** TODO */
_procedural_uv_generator_object::_procedural_uv_generator_object(_procedural_uv_generator*         in_owning_generator_ptr,
                                                                 mesh                              in_owned_mesh,
                                                                 mesh_layer_id                     in_owned_mesh_layer_id,
                                                                 procedural_uv_generator_object_id in_object_id)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(in_owned_mesh != NULL,
                      "Input mesh instance is NULL");
    ASSERT_DEBUG_SYNC(in_owning_generator_ptr != NULL,
                      "Input owning generator is NULL");

    /* Fill the descriptor */
    object_id            =  in_object_id;
    owned_mesh           =  in_owned_mesh;
    owned_mesh_layer_id  =  in_owned_mesh_layer_id;
    owning_generator_ptr =  in_owning_generator_ptr;
    uv_bo_id             =  0;
    uv_bo_size           =  0;
    uv_bo_start_offset   = -1;

    /* Retain the object, so that it never gets released while we use it */
    mesh_retain(in_owned_mesh);
}

/** TODO */
_procedural_uv_generator::~_procedural_uv_generator()
{
    /* Release all initialized objects */
    if (objects != NULL)
    {
        uint32_t n_objects = 0;

        system_resizable_vector_get_property(objects,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_objects);

        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            _procedural_uv_generator_object* object_ptr = NULL;

            system_resizable_vector_get_element_at(objects,
                                                   n_object,
                                                  &object_ptr);

            if (object_ptr != NULL)
            {
                delete object_ptr;

                object_ptr = NULL;
            } /* if (object_ptr != NULL) */
        } /* for (all created objects) */

        system_resizable_vector_release(objects);
        objects = NULL;
    } /* if (objects != NULL) */

    /* Release all other owned instances */
    if (context != NULL)
    {
        ogl_context_release(context);

        context = NULL;
    }

    if (generator_po != NULL)
    {
        ogl_program_release(generator_po);

        generator_po = NULL;
    }
}

/** TODO */
_procedural_uv_generator_object::~_procedural_uv_generator_object()
{
    if (owned_mesh != NULL)
    {
        mesh_release(owned_mesh);

        owned_mesh = NULL;
    }

    /* Release buffer memory allocated to hold the texcoords data */
    if (uv_bo_id != 0)
    {
        ogl_buffers buffers = NULL;

        ogl_context_get_property(owning_generator_ptr->context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &buffers);

        ogl_buffers_free_buffer_memory(buffers,
                                       uv_bo_id,
                                       uv_bo_start_offset);

        uv_bo_id           =  0;
        uv_bo_start_offset = -1;
    }
}


/** TODO */
PRIVATE system_hashed_ansi_string _procedural_uv_generator_get_generator_name(procedural_uv_generator_type type)
{
    system_hashed_ansi_string result = NULL;

    switch (type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            result = system_hashed_ansi_string_create("Spherical mapping with normals");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural UV generator type");
        }
    } /* switch (type) */

    return result;
}

/** TODO */
PRIVATE ogl_program _procedural_uv_generator_init_generator_po(_procedural_uv_generator* generator_ptr)
{
    const char*                 cs_body                             = NULL;
    ogl_context_gl_limits*      limits_ptr                          = NULL;
    ogl_programs                programs                            = NULL;
    ogl_program                 result                              = NULL;
    char                        temp[128];
    const ogl_program_variable* variable_n_normals_defined_ptr      = NULL;
    const ogl_program_variable* variable_normal_data_stride_ptr     = NULL;
    const ogl_program_variable* variable_start_offset_in_floats_ptr = NULL;

    const unsigned int        n_token_key_values               = 3;
    system_hashed_ansi_string token_keys  [n_token_key_values] = {NULL};
    system_hashed_ansi_string token_values[n_token_key_values] = {NULL};

    /* Check if the program object is not already cached for the rendering context. */
    ogl_context_get_property(generator_ptr->context,
                             OGL_CONTEXT_PROPERTY_PROGRAMS,
                            &programs);

    result = ogl_programs_get_program_by_name(programs,
                                              _procedural_uv_generator_get_generator_name(generator_ptr->type) );

    if (result != NULL)
    {
        ogl_program_retain(result);

        goto end;
    }

    /* Pick the right compute shader template */
    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            cs_body = "#version 430 core\n"
                      "\n"
                      "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = 1, local_size_z = 1) in;\n"
                      "\n"
                      "layout(binding = 0) readonly buffer inputData\n"
                      "{\n"
                      "    restrict float meshData[];\n"
                      "};\n"
                      "layout(binding = 1) writeonly buffer outputData\n"
                      "{\n"
                      "    restrict float resultData[];\n"
                      "};\n"
                      "\n"
                      /* NOTE: In order to handle buffer-memory backed storage of the number of normals defined for the object,
                       *       we need to use a separate UB. If the value is stored in buffer memory, we'll just bind it to the UB.
                       *       Otherwise we'll update & sync the ogl_program_ub at each _execute() call.*/
                      "layout(packed, binding = 0) uniform nNormalsBlock\n"
                      "{\n"
                      "    uint n_normals_defined;\n"
                      "};\n"
                      "\n"
                      "layout(packed, binding = 1) uniform inputParams\n"
                      "{\n"
                      "    uint normal_data_stride_in_floats;\n"
                      "    uint start_offset_in_floats;\n"
                      "};\n"
                      "\n"
                      "void main()\n"
                      "{\n"
                      "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * LOCAL_WG_SIZE_Y) +\n"
                      "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)                   +\n"
                      "                                            gl_GlobalInvocationID.x);\n"
                      "\n"
                      "    if (global_invocation_id_flat > n_normals_defined)\n"
                      "    {\n"
                      "        return;"
                      "    }\n"
                      "\n"
                      "    const vec2 normal_data = vec2(meshData[start_offset_in_floats + global_invocation_id_flat * normal_data_stride_in_floats],\n"
                      "                                  meshData[start_offset_in_floats + global_invocation_id_flat * normal_data_stride_in_floats + 1]);\n"
                      "\n"
                      "    resultData[global_invocation_id_flat * 2 + 0] = asin(normal_data.x) * 3.14152965 + 0.5;\n"
                      "    resultData[global_invocation_id_flat * 2 + 1] = asin(normal_data.y) * 3.14152965 + 0.5;\n"
                      "}\n";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural UV generator type");
        }
    } /* switch (generator_ptr->type) */

    /* Determine the local work-group size. */
    ogl_context_get_property(generator_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    generator_ptr->wg_local_size[0] = limits_ptr->max_compute_work_group_size[0]; /* TODO: smarterize me */
    generator_ptr->wg_local_size[1] = 1;
    generator_ptr->wg_local_size[2] = 1;

    /* Prepare token key/value pairs */
    token_keys[0] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_X");
    token_keys[1] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Y");
    token_keys[2] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Z");

    snprintf(temp,
             sizeof(temp),
             "%u",
             generator_ptr->wg_local_size[0]);
    token_values[0] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%u",
             generator_ptr->wg_local_size[1]);
    token_values[1] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%u",
             generator_ptr->wg_local_size[2]);
    token_values[2] = system_hashed_ansi_string_create(temp);

    /* Set up the compute shader program */
    generator_ptr->generator_cs = ogl_shader_create (generator_ptr->context,
                                                     SHADER_TYPE_COMPUTE,
                                                     _procedural_uv_generator_get_generator_name(generator_ptr->type) );
    generator_ptr->generator_po = ogl_program_create(generator_ptr->context,
                                                     _procedural_uv_generator_get_generator_name(generator_ptr->type),
                                                     OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_shader_set_body_with_token_replacement(generator_ptr->generator_cs,
                                               cs_body,
                                               n_token_key_values,
                                               token_keys,
                                               token_values);

    if (!ogl_shader_compile(generator_ptr->generator_cs) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Failed to compile procedural UV generator compute shader");

        goto end;
    }

    ogl_program_attach_shader(generator_ptr->generator_po,
                              generator_ptr->generator_cs);

    if (!ogl_program_link(generator_ptr->generator_po) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Failed to link procedural UV generator computer shader program");

        goto end;
    }

    /* Retrieve the inputParams uniform block instance */
    ogl_program_get_uniform_block_by_name(generator_ptr->generator_po,
                                          system_hashed_ansi_string_create("inputParams"),
                                         &generator_ptr->input_params_ub);

    ASSERT_DEBUG_SYNC(generator_ptr->input_params_ub != NULL,
                      "Could not retrieve inputParams uniform block instance");

    ogl_program_ub_get_property(generator_ptr->input_params_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &generator_ptr->input_params_ub_bo_id);
    ogl_program_ub_get_property(generator_ptr->input_params_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &generator_ptr->input_params_ub_bo_start_offset);
    ogl_program_ub_get_property(generator_ptr->input_params_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &generator_ptr->input_params_ub_bo_size);

    /* Retrieve the nNormals uniform block instance */
    ogl_program_get_uniform_block_by_name(generator_ptr->generator_po,
                                          system_hashed_ansi_string_create("nNormalsBlock"),
                                         &generator_ptr->n_normals_ub);

    ASSERT_DEBUG_SYNC(generator_ptr->n_normals_ub != NULL,
                      "Could not retrieve nNormalsBlock uniform block instance");

    ogl_program_ub_get_property(generator_ptr->n_normals_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &generator_ptr->n_normals_ub_bo_id);
    ogl_program_ub_get_property(generator_ptr->n_normals_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &generator_ptr->n_normals_ub_bo_start_offset);

    ogl_program_get_uniform_by_name(generator_ptr->generator_po,
                                    system_hashed_ansi_string_create("n_normals_defined"),
                                   &variable_n_normals_defined_ptr);

    ASSERT_DEBUG_SYNC(variable_n_normals_defined_ptr != NULL,
                      "Could not retrieve nNormalsBlock uniform block member descriptors");
    ASSERT_DEBUG_SYNC(variable_n_normals_defined_ptr->block_offset == 0,
                      "Invalid n_normals_defined offset reported by GL");

    /* Retrieve the shader storage block instances */
    ogl_program_get_shader_storage_block_by_name(generator_ptr->generator_po,
                                                 system_hashed_ansi_string_create("inputData"),
                                                &generator_ptr->input_data_ssb);
    ogl_program_get_shader_storage_block_by_name(generator_ptr->generator_po,
                                                 system_hashed_ansi_string_create("outputData"),
                                                &generator_ptr->output_data_ssb);

    ASSERT_DEBUG_SYNC(generator_ptr->input_data_ssb  != NULL &&
                      generator_ptr->output_data_ssb != NULL,
                      "Could not retrieve shader storage block instance descriptors");

    /* ..and query their properties */
    ogl_program_ssb_get_property(generator_ptr->input_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &generator_ptr->input_data_ssb_indexed_bp);
    ogl_program_ssb_get_property(generator_ptr->output_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &generator_ptr->output_data_ssb_indexed_bp);

    /* Retrieve the uniform block member descriptors */
    ogl_program_get_uniform_by_name(generator_ptr->generator_po,
                                    system_hashed_ansi_string_create("normal_data_stride_in_floats"),
                                   &variable_normal_data_stride_ptr);
    ogl_program_get_uniform_by_name(generator_ptr->generator_po,
                                    system_hashed_ansi_string_create("start_offset_in_floats"),
                                   &variable_start_offset_in_floats_ptr);

    ASSERT_DEBUG_SYNC(variable_normal_data_stride_ptr     != NULL &&
                      variable_start_offset_in_floats_ptr != NULL,
                      "Could not retrieve inputParams uniform block member descriptors");

    generator_ptr->input_params_ub_normal_data_stride_block_offset = variable_normal_data_stride_ptr->block_offset;
    generator_ptr->input_params_ub_start_offset_block_offset       = variable_start_offset_in_floats_ptr->block_offset;

end:
    if (generator_ptr->generator_cs != NULL)
    {
        ogl_shader_release(generator_ptr->generator_cs);

        generator_ptr->generator_cs = NULL;
    }

    return result;
}


/** TODO */
PRIVATE void _procedural_uv_generator_release(void* ptr)
{
    _procedural_uv_generator* generator_ptr = (_procedural_uv_generator*) ptr;

    /* Nothing to do - owned assets will be released by the destructor */
}

/** TODO */
PRIVATE void _procedural_uv_generator_run_spherical_mapping_with_normals_po(_procedural_uv_generator*        generator_ptr,
                                                                            _procedural_uv_generator_object* object_ptr)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr                          = NULL;
    unsigned int                      global_wg_size[3];
    const ogl_context_gl_limits*      limits_ptr                               = NULL;
    GLuint                            n_normals_bo_id                          = 0;
    bool                              n_normals_bo_requires_memory_barrier     = false;
    unsigned int                      n_normals_bo_start_offset                = 0;
    mesh_layer_data_stream_source     n_normals_source                         = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;
    GLuint                            normal_data_bo_id                        = 0;
    unsigned int                      normal_data_bo_size                      = 0;
    unsigned int                      normal_data_bo_start_offset              = 0;
    unsigned int                      normal_data_bo_stride                    = 0;
    int                               normal_data_offset_adjustment            = 0;
    unsigned int                      normal_data_bo_start_offset_ssbo_aligned = 0;

    ogl_context_get_property(generator_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(generator_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* Retrieve normal data BO properties.
     *
     * The number of normals can be stored in either buffer or client memory. Make sure to correctly
     * detect and handle both cases. */
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_normals_source);

    switch (n_normals_source)
    {
        case MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY:
        {
            mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                object_ptr->owned_mesh_layer_id,
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_ID,
                                               &n_normals_bo_id);
            mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                object_ptr->owned_mesh_layer_id,
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_START_OFFSET,
                                               &n_normals_bo_start_offset);
            mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                object_ptr->owned_mesh_layer_id,
                                                MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER,
                                               &n_normals_bo_requires_memory_barrier);

            entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    0, /* index */
                                    n_normals_bo_id,
                                    n_normals_bo_start_offset,
                                    sizeof(unsigned int) );

            break;
        } /* case MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY: */

        case MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY:
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO");

            break;
        } /* case MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY: */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized source for n_items property of the normal data stream.");
        }
    } /* switch (n_normals_source) */

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_ID,
                                       &normal_data_bo_id);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_SIZE,
                                       &normal_data_bo_size);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE,
                                       &normal_data_bo_stride);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &normal_data_bo_start_offset);

    ASSERT_DEBUG_SYNC(normal_data_bo_id != 0,
                      "Invalid normal data BO ID defined for the object.");

    /* NOTE: Normal data may not start at the alignment boundary needed for SSBOs. Adjust the offset
     *       we pass to the UB, so that the requirement is met.
     */
    normal_data_offset_adjustment            = normal_data_bo_start_offset % limits_ptr->shader_storage_buffer_offset_alignment;
    normal_data_bo_start_offset_ssbo_aligned = normal_data_bo_start_offset - normal_data_offset_adjustment;

    normal_data_offset_adjustment = normal_data_offset_adjustment;

    /* Set up UB bindings & contents. The bindings are hard-coded in the shader. */
    ogl_program_ub_set_nonarrayed_uniform_value(generator_ptr->input_params_ub,
                                                generator_ptr->input_params_ub_normal_data_stride_block_offset,
                                               &normal_data_bo_stride,
                                                0, /* src_data_flags */
                                                sizeof(normal_data_bo_stride) );
    ogl_program_ub_set_nonarrayed_uniform_value(generator_ptr->input_params_ub,
                                                generator_ptr->input_params_ub_start_offset_block_offset,
                                               &normal_data_offset_adjustment,
                                                0, /* src_data_flags */
                                                sizeof(unsigned int) );

    ogl_program_ub_sync(generator_ptr->input_params_ub);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        1, /* index */
                                        generator_ptr->input_params_ub_bo_id,
                                        generator_ptr->input_params_ub_bo_start_offset,
                                        generator_ptr->input_params_ub_bo_size);

    if (n_normals_bo_requires_memory_barrier)
    {
        entrypoints_ptr->pGLMemoryBarrier(GL_UNIFORM_BARRIER_BIT);
    }

    /* Set up SSB bindings. Specific indices are defined in the shader. */
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        normal_data_bo_id,
                                        normal_data_bo_start_offset_ssbo_aligned,
                                        normal_data_bo_size + normal_data_offset_adjustment); /* size */
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        1, /* index */
                                        object_ptr->uv_bo_id,
                                        object_ptr->uv_bo_start_offset,
                                        object_ptr->uv_bo_size);

    /* Issue the dispatch call */
    global_wg_size[0] = 1 + (normal_data_bo_size / sizeof(float) / 2 /* uv */) / generator_ptr->wg_local_size[0];
    global_wg_size[1] = 1;
    global_wg_size[2] = 1;

    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(generator_ptr->generator_po) );
    entrypoints_ptr->pGLDispatchCompute(global_wg_size[0],
                                        global_wg_size[1],
                                        global_wg_size[2]);
}

/** TODO */
PRIVATE void _procedural_uv_generator_verify_result_buffer_capacity(_procedural_uv_generator*        generator_ptr,
                                                                    _procedural_uv_generator_object* object_ptr)
{
    mesh_layer_data_stream_source n_normals_source = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_normals_source);

    switch (n_normals_source)
    {
        case MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY:
        {
            if (object_ptr->uv_bo_size == 0)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Invalid request - number of normals must be either stored in client memory, or the UV generator result data buffer must be preallocated in advance.");

                goto end;
            }

            /* Nothing to do here - result data buffer is already preallocated */
            break;
        }

        case MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY:
        {
            uint32_t n_normals              = 0;
            uint32_t n_storage_bytes_needed = 0;

            /* TODO - the existing implementation requires verification */
            ASSERT_DEBUG_SYNC(false,
                              "TODO");

            if (!mesh_get_layer_data_stream_data(object_ptr->owned_mesh,
                                                 object_ptr->owned_mesh_layer_id,
                                                 MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                &n_normals,
                                                 NULL /* out_data_ptr */) ||
                n_normals == 0)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "No normal data defined for the object.");

                goto end;
            }

            /* Do we need to update the storage? */
            n_storage_bytes_needed = n_normals * sizeof(float) * 2;

            if (object_ptr->uv_bo_size == 0                       ||
                object_ptr->uv_bo_size != 0                       &&
                object_ptr->uv_bo_size <  n_storage_bytes_needed)
            {
                /* Need to (re-)allocate the buffer memory storage.. */
                if (!procedural_uv_generator_alloc_result_buffer_memory( (procedural_uv_generator) generator_ptr,
                                                                        object_ptr->object_id,
                                                                        n_storage_bytes_needed) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Result buffer allocation failed.");

                    goto end;
                }
            } /* if (re-allocation needed) */

            break;
        } /* case MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY: */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized number of normals storage source type.");
        }
    } /* switch (n_normals_source) */

end:
    ;
}


/** Please see header for specification */
PUBLIC EMERALD_API procedural_uv_generator_object_id procedural_uv_generator_add_mesh(procedural_uv_generator in_generator,
                                                                                      mesh                    in_mesh,
                                                                                      mesh_layer_id           in_mesh_layer_id)
{
    ogl_buffers                       buffers        = NULL;
    _procedural_uv_generator*         generator_ptr  = (_procedural_uv_generator*) in_generator;
    _procedural_uv_generator_object*  new_object_ptr = NULL;
    procedural_uv_generator_object_id result_id      = 0xFFFFFFFF;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(in_generator != NULL,
                      "Input UV generator instance is NULL");
    ASSERT_DEBUG_SYNC(in_mesh != NULL,
                      "Input mesh is NULL");

    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            bool result;

            /* Normal data must be defined at the creation time. */
            result = mesh_get_layer_data_stream_data(in_mesh,
                                                     in_mesh_layer_id,
                                                     MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                     NULL,  /* out_n_items_ptr */
                                                     NULL); /* out_data_ptr    */

            ASSERT_DEBUG_SYNC(result,
                              "Normal data is undefined for the specified input mesh");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural UV generator type requested.");
        }
    } /* switch (in_type) */

    /* Texture coordinates must be undefined */
    ASSERT_ALWAYS_SYNC(!mesh_get_layer_data_stream_data(in_mesh,
                                                        in_mesh_layer_id,
                                                        MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                        NULL,  /* out_n_items_ptr */
                                                        NULL), /* out_data_ptr    */
                       "Texture coordinates data is already defined for the specified input mesh.");

    /* Define it now! We will update the BO id, its size and start offset later on .. */
    mesh_add_layer_data_stream_from_buffer_memory(in_mesh,
                                                  in_mesh_layer_id,
                                                  MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                  2,                 /* n_components */
                                                  0,                 /* bo_id */
                                                 -1,                 /* bo_start_offset */
                                                  sizeof(float) * 2, /* bo_stride */
                                                  0);                /* bo_size */

    /* Create and initialize a new object descriptor */
    system_resizable_vector_get_property(generator_ptr->objects,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result_id);

    new_object_ptr = new (std::nothrow) _procedural_uv_generator_object(generator_ptr,
                                                                        in_mesh,
                                                                        in_mesh_layer_id,
                                                                        result_id);

    if (new_object_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    system_resizable_vector_push(generator_ptr->objects,
                                 new_object_ptr);

    /* All done */
end:
    return result_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool procedural_uv_generator_alloc_result_buffer_memory(procedural_uv_generator           in_generator,
                                                                           procedural_uv_generator_object_id in_object_id,
                                                                           uint32_t                          n_bytes_to_preallocate)
{
    ogl_buffers                      buffers                            = NULL;
    _procedural_uv_generator*        generator_ptr                      = (_procedural_uv_generator*) in_generator;
    const ogl_context_gl_limits*     limits_ptr                         = NULL;
    GLuint                           n_normals_bo_id                    = 0;
    bool                             n_normals_bo_read_reqs_mem_barrier = false;
    unsigned int                     n_normals_bo_start_offset          = -1;
    mesh_layer_data_stream_source    n_normals_source                   = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;
    _procedural_uv_generator_object* object_ptr                         = NULL;
    bool                             result                             = false;

    LOG_ERROR("Performance warning: preallocating buffer memory for UV generator result data storage.");

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(generator_ptr != NULL,
                      "Input UV generator instance is NULL");

    if (!system_resizable_vector_get_element_at(generator_ptr->objects,
                                                in_object_id,
                                               &object_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "UV generator object [%d] was not recognized.",
                          in_object_id);

        goto end;
    }

    /* If a buffer memory is already allocated for the UV generator object, dealloc it before continuing */
    ogl_context_get_property(generator_ptr->context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);

    if (object_ptr->uv_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(buffers,
                                       object_ptr->uv_bo_id,
                                       object_ptr->uv_bo_start_offset);

        object_ptr->uv_bo_id           = 0;
        object_ptr->uv_bo_size         = 0;
        object_ptr->uv_bo_start_offset = 0;
    }

    /* Allocate a new buffer for the result data storage. */
    ogl_context_get_property(generator_ptr->context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    if (!ogl_buffers_allocate_buffer_memory(buffers,
                                            n_bytes_to_preallocate,
                                            limits_ptr->shader_storage_buffer_offset_alignment,
                                            OGL_BUFFERS_MAPPABILITY_NONE,
                                            OGL_BUFFERS_USAGE_VBO,
                                            0, /* flags */
                                           &object_ptr->uv_bo_id,
                                           &object_ptr->uv_bo_start_offset) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Result buffer memory storage allocation failed");

        goto end;
    }

    object_ptr->uv_bo_size = n_bytes_to_preallocate;

    /* Update mesh configuration */
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_normals_source);

    ASSERT_DEBUG_SYNC(n_normals_source == MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY,
                      "TODO: Support for client memory");

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_ID,
                                       &n_normals_bo_id);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER,
                                       &n_normals_bo_read_reqs_mem_barrier);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_START_OFFSET,
                                       &n_normals_bo_start_offset);

    if (!mesh_set_layer_data_stream_property_with_buffer_memory(object_ptr->owned_mesh,
                                                                object_ptr->owned_mesh_layer_id,
                                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS,
                                                                n_normals_bo_id,
                                                                sizeof(unsigned int),
                                                                n_normals_bo_start_offset,
                                                                n_normals_bo_read_reqs_mem_barrier) ||
        !mesh_set_layer_data_stream_property_with_buffer_memory(object_ptr->owned_mesh,
                                                                object_ptr->owned_mesh_layer_id,
                                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_ID,
                                                                object_ptr->uv_bo_id,
                                                                n_bytes_to_preallocate,
                                                                object_ptr->uv_bo_start_offset,
                                                                true) /* does_read_require_memory_barrier */
                                                                ) 
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not update texcoords data stream configuration");

        goto end;
    }

    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API procedural_uv_generator procedural_uv_generator_create(ogl_context                  in_context,
                                                                          procedural_uv_generator_type in_type,
                                                                          system_hashed_ansi_string    in_name)
{
    /* Instantiate the object */
    _procedural_uv_generator* generator_ptr = new (std::nothrow) _procedural_uv_generator(in_context,
                                                                                          in_type);

    ASSERT_DEBUG_SYNC(generator_ptr != NULL,
                      "Out of memory");

    if (generator_ptr == NULL)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(generator_ptr,
                                                   _procedural_uv_generator_release,
                                                   OBJECT_TYPE_PROCEDURAL_UV_GENERATOR,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Procedural UV Generators\\",
                                                                                                           system_hashed_ansi_string_get_buffer(in_name)) );

    /* Return the object */
end:
    return (procedural_uv_generator) generator_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void procedural_uv_generator_update(procedural_uv_generator           in_generator,
                                                                              procedural_uv_generator_object_id in_object_id)
{
    _procedural_uv_generator*         generator_ptr    = (_procedural_uv_generator*) in_generator;
    uint32_t                          n_normals        = 0;
    _procedural_uv_generator_object*  object_ptr       = NULL;
    uint32_t                          uv_data_size     = 0;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(generator_ptr != NULL,
                      "Input UV generator instance is NULL");

    /* Retrieve the object descriptor */
    system_resizable_vector_get_element_at(generator_ptr->objects,
                                           in_object_id,
                                          &object_ptr);

    if (object_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "UV generator object descriptor is NULL");

        goto end;
    } /* if (object_ptr == NULL) */

    /* Check if we have enough space to generate new UV data .. */
    _procedural_uv_generator_verify_result_buffer_capacity(generator_ptr,
                                                           object_ptr);

    /* Execute the generator program */
    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            _procedural_uv_generator_run_spherical_mapping_with_normals_po(generator_ptr,
                                                                           object_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized generator type");
        }
    } /* switch (generator_ptr->type) */

    /* All done */
end:
    ;
}