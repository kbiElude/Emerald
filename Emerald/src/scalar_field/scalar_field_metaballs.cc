/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "scalar_field/scalar_field_metaballs.h"
#include "system/system_log.h"


typedef struct _scalar_field_metaballs
{
    raGL_backend              backend;
    ral_context               context; /* DO NOT release */
    GLint                     global_wg_size[3];
    unsigned int              grid_size_xyz[3];
    bool                      is_update_needed;
    float*                    metaball_data;
    unsigned int              n_max_metaballs;
    unsigned int              n_max_metaballs_cross_platform;
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

        backend                             =  NULL;
        context                             =  in_context;
        is_update_needed                    =  true;
        metaball_data                       =  NULL;
        n_max_metaballs                     =  0;
        n_max_metaballs_cross_platform      =  0;
        n_metaballs                         =  0;
        name                                =  in_name;
        po                                  =  NULL;
        po_props_ub                         =  NULL;
        po_props_ub_bo_offset_metaball_data = -1;
        po_props_ub_bo_offset_n_metaballs   = -1;
        po_props_ub_bo_size                 = 0;
        scalar_field_bo                     = NULL;
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


/** TODO */
PRIVATE void _scalar_field_metaballs_deinit_rendering_thread_callback(ogl_context context,
                                                                      void*       user_arg)
{
    _scalar_field_metaballs* metaballs_ptr = (_scalar_field_metaballs*) user_arg;

    if (metaballs_ptr->po != NULL)
    {
        ral_context_delete_objects(metaballs_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &metaballs_ptr->po);

        metaballs_ptr->po = NULL;
    }

    if (metaballs_ptr->po_props_ub != NULL)
    {
        ral_program_block_buffer_release(metaballs_ptr->po_props_ub);

        metaballs_ptr->po_props_ub = NULL;
    }

    if (metaballs_ptr->scalar_field_bo != NULL)
    {
        ral_context_delete_objects(metaballs_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   (const void**) &metaballs_ptr->scalar_field_bo);

        metaballs_ptr->scalar_field_bo = NULL;
    }
}

/** TODO */
PRIVATE void _scalar_field_metaballs_get_token_key_value_arrays(const ogl_context_gl_limits* limits_ptr,
                                                                const unsigned int*          grid_size_xyz,
                                                                unsigned int                 n_metaballs,
                                                                system_hashed_ansi_string**  out_token_key_array_ptr,
                                                                system_hashed_ansi_string**  out_token_value_array_ptr,
                                                                unsigned int*                out_n_token_key_value_pairs_ptr,
                                                                GLint*                       out_global_wg_size_uvec3_ptr)
{
    *out_token_key_array_ptr   = new system_hashed_ansi_string[7];
    *out_token_value_array_ptr = new system_hashed_ansi_string[7];

    ASSERT_ALWAYS_SYNC(*out_token_key_array_ptr   != NULL &&
                       *out_token_value_array_ptr != NULL,
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
    const GLint    wg_local_size_x  = limits_ptr->max_compute_work_group_size[0]; /* TODO: smarterize me */
    const GLint    wg_local_size_y  = 1;
    const GLint    wg_local_size_z  = 1;

    out_global_wg_size_uvec3_ptr[0] = 1 + n_total_scalars / wg_local_size_x;
    out_global_wg_size_uvec3_ptr[1] = 1;
    out_global_wg_size_uvec3_ptr[2] = 1;

    ASSERT_DEBUG_SYNC(wg_local_size_x * wg_local_size_y * wg_local_size_z <= limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(out_global_wg_size_uvec3_ptr[0] < limits_ptr->max_compute_work_group_count[0] &&
                      out_global_wg_size_uvec3_ptr[1] < limits_ptr->max_compute_work_group_count[1] &&
                      out_global_wg_size_uvec3_ptr[2] < limits_ptr->max_compute_work_group_count[2],
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
PRIVATE void _scalar_field_metaballs_init_rendering_thread_callback(ogl_context context,
                                                                    void*       user_arg)
{
    ral_shader                        cs                      = NULL;
    const ogl_context_gl_entrypoints* entrypoints_ptr         = NULL;
    const ogl_context_gl_limits*      limits_ptr              = NULL;
    _scalar_field_metaballs*          metaballs_ptr           = (_scalar_field_metaballs*) user_arg;
    unsigned int                      n_token_key_value_pairs = 0;
    system_hashed_ansi_string*        token_key_array_ptr     = NULL;
    system_hashed_ansi_string*        token_value_array_ptr   = NULL;

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

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    _scalar_field_metaballs_get_token_key_value_arrays(limits_ptr,
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
    token_key_array_ptr = NULL;

    delete [] token_value_array_ptr;
    token_value_array_ptr = NULL;

    /* Configure & link the program object */
    if (!ral_program_attach_shader(metaballs_ptr->po,
                                   cs))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to attach RAL shader to the metaballs RAL program");
    }

    /* Retrieve properties of the "props" UB */
    const raGL_program          program_raGL                           = ral_context_get_program_gl(metaballs_ptr->context,
                                                                                                    metaballs_ptr->po);
    ral_buffer                  props_ub_bo_ral                        = NULL;
    const ral_program_variable* uniform_metaball_data_variable_ral_ptr = NULL;
    const ral_program_variable* uniform_n_metaballs_variable_ral_ptr   = NULL;

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

    /* All done! */
    ral_context_delete_objects(metaballs_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               (const void**) &cs);
}

/** Please see header for specification */
PRIVATE void _scalar_field_metaballs_release(void* metaballs)
{
    _scalar_field_metaballs* metaballs_ptr = (_scalar_field_metaballs*) metaballs;

    /* Request rendering thread call-back */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(metaballs_ptr->context),
                                                     _scalar_field_metaballs_deinit_rendering_thread_callback,
                                                     metaballs_ptr);

    /* Release any memory buffers allocated during init time */
    if (metaballs_ptr->metaball_data != NULL)
    {
        delete [] metaballs_ptr->metaball_data;

        metaballs_ptr->metaball_data = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API scalar_field_metaballs scalar_field_metaballs_create(ral_context               context,
                                                                        const unsigned int*       grid_size_xyz,
                                                                        system_hashed_ansi_string name)
{
    _scalar_field_metaballs* metaballs_ptr = new (std::nothrow) _scalar_field_metaballs(context,
                                                                                        grid_size_xyz,
                                                                                        name);

    ASSERT_DEBUG_SYNC(metaballs_ptr != NULL,
                      "Out of memory");

    if (metaballs_ptr != NULL)
    {
        /* Set up metaballs storage */
        const ogl_context_gl_limits* limits_ptr = NULL;

        ogl_context_get_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        metaballs_ptr->n_max_metaballs                = (limits_ptr->max_uniform_block_size - 4 * sizeof(unsigned int) ) /
                                                        sizeof(float)                                                    /
                                                        4; /* size + xyz */
        metaballs_ptr->n_max_metaballs_cross_platform = (limits_ptr->min_max_uniform_block_size - 4 * sizeof(unsigned int) ) /
                                                        sizeof(float)                                                        /
                                                        4; /* size + xyz */

        metaballs_ptr->metaball_data = new float[4 /* size + xyz */ * metaballs_ptr->n_max_metaballs];

        /* Call back the rendering thread to set up context-specific objects */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                         _scalar_field_metaballs_init_rendering_thread_callback,
                                                         metaballs_ptr);

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
PUBLIC EMERALD_API void scalar_field_metaballs_get_property(scalar_field_metaballs          metaballs,
                                                            scalar_field_metaballs_property property,
                                                            void*                           out_result)
{
    _scalar_field_metaballs* metaballs_ptr = (_scalar_field_metaballs*) metaballs;

    switch (property)
    {
        case SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_RAL:
        {
            *(ral_buffer*) out_result = metaballs_ptr->scalar_field_bo;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scalar_field_metaballs_property value.");
        }
    } /* switch (property) */
}

/** TODO */
PUBLIC EMERALD_API void scalar_field_metaballs_set_metaball_property(scalar_field_metaballs                   metaballs,
                                                                     scalar_field_metaballs_metaball_property property,
                                                                     unsigned int                             n_metaball,
                                                                     const void*                              data)
{
    _scalar_field_metaballs* metaballs_ptr = (_scalar_field_metaballs*) metaballs;

    ASSERT_DEBUG_SYNC(n_metaball < metaballs_ptr->n_max_metaballs,
                      "Invalid metaball index");
    ASSERT_DEBUG_SYNC(n_metaball < metaballs_ptr->n_metaballs,
                       "Attempting to set a property value to a metaball with invalid index.");

    if (n_metaball >= metaballs_ptr->n_max_metaballs_cross_platform)
    {
        LOG_ERROR("Metaball index [%u] may not be cross-platform",
                  n_metaball);
    } /* if (n_metaball < metaballs_ptr->n_max_metaballs_cross_platform) */

    switch (property)
    {
        case SCALAR_FIELD_METABALLS_METABALL_PROPERTY_SIZE:
        {
            metaballs_ptr->metaball_data[n_metaball * 4 /* size + xyz */] = *(float*) data;

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
    } /* switch (property) */

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
    _scalar_field_metaballs* metaballs_ptr = (_scalar_field_metaballs*) metaballs;

    switch (property)
    {
        case SCALAR_FIELD_METABALLS_PROPERTY_N_METABALLS:
        {
            metaballs_ptr->is_update_needed = true;
            metaballs_ptr->n_metaballs      = *(unsigned int*) data;

            ASSERT_DEBUG_SYNC(metaballs_ptr->n_metaballs < metaballs_ptr->n_max_metaballs_cross_platform,
                              "Requested number of metaballs may not be cross-platform.");

            break;
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API bool scalar_field_metaballs_update(scalar_field_metaballs metaballs)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _scalar_field_metaballs*          metaballs_ptr   = (_scalar_field_metaballs*) metaballs;
    bool                              result          = false;

    if (metaballs_ptr->is_update_needed)
    {
        ogl_context_get_property(ral_context_get_gl_context(metaballs_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);

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
        } /* if (metaball data needs an update) */

        ral_program_block_buffer_sync(metaballs_ptr->po_props_ub);

        /* Run the CS and generate the scalar field data */
        GLuint             po_props_ub_bo_id            = 0;
        raGL_buffer        po_props_ub_bo_raGL          = NULL;
        ral_buffer         po_props_ub_bo_ral           = NULL;
        uint32_t           po_props_ub_bo_start_offset  = -1;
        const raGL_program po_raGL                      = ral_context_get_program_gl(metaballs_ptr->context,
                                                                                     metaballs_ptr->po);
        GLuint             po_raGL_id                   = 0;
        GLuint             scalar_field_bo_id           = 0;
        raGL_buffer        scalar_field_bo_raGL         = NULL;
        uint32_t           scalar_field_bo_size         = 0;
        uint32_t           scalar_field_bo_start_offset = -1;

        raGL_program_get_property(po_raGL,
                                  RAGL_PROGRAM_PROPERTY_ID,
                                 &po_raGL_id);
        ral_buffer_get_property  (metaballs_ptr->scalar_field_bo,
                                  RAL_BUFFER_PROPERTY_SIZE,
                                 &scalar_field_bo_size);

        ral_program_block_buffer_get_property(metaballs_ptr->po_props_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &po_props_ub_bo_ral);

        raGL_backend_get_buffer(metaballs_ptr->backend,
                                po_props_ub_bo_ral,
                      (void**) &po_props_ub_bo_raGL);
        raGL_backend_get_buffer(metaballs_ptr->backend,
                                metaballs_ptr->scalar_field_bo,
                      (void**) &scalar_field_bo_raGL);

        raGL_buffer_get_property(po_props_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &po_props_ub_bo_id);
        raGL_buffer_get_property(po_props_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &po_props_ub_bo_start_offset);
        raGL_buffer_get_property(scalar_field_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &scalar_field_bo_id);
        raGL_buffer_get_property(scalar_field_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &scalar_field_bo_start_offset);

        entrypoints_ptr->pGLUseProgram     (po_raGL_id);
        entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                            0, /* index */
                                            scalar_field_bo_id,
                                            scalar_field_bo_start_offset,
                                            scalar_field_bo_size);
        entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                            0, /* index */
                                            po_props_ub_bo_id,
                                            po_props_ub_bo_start_offset,
                                            metaballs_ptr->po_props_ub_bo_size);

        entrypoints_ptr->pGLDispatchCompute(metaballs_ptr->global_wg_size[0],
                                            metaballs_ptr->global_wg_size[1],
                                            metaballs_ptr->global_wg_size[2]);

        metaballs_ptr->is_update_needed = false;
        result                          = true;
    } /* if (metaballs_ptr->is_update_needed) */

    return result;
}