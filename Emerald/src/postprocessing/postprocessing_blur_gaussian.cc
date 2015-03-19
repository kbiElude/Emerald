/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shaders.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_textures.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>


#define COEFFS_DATA_UB_BP               (0)
#define DATA_SAMPLER_TEXTURE_UNIT_INDEX (0)
#define DATA_SAMPLER_UNIFORM_LOCATION   (0)
#define N_TAPS_UNIFORM_LOCATION         (1)


static const char* fs_preamble = "#version 420 core\n"
                                 "\n"
                                 "#extension GL_ARB_explicit_uniform_location : require\n"
                                 "\n";
static const char* fs_body =     "layout(binding = 0, std140) uniform coeffs_data\n"
                                 "{\n"
                                 /* data[0]..data[n_taps_half-1]: coeffs, data[n_taps_half..n_taps_half*2-1] - offsets */
                                 "    float data[N_MAX_COEFFS];\n"
                                 "};\n"
                                 "\n"
                                 "layout(location = 0, binding = 0) uniform sampler2DArray data_sampler;\n"
                                 "layout(location = 1)              uniform int            n_taps;\n"
                                 "\n"
                                 "flat in  vec2 offset_multiplier;\n"
                                 "flat in  int  read_layer_id;\n"
                                 "     out vec4 result;\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    const int  n_taps_half  = (n_taps + 1) / 2;\n"
                                 "    const int  n_taps_mod_2 = n_taps % 2;\n"
                                 "    const int  n_start_tap  = (n_taps_mod_2 != 0) ? 1 : 0;\n"
                                 "          vec2 texture_size = vec2(textureSize(data_sampler, 0).xy);\n"
                                 "          vec2 uv           = gl_FragCoord.xy / texture_size;\n"
                                 "\n"
                                 "    if (n_taps_mod_2 != 0)\n"
                                 "    {\n"
                                 "        result = textureLod(data_sampler,\n"
                                 "                            vec3(uv, float(read_layer_id) ),\n"
                                 "                            0.0) * data[0];\n"
                                 "    }\n"
                                 "    else\n"
                                 "    {\n"
                                 "        result = vec4(0.0);\n"
                                 "    }\n"
                                 "\n"
                                 "    for (int n_tap = n_start_tap;\n"
                                 "             n_tap < n_taps_half;\n"
                                 "             n_tap ++)\n"
                                 "    {\n"
                                 "        vec4 tap_weight = vec4(data[n_tap]);\n"
                                 "        vec2 tap_offset = vec2(data[n_taps_half + n_tap]);\n"
                                 "\n"
                                 "        result += textureLod(data_sampler,\n"
                                 "                             vec3(uv + offset_multiplier * tap_offset, float(read_layer_id) ),\n"
                                 "                             0.0) * tap_weight;\n"
                                 "\n"
                                 "        if (n_tap != 0)\n"
                                 "        {\n"
                                 "            result += textureLod(data_sampler,\n"
                                 "                                 vec3(uv - offset_multiplier * tap_offset, float(read_layer_id) ),\n"
                                 "                                 0.0) * tap_weight;\n"
                                 "        }\n"
                                 "        else\n"
                                 "        {\n"
                                 "            result *= 2.0;\n"
                                 "        }\n"
                                 "    }\n"
                                 "}\n";
static const char* vs_body =     "#version 420 core\n"
                                 "\n"
                                 "#extension GL_ARB_explicit_uniform_location : require\n"
                                 "\n"
                                 "layout(location = 0, binding = 0)      uniform sampler2DArray data_sampler;\n"
                                 "                                  flat out     int            read_layer_id;\n"
                                 "                                  flat out     vec2           offset_multiplier;\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    int vertex_id_mod_4 = gl_VertexID % 4;\n"
                                 "\n"
                                 "    read_layer_id = gl_VertexID / 4;\n"
                                 "\n"
                                 "    if (read_layer_id == 0)\n"
                                 "    {\n"
                                 "        offset_multiplier = vec2(1.0 / textureSize(data_sampler, 0).x, 0.0);\n"
                                 "    }\n"
                                 "    else\n"
                                 "    {\n"
                                 "        offset_multiplier = vec2(0.0, 1.0 / textureSize(data_sampler, 0).y);\n"
                                 "    }\n"
                                 "\n"
                                 "    switch (vertex_id_mod_4)\n"
                                 "    {\n"
                                 "        case 0: gl_Position = vec4(-1.0, -1.0, 0.0, 1.0); break;\n"
                                 "        case 1: gl_Position = vec4(-1.0,  1.0, 0.0, 1.0); break;\n"
                                 "        case 2: gl_Position = vec4( 1.0, -1.0, 0.0, 1.0); break;\n"
                                 "        case 3: gl_Position = vec4( 1.0,  1.0, 0.0, 1.0); break;\n"
                                 "    }\n"
                                 "}\n";

