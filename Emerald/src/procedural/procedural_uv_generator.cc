/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "procedural/procedural_uv_generator.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
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
    mesh_layer_data_stream_type  source_item_data_stream_type;
    procedural_uv_generator_type type;

    ral_shader               generator_cs;
    ral_program              generator_po;
    ral_program_block_buffer input_data_ssb;
    ral_program_block_buffer input_params_ub;
    uint32_t                 input_params_ub_arg1_block_offset;
    uint32_t                 input_params_ub_arg2_block_offset;
    uint32_t                 input_params_ub_item_data_stride_block_offset;
    uint32_t                 input_params_ub_start_offset_block_offset;
    ral_program_block_buffer n_items_ub;
    ral_program_block_buffer output_data_ssb;
    unsigned int             wg_local_size[3];

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

    ral_command_buffer command_buffer;
    mesh               owned_mesh;
    mesh_layer_id      owned_mesh_layer_id;
    ral_buffer         uv_bo;
    uint32_t           uv_bo_size;

    _procedural_uv_generator* owning_generator_ptr;

    explicit _procedural_uv_generator_object(_procedural_uv_generator*         in_owning_generator_ptr,
                                             mesh                              in_owned_mesh,
                                             mesh_layer_id                     in_owned_mesh_layer_id,
                                             procedural_uv_generator_object_id in_object_id);

    ~_procedural_uv_generator_object();
} _procedural_uv_generator_object;


/** Forward declarations */
PRIVATE system_hashed_ansi_string _procedural_uv_generator_get_generator_name                 (procedural_uv_generator_type           type);
PRIVATE void                      _procedural_uv_generator_get_item_data_stream_properties    (const _procedural_uv_generator*        generator_ptr,
                                                                                               const _procedural_uv_generator_object* object_ptr,
                                                                                               mesh_layer_data_stream_type*           out_opt_item_data_stream_type_ptr,
                                                                                               unsigned int*                          out_opt_n_bytes_per_item_ptr);
PRIVATE void                      _procedural_uv_generator_update_input_params_ub_cpu_callback(void*                                  object_raw_ptr);
PRIVATE void                      _procedural_uv_generator_verify_result_buffer_capacity      (_procedural_uv_generator*              generator_ptr,
                                                                                               _procedural_uv_generator_object*       object_ptr);

PRIVATE bool _procedural_uv_generator_build_generator_po(_procedural_uv_generator* generator_ptr);
PRIVATE void _procedural_uv_generator_init_generator_po (_procedural_uv_generator* generator_ptr);
PRIVATE void _procedural_uv_generator_release           (void*                     ptr);

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
    backend                                       = nullptr;
    context                                       = in_context;
    generator_cs                                  = nullptr;
    generator_po                                  = nullptr;
    input_data_ssb                                = nullptr;
    input_params_ub                               = nullptr;
    input_params_ub_arg1_block_offset             = -1;
    input_params_ub_arg2_block_offset             = -1;
    input_params_ub_item_data_stride_block_offset = -1;
    input_params_ub_start_offset_block_offset     = -1;
    n_items_ub                                    = nullptr;
    objects                                       = system_resizable_vector_create(4 /* capacity */);
    output_data_ssb                               = nullptr;
    source_item_data_stream_type                  = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;
    type                                          = in_type;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND,
                             &backend);

    u_plane_coeffs[0] = 1.0f; u_plane_coeffs[1] = 0.0f; u_plane_coeffs[2] = 0.0f; u_plane_coeffs[3] = 0.0f;
    v_plane_coeffs[0] = 0.0f; v_plane_coeffs[1] = 1.0f; v_plane_coeffs[2] = 0.0f; v_plane_coeffs[3] = 0.0f;

    memset(wg_local_size,
           0,
           sizeof(wg_local_size) );

    switch (type)
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
    }

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
    ASSERT_DEBUG_SYNC(in_owned_mesh != nullptr,
                      "Input mesh instance is nullptr");
    ASSERT_DEBUG_SYNC(in_owning_generator_ptr != nullptr,
                      "Input owning generator is nullptr");

    /* Fill the descriptor */
    object_id            = in_object_id;
    owned_mesh           = in_owned_mesh;
    owned_mesh_layer_id  = in_owned_mesh_layer_id;
    owning_generator_ptr = in_owning_generator_ptr;
    uv_bo                = nullptr;

    /* Create a new command buffer instance */
    ral_command_buffer_create_info cmd_buffer_create_info;

    cmd_buffer_create_info.compatible_queues = RAL_QUEUE_COMPUTE_BIT;
    cmd_buffer_create_info.is_executable     = true;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = true;
    cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(in_owning_generator_ptr->context,
                                       1, /* n_command_Buffers */
                                       &cmd_buffer_create_info,
                                       &command_buffer);

    /* Retain the object, so that it never gets released while we use it */
    mesh_retain(in_owned_mesh);
}

