/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "scalar_field/scalar_field_metaballs.h"
#include "system/system_log.h"


typedef struct _scalar_field_metaballs
{
    raGL_backend              backend;
    ral_context               context; /* DO NOT release */
    uint32_t                  global_wg_size[3];
    unsigned int              grid_size_xyz[3];
    bool                      is_update_needed;
    float*                    metaball_data;
    unsigned int              n_max_metaballs;
    unsigned int              n_metaballs;
    system_hashed_ansi_string name;
    ral_program               po;
    ral_program_block_buffer  po_props_ub;
    unsigned int              po_props_ub_bo_offset_metaball_data;
    unsigned int              po_props_ub_bo_offset_n_metaballs;
    uint32_t                  po_props_ub_bo_size;
    ral_buffer                scalar_field_bo;
    int                       sync_max_index;
    int                       sync_min_index;

    ral_present_task present_task_with_compute;
    ral_present_task present_task_wo_compute;

    REFCOUNT_INSERT_VARIABLES


    explicit _scalar_field_metaballs(ral_context               in_context,
                                     const unsigned int*       in_grid_size_xyz,
                                     system_hashed_ansi_string in_name)
    {
        memset(global_wg_size,
               0,
               sizeof(global_wg_size) );
        memcpy(grid_size_xyz,
               in_grid_size_xyz,
               sizeof(grid_size_xyz) );

        backend                             =  nullptr;
        context                             =  in_context;
        is_update_needed                    =  true;
        metaball_data                       =  nullptr;
        n_max_metaballs                     =  0;
        n_metaballs                         =  0;
        name                                =  in_name;
        po                                  =  nullptr;
        po_props_ub                         =  nullptr;
        po_props_ub_bo_offset_metaball_data = -1;
        po_props_ub_bo_offset_n_metaballs   = -1;
        po_props_ub_bo_size                 = 0;
        present_task_with_compute           = nullptr;
        present_task_wo_compute             = nullptr;
        scalar_field_bo                     = nullptr;
        sync_max_index                      = -1;
        sync_min_index                      = -1;

        ral_context_get_property(in_context,
                                 RAL_CONTEXT_PROPERTY_BACKEND,
                                &backend);
    }
} _scalar_field_metaballs;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(scalar_field_metaballs,
                               scalar_field_metaballs,
                              _scalar_field_metaballs);


/* Forward declarations */
PRIVATE void _scalar_field_metaballs_get_token_key_value_arrays    (ral_context                 context,
                                                                    const unsigned int*         grid_size_xyz,
                                                                    unsigned int                n_metaballs,
                                                                    system_hashed_ansi_string** out_token_key_array_ptr,
                                                                    system_hashed_ansi_string** out_token_value_array_ptr,
                                                                    unsigned int*               out_n_token_key_value_pairs_ptr,
                                                                    uint32_t*                   out_global_wg_size_uvec3_ptr);
PRIVATE void _scalar_field_metaballs_init                          (_scalar_field_metaballs*    metaballs_ptr);
PRIVATE void _scalar_field_metaballs_init_present_tasks            (_scalar_field_metaballs*    metaballs_ptr);
PRIVATE void _scalar_field_metaballs_release                       (void*                       metaballs);
PRIVATE void _scalar_field_metaballs_update_props_cpu_task_callback(void*                       metaballs_raw_ptr);


