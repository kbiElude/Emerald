/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_skybox.h"
#include "ogl/ogl_texture.h"
#include "object_manager/object_manager_general.h"
#include "raGL/raGL_buffer.h"
#include "shaders/shaders_embeddable_sh.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
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
                                                        "    result = texture(skybox, asin(view_vector_n.xy) / 3.1415265 + 0.5).xyz;\r\n"
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
    ogl_context               context;
    system_hashed_ansi_string name;
    sh_samples                samples;
    ogl_texture               texture;
    _ogl_skybox_type          type;

    GLuint         input_sh_light_data_uniform_location;
    GLuint         inverse_projection_ub_offset;
    GLuint         mv_ub_offset;
    ogl_program    program;
    ogl_program_ub program_ub;
    raGL_buffer    program_ub_bo;
    unsigned int   program_ub_bo_size;
    GLuint         skybox_uniform_location;

    REFCOUNT_INSERT_VARIABLES
} _ogl_skybox;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_skybox_verify_context_type(ogl_context);
#else
    #define _ogl_skybox_verify_context_type(x)
#endif


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_skybox,
                               ogl_skybox,
                              _ogl_skybox);

/* Forward declarations */
PRIVATE std::string _ogl_skybox_get_fragment_shader_body(sh_samples                samples);
PRIVATE void        _ogl_skybox_init_ogl_skybox         (_ogl_skybox*              skybox_ptr,
                                                         system_hashed_ansi_string name,
                                                         _ogl_skybox_type          type,
                                                         sh_samples                samples,
                                                         ogl_context               context,
                                                         ogl_texture               texture);
PRIVATE void        _ogl_skybox_init_ogl_skybox_sh      (_ogl_skybox*              skybox_ptr);
PRIVATE void        _ogl_skybox_init_ub                 (_ogl_skybox*              skybox_ptr);

#ifdef INCLUDE_OPENCL
    /** TODO */
    PRIVATE std::string _ogl_skybox_get_fragment_shader_body(sh_samples samples)
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
    PRIVATE void _ogl_skybox_init_ogl_skybox_sh(_ogl_skybox* skybox_ptr)
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
                            system_hashed_ansi_string_create(_ogl_skybox_get_fragment_shader_body(skybox_ptr->samples).c_str()) );

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
        _ogl_skybox_init_ub(skybox_ptr);

        /* Done */
        ogl_shader_release(fragment_shader);
        ogl_shader_release(vertex_shader);
    }
#endif

/** TODO */
PRIVATE void _ogl_skybox_init_ogl_skybox_spherical_projection_texture(_ogl_skybox* skybox_ptr)
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
                        system_hashed_ansi_string_create(fragment_shader_spherical_texture_preview) );

    /* Create a program */
    skybox_ptr->program = ogl_program_create(skybox_ptr->context,
                                             system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(skybox_ptr->name),
                                                                                                     " program"),
                                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ASSERT_DEBUG_SYNC(skybox_ptr->program != NULL,
                      "Could not create skybox program");

    if (!ogl_program_attach_shader(skybox_ptr->program,
                                   fragment_shader)     ||
        !ogl_program_attach_shader(skybox_ptr->program,
                                   vertex_shader))
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
    const ogl_program_variable* inverse_projection_uniform_descriptor  = NULL;
    const ogl_program_variable* mv_data_uniform_descriptor             = NULL;
    const ogl_program_variable* skybox_uniform_descriptor              = NULL;

    ogl_program_get_uniform_by_name(skybox_ptr->program,
                                    system_hashed_ansi_string_create("inv_projection"),
                                   &inverse_projection_uniform_descriptor);
    ogl_program_get_uniform_by_name(skybox_ptr->program,
                                    system_hashed_ansi_string_create("mv"),
                                   &mv_data_uniform_descriptor);
    ogl_program_get_uniform_by_name(skybox_ptr->program,
                                    system_hashed_ansi_string_create("skybox"),
                                   &skybox_uniform_descriptor);

    skybox_ptr->inverse_projection_ub_offset = (inverse_projection_uniform_descriptor != NULL) ? inverse_projection_uniform_descriptor->block_offset : -1;
    skybox_ptr->mv_ub_offset                 = (mv_data_uniform_descriptor            != NULL) ? mv_data_uniform_descriptor->block_offset            : -1;
    skybox_ptr->skybox_uniform_location      = (skybox_uniform_descriptor             != NULL) ? skybox_uniform_descriptor->location                 : -1;

    /* Retrieve uniform block info */
    _ogl_skybox_init_ub(skybox_ptr);

    /* Done */
    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);
}

/** TODO */
PRIVATE void _ogl_skybox_init_ogl_skybox(_ogl_skybox*              skybox_ptr,
                                         system_hashed_ansi_string name,
                                         _ogl_skybox_type          type,
                                         sh_samples                samples,
                                         ogl_context               context,
                                         ogl_texture               texture)
{
    memset(skybox_ptr,
           0,
           sizeof(_ogl_skybox) );

    skybox_ptr->context = context;
    skybox_ptr->name    = name;
    skybox_ptr->samples = samples;
    skybox_ptr->texture = texture;
    skybox_ptr->type    = type;

    /* Perform type-specific initialization */
    switch (type)
    {
#ifdef INCLUDE_OPENCL
        case OGL_SKYBOX_LIGHT_PROJECTION_SH:
        {
            _ogl_skybox_init_ogl_skybox_sh(skybox_ptr);

            break;
        } /* case OGL_SKYBOX_LIGHT_PROJECTION_SH: */
#endif

        case OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE:
        {
            _ogl_skybox_init_ogl_skybox_spherical_projection_texture(skybox_ptr);

            break;
        } /* case OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE: */

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized skybox type");
        } /* default:*/
    } /* switch (type) */

    ogl_context_retain(skybox_ptr->context);
}