/** TODO */
_procedural_uv_generator::~_procedural_uv_generator()
{
    /* Release all initialized objects */
    if (objects != nullptr)
    {
        uint32_t n_objects = 0;

        system_resizable_vector_get_property(objects,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_objects);

        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            _procedural_uv_generator_object* object_ptr = nullptr;

            system_resizable_vector_get_element_at(objects,
                                                   n_object,
                                                  &object_ptr);

            if (object_ptr != nullptr)
            {
                delete object_ptr;

                object_ptr = nullptr;
            }
        }

        system_resizable_vector_release(objects);
        objects = nullptr;
    }

    /* Release all other owned instances */
    ral_program_block_buffer block_buffers[] =
    {
        input_data_ssb,
        input_params_ub,
        n_items_ub,
        output_data_ssb
    };
    const uint32_t n_block_buffers = sizeof(block_buffers) / sizeof(block_buffers[0]);

    for (uint32_t n_block_buffer = 0;
                  n_block_buffer < n_block_buffers;
                ++n_block_buffer)
    {
        if (block_buffers[n_block_buffer] != nullptr)
        {
            ral_program_block_buffer_release(block_buffers[n_block_buffer]);
        }
    }

    if (generator_po != nullptr)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&generator_po) );

        generator_po = nullptr;
    }

    if (context != nullptr)
    {
        ral_context_release(context);

        context = nullptr;
    }

}

/** TODO */
_procedural_uv_generator_object::~_procedural_uv_generator_object()
{
    if (owned_mesh != nullptr)
    {
        mesh_release(owned_mesh);

        owned_mesh = nullptr;
    }

    if (command_buffer != nullptr)
    {
        ral_context_delete_objects(owning_generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&command_buffer) );
    }

    /* Release buffer memory allocated to hold the texcoords data */
    if (uv_bo != nullptr)
    {
        ral_context_delete_objects(owning_generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&uv_bo) );

        uv_bo = nullptr;
    }
}


/** TODO */
PRIVATE bool _procedural_uv_generator_build_generator_po(_procedural_uv_generator* generator_ptr)
{
    const unsigned int        n_token_key_values               = 3;
    bool                      result                           = false;
    char                      temp[128];
    system_hashed_ansi_string token_keys  [n_token_key_values] = {nullptr};
    system_hashed_ansi_string token_values[n_token_key_values] = {nullptr};

    static const char* cs_body_preamble =
        "#version 430 core\n"
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
        "layout(binding = 2, packed) uniform nItemsBlock\n"
        "{\n"
        "    uint n_items_defined;\n"
        "};\n"
        "\n"
        "layout(binding = 3, packed) uniform inputParams\n"
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
        nullptr, /* filled later */
        cs_body_terminator
    };
    const unsigned int n_cs_body_parts = sizeof(cs_body_parts) / sizeof(cs_body_parts[0]);


    /* Pick the right compute shader template first. */
    switch (generator_ptr->type)
    {
        case PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR:
        {
            cs_body_parts[1] =
                "    vec3 item_data = vec3(meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats],\n"
                "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 1],\n"
                "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 2]);\n"
                "\n"
                "    resultData[global_invocation_id_flat * 2 + 0] = dot(arg1, vec4(item_data, 1.0) );\n"
                "    resultData[global_invocation_id_flat * 2 + 1] = dot(arg2, vec4(item_data, 1.0) );\n";

            break;
        }

        case PROCEDURAL_UV_GENERATOR_TYPE_POSITIONAL_SPHERICAL_MAPPING:
        {
            cs_body_parts[1] =
                "    vec3 item_data = vec3(meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats],\n"
                "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 1],\n"
                "                          meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 2]);\n"
                "\n"
                "    item_data = normalize(item_data);\n"
                "\n"
                "    resultData[global_invocation_id_flat * 2 + 0] = asin(item_data.x) / 3.14152965 + 0.5;\n"
                "    resultData[global_invocation_id_flat * 2 + 1] = asin(item_data.y) / 3.14152965 + 0.5;\n";

            break;
        }

        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            cs_body_parts[1] =
                "    const vec2 item_data = vec2(meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats],\n"
                "                                meshData[start_offset_in_floats + global_invocation_id_flat * item_data_stride_in_floats + 1]);\n"
                "\n"
                "    resultData[global_invocation_id_flat * 2 + 0] = asin(item_data.x) / 3.14152965 + 0.5;\n"
                "    resultData[global_invocation_id_flat * 2 + 1] = asin(item_data.y) / 3.14152965 + 0.5;\n";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized procedural UV generator type");
        }
    }

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
                                   generator_ptr->generator_cs,
                                   true /* async */) )
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
    system_hashed_ansi_string result = nullptr;

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
    }

    return result;
}