/** TODO */
PRIVATE void _scalar_field_metaballs_get_token_key_value_arrays(ral_context                  context,
                                                                const unsigned int*          grid_size_xyz,
                                                                unsigned int                 n_metaballs,
                                                                system_hashed_ansi_string**  out_token_key_array_ptr,
                                                                system_hashed_ansi_string**  out_token_value_array_ptr,
                                                                unsigned int*                out_n_token_key_value_pairs_ptr,
                                                                uint32_t*                    out_global_wg_size_uvec3_ptr)
{
    const uint32_t* max_compute_work_group_count       = nullptr;
    uint32_t        max_compute_work_group_invocations = 0;
    const uint32_t* max_compute_work_group_size        = nullptr;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_COUNT,
                            &max_compute_work_group_count);
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                            &max_compute_work_group_invocations);
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,
                            &max_compute_work_group_size);

    *out_token_key_array_ptr   = new system_hashed_ansi_string[7];
    *out_token_value_array_ptr = new system_hashed_ansi_string[7];

    ASSERT_ALWAYS_SYNC(*out_token_key_array_ptr   != nullptr &&
                       *out_token_value_array_ptr != nullptr,
                       "Out of memory");

    (*out_token_key_array_ptr)[0] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_X"),
    (*out_token_key_array_ptr)[1] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Y"),
    (*out_token_key_array_ptr)[2] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Z"),
    (*out_token_key_array_ptr)[3] = system_hashed_ansi_string_create("BLOB_SIZE_X"),
    (*out_token_key_array_ptr)[4] = system_hashed_ansi_string_create("BLOB_SIZE_Y"),
    (*out_token_key_array_ptr)[5] = system_hashed_ansi_string_create("BLOB_SIZE_Z"),
    (*out_token_key_array_ptr)[6] = system_hashed_ansi_string_create("N_METABALLS");

    *out_n_token_key_value_pairs_ptr = 7;

    /* Compute global work-group size */
    const uint32_t n_total_scalars  = grid_size_xyz[0] * grid_size_xyz[1] * grid_size_xyz[2];
    const GLint    wg_local_size_x  = max_compute_work_group_size[0]; /* TODO: smarterize me */
    const GLint    wg_local_size_y  = 1;
    const GLint    wg_local_size_z  = 1;

    out_global_wg_size_uvec3_ptr[0] = 1 + n_total_scalars / wg_local_size_x;
    out_global_wg_size_uvec3_ptr[1] = 1;
    out_global_wg_size_uvec3_ptr[2] = 1;

    ASSERT_DEBUG_SYNC(static_cast<uint32_t>(wg_local_size_x * wg_local_size_y * wg_local_size_z) <= max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(out_global_wg_size_uvec3_ptr[0] < max_compute_work_group_count[0] &&
                      out_global_wg_size_uvec3_ptr[1] < max_compute_work_group_count[1] &&
                      out_global_wg_size_uvec3_ptr[2] < max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    /* Fill the token value array */
    char temp_buffer[64];

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_x);
    (*out_token_value_array_ptr)[0] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_y);
    (*out_token_value_array_ptr)[1] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_z);
    (*out_token_value_array_ptr)[2] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             grid_size_xyz[0]);
    (*out_token_value_array_ptr)[3] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             grid_size_xyz[1]);
    (*out_token_value_array_ptr)[4] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             grid_size_xyz[2]);
    (*out_token_value_array_ptr)[5] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             n_metaballs);
    (*out_token_value_array_ptr)[6] = system_hashed_ansi_string_create(temp_buffer);
}

