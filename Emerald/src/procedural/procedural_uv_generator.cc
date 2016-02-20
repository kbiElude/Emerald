/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_ssb.h"
#include "ogl/ogl_program_ub.h"
#include "procedural/procedural_uv_generator.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


/** Internal type definition */
typedef struct _procedural_uv_generator
{
    raGL_backend                 backend;
    ral_context                  context;
    procedural_uv_generator_type type;

    ral_shader      generator_cs;
    ral_program     generator_po;
    ogl_program_ssb input_data_ssb;
    GLuint          input_data_ssb_indexed_bp;
    ogl_program_ub  input_params_ub;
    GLint           input_params_ub_arg1_block_offset;
    GLint           input_params_ub_arg2_block_offset;
    ral_buffer      input_params_ub_bo;
    raGL_buffer     input_params_ub_bo_raGL;
    unsigned int    input_params_ub_bo_size;
    GLuint          input_params_ub_indexed_bp;
    GLint           input_params_ub_item_data_stride_block_offset;
    GLint           input_params_ub_start_offset_block_offset;
    ogl_program_ub  n_items_ub;
    ral_buffer      n_items_ub_bo;
    raGL_buffer     n_items_ub_bo_raGL;
    GLuint          n_items_ub_indexed_bp;
    ogl_program_ssb output_data_ssb;
    GLuint          output_data_ssb_indexed_bp;
    unsigned int    wg_local_size[3];

    system_resizable_vector objects;

    /* U/V plane coeffs are only used by object linear generators */
    float u_plane_coeffs[4];
    float v_plane_coeffs[4];

            ~_procedural_uv_generator();
    explicit _procedural_uv_generator(ral_context                  in_context,
                                      procedural_uv_generator_type in_type);

    REFCOUNT_INSERT_VARIABLES
} _procedural_uv_generator;

typedef struct _procedural_uv_generator_object
{
    procedural_uv_generator_object_id object_id;

    ral_buffer   uv_bo;
    raGL_buffer  uv_bo_raGL;
    unsigned int uv_bo_size;

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
PRIVATE bool                      _procedural_uv_generator_build_generator_po             (_procedural_uv_generator*              generator_ptr);
PRIVATE system_hashed_ansi_string _procedural_uv_generator_get_generator_name             (procedural_uv_generator_type           type);
PRIVATE void                      _procedural_uv_generator_get_item_data_stream_properties(const _procedural_uv_generator*        generator_ptr,
                                                                                           const _procedural_uv_generator_object* object_ptr,
                                                                                           mesh_layer_data_stream_type*           out_opt_item_data_stream_type_ptr,
                                                                                           unsigned int*                          out_opt_n_bytes_per_item_ptr);
PRIVATE void                      _procedural_uv_generator_init_generator_po              (_procedural_uv_generator*              generator_ptr);
PRIVATE void                      _procedural_uv_generator_release                        (void*                                  ptr);
PRIVATE void                      _procedural_uv_generator_run_po                         (_procedural_uv_generator*              generator_ptr,
                                                                                           _procedural_uv_generator_object*       object_ptr);
PRIVATE void                      _procedural_uv_generator_verify_result_buffer_capacity  (_procedural_uv_generator*              generator_ptr,
                                                                                           _procedural_uv_generator_object*       object_ptr);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(procedural_uv_generator,
                               procedural_uv_generator,
                              _procedural_uv_generator);


/** TODO */
_procedural_uv_generator::_procedural_uv_generator(ral_context                  in_context,
                                                   procedural_uv_generator_type in_type)
{
    unsigned int texcoords_data_size = 0;

    /* Perform actual initialization. */
    backend                                       = NULL;
    context                                       = in_context;
    generator_cs                                  = NULL;
    generator_po                                  = NULL;
    input_data_ssb                                = NULL;
    input_data_ssb_indexed_bp                     = -1;
    input_params_ub                               = NULL;
    input_params_ub_arg1_block_offset             = -1;
    input_params_ub_arg2_block_offset             = -1;
    input_params_ub_bo                            = NULL;
    input_params_ub_bo_size                       = 0;
    input_params_ub_indexed_bp                    = -1;
    input_params_ub_item_data_stride_block_offset = -1;
    input_params_ub_start_offset_block_offset     = -1;
    n_items_ub                                    = NULL;
    n_items_ub_bo                                 = NULL;
    n_items_ub_indexed_bp                         = 0;
    objects                                       = system_resizable_vector_create(4 /* capacity */);
    output_data_ssb                               = NULL;
    output_data_ssb_indexed_bp                    = -1;
    type                                          = in_type;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND,
                             &backend);

    u_plane_coeffs[0] = 1.0f; u_plane_coeffs[1] = 0.0f; u_plane_coeffs[2] = 0.0f; u_plane_coeffs[3] = 0.0f;
    v_plane_coeffs[0] = 0.0f; v_plane_coeffs[1] = 1.0f; v_plane_coeffs[2] = 0.0f; v_plane_coeffs[3] = 0.0f;

    memset(wg_local_size,
           0,
           sizeof(wg_local_size) );

    ral_context_retain(in_context);

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
    object_id            = in_object_id;
    owned_mesh           = in_owned_mesh;
    owned_mesh_layer_id  = in_owned_mesh_layer_id;
    owning_generator_ptr = in_owning_generator_ptr;
    uv_bo                = NULL;
    uv_bo_size           = 0;

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
        ral_context_release(context);

        context = NULL;
    }

    if (generator_po != NULL)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) generator_po);

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
    if (uv_bo != NULL)
    {
        ral_context_delete_objects(owning_generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   (const void**) &uv_bo);

        uv_bo = NULL;
    }
}