/** Internal type definition */
typedef struct _postprocessing_blur_gaussian
{
    GLuint                    coeff_bo_id;
    unsigned int*             coeff_buffer_offsets; // [0] for n_min_taps, [1] for n_min_taps+1, etc..
    ogl_context               context;
    GLuint                    fbo_ids[2]; /* ping/pong FBO */
    system_hashed_ansi_string name;
    unsigned int              n_max_data_coeffs;
    unsigned int              n_max_taps;
    unsigned int              n_min_taps;
    ogl_program               po;

    _postprocessing_blur_gaussian(__in __notnull ogl_context               in_context,
                                  __in           unsigned int              in_n_min_taps,
                                  __in           unsigned int              in_n_max_taps,
                                  __in __notnull system_hashed_ansi_string in_name)
    {
        ASSERT_DEBUG_SYNC(in_n_min_taps <= in_n_max_taps,
                          "Invalid min/max tap argument values");

        coeff_bo_id          = 0;
        coeff_buffer_offsets = NULL;
        context              = in_context;
        n_max_data_coeffs    = 0;
        n_max_taps           = in_n_max_taps;
        n_min_taps           = in_n_min_taps;
        name                 = in_name;
        po                   = NULL;

        memset(fbo_ids,
               0,
               sizeof(fbo_ids) );

        ogl_context_retain(context);
    }

    ~_postprocessing_blur_gaussian()
    {
        ASSERT_DEBUG_SYNC(coeff_bo_id == 0,
                          "Coefficients BO id is not 0");
        ASSERT_DEBUG_SYNC(fbo_ids[0] == 0 &&
                          fbo_ids[1] == 0,
                          "Ping/pong FBOs not released");

        if (coeff_buffer_offsets != NULL)
        {
            delete [] coeff_buffer_offsets;

            coeff_buffer_offsets = NULL;
        }

        if (context != NULL)
        {
            ogl_context_release(context);

            context = NULL;
        }

        if (po != NULL)
        {
            ogl_program_release(po);

            po = NULL;
        }
    }

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_blur_gaussian;


/** Internal variables */

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_blur_gaussian,
                               postprocessing_blur_gaussian,
                              _postprocessing_blur_gaussian);


