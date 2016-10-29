/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "object_manager/object_manager_general.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "shaders/shaders_embeddable_sh.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "varia/varia_skybox.h"
#include <string>
#include <sstream>

#ifdef INCLUDE_OPENCL
    #include "sh/sh_samples.h"
#endif

/** TODO: SH skybox is crippled a bit - view vector is calculated correctly, however it looks like
 *        theta/phi calculations are a bit broken. Weird, it's just a typical rectangular->spherical
 *        coordinate conversion. It does the job in showing how SH light projection looks like so
 *        screw it for now. 
 */
/** Internal shaders */
const char* fragment_shader_sh_preview =
    "#version 430 core\n"
    "\n"
    "#define N_BANDS (N_BANDS_VALUE)\n"
    "\n"
    "SH_CODE_GOES_HERE"
    "\n"
    "layout(binding = 0) uniform samplerBuffer input_light_sh_data;\n"
    "\n"
    "layout(location = 0) in  vec3 view_vector;\n"
    "layout(location = 0) out vec4 result_nonsh;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec3 view_vector_n = normalize(view_vector);\n"
    "\n"
    /* NOTE: When we convert from cartesian coordinate, we use ZXY component ordering
     *       instead of the usual XYZ */
    "    float theta  = atan(length(view_vector_n.zx), view_vector_n.y);\n"
    "    float phi    = atan(view_vector_n.x, view_vector_n.z);\n"
    "    vec3  result = vec3(0.0);\n"
    "\n"
    "    for (int l = 0; l < N_BANDS; ++l)\n"
    "    {\n"
    "        for (int m = -l; m <= l; ++m)\n"
    "        {\n"
    "            vec3 input_sh = vec3(texelFetch(input_light_sh_data, 3*(l * (l+1) + m) + 0).x,\n"
    "                                 texelFetch(input_light_sh_data, 3*(l * (l+1) + m) + 1).x,\n"
    "                                 texelFetch(input_light_sh_data, 3*(l * (l+1) + m) + 2).x);\n"
    "\n"
    "            result += input_sh * vec3(spherical_harmonics(l, m, theta, phi));\n"
    "        }\n"
    "    }\n"
    "\n"
    "   result_nonsh = vec4(result, 1.0);\n"
    "}\n";

const char* fragment_shader_spherical_texture_preview =
    "#version 430 core\n"
    "\n"
    "layout(location = 0) in      vec3      view_vector;\n"
    "uniform sampler2D skybox;\n"
    "layout(location = 0) out     vec3      result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec3  view_vector_n = normalize(view_vector);\n"
    "    float radius        = 2 * sqrt(view_vector_n.x*view_vector_n.x + view_vector_n.y*view_vector_n.y + (view_vector_n.z+1)*(view_vector_n.z+1) );\r\n"
    "\r\n"
    "    vec2  texture_uv = asin(view_vector_n.xy) / 3.1415265 + 0.5;\n"
    "\n"
    "    texture_uv.y = 1.0 - texture_uv.y;\n"
    "    result       = textureLod(skybox, texture_uv, 0.0).xyz;\r\n"
    "}\n";

const char* vertex_shader_preview =
    "#version 430 core\n"
    "\n"
    "layout(std140) uniform dataVS\n"
    "{\n"
    "    mat4 mv;\n"
    "    mat4 inv_projection;\n"
    "};\n"
    "layout(location = 0) out vec3 view_vector;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 vertex_data = vec4(gl_VertexID < 2 ? -1.0 : 1.0, (gl_VertexID == 0 || gl_VertexID == 3) ? -1.0 : 1.0, 0, 1);\n"
    "    mat4 mv2         = mv;\n"
    "\n"
    "    mv2[3].xyz = vec3(0);\n"
    "\n"
    "    vec4 view_vector_temp = inverse(mv2) * inv_projection * vertex_data;\n"
    "\n"
    "    view_vector = (view_vector_temp / view_vector_temp.w).xyz;\n"
    "    gl_Position = vertex_data;\n"
    "}\n";

