/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "scalar_field/scalar_field_metaballs.h"
#include "system/system_log.h"


typedef struct _scalar_field_metaballs
{
    ogl_context               context; /* DO NOT release */
    GLint                     global_wg_size[3];
    unsigned int              grid_size_xyz[3];
    bool                      is_update_needed;
    system_hashed_ansi_string name;
    ogl_program               po;
    GLuint                    scalar_field_bo_id;
    unsigned int              scalar_field_bo_size;
    unsigned int              scalar_field_bo_start_offset;

    REFCOUNT_INSERT_VARIABLES


    explicit _scalar_field_metaballs(ogl_context               in_context,
                                     const unsigned int*       in_grid_size_xyz,
                                     system_hashed_ansi_string in_name)
    {
        memset(global_wg_size,
               0,
               sizeof(global_wg_size) );
        memcpy(grid_size_xyz,
               in_grid_size_xyz,
               sizeof(grid_size_xyz) );

        context                      = in_context;
        is_update_needed             = true;
        name                         = in_name;
        po                           = NULL;
        scalar_field_bo_id           = 0;
        scalar_field_bo_size         = 0;
        scalar_field_bo_start_offset = 0;
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
    ogl_buffers                       buffers         = NULL;
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _scalar_field_metaballs*          metaballs_ptr   = (_scalar_field_metaballs*) user_arg;

    ogl_context_get_property(metaballs_ptr->context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);
    ogl_context_get_property(metaballs_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (metaballs_ptr->po != NULL)
    {
        ogl_program_release(metaballs_ptr->po);

        metaballs_ptr->po = NULL;
    }

    if (metaballs_ptr->scalar_field_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(buffers,
                                       metaballs_ptr->scalar_field_bo_id,
                                       metaballs_ptr->scalar_field_bo_start_offset);

        metaballs_ptr->scalar_field_bo_id           = 0;
        metaballs_ptr->scalar_field_bo_size         = 0;
        metaballs_ptr->scalar_field_bo_start_offset = 0;
    }
}

/** TODO */
PRIVATE void _scalar_field_metaballs_get_token_key_value_arrays(const ogl_context_gl_limits* limits_ptr,
                                                                const unsigned int*          grid_size_xyz,
                                                                system_hashed_ansi_string**  out_token_key_array_ptr,
                                                                system_hashed_ansi_string**  out_token_value_array_ptr,
                                                                unsigned int*                out_n_token_key_value_pairs_ptr,
                                                                GLint*                       out_global_wg_size_uvec3_ptr)
{
    *out_token_key_array_ptr   = new system_hashed_ansi_string[6];
    *out_token_value_array_ptr = new system_hashed_ansi_string[6];

    ASSERT_ALWAYS_SYNC(*out_token_key_array_ptr   != NULL &&
                       *out_token_value_array_ptr != NULL,
                       "Out of memory");

    (*out_token_key_array_ptr)[0] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_X"),
    (*out_token_key_array_ptr)[1] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Y"),
    (*out_token_key_array_ptr)[2] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Z"),
    (*out_token_key_array_ptr)[3] = system_hashed_ansi_string_create("BLOB_SIZE_X"),
    (*out_token_key_array_ptr)[4] = system_hashed_ansi_string_create("BLOB_SIZE_Y"),
    (*out_token_key_array_ptr)[5] = system_hashed_ansi_string_create("BLOB_SIZE_Z"),

    *out_n_token_key_value_pairs_ptr = 6;

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
}

/** TODO */
PRIVATE void _scalar_field_metaballs_init_rendering_thread_callback(ogl_context context,
                                                                    void*       user_arg)
{
    ogl_buffers                       buffers                 = NULL;
    ogl_shader                        cs                      = NULL;
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
                                   "layout(std430) writeonly buffer data\n"
                                   "{\n"
                                   /* TODO: SIMDify me */
                                   "    restrict float result[];\n"
                                   "};\n"
                                   "\n"
                                   /* TODO: Uniformify me */
                                   "const uvec3 blob_size = uvec3(BLOB_SIZE_X, BLOB_SIZE_Y, BLOB_SIZE_Z);\n"
                                   "\n"
                                   "void main()\n"
                                   "{\n"
                                   "    struct\n" /* TODO: Constifying this on Intel driver causes compilation errors? */
                                   "    {\n"
                                   "        vec3  xyz;\n"
                                   "        float radius;\n"
                                   "    } spheres[3] =\n"
                                   "    {\n"
                                   "        {vec3(0.17, 0.17, 0.17), 0.1 },\n"  /* corner case lol */
                                   "        {vec3(0.3,  0.3,  0.3),  0.2 },\n"
                                   "        {vec3(0.5,  0.5,  0.5),  0.4 } };\n"
                                   "    const uint n_spheres = 3;\n"
                                   "    \n"
                                   "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * LOCAL_WG_SIZE_Y) +\n"
                                   "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)                   +\n"
                                   "                                            gl_GlobalInvocationID.x);\n"
                                   "    const uint cube_x =  global_invocation_id_flat                                % blob_size.x;\n"
                                   "    const uint cube_y = (global_invocation_id_flat /  blob_size.x)                % blob_size.y;\n"
                                   "    const uint cube_z = (global_invocation_id_flat / (blob_size.x * blob_size.y));\n"
                                   "\n"
                                   "    if (cube_z >= blob_size.z)\n"
                                   "    {\n"
                                   "        return;\n"
                                   "    }\n"
                                   "\n"
                                   "    const float cube_x_normalized = (float(cube_x) + 0.5) / float(blob_size.x - 1);\n"
                                   "    const float cube_y_normalized = (float(cube_y) + 0.5) / float(blob_size.y - 1);\n"
                                   "    const float cube_z_normalized = (float(cube_z) + 0.5) / float(blob_size.z - 1);\n"
                                   "\n"
                                   "    float max_power = 0.0;\n"
                                   "\n"
                                   "    for (uint n_sphere = 0;\n"
                                   "              n_sphere < n_spheres;\n"
                                   "            ++n_sphere)\n"
                                   "    {\n"
                                   "        vec3  delta = vec3(cube_x_normalized, cube_y_normalized, cube_z_normalized) - spheres[n_sphere].xyz;\n"
                                   "        float power = 1.0 - clamp(length(delta) / spheres[n_sphere].radius, 0.0, 1.0);\n"
                                   "\n"
                                   "        max_power = max(max_power, power);\n"
                                   "    }\n"
                                   "\n"
                                   "    result[global_invocation_id_flat] = max_power;\n"
                                   "}";

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    _scalar_field_metaballs_get_token_key_value_arrays(limits_ptr,
                                                       metaballs_ptr->grid_size_xyz,
                                                      &token_key_array_ptr,
                                                      &token_value_array_ptr,
                                                      &n_token_key_value_pairs,
                                                       metaballs_ptr->global_wg_size);

    /* Create program & shader objects */
    cs = ogl_shader_create(metaballs_ptr->context,
                           SHADER_TYPE_COMPUTE,
                           system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(metaballs_ptr->name),
                                                                                   " CS") );

    metaballs_ptr->po = ogl_program_create(metaballs_ptr->context,
                                           system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(metaballs_ptr->name),
                                                                                   " CS"),
                                           OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE);

    /* Configure the shader object */
    ogl_shader_set_body_with_token_replacement(cs,
                                               cs_body_template,
                                               n_token_key_value_pairs,
                                               token_key_array_ptr,
                                               token_value_array_ptr);

    delete [] token_key_array_ptr;
    token_key_array_ptr = NULL;

    delete [] token_value_array_ptr;
    token_value_array_ptr = NULL;

    /* Configure & link the program object */
    ogl_program_attach_shader(metaballs_ptr->po,
                              cs);

    ogl_program_link(metaballs_ptr->po);

    /* Prepare a BO which is going to hold the scalar field data */
    ogl_context_get_property(metaballs_ptr->context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);

    metaballs_ptr->scalar_field_bo_size = sizeof(float) *
                                          metaballs_ptr->grid_size_xyz[0] *
                                          metaballs_ptr->grid_size_xyz[1] *
                                          metaballs_ptr->grid_size_xyz[2];

    ogl_buffers_allocate_buffer_memory(buffers,
                                       metaballs_ptr->scalar_field_bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                       0, /* flags */
                                      &metaballs_ptr->scalar_field_bo_id,
                                      &metaballs_ptr->scalar_field_bo_start_offset);

    /* All done! */
    ogl_shader_release(cs);
}

