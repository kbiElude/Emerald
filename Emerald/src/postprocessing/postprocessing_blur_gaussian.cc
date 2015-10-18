/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_samplers.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_context_textures.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_sampler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shaders.h"
#include "ogl/ogl_texture.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_types.h"
#include <sstream>


#define COEFFS_DATA_UB_BP               (0)
#define DATA_SAMPLER_TEXTURE_UNIT_INDEX (0)
#define DATA_SAMPLER_UNIFORM_LOCATION   (0)
#define OTHER_DATA_UB_BP                (1)

static const char* fs_preamble = "#version 430 core\n"
                                 "\n";
static const char* fs_body =     "layout(binding = 0, std140) uniform coeffs_data\n"
                                 "{\n"
                                 /* data[0]..data[n_taps_half-1]: coeffs, data[n_taps_half..n_taps_half*2-1] - offsets */
                                 "    float data[N_MAX_COEFFS];\n"
                                 "};\n"
                                 "\n"
                                 "layout(binding = 1, std140) uniform other_data\n"
                                 "{\n"
                                 "    int n_taps;\n"
                                 "};\n"
                                 "\n"
                                 "layout(location = 0, binding = 0) uniform sampler2DArray data_sampler;\n"
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
                                 /* Note: we also use this program for blending if frac(n_taps) != .0. In this case,
                                  *       n_taps is set to 1, which essentially fetches 1 texel from data_sampler and
                                  *       returns the read value. data[0] is set to 1.0 for this iteration.
                                  */
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
                                 "        result += textureLod(data_sampler,\n"
                                 "                             vec3(uv - offset_multiplier * tap_offset, float(read_layer_id) ),\n"
                                 "                             0.0) * tap_weight;\n"
                                 "    }\n"
                                 "}\n";
