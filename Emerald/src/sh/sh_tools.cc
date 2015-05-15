/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kernel.h"
#include "ocl/ocl_program.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "procedural/procedural_mesh_sphere.h"
#include "procedural/procedural_types.h"
#include "sh/sh_tools.h"
#include "sh/sh_types.h"
#include "shaders/shaders_embeddable_sh.h"
#include "system/system_log.h"
#include <sstream>
#include <string>

/* Internal variables */

/** Internal type definition */
const char* preview_generation_template_body = "#version 420 core\n"
                                               "\n"
                                               "#define N_BANDS (%d)\n"
                                               /** SH code goes here ==> */
                                               "%s\n"
                                               /** <==..and ends here */
                                               "uniform samplerBuffer input_light_samples_data;\n"
                                               "uniform samplerBuffer input_vertex_data;\n"
                                               "\n"
                                               "out result\n"
                                               "{\n"
                                               "    vec3 result_nonsh;\n"
                                               "} Out;\n"
                                               "\n"
                                               "\n"
                                               "void main()\n"
                                               "{\n"
                                               "    vec3 result = vec3(0.0);\n"
                                               "\n"
                                               "    vec3  normal = texelFetch(input_vertex_data, gl_VertexID).xyz;\n"
                                               "    float theta  = acos(normal.z);\n"
                                               "    float phi    = atan(normal.y, normal.x);\n"
                                               "\n"
                                               "    for (int l = 0; l < N_BANDS; ++l)\n"
                                               "    {\n"
                                               "        for (int m = -l; m <= l; ++m)\n"
                                               "        {\n"
                                               "            vec3 input_coeff = texelFetch(input_light_samples_data, l * (l + 1) + m).xyz;\n"
                                               "\n"
                                               "            result += spherical_harmonics(l, m, theta, phi) * input_coeff;\n"
                                               "        }\n"
                                               "    }\n"
                                               "\n"
                                               "    Out.result_nonsh = result;\n"
                                               "\n"
                                               "}\n";

static ogl_program _preview_generation_program                              = NULL;
static GLuint      _preview_generation_program_id                           = -1;
static ogl_shader  _preview_generation_vp_shader                            = NULL;
static GLuint      _preview_generation_input_light_samples_location         = -1;
static GLuint      _preview_generation_input_samples_theta_phi_location     = -1;
static GLuint      _preview_generation_reference_sphere_vertexes_location   = -1;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _sh_tools_verify_context_type(__in __notnull ogl_context);
#else
    #define _sh_tools_verify_context_type(x)
#endif

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _sh_tools_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "sh_tools is only supported under GL contexts")
    }
#endif

#ifdef INCLUDE_OPENCL
    typedef struct
    {
        bool        initialized;
        ocl_program program;
        ocl_kernel  program_kernel;
        cl_int      program_kernel_work_group_size;
    } _sh_generation_descriptor;

    typedef struct
    {
        bool        initialized;
        ocl_program program;
        ocl_kernel  program_kernel;        
    } _sh_sum_descriptor;

    /** TODO */
    _sh_generation_descriptor* _generation_descriptors = NULL;
    _sh_sum_descriptor*        _sum_descriptor         = NULL;

    #define GET_SH_GENERATION_DESCRIPTOR_INDEX(components, logic) (SH_COMPONENTS_LAST * logic + components)
#endif /* INCLUDE_OPENCL */


static GLuint _vaa_id = -1;


/* Please see header for specification */
PUBLIC EMERALD_API void sh_tools_deinit()
{
    if (_preview_generation_program != NULL)
    {
        ogl_program_release(_preview_generation_program);

        _preview_generation_program = NULL;
    }
    
    #ifdef INCLUDE_OPENCL
    {
        if (_generation_descriptors != NULL)
        {
            for (uint32_t n = 0; n < SH_COMPONENTS_LAST * SH_INTEGRATION_LOGIC_LAST; ++n)
            {
                if (_generation_descriptors[n].initialized)
                {
                    ocl_program_release(_generation_descriptors[n].program);
                    ocl_kernel_release (_generation_descriptors[n].program_kernel);
                }
            }

            delete [] _generation_descriptors;
            _generation_descriptors = NULL;
        }

        if (_sum_descriptor != NULL)
        {
            if (_sum_descriptor->initialized)
            {
                ocl_program_release(_sum_descriptor->program);
                ocl_kernel_release (_sum_descriptor->program_kernel);
            }

            delete _sum_descriptor;
            _sum_descriptor = NULL;
        }
    }
    #endif /* INCLUDE_OPENCL */
}

