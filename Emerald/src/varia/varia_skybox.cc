/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "object_manager/object_manager_general.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
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
const char* fragment_shader_sh_preview = "#version 430 core\n"
                                         "\n"
                                         "#define N_BANDS (N_BANDS_VALUE)\n"
                                         "\n"
                                         "SH_CODE_GOES_HERE"
                                         "\n"
                                         "uniform samplerBuffer input_light_sh_data;\n"
                                         "\n"
                                         "in  vec3 view_vector;\n"
                                         "out vec4 result_nonsh;\n"
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

const char* fragment_shader_spherical_texture_preview = "#version 430 core\n"
                                                        "\n"
                                                        "in      vec3      view_vector;\n"
                                                        "uniform sampler2D skybox;\n"
                                                        "out     vec3      result;\n"
                                                        "\n"
                                                        "void main()\n"
                                                        "{\n"
                                                        "    vec3  view_vector_n = normalize(view_vector);\n"
                                                        "    float radius        = 2 * sqrt(view_vector_n.x*view_vector_n.x + view_vector_n.y*view_vector_n.y + (view_vector_n.z+1)*(view_vector_n.z+1) );\r\n"
                                                        "\r\n"
                                                        "    vec2  texture_uv = asin(view_vector_n.xy) / 3.1415265 + 0.5;\n"
                                                        "\n"
                                                        "    texture_uv.y = 1.0 - texture_uv.y;\n"
                                                        "    result       = texture(skybox, texture_uv).xyz;\r\n"
                                                        "}\n";

const char* vertex_shader_preview = "#version 430 core\n"
                                    "\n"
                                    "uniform dataVS\n"
                                    "{\n"
                                    "    mat4 mv;\n"
                                    "    mat4 inv_projection;\n"
                                    "};\n"
                                    "out vec3 view_vector;\n"
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
    _varia_skybox_type        type;

    GLuint                   input_sh_light_data_uniform_location;
    GLuint                   inverse_projection_ub_offset;
    GLuint                   mv_ub_offset;
    ral_program              program;
    ral_program_block_buffer program_ub;
    unsigned int             program_ub_bo_size;
    GLuint                   skybox_uniform_location;

    REFCOUNT_INSERT_VARIABLES
} _varia_skybox;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _varia_skybox_verify_context_type(ogl_context);
#else
    #define _varia_skybox_verify_context_type(x)
#endif


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(varia_skybox,
                               varia_skybox,
                              _varia_skybox);

/* Forward declarations */
PRIVATE std::string _varia_skybox_get_fragment_shader_body(sh_samples                samples);
PRIVATE void        _varia_skybox_init_varia_skybox       (_varia_skybox*            skybox_ptr,
                                                           system_hashed_ansi_string name,
                                                           _varia_skybox_type        type,
                                                           sh_samples                samples,
                                                           ral_context               context,
                                                           ral_texture               texture);
PRIVATE void        _varia_skybox_init_varia_skybox_sh    (_varia_skybox*            skybox_ptr);
PRIVATE void        _varia_skybox_init_ub                 (_varia_skybox*            skybox_ptr);

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
        ASSERT_DEBUG_SYNC(vertex_shader != NULL,
                          "Could not create skybox vertex shader");

        ogl_shader_set_body(vertex_shader,
                            system_hashed_ansi_string_create(vertex_shader_preview) );

        /* Initialize the fragment shader */
        ogl_shader fragment_shader = ogl_shader_create(skybox_ptr->context,
                                                       RAL_SHADER_TYPE_FRAGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                               " fragment shader") );
        ASSERT_DEBUG_SYNC(fragment_shader != NULL,
                          "Could not create skybox fragment shader");

        ogl_shader_set_body(fragment_shader,
                            system_hashed_ansi_string_create(_varia_skybox_get_fragment_shader_body(skybox_ptr->samples).c_str()) );

        /* Create a program */
        skybox_ptr->program = ogl_program_create(skybox_ptr->context,
                                                 system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                         " program"),
                                                 OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

        ASSERT_DEBUG_SYNC(skybox_ptr->program != NULL,
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
        const ogl_program_variable* input_light_sh_data_uniform_descriptor = NULL;
        const ogl_program_variable* inverse_projection_uniform_descriptor  = NULL;
        const ogl_program_variable* mv_data_uniform_descriptor             = NULL;

        ogl_program_get_uniform_by_name(skybox_ptr->program,
                                        system_hashed_ansi_string_create("input_light_sh_data"),
                                       &input_light_sh_data_uniform_descriptor);
        ogl_program_get_uniform_by_name(skybox_ptr->program,
                                        system_hashed_ansi_string_create("inv_projection"),
                                       &inverse_projection_uniform_descriptor);
        ogl_program_get_uniform_by_name(skybox_ptr->program,
                                        system_hashed_ansi_string_create("mv"),
                                       &mv_data_uniform_descriptor);

        skybox_ptr->input_sh_light_data_uniform_location = (input_light_sh_data_uniform_descriptor != NULL) ? input_light_sh_data_uniform_descriptor->location    : -1;
        skybox_ptr->inverse_projection_ub_offset         = (inverse_projection_uniform_descriptor  != NULL) ? inverse_projection_uniform_descriptor->block_offset : -1;
        skybox_ptr->mv_ub_offset                         = (mv_data_uniform_descriptor             != NULL) ? mv_data_uniform_descriptor->block_offset            : -1;

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
                                   result_shaders[0]) ||
        !ral_program_attach_shader(skybox_ptr->program,
                                   result_shaders[1]))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not attach shaders to the skybox program");
    }

    /* Retrieve uniform locations */
    const ral_program_variable*   inverse_projection_uniform_ral_ptr = NULL;
    const ral_program_variable*   mv_data_uniform_ral_ptr            = NULL;
    raGL_program                  program_raGL                       = ral_context_get_program_gl(skybox_ptr->context,
                                                                                                  skybox_ptr->program);
    const _raGL_program_variable* skybox_uniform_raGL_ptr            = NULL;

    ral_program_get_block_variable_by_name(skybox_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("inv_projection"),
                                          &inverse_projection_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(skybox_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("mv"),
                                          &mv_data_uniform_ral_ptr);
    raGL_program_get_uniform_by_name      (program_raGL,
                                           system_hashed_ansi_string_create("skybox"),
                                          &skybox_uniform_raGL_ptr);

    skybox_ptr->inverse_projection_ub_offset = (inverse_projection_uniform_ral_ptr != NULL) ? inverse_projection_uniform_ral_ptr->block_offset : -1;
    skybox_ptr->mv_ub_offset                 = (mv_data_uniform_ral_ptr            != NULL) ? mv_data_uniform_ral_ptr->block_offset            : -1;
    skybox_ptr->skybox_uniform_location      = (skybox_uniform_raGL_ptr            != NULL) ? skybox_uniform_raGL_ptr->location                : -1;

    /* Retrieve uniform block info */
    _varia_skybox_init_ub(skybox_ptr);

    /* Done */
    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_create_info_items,
                               (const void**) result_shaders);
}