/** TODO */
PRIVATE void _ogl_skybox_init_ub(_ogl_skybox* skybox_ptr)
{
    ogl_program_get_uniform_block_by_name(skybox_ptr->program,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &skybox_ptr->program_ub);

    ASSERT_DEBUG_SYNC(skybox_ptr->program_ub != NULL,
                      "dataVS uniform block descriptor is NULL.");

    ogl_program_ub_get_property(skybox_ptr->program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &skybox_ptr->program_ub_bo_size);
    ogl_program_ub_get_property(skybox_ptr->program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &skybox_ptr->program_ub_bo);
}

/** TODO */
PRIVATE void _ogl_skybox_release(void* skybox)
{
    _ogl_skybox* skybox_ptr = (_ogl_skybox*) skybox;

    ogl_context_release(skybox_ptr->context);
    ogl_program_release(skybox_ptr->program);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_skybox_verify_context_type(ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_skybox is only supported under GL contexts")
    }
#endif

#ifdef INCLUDE_OPENCL
    /** Please see header for specification */
    PUBLIC EMERALD_API ogl_skybox ogl_skybox_create_light_projection_sh(ogl_context               context,
                                                                        sh_samples                samples,
                                                                        system_hashed_ansi_string name)
    {
        _ogl_skybox_verify_context_type(context);

        _ogl_skybox* new_instance = new (std::nothrow) _ogl_skybox;

        ASSERT_DEBUG_SYNC(new_instance != NULL,
                          "Out of memory");

        if (new_instance != NULL)
        {
            _ogl_skybox_init_ogl_skybox(new_instance,
                                        name,
                                        OGL_SKYBOX_LIGHT_PROJECTION_SH,
                                        samples,
                                        context,
                                        NULL);

            REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                           _ogl_skybox_release,
                                                           OBJECT_TYPE_OGL_SKYBOX,
                                                           system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Skyboxes\\",
                                                                                                                   system_hashed_ansi_string_get_buffer(name)) );
        } /* if (new_instance != NULL) */

        return (ogl_skybox) new_instance;
    }
#endif

/** Please see header for specification */
PUBLIC EMERALD_API ogl_skybox ogl_skybox_create_spherical_projection_texture(ogl_context               context,
                                                                             ogl_texture               texture,
                                                                             system_hashed_ansi_string name)
{
    _ogl_skybox_verify_context_type(context);

    _ogl_skybox* new_instance = new (std::nothrow) _ogl_skybox;

    ASSERT_DEBUG_SYNC(new_instance != NULL,
                      "Out of memory");

    if (new_instance != NULL)
    {
        _ogl_skybox_init_ogl_skybox(new_instance,
                                    name,
                                    OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE,
                                    NULL,
                                    context,
                                    texture);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                       _ogl_skybox_release,
                                                       OBJECT_TYPE_OGL_SKYBOX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Skyboxes\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_instance != NULL) */

    return (ogl_skybox) new_instance;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_skybox_draw(ogl_skybox       skybox,
                                        ogl_texture      light_sh_data_tbo,
                                        system_matrix4x4 modelview,
                                        system_matrix4x4 inverted_projection)
{
    _ogl_skybox*                                              skybox_ptr        = (_ogl_skybox*) skybox;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points  = NULL;
    const ogl_context_gl_entrypoints*                         entry_points      = NULL;
    GLuint                                                    skybox_program_id = ogl_program_get_id(skybox_ptr->program);
    GLuint                                                    vao_id            = 0;

    ogl_context_get_property(skybox_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(skybox_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(skybox_ptr->context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entry_points->pGLUseProgram     (skybox_program_id);
    entry_points->pGLBindVertexArray(vao_id);

    /* Update general uniforms */
    ogl_program_ub_set_nonarrayed_uniform_value(skybox_ptr->program_ub,
                                                skybox_ptr->inverse_projection_ub_offset,
                                                system_matrix4x4_get_column_major_data(inverted_projection),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_set_nonarrayed_uniform_value(skybox_ptr->program_ub,
                                                skybox_ptr->mv_ub_offset,
                                                system_matrix4x4_get_column_major_data(modelview),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);

#ifdef INCLUDE_OPENCL
    /* Do SH-specific stuff */
    if (skybox_ptr->type == OGL_SKYBOX_LIGHT_PROJECTION_SH)
    {
        entry_points->pGLProgramUniform1i(skybox_program_id,
                                          skybox_ptr->input_sh_light_data_uniform_location,
                                          0);

        /* Bind SH data TBO */
        dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                 GL_TEXTURE_BUFFER,
                                                 light_sh_data_tbo);
    } /* if (skybox_ptr->type == OGL_SKYBOX_LIGHT_PROJECTION_SH) */
#endif

    /* Do spherial projection-specific stuff */
    if (skybox_ptr->type == OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE)
    {
        dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                 GL_TEXTURE_2D,
                                                 skybox_ptr->texture);
    } /* if (skybox_ptr->type == OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE) */

    /* Draw. Do not modify depth information */
    GLuint   program_ub_bo_id           = 0;
    uint32_t program_ub_bo_start_offset = -1;

    raGL_buffer_get_property(skybox_ptr->program_ub_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_ub_bo_id);
    raGL_buffer_get_property(skybox_ptr->program_ub_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_ub_bo_start_offset);

    ogl_program_ub_sync(skybox_ptr->program_ub);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                     program_ub_bo_id,
                                     program_ub_bo_start_offset,
                                     skybox_ptr->program_ub_bo_size);
    entry_points->pGLDepthMask      (GL_FALSE);
    {
        entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                    0,
                                    4);
    }
    entry_points->pGLDepthMask(GL_TRUE);
}