/** TODO */
PRIVATE void _procedural_uv_generator_get_item_data_offsets(_procedural_uv_generator_object* object_ptr,
                                                            uint32_t*                        out_item_data_offset_adjustment_ptr,
                                                            uint32_t*                        out_item_data_bo_start_offset_ssbo_aligned_ptr)
{
    uint32_t item_data_bo_start_offset;
    uint32_t offset_adjustment;
    uint32_t storage_buffer_alignment;

    ral_context_get_property(object_ptr->owning_generator_ptr->context,
                             RAL_CONTEXT_PROPERTY_STORAGE_BUFFER_ALIGNMENT,
                             &storage_buffer_alignment);

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        object_ptr->owning_generator_ptr->source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &item_data_bo_start_offset);

    offset_adjustment = item_data_bo_start_offset % storage_buffer_alignment;

    *out_item_data_offset_adjustment_ptr            = offset_adjustment;
    *out_item_data_bo_start_offset_ssbo_aligned_ptr = item_data_bo_start_offset - offset_adjustment;
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
        }

        case PROCEDURAL_UV_GENERATOR_TYPE_SPHERICAL_MAPPING_WITH_NORMALS:
        {
            /* The generator uses normal data */
            result_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_NORMALS;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized UV generator type");
        }
    }

    if (result_data_stream_type != MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN)
    {
        if (out_opt_item_data_stream_type_ptr != nullptr)
        {
            *out_opt_item_data_stream_type_ptr = result_data_stream_type;
        }

        if (out_opt_n_bytes_per_item_ptr != nullptr)
        {
            ASSERT_DEBUG_SYNC(opt_object_ptr != nullptr,
                              "Object descriptor pointer is nullptr");

            mesh_get_layer_data_stream_property(opt_object_ptr->owned_mesh,
                                                opt_object_ptr->owned_mesh_layer_id,
                                                result_data_stream_type,
                                                MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,
                                               &n_item_components);

            ASSERT_DEBUG_SYNC(n_item_components != 0,
                              "No vertex data item components defined!");

            *out_opt_n_bytes_per_item_ptr = sizeof(float) * n_item_components;
        }
    }
}