#ifdef INCLUDE_OPENCL
    /* Please see header for specification */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_unshadowed_transfer_constant_albedo_input_data(__in                                __notnull ocl_context   context,
                                                                                                                            __in                                          sh_components n_result_sh_components,
                                                                                                                            __in_ecount(n_result_sh_components)           const float*  albedo,
                                                                                                                            __in                                          uint32_t      n_normals,
                                                                                                                            __in                                __notnull const float*  normals,
                                                                                                                            __in                                          cl_mem        result_cl_buffer)
    {
        cl_int                    cl_error_code  = 0;
        float*                    data           = NULL;
        const ocl_11_entrypoints* entry_points   = ocl_get_entrypoints();
        uint32_t                  n_bytes_needed = sh_tools_get_diffuse_unshadowed_transfer_constant_albedo_input_data_size(n_result_sh_components, n_normals);
        bool                      result         = false;
        
        /* Arrange the data */
        data = (float*) new (std::nothrow) char[n_bytes_needed];
        
        ASSERT_ALWAYS_SYNC(data != NULL, "Out of memory");
        if (data == NULL)
        {
            goto end;
        }

        memcpy(data,                          albedo,  sizeof(float) * n_result_sh_components);
        memcpy(data + n_result_sh_components, normals, sizeof(float) * n_normals * 3);

        /* Copy the data into buffer */
        cl_error_code = entry_points->pCLEnqueueWriteBuffer(ocl_context_get_command_queue(context, 0), 
                                                            result_cl_buffer,
                                                            true, /* blocking write */
                                                            0,
                                                            n_bytes_needed,
                                                            data,
                                                            0,    /* no events in wait list */
                                                            NULL, /* no events in wait list */
                                                            NULL);/* we don't want a wait event */
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not store data in OpenCL buffer.");

        if (cl_error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Free user-space buffer */
        result = true;
end:    
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }

        return result;
    }

    /* Please see header for specification */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_unshadowed_transfer_per_vertex_albedo_input_data(__in                                           __notnull ocl_context   context,
                                                                                                                              __in                                                     sh_components n_result_sh_components,
                                                                                                                              __in                                                     uint32_t      n_vertices,
                                                                                                                              __in_ecount(n_result_sh_components*n_vertices) __notnull const float*  albedo,
                                                                                                                              __in_ecount(3*n_vertices)                      __notnull const float*  normals,
                                                                                                                              __in                                           __notnull cl_mem        result_cl_buffer)
    {
        cl_int                    cl_error_code  = 0;
        float*                    data           = NULL;
        const ocl_11_entrypoints* entry_points   = ocl_get_entrypoints();
        uint32_t                  n_bytes_needed = sh_tools_get_diffuse_unshadowed_transfer_per_vertex_albedo_input_data_size(n_result_sh_components, n_vertices); 
        bool                      result         = false;

        /* Arrange the data */
        data = (float*) new (std::nothrow) char[n_bytes_needed];
        
        ASSERT_ALWAYS_SYNC(data != NULL, "Out of memory");
        if (data == NULL)
        {
            goto end;
        }

        const float* albedo_traveller_ptr  = albedo;
        float*       data_traveller_ptr    = data;
        const float* normals_traveller_ptr = normals;

        for (uint32_t n_vertex = 0; n_vertex < n_vertices; ++n_vertex)
        {
            memcpy(data_traveller_ptr, albedo_traveller_ptr,  sizeof(float) * n_result_sh_components); albedo_traveller_ptr  += n_result_sh_components; data_traveller_ptr += n_result_sh_components;
            memcpy(data_traveller_ptr, normals_traveller_ptr, sizeof(float) * 3);                      normals_traveller_ptr += 3;                      data_traveller_ptr += 3;
        }

        /* Write the data to the buffer */
        cl_error_code = entry_points->pCLEnqueueWriteBuffer(ocl_context_get_command_queue(context, 0), 
                                                            result_cl_buffer,
                                                            true, /* blocking write */
                                                            0,
                                                            n_bytes_needed,
                                                            data,
                                                            0,    /* no events in wait list */
                                                            NULL, /* no events in wait list */
                                                            NULL);/* we don't want a wait event */
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not store data in OpenCL buffer.");

        if (cl_error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Free user-space buffer */
        result = true;
end:    
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }

        return result;
    }

    /* Please see header for specification */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_shadowed_transfer_constant_albedo_input_data(__in                                __notnull ocl_context   context,
                                                                                                                          __in                                          sh_components n_result_sh_components,
                                                                                                                          __in_ecount(n_result_sh_components)           const float*  albedo,
                                                                                                                          __in                                          uint32_t      n_normals,
                                                                                                                          __in                                __notnull const float*  normals,
                                                                                                                          __in                                          uint32_t      n_samples,
                                                                                                                          __in                                          uint32_t      n_sh_bands,
                                                                                                                          __in                                          cl_mem        visibility_cl_buffer,
                                                                                                                          __in                                          cl_mem        result_cl_buffer)
    {
        cl_int                    cl_error_code            = 0;
        cl_command_queue          command_queue_cl         = ocl_context_get_command_queue(context, 0);
        cl_context                context_cl               = ocl_context_get_context(context);
        float*                    data                     = NULL;
        const ocl_11_entrypoints* entry_points             = ocl_get_entrypoints();
        uint32_t                  n_bytes_needed_userspace = (n_result_sh_components + n_normals * 3) * sizeof(float);
        uint32_t                  n_bytes_needed_clspace   = sh_tools_get_diffuse_shadowed_transfer_constant_albedo_input_data_size(n_result_sh_components, n_normals, n_samples, n_sh_bands); 
        bool                      result                   = false;
        
        /* Arrange the data */
        data = (float*) new (std::nothrow) char[n_bytes_needed_userspace];
        
        ASSERT_ALWAYS_SYNC(data != NULL, "Out of memory");
        if (data == NULL)
        {
            goto end;
        }

        memcpy(data,                          albedo,  sizeof(float) * n_result_sh_components);
        memcpy(data + n_result_sh_components, normals, sizeof(float) * n_normals * 3);

        /* Copy user-space data to the buffer */
        cl_error_code = entry_points->pCLEnqueueWriteBuffer(command_queue_cl,
                                                            result_cl_buffer,
                                                            true, /* blocking write */
                                                            0,    /* no offset */
                                                            n_bytes_needed_userspace,
                                                            data,
                                                            0,    /* no events to wait on */
                                                            NULL, /* no events to wait on */
                                                            NULL);/* blocking write, don't need a wait event */
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not copy user-space data to the CL buffer");

        if (cl_error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Append CL-space visibility data to the buffer */
        cl_event wait_event = NULL;

        cl_error_code = entry_points->pCLEnqueueCopyBuffer(command_queue_cl,
                                                           visibility_cl_buffer,
                                                           result_cl_buffer,
                                                           0,                        /* source offset */
                                                           n_bytes_needed_userspace, /* destination offset */
                                                           n_bytes_needed_clspace - n_bytes_needed_userspace,
                                                           0,   /* no events to wait on */
                                                           NULL,/* no events to wait on */
                                                           &wait_event);

        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not append visibility data");

        if (cl_error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Wait till the operation finishes */
        cl_error_code = entry_points->pCLWaitForEvents(1, &wait_event);
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not wait for the append operation to finish");

        /* Free user-space buffer */
        result = true;
end:    
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }

        return result;
    }
    
    /* Please see header for specification */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_shadowed_transfer_per_vertex_albedo_input_data(__in                                           __notnull ocl_context   context,
                                                                                                                            __in                                                     sh_components n_result_sh_components,
                                                                                                                            __in                                                     uint32_t      n_vertices,
                                                                                                                            __in_ecount(n_result_sh_components*n_vertices) __notnull const float*  albedo,
                                                                                                                            __in_ecount(3*n_vertices)                      __notnull const float*  normals,
                                                                                                                            __in                                                     uint32_t      n_samples,
                                                                                                                            __in                                                     cl_mem        visibility_cl_buffer,
                                                                                                                            __in                                           __notnull cl_mem        result_cl_buffer)
    {
        cl_int                    cl_error_code            = 0;
        float*                    data                     = NULL;
        const ocl_11_entrypoints* entry_points             = ocl_get_entrypoints();
        uint32_t                  n_bytes_needed_userspace = (n_vertices * (n_result_sh_components + 3) ) * sizeof(float);
        uint32_t                  n_bytes_needed_clspace   = sh_tools_get_diffuse_shadowed_transfer_per_vertex_albedo_input_data_size(n_result_sh_components, n_vertices, n_samples); 
        bool                      result                   = false;        

        /* Arrange the data */
        data = (float*) new (std::nothrow) char[n_bytes_needed_userspace];
        
        ASSERT_ALWAYS_SYNC(data != NULL, "Out of memory");
        if (data == NULL)
        {
            goto end;
        }

        const float* albedo_traveller_ptr  = albedo;
        float*       data_traveller_ptr    = data;
        const float* normals_traveller_ptr = normals;

        for (uint32_t n_vertex = 0; n_vertex < n_vertices; ++n_vertex)
        {
            memcpy(data_traveller_ptr, albedo_traveller_ptr,  sizeof(float) * n_result_sh_components); albedo_traveller_ptr  += n_result_sh_components; data_traveller_ptr += n_result_sh_components;
            memcpy(data_traveller_ptr, normals_traveller_ptr, sizeof(float) * 3);                      normals_traveller_ptr += 3;                      data_traveller_ptr += 3;
        }

        /* Copy the data from the buffer we created */
        cl_command_queue command_queue_cl = ocl_context_get_command_queue(context, 0);

        cl_error_code = entry_points->pCLEnqueueWriteBuffer(command_queue_cl,
                                                            result_cl_buffer,
                                                            true,
                                                            0, /* no offset */
                                                            n_bytes_needed_userspace,
                                                            data,
                                                            0,
                                                            NULL,
                                                            NULL);

        if (cl_error_code != CL_SUCCESS)
        {
            LOG_INFO("Could not copy user-space data to VRAM buffer");

            goto end;
        }

        /* Now's the time to append visibility buffer contents */
        cl_event wait_event = NULL;

        cl_error_code = entry_points->pCLEnqueueCopyBuffer(command_queue_cl,
                                                           visibility_cl_buffer,
                                                           result_cl_buffer,
                                                           0,                        /* source offset */
                                                           n_bytes_needed_userspace, /* destination offset */
                                                           n_bytes_needed_clspace - n_bytes_needed_userspace,
                                                           0,   /* no events to wait on */
                                                           NULL,/* no events to wait on */
                                                           &wait_event);

        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not append visibility data");

        if (cl_error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Wait till the operation finishes */
        cl_error_code = entry_points->pCLWaitForEvents(1, &wait_event);
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not wait for the append operation to finish");

        /* Free user-space buffer */
        result = true;
end:    
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }

        return result;
    }

    /* Please see header for specification */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_shadowed_interreflected_transfer_per_vertex_albedo_input_data(__in                                            __notnull ocl_context   context,
                                                                                                                                           __in                                                      sh_components n_result_sh_components,
                                                                                                                                           __in                                                      uint32_t      n_vertices,
                                                                                                                                           __in_ecount(n_result_sh_components*n_vertices) __notnull  const float*  albedo,
                                                                                                                                           __in_ecount(3*n_vertices)                      __notnull  const float*  normals,
                                                                                                                                           __in                                                      uint32_t      n_samples,
                                                                                                                                           __in                                                      cl_mem        visibility_cl_buffer,
                                                                                                                                           __in                                                      cl_mem        triangle_hit_data_buffer,
                                                                                                                                           __in                                           __notnull  cl_mem        result_cl_buffer)
    {
        cl_int                    cl_error_code                     = 0;
        float*                    data                              = NULL;
        const ocl_11_entrypoints* entry_points                      = ocl_get_entrypoints();
        uint32_t                  n_bytes_needed_userspace          = (n_vertices * (n_result_sh_components + 3) ) * sizeof(float);
        uint32_t                  n_bytes_needed_visibility_clspace = 2 * n_vertices * n_samples                   * sizeof(float);
        uint32_t                  n_bytes_needed_hit_data_clspace   = 3 * n_vertices * n_samples                   * sizeof(uint32_t);
        bool                      result                            = false;

        ASSERT_DEBUG_SYNC(n_bytes_needed_userspace + n_bytes_needed_visibility_clspace + n_bytes_needed_hit_data_clspace == 
                          sh_tools_get_diffuse_shadowed_interreflected_transfer_per_vertex_albedo_input_data_size(n_result_sh_components, n_vertices, n_samples),
                          "Input data size mismatch");

        /* Arrange the data */
        data = (float*) new (std::nothrow) char[n_bytes_needed_userspace];
        
        ASSERT_ALWAYS_SYNC(data != NULL, "Out of memory");
        if (data == NULL)
        {
            goto end;
        }

        const float* albedo_traveller_ptr  = albedo;
        float*       data_traveller_ptr    = data;
        const float* normals_traveller_ptr = normals;

        for (uint32_t n_vertex = 0; n_vertex < n_vertices; ++n_vertex)
        {
            memcpy(data_traveller_ptr, albedo_traveller_ptr,  sizeof(float) * n_result_sh_components); albedo_traveller_ptr  += n_result_sh_components; data_traveller_ptr += n_result_sh_components;
            memcpy(data_traveller_ptr, normals_traveller_ptr, sizeof(float) * 3);                      normals_traveller_ptr += 3;                      data_traveller_ptr += 3;
        }

        /* Copy the data from the buffer we created */
        cl_command_queue command_queue_cl = ocl_context_get_command_queue(context, 0);

        cl_error_code = entry_points->pCLEnqueueWriteBuffer(command_queue_cl,
                                                            result_cl_buffer,
                                                            true,
                                                            0, /* no offset */
                                                            n_bytes_needed_userspace,
                                                            data,
                                                            0,
                                                            NULL,
                                                            NULL);

        if (cl_error_code != CL_SUCCESS)
        {
            LOG_INFO("Could not copy user-space data to VRAM buffer");

            goto end;
        }

        /* Now's the time to append visibility buffer contents */
        cl_event wait_event_1 = NULL;

        cl_error_code = entry_points->pCLEnqueueCopyBuffer(command_queue_cl,
                                                           visibility_cl_buffer,
                                                           result_cl_buffer,
                                                           0,                        /* source offset */
                                                           n_bytes_needed_userspace, /* destination offset */
                                                           n_bytes_needed_visibility_clspace,
                                                           0,   /* no events to wait on */
                                                           NULL,/* no events to wait on */
                                                           &wait_event_1);

        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not append visibility data");

        if (cl_error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Don't forget about hit data buffer */
        cl_event wait_event_2 = NULL;

        cl_error_code = entry_points->pCLEnqueueCopyBuffer(command_queue_cl,
                                                           triangle_hit_data_buffer,
                                                           result_cl_buffer,
                                                           0,                                                            /* source offset */
                                                           n_bytes_needed_userspace + n_bytes_needed_visibility_clspace, /* destination offset */
                                                           n_bytes_needed_hit_data_clspace,
                                                           0,                                                            /* no events to wait on */
                                                           NULL,                                                         /* no events to wait on */
                                                           &wait_event_2);
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not append hit data");

        /* Wait till the operation finishes */
        cl_event wait_events[] = {wait_event_1, wait_event_2};
        uint32_t n_wait_events = sizeof(wait_events) / sizeof(wait_events[0]);

        cl_error_code = entry_points->pCLWaitForEvents(n_wait_events, wait_events);
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not wait for the append operations to finish");

        /* Free user-space buffer */
        result = true;
end:    
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }

        return result;
    }

    /* Please see header for specification */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_sh_opencl(__in                     __notnull   ocl_context                            context,
                                                                               __in                     __notnull   cl_mem                                 input_data,
                                                                               __in                     __notnull   cl_mem                                 input_sh_coeffs_data,      /* input sh coeffs data */
                                                                               __in                     __notnull   cl_mem                                 input_sh_coeffs_unit_vecs, /* input sh coeffs unit vector data */
                                                                               __in                     __maybenull cl_mem                                 input_prev_pass_data,      /* optional, only used for interreflected diffuse transfer */
                                                                               __in                     __notnull   cl_mem                                 output_sh_coeffs,          /* result sh coeffs data */
                                                                               __in                                 uint32_t                               n_bands,
                                                                               __in                                 uint32_t                               n_samples,
                                                                               __in                                 uint32_t                               n_vertices,
                                                                               __in                     __maybenull PFNSHTOOLSGENERATESHOPENCLCALLBACKPROC on_processed_vertex_index_update_callback_proc,
                                                                               __in                     __maybenull void*                                  on_processed_vertex_index_update_callback_user_arg,
                                                                               __in                                 sh_components                          n_result_sh_components,
                                                                               __in                                 sh_integration_logic                   result_integration_logic)
    {
        bool result = true;

        const char* samples_generation_program_body_template               = "__kernel void generate_sh(__global       IN_DATATYPE*   input_data,\n"
                                                                             "                          __global const float*         sh_coeffs_data,\n"
                                                                             "                          __global const float*         sh_coeffs_unit_vecs,\n" 
                                                                             "                          __global       float*         result_data,\n"
                                                                             "                                   const unsigned int   n_coeffs,\n"
                                                                             "                                   const unsigned int   n_samples,\n"
                                                                             "                                   const unsigned int   n_vertices,\n"
                                                                             "                          __global       float*         former_pass_sh_coeffs_data)\n" // only used for diffuse interreflected transfer
                                                                             "{\n"
                                                                             "    const unsigned int n_coeff_all = get_global_offset(0) + get_local_size(0) * get_group_id(0) + get_local_id(0);\n"
                                                                             "    const unsigned int n_coeff     = n_coeff_all % n_coeffs;\n"
                                                                             "    const unsigned int n_vertex    = n_coeff_all / n_coeffs;\n"
                                                                             "    OUT_DATATYPE       result      = (OUT_DATATYPE)(0);\n"
                                                                             "\n"
                                                                             "    if (n_vertex >= n_vertices) return;\n"
                                                                             "\n"
                                                                             "    for (int n_sample = 0; n_sample < n_samples; ++n_sample)\n"
                                                                             "    {\n"
                                                                             "        float3 sh_unit_vec = vload3(n_sample, sh_coeffs_unit_vecs);\n"
                                                                             "\n"
                                                                             "        INTEGRATION_LOGIC\n"
                                                                             "    }\n"
                                                                             "\n"
                                                                             "    result *= (OUT_DATATYPE)(4.0 * 3.14152965) / (OUT_DATATYPE)(n_samples);\n"
                                                                             "\n"
                                                                             "    STORE_RESULT\n"
                                                                             "}\n";

        const char* visibility_from_uchar8_integration_logic_body          = "        if (input_data[n_vertex * n_samples + n_sample] == 0)\n"
                                                                             "            result += (OUT_DATATYPE)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n";

        const char* diffuse_unshadowed_transfer_constant_albedo_red_integration_logic_body = "        float3 normal = vload3(0, (__global float*) input_data + 1 + n_vertex * 3);\n"
                                                                                             "        float  albedo = input_data[0];\n"
                                                                                             "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                             "\n"
                                                                                             "        if (dot_pr > 0)\n"
                                                                                             "        {\n"
                                                                                             "            result += dot_pr * albedo * sh_coeffs_data[n_sample * n_coeffs + n_coeff];\n"
                                                                                             "        }\n";
        const char* diffuse_unshadowed_transfer_constant_albedo_rg_integration_logic_body  = "        float3 normal = vload3(0, (__global float*)input_data + 2 + n_vertex * 3);\n"
                                                                                             "        float2 albedo = vload2(0, (__global float*)input_data);\n"
                                                                                             "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                             "\n"
                                                                                             "        if (dot_pr > 0)\n"
                                                                                             "        {\n"
                                                                                             "            result += (float2)(dot_pr) * albedo * (float2)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                             "        }\n";
        const char* diffuse_unshadowed_transfer_constant_albedo_rgb_integration_logic_body = "        float3 normal = vload3(0, (__global float*)input_data + 3 + n_vertex * 3);\n"
                                                                                             "        float3 albedo = vload3(0, (__global float*)input_data);\n"
                                                                                             "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                             "\n"
                                                                                             "        if (dot_pr > 0)\n"
                                                                                             "        {\n"
                                                                                             "            result += (float3)(dot_pr) * albedo * (float3)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                             "        }\n";

        const char* diffuse_unshadowed_transfer_per_vertex_albedo_red_integration_logic_body = "        float3 normal = vload3(0, (__global float*)input_data + 4 * n_vertex + 1);\n"
                                                                                               "        float  albedo = vload3(0, (__global float*)input_data + 4 * n_vertex).xy;\n"
                                                                                               "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                               "\n"
                                                                                               "        if (dot_pr > 0)\n"
                                                                                               "        {\n"
                                                                                               "            result += dot_pr * albedo * (sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                               "        }\n";
        const char* diffuse_unshadowed_transfer_per_vertex_albedo_rg_integration_logic_body  = "        float3 normal = vload3(0, (__global float*)input_data + 5 * n_vertex + 2);\n"
                                                                                               "        float2 albedo = vload3(0, (__global float*)input_data + 5 * n_vertex).xy;\n"
                                                                                               "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                               "\n"
                                                                                               "        if (dot_pr > 0)\n"
                                                                                               "        {\n"
                                                                                               "            result += (float2)(dot_pr) * albedo * (float2)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                               "        }\n";
        const char* diffuse_unshadowed_transfer_per_vertex_albedo_rgb_integration_logic_body = "        float3 normal = vload3(n_vertex*2+1, (__global float*)input_data);\n"
                                                                                               "        float3 albedo = vload3(n_vertex*2,   (__global float*)input_data);\n"
                                                                                               "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                               "\n"
                                                                                               "        if (dot_pr > 0)\n"
                                                                                               "        {\n"
                                                                                               "            result += (float3)(dot_pr) * albedo * (float3)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                               "        }\n";

        const char* diffuse_shadowed_transfer_constant_albedo_red_integration_logic_body = "        float3 normal = vload3(0, (__global float*) input_data + 1 /* red */ + n_vertex * 3);\n"
                                                                                           "        float3 albedo = vload3(0, (__global float*) input_data);\n"
                                                                                           "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                           "\n"
                                                                                           "        if (dot_pr > 0)\n"
                                                                                           "        {\n"
                                                                                           "            unsigned char is_visible = input_data[4 * (1 /* red */ + n_vertices * 3) + n_vertex * n_samples + n_sample];\n"
                                                                                           "\n"
                                                                                           "            if (is_visible == 0)\n"
                                                                                           "            {\n"
                                                                                           "                result += (float3)(dot_pr) * albedo * (float3)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                           "            }\n"
                                                                                           "        }\n";
        const char* diffuse_shadowed_transfer_constant_albedo_rg_integration_logic_body = "        float3 normal = vload3(0, (__global float*) input_data + 2 /* RG */ + n_vertex * 3);\n"
                                                                                          "        float3 albedo = vload3(0, (__global float*) input_data);\n"
                                                                                          "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                          "\n"
                                                                                          "        if (dot_pr > 0)\n"
                                                                                          "        {\n"
                                                                                          "            unsigned char is_visible = input_data[4 * (2 /* RG */ + n_vertices * 3) + n_vertex * n_samples + n_sample];\n"
                                                                                          "\n"
                                                                                          "            if (is_visible == 0)\n"
                                                                                          "            {\n"
                                                                                          "                result += (float3)(dot_pr) * albedo * (float3)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                          "            }\n"
                                                                                          "        }\n";
        const char* diffuse_shadowed_transfer_constant_albedo_rgb_integration_logic_body = "        float3 normal = vload3(1 + n_vertex, (__global float*) input_data);\n"
                                                                                           "        float3 albedo = vload3(0,            (__global float*) input_data);\n"
                                                                                           "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                           "\n"
                                                                                           "        if (dot_pr > 0)\n"
                                                                                           "        {\n"
                                                                                           "            unsigned char is_hit = input_data[4 * (3 /* RGB */ + n_vertices * 3) + n_vertex * n_samples + n_sample];\n"
                                                                                           "\n"
                                                                                           "            if (is_hit == 0)\n"
                                                                                           "            {\n"
                                                                                           "                result += (float3)(dot_pr) * albedo * (float3)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                           "            }\n"
                                                                                           "        }\n";
        
        const char* diffuse_shadowed_transfer_per_vertex_albedo_red_integration_logic_body = "        float3 normal = vload3(0, (__global float*) input_data + n_vertex * 4 + 1);\n"
                                                                                             "        float  albedo = *((__global float*)input_data + n_vertex * 4) );\n"
                                                                                             "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                             "\n"
                                                                                             "        if (dot_pr > 0)\n"
                                                                                             "        {\n"
                                                                                             "            unsigned char is_hit = input_data[4 /* red + xyz */ * n_vertices * 4 /* float */ + n_vertex * n_samples + n_sample];\n"
                                                                                             "\n"
                                                                                             "            if (is_hit == 0)\n"
                                                                                             "            {\n"
                                                                                             "                result += dot_pr * albedo * (sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                             "            }\n"
                                                                                             "        }\n";
        const char* diffuse_shadowed_transfer_per_vertex_albedo_rg_integration_logic_body  = "        float3 normal = vload3(0, (__global float*) input_data + n_vertex * 5 + 2);\n"
                                                                                             "        float2 albedo = vload2(0, (__global float*) input_data + n_vertex * 5);\n"
                                                                                             "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                             "\n"
                                                                                             "        if (dot_pr > 0)\n"
                                                                                             "        {\n"
                                                                                             "            unsigned char is_hit = input_data[5 /* RG + xyz */ * n_vertices * 4 /* float */ + n_vertex * n_samples + n_sample];\n"
                                                                                             "\n"
                                                                                             "            if (is_hit == 0)\n"
                                                                                             "            {\n"
                                                                                             "                result += (float2)(dot_pr) * albedo * (float2)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                             "            }\n"
                                                                                             "        }\n";
        const char* diffuse_shadowed_transfer_per_vertex_albedo_rgb_integration_logic_body = "        float3 normal = vload3(0, (__global float*) input_data + 6 * n_vertex + 3);\n"
                                                                                             "        float3 albedo = vload3(0, (__global float*) input_data + 6 * n_vertex);\n"
                                                                                             "        float  dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                             "\n"
                                                                                             "        if (dot_pr > 0)\n"
                                                                                             "        {\n"
                                                                                             "            unsigned char is_hit = input_data[6 /* RGB + xyz */ * n_vertices * 4 /* float */ + n_vertex * n_samples + n_sample];\n"
                                                                                             "\n"
                                                                                             "            if (is_hit == 0)\n"
                                                                                             "            {\n"
                                                                                             "                result += (float3)(dot_pr) * albedo * (float3)(sh_coeffs_data[n_sample * n_coeffs + n_coeff]);\n"
                                                                                             "            }\n"
                                                                                             "        }\n";
        
        const char* diffuse_shadowed_interreflected_transfer_per_vertex_albedo_rgb_integration_logic_body = "float3 normal = vload3(0, (__global float*) input_data + 6 * n_vertex + 3);\n"
                                                                                                            "float3 albedo = vload3(0, (__global float*) input_data + 6 * n_vertex);\n"
                                                                                                            "float2 hit_uv = vload2(0, (__global float*) input_data + 6 * n_vertices + 2 * (n_samples * n_vertex + n_sample) );\n"
                                                                                                            "\n"
                                                                                                            "if (hit_uv.x != -1)\n"
                                                                                                            "{\n"
                                                                                                            "    float dot_pr = dot(sh_unit_vec, normal);\n"
                                                                                                            "\n"
                                                                                                            "    if (dot_pr > 0)\n"
                                                                                                            "    {\n"
                                                                                                            "        uint3 hit_point_ids = vload3(0, ((__global unsigned int*) input_data) + 6 * n_vertices + 2 * n_vertices * n_samples + 3 * (n_vertex * n_samples + n_sample) );\n"
                                                                                                            "        float w             = 1.0 - hit_uv.x - hit_uv.y;\n"
                                                                                                            "\n"
                                                                                                            "        float3 point1_sh_coeff;\n"
                                                                                                            "        float3 point2_sh_coeff;\n"
                                                                                                            "        float3 point3_sh_coeff;\n"
                                                                                                            "\n"
                                                                                                            "        point1_sh_coeff.x = former_pass_sh_coeffs_data[hit_point_ids.x * 3 * n_coeffs                + n_coeff];\n"
                                                                                                            "        point1_sh_coeff.y = former_pass_sh_coeffs_data[hit_point_ids.x * 3 * n_coeffs +     n_coeffs + n_coeff];\n"
                                                                                                            "        point1_sh_coeff.z = former_pass_sh_coeffs_data[hit_point_ids.x * 3 * n_coeffs + 2 * n_coeffs + n_coeff];\n"
                                                                                                            "        point2_sh_coeff.x = former_pass_sh_coeffs_data[hit_point_ids.y * 3 * n_coeffs                + n_coeff];\n"
                                                                                                            "        point2_sh_coeff.y = former_pass_sh_coeffs_data[hit_point_ids.y * 3 * n_coeffs +     n_coeffs + n_coeff];\n"
                                                                                                            "        point2_sh_coeff.z = former_pass_sh_coeffs_data[hit_point_ids.y * 3 * n_coeffs + 2 * n_coeffs + n_coeff];\n"
                                                                                                            "        point3_sh_coeff.x = former_pass_sh_coeffs_data[hit_point_ids.z * 3 * n_coeffs                + n_coeff];\n"
                                                                                                            "        point3_sh_coeff.y = former_pass_sh_coeffs_data[hit_point_ids.z * 3 * n_coeffs +     n_coeffs + n_coeff];\n"
                                                                                                            "        point3_sh_coeff.z = former_pass_sh_coeffs_data[hit_point_ids.z * 3 * n_coeffs + 2 * n_coeffs + n_coeff];\n"
                                                                                                            "\n"
                                                                                                            "        result += albedo * (float3) (dot_pr) * ((float3)(hit_uv.x) * point1_sh_coeff + (float3)(hit_uv.y) * point2_sh_coeff + (float3)(w) * point3_sh_coeff);\n"
                                                                                                            "    }\n"
                                                                                                            "}\n";

        const char* store_result_red_body = "    result_data[n_vertex * n_coeffs + n_coeff] = result.x;\n";

        const char* store_result_rg_body  = "    result_data[n_vertex * 2 * n_coeffs +            n_coeff] = result.x;\n"
                                            "    result_data[n_vertex * 2 * n_coeffs + n_coeffs + n_coeff] = result.y;\n";

        const char* store_result_rgb_body = "    result_data[n_vertex * 3 * n_coeffs +              n_coeff] = result.x;\n"
                                            "    result_data[n_vertex * 3 * n_coeffs +   n_coeffs + n_coeff] = result.y;\n"
                                            "    result_data[n_vertex * 3 * n_coeffs + 2*n_coeffs + n_coeff] = result.z;\n"
                                            "\n";
                                            
        /* Make sure generation descriptors array is available before continuing */
        if (_generation_descriptors == NULL)
        {
            _generation_descriptors = new (std::nothrow) _sh_generation_descriptor[SH_COMPONENTS_LAST * SH_INTEGRATION_LOGIC_LAST];

            ASSERT_ALWAYS_SYNC(_generation_descriptors != NULL, "Out of memory");
            if (_generation_descriptors != NULL)
            {
                for (uint32_t n = 0; n < SH_COMPONENTS_LAST * SH_INTEGRATION_LOGIC_LAST; ++n)
                {
                    _generation_descriptors[n].initialized = false;
                }
            }
        }

        /* Find the descriptor. If unavailable, spawn one */
        uint32_t descriptor_index = GET_SH_GENERATION_DESCRIPTOR_INDEX(n_result_sh_components, result_integration_logic);

        if (!_generation_descriptors[descriptor_index].initialized)
        {
            /* Prepare the body */
            std::string body_string = samples_generation_program_body_template;

            while (body_string.find("INTEGRATION_LOGIC") != std::string::npos)
            {
                switch (result_integration_logic)
                {
                    case SH_INTEGRATION_LOGIC_VISIBILITY_FROM_UCHAR8:
                    {
                        body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), visibility_from_uchar8_integration_logic_body);
                        
                        break;
                    }

                    case SH_INTEGRATION_LOGIC_DIFFUSE_UNSHADOWED_TRANSFER_CONSTANT_ALBEDO:
                    {
                        switch(n_result_sh_components)
                        {
                            case SH_COMPONENTS_RED:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_unshadowed_transfer_constant_albedo_red_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RG:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_unshadowed_transfer_constant_albedo_rg_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RGB:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_unshadowed_transfer_constant_albedo_rgb_integration_logic_body);

                                break;
                            }

                            default:
                            {
                                ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");

                                break;
                            }
                        }

                        break;
                    }

                    case SH_INTEGRATION_LOGIC_DIFFUSE_UNSHADOWED_TRANSFER_PER_VERTEX_ALBEDO:
                    {
                        switch(n_result_sh_components)
                        {
                            case SH_COMPONENTS_RED:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_unshadowed_transfer_per_vertex_albedo_red_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RG:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_unshadowed_transfer_per_vertex_albedo_rg_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RGB:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_unshadowed_transfer_per_vertex_albedo_rgb_integration_logic_body);

                                break;
                            }

                            default:
                            {
                                ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");

                                break;
                            }
                        }

                        break;
                    }

                    case SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_CONSTANT_ALBEDO:
                    {
                        switch(n_result_sh_components)
                        {
                            case SH_COMPONENTS_RED:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_transfer_constant_albedo_red_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RG:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_transfer_constant_albedo_rg_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RGB:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_transfer_constant_albedo_rgb_integration_logic_body);

                                break;
                            }

                            default:
                            {
                                ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");

                                break;
                            }                            
                        }

                        break;
                    }

                    case SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_PER_VERTEX_ALBEDO:
                    {
                        switch(n_result_sh_components)
                        {
                            case SH_COMPONENTS_RED:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_transfer_per_vertex_albedo_red_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RG:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_transfer_per_vertex_albedo_rg_integration_logic_body);

                                break;
                            }

                            case SH_COMPONENTS_RGB:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_transfer_per_vertex_albedo_rgb_integration_logic_body);

                                break;
                            }

                            default:
                            {
                                ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");

                                break;
                            }                            
                        }

                        break;
                    }

                    case SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_INTERREFLECTED_PER_VERTEX_ALBEDO:
                    {
                        switch(n_result_sh_components)
                        {
                            case SH_COMPONENTS_RGB:
                            {
                                body_string.replace(body_string.find("INTEGRATION_LOGIC"), strlen("INTEGRATION_LOGIC"), diffuse_shadowed_interreflected_transfer_per_vertex_albedo_rgb_integration_logic_body);

                                break;
                            }

                            default:
                            {
                                ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");

                                break;
                            }                            
                        }

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false, "Unrecognized SH integration logic");

                        break;
                    }
                }
            } /* while (body_string.find("INTEGRATION_LOGIC") != std::string::npos) */

            while (body_string.find("STORE_RESULT") != std::string::npos)
            {
                switch (n_result_sh_components)
                {
                    case SH_COMPONENTS_RED:
                    {
                        body_string.replace(body_string.find("STORE_RESULT"), strlen("STORE_RESULT"), store_result_red_body);

                        break;
                    }

                    case SH_COMPONENTS_RG:
                    {
                        body_string.replace(body_string.find("STORE_RESULT"), strlen("STORE_RESULT"), store_result_rg_body);

                        break;
                    }

                    case SH_COMPONENTS_RGB:
                    {
                        body_string.replace(body_string.find("STORE_RESULT"), strlen("STORE_RESULT"), store_result_rgb_body);

                        break;
                    }

                    default:
                    {
                        ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");

                        break;
                    }
                }
            }

            while (body_string.find("IN_DATATYPE") != std::string::npos)
            {
                switch (result_integration_logic)
                {
                    case SH_INTEGRATION_LOGIC_VISIBILITY_FROM_UCHAR8:
                    case SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_CONSTANT_ALBEDO:
                    case SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_INTERREFLECTED_PER_VERTEX_ALBEDO:
                    case SH_INTEGRATION_LOGIC_DIFFUSE_SHADOWED_TRANSFER_PER_VERTEX_ALBEDO:
                    {
                        body_string.replace(body_string.find("IN_DATATYPE"), strlen("IN_DATATYPE"), "unsigned char"); 
                        
                        break;
                    }

                    case SH_INTEGRATION_LOGIC_DIFFUSE_UNSHADOWED_TRANSFER_CONSTANT_ALBEDO:
                    case SH_INTEGRATION_LOGIC_DIFFUSE_UNSHADOWED_TRANSFER_PER_VERTEX_ALBEDO:
                    {
                        body_string.replace(body_string.find("IN_DATATYPE"), strlen("IN_DATATYPE"), "float");

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false, "Unrecognized SH integration logic");

                        break;
                    }
                }
            } /* while (body_string.find("IN_DATATYPE") != std::string::npos) */

            while (body_string.find("OUT_DATATYPE") != std::string::npos)
            {
                switch (n_result_sh_components)
                {
                    case SH_COMPONENTS_RED: body_string.replace(body_string.find("OUT_DATATYPE"), strlen("OUT_DATATYPE"), "float"); break;
                    case SH_COMPONENTS_RG:  body_string.replace(body_string.find("OUT_DATATYPE"), strlen("OUT_DATATYPE"), "float2");break;
                    case SH_COMPONENTS_RGB: body_string.replace(body_string.find("OUT_DATATYPE"), strlen("OUT_DATATYPE"), "float3");break;

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false, "Unrecognized SH components enum");

                        break;
                    }
                }
            } /* while (body_string.find("OUT_DATATYPE") != std::string::npos) */

            /* Prepare kernel name */
            std::stringstream kernel_name_stringstream;
            
            kernel_name_stringstream << "SH generation program" << result_integration_logic << " " << n_result_sh_components;

            /* Kernel body is ready, try to create it now */
            const char* body_string_raw = body_string.c_str();

            _generation_descriptors[descriptor_index].program = ocl_program_create_from_source(context, 1, &body_string_raw, NULL, system_hashed_ansi_string_create(kernel_name_stringstream.str().c_str() ));
            ASSERT_ALWAYS_SYNC(_generation_descriptors[descriptor_index].program != NULL, "Could not create sh generation program");

            _generation_descriptors[descriptor_index].program_kernel = ocl_program_get_kernel_by_name(_generation_descriptors[descriptor_index].program , system_hashed_ansi_string_create("generate_sh") );
            ASSERT_ALWAYS_SYNC(_generation_descriptors[descriptor_index].program_kernel != NULL, "Could not retrieve sh generation program kernel");

            _generation_descriptors[descriptor_index].program_kernel_work_group_size = ocl_kernel_get_work_group_size(_generation_descriptors[descriptor_index].program_kernel);
            _generation_descriptors[descriptor_index].initialized                    = true;
        }

        /* Set arguments */
        cl_int                    cl_result       = CL_SUCCESS;
        cl_kernel                 kernel          = ocl_kernel_get_kernel(_generation_descriptors[descriptor_index].program_kernel);
        uint32_t                  n_coeffs        = n_bands * n_bands;
        const ocl_11_entrypoints* ocl_entrypoints = ocl_get_entrypoints();

        cl_result  = ocl_entrypoints->pCLSetKernelArg(kernel, 0, sizeof(cl_mem),    &input_data);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 1, sizeof(cl_mem),    &input_sh_coeffs_data);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 2, sizeof(cl_mem),    &input_sh_coeffs_unit_vecs);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 3, sizeof(cl_mem),    &output_sh_coeffs);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 4, sizeof(uint32_t),  &n_coeffs);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 5, sizeof(uint32_t),  &n_samples);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 6, sizeof(uint32_t),  &n_vertices);
        cl_result += ocl_entrypoints->pCLSetKernelArg(kernel, 7, sizeof(cl_mem),    &input_prev_pass_data);

        ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not set kernel arguments");
        if (cl_result != CL_SUCCESS)
        {
            result = false;

            goto end;
        }

        /* Let's process */
        cl_platform_id         platform_id            = ocl_context_get_platform_id(context);
        cl_device_id           device_id              = NULL;
        const ocl_device_info* device_info_ptr        = NULL;
        uint32_t               kernel_work_group_size = 0;
        uint32_t               max_compute_units      = 0;
        size_t                 max_work_group_size    = 0;
        size_t*                max_work_item_sizes    = NULL;

        ASSERT_ALWAYS_SYNC(ocl_get_device_id(platform_id, 0, &device_id),                            "Could not retrieve device id");
        ASSERT_ALWAYS_SYNC( (device_info_ptr = ocl_get_device_info(platform_id, device_id)) != NULL, "Could not retrieve device properties");

        max_compute_units    = device_info_ptr->device_max_compute_units;
        max_work_group_size  = device_info_ptr->device_max_work_group_size;
        max_work_item_sizes  = device_info_ptr->device_max_work_item_sizes;

        /* Calculate work size */
        uint32_t global_work_size                 = 0;
        uint32_t local_work_size                  = 0;
        uint32_t n_vertices_per_request           = 0;
        cl_event vertex_data_buffer_release_event = NULL;

        local_work_size        = (max_work_group_size > (size_t) _generation_descriptors[descriptor_index].program_kernel_work_group_size ? _generation_descriptors[descriptor_index].program_kernel_work_group_size : max_work_group_size);
        n_vertices_per_request = local_work_size / n_coeffs;
        global_work_size       = n_vertices_per_request * n_coeffs;

        ASSERT_ALWAYS_SYNC(global_work_size <= local_work_size, "Work size computation failed.");
        if (global_work_size > local_work_size)
        {
            result = false;

            goto end;
        }

        /* As global work size is always smaller than allowed local work size, let's make the GPU use only as many computing units for one run
         * as we can. 
         */
        local_work_size = global_work_size;

        /* Iterate through all normals and calculate. The reason we're not doing a single fat run here is that we don't want NVIDIA driver
         * to reset due to running out of allowed execution time.
         */
        for (uint32_t n_vertex = 0; n_vertex < n_vertices; n_vertex += n_vertices_per_request)
        {
            /* Call back before we continue */
            if (on_processed_vertex_index_update_callback_proc != NULL)
            {
                on_processed_vertex_index_update_callback_proc(n_vertex, on_processed_vertex_index_update_callback_user_arg);
            }

            /* Hit it */
            cl_event wait_event = NULL;

            if (cl_result == CL_SUCCESS)
            {
                uint32_t offset = n_vertex * n_bands * n_bands;

                cl_result = ocl_entrypoints->pCLEnqueueNDRangeKernel(ocl_context_get_command_queue(context, 0),
                                                                     ocl_kernel_get_kernel        (_generation_descriptors[descriptor_index].program_kernel),
                                                                     1,
                                                                     &offset,
                                                                     &global_work_size,
                                                                     &local_work_size,
                                                                     0,
                                                                     NULL,
                                                                     &wait_event);

                ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not enqueue kernel.");
                if (cl_result != CL_SUCCESS)
                {
                    result = false;

                    goto end;
                }

                ocl_entrypoints->pCLWaitForEvents(1, &wait_event);
            }
        } /* for (uint32_t n_normal = 0; n_normal < n_normals; ++n_normal) */

        result = true;

