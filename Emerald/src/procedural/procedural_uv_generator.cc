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


/** Internal type definition */
typedef struct _procedural_uv_generator
{
    ogl_context                  context;
    mesh                         dst_mesh;
    mesh_layer_id                dst_mesh_layer_id;
    unsigned int                 texcoords_data_bo_id;
    unsigned int                 texcoords_data_bo_start_offset;
    procedural_uv_generator_type type;

    ogl_shader      generator_cs;
    ogl_program     generator_po;
    ogl_program_ssb input_data_ssb;
    unsigned int    input_data_ssb_block_size;
    GLuint          input_data_ssb_indexed_bp;
    ogl_program_ub  input_params_ub;
    GLint           input_params_ub_normal_data_stride_block_offset;
    GLint           input_params_ub_start_offset_block_offset;
    ogl_program_ssb output_data_ssb;
    unsigned int    output_data_ssb_block_size;
    GLuint          output_data_ssb_indexed_bp;
    unsigned int    wg_local_size[3];

            ~_procedural_uv_generator();
    explicit _procedural_uv_generator(ogl_context                  in_context,
                                      mesh                         in_dst_mesh,
                                      mesh_layer_id                in_dst_mesh_layer_id,
                                      procedural_uv_generator_type in_type);

    REFCOUNT_INSERT_VARIABLES
} _procedural_uv_generator;

/** Forward declarations */
PRIVATE system_hashed_ansi_string _procedural_uv_generator_get_generator_name(procedural_uv_generator_type type);
PRIVATE ogl_program               _procedural_uv_generator_init_generator_po (_procedural_uv_generator*    generator_ptr);
PRIVATE void                      _procedural_uv_generator_release           (void*                        ptr);


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_uv_generator,
                               procedural_uv_generator,
                              _procedural_uv_generator);


/** TODO */
_procedural_uv_generator::_procedural_uv_generator(ogl_context                  in_context,
                                                   mesh                         in_dst_mesh,
                                                   mesh_layer_id                in_dst_mesh_layer_id,
                                                   procedural_uv_generator_type in_type)
{
    ogl_buffers  buffers             = NULL;
    unsigned int texcoords_data_size = 0;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(in_dst_mesh != NULL,
                      "Input mesh is NULL");

    switch (in_type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            unsigned int n_normal_vectors = 0;
            bool         result;

            /* Normal data must be defined at the creation time. */
            result = mesh_get_layer_data_stream_data(in_dst_mesh,
                                                     in_dst_mesh_layer_id,
                                                     MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                    &n_normal_vectors,
                                                     NULL); /* out_data_ptr */

            ASSERT_DEBUG_SYNC(result               &&
                              n_normal_vectors > 0,
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
    ASSERT_ALWAYS_SYNC(!mesh_get_layer_data_stream_data(in_dst_mesh,
                                                        in_dst_mesh_layer_id,
                                                        MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                        NULL,  /* out_n_items_ptr */
                                                        NULL), /* out_data_ptr    */
                       "Texture coordinates data is already defined for the specified input mesh.");

    /* Allocate buffer memory to hold the texcoord data */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);

    texcoords_data_bo_id           =  0;
    texcoords_data_bo_start_offset = -1;

    ogl_buffers_allocate_buffer_memory(buffers,
                                       texcoords_data_size,
                                       0, /* alignment_requirement */
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_VBO,
                                       0, /* flags */
                                      &texcoords_data_bo_id,
                                      &texcoords_data_bo_start_offset);

    /* Perform actual initialization. */
    context                    = in_context;
    dst_mesh                   = in_dst_mesh;
    dst_mesh_layer_id          = in_dst_mesh_layer_id;
    generator_cs               = NULL;
    generator_po               = NULL;
    input_data_ssb             = NULL;
    input_data_ssb_block_size  = 0;
    input_data_ssb_indexed_bp  = -1;
    input_params_ub            = NULL;
    output_data_ssb            = NULL;
    output_data_ssb_block_size = 0;
    output_data_ssb_indexed_bp = -1;
    type                       = in_type;

    ogl_context_retain(in_context);
    mesh_retain       (in_dst_mesh);

    memset(wg_local_size,
           0,
           sizeof(wg_local_size) );

    _procedural_uv_generator_init_generator_po(this);
}

/** TODO */
_procedural_uv_generator::~_procedural_uv_generator()
{
    /* Release buffer memory allocated to hold the texcoords data*/
    if (texcoords_data_bo_id != 0)
    {
        ogl_buffers buffers = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &buffers);

        ogl_buffers_free_buffer_memory(buffers,
                                       texcoords_data_bo_id,
                                       texcoords_data_bo_start_offset);

        texcoords_data_bo_id           =  0;
        texcoords_data_bo_start_offset = -1;
    }

    /* Release all other instances */
    if (context != NULL)
    {
        ogl_context_release(context);

        context = NULL;
    }

    if (dst_mesh != NULL)
    {
        mesh_release(dst_mesh);

        dst_mesh = NULL;
    }

    if (generator_po != NULL)
    {
        ogl_program_release(generator_po);

        generator_po = NULL;
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
                      "readonly buffer inputData\n"
                      "{\n"
                      "    restrict float meshData[];\n"
                      "};\n"
                      "writeonly buffer outputData\n"
                      "{\n"
                      "    restrict float resultData[];\n"
                      "};\n"
                      "\n"
                      "layout(packed) uniform inputParams\n"
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
             "%d",
             generator_ptr->wg_local_size[0]);
    token_values[0] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%d",
             generator_ptr->wg_local_size[1]);
    token_values[1] = system_hashed_ansi_string_create(temp);

    snprintf(temp,
             sizeof(temp),
             "%d",
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
                                 OGL_PROGRAM_SSB_PROPERTY_BLOCK_DATA_SIZE,
                                &generator_ptr->input_data_ssb_block_size);
    ogl_program_ssb_get_property(generator_ptr->input_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &generator_ptr->input_data_ssb_indexed_bp);

    ogl_program_ssb_get_property(generator_ptr->output_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_BLOCK_DATA_SIZE,
                                &generator_ptr->output_data_ssb_block_size);
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


/** Please see header for specification */
PUBLIC EMERALD_API procedural_uv_generator procedural_uv_generator_create(ogl_context                  in_context,
                                                                          mesh                         in_mesh,
                                                                          mesh_layer_id                in_mesh_layer_id,
                                                                          procedural_uv_generator_type type,
                                                                          system_hashed_ansi_string    name)
{
    /* Instantiate the object */
    _procedural_uv_generator* generator_ptr = new (std::nothrow) _procedural_uv_generator(in_context,
                                                                                          in_mesh,
                                                                                          in_mesh_layer_id,
                                                                                          type);

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
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
end:
    return (procedural_uv_generator) generator_ptr;
}