/** TODO */
PRIVATE ral_present_task _procedural_uv_generator_get_present_task(_procedural_uv_generator*        generator_ptr,
                                                                   _procedural_uv_generator_object* object_ptr)
{
    uint32_t                      global_wg_size[3];
    ral_buffer                    input_params_ub_bo_ral                 = nullptr;
    ral_buffer                    item_data_bo_ral                       = 0;
    uint32_t                      item_data_bo_size                      = 0;
    uint32_t                      item_data_offset_adjustment            = 0;
    uint32_t                      item_data_offset_adjustment_div_4      = 0;
    unsigned int                  item_data_bo_start_offset_ssbo_aligned = 0;
    bool                          n_items_bo_requires_memory_barrier     = false;
    mesh_layer_data_stream_source n_items_source                         = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;

    /* Start recording the command buffer. */
    ral_command_buffer_start_recording(object_ptr->command_buffer);
    {
        ral_command_buffer_record_set_program(object_ptr->command_buffer,
                                              generator_ptr->generator_po);

        /* Retrieve normal data BO properties.
         *
         * The number of normals can be stored in either buffer or client memory. Make sure to correctly
         * detect and handle both cases. */
        mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                            object_ptr->owned_mesh_layer_id,
                                            generator_ptr->source_item_data_stream_type,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                           &n_items_source);

        switch (n_items_source)
        {
            case MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY:
            {
                ral_command_buffer_set_binding_command_info binding;
                ral_buffer                                  n_items_bo = nullptr;

                mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                    object_ptr->owned_mesh_layer_id,
                                                    generator_ptr->source_item_data_stream_type,
                                                    MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_RAL,
                                                   &n_items_bo);
                mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                                    object_ptr->owned_mesh_layer_id,
                                                    generator_ptr->source_item_data_stream_type,
                                                    MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER,
                                                   &n_items_bo_requires_memory_barrier);

                binding.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
                binding.name                          = system_hashed_ansi_string_create("nItemsBlock");
                binding.uniform_buffer_binding.buffer = n_items_bo;
                binding.uniform_buffer_binding.offset = 0;
                binding.uniform_buffer_binding.size   = 0;

                ral_command_buffer_record_set_bindings(object_ptr->command_buffer,
                                                       1, /* n_bindings */
                                                      &binding);

                break;
            }

            case MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "TODO");

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized source for n_items property of the normal data stream.");
            }
        }

        mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                            object_ptr->owned_mesh_layer_id,
                                            generator_ptr->source_item_data_stream_type,
                                            MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,
                                           &item_data_bo_ral);

        ASSERT_DEBUG_SYNC(item_data_bo_ral != nullptr,
                          "Invalid item data BO defined for the object.");

        ral_buffer_get_property(item_data_bo_ral,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &item_data_bo_size);

        /* NOTE: Item data may not start at the alignment boundary needed for SSBOs. Adjust the offset
         *       we pass to the UB, so that the requirement is met.
         */
        ral_command_buffer_set_binding_command_info bindings[3];

        _procedural_uv_generator_get_item_data_offsets(object_ptr,
                                                      &item_data_offset_adjustment,
                                                      &item_data_bo_start_offset_ssbo_aligned);

        ASSERT_DEBUG_SYNC(item_data_offset_adjustment % sizeof(float) == 0,
                          "Data alignment error");

        item_data_offset_adjustment_div_4 = item_data_offset_adjustment / sizeof(float);


        ral_program_block_buffer_get_property(generator_ptr->input_params_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &input_params_ub_bo_ral);

        bindings[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        bindings[0].name                          = system_hashed_ansi_string_create("inputParams");
        bindings[0].uniform_buffer_binding.buffer = input_params_ub_bo_ral;
        bindings[0].uniform_buffer_binding.offset = 0;
        bindings[0].uniform_buffer_binding.size   = 0;

        if (n_items_bo_requires_memory_barrier)
        {
#if 0
            entrypoints_ptr->pGLMemoryBarrier(GL_UNIFORM_BARRIER_BIT);
#endif
        }

        /* Set up SSB bindings. */
        bindings[1].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
        bindings[1].name                          = system_hashed_ansi_string_create("inputData");
        bindings[1].storage_buffer_binding.buffer = item_data_bo_ral;
        bindings[1].storage_buffer_binding.offset = item_data_bo_start_offset_ssbo_aligned;
        bindings[1].storage_buffer_binding.size   = item_data_bo_size + item_data_offset_adjustment;

        bindings[2].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
        bindings[2].name                          = system_hashed_ansi_string_create("outputData");
        bindings[2].storage_buffer_binding.buffer = object_ptr->uv_bo;
        bindings[2].storage_buffer_binding.offset = 0;
        bindings[2].storage_buffer_binding.size   = object_ptr->uv_bo_size;

        ral_command_buffer_record_set_bindings(object_ptr->command_buffer,
                                               sizeof(bindings) / sizeof(bindings[0]),
                                               bindings);

        /* Issue the dispatch call */
        global_wg_size[0] = 1 + (item_data_bo_size / sizeof(float) / 2 /* uv */) / generator_ptr->wg_local_size[0];
        global_wg_size[1] = 1;
        global_wg_size[2] = 1;

        ral_command_buffer_record_dispatch(object_ptr->command_buffer,
                                           global_wg_size);

        ral_program_block_buffer_sync_immediately(generator_ptr->input_params_ub);
    }
    ral_command_buffer_stop_recording(object_ptr->command_buffer);

    /* Create the result present task */
    ral_present_task                    cpu_task;
    ral_present_task_cpu_create_info    cpu_task_create_info;
    ral_present_task_io                 cpu_task_unique_output;
    ral_present_task                    gpu_task;
    ral_present_task_gpu_create_info    gpu_task_create_info;
    ral_present_task_io                 gpu_task_unique_output;
    ral_present_task_group_create_info  group_task_create_info;
    ral_present_task_ingroup_connection group_task_ingroup_connection;
    ral_present_task_group_mapping      group_task_output_mapping;
    ral_present_task                    group_task_present_tasks[2];
    ral_present_task                    result_task;

    cpu_task_unique_output.buffer      = input_params_ub_bo_ral;
    cpu_task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    gpu_task_unique_output.buffer      = object_ptr->uv_bo;
    gpu_task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_task_create_info.cpu_task_callback_user_arg = object_ptr;
    cpu_task_create_info.n_unique_inputs            = 0;
    cpu_task_create_info.n_unique_outputs           = 1;
    cpu_task_create_info.pfn_cpu_task_callback_proc = _procedural_uv_generator_update_input_params_ub_cpu_callback;
    cpu_task_create_info.unique_inputs              = nullptr;
    cpu_task_create_info.unique_outputs             = &cpu_task_unique_output;

    gpu_task_create_info.command_buffer   = object_ptr->command_buffer;
    gpu_task_create_info.n_unique_inputs  = 1;
    gpu_task_create_info.n_unique_outputs = 1;
    gpu_task_create_info.unique_inputs    = &cpu_task_unique_output;
    gpu_task_create_info.unique_outputs   = &gpu_task_unique_output;

    cpu_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Procedural UV generator: CPU task"),
                                                                           &cpu_task_create_info);
    gpu_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Procedural UV generator: GPU task"),
                                                                           &gpu_task_create_info);


    group_task_present_tasks[0] = cpu_task;
    group_task_present_tasks[1] = gpu_task;

    group_task_ingroup_connection.input_present_task_index     = 1;
    group_task_ingroup_connection.input_present_task_io_index  = 0;
    group_task_ingroup_connection.output_present_task_index    = 0;
    group_task_ingroup_connection.output_present_task_io_index = 0;

    group_task_output_mapping.group_task_io_index   = 0;
    group_task_output_mapping.n_present_task        = 1;
    group_task_output_mapping.present_task_io_index = 0;

    group_task_create_info.ingroup_connections                      = &group_task_ingroup_connection;
    group_task_create_info.n_ingroup_connections                    = 1;
    group_task_create_info.n_present_tasks                          = sizeof(group_task_present_tasks) / sizeof(group_task_present_tasks[0]);
    group_task_create_info.n_total_unique_inputs                    = 0;
    group_task_create_info.n_total_unique_outputs                   = 1;
    group_task_create_info.n_unique_input_to_ingroup_task_mappings  = 0;
    group_task_create_info.n_unique_output_to_ingroup_task_mappings = 1;
    group_task_create_info.present_tasks                            = group_task_present_tasks;
    group_task_create_info.unique_input_to_ingroup_task_mapping     = nullptr;
    group_task_create_info.unique_output_to_ingroup_task_mapping    = &group_task_output_mapping;

    result_task = ral_present_task_create_group(system_hashed_ansi_string_create("Procedural UV generation"),
                                               &group_task_create_info);

    ral_present_task_release(cpu_task);
    ral_present_task_release(gpu_task);

    return result_task;
}