/** TODO */
PRIVATE bool _procedural_uv_generator_build_generator_po(_procedural_uv_generator* generator_ptr)
{
    const unsigned int        n_token_key_values               = 3;
    bool                      result                           = false;
    char                      temp[128];
    system_hashed_ansi_string token_keys  [n_token_key_values] = {NULL};
    system_hashed_ansi_string token_values[n_token_key_values] = {NULL};

    static const char* cs_body_preamble   = "#version 430 core\n"
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
                                            /* NOTE: In order to handle buffer-memory backed storage of the number of items (normals, vertices, etc.) defined for the object,
                                             *       we need to use a separate UB. If the value is stored in buffer memory, we'll just bind it to the UB.
                                             *       Otherwise we'll update & sync the ogl_program_ub at each _execute() call.*/
                                            "layout(packed) uniform nItemsBlock\n"
                                            "{\n"
                                            "    uint n_items_defined;\n"
                                            "};\n"
                                            "\n"
                                            "layout(packed) uniform inputParams\n"
                                            "{\n"
                                            "    uint item_data_stride_in_floats;\n"
                                            "    uint start_offset_in_floats;\n"
                                            "    vec4 arg1;\n"
                                            "    vec4 arg2;\n"
                                            "};\n"
                                            "\n"
                                            "void main()\n"
                                            "{\n"
                                            "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * LOCAL_WG_SIZE_Y) +\n"
                                            "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)                   +\n"
                                            "                                            gl_GlobalInvocationID.x);\n"
                                            "\n"
                                            "    if (global_invocation_id_flat > n_items_defined)\n"
                                            "    {\n"
                                            "        return;"
                                            "    }\n"
                                            "\n";
    static const char* cs_body_terminator = "}\n";
    const char*        cs_body_parts[3]   =
    {
        cs_body_preamble,
        NULL, /* filled later */
        cs_body_terminator
    };
    const unsigned int n_cs_body_parts    = sizeof(cs_body_parts) / sizeof(cs_body_parts[0]);


    /* Pick the right compute shader template first. */
    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR:
        {
            cs_body_parts[1] = "    vec3 item_data = vec3(meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats],\n"
                               "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 1],\n"
                               "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 2]);\n"
                               "\n"
                               "    resultData[global_invocation_id_flat * 2 + 0] = dot(arg1, vec4(item_data, 1.0) );\n"
                               "    resultData[global_invocation_id_flat * 2 + 1] = dot(arg2, vec4(item_data, 1.0) );\n";

            break;
        }

        case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING:
        {
            cs_body_parts[1] = "    vec3 item_data = vec3(meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats],\n"
                               "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 1],\n"
                               "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 2]);\n"
                               "\n"
                               "    item_data = normalize(item_data);\n"
                               "\n"
                               "    resultData[global_invocation_id_flat * 2 + 0] = asin(item_data.x) / 3.14152965 + 0.5;\n"
                               "    resultData[global_invocation_id_flat * 2 + 1] = asin(item_data.y) / 3.14152965 + 0.5;\n";

            break;
        } /* case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING: */

        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            cs_body_parts[1] = "    const vec2 item_data = vec2(meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats],\n"
                               "                                meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 1]);\n"
                               "\n"
                               "    resultData[global_invocation_id_flat * 2 + 0] = asin(item_data.x) / 3.14152965 + 0.5;\n"
                               "    resultData[global_invocation_id_flat * 2 + 1] = asin(item_data.y) / 3.14152965 + 0.5;\n";

            break;
        } /* case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS: */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural UV generator type");
        }
    } /* switch (generator_ptr->type) */

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
    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
        _procedural_uv_generator_get_generator_name(generator_ptr->type)
    };

    const ral_shader_create_info shader_create_info =
    {
        _procedural_uv_generator_get_generator_name(generator_ptr->type),
        RAL_SHADER_TYPE_COMPUTE
    };

    const system_hashed_ansi_string merged_cs_body = system_hashed_ansi_string_create_by_merging_strings  (n_cs_body_parts,
                                                                                                           cs_body_parts);
    const system_hashed_ansi_string final_cs_body  = system_hashed_ansi_string_create_by_token_replacement(system_hashed_ansi_string_get_buffer(merged_cs_body),
                                                                                                           n_token_key_values,
                                                                                                           token_keys,
                                                                                                           token_values);

    if (!ral_context_create_shaders(generator_ptr->context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &generator_ptr->generator_cs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed");

        goto end;
    }

    if (!ral_context_create_programs(generator_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &generator_ptr->generator_po) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed");

        goto end;
    }

    ral_shader_set_property(generator_ptr->generator_cs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            &final_cs_body);

    if (!ral_program_attach_shader(generator_ptr->generator_po,
                                   generator_ptr->generator_cs))
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Failed to link procedural UV generator computer shader program");

        goto end;
    }

    result = true;