/** TODO */
PRIVATE void _scalar_field_metaballs_init(_scalar_field_metaballs* metaballs_ptr)
{
    ral_shader                 cs                      = nullptr;
    unsigned int               n_token_key_value_pairs = 0;
    system_hashed_ansi_string* token_key_array_ptr     = nullptr;
    system_hashed_ansi_string* token_value_array_ptr   = nullptr;

    const char* cs_body_template = "#version 430 core\n"
                                   "\n"
                                   "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = LOCAL_WG_SIZE_Y, local_size_z = LOCAL_WG_SIZE_Z) in;\n"
                                   "\n"
                                   "layout(std140) uniform props\n"
                                   "{\n"
                                   "    uint n_metaballs;\n"
                                   "    vec4 metaball_data[N_METABALLS];\n"
                                   "};\n"
                                   "\n"
                                   "layout(std430) writeonly buffer data\n"
                                   "{\n"
                                   /* TODO: SIMDify me */
                                   "    restrict float result[];\n"
                                   "};\n"
                                   "\n"
                                   "void main()\n"
                                   "{\n"
                                   "    const uint  global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * LOCAL_WG_SIZE_Y) +\n"
                                   "                                             gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)                   +\n"
                                   "                                             gl_GlobalInvocationID.x);\n"
                                   "    const uvec3 cube_xyz                  = uvec3( global_invocation_id_flat                                % BLOB_SIZE_X,\n"
                                   "                                                  (global_invocation_id_flat /  BLOB_SIZE_X)                % BLOB_SIZE_Y,\n"
                                   "                                                  (global_invocation_id_flat / (BLOB_SIZE_X * BLOB_SIZE_Y)) );\n"
                                   "\n"
                                   "    if (cube_xyz.z >= BLOB_SIZE_Z)\n"
                                   "    {\n"
                                   "        return;\n"
                                   "    }\n"
                                   "\n"
                                   "    const vec3 cube_xyz_normalized = (vec3(cube_xyz) + vec3(0.5)) / vec3(BLOB_SIZE_X, BLOB_SIZE_Y, BLOB_SIZE_Z);\n"
                                   "\n"
                                   /* As per Wyvill's et al paper called "Data structure for soft objects" */
                                   "    float power_sum = 0.0;\n"
                                   "\n"
                                   "    for (uint n_metaball = 0;\n"
                                   "              n_metaball < n_metaballs;\n"
                                   "            ++n_metaball)\n"
                                   "    {\n"
                                   "        float dist   = distance(metaball_data[n_metaball].yzw, cube_xyz_normalized);\n"
                                   "        float dist_2 = dist                        * dist;\n"
                                   "        float dist_3 = dist_2                      * dist;\n"
                                   "        float size_2 = metaball_data[n_metaball].x * metaball_data[n_metaball].x;\n"
                                   "        float size_3 = size_2                      * metaball_data[n_metaball].x;\n"
                                   "        float power  = clamp(2.0 * dist_3 / size_3 - 3.0 * dist_2 / size_2 + 1.0, 0.0, 1.0);\n"
                                   "\n"
                                   "        power_sum += power;\n"
                                   "    }\n"
                                   "\n"
                                   "    result[global_invocation_id_flat] = power_sum;\n"
                                   "}";

    _scalar_field_metaballs_get_token_key_value_arrays(metaballs_ptr->context,
                                                       metaballs_ptr->grid_size_xyz,
                                                       metaballs_ptr->n_max_metaballs,
                                                      &token_key_array_ptr,
                                                      &token_value_array_ptr,
                                                      &n_token_key_value_pairs,
                                                       metaballs_ptr->global_wg_size);

    /* Create program & shader objects */
    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(metaballs_ptr->name),
                                                                " PO")
    };

    const ral_shader_create_info shader_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(metaballs_ptr->name),
                                                                " CS"),
        RAL_SHADER_TYPE_COMPUTE
    };

    if (!ral_context_create_shaders(metaballs_ptr->context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &cs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed");
    }

    if (!ral_context_create_programs(metaballs_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &metaballs_ptr->po) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed");
    }

    /* Configure the shader object */
    const system_hashed_ansi_string cs_body = system_hashed_ansi_string_create_by_token_replacement(cs_body_template,
                                                                                                    n_token_key_value_pairs,
                                                                                                    token_key_array_ptr,
                                                                                                    token_value_array_ptr);

    ral_shader_set_property(cs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &cs_body);

    delete [] token_key_array_ptr;
    token_key_array_ptr = nullptr;

    delete [] token_value_array_ptr;
    token_value_array_ptr = nullptr;

    /* Configure & link the program object */
    if (!ral_program_attach_shader(metaballs_ptr->po,
                                   cs,
                                   true /* async */))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to attach RAL shader to the metaballs RAL program");
    }

    /* Prepare a BO which is going to hold the scalar field data */
    ral_buffer_create_info scalar_field_buffer_create_info;

    scalar_field_buffer_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    scalar_field_buffer_create_info.property_bits    = 0;
    scalar_field_buffer_create_info.size             = sizeof(float)                   *
                                                       metaballs_ptr->grid_size_xyz[0] *
                                                       metaballs_ptr->grid_size_xyz[1] *
                                                       metaballs_ptr->grid_size_xyz[2];
    scalar_field_buffer_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT;
    scalar_field_buffer_create_info.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(metaballs_ptr->context,
                               1, /* n_buffers */
                              &scalar_field_buffer_create_info,
                              &metaballs_ptr->scalar_field_bo);

    /* Retrieve properties of the "props" UB */
    ral_buffer                  props_ub_bo_ral                        = nullptr;
    const ral_program_variable* uniform_metaball_data_variable_ral_ptr = nullptr;
    const ral_program_variable* uniform_n_metaballs_variable_ral_ptr   = nullptr;

    metaballs_ptr->po_props_ub = ral_program_block_buffer_create(metaballs_ptr->context,
                                                                 metaballs_ptr->po,
                                                                 system_hashed_ansi_string_create("props") );

    ral_program_block_buffer_get_property(metaballs_ptr->po_props_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &props_ub_bo_ral);
    ral_buffer_get_property              (props_ub_bo_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &metaballs_ptr->po_props_ub_bo_size);

    ral_program_get_block_variable_by_name(metaballs_ptr->po,
                                           system_hashed_ansi_string_create("props"),
                                           system_hashed_ansi_string_create("metaball_data[0]"),
                                          &uniform_metaball_data_variable_ral_ptr);
    ral_program_get_block_variable_by_name(metaballs_ptr->po,
                                           system_hashed_ansi_string_create("props"),
                                           system_hashed_ansi_string_create("n_metaballs"),
                                          &uniform_n_metaballs_variable_ral_ptr);

    metaballs_ptr->po_props_ub_bo_offset_metaball_data = uniform_metaball_data_variable_ral_ptr->block_offset;
    metaballs_ptr->po_props_ub_bo_offset_n_metaballs   = uniform_n_metaballs_variable_ral_ptr->block_offset;

    /* All done! */
    ral_context_delete_objects(metaballs_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               (const void**) &cs);
}