/** Internal type declarations */
typedef struct
{
    ral_context               context;
    system_hashed_ansi_string name;
    sh_samples                samples;
    ral_texture               texture;
    ral_texture_view          texture_view;
    _varia_skybox_type        type;

    ral_command_buffer       cached_command_buffer;
    ral_gfx_state            cached_gfx_state;
    ral_present_task         cached_present_task;
    system_matrix4x4         cached_present_task_inv_proj;
    system_matrix4x4         cached_present_task_mv;
    ral_texture_view         cached_present_task_texture_view;
    ral_sampler              cached_sampler;
    system_critical_section  matrix_cs;

    uint32_t                 inverse_projection_ub_offset;
    uint32_t                 mv_ub_offset;
    ral_program              program;
    ral_program_block_buffer program_ub;

    REFCOUNT_INSERT_VARIABLES
} _varia_skybox;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(varia_skybox,
                               varia_skybox,
                              _varia_skybox);

/* Forward declarations */
PRIVATE std::string _varia_skybox_get_fragment_shader_body(sh_samples     samples);
PRIVATE void        _varia_skybox_init_varia_skybox_sh    (_varia_skybox* skybox_ptr);
PRIVATE void        _varia_skybox_init_ub                 (_varia_skybox* skybox_ptr);

#ifdef INCLUDE_OPENCL
    /** TODO */
    PRIVATE std::string _varia_skybox_get_fragment_shader_body(sh_samples samples)
    {
        std::string       body;
        std::string       n_bands_value_string;
        std::stringstream n_bands_value_stringstream;

        n_bands_value_stringstream << sh_samples_get_amount_of_bands(samples);
        n_bands_value_string = n_bands_value_stringstream.str();

        /* Set the string stream */
        body = fragment_shader_sh_preview;

        body = body.replace(body.find("N_BANDS_VALUE"),
                            strlen("N_BANDS_VALUE"),
                            n_bands_value_string);
        body = body.replace(body.find("SH_CODE_GOES_HERE"),
                            strlen("SH_CODE_GOES_HERE"),
                            glsl_embeddable_sh);

        /* Good to go! */
        return body;
    }

    /** TODO */
    PRIVATE void _varia_skybox_init_varia_skybox_sh(_varia_skybox* skybox_ptr)
    {
        /* Initialize the vertex shader. */
        ogl_shader vertex_shader = ogl_shader_create(skybox_ptr->context,
                                                     RAL_SHADER_TYPE_VERTEX,
                                                     system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                             " vertex shader") );
        ASSERT_DEBUG_SYNC(vertex_shader != nullptr,
                          "Could not create skybox vertex shader");

        ogl_shader_set_body(vertex_shader,
                            system_hashed_ansi_string_create(vertex_shader_preview) );

        /* Initialize the fragment shader */
        ogl_shader fragment_shader = ogl_shader_create(skybox_ptr->context,
                                                       RAL_SHADER_TYPE_FRAGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                               " fragment shader") );
        ASSERT_DEBUG_SYNC(fragment_shader != nullptr,
                          "Could not create skybox fragment shader");

        ogl_shader_set_body(fragment_shader,
                            system_hashed_ansi_string_create(_varia_skybox_get_fragment_shader_body(skybox_ptr->samples).c_str()) );

        /* Create a program */
        skybox_ptr->program = ogl_program_create(skybox_ptr->context,
                                                 system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                         " program"),
                                                 OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

        ASSERT_DEBUG_SYNC(skybox_ptr->program != nullptr,
                          "Could not create skybox program");

        if (!ogl_program_attach_shader(skybox_ptr->program, fragment_shader) ||
            !ogl_program_attach_shader(skybox_ptr->program, vertex_shader))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not attach shaders to the skybox program");
        }

        if (!ogl_program_link(skybox_ptr->program) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not link the skybox program");
        }

        /* Retrieve uniform locations */
        const ogl_program_variable* input_light_sh_data_uniform_descriptor = nullptr;
        const ogl_program_variable* inverse_projection_uniform_descriptor  = nullptr;
        const ogl_program_variable* mv_data_uniform_descriptor             = nullptr;

        ogl_program_get_uniform_by_name(skybox_ptr->program,
                                        system_hashed_ansi_string_create("input_light_sh_data"),
                                       &input_light_sh_data_uniform_descriptor);
        ogl_program_get_uniform_by_name(skybox_ptr->program,
                                        system_hashed_ansi_string_create("inv_projection"),
                                       &inverse_projection_uniform_descriptor);
        ogl_program_get_uniform_by_name(skybox_ptr->program,
                                        system_hashed_ansi_string_create("mv"),
                                       &mv_data_uniform_descriptor);

        skybox_ptr->input_sh_light_data_uniform_location = (input_light_sh_data_uniform_descriptor != nullptr) ? input_light_sh_data_uniform_descriptor->location    : -1;
        skybox_ptr->inverse_projection_ub_offset         = (inverse_projection_uniform_descriptor  != nullptr) ? inverse_projection_uniform_descriptor->block_offset : -1;
        skybox_ptr->mv_ub_offset                         = (mv_data_uniform_descriptor             != nullptr) ? mv_data_uniform_descriptor->block_offset            : -1;

        /* Retrieve uniform block info */
        _varia_skybox_init_ub(skybox_ptr);

        /* Done */
        ogl_shader_release(fragment_shader);
        ogl_shader_release(vertex_shader);
    }