end:
    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _procedural_uv_generator_get_generator_name(procedural_uv_generator_type type)
{
    system_hashed_ansi_string result = NULL;

    switch (type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR:
        {
            result = system_hashed_ansi_string_create("Object<->plane linear mapping");

            break;
        }

        case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING:
        {
            result = system_hashed_ansi_string_create("Positional spherical mapping");

            break;
        }

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
PRIVATE void _procedural_uv_generator_get_item_data_stream_properties(const _procedural_uv_generator*        generator_ptr,
                                                                      const _procedural_uv_generator_object* opt_object_ptr,
                                                                      mesh_layer_data_stream_type*           out_opt_item_data_stream_type_ptr,
                                                                      unsigned int*                          out_opt_n_bytes_per_item_ptr)
{
    unsigned int                n_item_components       = 0;
    mesh_layer_data_stream_type result_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;

    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR:
        case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING:
        {
            /* The generator uses vertex data */
            result_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_VERTICES;

            break;
        } /* case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING: */

        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            /* The generator uses normal data */
            result_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_NORMALS;

            break;
        } /* case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS: */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized UV generator type");
        }
    } /* switch (generator_ptr->type) */

    if (result_data_stream_type != MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN)
    {
        if (out_opt_item_data_stream_type_ptr != NULL)
        {
            *out_opt_item_data_stream_type_ptr = result_data_stream_type;
        } /* if (out_opt_item_data_stream_type_ptr != NULL) */

        if (out_opt_n_bytes_per_item_ptr != NULL)
        {
            ASSERT_DEBUG_SYNC(opt_object_ptr != NULL,
                              "Object descriptor pointer is NULL");

            mesh_get_layer_data_stream_property(opt_object_ptr->owned_mesh,
                                                opt_object_ptr->owned_mesh_layer_id,
                                                result_data_stream_type,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,
                                               &n_item_components);

            ASSERT_DEBUG_SYNC(n_item_components != 0,
                              "No vertex data item components defined!");

            *out_opt_n_bytes_per_item_ptr = sizeof(float) * n_item_components;
        } /* if (out_opt_n_bytes_per_item_ptr != NULL) */
    }
}