/** TODO */
PRIVATE void _scalar_field_metaballs_init_present_tasks(_scalar_field_metaballs* metaballs_ptr)
{
    /* First, create the present task which is going to be returned if the scalar field buffer
     * does not need an update. In this case, the task is effectively a nop. */
    {
        ral_present_task_gpu_create_info present_task_create_info;
        ral_present_task_io              present_task_unique_output;

        present_task_unique_output.buffer      = metaballs_ptr->scalar_field_bo;
        present_task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        present_task_create_info.command_buffer   = nullptr;
        present_task_create_info.n_unique_inputs  = 0;
        present_task_create_info.n_unique_outputs = 1;
        present_task_create_info.unique_inputs    = nullptr;
        present_task_create_info.unique_outputs   = &present_task_unique_output;

        metaballs_ptr->present_task_wo_compute = ral_present_task_create_gpu(system_hashed_ansi_string_create("Scalar field metaballs: NOP"),
                                                                            &present_task_create_info);
    }

    /* Now, for the other case.. */
    {
        /* Bake the command buffer. */
        ral_command_buffer             command_buffer;
        ral_command_buffer_create_info command_buffer_create_info;
        ral_buffer                     po_props_ub_bo_ral;

        ral_program_block_buffer_get_property(metaballs_ptr->po_props_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &po_props_ub_bo_ral);

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT;
        command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        command_buffer_create_info.is_resettable                           = false;
        command_buffer_create_info.is_transient                            = false;

        command_buffer = ral_command_buffer_create(metaballs_ptr->context,
                                                  &command_buffer_create_info);

        ral_command_buffer_start_recording(command_buffer);
        {
            ral_command_buffer_set_binding_command_info binding_info[2];
            uint32_t                                    scalar_field_bo_size = 0;

            ral_buffer_get_property(metaballs_ptr->scalar_field_bo,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &scalar_field_bo_size);

            binding_info[0].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
            binding_info[0].name                          = system_hashed_ansi_string_create("data");
            binding_info[0].storage_buffer_binding.buffer = metaballs_ptr->scalar_field_bo;
            binding_info[0].storage_buffer_binding.offset = 0;
            binding_info[0].storage_buffer_binding.size   = scalar_field_bo_size;

            binding_info[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            binding_info[1].name                          = system_hashed_ansi_string_create("props");
            binding_info[1].uniform_buffer_binding.buffer = po_props_ub_bo_ral;
            binding_info[1].uniform_buffer_binding.offset = 0;
            binding_info[1].uniform_buffer_binding.size   = metaballs_ptr->po_props_ub_bo_size;

            ral_command_buffer_record_set_bindings(command_buffer,
                                                   sizeof(binding_info) / sizeof(binding_info[0]),
                                                   binding_info);
            ral_command_buffer_record_set_program(command_buffer,
                                                  metaballs_ptr->po);
            ral_command_buffer_record_dispatch   (command_buffer,
                                                  metaballs_ptr->global_wg_size);
        }
        ral_command_buffer_stop_recording(command_buffer);

        /* ..and proceed with the present tasks */
        ral_present_task                    present_task_cpu;
        ral_present_task_cpu_create_info    present_task_cpu_create_info;
        ral_present_task_io                 present_task_cpu_unique_output;
        ral_present_task                    present_task_gpu;
        ral_present_task_gpu_create_info    present_task_gpu_create_info;
        ral_present_task_io                 present_task_gpu_unique_input;
        ral_present_task_io                 present_task_gpu_unique_output;
        ral_present_task_group_create_info  result_present_task_create_info;
        ral_present_task_ingroup_connection result_present_task_ingroup_connection;
        ral_present_task                    result_present_task_present_tasks[2];
        ral_present_task_group_mapping      result_present_task_unique_output_mapping;

        present_task_cpu_unique_output.buffer      = metaballs_ptr->scalar_field_bo;
        present_task_cpu_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        present_task_cpu_create_info.cpu_task_callback_user_arg = metaballs_ptr;
        present_task_cpu_create_info.n_unique_inputs            = 0;
        present_task_cpu_create_info.n_unique_outputs           = 1;
        present_task_cpu_create_info.pfn_cpu_task_callback_proc = _scalar_field_metaballs_update_props_cpu_task_callback;
        present_task_cpu_create_info.unique_inputs              = nullptr;
        present_task_cpu_create_info.unique_outputs             = &present_task_cpu_unique_output;

        present_task_cpu = ral_present_task_create_cpu(system_hashed_ansi_string_create("Scalar field metaballs: Props update"),
                                                      &present_task_cpu_create_info);


        present_task_gpu_unique_input.buffer       = po_props_ub_bo_ral;
        present_task_gpu_unique_input.object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
        present_task_gpu_unique_output.buffer      = metaballs_ptr->scalar_field_bo;
        present_task_gpu_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        present_task_gpu_create_info.command_buffer   = nullptr;
        present_task_gpu_create_info.n_unique_inputs  = 1;
        present_task_gpu_create_info.n_unique_outputs = 1;
        present_task_gpu_create_info.unique_inputs    = &present_task_gpu_unique_input;
        present_task_gpu_create_info.unique_outputs   = &present_task_gpu_unique_output;

        present_task_gpu = ral_present_task_create_gpu(system_hashed_ansi_string_create("Scalar field metaballs: Update"),
                                                                                       &present_task_gpu_create_info);


        result_present_task_present_tasks[0] = present_task_cpu;
        result_present_task_present_tasks[1] = present_task_gpu;

        result_present_task_ingroup_connection.input_present_task_index     = 1; /* po_props_ub_bo_ral */
        result_present_task_ingroup_connection.input_present_task_io_index  = 0;
        result_present_task_ingroup_connection.output_present_task_index    = 0; /* po_props_ub_bo_ral */
        result_present_task_ingroup_connection.output_present_task_io_index = 0;

        result_present_task_unique_output_mapping.io_index       = 0;
        result_present_task_unique_output_mapping.n_present_task = 1; /* scalar_field_bo */

        result_present_task_create_info.ingroup_connections                   = &result_present_task_ingroup_connection;
        result_present_task_create_info.n_ingroup_connections                 = 1;
        result_present_task_create_info.n_present_tasks                       = sizeof(result_present_task_present_tasks) / sizeof(result_present_task_present_tasks[0]);
        result_present_task_create_info.n_unique_inputs                       = 0;
        result_present_task_create_info.n_unique_outputs                      = 1;
        result_present_task_create_info.present_tasks                         = result_present_task_present_tasks;
        result_present_task_create_info.unique_input_to_ingroup_task_mapping  = nullptr;
        result_present_task_create_info.unique_output_to_ingroup_task_mapping = &result_present_task_unique_output_mapping;

        metaballs_ptr->present_task_with_compute = ral_present_task_create_group(&result_present_task_create_info);

        /* Clean up */
        ral_command_buffer_release(command_buffer);
        ral_present_task_release  (present_task_cpu);
        ral_present_task_release  (present_task_gpu);
    }
}

/** Please see header for specification */
PRIVATE void _scalar_field_metaballs_release(void* metaballs)
{
    _scalar_field_metaballs* metaballs_ptr = reinterpret_cast<_scalar_field_metaballs*>(metaballs);

    ral_present_task* present_tasks_to_release[] =
    {
        &metaballs_ptr->present_task_with_compute,
        &metaballs_ptr->present_task_wo_compute,
    };
    const uint32_t n_present_tasks_to_release = sizeof(present_tasks_to_release) / sizeof(present_tasks_to_release[0]);

    for (uint32_t n_present_task = 0;
                  n_present_task < n_present_tasks_to_release;
                ++n_present_task)
    {
        ral_present_task_release(*(present_tasks_to_release[n_present_task]));

        *present_tasks_to_release[n_present_task] = nullptr;
    }

    if (metaballs_ptr->po != nullptr)
    {
        ral_context_delete_objects(metaballs_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &metaballs_ptr->po);

        metaballs_ptr->po = nullptr;
    }

    if (metaballs_ptr->po_props_ub != nullptr)
    {
        ral_program_block_buffer_release(metaballs_ptr->po_props_ub);

        metaballs_ptr->po_props_ub = nullptr;
    }

    if (metaballs_ptr->scalar_field_bo != nullptr)
    {
        ral_context_delete_objects(metaballs_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   (const void**) &metaballs_ptr->scalar_field_bo);

        metaballs_ptr->scalar_field_bo = nullptr;
    }

    /* Release any memory buffers allocated during init time */
    if (metaballs_ptr->metaball_data != nullptr)
    {
        delete [] metaballs_ptr->metaball_data;

        metaballs_ptr->metaball_data = nullptr;
    }
}

/** TODO */
PRIVATE void _scalar_field_metaballs_update_props_cpu_task_callback(void* metaballs_raw_ptr)
{
    _scalar_field_metaballs* metaballs_ptr = reinterpret_cast<_scalar_field_metaballs*>(metaballs_raw_ptr);

    /* Update the metaball props UB if necessary */
    ral_program_block_buffer_set_nonarrayed_variable_value(metaballs_ptr->po_props_ub,
                                                           metaballs_ptr->po_props_ub_bo_offset_n_metaballs,
                                                          &metaballs_ptr->n_metaballs,
                                                           sizeof(unsigned int) );

    if (metaballs_ptr->sync_max_index != -1 &&
        metaballs_ptr->sync_min_index != -1)
    {
        const unsigned int n_metaballs_to_update = (metaballs_ptr->sync_max_index - metaballs_ptr->sync_min_index + 1);

        ral_program_block_buffer_set_arrayed_variable_value(metaballs_ptr->po_props_ub,
                                                            metaballs_ptr->po_props_ub_bo_offset_metaball_data,
                                                            metaballs_ptr->metaball_data + metaballs_ptr->sync_min_index * 4 /* size + xyz */,
                                                            sizeof(float) * n_metaballs_to_update * 4 /* size + xyz */, /* src_data_size         */
                                                            metaballs_ptr->sync_min_index,                              /* dst_array_start_index */
                                                            n_metaballs_to_update);                                     /* dst_array_item_count  */

        metaballs_ptr->sync_max_index = -1;
        metaballs_ptr->sync_min_index = -1;
    }

    ral_program_block_buffer_sync_immediately(metaballs_ptr->po_props_ub);
}


/** Please see header for specification */
PUBLIC EMERALD_API scalar_field_metaballs scalar_field_metaballs_create(ral_context               context,
                                                                        const unsigned int*       grid_size_xyz,
                                                                        system_hashed_ansi_string name)
{
    _scalar_field_metaballs* metaballs_ptr = new (std::nothrow) _scalar_field_metaballs(context,
                                                                                        grid_size_xyz,
                                                                                        name);

    ASSERT_DEBUG_SYNC(metaballs_ptr != nullptr,
                      "Out of memory");

    if (metaballs_ptr != nullptr)
    {
        uint32_t max_uniform_block_size = 0;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_MAX_UNIFORM_BLOCK_SIZE,
                                &max_uniform_block_size);

        /* Set up metaballs storage */
        metaballs_ptr->n_max_metaballs = (max_uniform_block_size - 4 * sizeof(unsigned int) ) /
                                         sizeof(float)                                        /
                                         4; /* size + xyz */

        metaballs_ptr->metaball_data = new float[4 /* size + xyz */ * metaballs_ptr->n_max_metaballs];

        /* Init renderer objects */
        _scalar_field_metaballs_init(metaballs_ptr);

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(metaballs_ptr,
                                                       _scalar_field_metaballs_release,
                                                       OBJECT_TYPE_SCALAR_FIELD_METABALLS,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Scalar Field (Metaballs)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (scalar_field_metaballs) metaballs_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task scalar_field_metaballs_get_present_task(scalar_field_metaballs metaballs)
{
    _scalar_field_metaballs* metaballs_ptr = reinterpret_cast<_scalar_field_metaballs*>(metaballs);
    ral_present_task         result;

    if (!metaballs_ptr->is_update_needed)
    {
        result = metaballs_ptr->present_task_with_compute;
    }
    else
    {
        result = metaballs_ptr->present_task_wo_compute;

        metaballs_ptr->is_update_needed = false;
    }

    /* All done */
    ral_present_task_retain(result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scalar_field_metaballs_get_property(scalar_field_metaballs          metaballs,
                                                            scalar_field_metaballs_property property,
                                                            void*                           out_result_ptr)
{
    _scalar_field_metaballs* metaballs_ptr = reinterpret_cast<_scalar_field_metaballs*>(metaballs);

    switch (property)
    {
        case SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_RAL:
        {
            *reinterpret_cast<ral_buffer*>(out_result_ptr) = metaballs_ptr->scalar_field_bo;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scalar_field_metaballs_property value.");
        }
    }
}

/** TODO */
PUBLIC EMERALD_API void scalar_field_metaballs_set_metaball_property(scalar_field_metaballs                   metaballs,
                                                                     scalar_field_metaballs_metaball_property property,
                                                                     unsigned int                             n_metaball,
                                                                     const void*                              data)
{
    _scalar_field_metaballs* metaballs_ptr = reinterpret_cast<_scalar_field_metaballs*>(metaballs);

    ASSERT_DEBUG_SYNC(n_metaball < metaballs_ptr->n_max_metaballs,
                      "Invalid metaball index");
    ASSERT_DEBUG_SYNC(n_metaball < metaballs_ptr->n_metaballs,
                       "Attempting to set a property value to a metaball with invalid index.");

    switch (property)
    {
        case SCALAR_FIELD_METABALLS_METABALL_PROPERTY_SIZE:
        {
            metaballs_ptr->metaball_data[n_metaball * 4 /* size + xyz */] = *reinterpret_cast<const float*>(data);

            break;
        }

        case SCALAR_FIELD_METABALLS_METABALL_PROPERTY_XYZ:
        {
            memcpy(metaballs_ptr->metaball_data + n_metaball * 4 + 1 /* size */,
                   data,
                   sizeof(float) * 3);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "scalar_field_metaballs_metaball_property value not recognized.");
        }
    }

    if (metaballs_ptr->sync_max_index <  (int) n_metaball ||
        metaballs_ptr->sync_max_index == -1)
    {
        metaballs_ptr->sync_max_index = (int) n_metaball;
    }

    if (metaballs_ptr->sync_min_index == -1         ||
        metaballs_ptr->sync_min_index >  (int) n_metaball)
    {
        metaballs_ptr->sync_min_index = n_metaball;
    }

    metaballs_ptr->is_update_needed = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scalar_field_metaballs_set_property(scalar_field_metaballs          metaballs,
                                                            scalar_field_metaballs_property property,
                                                            const void*                     data)
{
    _scalar_field_metaballs* metaballs_ptr = reinterpret_cast<_scalar_field_metaballs*>(metaballs);

    switch (property)
    {
        case SCALAR_FIELD_METABALLS_PROPERTY_N_METABALLS:
        {
            metaballs_ptr->is_update_needed = true;
            metaballs_ptr->n_metaballs      = *reinterpret_cast<const unsigned int*>(data);

            break;
        }
    }
}