/** TODO */
PRIVATE void _varia_skybox_init(_varia_skybox*            skybox_ptr,
                                system_hashed_ansi_string name,
                                _varia_skybox_type        type,
                                sh_samples                samples,
                                ral_context               context,
                                ral_texture               texture)
{
    memset(skybox_ptr,
           0,
           sizeof(_varia_skybox) );

    skybox_ptr->context = context;
    skybox_ptr->name    = name;
    skybox_ptr->samples = samples;
    skybox_ptr->texture = texture;
    skybox_ptr->type    = type;

    /* Perform type-specific initialization */
    switch (type)
    {
#ifdef INCLUDE_OPENCL
        case VARIA_SKYBOX_LIGHT_PROJECTION_SH:
        {
            _varia_skybox_init_varia_skybox_sh(skybox_ptr);

            break;
        } /* case VARIA_SKYBOX_LIGHT_PROJECTION_SH: */
#endif

        case VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE:
        {
            _varia_skybox_init_spherical_projection_texture(skybox_ptr);

            break;
        } /* case VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE: */

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized skybox type");
        } /* default:*/
    } /* switch (type) */
}

/** TODO */
PRIVATE void _varia_skybox_init_ub(_varia_skybox* skybox_ptr)
{
    ral_buffer ub_buffer_ral = NULL;

    skybox_ptr->program_ub = ral_program_block_buffer_create(skybox_ptr->context,
                                                             skybox_ptr->program,
                                                             system_hashed_ansi_string_create("dataVS") );

    ASSERT_DEBUG_SYNC(skybox_ptr->program_ub != NULL,
                      "Could not create a ral_program_block_buffer instance for the dataVS uniform block.");

    ral_program_block_buffer_get_property(skybox_ptr->program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_buffer_ral);
    ral_buffer_get_property              (ub_buffer_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &skybox_ptr->program_ub_bo_size);
}

/** TODO */
PRIVATE void _varia_skybox_release(void* skybox)
{
    _varia_skybox* skybox_ptr = (_varia_skybox*) skybox;

    ral_context_delete_objects(skybox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &skybox_ptr->program);

    if (skybox_ptr->program_ub != NULL)
    {
        ral_program_block_buffer_release(skybox_ptr->program_ub);

        skybox_ptr->program_ub = NULL;
    }
}

#ifdef INCLUDE_OPENCL
    /** Please see header for specification */
    PUBLIC EMERALD_API varia_skybox varia_skybox_create_light_projection_sh(ogl_context               context,
                                                                            sh_samples                samples,
                                                                            system_hashed_ansi_string name)
    {
        _varia_skybox_verify_context_type(context);

        _varia_skybox* new_instance = new (std::nothrow) _varia_skybox;

        ASSERT_DEBUG_SYNC(new_instance != NULL,
                          "Out of memory");

        if (new_instance != NULL)
        {
            _varia_skybox_init_varia_skybox(new_instance,
                                            name,
                                            VARIA_SKYBOX_LIGHT_PROJECTION_SH,
                                            samples,
                                            context,
                                            NULL);

            REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                           _varia_skybox_release,
                                                           OBJECT_TYPE_VARIA_SKYBOX,
                                                           system_hashed_ansi_string_create_by_merging_two_strings("\\Varia Skyboxes\\",
                                                                                                                   system_hashed_ansi_string_get_buffer(name)) );
        } /* if (new_instance != NULL) */

        return (varia_skybox) new_instance;
    }