/** TODO */
PRIVATE void _procedural_uv_generator_init_generator_po(_procedural_uv_generator* generator_ptr)
{
    ogl_context_gl_limits*      limits_ptr                              = NULL;
    const ral_program_variable* variable_arg1_ral_ptr                   = NULL;
    const ral_program_variable* variable_arg2_ral_ptr                   = NULL;
    const ral_program_variable* variable_n_items_defined_ral_ptr        = NULL;
    const ral_program_variable* variable_item_data_stride_ral_ptr       = NULL;
    const ral_program_variable* variable_start_offset_in_floats_ral_ptr = NULL;

    /* Determine the local work-group size. */
    ogl_context_get_property(ral_context_get_gl_context(generator_ptr->context),
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    generator_ptr->wg_local_size[0] = limits_ptr->max_compute_work_group_size[0]; /* TODO: smarterize me */
    generator_ptr->wg_local_size[1] = 1;
    generator_ptr->wg_local_size[2] = 1;

    /* Check if the program object is not already cached for the rendering context. */
    generator_ptr->generator_po = ral_context_get_program_by_name(generator_ptr->context,
                                                                  _procedural_uv_generator_get_generator_name(generator_ptr->type) );

    if (generator_ptr->generator_po == NULL)
    {
        /* Need to create the program from scratch. */
        bool result = _procedural_uv_generator_build_generator_po(generator_ptr);

        if (!result)
        {
            ASSERT_ALWAYS_SYNC(false,
                              "Failed to build UV generator program object");

            goto end;
        }
    } /* if (generator_ptr->generator_po == NULL) */
    else
    {
        ral_context_retain_object(generator_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  generator_ptr->generator_po);
    }

    /* Retrieve the inputParams uniform block instance */
    const raGL_program program_raGL = ral_context_get_program_gl(generator_ptr->context,
                                                                 generator_ptr->generator_po);

    raGL_program_get_uniform_block_by_name(program_raGL,
                                          system_hashed_ansi_string_create("inputParams"),
                                         &generator_ptr->input_params_ub);

    ASSERT_DEBUG_SYNC(generator_ptr->input_params_ub != NULL,
                      "Could not retrieve inputParams uniform block instance");

    ogl_program_ub_get_property(generator_ptr->input_params_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &generator_ptr->input_params_ub_bo);
    ogl_program_ub_get_property(generator_ptr->input_params_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &generator_ptr->input_params_ub_bo_size);
    ogl_program_ub_get_property(generator_ptr->input_params_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &generator_ptr->input_params_ub_indexed_bp);

    raGL_backend_get_buffer(generator_ptr->backend,
                            generator_ptr->input_params_ub_bo,
                  (void**) &generator_ptr->input_params_ub_bo_raGL);

    /* Retrieve the nItems uniform block instance */
    raGL_program_get_uniform_block_by_name(program_raGL,
                                           system_hashed_ansi_string_create("nItemsBlock"),
                                          &generator_ptr->n_items_ub);

    ASSERT_DEBUG_SYNC(generator_ptr->n_items_ub != NULL,
                      "Could not retrieve nItemsBlock uniform block instance");

    ogl_program_ub_get_property(generator_ptr->n_items_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &generator_ptr->n_items_ub_bo);
    ogl_program_ub_get_property(generator_ptr->n_items_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &generator_ptr->n_items_ub_indexed_bp);

    raGL_backend_get_buffer(generator_ptr->backend,
                            generator_ptr->n_items_ub_bo,
                  (void**) &generator_ptr->n_items_ub_bo_raGL);

    ral_program_get_block_variable_by_name(generator_ptr->generator_po,
                                           system_hashed_ansi_string_create("nItemsBlock"),
                                           system_hashed_ansi_string_create("n_items_defined"),
                                          &variable_n_items_defined_ral_ptr);

    ASSERT_DEBUG_SYNC(variable_n_items_defined_ral_ptr != NULL,
                      "Could not retrieve nItemsBlock uniform block member descriptors");
    ASSERT_DEBUG_SYNC(variable_n_items_defined_ral_ptr->block_offset == 0,
                      "Invalid n_items_defined offset reported by GL");

    /* Retrieve the shader storage block instances */
    raGL_program_get_shader_storage_block_by_name(program_raGL,
                                                  system_hashed_ansi_string_create("inputData"),
                                                 &generator_ptr->input_data_ssb);
    raGL_program_get_shader_storage_block_by_name(program_raGL,
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

    /* Retrieve the uniform block member descriptors
     *
     * NOTE: arg1 & arg2 may be removed by the compiler for all but the "object linear" program.
     **/
    ral_program_get_block_variable_by_name(generator_ptr->generator_po,
                                           system_hashed_ansi_string_create("inputParams"),
                                           system_hashed_ansi_string_create("arg1"),
                                          &variable_arg1_ral_ptr);
    ral_program_get_block_variable_by_name(generator_ptr->generator_po,
                                           system_hashed_ansi_string_create("inputParams"),
                                           system_hashed_ansi_string_create("arg2"),
                                          &variable_arg2_ral_ptr);
    ral_program_get_block_variable_by_name(generator_ptr->generator_po,
                                           system_hashed_ansi_string_create("inputParams"),
                                           system_hashed_ansi_string_create("item_data_stride_in_floats"),
                                          &variable_item_data_stride_ral_ptr);
    ral_program_get_block_variable_by_name(generator_ptr->generator_po,
                                           system_hashed_ansi_string_create("inputParams"),
                                           system_hashed_ansi_string_create("start_offset_in_floats"),
                                          &variable_start_offset_in_floats_ral_ptr);

    if (generator_ptr->type == PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR)
    {
        ASSERT_DEBUG_SYNC(variable_arg1_ral_ptr != NULL &&
                          variable_arg2_ral_ptr != NULL,
                          "Could not retrieve arg1 and/or arg2 uniform block member descriptors");

        generator_ptr->input_params_ub_arg1_block_offset = variable_arg1_ral_ptr->block_offset;
        generator_ptr->input_params_ub_arg2_block_offset = variable_arg2_ral_ptr->block_offset;
    }

    ASSERT_DEBUG_SYNC(variable_item_data_stride_ral_ptr       != NULL &&
                      variable_start_offset_in_floats_ral_ptr != NULL,
                      "Could not retrieve inputParams uniform block member descriptors");

    generator_ptr->input_params_ub_item_data_stride_block_offset = variable_item_data_stride_ral_ptr->block_offset;
    generator_ptr->input_params_ub_start_offset_block_offset     = variable_start_offset_in_floats_ral_ptr->block_offset;

end:
    /* Clean up */
    if (generator_ptr->generator_cs != NULL)
    {
        ral_context_delete_objects(generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   (const void**) &generator_ptr->generator_cs);

        generator_ptr->generator_cs = NULL;
    }
}


/** TODO */
PRIVATE void _procedural_uv_generator_release(void* ptr)
{
    _procedural_uv_generator* generator_ptr = (_procedural_uv_generator*) ptr;

    /* Nothing to do - owned assets will be released by the destructor */
}

/** TODO */
PRIVATE void _procedural_uv_generator_run_po(_procedural_uv_generator*        generator_ptr,
                                             _procedural_uv_generator_object* object_ptr)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr                        = NULL;
    unsigned int                      global_wg_size[3];
    ral_buffer                        item_data_bo_ral                       = 0;
    unsigned int                      item_data_bo_size                      = 0;
    unsigned int                      item_data_bo_start_offset              = 0;
    unsigned int                      item_data_bo_stride                    = 0;
    unsigned int                      item_data_bo_stride_div_4              = 0;
    int                               item_data_offset_adjustment            = 0;
    int                               item_data_offset_adjustment_div_4      = 0;
    unsigned int                      item_data_bo_start_offset_ssbo_aligned = 0;
    const ogl_context_gl_limits*      limits_ptr                             = NULL;
    bool                              n_items_bo_requires_memory_barrier     = false;
    mesh_layer_data_stream_source     n_items_source                         = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;
    mesh_layer_data_stream_type       source_item_data_stream_type           = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;

    ogl_context_get_property(ral_context_get_gl_context(generator_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(ral_context_get_gl_context(generator_ptr->context),
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* Determine the source data stream type. */
    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR:
        case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING:
        {
            source_item_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_VERTICES;

            break;
        }

        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            source_item_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_NORMALS;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural UV generator type");
        }
    } /* switch (generator_ptr->type) */

    /* Retrieve normal data BO properties.
     *
     * The number of normals can be stored in either buffer or client memory. Make sure to correctly
     * detect and handle both cases. */
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_items_source);

    switch (n_items_source)
    {
        case MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY:
        {
            ral_buffer  n_items_bo              = NULL;
            GLuint      n_items_bo_id           = 0;
            raGL_buffer n_items_bo_raGL         = NULL;
            uint32_t    n_items_bo_start_offset = -1;

            mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                object_ptr->owned_mesh_layer_id,
                                                source_item_data_stream_type,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_RAL,
                                               &n_items_bo);
            mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                object_ptr->owned_mesh_layer_id,
                                                source_item_data_stream_type,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER,
                                               &n_items_bo_requires_memory_barrier);

            raGL_backend_get_buffer(generator_ptr->backend,
                                    n_items_bo,
                          (void**) &n_items_bo_raGL);

            raGL_buffer_get_property(n_items_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &n_items_bo_id);
            raGL_buffer_get_property(n_items_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &n_items_bo_start_offset);

            entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                                generator_ptr->n_items_ub_indexed_bp,
                                                n_items_bo_id,
                                                n_items_bo_start_offset,
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
    } /* switch (n_items_source) */

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL,
                                       &item_data_bo_ral);

    ASSERT_DEBUG_SYNC(item_data_bo_ral != NULL,
                      "Invalid item data BO defined for the object.");

    ral_buffer_get_property(item_data_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &item_data_bo_size);

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE,
                                       &item_data_bo_stride);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &item_data_bo_start_offset);

    /* NOTE: Item data may not start at the alignment boundary needed for SSBOs. Adjust the offset
     *       we pass to the UB, so that the requirement is met.
     */
    item_data_offset_adjustment            = item_data_bo_start_offset % limits_ptr->shader_storage_buffer_offset_alignment;
    item_data_bo_start_offset_ssbo_aligned = item_data_bo_start_offset - item_data_offset_adjustment;

    item_data_offset_adjustment = item_data_offset_adjustment;

    ASSERT_DEBUG_SYNC(item_data_offset_adjustment % sizeof(float) == 0 &&
                      item_data_bo_stride         % sizeof(float) == 0,
                      "Data alignment error");

    item_data_offset_adjustment_div_4 = item_data_offset_adjustment / sizeof(float);
    item_data_bo_stride_div_4         = item_data_bo_stride         / sizeof(float);

    /* Set up UB bindings & contents. The bindings are hard-coded in the shader. */
    ogl_program_ub_set_nonarrayed_uniform_value(generator_ptr->input_params_ub,
                                                generator_ptr->input_params_ub_item_data_stride_block_offset,
                                               &item_data_bo_stride_div_4,
                                                0, /* src_data_flags */
                                                sizeof(item_data_bo_stride) );
    ogl_program_ub_set_nonarrayed_uniform_value(generator_ptr->input_params_ub,
                                                generator_ptr->input_params_ub_start_offset_block_offset,
                                               &item_data_offset_adjustment_div_4,
                                                0, /* src_data_flags */
                                                sizeof(unsigned int) );

    if (generator_ptr->type == PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR)
    {
        /* U/V plane coefficients */
        ogl_program_ub_set_nonarrayed_uniform_value(generator_ptr->input_params_ub,
                                                    generator_ptr->input_params_ub_arg1_block_offset,
                                                    generator_ptr->u_plane_coeffs,
                                                    0, /* src_data_flags */
                                                    sizeof(generator_ptr->u_plane_coeffs) );

        ogl_program_ub_set_nonarrayed_uniform_value(generator_ptr->input_params_ub,
                                                    generator_ptr->input_params_ub_arg2_block_offset,
                                                    generator_ptr->v_plane_coeffs,
                                                    0, /* src_data_flags */
                                                    sizeof(generator_ptr->v_plane_coeffs) );
    }

    ogl_program_ub_sync(generator_ptr->input_params_ub);

    GLuint   input_params_ub_bo_id           = 0;
    uint32_t input_params_ub_bo_start_offset = -1;

    raGL_buffer_get_property(generator_ptr->input_params_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &input_params_ub_bo_id);
    raGL_buffer_get_property(generator_ptr->input_params_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &input_params_ub_bo_start_offset);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        generator_ptr->input_params_ub_indexed_bp,
                                        input_params_ub_bo_id,
                                        input_params_ub_bo_start_offset,
                                        generator_ptr->input_params_ub_bo_size);

    if (n_items_bo_requires_memory_barrier)
    {
        entrypoints_ptr->pGLMemoryBarrier(GL_UNIFORM_BARRIER_BIT);
    }

    /* Set up SSB bindings. */
    GLuint      item_data_bo_id    = 0;
    raGL_buffer item_data_bo_raGL  = NULL;
    GLuint      uv_bo_id           = 0;
    uint32_t    uv_bo_start_offset = -1;

    raGL_backend_get_buffer(generator_ptr->backend,
                            item_data_bo_ral,
                  (void**) &item_data_bo_raGL);

    raGL_buffer_get_property(item_data_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &item_data_bo_id);
    raGL_buffer_get_property(object_ptr->uv_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &uv_bo_id);
    raGL_buffer_get_property(object_ptr->uv_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &uv_bo_start_offset);

    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        generator_ptr->input_data_ssb_indexed_bp,
                                        item_data_bo_id,
                                        item_data_bo_start_offset_ssbo_aligned,
                                        item_data_bo_size + item_data_offset_adjustment); /* size */
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        generator_ptr->output_data_ssb_indexed_bp,
                                        uv_bo_id,
                                        uv_bo_start_offset,
                                        object_ptr->uv_bo_size);

    /* Issue the dispatch call */
    const raGL_program program_raGL    = ral_context_get_program_gl(generator_ptr->context,
                                                                    generator_ptr->generator_po);
    GLuint             program_raGL_id = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    global_wg_size[0] = 1 + (item_data_bo_size / sizeof(float) / 2 /* uv */) / generator_ptr->wg_local_size[0];
    global_wg_size[1] = 1;
    global_wg_size[2] = 1;

    entrypoints_ptr->pGLUseProgram     (program_raGL_id);
    entrypoints_ptr->pGLDispatchCompute(global_wg_size[0],
                                        global_wg_size[1],
                                        global_wg_size[2]);
}