#endif

/** TODO */
PRIVATE void _varia_skybox_init_spherical_projection_texture(_varia_skybox* skybox_ptr)
{
    /* Initialize the shaders. */
    ral_shader_create_info          fs_create_info;
    ral_shader_create_info          vs_create_info;
    const system_hashed_ansi_string fs_body_has = system_hashed_ansi_string_create(fragment_shader_spherical_texture_preview);
    const system_hashed_ansi_string vs_body_has = system_hashed_ansi_string_create(vertex_shader_preview);


    fs_create_info.name   = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                    " fragment shader");
    fs_create_info.source = RAL_SHADER_SOURCE_GLSL;
    fs_create_info.type   = RAL_SHADER_TYPE_FRAGMENT;

    vs_create_info.name   = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                    " vertex shader");
    vs_create_info.source = RAL_SHADER_SOURCE_GLSL;
    vs_create_info.type   = RAL_SHADER_TYPE_VERTEX;


    const ral_shader_create_info create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_create_info_items = sizeof(create_info_items) / sizeof(create_info_items[0]);

    ral_shader result_shaders[n_create_info_items];


    if (!ral_context_create_shaders(skybox_ptr->context,
                                    n_create_info_items,
                                    create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }


    ral_shader_set_property(result_shaders[0],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body_has);
    ral_shader_set_property(result_shaders[1],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body_has);

    /* Create a program */
    ral_program_create_info program_create_info;

    program_create_info.active_shader_stages = RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX;
    program_create_info.name                 = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                       " program");

    if (!ral_context_create_programs(skybox_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &skybox_ptr->program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    if (!ral_program_attach_shader(skybox_ptr->program,
                                   result_shaders[0],
                                   true /* async */) ||
        !ral_program_attach_shader(skybox_ptr->program,
                                   result_shaders[1],
                                   true /* async */))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not attach shaders to the skybox program");
    }

    /* Retrieve uniform locations */
    const ral_program_variable* inverse_projection_uniform_ral_ptr = nullptr;
    const ral_program_variable* mv_data_uniform_ral_ptr            = nullptr;

    ral_program_get_block_variable_by_name(skybox_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("inv_projection"),
                                          &inverse_projection_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(skybox_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("mv"),
                                          &mv_data_uniform_ral_ptr);

    skybox_ptr->inverse_projection_ub_offset = (inverse_projection_uniform_ral_ptr != nullptr) ? inverse_projection_uniform_ral_ptr->block_offset : -1;
    skybox_ptr->mv_ub_offset                 = (mv_data_uniform_ral_ptr            != nullptr) ? mv_data_uniform_ral_ptr->block_offset            : -1;

    /* Retrieve uniform block info */
    _varia_skybox_init_ub(skybox_ptr);

    /* Cache a gfx_state instance */
    ral_gfx_state_create_info gfx_state_create_info;

    gfx_state_create_info.depth_test                   = false;
    gfx_state_create_info.depth_writes                 = false;
    gfx_state_create_info.primitive_type               = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
    gfx_state_create_info.scissor_test                 = false;
    gfx_state_create_info.static_scissor_boxes_enabled = false;
    gfx_state_create_info.static_viewports_enabled     = false;

    ral_context_create_gfx_states(skybox_ptr->context,
                                  1, /* n_create_info_items */
                                 &gfx_state_create_info,
                                 &skybox_ptr->cached_gfx_state);

    /* Cache a sampler instance */
    ral_sampler_create_info sampler_create_info;

    sampler_create_info.mipmap_mode = RAL_TEXTURE_MIPMAP_MODE_NEAREST;

    ral_context_create_samplers(skybox_ptr->context,
                                1, /* n_create_info_items */
                               &sampler_create_info,
                               &skybox_ptr->cached_sampler);

    /* Done */
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_create_info_items,
                               reinterpret_cast<void* const*>(result_shaders) );
}