/** TODO */
PRIVATE void _procedural_uv_generator_init_generator_po(_procedural_uv_generator* generator_ptr)
{
    const uint32_t*             max_compute_work_group_size_ptr;
    const ral_program_variable* variable_arg1_ral_ptr                   = nullptr;
    const ral_program_variable* variable_arg2_ral_ptr                   = nullptr;
    const ral_program_variable* variable_n_items_defined_ral_ptr        = nullptr;
    const ral_program_variable* variable_item_data_stride_ral_ptr       = nullptr;
    const ral_program_variable* variable_start_offset_in_floats_ral_ptr = nullptr;

    /* Determine the local work-group size. */
    ral_context_get_property(generator_ptr->context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,
                             &max_compute_work_group_size_ptr);

    generator_ptr->wg_local_size[0] = max_compute_work_group_size_ptr[0]; /* TODO: smarterize me */
    generator_ptr->wg_local_size[1] = 1;
    generator_ptr->wg_local_size[2] = 1;

    /* Check if the program object is not already cached for the rendering context. */
    generator_ptr->generator_po = ral_context_get_program_by_name(generator_ptr->context,
                                                                  _procedural_uv_generator_get_generator_name(generator_ptr->type) );

    if (generator_ptr->generator_po == nullptr)
    {
        /* Need to create the program from scratch. */
        bool result = _procedural_uv_generator_build_generator_po(generator_ptr);

        if (!result)
        {
            ASSERT_ALWAYS_SYNC(false,
                              "Failed to build UV generator program object");

            goto end;
        }
    }
    else
    {
        ral_context_retain_object(generator_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  generator_ptr->generator_po);
    }

    /* Retrieve the inputParams uniform block instance */
    generator_ptr->input_params_ub = ral_program_block_buffer_create(generator_ptr->context,
                                                                     generator_ptr->generator_po,
                                                                     system_hashed_ansi_string_create("inputParams") );

    /* Retrieve the nItems uniform block instance */
    generator_ptr->n_items_ub = ral_program_block_buffer_create(generator_ptr->context,
                                                                generator_ptr->generator_po,
                                                                system_hashed_ansi_string_create("nItemsBlock") );

    ral_program_get_block_variable_by_name(generator_ptr->generator_po,
                                           system_hashed_ansi_string_create("nItemsBlock"),
                                           system_hashed_ansi_string_create("n_items_defined"),
                                          &variable_n_items_defined_ral_ptr);

    ASSERT_DEBUG_SYNC(variable_n_items_defined_ral_ptr != nullptr,
                      "Could not retrieve nItemsBlock uniform block member descriptors");
    ASSERT_DEBUG_SYNC(variable_n_items_defined_ral_ptr->block_offset == 0,
                      "Invalid n_items_defined offset reported by GL");

    /* Retrieve the shader storage block instances */
    generator_ptr->input_data_ssb  = ral_program_block_buffer_create(generator_ptr->context,
                                                                     generator_ptr->generator_po,
                                                                     system_hashed_ansi_string_create("inputData") );
    generator_ptr->output_data_ssb = ral_program_block_buffer_create(generator_ptr->context,
                                                                     generator_ptr->generator_po,
                                                                     system_hashed_ansi_string_create("outputData") );

    ASSERT_DEBUG_SYNC(generator_ptr->input_data_ssb  != nullptr &&
                      generator_ptr->output_data_ssb != nullptr,
                      "Could not retrieve shader storage block instance descriptors");

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
        ASSERT_DEBUG_SYNC(variable_arg1_ral_ptr != nullptr &&
                          variable_arg2_ral_ptr != nullptr,
                          "Could not retrieve arg1 and/or arg2 uniform block member descriptors");

        generator_ptr->input_params_ub_arg1_block_offset = variable_arg1_ral_ptr->block_offset;
        generator_ptr->input_params_ub_arg2_block_offset = variable_arg2_ral_ptr->block_offset;
    }

    ASSERT_DEBUG_SYNC(variable_item_data_stride_ral_ptr       != nullptr &&
                      variable_start_offset_in_floats_ral_ptr != nullptr,
                      "Could not retrieve inputParams uniform block member descriptors");

    generator_ptr->input_params_ub_item_data_stride_block_offset = variable_item_data_stride_ral_ptr->block_offset;
    generator_ptr->input_params_ub_start_offset_block_offset     = variable_start_offset_in_floats_ral_ptr->block_offset;