end:
        return result;
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_unshadowed_transfer_constant_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                         __in uint32_t      n_normals)
    {
        return (n_result_sh_components + n_normals * 3) * sizeof(float);
    }
    
    /* Please see header for specification */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_unshadowed_transfer_per_vertex_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                           __in uint32_t      n_vertices)
    {
        return (n_vertices * n_result_sh_components + 3 * n_vertices) * sizeof(float);
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_shadowed_transfer_constant_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                       __in uint32_t      n_normals,
                                                                                                       __in uint32_t      n_samples,
                                                                                                       __in uint32_t      n_sh_bands)
    {
        return (n_result_sh_components + n_normals * 3) * sizeof(float) + n_samples * n_normals;
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_shadowed_transfer_per_vertex_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                         __in uint32_t      n_vertices,
                                                                                                         __in uint32_t      n_samples)
    {
        return (n_vertices * (n_result_sh_components + 3) ) * sizeof(float) + n_samples * n_vertices;
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_shadowed_interreflected_transfer_per_vertex_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                                        __in uint32_t      n_vertices,
                                                                                                                        __in uint32_t      n_samples)
    {
        return    (n_vertices * (n_result_sh_components + 3) ) * sizeof(float) +
               2 * n_vertices *  n_samples                     * sizeof(float) +
               3 * n_vertices *  n_samples                     * sizeof(uint32_t);
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API sh_projector_property_datatype sh_tools_get_sh_projector_property_datatype(__in sh_projector_property projection_property)
    {
        switch (projection_property)
        {
            case SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE:
            case SH_PROJECTOR_PROPERTY_COLOR:
            case SH_PROJECTOR_PROPERTY_CUTOFF:
            case SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE:
            case SH_PROJECTOR_PROPERTY_ROTATION:
            case SH_PROJECTOR_PROPERTY_TEXTURE_ID:
            {
                return SH_PROJECTOR_PROPERTY_DATATYPE_CURVE_CONTAINER;
            }

            case SH_PROJECTOR_PROPERTY_TYPE:
            {
                return SH_PROJECTOR_PROPERTY_DATATYPE_SH_PROJECTION_TYPE;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unrecognized projector property");

                return SH_PROJECTOR_PROPERTY_DATATYPE_UNKNOWN;
            }
        } /* switch (projection_property) */
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API system_hashed_ansi_string sh_tools_get_sh_projector_property_string(__in sh_projector_property projection_property)
    {
        switch (projection_property)
        {
            case SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE: return system_hashed_ansi_string_create("Zenith luminance");
            case SH_PROJECTOR_PROPERTY_COLOR:                return system_hashed_ansi_string_create("Color");
            case SH_PROJECTOR_PROPERTY_CUTOFF:               return system_hashed_ansi_string_create("Cut-off");
            case SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE:      return system_hashed_ansi_string_create("Linear exposure");
            case SH_PROJECTOR_PROPERTY_ROTATION:             return system_hashed_ansi_string_create("Rotation");
            case SH_PROJECTOR_PROPERTY_TEXTURE_ID:           return system_hashed_ansi_string_create("Texture ID");
            case SH_PROJECTOR_PROPERTY_TYPE:                 return system_hashed_ansi_string_create("Type");
            default:                                         return system_hashed_ansi_string_create("??");
        } /* switch (projection_property) */
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API system_hashed_ansi_string sh_tools_get_sh_projection_type_string(__in sh_projection_type projection_type)
    {
        switch (projection_type)
        {
            case SH_PROJECTION_TYPE_CIE_OVERCAST:
            {
                return system_hashed_ansi_string_create("CIE overcast");
            }

            case SH_PROJECTION_TYPE_SPHERICAL_TEXTURE:
            {
                return system_hashed_ansi_string_create("Spherical environment map");
            }

            case SH_PROJECTION_TYPE_STATIC_COLOR:
            {
                return system_hashed_ansi_string_create("Static color");
            }

            case SH_PROJECTION_TYPE_SYNTHETIC_LIGHT:
            {
                return system_hashed_ansi_string_create("Synthetic light");
            }

            default:
            {
                return system_hashed_ansi_string_create("??");
            }
        } /* switch (projection_type) */
    }

    /* Please see header for specification */
    PUBLIC EMERALD_API bool sh_tools_sum_sh(__in __notnull        ocl_context   context, 
                                            __in                  uint32_t      n_passes,
                                            __in_ecount(n_passes) cl_mem*       pass_buffers,
                                            __in                  uint32_t      n_sh_bands,
                                            __in                  sh_components n_sh_components,
                                            __in                  uint32_t      n_sh_samples)
    {
        bool        result                = false;
        const char* sh_coeffs_sum_program = "__kernel void sum_sh(__global float*             data,\n"
                                            "                     __global float*             data2,\n"
                                            "                              const unsigned int n_coeffs,\n"
                                            "                              const unsigned int n_max_coeff)\n"
                                            "{\n"
                                            "    const unsigned int start_index = (get_local_size(0) * get_group_id(0) + get_local_id(0) ) * n_coeffs;\n"
                                            "    const unsigned int end_index   = start_index + n_coeffs;\n"
                                            "\n"
                                            "    if (start_index >= n_max_coeff)\n"
                                            "    {\n"
                                            "        return;\n"
                                            "    }\n"
                                            "\n"
                                            "    for (unsigned int index = start_index; index < end_index; ++index)\n"
                                            "    {\n"
                                            "        data[index] += data2[index];\n"
                                            "    }\n"
                                            "}\n";

        /* Initialize the program if necessary */
        if (_sum_descriptor == NULL)
        {
            _sum_descriptor = new (std::nothrow) _sh_sum_descriptor;

            ASSERT_ALWAYS_SYNC(_sum_descriptor != NULL, "Out of memory");
            if (_sum_descriptor != NULL)
            {
                _sum_descriptor->program = ocl_program_create_from_source(context, 1, &sh_coeffs_sum_program, NULL, system_hashed_ansi_string_create("SH coeffs sum program"));
                ASSERT_ALWAYS_SYNC(_sum_descriptor->program != NULL, "Could not create sh sum program");

                _sum_descriptor->program_kernel = ocl_program_get_kernel_by_name(_sum_descriptor->program, system_hashed_ansi_string_create("sum_sh") );
                ASSERT_ALWAYS_SYNC(_sum_descriptor->program_kernel != NULL, "Could not retrieve sh sum kernel");

                _sum_descriptor->initialized = true;
            } /* if (_sum_descriptor != NULL) */
        } /* if (_sum_descriptor == NULL) */

        ASSERT_ALWAYS_SYNC(n_passes >= 2, "At least two pass buffers are required for summing");
        for (uint32_t n = 1; n < n_passes; ++n)
        {
            /* Set arguments */
            cl_kernel                 cl_binary       = ocl_kernel_get_kernel(_sum_descriptor->program_kernel);
            cl_int                    cl_result       = 0;
            cl_uint                   n_coeffs        = n_sh_components * n_sh_bands * n_sh_bands;
            cl_uint                   n_max_coeff     = n_coeffs * n_sh_samples;
            const ocl_11_entrypoints* ocl_entrypoints = ocl_get_entrypoints();

            cl_result  = ocl_entrypoints->pCLSetKernelArg(cl_binary, 0, sizeof(cl_mem),   pass_buffers + 0);
            cl_result += ocl_entrypoints->pCLSetKernelArg(cl_binary, 1, sizeof(cl_mem),   pass_buffers + n);
            cl_result += ocl_entrypoints->pCLSetKernelArg(cl_binary, 2, sizeof(cl_uint), &n_coeffs);
            cl_result += ocl_entrypoints->pCLSetKernelArg(cl_binary, 3, sizeof(cl_uint), &n_max_coeff);

            ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not set kernel arguments");
            if (cl_result != CL_SUCCESS)
            {
                goto end;
            }

            /* Let's process */
            cl_platform_id         platform_id            = ocl_context_get_platform_id(context);
            cl_device_id           device_id              = NULL;
            const ocl_device_info* device_info_ptr        = NULL;
            uint32_t               kernel_work_group_size = 0;
            size_t                 max_work_group_size    = 0;

            ASSERT_ALWAYS_SYNC(ocl_get_device_id(platform_id, 0, &device_id),                            "Could not retrieve device id");
            ASSERT_ALWAYS_SYNC( (device_info_ptr = ocl_get_device_info(platform_id, device_id)) != NULL, "Could not retrieve device properties");

            max_work_group_size  = device_info_ptr->device_max_work_group_size;

            /* Calculate work size */
            uint32_t global_work_size = 0;
            uint32_t local_work_size  = 0;
            uint32_t n_runs_needed    = 0;

            local_work_size  = (max_work_group_size > n_sh_samples ? n_sh_samples : max_work_group_size);
            n_runs_needed    = n_sh_samples / local_work_size + 1;
            global_work_size = n_runs_needed * local_work_size;

            /* Calculate.*/
            cl_event wait_event = NULL;

            cl_result = ocl_entrypoints->pCLEnqueueNDRangeKernel(ocl_context_get_command_queue(context, 0),
                                                                 ocl_kernel_get_kernel        (_sum_descriptor->program_kernel),
                                                                 1,
                                                                 NULL,
                                                                 &global_work_size,
                                                                 &local_work_size,
                                                                 0,
                                                                 NULL,
                                                                 &wait_event);

            ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not enqueue kernel.");
            if (cl_result != CL_SUCCESS)
            {
                result = false;

                goto end;
            }

            ocl_entrypoints->pCLWaitForEvents(1, &wait_event);
        }

        result = true;

end:
        return result;
    }
#endif /* INCLUDE_OPENCL */


/* Please see header for specification */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_tools_generate_preview(__in  __notnull ogl_context            context,
                                                                         __in  __notnull ogl_texture            input_light_sh_data_tbo,
                                                                         __in            uint32_t               n_bands,
                                                                         __in  __notnull procedural_mesh_sphere reference_sphere,
                                                                         __out __notnull GLuint*                out_vertex_color_bo_id)
{
#if 1
    ASSERT_DEBUG_SYNC(false,
                      "The former implementation seems to be.. broken?");
#else
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    unsigned int                                              n_points         = 0;
    procedural_mesh_sphere_get_property(reference_sphere,
                                        PROCEDURAL_MESH_SPHERE_PROPERTY_N_POINTS,
                                       &n_points);

    _sh_tools_verify_context_type(context);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (_preview_generation_program == NULL)
    {
        /* Generate body */
        uint32_t preview_generation_body_length = strlen(preview_generation_template_body) + strlen(glsl_embeddable_sh) + 1;
        char*    preview_generation_body        = new (std::nothrow) char[preview_generation_body_length];

        ASSERT_DEBUG_SYNC(preview_generation_body != NULL, "Out of memory");
        if (preview_generation_body != NULL)
        {
            sprintf_s(preview_generation_body, preview_generation_body_length, preview_generation_template_body, n_bands, glsl_embeddable_sh);
        }

        /* Prepare the program before usage */
        static const char* tf_output_data[] = {"result.result_nonsh"};

        _preview_generation_program    = ogl_program_create(context,                     system_hashed_ansi_string_create("SH tools preview generator program") );
        _preview_generation_vp_shader  = ogl_shader_create (context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create("SH tools preview generator vertex shader") );
        _preview_generation_program_id = ogl_program_get_id(_preview_generation_program);

        ogl_shader_set_body      (_preview_generation_vp_shader, system_hashed_ansi_string_create(preview_generation_body) );
        ogl_program_attach_shader(_preview_generation_program,   _preview_generation_vp_shader);
        
        entry_points->pGLTransformFeedbackVaryings(_preview_generation_program_id,
                                                   sizeof(tf_output_data) / sizeof(tf_output_data[0]),
                                                   tf_output_data,
                                                   GL_INTERLEAVED_ATTRIBS);

        ogl_program_link(_preview_generation_program);


        const ogl_program_variable* input_light_samples_data_ptr = NULL;
        const ogl_program_variable* input_vertex_data_ptr        = NULL;

        ogl_program_get_uniform_by_name(_preview_generation_program, system_hashed_ansi_string_create("input_light_samples_data"), &input_light_samples_data_ptr);
        ogl_program_get_uniform_by_name(_preview_generation_program, system_hashed_ansi_string_create("input_vertex_data"),        &input_vertex_data_ptr);

        _preview_generation_input_light_samples_location     = input_light_samples_data_ptr->location;
        _preview_generation_input_samples_theta_phi_location = input_vertex_data_ptr->location;

        entry_points->pGLProgramUniform1i(_preview_generation_program_id, _preview_generation_input_light_samples_location,     0);
        entry_points->pGLProgramUniform1i(_preview_generation_program_id, _preview_generation_input_samples_theta_phi_location, 1);

        entry_points->pGLGenVertexArrays(1, &_vaa_id);

        /* Release body */
        if (preview_generation_body != NULL)
        {
            delete [] preview_generation_body;

            preview_generation_body = NULL;
        }
    }

    if (*out_vertex_color_bo_id == 0)
    {
        entry_points->pGLGenBuffers            (1,                       out_vertex_color_bo_id);
        dsa_entry_points->pGLNamedBufferDataEXT(*out_vertex_color_bo_id, n_reference_sphere_points * sizeof(GLfloat) * 3, NULL, GL_STATIC_COPY);    <- NULL?!
    }

    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, input_light_sh_data_tbo);
    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE1, GL_TEXTURE_BUFFER, procedural_mesh_sphere_get_arrays_tbo(reference_sphere) );

    entry_points->pGLBindVertexArray       (_vaa_id);
    entry_points->pGLBindBufferBase        (GL_TRANSFORM_FEEDBACK_BUFFER, 0, *out_vertex_color_bo_id);
    entry_points->pGLEnable                (GL_RASTERIZER_DISCARD);
    entry_points->pGLUseProgram            (_preview_generation_program_id);
    entry_points->pGLBeginTransformFeedback(GL_POINTS);
    {
        entry_points->pGLDrawArrays(GL_POINTS, 0, n_reference_sphere_points);
    }
    entry_points->pGLEndTransformFeedback();
    entry_points->pGLDisable             (GL_RASTERIZER_DISCARD);
#endif
}