/** TODO */
PRIVATE void _varia_skybox_init(_varia_skybox*            skybox_ptr,
                                system_hashed_ansi_string name,
                                _varia_skybox_type        type,
                                sh_samples                samples,
                                ral_context               context,
                                ral_texture               texture)
{
    ral_texture_view_create_info texture_view_create_info(texture);

    memset(skybox_ptr,
           0,
           sizeof(_varia_skybox) );

    skybox_ptr->cached_present_task_inv_proj = system_matrix4x4_create();
    skybox_ptr->cached_present_task_mv       = system_matrix4x4_create();
    skybox_ptr->context                      = context;
    skybox_ptr->matrix_cs                    = system_critical_section_create();
    skybox_ptr->name                         = name;
    skybox_ptr->samples                      = samples;
    skybox_ptr->texture                      = texture;
    skybox_ptr->type                         = type;

    ral_context_retain_object(context,
                              RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                              texture);

    ral_context_create_texture_views(context,
                                     1, /* n_texture_views */
                                    &texture_view_create_info,
                                    &skybox_ptr->texture_view);

    /* Perform type-specific initialization */
    switch (type)
    {
#ifdef INCLUDE_OPENCL
        case VARIA_SKYBOX_LIGHT_PROJECTION_SH:
        {
            _varia_skybox_init_varia_skybox_sh(skybox_ptr);

            break;
        }
#endif

        case VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE:
        {
            _varia_skybox_init_spherical_projection_texture(skybox_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized skybox type");
        }
    }
}

/** TODO */
PRIVATE void _varia_skybox_init_ub(_varia_skybox* skybox_ptr)
{
    ral_buffer ub_buffer_ral = nullptr;

    skybox_ptr->program_ub = ral_program_block_buffer_create(skybox_ptr->context,
                                                             skybox_ptr->program,
                                                             system_hashed_ansi_string_create("dataVS") );

    ASSERT_DEBUG_SYNC(skybox_ptr->program_ub != nullptr,
                      "Could not create a ral_program_block_buffer instance for the dataVS uniform block.");

    ral_program_block_buffer_get_property(skybox_ptr->program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_buffer_ral);
}

/** TODO */
PRIVATE void _varia_skybox_release(void* skybox)
{
    _varia_skybox* skybox_ptr = (_varia_skybox*) skybox;

    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&skybox_ptr->cached_command_buffer) );
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&skybox_ptr->cached_gfx_state) );
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&skybox_ptr->program) );
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&skybox_ptr->cached_sampler) );
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&skybox_ptr->texture) );
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&skybox_ptr->texture_view) );

    if (skybox_ptr->cached_present_task != nullptr)
    {
        ral_present_task_release(skybox_ptr->cached_present_task);

        skybox_ptr->cached_present_task = nullptr;
    }

    if (skybox_ptr->program_ub != nullptr)
    {
        ral_program_block_buffer_release(skybox_ptr->program_ub);

        skybox_ptr->program_ub = nullptr;
    }

    system_matrix4x4_release       (skybox_ptr->cached_present_task_inv_proj);
    system_matrix4x4_release       (skybox_ptr->cached_present_task_mv);
    system_critical_section_release(skybox_ptr->matrix_cs);
}