/** Please see header for specification */
PRIVATE void _scalar_field_metaballs_release(void* metaballs)
{
    _scalar_field_metaballs* metaballs_ptr = (_scalar_field_metaballs*) metaballs;

    ogl_context_request_callback_from_context_thread(metaballs_ptr->context,
                                                     _scalar_field_metaballs_deinit_rendering_thread_callback,
                                                     metaballs_ptr);
}


/** Please see header for specification */
PUBLIC EMERALD_API scalar_field_metaballs scalar_field_metaballs_create(ogl_context               context,
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
        ogl_context_request_callback_from_context_thread(context,
                                                         _scalar_field_metaballs_init_rendering_thread_callback,
                                                         metaballs_ptr);

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
        case SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_ID:
        {
            *(GLuint*) out_result = metaballs_ptr->scalar_field_bo_id;

            break;
        }

        case SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_SIZE:
        {
            *(unsigned int*) out_result = metaballs_ptr->scalar_field_bo_size;

            break;
        }

        case SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_START_OFFSET:
        {
            *(unsigned int*) out_result = metaballs_ptr->scalar_field_bo_start_offset;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized scalar_field_metaballs_property value.");
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
        ogl_context_get_property(metaballs_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);

        /* Run the CS and generate the scalar field data */
        entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(metaballs_ptr->po) );
        entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                            0, /* index */
                                            metaballs_ptr->scalar_field_bo_id,
                                            metaballs_ptr->scalar_field_bo_start_offset,
                                            metaballs_ptr->scalar_field_bo_size);

        entrypoints_ptr->pGLDispatchCompute(metaballs_ptr->global_wg_size[0],
                                            metaballs_ptr->global_wg_size[1],
                                            metaballs_ptr->global_wg_size[2]);

        metaballs_ptr->is_update_needed = false;
        result                          = true;
    } /* if (metaballs_ptr->is_update_needed) */

    return result;
}