#endif

/** Please see header for specification */
PUBLIC EMERALD_API varia_skybox varia_skybox_create_spherical_projection_texture(ral_context               context,
                                                                                 ral_texture               texture,
                                                                                 system_hashed_ansi_string name)
{
    _varia_skybox* new_skybox_ptr = new (std::nothrow) _varia_skybox;

    ASSERT_DEBUG_SYNC(new_skybox_ptr != NULL,
                      "Out of memory");

    if (new_skybox_ptr != NULL)
    {
        _varia_skybox_init(new_skybox_ptr,
                           name,
                           VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE,
                           NULL,
                           context,
                           texture);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_skybox_ptr,
                                                       _varia_skybox_release,
                                                       OBJECT_TYPE_VARIA_SKYBOX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Varia Skyboxes\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_instance != NULL) */

    return (varia_skybox) new_skybox_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_skybox_draw(varia_skybox     skybox,
                                          system_matrix4x4 modelview,
                                          system_matrix4x4 inverted_projection)
{
    ogl_context                                               context_gl             = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points_ptr   = NULL;
    const ogl_context_gl_entrypoints*                         entry_points_ptr       = NULL;
    _varia_skybox*                                            skybox_ptr             = (_varia_skybox*) skybox;
    raGL_program                                              skybox_program_raGL    = ral_context_get_program_gl(skybox_ptr->context,
                                                                                                                  skybox_ptr->program);
    GLuint                                                    skybox_program_raGL_id = 0;
    GLuint                                                    vao_id                 = 0;

    raGL_program_get_property(skybox_program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &skybox_program_raGL_id);

    context_gl = ral_context_get_gl_context(skybox_ptr->context);

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points_ptr);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points_ptr);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entry_points_ptr->pGLUseProgram     (skybox_program_raGL_id);
    entry_points_ptr->pGLBindVertexArray(vao_id);

    /* Update general uniforms */
    ral_program_block_buffer_set_nonarrayed_variable_value(skybox_ptr->program_ub,
                                                           skybox_ptr->inverse_projection_ub_offset,
                                                           system_matrix4x4_get_column_major_data(inverted_projection),
                                                           sizeof(float) * 16);
    ral_program_block_buffer_set_nonarrayed_variable_value(skybox_ptr->program_ub,
                                                           skybox_ptr->mv_ub_offset,
                                                           system_matrix4x4_get_column_major_data(modelview),
                                                           sizeof(float) * 16);

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
    } /* if (skybox_ptr->type == VARIA_SKYBOX_LIGHT_PROJECTION_SH) */
#endif

    /* Do spherial projection-specific stuff */
    if (skybox_ptr->type == VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE)
    {
        GLuint texture_id = ral_context_get_texture_gl_id(skybox_ptr->context,
                                                          skybox_ptr->texture);

        entry_points_ptr->pGLBindSampler            (0, /* unit */
                                                     0  /* sampler */);
        dsa_entry_points_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                     GL_TEXTURE_2D,
                                                     texture_id);
    } /* if (skybox_ptr->type == varia_skybox_SPHERICAL_PROJECTION_TEXTURE) */

    /* Draw. Do not modify depth information */
    GLuint      program_ub_bo_id                = 0;
    raGL_buffer program_ub_bo_raGL              = NULL;
    uint32_t    program_ub_bo_raGL_start_offset = -1;
    ral_buffer  program_ub_bo_ral               = NULL;
    uint32_t    program_ub_bo_ral_start_offset  = -1;

    ral_program_block_buffer_get_property(skybox_ptr->program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_bo_ral);

    program_ub_bo_raGL = ral_context_get_buffer_gl(skybox_ptr->context,
                                                   program_ub_bo_ral);

    raGL_buffer_get_property(program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_ub_bo_id);
    raGL_buffer_get_property(program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_ub_bo_raGL_start_offset);
    ral_buffer_get_property (program_ub_bo_ral,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &program_ub_bo_ral_start_offset);

    ral_program_block_buffer_sync(skybox_ptr->program_ub);

    entry_points_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         0, /* index */
                                         program_ub_bo_id,
                                         program_ub_bo_raGL_start_offset + program_ub_bo_ral_start_offset,
                                         skybox_ptr->program_ub_bo_size);
    entry_points_ptr->pGLDepthMask      (GL_FALSE);
    {
        entry_points_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                        0,
                                        4);
    }
    entry_points_ptr->pGLDepthMask(GL_TRUE);
}