/** TODO */
PRIVATE void _varia_skybox_update_ub_cpu_callback(void* skybox_raw_ptr)
{
    _varia_skybox* skybox_ptr = reinterpret_cast<_varia_skybox*>(skybox_raw_ptr);

    system_critical_section_enter(skybox_ptr->matrix_cs);
    {
        ral_program_block_buffer_set_nonarrayed_variable_value(skybox_ptr->program_ub,
                                                               skybox_ptr->inverse_projection_ub_offset,
                                                               system_matrix4x4_get_column_major_data(skybox_ptr->cached_present_task_inv_proj),
                                                               sizeof(float) * 16);
        ral_program_block_buffer_set_nonarrayed_variable_value(skybox_ptr->program_ub,
                                                               skybox_ptr->mv_ub_offset,
                                                               system_matrix4x4_get_column_major_data(skybox_ptr->cached_present_task_mv),
                                                               sizeof(float) * 16);
    }
    system_critical_section_leave(skybox_ptr->matrix_cs);

    ral_program_block_buffer_sync_immediately(skybox_ptr->program_ub);
}

#ifdef INCLUDE_OPENCL
    /** Please see header for specification */
    PUBLIC EMERALD_API varia_skybox varia_skybox_create_light_projection_sh(ogl_context               context,
                                                                            sh_samples                samples,
                                                                            system_hashed_ansi_string name)
    {
        _varia_skybox_verify_context_type(context);

        _varia_skybox* new_instance = new (std::nothrow) _varia_skybox;

        ASSERT_DEBUG_SYNC(new_instance != nullptr,
                          "Out of memory");

        if (new_instance != nullptr)
        {
            _varia_skybox_init_varia_skybox(new_instance,
                                            name,
                                            VARIA_SKYBOX_LIGHT_PROJECTION_SH,
                                            samples,
                                            context,
                                            nullptr);

            REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                           _varia_skybox_release,
                                                           OBJECT_TYPE_VARIA_SKYBOX,
                                                           system_hashed_ansi_string_create_by_merging_two_strings("\\Varia Skyboxes\\",
                                                                                                                   system_hashed_ansi_string_get_buffer(name)) );
        }

        return (varia_skybox) new_instance;
    }
#endif