end:
    /* Clean up */
    if (generator_ptr->generator_cs != nullptr)
    {
        ral_context_delete_objects(generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&generator_ptr->generator_cs) );

        generator_ptr->generator_cs = nullptr;
    }
}


/** TODO */
PRIVATE void _procedural_uv_generator_release(void* ptr)
{
    /* Nothing to do - owned assets will be released by the destructor */
}

/** TODO */
PRIVATE void _procedural_uv_generator_update_input_params_ub_cpu_callback(void* object_raw_ptr)
{
    uint32_t                         item_data_bo_stride                    = -1;
    uint32_t                         item_data_bo_stride_div_4              = -1;
    uint32_t                         item_data_bo_start_offset_ssbo_aligned = -1;
    uint32_t                         item_data_offset_adjustment            = -1;
    uint32_t                         item_data_offset_adjustment_div_4      = -1;
    _procedural_uv_generator_object* object_ptr                             = reinterpret_cast<_procedural_uv_generator_object*>(object_raw_ptr);

    _procedural_uv_generator_get_item_data_offsets(object_ptr,
                                                  &item_data_offset_adjustment,
                                                  &item_data_bo_start_offset_ssbo_aligned);

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        object_ptr->owning_generator_ptr->source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL_STRIDE,
                                       &item_data_bo_stride);

    ASSERT_DEBUG_SYNC(item_data_bo_stride % sizeof(float) == 0,
                      "Data alignment error");

    item_data_bo_stride_div_4         = item_data_bo_stride         / sizeof(float);
    item_data_offset_adjustment_div_4 = item_data_offset_adjustment / 4;

    /* Set up UB bindings & contents. The bindings are hard-coded in the shader. */
    ral_program_block_buffer_set_nonarrayed_variable_value(object_ptr->owning_generator_ptr->input_params_ub,
                                                           object_ptr->owning_generator_ptr->input_params_ub_item_data_stride_block_offset,
                                                          &item_data_bo_stride_div_4,
                                                           sizeof(uint32_t) );
    ral_program_block_buffer_set_nonarrayed_variable_value(object_ptr->owning_generator_ptr->input_params_ub,
                                                           object_ptr->owning_generator_ptr->input_params_ub_start_offset_block_offset,
                                                          &item_data_offset_adjustment_div_4,
                                                           sizeof(unsigned int) );

    if (object_ptr->owning_generator_ptr->type == PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR)
    {
        /* U/V plane coefficients */
        ral_program_block_buffer_set_nonarrayed_variable_value(object_ptr->owning_generator_ptr->input_params_ub,
                                                               object_ptr->owning_generator_ptr->input_params_ub_arg1_block_offset,
                                                               object_ptr->owning_generator_ptr->u_plane_coeffs,
                                                               sizeof(object_ptr->owning_generator_ptr->u_plane_coeffs) );

        ral_program_block_buffer_set_nonarrayed_variable_value(object_ptr->owning_generator_ptr->input_params_ub,
                                                               object_ptr->owning_generator_ptr->input_params_ub_arg2_block_offset,
                                                               object_ptr->owning_generator_ptr->v_plane_coeffs,
                                                               sizeof(object_ptr->owning_generator_ptr->v_plane_coeffs) );
    }

    ral_program_block_buffer_sync_immediately(object_ptr->owning_generator_ptr->input_params_ub);
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
                                                 nullptr /* out_data_ptr */) ||
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
                if (!procedural_uv_generator_alloc_result_buffer_memory(reinterpret_cast<procedural_uv_generator>(generator_ptr),
                                                                        object_ptr->object_id,
                                                                        n_storage_bytes_needed) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Result buffer allocation failed.");

                    goto end;
                }
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized number of data stream item storage source type.");
        }
    }