/** TODO */
PRIVATE void _procedural_uv_generator_verify_result_buffer_capacity(_procedural_uv_generator*        generator_ptr,
                                                                    _procedural_uv_generator_object* object_ptr)
{
    mesh_layer_data_stream_type   item_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;
    unsigned int                  n_bytes_per_item      = 0;
    mesh_layer_data_stream_source n_items_source        = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;

    _procedural_uv_generator_get_item_data_stream_properties(generator_ptr,
                                                             object_ptr,
                                                            &item_data_stream_type,
                                                            &n_bytes_per_item);

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_items_source);

    switch (n_items_source)
    {
        case MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY:
        {
            if (object_ptr->uv_bo_size == 0)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Invalid request - number of data stream items must be either stored in client memory, "
                                   "or the UV generator result data buffer must be preallocated in advance.");

                goto end;
            }

            /* Nothing to do here - result data buffer is already preallocated */
            break;
        }

        case MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY:
        {
            uint32_t n_items                = 0;
            uint32_t n_storage_bytes_needed = 0;

            /* TODO - the existing implementation requires verification */
            ASSERT_DEBUG_SYNC(false,
                              "TODO");

            if (!mesh_get_layer_data_stream_data(object_ptr->owned_mesh,
                                                 object_ptr->owned_mesh_layer_id,
                                                 item_data_stream_type,
                                                &n_items,
                                                 NULL /* out_data_ptr */) ||
                n_items == 0)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "No source data items, required to generate UV data, defined for the object.");

                goto end;
            }

            /* Do we need to update the storage? */
            n_storage_bytes_needed = n_items * n_bytes_per_item;

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
                              "Unrecognized number of data stream item storage source type.");
        }
    } /* switch (n_items_source) */