/** TODO */
PRIVATE void _postprocessing_blur_gaussian_deinit_rendering_thread_callback(__in __notnull ogl_context context,
                                                                            __in __notnull void*       user_arg)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _postprocessing_blur_gaussian*    instance_ptr    = (_postprocessing_blur_gaussian*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (instance_ptr->coeff_bo_id != 0)
    {
        entrypoints_ptr->pGLDeleteBuffers(1,
                                         &instance_ptr->coeff_bo_id);

        instance_ptr->coeff_bo_id = 0;
    } /* if (instance_ptr->coeff_bo_id != 0) */

    if (instance_ptr->fbo_ids[0] != 0 ||
        instance_ptr->fbo_ids[1] != 0)
    {
        /* Invalid FBO ids will be released */
        entrypoints_ptr->pGLDeleteFramebuffers(sizeof(instance_ptr->fbo_ids) / sizeof(instance_ptr->fbo_ids[0]),
                                               instance_ptr->fbo_ids);
    }
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_blur_gaussian_get_fs_name(__in uint32_t n_max_data_coeffs)
{
    std::stringstream name_sstream;

    name_sstream << "Postprocessing blur gaussian "
                 << n_max_data_coeffs
                 << " FS";

    return system_hashed_ansi_string_create(name_sstream.str().c_str() );
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_blur_gaussian_get_po_name(__in uint32_t n_max_data_coeffs)
{
    std::stringstream name_sstream;

    name_sstream << "Postprocessing blur gaussian "
                 << n_max_data_coeffs
                 << " PO";

    return system_hashed_ansi_string_create(name_sstream.str().c_str() );
}

/** TODO */
PRIVATE void _postprocessing_blur_gaussian_init_rendering_thread_callback(__in __notnull ogl_context context,
                                                                          __in __notnull void*       user_arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    _postprocessing_blur_gaussian*                            instance_ptr        = (_postprocessing_blur_gaussian*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Generate the coefficients.
     *
     * To strengthen the blur effect, we ignore the first two coeffs which give little contribution
     * to the result value. This is why we bump the input n_taps value by 3. For example, for n_taps = 4 we get:
     *
     * Index:                  7
     * Pascal triangle values: 1 7 21 35 35 21 7 1
     *
     * In the end, we will use the "21 35 35 21" value set".
     *
     * We generate coefficient sets for the requested tap range.
     *
     *
     * Apart from coeffs, we also store tap offsets. While currently these are pretty straightforward,
     * they will later on be adjusted to make use of the hw-accelerated bilinear filtering which will
     * reduce the number of load ops by ~half.
     */
    const unsigned int max_binomial_n        = instance_ptr->n_max_taps + 3;
    const unsigned int max_n_binomial_values = max_binomial_n + 1;
    unsigned __int64*  binomial_values       = new (std::nothrow) unsigned __int64[max_n_binomial_values];
    unsigned __int64*  factorial_values      = new (std::nothrow) unsigned __int64[max_n_binomial_values];

    ASSERT_DEBUG_SYNC(max_binomial_n < 20,
                      "Insufficient precision for requested number of factorial values");

    ASSERT_DEBUG_SYNC(binomial_values  != NULL &&
                      factorial_values != NULL,
                      "Out of memory");

    /* Precalc the factorial values */
    for (unsigned int n_factorial_value = 0;
                      n_factorial_value < max_n_binomial_values;
                    ++n_factorial_value)
    {
        if (n_factorial_value == 0)
        {
            factorial_values[0] = 1;
        }
        else
        {
            factorial_values[n_factorial_value] = factorial_values[n_factorial_value - 1] * n_factorial_value;
        }
    } /* for (all factorial values) */

    /* Generate binomial data array. The number of coeffs is equal to: 2*sum(n_min_tap + (n_min_tap + 1) + .. + 3 * n_taps) */
    system_resizable_vector coeff_vector              = system_resizable_vector_create(4, /* capacity */
                                                                                       sizeof(float) );
          unsigned int      n_max_data_coeffs         = 0;
    const unsigned int      n_max_taps_plus_1         = instance_ptr->n_max_taps + 1;
    const unsigned int      n_max_taps_plus_1_half_2  = (instance_ptr->n_max_taps + 1) / 2;
    const unsigned int      n_min_taps_minus_1_half_2 = instance_ptr->n_min_taps - 1;
    const unsigned int      n_tap_datasets            = n_max_taps_plus_1 - instance_ptr->n_min_taps;
    const float             padding                   = 0.0f;

    instance_ptr->coeff_buffer_offsets = new (std::nothrow) unsigned int[n_tap_datasets];

    for (unsigned int n_taps  = instance_ptr->n_min_taps;
                      n_taps <= instance_ptr->n_max_taps;
                    ++n_taps)
    {
        /* Store the offset for this dataset */
        instance_ptr->coeff_buffer_offsets[n_taps - instance_ptr->n_min_taps] = system_resizable_vector_get_amount_of_elements(coeff_vector) * sizeof(float);

        /* Precalc the binomial values */
        const unsigned int binomial_n        = n_taps + 3;
        unsigned __int64   binomial_sum      = 0;
        const unsigned int n_binomial_values = binomial_n + 1;
        const unsigned int n_tap_weights     = (n_taps + 1) / 2;

        for (unsigned int n_binomial_value = 0;
                          n_binomial_value < n_binomial_values;
                        ++n_binomial_value)
        {
            /* NOTE: We write the result values in reversed order */
            const unsigned int write_index = n_binomial_values - n_binomial_value - 1;

            binomial_values[write_index]  = (unsigned int) ((factorial_values[binomial_n]) /
                                                            (factorial_values[n_binomial_value] * factorial_values[binomial_n - n_binomial_value]));
            binomial_sum                 += binomial_values[write_index];
        } /* for (all binomial values) */

        /* Calculate the tap weights */
        float sanity_check_sum = 0.0f;

        binomial_sum -= 2 * (1 + binomial_n); /* subtract the first and the last two binomial values */

        for (int n_tap_weight  = n_tap_weights - 1;
                 n_tap_weight >= 0;
               --n_tap_weight)
        {
            const float coeff = float(binomial_values[2 + n_tap_weight]) / float(binomial_sum);

            sanity_check_sum += coeff;

            /* Store the coeff.
             *
             * NOTE: We use std140 packing, so also add padding */
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &coeff));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
        } /* for (all tap weights) */

        ASSERT_DEBUG_SYNC(fabs(2.0f * sanity_check_sum - 1.0f) < 1e-5f,
                          "Sanity check failed");

        /* Calculate the tap offsets */
        for (unsigned int n_tap_offset = 0;
                          n_tap_offset < n_tap_weights;
                        ++n_tap_offset)
        {
            /* Two cases here:
             *
             * For an odd number of taps, we get the following binomial set:
             *
             *            A B C D E D C B A
             *
             * which is fine, because we can set offset values to a set of consecutive
             * texel offsets (in px space)
             *
             * For an even number of taps, things get a dab whack:
             *
             *             A B C D C B A
             *
             * In this case, we only sample D once (and give it a doubled weight, and
             * need to ensure that the offsets are properly adjusted, so that:
             * - central texel are unaffected
             * - border texels are unaffected
             *
             */
            const float offset = ((n_taps % 2) != 0) ?  float(n_tap_offset)
                                                     : (float(n_tap_weights) * float(n_tap_offset) / float(n_tap_weights - 1));

            system_resizable_vector_push(coeff_vector,
                                         *((void**) &offset));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
        } /* for (all tap offsets) */

        /* Update the n_max_data_coeffs variable. */
        n_max_data_coeffs = n_tap_weights * 2 /* weight + offset */;
    }

    /* n_max_data_coeffs tells how much data we need bound for the whole run. Store it */
    instance_ptr->n_max_data_coeffs = n_max_data_coeffs;

    /* Set up the BO */
    entrypoints_ptr->pGLGenBuffers               (1,
                                                 &instance_ptr->coeff_bo_id);
    dsa_entrypoints_ptr->pGLNamedBufferStorageEXT(instance_ptr->coeff_bo_id,
                                                  system_resizable_vector_get_amount_of_elements(coeff_vector) * sizeof(float),
                                                  system_resizable_vector_get_array             (coeff_vector),
                                                  0); /* flags */

    /* Set up the PO */
    ogl_shader                      fs       = NULL;
    const system_hashed_ansi_string fs_name  = _postprocessing_blur_gaussian_get_fs_name(n_max_data_coeffs);
    const system_hashed_ansi_string po_name  = _postprocessing_blur_gaussian_get_po_name(n_max_data_coeffs);
    ogl_programs                    programs = NULL;
    ogl_shaders                     shaders  = NULL;
    ogl_shader                      vs       = NULL;
    const system_hashed_ansi_string vs_name  = system_hashed_ansi_string_create("Postprocessing blur Gaussian VS");

    ogl_context_get_property(instance_ptr->context,
                             OGL_CONTEXT_PROPERTY_PROGRAMS,
                            &programs);
    ogl_context_get_property(instance_ptr->context,
                             OGL_CONTEXT_PROPERTY_SHADERS,
                            &shaders);

    if ( (instance_ptr->po = ogl_programs_get_program_by_name(programs,
                                                              po_name)) == NULL)
    {
        instance_ptr->po = ogl_program_create(instance_ptr->context,
                                              po_name);

        if ( (fs = ogl_shaders_get_shader_by_name(shaders,
                                                  fs_name)) == NULL)
        {
            /* Create the FS body */
            std::stringstream fs_body_sstream;

            fs_body_sstream << fs_preamble
                            << "#define N_MAX_COEFFS (" << n_max_data_coeffs << ")\n"
                            << fs_body;

            /* Create the fragment shader */
            fs = ogl_shader_create(context,
                                   SHADER_TYPE_FRAGMENT,
                                   fs_name);

            ogl_shader_set_body(fs,
                                system_hashed_ansi_string_create(fs_body_sstream.str().c_str() ) );

            if (!ogl_shader_compile(fs) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not link postprocessing blur gaussian fragment shader");
            }
        } /* if (no compiled FS is available) */

        if ( (vs = ogl_shaders_get_shader_by_name(shaders,
                                                  vs_name)) == NULL)
        {
            /* Create the vertex shader */
            vs = ogl_shader_create(context,
                                   SHADER_TYPE_VERTEX,
                                   vs_name);

            ogl_shader_set_body(vs,
                                system_hashed_ansi_string_create(vs_body) );

            if (!ogl_shader_compile(vs) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not link postprocessing blur gaussian vertex shader");
            }
        } /* if (no compiled VS is available) */

        ogl_program_attach_shader(instance_ptr->po,
                                  fs);

        ogl_program_attach_shader(instance_ptr->po,
                                  vs);

        if (!ogl_program_link(instance_ptr->po) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not link postprocessing blur gaussian program object");
        }
    }
    else
    {
        ogl_program_retain(instance_ptr->po);
    }

    /* Reserve FBO ids */
    entrypoints_ptr->pGLGenFramebuffers(sizeof(instance_ptr->fbo_ids) / sizeof(instance_ptr->fbo_ids[0]),
                                        instance_ptr->fbo_ids);

    /* Clean up */
    delete [] binomial_values;
    binomial_values = NULL;

    if (coeff_vector != NULL)
    {
        system_resizable_vector_release(coeff_vector);

        coeff_vector = NULL;
    }

    delete [] factorial_values;
    factorial_values = NULL;

    if (fs != NULL)
    {
        ogl_shader_release(fs);

        fs = NULL;
    }

    if (vs != NULL)
    {
        ogl_shader_release(vs);

        vs = NULL;
    }
}

/** TODO */
PRIVATE void _postprocessing_blur_gaussian_release(__in __notnull __deallocate(mem) void* ptr)
{
    _postprocessing_blur_gaussian* data_ptr = (_postprocessing_blur_gaussian*) ptr;

    ogl_context_request_callback_from_context_thread(data_ptr->context,
                                                     _postprocessing_blur_gaussian_deinit_rendering_thread_callback,
                                                     data_ptr);
}


/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API postprocessing_blur_gaussian postprocessing_blur_gaussian_create(__in __notnull ogl_context               context,
                                                                                                           __in __notnull system_hashed_ansi_string name,
                                                                                                           __in           unsigned int              n_min_taps,
                                                                                                           __in           unsigned int              n_max_taps)
{
    /* Instantiate the object */
    _postprocessing_blur_gaussian* result_object = new (std::nothrow) _postprocessing_blur_gaussian(context,
                                                                                                    n_min_taps,
                                                                                                    n_max_taps,
                                                                                                    name);

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    /* Since we're already in a rendering thread, just call the rendering thread callback entry-point
     * directly. */
    _postprocessing_blur_gaussian_init_rendering_thread_callback(context,
                                                                 result_object);

    /* Register it with the object manager */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _postprocessing_blur_gaussian_release,
                                                   OBJECT_TYPE_POSTPROCESSING_BLUR_GAUSSIAN,
                                                   GET_OBJECT_PATH(name,
                                                                   OBJECT_TYPE_POSTPROCESSING_BLUR_GAUSSIAN,
                                                                   NULL) );

    /* Return the object */
    return (postprocessing_blur_gaussian) result_object;

end:
    if (result_object != NULL)
    {
        _postprocessing_blur_gaussian_release(result_object);

        delete result_object;
    }

    return NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void postprocessing_blur_gaussian_execute(__in __notnull postprocessing_blur_gaussian blur,
                                                                                    __in           unsigned int                 n_taps,
                                                                                    __in           unsigned int                 n_iterations,
                                                                                    __in __notnull ogl_texture                  src_texture)
{
    _postprocessing_blur_gaussian*                            blur_ptr                   = (_postprocessing_blur_gaussian*) blur;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr        = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr            = NULL;
    unsigned int                                              src_texture_height         = 0;
    GLenum                                                    src_texture_internalformat = GL_NONE;
    unsigned int                                              src_texture_width          = 0;
    ogl_context_state_cache                                   state_cache                = NULL;
    ogl_texture                                               temp_2d_array_texture      = NULL;
    GLuint                                                    vao_id                     = 0;
    GLint                                                     viewport_data[4]           = {0};

    ASSERT_DEBUG_SYNC(n_taps >= blur_ptr->n_min_taps &&
                      n_taps <= blur_ptr->n_max_taps,
                      "Invalid number of taps requested");

    ogl_context_get_property            (blur_ptr->context,
                                         OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                        &dsa_entrypoints_ptr);
    ogl_context_get_property            (blur_ptr->context,
                                         OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                        &entrypoints_ptr);
    ogl_context_get_property            (blur_ptr->context,
                                         OGL_CONTEXT_PROPERTY_STATE_CACHE,
                                        &state_cache);
    ogl_context_get_property            (blur_ptr->context,
                                         OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                                        &vao_id);
    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT,
                                         viewport_data);
    ogl_texture_get_mipmap_property     (src_texture,
                                         0, /* mipmap_level */
                                         OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                         &src_texture_height);
    ogl_texture_get_property            (src_texture,
                                         OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                         &src_texture_internalformat);
    ogl_texture_get_mipmap_property     (src_texture,
                                         0, /* mipmap_level */
                                         OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                         &src_texture_width);

    /* Retrieve a 2D Array texture we will use for execution of the iterations */
    temp_2d_array_texture = ogl_textures_get_texture_from_pool(blur_ptr->context,
                                                               OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_ARRAY,
                                                               1, /* n_mipmaps */
                                                               src_texture_internalformat,
                                                               src_texture_width,
                                                               src_texture_height,
                                                               2,      /* base_mipmap_depth */
                                                               1,      /* n_samples */
                                                               false); /* fixed_sample_locations */

    /* The function carries out the following tasks:
     *
     * 1) Copies data from the source texture to ping (0th) texture layer.
     *    Draw (ping) FBO is attached the ping texture layer.
     *    Read (pong) FBO is attached the source texture.
     *
     * 2) For each iteration:
     *    a) Horizontal pass:
     *       X Draw (pong) FBO is attached the pong (1st) texture layer.
     *       X Read (ping) FBO is attached the ping (0th) texture layer.
     *
     *    b) Vertical pass:
     *       X Draw (ping) FBO is attached the ping (0th) texture layer.
     *       X Read (pong) FBO is attached the pong (1st) texture layer.
     *
     * 3) Copies data from the ping (0th) texture layer to the source texture.
     *    Draw (pong) FBO is attached the source texture.
     *    Read (ping) FBO is attached the ping (0th) texture layer.
     */
    const GLuint ping_fbo_id = blur_ptr->fbo_ids[0];
    const GLuint pong_fbo_id = blur_ptr->fbo_ids[1];

    entrypoints_ptr->pGLColorMask(GL_TRUE,
                                  GL_TRUE,
                                  GL_TRUE,
                                  GL_TRUE);
    entrypoints_ptr->pGLDisable  (GL_CULL_FACE);

    /* Step 1) */
    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        ping_fbo_id);
    entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                        pong_fbo_id);

    dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(ping_fbo_id,
                                                            GL_COLOR_ATTACHMENT0,
                                                            temp_2d_array_texture,
                                                            0,  /* level */
                                                            0); /* layer */
    dsa_entrypoints_ptr->pGLNamedFramebufferTexture2DEXT   (pong_fbo_id,
                                                            GL_COLOR_ATTACHMENT0,
                                                            GL_TEXTURE_2D,
                                                            src_texture,
                                                            0); /* level */
    entrypoints_ptr->pGLBlitFramebuffer(0, /* srcX0 */
                                        0, /* srcY0 */
                                        src_texture_width,
                                        src_texture_height,
                                        0, /* dstX0 */
                                        0, /* dstY0 */
                                        src_texture_width,
                                        src_texture_height,
                                        GL_COLOR_BUFFER_BIT,
                                        GL_NEAREST); /* no need for any interpolation */

    /* Step 2): Set-up */
    const GLuint po_id = ogl_program_get_id(blur_ptr->po);

    entrypoints_ptr->pGLBindVertexArray(vao_id);
    entrypoints_ptr->pGLUseProgram     (po_id);

    dsa_entrypoints_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0 + DATA_SAMPLER_TEXTURE_UNIT_INDEX,
                                                GL_TEXTURE_2D_ARRAY,
                                                temp_2d_array_texture);
    entrypoints_ptr->pGLBindBufferRange        (GL_UNIFORM_BUFFER,
                                                COEFFS_DATA_UB_BP,
                                                blur_ptr->coeff_bo_id,
                                                blur_ptr->coeff_buffer_offsets[n_taps - blur_ptr->n_min_taps],
                                                blur_ptr->n_max_data_coeffs * 4 /* padding */ * sizeof(float) );
    entrypoints_ptr->pGLProgramUniform1i       (po_id,
                                                N_TAPS_UNIFORM_LOCATION,
                                                n_taps);

    entrypoints_ptr->pGLViewport                           (0, /* x */
                                                            0, /* y */
                                                            src_texture_width,
                                                            src_texture_height);
    dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(ping_fbo_id,
                                                            GL_COLOR_ATTACHMENT0,
                                                            temp_2d_array_texture,
                                                            0,  /* level */
                                                            0); /* layer */
    dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(pong_fbo_id,
                                                            GL_COLOR_ATTACHMENT0,
                                                            temp_2d_array_texture,
                                                            0,  /* level */
                                                            1); /* layer */

    /* Step 2): Kick off! */
    for (unsigned int n_iteration = 0;
                      n_iteration < n_iterations;
                    ++n_iteration)
    {
        /* Horizontal pass */
        entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                            pong_fbo_id);
        entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                            ping_fbo_id);

        entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                       0,  /* first - 0 will cause FS to read from layer 0 */
                                       4); /* count */

        /* Vertical pass */
        entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                            ping_fbo_id);
        entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                            pong_fbo_id);

        entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                       4,  /* first - 4 will cause FS to read from layer 1 */
                                       4); /* count */
    } /* for (all iterations) */

    /* Step 3): Store the result in the user-specified texture */
    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        pong_fbo_id);
    entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                        ping_fbo_id);

    dsa_entrypoints_ptr->pGLNamedFramebufferTexture2DEXT(pong_fbo_id,
                                                         GL_COLOR_ATTACHMENT0,
                                                         GL_TEXTURE_2D,
                                                         src_texture,
                                                         0); /* level */

    entrypoints_ptr->pGLBlitFramebuffer(0,                  /* srcX0 */
                                        0,                  /* srcY0 */
                                        src_texture_width,
                                        src_texture_height,
                                        0,                  /* dstX0 */
                                        0,                  /* dstY0 */
                                        src_texture_width,
                                        src_texture_height,
                                        GL_COLOR_BUFFER_BIT,
                                        GL_NEAREST);

    /* All done */
    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        0);
    entrypoints_ptr->pGLViewport       (viewport_data[0],
                                        viewport_data[1],
                                        viewport_data[2],
                                        viewport_data[3]);

    ogl_textures_return_reusable(blur_ptr->context,
                                 temp_2d_array_texture);
}