end:
    ;
}


/** Please see header for specification */
PUBLIC EMERALD_API procedural_uv_generator_object_id procedural_uv_generator_add_mesh(procedural_uv_generator in_generator,
                                                                                      mesh                    in_mesh,
                                                                                      mesh_layer_id           in_mesh_layer_id)
{
    _procedural_uv_generator*         generator_ptr         = reinterpret_cast<_procedural_uv_generator*>(in_generator);
    mesh_layer_data_stream_type       item_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;
    _procedural_uv_generator_object*  new_object_ptr       = nullptr;
    bool                              result               = false;
    procedural_uv_generator_object_id result_id            = 0xFFFFFFFF;

    _procedural_uv_generator_get_item_data_stream_properties(generator_ptr,
                                                             nullptr, /* object_ptr */
                                                            &item_data_stream_type,
                                                             nullptr); /* out_opt_n_bytes_item_ptr */

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(in_generator != nullptr,
                      "Input UV generator instance is nullptr");
    ASSERT_DEBUG_SYNC(in_mesh != nullptr,
                      "Input mesh is nullptr");
    ASSERT_DEBUG_SYNC(item_data_stream_type != MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN,
                      "Item data stream type could not be determined.");

    /* Make sure the source item data is already defined */
    result = mesh_get_layer_data_stream_data(in_mesh,
                                             in_mesh_layer_id,
                                             item_data_stream_type,
                                             nullptr,  /* out_n_items_ptr */
                                             nullptr); /* out_data_ptr    */

    ASSERT_DEBUG_SYNC(result,
                      "Required item data stream is undefined for the specified input mesh");

    /* Texture coordinates must be undefined */
    ASSERT_ALWAYS_SYNC(!mesh_get_layer_data_stream_data(in_mesh,
                                                        in_mesh_layer_id,
                                                        MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                        nullptr,  /* out_n_items_ptr */
                                                        nullptr), /* out_data_ptr    */
                       "Texture coordinates data is already defined for the specified input mesh.");

    /* Spawn a new texcoord data stream. We will update the BO id, its size and start offset later though .. */
    mesh_add_layer_data_stream_from_buffer_memory(in_mesh,
                                                  in_mesh_layer_id,
                                                  MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,
                                                  2,                  /* n_components */
                                                  nullptr,            /* bo           */
                                                  sizeof(float) * 2); /* bo_stride    */

    /* Create and initialize a new object descriptor */
    system_resizable_vector_get_property(generator_ptr->objects,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result_id);

    new_object_ptr = new (std::nothrow) _procedural_uv_generator_object(generator_ptr,
                                                                        in_mesh,
                                                                        in_mesh_layer_id,
                                                                        result_id);

    if (new_object_ptr == nullptr)
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
    _procedural_uv_generator*        generator_ptr                    = reinterpret_cast<_procedural_uv_generator*>(in_generator);
    bool                             n_items_bo_read_reqs_mem_barrier = false;
    unsigned int                     n_items_bo_start_offset          = -1;
    mesh_layer_data_stream_source    n_items_source                   = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;
    _procedural_uv_generator_object* object_ptr                       = nullptr;
    bool                             result                           = false;
    mesh_layer_data_stream_type      source_item_data_stream_type     = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;
    ral_buffer_create_info           uv_bo_create_info;

    LOG_ERROR("Performance warning: preallocating buffer memory for UV generator result data storage.");

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(generator_ptr != nullptr,
                      "Input UV generator instance is nullptr");

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
    if (object_ptr->uv_bo != nullptr)
    {
        ral_context_delete_objects(generator_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   reinterpret_cast<void* const*>(&object_ptr->uv_bo) );

        object_ptr->uv_bo      = nullptr;
        object_ptr->uv_bo_size = 0;
    }

    /* Allocate a new buffer for the result data storage. */
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

    object_ptr->uv_bo_size = n_bytes_to_preallocate;

    /* Update mesh configuration */
    _procedural_uv_generator_get_item_data_stream_properties(generator_ptr,
                                                             object_ptr,
                                                            &source_item_data_stream_type,
                                                             nullptr); /* out_opt_n_bytes_item_ptr */

    mesh_get_layer_data_stream_property(object_ptr->owned_mesh,
                                        object_ptr->owned_mesh_layer_id,
                                        source_item_data_stream_type,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,
                                       &n_items_source);

    ASSERT_DEBUG_SYNC(n_items_source == MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY,
                      "TODO: Support for client memory");

    ral_buffer n_items_bo = nullptr;

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
                                                                MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,
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
    _procedural_uv_generator* generator_ptr = nullptr;

    generator_ptr = new (std::nothrow) _procedural_uv_generator(in_context,
                                                                in_type);

    ASSERT_DEBUG_SYNC(generator_ptr != nullptr,
                      "Out of memory");

    if (generator_ptr == nullptr)
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
PUBLIC EMERALD_API ral_present_task procedural_uv_generator_get_present_task(procedural_uv_generator           in_generator,
                                                                             procedural_uv_generator_object_id in_object_id)
{
    _procedural_uv_generator*         generator_ptr = reinterpret_cast<_procedural_uv_generator*>(in_generator);
    _procedural_uv_generator_object*  object_ptr    = nullptr;
    ral_present_task                  result        = nullptr;
    uint32_t                          uv_data_size  = 0;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(generator_ptr != nullptr,
                      "Input UV generator instance is nullptr");

    /* Retrieve the object descriptor */
    system_resizable_vector_get_element_at(generator_ptr->objects,
                                           in_object_id,
                                          &object_ptr);

    if (object_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "UV generator object descriptor is nullptr");

        goto end;
    }

    /* Check if we have enough space to generate new UV data .. */
    _procedural_uv_generator_verify_result_buffer_capacity(generator_ptr,
                                                           object_ptr);

    /* Execute the generator program */
    result = _procedural_uv_generator_get_present_task(generator_ptr,
                                                       object_ptr);

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void procedural_uv_generator_set_object_property(procedural_uv_generator                 in_generator,
                                                                    procedural_uv_generator_object_id       in_object_id,
                                                                    procedural_uv_generator_object_property in_property,
                                                                    const void*                             in_data)
{
    _procedural_uv_generator*        generator_ptr = reinterpret_cast<_procedural_uv_generator*>(in_generator);
    _procedural_uv_generator_object* object_ptr    = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(generator_ptr != nullptr,
                      "Input UV generator instance is nullptr");

    if (!system_resizable_vector_get_element_at(generator_ptr->objects,
                                                in_object_id,
                                               &object_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve object descriptor");

        goto end;
    }

    ASSERT_DEBUG_SYNC(object_ptr != nullptr,
                      "Object descriptor at specified index is nullptr");

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
    }

    /* All done */
end:
    ;
}