end:
    ;
}


/** Please see header for specification */
PUBLIC EMERALD_API procedural_uv_generator_object_id procedural_uv_generator_add_mesh(procedural_uv_generator in_generator,
                                                                                      mesh                    in_mesh,
                                                                                      mesh_layer_id           in_mesh_layer_id)
{
    _procedural_uv_generator*         generator_ptr         = (_procedural_uv_generator*) in_generator;
    mesh_layer_data_stream_type       item_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;
    _procedural_uv_generator_object*  new_object_ptr       = NULL;
    bool                              result               = false;
    procedural_uv_generator_object_id result_id            = 0xFFFFFFFF;

    _procedural_uv_generator_get_item_data_stream_properties(generator_ptr,
                                                             NULL, /* object_ptr */
                                                            &item_data_stream_type,
                                                             NULL); /* out_opt_n_bytes_item_ptr */

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(in_generator != NULL,
                      "Input UV generator instance is NULL");
    ASSERT_DEBUG_SYNC(in_mesh != NULL,
                      "Input mesh is NULL");
    ASSERT_DEBUG_SYNC(item_data_stream_type != MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN,
                      "Item data stream type could not be determined.");

    /* Make sure the source item data is already defined */
    result = mesh_get_layer_data_stream_data(in_mesh,
                                             in_mesh_layer_id,
                                             item_data_stream_type,
                                             NULL,  /* out_n_items_ptr */
                                             NULL); /* out_data_ptr    */

    ASSERT_DEBUG_SYNC(result,
                      "Required item data stream is undefined for the specified input mesh");

    /* Texture coordinates must be undefined */
    ASSERT_ALWAYS_SYNC(!mesh_get_layer_data_stream_data(in_mesh,
                                                        in_mesh_layer_id,
                                                        MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                        NULL,  /* out_n_items_ptr */
                                                        NULL), /* out_data_ptr    */
                       "Texture coordinates data is already defined for the specified input mesh.");

    /* Spawn a new texcoord data stream. We will update the BO id, its size and start offset later though .. */
    ASSERT_DEBUG_SYNC(false,
                      "TODO: Will this work?");

    mesh_add_layer_data_stream_from_buffer_memory(in_mesh,
                                                  in_mesh_layer_id,
                                                  MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                  2,                  /* n_components */
                                                  NULL,               /* bo */
                                                  sizeof(float) * 2); /* bo_stride */

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
    _procedural_uv_generator*        generator_ptr                    = (_procedural_uv_generator*) in_generator;
    const ogl_context_gl_limits*     limits_ptr                       = NULL;
    GLuint                           n_items_bo_id                    = 0;
    bool                             n_items_bo_read_reqs_mem_barrier = false;
    unsigned int                     n_items_bo_start_offset          = -1;
    mesh_layer_data_stream_source    n_items_source                   = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;
    _procedural_uv_generator_object* object_ptr                       = NULL;
    bool                             result                           = false;
    mesh_layer_data_stream_type      source_item_data_stream_type     = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;
    ral_buffer_create_info           uv_bo_create_info;

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
    if (object_ptr->uv_bo != NULL)
    {
        ral_context_delete_objects(generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   (const void**) &object_ptr->uv_bo);

        object_ptr->uv_bo      = NULL;
        object_ptr->uv_bo_raGL = NULL;
        object_ptr->uv_bo_size = 0;
    }

    /* Allocate a new buffer for the result data storage. */
    ogl_context_get_property(ral_context_get_gl_context(generator_ptr->context),
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    uv_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    uv_bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
    uv_bo_create_info.size             = n_bytes_to_preallocate;
    uv_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT |
                                         RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT        |
                                         RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    uv_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

    if (!ral_context_create_buffers(generator_ptr->context,
                                    1, /* n_buffers */
                                   &uv_bo_create_info,
                                   &object_ptr->uv_bo) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Result buffer memory storage allocation failed");

        goto end;
    }

    raGL_backend_get_buffer(generator_ptr->backend,
                            object_ptr->uv_bo,
                  (void**) &object_ptr->uv_bo_raGL);

    object_ptr->uv_bo_size = n_bytes_to_preallocate;

    /* Update mesh configuration */
    _procedural_uv_generator_get_item_data_stream_properties(generator_ptr,
                                                             object_ptr,
                                                            &source_item_data_stream_type,
                                                             NULL); /* out_opt_n_bytes_item_ptr */

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_items_source);

    ASSERT_DEBUG_SYNC(n_items_source == MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY,
                      "TODO: Support for client memory");

    ral_buffer n_items_bo = NULL;

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_RAL,
                                       &n_items_bo);
    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER,
                                       &n_items_bo_read_reqs_mem_barrier);

    if (!mesh_set_layer_data_stream_property_with_buffer_memory(object_ptr->owned_mesh,
                                                                object_ptr->owned_mesh_layer_id,
                                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS,
                                                                n_items_bo,
                                                                n_items_bo_read_reqs_mem_barrier) ||
        !mesh_set_layer_data_stream_property_with_buffer_memory(object_ptr->owned_mesh,
                                                                object_ptr->owned_mesh_layer_id,
                                                                MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                                MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL,
                                                                object_ptr->uv_bo,
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
PUBLIC EMERALD_API procedural_uv_generator procedural_uv_generator_create(ral_context                  in_context,
                                                                          procedural_uv_generator_type in_type,
                                                                          system_hashed_ansi_string    in_name)
{
    /* Instantiate the object */
    _procedural_uv_generator* generator_ptr = NULL;

    generator_ptr = new (std::nothrow) _procedural_uv_generator(in_context,
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
PUBLIC EMERALD_API void procedural_uv_generator_set_object_property(procedural_uv_generator                 in_generator,
                                                                    procedural_uv_generator_object_id       in_object_id,
                                                                    procedural_uv_generator_object_property in_property,
                                                                    const void*                             in_data)
{
    _procedural_uv_generator*        generator_ptr = (_procedural_uv_generator*) in_generator;
    _procedural_uv_generator_object* object_ptr    = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(generator_ptr != NULL,
                      "Input UV generator instance is NULL");

    if (!system_resizable_vector_get_element_at(generator_ptr->objects,
                                                in_object_id,
                                               &object_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve object descriptor");

        goto end;
    }

    ASSERT_DEBUG_SYNC(object_ptr != NULL,
                      "Object descriptor at specified index is NULL");

    switch (in_property)
    {
        case PROCEDURAL_UV_GENERATOR_OBJECT_PROPERTY_U_PLANE_COEFFS:
        {
            memcpy(generator_ptr->u_plane_coeffs,
                   in_data,
                   sizeof(generator_ptr->u_plane_coeffs) );

            break;
        }

        case PROCEDURAL_UV_GENERATOR_OBJECT_PROPERTY_V_PLANE_COEFFS:
        {
            memcpy(generator_ptr->v_plane_coeffs,
                   in_data,
                   sizeof(generator_ptr->v_plane_coeffs) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized object property requested");
        }
    } /* switch (in_property) */
    /* All done */
end:
    ;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void procedural_uv_generator_update(procedural_uv_generator           in_generator,
                                                                              procedural_uv_generator_object_id in_object_id)
{
    _procedural_uv_generator*         generator_ptr = (_procedural_uv_generator*) in_generator;
    _procedural_uv_generator_object*  object_ptr    = NULL;
    uint32_t                          uv_data_size  = 0;

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
    _procedural_uv_generator_run_po(generator_ptr,
                                    object_ptr);

    /* All done */
end:
    ;
}