static const char* vs_body =     "#version 430 core\n"
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
    ogl_buffers               buffers;     /* owned by ogl_context - do NOT release */
    GLuint                    coeff_bo_id; /* owned by ogl_buffers - do NOT release */
    unsigned int              coeff_bo_start_offset;
    unsigned int              coeff_buffer_offset_for_value_1; /* holds offset to where 1.0 is stored in BO with id coeff_bo_id */
    unsigned int*             coeff_buffer_offsets; // [0] for n_min_taps, [1] for n_min_taps+1, etc..
    ogl_context               context;
    GLuint                    fbo_ids[2]; /* ping/pong FBO */
    system_hashed_ansi_string name;
    unsigned int              n_max_data_coeffs;
    unsigned int              n_max_taps;
    unsigned int              n_min_taps;
    ogl_program               po;
    ogl_sampler               sampler; /* do not release - retrieved from context-wide ogl_samplers */

    /* other data BO holds:
     *
     * [0] iteration-specific n_taps value.
     * [1] 1
     *
     * The [0] data is only uploaded when it changes. when performing the actual rendering process, we do GPU<->GPU
     * memory copies to avoid pipeline stalls.
     */
    unsigned int other_data_bo_cached_value;
    GLuint       other_data_bo_id;           /* owned by ogl_buffers - do NOT release */
    unsigned int other_data_bo_start_offset;

    _postprocessing_blur_gaussian(ogl_context               in_context,
                                  unsigned int              in_n_min_taps,
                                  unsigned int              in_n_max_taps,
                                  system_hashed_ansi_string in_name)
    {
        ASSERT_DEBUG_SYNC(in_n_min_taps <= in_n_max_taps,
                          "Invalid min/max tap argument values");

        buffers                         = NULL;
        coeff_bo_id                     = 0;
        coeff_bo_start_offset           = -1;
        coeff_buffer_offset_for_value_1 = 0; /* always zero */
        coeff_buffer_offsets            = NULL;
        context                         = in_context;
        n_max_data_coeffs               = 0;
        n_max_taps                      = in_n_max_taps;
        n_min_taps                      = in_n_min_taps;
        name                            = in_name;
        other_data_bo_cached_value      = 0;
        other_data_bo_id                = 0;
        other_data_bo_start_offset      = -1;
        po                              = NULL;
        sampler                         = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &buffers);

        memset(fbo_ids,
               0,
               sizeof(fbo_ids) );

        ogl_context_retain(context);
    }

    ~_postprocessing_blur_gaussian()
    {
        ASSERT_DEBUG_SYNC(fbo_ids[0] == 0 &&
                          fbo_ids[1] == 0,
                          "Ping/pong FBOs not released");

        if (coeff_buffer_offsets != NULL)
        {
            delete [] coeff_buffer_offsets;

            coeff_buffer_offsets = NULL;
        }

        if (po != NULL)
        {
            ogl_program_release(po);

            po = NULL;
        }

        if (context != NULL)
        {
            ogl_context_release(context);

            context = NULL;
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
PRIVATE void _postprocessing_blur_gaussian_deinit_rendering_thread_callback(ogl_context context,
                                                                            void*       user_arg)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _postprocessing_blur_gaussian*    instance_ptr    = (_postprocessing_blur_gaussian*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (instance_ptr->coeff_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(instance_ptr->buffers,
                                       instance_ptr->coeff_bo_id,
                                       instance_ptr->coeff_bo_start_offset);

        instance_ptr->coeff_bo_id           = 0;
        instance_ptr->coeff_bo_start_offset = -1;
    } /* if (instance_ptr->coeff_bo_id != 0) */

    if (instance_ptr->fbo_ids[0] != 0 ||
        instance_ptr->fbo_ids[1] != 0)
    {
        /* Invalid FBO ids will be released */
        entrypoints_ptr->pGLDeleteFramebuffers(sizeof(instance_ptr->fbo_ids) / sizeof(instance_ptr->fbo_ids[0]),
                                               instance_ptr->fbo_ids);

        memset(instance_ptr->fbo_ids,
               0,
               sizeof(instance_ptr->fbo_ids) );
    }

    if (instance_ptr->other_data_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(instance_ptr->buffers,
                                       instance_ptr->other_data_bo_id,
                                       instance_ptr->other_data_bo_start_offset);
    } /* if (instance_ptr->other_data_bo_id != 0) */
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_blur_gaussian_get_fs_name(uint32_t n_max_data_coeffs)
{
    std::stringstream name_sstream;

    name_sstream << "Postprocessing blur gaussian "
                 << n_max_data_coeffs
                 << " FS";

    return system_hashed_ansi_string_create(name_sstream.str().c_str() );
}

/** TODO */
PRIVATE system_hashed_ansi_string _postprocessing_blur_gaussian_get_po_name(uint32_t n_max_data_coeffs)
{
    std::stringstream name_sstream;

    name_sstream << "Postprocessing blur gaussian "
                 << n_max_data_coeffs
                 << " PO";

    return system_hashed_ansi_string_create(name_sstream.str().c_str() );
}

/** TODO */
PRIVATE void _postprocessing_blur_gaussian_init_rendering_thread_callback(ogl_context context,
                                                                          void*       user_arg)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    const ogl_context_gl_limits*                              limits_ptr          = NULL;
    _postprocessing_blur_gaussian*                            instance_ptr        = (_postprocessing_blur_gaussian*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

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
    __uint64*          binomial_values       = new (std::nothrow) __uint64[max_n_binomial_values];
    __uint64*          factorial_values      = new (std::nothrow) __uint64[max_n_binomial_values];

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
    system_resizable_vector coeff_vector              = system_resizable_vector_create(4 /* capacity */);
          unsigned int      n_max_data_coeffs         = 0;
    const unsigned int      n_max_taps_plus_1         = instance_ptr->n_max_taps + 1;
    const unsigned int      n_max_taps_plus_1_half_2  = (instance_ptr->n_max_taps + 1) / 2;
    const unsigned int      n_min_taps_minus_1_half_2 = instance_ptr->n_min_taps - 1;
    const unsigned int      n_tap_datasets            = n_max_taps_plus_1 - instance_ptr->n_min_taps;

    /* Uniform buffer bindings must be aligned. When we generate cooefs & offsets, we ignore this. Later on,
     * after these are calculated, we will prepare a final data buffer which will also take UB alignment requirements
     * into consideration.
     */
    unsigned int* misaligned_coeff_buffer_offsets = new (std::nothrow) unsigned int[n_tap_datasets];

    ASSERT_DEBUG_SYNC(misaligned_coeff_buffer_offsets != NULL,
                      "Out of memory");

    for (unsigned int n_taps  = instance_ptr->n_min_taps;
                      n_taps <= instance_ptr->n_max_taps;
                    ++n_taps)
    {
        const float padding = 0.0f;

        /* Store the offset for this dataset */
        uint32_t n_coeff_vector_items = 0;

        system_resizable_vector_get_property(coeff_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_coeff_vector_items);

        misaligned_coeff_buffer_offsets[n_taps - instance_ptr->n_min_taps] = n_coeff_vector_items * sizeof(float);

        /* Precalc the binomial values */
        const unsigned int binomial_n        = n_taps + 3;
        __uint64           binomial_sum      = 0;
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
        unsigned int n_tap_dataset_coeffs = 0;
        int          n_tap_weight_center  = n_tap_weights - 1;
        float        sanity_check_sum     = 0.0f;

        binomial_sum -= 2 * (1 + binomial_n); /* subtract the first and the last two binomial values */

        ASSERT_DEBUG_SYNC((n_tap_weight_center + 1) == n_tap_weights,
                          "Sanity check failed");

        for (int n_tap_weight  = n_tap_weight_center;
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

            n_tap_dataset_coeffs++;
        } /* for (all tap weights) */

        if ((n_taps % 2) == 0)
        {
            ASSERT_DEBUG_SYNC(fabs(2.0f * sanity_check_sum - 1.0f) < 1e-5f,
                              "Sanity check failed");
        }
        else
        {
            float center_item                     = float(binomial_values[2 + n_tap_weight_center]) / float(binomial_sum);
            float sanity_check_sum_wo_center_item = sanity_check_sum - center_item;

            ASSERT_DEBUG_SYNC(fabs(2.0f * sanity_check_sum_wo_center_item + center_item - 1.0f) < 1e-5f,
                              "Sanity check failed");
        }

        /* Calculate the tap offsets */
        for (unsigned int n_tap_offset = 0;
                          n_tap_offset < n_tap_weights;
                        ++n_tap_offset)
        {
            /* Two cases here:
             *
             * For an odd number of taps (after +3 is added to the even input number) we get the following binomial set:
             *
             *            A B C D E D C B A
             *
             * which is fine, because we can set offset values to a set of consecutive
             * texel offsets (in px space)
             *
             * For an even number of taps (after +3 is added to the odd input number), things get a dab whack:
             *
             *             A B C C B A
             *
             * In this case, we only sample C once (and give it a doubled weight, and
             * need to ensure that the offsets are properly adjusted, so that weights
             * reflect the fact the samples are between input texels.
             *
             */
            const float offset = ((n_taps % 2) != 0) ?  float(n_tap_offset)
                                                     :  (float(n_tap_offset) + float(n_tap_offset + 1.0f) / float(n_tap_weights + 1.0f));

            system_resizable_vector_push(coeff_vector,
                                         *((void**) &offset));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));
            system_resizable_vector_push(coeff_vector,
                                         *((void**) &padding));

            n_tap_dataset_coeffs++;
        } /* for (all tap offsets) */

        /* Update the n_max_data_coeffs variable. */
        if (n_tap_dataset_coeffs > n_max_data_coeffs)
        {
            n_max_data_coeffs = n_tap_dataset_coeffs;
        }
    }

    /* n_max_data_coeffs tells how much data we need bound for the whole run. Store it,
     * this will be used later on when building the shader code.
     */
    instance_ptr->n_max_data_coeffs = n_max_data_coeffs;

    /* We next run two iterations of the same loop.
     *
     * In the first run, we calculate the final data buffer size.
     *
     * In the second run, we allocate an actual buffer and copy the memory blocks
     * from the coeff data buffer we've created earlier.
     */
    const char*  coeff_vector_data_raw_ptr       = NULL;
    char*        final_data_bo_raw_ptr           = NULL;
    char*        final_data_bo_raw_traveller_ptr = NULL;
    unsigned int final_data_bo_size              = 0;

    system_resizable_vector_get_property(coeff_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                         &coeff_vector_data_raw_ptr);

    instance_ptr->coeff_buffer_offsets = new (std::nothrow) unsigned int [n_tap_datasets];

    ASSERT_DEBUG_SYNC(instance_ptr->coeff_buffer_offsets != NULL,
                      "Out of memory");

    for (unsigned int n_iteration = 0;
                      n_iteration < 2;
                    ++n_iteration)
    {
        /* Allocate the buffer if this is the second run */
        if (n_iteration == 1)
        {
            ASSERT_DEBUG_SYNC(final_data_bo_size != 0,
                              "No data BO size defined for the second loop iteration");

            final_data_bo_raw_ptr           = new (std::nothrow) char[final_data_bo_size];
            final_data_bo_raw_traveller_ptr = final_data_bo_raw_ptr;

            ASSERT_DEBUG_SYNC(final_data_bo_raw_ptr != NULL,
                              "Out of memory");
        } /* if (n_iteration == 1) */

        /* Go ahead with the tap datasets.
         *
         * A special iteration with index -1 is used to store a single value of 1.0, used
         * for the optional blending phase.
         */
        for (int n_tap_dataset = -1;
                 n_tap_dataset < (int) n_tap_datasets;
               ++n_tap_dataset)
        {
            unsigned int aligned_dataset_size    = 0;
            unsigned int misaligned_dataset_size = 0;
            unsigned int padding                 = 0;

            if (n_tap_dataset == -1)
            {
                misaligned_dataset_size = sizeof(float);
            }
            else
            if (n_tap_dataset != (n_tap_datasets - 1))
            {
                misaligned_dataset_size = misaligned_coeff_buffer_offsets[n_tap_dataset + 1] -
                                          misaligned_coeff_buffer_offsets[n_tap_dataset];
            } /* if (n_tap_dataset == (n_tap_datasets - 1)) */
            else
            {
                uint32_t n_coeff_vector_items = 0;

                system_resizable_vector_get_property(coeff_vector,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_coeff_vector_items);

                misaligned_dataset_size = n_coeff_vector_items                           * sizeof(float) -
                                          misaligned_coeff_buffer_offsets[n_tap_dataset];
            }

            /* Calculated padding we need to insert */
            padding = (limits_ptr->uniform_buffer_offset_alignment - (misaligned_dataset_size % limits_ptr->uniform_buffer_offset_alignment) );

            if (n_iteration == 0)
            {
                final_data_bo_size += misaligned_dataset_size + padding;
            }
            else
            {
                /* Copy the data sub-buffers to our final BO */
                unsigned int dataset_size = 0;

                if (n_tap_dataset != -1)
                {
                    memcpy(final_data_bo_raw_traveller_ptr,
                           coeff_vector_data_raw_ptr + misaligned_coeff_buffer_offsets[n_tap_dataset],
                           misaligned_dataset_size);

                    instance_ptr->coeff_buffer_offsets[n_tap_dataset]  = final_data_bo_raw_traveller_ptr - final_data_bo_raw_ptr;
                }
                else
                {
                    *(float*) final_data_bo_raw_traveller_ptr = 1.0f;
                }

                final_data_bo_raw_traveller_ptr += misaligned_dataset_size + padding;
            }
        } /* for (all tap datasets) */
    } /* for (both iterations) */

    /* Set up the BOs.
     *
     * NOTE: The other data BO must not come from sparse buffers under NV drivers. Please see GitHub#61
     *       for more details. May be worth revisiting in the future.
     */
    bool is_nv_driver = false;

    ogl_context_get_property(instance_ptr->context,
                             OGL_CONTEXT_PROPERTY_IS_NV_DRIVER,
                            &is_nv_driver);

    ogl_buffers_allocate_buffer_memory(instance_ptr->buffers,
                                       final_data_bo_size,
                                       limits_ptr->uniform_buffer_offset_alignment, /* alignment_requirement */
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_UBO,
                                       OGL_BUFFERS_FLAGS_NONE,
                                      &instance_ptr->coeff_bo_id,
                                      &instance_ptr->coeff_bo_start_offset);

    ogl_buffers_allocate_buffer_memory(instance_ptr->buffers,
                                       sizeof(int) * 3,                             /* other data = three integers */
                                       limits_ptr->uniform_buffer_offset_alignment, /* alignment_requirement */
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_UBO,
                                       (is_nv_driver) ? OGL_BUFFERS_FLAGS_IMMUTABLE_BUFFER_MEMORY_BIT :
                                                        OGL_BUFFERS_FLAGS_NONE,
                                      &instance_ptr->other_data_bo_id,
                                      &instance_ptr->other_data_bo_start_offset);

    dsa_entrypoints_ptr->pGLNamedBufferSubDataEXT(instance_ptr->coeff_bo_id,
                                                  instance_ptr->coeff_bo_start_offset,
                                                  final_data_bo_size,
                                                  final_data_bo_raw_ptr); /* flags */

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
                                   RAL_SHADER_TYPE_FRAGMENT,
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
                                   RAL_SHADER_TYPE_VERTEX,
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

    /* Retrieve the sampler object we will use to perform the blur operation */
    ogl_context_samplers context_samplers        = NULL;
    const GLenum         filter_gl_clamp_to_edge = GL_CLAMP_TO_EDGE;
    const GLenum         filter_gl_linear        = GL_LINEAR;

    ogl_context_get_property(instance_ptr->context,
                             OGL_CONTEXT_PROPERTY_SAMPLERS,
                            &context_samplers);

    instance_ptr->sampler = ogl_context_samplers_get_sampler(context_samplers,
                                                             NULL,                    /* border_color */
                                                             &filter_gl_linear,       /* mag_filter_ptr */
                                                             NULL,                    /* max_lod_ptr*/
                                                             &filter_gl_linear,       /* min_filter_ptr */
                                                             NULL,                    /* min_lod_ptr */
                                                             NULL,                    /* texture_compare_func_ptr */
                                                             NULL,                    /* texture_compare_mode_ptr */
                                                             NULL,                    /* wrap_r_ptr */
                                                            &filter_gl_clamp_to_edge, /* wrap_s_ptr */
                                                            &filter_gl_clamp_to_edge);/* wrap_t_ptr */

    ASSERT_DEBUG_SYNC(instance_ptr->sampler != NULL,
                      "Could not retrieve a sampler object from ogl_samplers");

    /* Reserve FBO ids */
    entrypoints_ptr->pGLGenFramebuffers(sizeof(instance_ptr->fbo_ids) / sizeof(instance_ptr->fbo_ids[0]),
                                        instance_ptr->fbo_ids);

    /* Clean up */
    if (binomial_values != NULL)
    {
        delete [] binomial_values;

        binomial_values = NULL;
    }

    if (coeff_vector != NULL)
    {
        system_resizable_vector_release(coeff_vector);

        coeff_vector = NULL;
    }

    if (factorial_values != NULL)
    {
        delete [] factorial_values;

        factorial_values = NULL;
    }

    if (final_data_bo_raw_ptr != NULL)
    {
        delete [] final_data_bo_raw_ptr;

        final_data_bo_raw_ptr = NULL;
    }

    if (misaligned_coeff_buffer_offsets != NULL)
    {
        delete [] misaligned_coeff_buffer_offsets;

        misaligned_coeff_buffer_offsets = NULL;
    }

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
PRIVATE void _postprocessing_blur_gaussian_release(void* ptr)
{
    _postprocessing_blur_gaussian* data_ptr = (_postprocessing_blur_gaussian*) ptr;

    ogl_context_request_callback_from_context_thread(data_ptr->context,
                                                     _postprocessing_blur_gaussian_deinit_rendering_thread_callback,
                                                     data_ptr);
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _postprocessing_blur_gaussian_update_other_data_bo_storage(_postprocessing_blur_gaussian* gaussian_ptr,
                                                                                               unsigned int                   new_other_data_cached_value)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa = NULL;

    const int data[] =
    {
        0,
        new_other_data_cached_value,
        1                            /* predefined value */
    };

    ogl_context_get_property(gaussian_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa);

    entrypoints_dsa->pGLNamedBufferSubDataEXT(gaussian_ptr->other_data_bo_id,
                                              gaussian_ptr->other_data_bo_start_offset,
                                              sizeof(data),
                                              data);

    gaussian_ptr->other_data_bo_cached_value = new_other_data_cached_value;
}


/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API postprocessing_blur_gaussian postprocessing_blur_gaussian_create(ogl_context               context,
                                                                                                           system_hashed_ansi_string name,
                                                                                                           unsigned int              n_min_taps,
                                                                                                           unsigned int              n_max_taps)
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
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void postprocessing_blur_gaussian_execute(postprocessing_blur_gaussian            blur,
                                                                                    unsigned int                            n_taps,
                                                                                    float                                   n_iterations,
                                                                                    ogl_texture                             src_texture,
                                                                                    postprocessing_blur_gaussian_resolution blur_resolution)
{
    _postprocessing_blur_gaussian*                            blur_ptr                      = (_postprocessing_blur_gaussian*) blur;
    GLuint                                                    context_default_fbo_id        = -1;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr           = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr               = NULL;
    ogl_texture_type                                          src_texture_type              = OGL_TEXTURE_TYPE_UNKNOWN;
    unsigned int                                              src_texture_height            = 0;
    GLenum                                                    src_texture_internalformat    = GL_NONE;
    unsigned int                                              src_texture_width             = 0;
    ogl_context_state_cache                                   state_cache                   = NULL;
    ogl_sampler                                               temp_2d_array_texture_sampler = NULL;
    ogl_texture                                               temp_2d_array_texture         = NULL;
    GLuint                                                    vao_id                        = 0;
    GLint                                                     viewport_data[4]              = {0};

    ASSERT_DEBUG_SYNC(n_taps >= blur_ptr->n_min_taps &&
                      n_taps <= blur_ptr->n_max_taps,
                      "Invalid number of taps requested");

    ogl_context_get_property            (blur_ptr->context,
                                         OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                                        &context_default_fbo_id);
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
                                         OGL_TEXTURE_PROPERTY_TYPE,
                                        &src_texture_type);
    ogl_texture_get_property            (src_texture,
                                         OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                         &src_texture_internalformat);
    ogl_texture_get_mipmap_property     (src_texture,
                                         0, /* mipmap_level */
                                         OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                         &src_texture_width);

    /* The implementation below may look a bit wooly. Below is the break-down of the
     * whole process:
     *
     * 1) Copy data from the source texture to ping (0th) texture layer.
     *    Downsample if requested.
     *
     *    Draw (ping) FBO is attached the ping texture layer.
     *    Read (pong) FBO is attached the source texture.
     *
     * 2a) For each iteration:
     *     a) Horizontal pass:
     *        X Draw (pong) FBO is attached the pong (1st) texture layer.
     *        X Texture layer 0 is read.
     *
     *     b) Vertical pass:
     *        X Draw (ping) FBO is attached the ping (0th) texture layer.
     *        X Texture layer 1 is read.
     *
     * 2b) If we need an extra iteration of step 2a (which happens if frac(n_iterations) != 0), re-configure
     *     ping/pong FBOs and execute extra run of step 2a:
     *
     *     a) Horizontal pass:
     *        X Draw (pong) FBO is attached the 2nd texture layer.
     *        X Texture layer 0 is read.
     *
     *     b) Vertical pass:
     *        X Draw (ping) FBO is attached the 1st texture layer.
     *        X Texture layer 2 is read.
     *
     *     As a result:
     *
     *     a) 0th texture layer holds texture blurred n_iterations   n times.
     *     b) 1st texture layer holds texture blurred n_iterations+1 n times.
     *
     *     ..which we will lerp between in optional step 4!
     *
     * 3) If frac(n_iterations) != 0, we use the same program to draw (1st) texture layer
     *    over the contents of the zeroth texture layer, with blending enabled & configured
     *    so that the operation acts as a lerp between the (n_iterations) and (n_iterations+1)
     *    texture layers, with the weight controlled by the fractional part.
     *    This saves us a PO switch.
     *
     * 4) Copy data from the ping (0th) texture layer to the source texture.
     *    Upsample if requested.
     *
     *    Draw (pong) FBO is attached the source texture.
     *    Read (ping) FBO is attached the ping (0th) texture layer.
     */
    bool         is_culling_enabled = false;
    const GLuint ping_fbo_id        = blur_ptr->fbo_ids[0];
    const GLuint pong_fbo_id        = blur_ptr->fbo_ids[1];

    ogl_context_state_cache_get_property(state_cache,
                                         OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE,
                                        &is_culling_enabled);

    entrypoints_ptr->pGLColorMask(GL_TRUE,
                                  GL_TRUE,
                                  GL_TRUE,
                                  GL_TRUE);
    entrypoints_ptr->pGLDisable  (GL_BLEND);
    entrypoints_ptr->pGLDisable  (GL_CULL_FACE);

    /* Step 1) */
    unsigned int target_height;
    GLenum       target_interpolation;
    unsigned int target_width;

    switch (blur_resolution)
    {
        case POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_ORIGINAL:
        {
            target_height        = src_texture_height;
            target_interpolation = GL_NEAREST; /* no interpolation needed */
            target_width         = src_texture_width;

            break;
        }

        case POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_HALF:
        {
            target_height        = src_texture_height / 2;
            target_interpolation = GL_LINEAR;
            target_width         = src_texture_width  / 2;

            break;
        }

        case POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_QUARTER:
        {
            target_height        = src_texture_height / 4;
            target_interpolation = GL_LINEAR;
            target_width         = src_texture_width  / 4;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized blur resolution requested");
        }
    } /* switch (blur_resolution) */

    /* Retrieve a 2D Array texture we will use for execution of the iterations */
    temp_2d_array_texture = ogl_context_textures_get_texture_from_pool(blur_ptr->context,
                                                                       OGL_TEXTURE_TYPE_GL_TEXTURE_2D_ARRAY,
                                                                       1, /* n_mipmaps */
                                                                       src_texture_internalformat,
                                                                       target_width,
                                                                       target_height,
                                                                       3,      /* base_mipmap_depth */
                                                                       1,      /* n_samples */
                                                                       false); /* fixed_sample_locations */

    /* Step 2): Set-up
     *
     * NOTE: For the time being, we need to handle two separate texture types:
     *
     * X 2D Textures
     * X Cube-map Textures
     *
     * ..possibly more in the future..
     *
     * For 2D textures, things are straightforward.
     *
     * For CM textures, we need to perform the same task for each cube-map face.
     * We could possibly improve performance by using multilayered rendering and
     * reworking the fragment shader, but.. This is left as homework. :D No,
     * seriously, this would require a major revamp of the implementation and
     * there are more interesting things to be looking at ATM!
     *
     */
    const GLuint po_id = ogl_program_get_id(blur_ptr->po);

    entrypoints_ptr->pGLBindVertexArray(vao_id);
    entrypoints_ptr->pGLUseProgram     (po_id);
    entrypoints_ptr->pGLViewport       (0, /* x */
                                        0, /* y */
                                        target_width,
                                        target_height);

    dsa_entrypoints_ptr->pGLBindMultiTextureEXT(GL_TEXTURE0 + DATA_SAMPLER_TEXTURE_UNIT_INDEX,
                                                GL_TEXTURE_2D_ARRAY,
                                                temp_2d_array_texture);
    entrypoints_ptr->pGLBindSampler            (DATA_SAMPLER_TEXTURE_UNIT_INDEX,
                                                ogl_sampler_get_id(blur_ptr->sampler) );

    /* Make sure the "other data BO" iteration-specific value is set to the number of taps the caller
     * is requesting */
    if (blur_ptr->other_data_bo_cached_value != n_taps)
    {
        _postprocessing_blur_gaussian_update_other_data_bo_storage(blur_ptr,
                                                                   n_taps);

        ASSERT_DEBUG_SYNC(blur_ptr->other_data_bo_cached_value == n_taps,
                          "Sanity check failed.");
    }

    /* Iterate over all layers we need blurred */
    ASSERT_DEBUG_SYNC(src_texture_type == OGL_TEXTURE_TYPE_GL_TEXTURE_2D       ||
                      src_texture_type == OGL_TEXTURE_TYPE_GL_TEXTURE_2D_ARRAY ||
                      src_texture_type == OGL_TEXTURE_TYPE_GL_TEXTURE_CUBE_MAP,
                      "Unsupported source texture type");

    unsigned int n_layers = 0;

    switch (src_texture_type)
    {
        case OGL_TEXTURE_TYPE_GL_TEXTURE_2D:
        {
            n_layers = 1;

            break;
        }

        case OGL_TEXTURE_TYPE_GL_TEXTURE_2D_ARRAY:
        {
            ogl_texture_get_mipmap_property(src_texture,
                                            0, /* mipmap_level */
                                            OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                           &n_layers);

            ASSERT_DEBUG_SYNC(n_layers != 0,
                              "Could not retrieve the number of layers of the input 2D Array Texture.");

            break;
        }

        case OGL_TEXTURE_TYPE_GL_TEXTURE_CUBE_MAP:
        {
            n_layers = 6;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported input texture type.");
        }
    } /* switch (src_texture_type) */

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        OTHER_DATA_UB_BP,
                                        blur_ptr->other_data_bo_id,
                                        blur_ptr->other_data_bo_start_offset,
                                        sizeof(int) * 3);

    for (unsigned int n_layer = 0;
                      n_layer < n_layers;
                    ++n_layer)
    {
        /* Use the iteration-specific number of taps */
        dsa_entrypoints_ptr->pGLNamedCopyBufferSubDataEXT(blur_ptr->other_data_bo_id,                             /* readBuffer */
                                                          blur_ptr->other_data_bo_id,                             /* writeBuffer */
                                                          blur_ptr->other_data_bo_start_offset + 1 * sizeof(int), /* readOffset */
                                                          blur_ptr->other_data_bo_start_offset,                   /* writeOffset */
                                                          sizeof(int) );

        entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                            COEFFS_DATA_UB_BP,
                                            blur_ptr->coeff_bo_id,
                                            blur_ptr->coeff_bo_start_offset + blur_ptr->coeff_buffer_offsets[n_taps - blur_ptr->n_min_taps],
                                            blur_ptr->n_max_data_coeffs * 4 /* padding */ * sizeof(float) );

        /* Copy the layer to blur */
        entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                            ping_fbo_id);
        entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                            pong_fbo_id);

        dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(ping_fbo_id,
                                                                GL_COLOR_ATTACHMENT0,
                                                                temp_2d_array_texture,
                                                                0,  /* level */
                                                                0); /* layer */

        if (src_texture_type == OGL_TEXTURE_TYPE_GL_TEXTURE_2D)
        {
            dsa_entrypoints_ptr->pGLNamedFramebufferTexture2DEXT(pong_fbo_id,
                                                                 GL_COLOR_ATTACHMENT0,
                                                                 GL_TEXTURE_2D,
                                                                 src_texture,
                                                                 0); /* level */
        }
        else
        {
            dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(pong_fbo_id,
                                                                    GL_COLOR_ATTACHMENT0,
                                                                    src_texture,
                                                                    0, /* level */
                                                                    n_layer);
        }

        entrypoints_ptr->pGLBlitFramebuffer(0, /* srcX0 */
                                            0, /* srcY0 */
                                            src_texture_width,
                                            src_texture_height,
                                            0, /* dstX0 */
                                            0, /* dstY0 */
                                            target_width,
                                            target_height,
                                            GL_COLOR_BUFFER_BIT,
                                            target_interpolation);

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

        /* Step 2a): Kick off. Result blurred texture is stored under layer 0 */
        float n_iterations_frac = fmod(n_iterations, 1.0f);

        for (unsigned int n_iteration = 0;
                          n_iteration < (unsigned int) floor(n_iterations);
                        ++n_iteration)
        {
            /* Horizontal pass */
            entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                pong_fbo_id);

            entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                           0,  /* first - 0 will cause FS to read from layer 0 */
                                           4); /* count */

            /* Vertical pass */
            entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                ping_fbo_id);

            entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                           4,  /* first - 4 will cause FS to read from layer 1 */
                                           4); /* count */
        } /* for (all iterations) */

        /* Step 2b) Run the extra iteration if frac(n_iterations) != 0 */
        if (n_iterations_frac > 1e-5f)
        {
            /* Draw the src texture, blurred (n_iterations + 1) times, into texture layer 1 */

            /* Horizontal pass */
            entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                pong_fbo_id);

            dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(pong_fbo_id,
                                                                    GL_COLOR_ATTACHMENT0,
                                                                    temp_2d_array_texture,
                                                                    0,  /* level */
                                                                    2); /* layer */

            entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                           0,  /* first - 0 will cause FS to read from layer 0 */
                                           4); /* count */

            /* Vertical pass */
            dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(pong_fbo_id,
                                                                    GL_COLOR_ATTACHMENT0,
                                                                    temp_2d_array_texture,
                                                                    0,  /* level */
                                                                    1); /* layer */

            entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                           8,  /* first - 8 will cause FS to read from layer 2 */
                                           4); /* count */

            /* Step 3): Blend the (n_iterations + 1) texture layer over the (n_iterations) texture layer*/
            entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                ping_fbo_id);
            entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                context_default_fbo_id);

            entrypoints_ptr->pGLEnable       (GL_BLEND);
            entrypoints_ptr->pGLBlendColor   (0.0f, /* red */
                                              0.0f, /* green */
                                              0.0f, /* blue */
                                              n_iterations_frac);
            entrypoints_ptr->pGLBlendEquation(GL_FUNC_ADD);
            entrypoints_ptr->pGLBlendFunc    (GL_CONSTANT_ALPHA,
                                              GL_ONE_MINUS_CONSTANT_ALPHA);

            entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                                COEFFS_DATA_UB_BP,
                                                blur_ptr->coeff_bo_id,
                                                blur_ptr->coeff_bo_start_offset + blur_ptr->coeff_buffer_offset_for_value_1,
                                                blur_ptr->n_max_data_coeffs * 4 /* padding */ * sizeof(float) );

            /* Set the number of taps to 1 */
            dsa_entrypoints_ptr->pGLNamedCopyBufferSubDataEXT(blur_ptr->other_data_bo_id,                             /* readBuffer  */
                                                              blur_ptr->other_data_bo_id,                             /* writeBuffer */
                                                              blur_ptr->other_data_bo_start_offset + 2 * sizeof(int), /* readOffset  */
                                                              blur_ptr->other_data_bo_start_offset,                   /* writeOffset */
                                                              sizeof(int) );

            /* Draw data from texture layer 1 */
            entrypoints_ptr->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                           4,  /* first */
                                           4); /* count */

            entrypoints_ptr->pGLDisable(GL_BLEND);
        } /* if (n_iterations_frac > 1e-5f) */

        /* Step 4): Store the result in the user-specified texture */
        entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                            pong_fbo_id);
        entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                            ping_fbo_id);

        if (src_texture_type == OGL_TEXTURE_TYPE_GL_TEXTURE_2D)
        {
            dsa_entrypoints_ptr->pGLNamedFramebufferTexture2DEXT(pong_fbo_id,
                                                                 GL_COLOR_ATTACHMENT0,
                                                                 GL_TEXTURE_2D,
                                                                 src_texture,
                                                                 0); /* level */
        }
        else
        {
            dsa_entrypoints_ptr->pGLNamedFramebufferTextureLayerEXT(pong_fbo_id,
                                                                    GL_COLOR_ATTACHMENT0,
                                                                    src_texture,
                                                                    0, /* level */
                                                                    n_layer);
        }

        entrypoints_ptr->pGLBlitFramebuffer(0,                  /* srcX0 */
                                            0,                  /* srcY0 */
                                            target_width,
                                            target_height,
                                            0,                  /* dstX0 */
                                            0,                  /* dstY0 */
                                            src_texture_width,
                                            src_texture_height,
                                            GL_COLOR_BUFFER_BIT,
                                            target_interpolation);
    } /* for (all layers that need to be blurred) */

    /* All done */
    GLuint temp_2d_array_texture_id = 0;

    ogl_texture_get_property(temp_2d_array_texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &temp_2d_array_texture_id);

    entrypoints_ptr->pGLInvalidateTexImage(temp_2d_array_texture_id,
                                           0); /* level */

    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        context_default_fbo_id);
    entrypoints_ptr->pGLBindSampler    (DATA_SAMPLER_TEXTURE_UNIT_INDEX,
                                        0);
    entrypoints_ptr->pGLViewport       (viewport_data[0],
                                        viewport_data[1],
                                        viewport_data[2],
                                        viewport_data[3]);

    if (is_culling_enabled)
    {
        entrypoints_ptr->pGLEnable(GL_CULL_FACE);
    }

    ogl_context_textures_return_reusable(blur_ptr->context,
                                         temp_2d_array_texture);
}