/** Please see header for specification */
PUBLIC EMERALD_API varia_skybox varia_skybox_create_spherical_projection_texture(ral_context               context,
                                                                                 ral_texture               texture,
                                                                                 system_hashed_ansi_string name)
{
    _varia_skybox* new_skybox_ptr = new (std::nothrow) _varia_skybox;

    ASSERT_DEBUG_SYNC(new_skybox_ptr != nullptr,
                      "Out of memory");

    if (new_skybox_ptr != nullptr)
    {
        _varia_skybox_init(new_skybox_ptr,
                           name,
                           VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE,
                           nullptr,
                           context,
                           texture);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_skybox_ptr,
                                                       _varia_skybox_release,
                                                       OBJECT_TYPE_VARIA_SKYBOX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Varia Skyboxes\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (varia_skybox) new_skybox_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task varia_skybox_get_present_task(varia_skybox     skybox,
                                                                  ral_texture_view target_texture_view,
                                                                  system_matrix4x4 modelview,
                                                                  system_matrix4x4 inverted_projection)
{
    ral_buffer       program_ub_bo_ral = nullptr;
    ral_present_task result            = nullptr;
    _varia_skybox*   skybox_ptr        = reinterpret_cast<_varia_skybox*>(skybox);

    ral_program_block_buffer_get_property(skybox_ptr->program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_bo_ral);

    /* Cache the MV & inv_projection matrices */
    system_critical_section_enter(skybox_ptr->matrix_cs);
    {
        system_matrix4x4_set_from_matrix4x4(skybox_ptr->cached_present_task_inv_proj,
                                            inverted_projection);
        system_matrix4x4_set_from_matrix4x4(skybox_ptr->cached_present_task_mv,
                                            modelview);
    }
    system_critical_section_leave(skybox_ptr->matrix_cs);

    /* We only need to re-create the present task if the request is made for the first time, or
     * if the target texture view is different from the one used before. */
    if (skybox_ptr->cached_present_task              != nullptr             &&
        skybox_ptr->cached_present_task_texture_view == target_texture_view)
    {
        result = skybox_ptr->cached_present_task;

        goto end;
    }

    if (skybox_ptr->cached_present_task != nullptr)
    {
        ral_present_task_release(skybox_ptr->cached_present_task);
    }

    /* Set up a command buffer for the GPU task. This only needs to be done once */
    if (skybox_ptr->cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info cmd_buffer_create_info;

        cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        cmd_buffer_create_info.is_executable                           = true;
        cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
        cmd_buffer_create_info.is_resettable                           = false;
        cmd_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(skybox_ptr->context,
                                           1, /* n_command_buffers */
                                          &cmd_buffer_create_info,
                                          &skybox_ptr->cached_command_buffer);

        ral_command_buffer_start_recording(skybox_ptr->cached_command_buffer);
        {
            ral_command_buffer_set_binding_command_info            bindings[2];
            ral_command_buffer_draw_call_regular_command_info      draw_call;
            ral_command_buffer_set_color_rendertarget_command_info rt          = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
            ral_command_buffer_set_scissor_box_command_info        scissor;
            uint32_t                                               target_texture_view_height;
            uint32_t                                               target_texture_view_width;
            ral_command_buffer_set_viewport_command_info           viewport;

            ral_texture_view_get_mipmap_property(target_texture_view,
                                                 0, /* n_layer  */
                                                 0, /* n_mipmap */
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                                &target_texture_view_width);
            ral_texture_view_get_mipmap_property(target_texture_view,
                                                 0, /* n_layer  */
                                                 0, /* n_mipmap */
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                                &target_texture_view_height);

            ral_command_buffer_record_set_program(skybox_ptr->cached_command_buffer,
                                                  skybox_ptr->program);

            #ifdef INCLUDE_OPENCL
                /* Do SH-specific stuff */
                if (skybox_ptr->type == VARIA_SKYBOX_LIGHT_PROJECTION_SH)
                {
                    entry_points->pGLProgramUniform1i(skybox_program_id,
                                                      skybox_ptr->input_sh_light_data_uniform_location,
                                                      0);

                    /* Bind SH data TBO */
                    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                             GL_TEXTURE_BUFFER,
                                                             light_sh_data_tbo);
                }
            #endif

            /* Do spherial projection-specific stuff */
            if (skybox_ptr->type == VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE)
            {
                bindings[0].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
                bindings[0].name                               = system_hashed_ansi_string_create("skybox");
                bindings[0].sampled_image_binding.sampler      = skybox_ptr->cached_sampler;
                bindings[0].sampled_image_binding.texture_view = skybox_ptr->texture_view;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "TODO");
            }

            bindings[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            bindings[1].name                          = system_hashed_ansi_string_create("dataVS");
            bindings[1].uniform_buffer_binding.buffer = program_ub_bo_ral;
            bindings[1].uniform_buffer_binding.offset = 0;
            bindings[1].uniform_buffer_binding.size   = 0;

            draw_call.base_instance = 0;
            draw_call.base_vertex   = 0;
            draw_call.n_instances   = 1;
            draw_call.n_vertices    = 4;

            rt.rendertarget_index = 0;
            rt.texture_view       = target_texture_view;

            scissor.index   = 0;
            scissor.size[0] = target_texture_view_width;
            scissor.size[1] = target_texture_view_height;
            scissor.xy  [0] = 0;
            scissor.xy  [1] = 0;

            viewport.depth_range[0] = 0;
            viewport.depth_range[1] = 1;
            viewport.index          = 0;
            viewport.size[0]        = static_cast<float>(target_texture_view_width);
            viewport.size[1]        = static_cast<float>(target_texture_view_height);
            viewport.xy  [0]        = 0;
            viewport.xy  [1]        = 0;

            ral_command_buffer_record_set_bindings           (skybox_ptr->cached_command_buffer,
                                                              sizeof(bindings) / sizeof(bindings[0]),
                                                              bindings);
            ral_command_buffer_record_set_color_rendertargets(skybox_ptr->cached_command_buffer,
                                                              1, /* n_rendertargets */
                                                             &rt);
            ral_command_buffer_record_set_gfx_state          (skybox_ptr->cached_command_buffer,
                                                              skybox_ptr->cached_gfx_state);
            ral_command_buffer_record_set_scissor_boxes      (skybox_ptr->cached_command_buffer,
                                                              1, /* n_scissor_boxes */
                                                             &scissor);
            ral_command_buffer_record_set_viewports         (skybox_ptr->cached_command_buffer,
                                                              1, /* n_viewports */
                                                             &viewport);

            ral_command_buffer_record_draw_call_regular(skybox_ptr->cached_command_buffer,
                                                        1, /* n_draw_calls */
                                                       &draw_call);
        }
        ral_command_buffer_stop_recording(skybox_ptr->cached_command_buffer);
    }

    /* Set up the present task */
    ral_present_task                    cpu_task;
    ral_present_task_cpu_create_info    cpu_task_info;
    ral_present_task_io                 cpu_task_unique_output;
    ral_present_task                    gpu_task;
    ral_present_task_gpu_create_info    gpu_task_info;
    ral_present_task_io                 gpu_task_unique_output;
    ral_present_task_group_create_info  group_task_info;
    ral_present_task_ingroup_connection group_task_ingroup_connection;
    ral_present_task_group_mapping      group_task_output_mapping;
    ral_present_task                    group_task_subtasks[2];

    cpu_task_unique_output.buffer      = program_ub_bo_ral;
    cpu_task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    gpu_task_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    gpu_task_unique_output.texture_view = target_texture_view;

    cpu_task_info.cpu_task_callback_user_arg = skybox_ptr;
    cpu_task_info.n_unique_inputs            = 0;
    cpu_task_info.n_unique_outputs           = 1;
    cpu_task_info.pfn_cpu_task_callback_proc = _varia_skybox_update_ub_cpu_callback;
    cpu_task_info.unique_inputs              = nullptr;
    cpu_task_info.unique_outputs             = &cpu_task_unique_output;

    gpu_task_info.command_buffer   = skybox_ptr->cached_command_buffer;
    gpu_task_info.n_unique_inputs  = 1;
    gpu_task_info.n_unique_outputs = 1;
    gpu_task_info.unique_inputs    = &cpu_task_unique_output;
    gpu_task_info.unique_outputs   = &gpu_task_unique_output;

    cpu_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Varia skybox: CPU task"),
                                          &cpu_task_info);
    gpu_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Varia skybox: GPU task"),
                                          &gpu_task_info);

    group_task_subtasks[0] = cpu_task;
    group_task_subtasks[1] = gpu_task;

    group_task_ingroup_connection.input_present_task_index     = 1;
    group_task_ingroup_connection.input_present_task_io_index  = 0;
    group_task_ingroup_connection.output_present_task_index    = 0;
    group_task_ingroup_connection.output_present_task_io_index = 0;

    group_task_output_mapping.group_task_io_index   = 0;
    group_task_output_mapping.n_present_task        = 1;
    group_task_output_mapping.present_task_io_index = 0;

    group_task_info.ingroup_connections                      = &group_task_ingroup_connection;
    group_task_info.n_ingroup_connections                    = 1;
    group_task_info.n_present_tasks                          = sizeof(group_task_subtasks) / sizeof(group_task_subtasks[0]);
    group_task_info.n_total_unique_inputs                    = 0;
    group_task_info.n_total_unique_outputs                   = 1;
    group_task_info.n_unique_input_to_ingroup_task_mappings  = 0;
    group_task_info.n_unique_output_to_ingroup_task_mappings = 1;
    group_task_info.present_tasks                            = group_task_subtasks;
    group_task_info.unique_input_to_ingroup_task_mapping     = nullptr;
    group_task_info.unique_output_to_ingroup_task_mapping    = &group_task_output_mapping;

    result = ral_present_task_create_group(system_hashed_ansi_string_create("Varia skybox: group task"),
                                                                           &group_task_info);

    skybox_ptr->cached_present_task              = result;
    skybox_ptr->cached_present_task_texture_view = target_texture_view;

    ral_present_task_release(cpu_task);
    ral_present_task_release(gpu_task);
end:

    return result;
}
