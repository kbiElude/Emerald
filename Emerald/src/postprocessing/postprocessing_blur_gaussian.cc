/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
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

static const char* fs_preamble =
    "#version 430 core\n"
    "\n";
static const char* fs_body =
    "layout(binding = 0, std140) uniform coeffs_data\n"
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

static const char* vs_body =
    "#version 430 core\n"
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
    ral_buffer                coeff_bo;
    unsigned int              coeff_buffer_offset_for_value_1; /* holds offset to where 1.0 is stored in BO with id coeff_bo_id */
    unsigned int*             coeff_buffer_offsets; // [0] for n_min_taps, [1] for n_min_taps+1, etc..
    ral_context               context;    /* DO NOT retain */
    system_hashed_ansi_string name;
    unsigned int              n_max_data_coeffs;
    unsigned int              n_max_taps;
    unsigned int              n_min_taps;
    ral_program               po;

    ral_command_buffer                      cached_command_buffer;
    ral_present_task                        cached_present_task;
    postprocessing_blur_gaussian_resolution cached_present_task_blur_resolution;
    ral_texture_view                        cached_present_task_dst_src_texture_view;
    float                                   cached_present_task_n_iterations;
    unsigned int                            cached_present_task_n_taps;
    ral_gfx_state                           gfx_state;
    ral_texture                             ping_pong_rt;
    ral_texture_view                        ping_pong_rt_view_l0;
    ral_texture_view                        ping_pong_rt_view_l012;
    ral_texture_view                        ping_pong_rt_view_l1;
    ral_texture_view                        ping_pong_rt_view_l2;
    ral_sampler                             sampler_blur_linear;
    ral_sampler                             sampler_blur_nearest;

    /* other data BO holds:
     *
     * [0] iteration-specific n_taps value.
     * [1] 1
     *
     * The [0] data is only uploaded when it changes. when performing the actual rendering process, we do GPU<->GPU
     * memory copies to avoid pipeline stalls.
     */
    unsigned int other_data_bo_cached_value;
    ral_buffer   other_data_bo;           /* do NOT release */

    _postprocessing_blur_gaussian(ral_context               in_context,
                                  unsigned int              in_n_min_taps,
                                  unsigned int              in_n_max_taps,
                                  system_hashed_ansi_string in_name)
    {
        ASSERT_DEBUG_SYNC(in_n_min_taps <= in_n_max_taps,
                          "Invalid min/max tap argument values");

        cached_command_buffer           = nullptr;
        cached_present_task             = nullptr;
        coeff_bo                        = nullptr;
        coeff_buffer_offset_for_value_1 = 0; /* always zero */
        coeff_buffer_offsets            = nullptr;
        context                         = in_context;
        gfx_state                       = nullptr;
        n_max_data_coeffs               = 0;
        n_max_taps                      = in_n_max_taps;
        n_min_taps                      = in_n_min_taps;
        name                            = in_name;
        other_data_bo_cached_value      = 0;
        other_data_bo                   = nullptr;
        ping_pong_rt                    = nullptr;
        ping_pong_rt_view_l0            = nullptr;
        ping_pong_rt_view_l012          = nullptr;
        ping_pong_rt_view_l1            = nullptr;
        ping_pong_rt_view_l2            = nullptr;
        po                              = nullptr;
        sampler_blur_linear             = nullptr;
        sampler_blur_nearest            = nullptr;
    }

    ~_postprocessing_blur_gaussian()
    {
        const ral_buffer buffers_to_release[] =
        {
            coeff_bo,
            other_data_bo
        };
        const ral_gfx_state gfx_states_to_release[] =
        {
            gfx_state
        };
        const ral_texture_view texture_views_to_release[] =
        {
            ping_pong_rt_view_l0,
            ping_pong_rt_view_l012,
            ping_pong_rt_view_l1,
            ping_pong_rt_view_l2,
        };
        const ral_sampler samplers_to_release[] =
        {
            sampler_blur_linear,
            sampler_blur_nearest
        };

        const uint32_t n_buffers_to_release       = sizeof(buffers_to_release)       / sizeof(buffers_to_release      [0]);
        const uint32_t n_gfx_states_to_release    = sizeof(gfx_states_to_release)    / sizeof(gfx_states_to_release   [0]);
        const uint32_t n_samplers_to_release      = sizeof(samplers_to_release)      / sizeof(samplers_to_release     [0]);
        const uint32_t n_texture_views_to_release = sizeof(texture_views_to_release) / sizeof(texture_views_to_release[0]);

        if (cached_command_buffer != nullptr)
        {
            ral_command_buffer_release(cached_command_buffer);

            cached_command_buffer = nullptr;
        }

        if (cached_present_task != nullptr)
        {
            ral_present_task_release(cached_present_task);

            cached_present_task = nullptr;
        }

        if (coeff_buffer_offsets != nullptr)
        {
            delete [] coeff_buffer_offsets;

            coeff_buffer_offsets = nullptr;
        }

        if (ping_pong_rt != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&ping_pong_rt) );
        }

        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                   n_texture_views_to_release,
                                   reinterpret_cast<void* const*>(texture_views_to_release) );

        if (po != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&po) );

            po = nullptr;
        }

        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   n_buffers_to_release,
                                   reinterpret_cast<void* const*>(buffers_to_release) );
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   n_gfx_states_to_release,
                                   reinterpret_cast<void* const*>(gfx_states_to_release) );
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                   n_samplers_to_release,
                                   reinterpret_cast<void* const*>(samplers_to_release) );
    }

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_blur_gaussian;


/** Internal variables */

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_blur_gaussian,
                               postprocessing_blur_gaussian,
                              _postprocessing_blur_gaussian);


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
PRIVATE void _postprocessing_blur_gaussian_init(_postprocessing_blur_gaussian* instance_ptr)
{
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
    uint32_t           required_sb_alignment = 0;
    uint32_t           required_ub_alignment = 0;

    ASSERT_DEBUG_SYNC(max_binomial_n < 20,
                      "Insufficient precision for requested number of factorial values");

    ral_context_get_property(instance_ptr->context,
                             RAL_CONTEXT_PROPERTY_STORAGE_BUFFER_ALIGNMENT,
                            &required_sb_alignment);
    ral_context_get_property(instance_ptr->context,
                             RAL_CONTEXT_PROPERTY_UNIFORM_BUFFER_ALIGNMENT,
                            &required_ub_alignment);

    __uint64* binomial_values  = new (std::nothrow) __uint64[max_n_binomial_values];
    __uint64* factorial_values = new (std::nothrow) __uint64[max_n_binomial_values];

    ASSERT_DEBUG_SYNC(binomial_values  != nullptr &&
                      factorial_values != nullptr,
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
    }

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

    ASSERT_DEBUG_SYNC(misaligned_coeff_buffer_offsets != nullptr,
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
        }

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
        }

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
        }

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
    const char*  coeff_vector_data_raw_ptr       = nullptr;
    char*        final_data_bo_raw_ptr           = nullptr;
    char*        final_data_bo_raw_traveller_ptr = nullptr;
    unsigned int final_data_bo_size              = 0;

    system_resizable_vector_get_property(coeff_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                         &coeff_vector_data_raw_ptr);

    instance_ptr->coeff_buffer_offsets = new (std::nothrow) unsigned int [n_tap_datasets];

    ASSERT_DEBUG_SYNC(instance_ptr->coeff_buffer_offsets != nullptr,
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

            ASSERT_DEBUG_SYNC(final_data_bo_raw_ptr != nullptr,
                              "Out of memory");
        }

        /* Go ahead with tap datasets.
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
            }
            else
            {
                uint32_t n_coeff_vector_items = 0;

                system_resizable_vector_get_property(coeff_vector,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_coeff_vector_items);

                misaligned_dataset_size = n_coeff_vector_items                           * sizeof(float) -
                                          misaligned_coeff_buffer_offsets[n_tap_dataset];
            }

            /* Calculate padding we need to insert */
            padding = (required_ub_alignment - (misaligned_dataset_size % required_ub_alignment) );

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
                    *reinterpret_cast<float*>(final_data_bo_raw_traveller_ptr) = 1.0f;
                }

                final_data_bo_raw_traveller_ptr += misaligned_dataset_size + padding;
            }
        }
    }

    /* Set up the BOs.
     *
     * NOTE: The other data BO must not come from sparse buffers under NV drivers. Please see GitHub#61
     *       for more details. May be worth revisiting in the future.
     */
    ral_buffer_create_info                                                coeff_bo_props;
    ral_buffer_client_sourced_update_info                                 coeff_bo_update;
    std::vector< std::shared_ptr<ral_buffer_client_sourced_update_info> > coeff_bo_update_ptrs;
    bool                                                                  is_nv_driver          = false;
    ral_buffer_create_info                                                other_data_bo_props;

    ral_context_get_property(instance_ptr->context,
                             RAL_CONTEXT_PROPERTY_IS_NV_DRIVER,
                            &is_nv_driver);

    coeff_bo_props.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    coeff_bo_props.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
    coeff_bo_props.size             = final_data_bo_size;
    coeff_bo_props.usage_bits       = RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    coeff_bo_props.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(instance_ptr->context,
                               1, /* n_buffers */
                               &coeff_bo_props,
                               &instance_ptr->coeff_bo);

    other_data_bo_props.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    other_data_bo_props.property_bits    = (is_nv_driver) ? 0 :
                                                            RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
    other_data_bo_props.size             = sizeof(int) * 3,                             /* other data = three integers */
    other_data_bo_props.usage_bits       = RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    other_data_bo_props.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(instance_ptr->context,
                               1, /* n_buffers */
                              &other_data_bo_props,
                              &instance_ptr->other_data_bo);

    coeff_bo_update.data         = final_data_bo_raw_ptr;
    coeff_bo_update.data_size    = final_data_bo_size;
    coeff_bo_update.start_offset = 0;

    coeff_bo_update_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&coeff_bo_update,
                                                                                          NullDeleter<ral_buffer_client_sourced_update_info>() ) );

    ral_buffer_set_data_from_client_memory(instance_ptr->coeff_bo,
                                           coeff_bo_update_ptrs,
                                           false, /* async */
                                           true   /* sync_other_contexts */);

    /* Set up the PO */
    ral_shader result_shaders[2] =
    {
        nullptr,
        nullptr
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        _postprocessing_blur_gaussian_get_po_name(n_max_data_coeffs)
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        {
            _postprocessing_blur_gaussian_get_fs_name(n_max_data_coeffs),
            RAL_SHADER_TYPE_FRAGMENT
        },
        {
            system_hashed_ansi_string_create("Postprocessing blur Gaussian VS"),
            RAL_SHADER_TYPE_VERTEX
        }
    };

    if ( (instance_ptr->po = ral_context_get_program_by_name(instance_ptr->context,
                                                             program_create_info.name)) == nullptr)
    {
        if (!ral_context_create_programs(instance_ptr->context,
                                         1, /* n_create_info_items */
                                        &program_create_info,
                                        &instance_ptr->po) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL program creation failed");
        }

        if ( (result_shaders[0] = ral_context_get_shader_by_name(instance_ptr->context,
                                                                 shader_create_info_items[0].name) ) == nullptr)
        {
            /* Create the FS body */
            std::stringstream         fs_body_sstream;
            system_hashed_ansi_string fs_body_has;

            fs_body_sstream << fs_preamble
                            << "#define N_MAX_COEFFS (" << n_max_data_coeffs << ")\n"
                            << fs_body;

            /* Create the fragment shader */
            ral_context_create_shaders(instance_ptr->context,
                                       1, /* n_create_info_items */
                                       shader_create_info_items + 0,
                                       result_shaders + 0);

            fs_body_has = system_hashed_ansi_string_create(fs_body_sstream.str().c_str() );

            ral_shader_set_property(result_shaders[0],
                                    RAL_SHADER_PROPERTY_GLSL_BODY,
                                   &fs_body_has);
        }

        if ( (result_shaders[1] = ral_context_get_shader_by_name(instance_ptr->context,
                                                                 shader_create_info_items[1].name) ) == nullptr)
        {
            const system_hashed_ansi_string vs_body_has = system_hashed_ansi_string_create(vs_body);

            /* Create the vertex shader */
            ral_context_create_shaders(instance_ptr->context,
                                       1, /* n_create_info_items */
                                       shader_create_info_items + 1,
                                       result_shaders + 1);

            ral_shader_set_property(result_shaders[1],
                                    RAL_SHADER_PROPERTY_GLSL_BODY,
                                   &vs_body_has);
        }

        if (!ral_program_attach_shader(instance_ptr->po,
                                       result_shaders[0],
                                       true /* async */) ||
            !ral_program_attach_shader(instance_ptr->po,
                                       result_shaders[1],
                                       true /* async */) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not link postprocessing blur gaussian program object");
        }
    }
    else
    {
        ral_context_retain_object(instance_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  instance_ptr->po);
    }

    /* Retrieve sampler objects we will use to perform the blur operation */
    const uint32_t          n_sampler_create_info_items = 2;
    ral_sampler             result_samplers          [n_sampler_create_info_items];
    ral_sampler_create_info sampler_create_info_items[n_sampler_create_info_items];

    sampler_create_info_items[0].mipmap_mode = RAL_TEXTURE_MIPMAP_MODE_LINEAR;
    sampler_create_info_items[0].wrap_s      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
    sampler_create_info_items[0].wrap_t      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;

    sampler_create_info_items[1].mipmap_mode = RAL_TEXTURE_MIPMAP_MODE_NEAREST;
    sampler_create_info_items[1].wrap_s      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
    sampler_create_info_items[1].wrap_t      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;

    ral_context_create_samplers(instance_ptr->context,
                                n_sampler_create_info_items,
                                sampler_create_info_items,
                                result_samplers);

    instance_ptr->sampler_blur_linear  = result_samplers[0];
    instance_ptr->sampler_blur_nearest = result_samplers[1];

    /* Clean up */
    if (binomial_values != nullptr)
    {
        delete [] binomial_values;

        binomial_values = nullptr;
    }

    if (coeff_vector != nullptr)
    {
        system_resizable_vector_release(coeff_vector);

        coeff_vector = nullptr;
    }

    if (factorial_values != nullptr)
    {
        delete [] factorial_values;

        factorial_values = nullptr;
    }

    if (final_data_bo_raw_ptr != nullptr)
    {
        delete [] final_data_bo_raw_ptr;

        final_data_bo_raw_ptr = nullptr;
    }

    if (misaligned_coeff_buffer_offsets != nullptr)
    {
        delete [] misaligned_coeff_buffer_offsets;

        misaligned_coeff_buffer_offsets = nullptr;
    }

    if (result_shaders[0] != nullptr &&
        result_shaders[1] != nullptr)
    {
        ral_context_delete_objects(instance_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   sizeof(result_shaders) / sizeof(result_shaders[0]),
                                   reinterpret_cast<void* const*>(result_shaders) );
    }
}

/** TODO */
PRIVATE void _postprocessing_blur_gaussian_release(void* ptr)
{
    /* Stub */
}


/** Please see header for specification */
PUBLIC postprocessing_blur_gaussian postprocessing_blur_gaussian_create(ral_context               context,
                                                                        system_hashed_ansi_string name,
                                                                        unsigned int              n_min_taps,
                                                                        unsigned int              n_max_taps)
{
    /* Instantiate the object */
    _postprocessing_blur_gaussian* result_ptr = new (std::nothrow) _postprocessing_blur_gaussian(context,
                                                                                                 n_min_taps,
                                                                                                 n_max_taps,
                                                                                                 name);

    ASSERT_DEBUG_SYNC(result_ptr != nullptr,
                      "Out of memory");

    if (result_ptr == nullptr)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    _postprocessing_blur_gaussian_init(result_ptr);

    /* Register it with the object manager */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                   _postprocessing_blur_gaussian_release,
                                                   OBJECT_TYPE_POSTPROCESSING_BLUR_GAUSSIAN,
                                                   GET_OBJECT_PATH(name,
                                                                   OBJECT_TYPE_POSTPROCESSING_BLUR_GAUSSIAN,
                                                                   nullptr) );

    /* Return the object */
    return (postprocessing_blur_gaussian) result_ptr;

end:
    if (result_ptr != nullptr)
    {
        _postprocessing_blur_gaussian_release(result_ptr);

        delete result_ptr;
    }

    return nullptr;
}

/** Please see header for spec */
PUBLIC ral_present_task postprocessing_blur_gaussian_create_present_task(postprocessing_blur_gaussian            blur,
                                                                         unsigned int                            n_taps,
                                                                         float                                   n_iterations,
                                                                         ral_texture_view                        dst_src_texture_view,
                                                                         postprocessing_blur_gaussian_resolution blur_resolution)
{
    _postprocessing_blur_gaussian* blur_ptr                    = reinterpret_cast<_postprocessing_blur_gaussian*>(blur);
    ral_command_buffer_create_info cmd_buffer_create_info;
    ral_texture                    dst_src_texture             = nullptr;
    ral_format                     dst_src_texture_view_format = RAL_FORMAT_UNKNOWN;
    unsigned int                   dst_src_texture_view_height = 0;
    unsigned int                   dst_src_texture_view_width  = 0;
    ral_texture_type               dst_src_texture_view_type   = RAL_TEXTURE_TYPE_UNKNOWN;
    ral_present_task               result                      = nullptr;

    ASSERT_DEBUG_SYNC(false,
                      "TODO: Separate into cpu+gpu tasks, so that n_iterations can be changed in run-time w/o rebuilding the whole thing.");

    ASSERT_DEBUG_SYNC(n_taps >= blur_ptr->n_min_taps &&
                      n_taps <= blur_ptr->n_max_taps,
                      "Invalid number of taps requested");

    ral_texture_view_get_property(dst_src_texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                 &dst_src_texture);

    /* Check if we can re-use a command buffer which has been created in the last invocation */
    if (blur_ptr->cached_present_task                      != nullptr              &&
        blur_ptr->cached_present_task_blur_resolution      == blur_resolution      &&
        blur_ptr->cached_present_task_n_iterations         == n_iterations         &&
        blur_ptr->cached_present_task_n_taps               == n_taps               &&
        blur_ptr->cached_present_task_dst_src_texture_view == dst_src_texture_view)
    {
        result = blur_ptr->cached_present_task;

        goto end;
    }

    if (blur_ptr->cached_present_task != nullptr)
    {
        ral_present_task_release(blur_ptr->cached_present_task);

        blur_ptr->cached_present_task = nullptr;
    }

    ral_texture_view_get_mipmap_property(dst_src_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &dst_src_texture_view_height);
    ral_texture_view_get_mipmap_property(dst_src_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &dst_src_texture_view_width);

    ral_texture_view_get_property(dst_src_texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_TYPE,
                                 &dst_src_texture_view_type);
    ral_texture_view_get_property(dst_src_texture_view,
                                  RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                 &dst_src_texture_view_format);

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

    /* Step 1) */
    unsigned int target_height;
    ral_sampler  target_sampler = nullptr;
    unsigned int target_width;

    switch (blur_resolution)
    {
        case POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_ORIGINAL:
        {
            target_height  = dst_src_texture_view_height;
            target_sampler = blur_ptr->sampler_blur_nearest; /* no interpolation needed */
            target_width   = dst_src_texture_view_width;

            break;
        }

        case POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_HALF:
        {
            target_height  = dst_src_texture_view_height / 2;
            target_sampler = blur_ptr->sampler_blur_linear;
            target_width   = dst_src_texture_view_width  / 2;

            break;
        }

        case POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_QUARTER:
        {
            target_height  = dst_src_texture_view_height / 4;
            target_sampler = blur_ptr->sampler_blur_linear;
            target_width   = dst_src_texture_view_width  / 4;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized blur resolution requested");
        }
    }

    /* Set up GFX state */
    {
        ral_gfx_state_create_info                       gfx_state_create_info;
        ral_command_buffer_set_scissor_box_command_info set_scissor_box_command_info;
        ral_command_buffer_set_viewport_command_info    set_viewport_command_info;

        if (blur_ptr->gfx_state != nullptr)
        {
            ral_gfx_state_release(blur_ptr->gfx_state);

            blur_ptr->gfx_state = nullptr;
        }

        set_scissor_box_command_info.index   = 0;
        set_scissor_box_command_info.size[0] = target_width;
        set_scissor_box_command_info.size[1] = target_height;
        set_scissor_box_command_info.xy  [0] = 0;
        set_scissor_box_command_info.xy  [1] = 0;

        set_viewport_command_info.depth_range[0] = 0.0;
        set_viewport_command_info.depth_range[1] = 1.0;
        set_viewport_command_info.index          = 0;
        set_viewport_command_info.size[0]        = static_cast<float>(target_width);
        set_viewport_command_info.size[1]        = static_cast<float>(target_height);
        set_viewport_command_info.xy[0]          = 0;
        set_viewport_command_info.xy[1]          = 0;

        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_STRIP;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes                 = &set_scissor_box_command_info;
        gfx_state_create_info.static_scissor_boxes_enabled         = true;
        gfx_state_create_info.static_viewports                     = &set_viewport_command_info;
        gfx_state_create_info.static_viewports_enabled             = true;

        ral_context_create_gfx_states(blur_ptr->context,
                                      1,
                                     &gfx_state_create_info,
                                     &blur_ptr->gfx_state);
    }

    /* Retrieve a 2D Array texture we will use for execution of the iterations */
    if (blur_ptr->ping_pong_rt != nullptr)
    {
        /* Make sure the ping pong RT we have cached matches the type of the specified
         * source texture view */
        ral_format ping_pong_rt_format;
        uint32_t   ping_pong_rt_size[2];

        ral_texture_get_mipmap_property(blur_ptr->ping_pong_rt,
                                        0, /* n_layer */
                                        0, /* n_mipmap */
                                        RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        ping_pong_rt_size + 0);
        ral_texture_get_mipmap_property(blur_ptr->ping_pong_rt,
                                        0, /* n_layer */
                                        0, /* n_mipmap */
                                        RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        ping_pong_rt_size + 1);
        ral_texture_get_property       (blur_ptr->ping_pong_rt,
                                        RAL_TEXTURE_PROPERTY_FORMAT,
                                       &ping_pong_rt_format);

        if (ping_pong_rt_format  != dst_src_texture_view_format ||
            ping_pong_rt_size[0] != dst_src_texture_view_width  ||
            ping_pong_rt_size[1] != dst_src_texture_view_height)
        {
            const ral_texture_view ping_pong_rt_views[] =
            {
                blur_ptr->ping_pong_rt_view_l0,
                blur_ptr->ping_pong_rt_view_l012,
                blur_ptr->ping_pong_rt_view_l1,
                blur_ptr->ping_pong_rt_view_l2,
            };
            const uint32_t n_ping_pong_rt_views = sizeof(ping_pong_rt_views) / sizeof(ping_pong_rt_views[0]);

            ral_context_delete_objects(blur_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&blur_ptr->ping_pong_rt) );
            ral_context_delete_objects(blur_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                       n_ping_pong_rt_views,
                                       reinterpret_cast<void* const*>(ping_pong_rt_views) );

            blur_ptr->ping_pong_rt           = nullptr;
            blur_ptr->ping_pong_rt_view_l0   = nullptr;
            blur_ptr->ping_pong_rt_view_l012 = nullptr;
            blur_ptr->ping_pong_rt_view_l1   = nullptr;
            blur_ptr->ping_pong_rt_view_l2   = nullptr;
        }
    }

    if (blur_ptr->ping_pong_rt == nullptr)
    {
        ral_texture_create_info      ping_pong_rt_create_info;
        ral_texture_view_create_info ping_pong_rt_view_create_info;
        ral_texture_view*            ping_pong_rt_views[] =
        {
            &blur_ptr->ping_pong_rt_view_l0,
            &blur_ptr->ping_pong_rt_view_l1,
            &blur_ptr->ping_pong_rt_view_l2,
        };

        ping_pong_rt_create_info.base_mipmap_depth      = 1;
        ping_pong_rt_create_info.base_mipmap_height     = target_height;
        ping_pong_rt_create_info.base_mipmap_width      = target_width;
        ping_pong_rt_create_info.fixed_sample_locations = false;
        ping_pong_rt_create_info.format                 = dst_src_texture_view_format;
        ping_pong_rt_create_info.name                   = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(blur_ptr->name),
                                                                                                                  " temp 2D array texture");
        ping_pong_rt_create_info.n_layers               = 3;
        ping_pong_rt_create_info.n_samples              = 1;
        ping_pong_rt_create_info.type                   = RAL_TEXTURE_TYPE_2D_ARRAY;
        ping_pong_rt_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_DST_BIT         |
                                                          RAL_TEXTURE_USAGE_BLIT_SRC_BIT         |
                                                          RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                          RAL_TEXTURE_USAGE_SAMPLED_BIT;
        ping_pong_rt_create_info.use_full_mipmap_chain  = false;

        ral_context_create_textures(blur_ptr->context,
                                    1, /* n_textures */
                                   &ping_pong_rt_create_info,
                                   &blur_ptr->ping_pong_rt);

        ping_pong_rt_view_create_info = ral_texture_view_create_info(blur_ptr->ping_pong_rt);

        ral_context_create_texture_views(blur_ptr->context,
                                         1, /* n_texture_views */
                                        &ping_pong_rt_view_create_info,
                                        &blur_ptr->ping_pong_rt_view_l012);

        for (uint32_t n_layer = 0;
                      n_layer < sizeof(ping_pong_rt_views) / sizeof(ping_pong_rt_views[0]);
                    ++n_layer)
        {
            ral_texture_view* result_view_ptr = ping_pong_rt_views[n_layer];

            ping_pong_rt_view_create_info.n_base_layer = n_layer;
            ping_pong_rt_view_create_info.n_layers      = 1;

            ral_context_create_texture_views(blur_ptr->context,
                                             1, /* n_texture_views */
                                            &ping_pong_rt_view_create_info,
                                             result_view_ptr);
        }
    }

    /* Start recording the command buffer.. */
    ral_command_buffer cmd_buffer;

    if (blur_ptr->cached_command_buffer == nullptr)
    {
        cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
        cmd_buffer_create_info.is_resettable                           = true;
        cmd_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(blur_ptr->context,
                                           1, /* n_command_buffers */
                                          &cmd_buffer_create_info,
                                          &blur_ptr->cached_command_buffer);
    }

    cmd_buffer = blur_ptr->cached_command_buffer;
    
    ral_command_buffer_start_recording(cmd_buffer);

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
     * reworking the fragment shader, but.. This is left as homework.
     *
     */
    ral_command_buffer_record_set_program  (cmd_buffer,
                                            blur_ptr->po);
    ral_command_buffer_record_set_gfx_state(cmd_buffer,
                                            blur_ptr->gfx_state);

    {
        ral_command_buffer_set_binding_command_info set_data_texture_binding_command_info;

        set_data_texture_binding_command_info.binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        set_data_texture_binding_command_info.sampled_image_binding.sampler      = target_sampler;
        set_data_texture_binding_command_info.sampled_image_binding.texture_view = blur_ptr->ping_pong_rt_view_l012;

        ral_command_buffer_record_set_bindings(cmd_buffer,
                                               1, /* n_bindings */
                                              &set_data_texture_binding_command_info);
    }

    /* Make sure the "other data BO" iteration-specific value is set to the number of taps the caller
     * is requesting */
    if (blur_ptr->other_data_bo_cached_value != n_taps)
    {
        const int data[] =
        {
            0,
            static_cast<int>(n_taps),
            1
        };
        std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > bo_update_ptrs;
        ral_buffer_client_sourced_update_info                                other_data_bo_update;

        other_data_bo_update.data         = data;
        other_data_bo_update.data_size    = sizeof(data);
        other_data_bo_update.start_offset = 0;

        bo_update_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&other_data_bo_update,
                                                                                        NullDeleter<ral_buffer_client_sourced_update_info>() ));

        ral_buffer_set_data_from_client_memory(blur_ptr->other_data_bo,
                                               bo_update_ptrs,
                                               false, /* async                */
                                               false  /* sync_other_contexts */);

        blur_ptr->other_data_bo_cached_value = n_taps;
    }

    /* Iterate over all layers we need blurred */
    ASSERT_DEBUG_SYNC(dst_src_texture_view_type == RAL_TEXTURE_TYPE_2D       ||
                      dst_src_texture_view_type == RAL_TEXTURE_TYPE_2D_ARRAY ||
                      dst_src_texture_view_type == RAL_TEXTURE_TYPE_CUBE_MAP,
                      "Unsupported source texture type");

    unsigned int n_layers = 0;

    switch (dst_src_texture_view_type)
    {
        case RAL_TEXTURE_TYPE_2D:
        {
            n_layers = 1;

            break;
        }

        case RAL_TEXTURE_TYPE_2D_ARRAY:
        {
            ral_texture_view_get_property(dst_src_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                         &n_layers);

            ASSERT_DEBUG_SYNC(n_layers != 0,
                              "Could not retrieve the number of layers of the input 2D Array Texture view.");

            break;
        }

        case RAL_TEXTURE_TYPE_CUBE_MAP:
        {
            n_layers = 6;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported input texture type.");
        }
    }

    {
        ral_command_buffer_set_binding_command_info set_other_data_binding_command_info;

        set_other_data_binding_command_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        set_other_data_binding_command_info.name                          = system_hashed_ansi_string_create("other_data");
        set_other_data_binding_command_info.uniform_buffer_binding.buffer = blur_ptr->other_data_bo;
        set_other_data_binding_command_info.uniform_buffer_binding.offset = 0;
        set_other_data_binding_command_info.uniform_buffer_binding.size   = sizeof(int) * 3;

        ral_command_buffer_record_set_bindings(cmd_buffer,
                                               1, /* n_bindings */
                                              &set_other_data_binding_command_info);
    }


    ral_command_buffer_copy_buffer_to_buffer_command_info copy_op_frac_command_info;
    ral_command_buffer_copy_buffer_to_buffer_command_info copy_op_per_layer_command_info;

    copy_op_frac_command_info.dst_buffer_start_offset = 0;
    copy_op_frac_command_info.size                    =     sizeof(int);
    copy_op_frac_command_info.src_buffer_start_offset = 2 * sizeof(int);

    copy_op_per_layer_command_info.dst_buffer_start_offset = 0;
    copy_op_per_layer_command_info.size                    = sizeof(int);
    copy_op_per_layer_command_info.src_buffer_start_offset = sizeof(int);

    for (unsigned int n_layer = 0;
                      n_layer < n_layers;
                    ++n_layer)
    {
        /* Use the iteration-specific number of taps */
        {
            copy_op_per_layer_command_info.dst_buffer = blur_ptr->other_data_bo;
            copy_op_per_layer_command_info.src_buffer = blur_ptr->other_data_bo;

            ral_command_buffer_record_copy_buffer_to_buffer(cmd_buffer,
                                                            1, /* n_copy_ops */
                                                           &copy_op_per_layer_command_info);
        }

        {
            ral_command_buffer_set_binding_command_info set_coeffs_data_binding_command_info;

            set_coeffs_data_binding_command_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            set_coeffs_data_binding_command_info.name                          = system_hashed_ansi_string_create("coeffs_data");
            set_coeffs_data_binding_command_info.uniform_buffer_binding.buffer = blur_ptr->coeff_bo;
            set_coeffs_data_binding_command_info.uniform_buffer_binding.offset = blur_ptr->coeff_buffer_offsets[n_taps - blur_ptr->n_min_taps];
            set_coeffs_data_binding_command_info.uniform_buffer_binding.size   = blur_ptr->n_max_data_coeffs * 4 /* padding */ * sizeof(float);

            ral_command_buffer_record_set_bindings(cmd_buffer,
                                                   1, /* n_bindings */
                                                  &set_coeffs_data_binding_command_info);
        }

        /* Copy the layer data to be blurred */
        {
            ral_command_buffer_copy_texture_to_texture_command_info copy_layer_to_temp_2d_array_command_info;

            copy_layer_to_temp_2d_array_command_info.aspect               = RAL_TEXTURE_ASPECT_COLOR_BIT;
            copy_layer_to_temp_2d_array_command_info.dst_size[0]          = target_width;
            copy_layer_to_temp_2d_array_command_info.dst_size[1]          = target_height;
            copy_layer_to_temp_2d_array_command_info.dst_size[2]          = 1;
            copy_layer_to_temp_2d_array_command_info.dst_start_xyz[0]     = 0;
            copy_layer_to_temp_2d_array_command_info.dst_start_xyz[1]     = 0;
            copy_layer_to_temp_2d_array_command_info.dst_start_xyz[2]     = 0;
            copy_layer_to_temp_2d_array_command_info.dst_texture          = blur_ptr->ping_pong_rt;
            copy_layer_to_temp_2d_array_command_info.n_dst_texture_layer  = 0;
            copy_layer_to_temp_2d_array_command_info.n_dst_texture_mipmap = 0;
            copy_layer_to_temp_2d_array_command_info.n_src_texture_layer  = n_layer;
            copy_layer_to_temp_2d_array_command_info.n_src_texture_mipmap = 0;
            copy_layer_to_temp_2d_array_command_info.scaling_filter       = (target_sampler == blur_ptr->sampler_blur_linear) ? RAL_TEXTURE_FILTER_LINEAR
                                                                                                                              : RAL_TEXTURE_FILTER_NEAREST;
            copy_layer_to_temp_2d_array_command_info.src_size[0]          = dst_src_texture_view_width;
            copy_layer_to_temp_2d_array_command_info.src_size[1]          = dst_src_texture_view_height;
            copy_layer_to_temp_2d_array_command_info.src_size[2]          = 1;
            copy_layer_to_temp_2d_array_command_info.src_start_xyz[0]     = 0;
            copy_layer_to_temp_2d_array_command_info.src_start_xyz[1]     = 0;
            copy_layer_to_temp_2d_array_command_info.src_start_xyz[2]     = 0;
            copy_layer_to_temp_2d_array_command_info.src_texture          = dst_src_texture;

            ral_command_buffer_record_copy_texture_to_texture(cmd_buffer,
                                                              1, /* n_copy_ops */
                                                             &copy_layer_to_temp_2d_array_command_info);
        }

        /* Step 2a): Kick off. Result blurred texture is stored under layer 0 */
        float n_iterations_frac = fmod(n_iterations, 1.0f);

        for (unsigned int n_iteration = 0;
                          n_iteration < (unsigned int) floor(n_iterations);
                        ++n_iteration)
        {
            /* Horizontal pass: */

            /* Update rendertarget */
            {
                ral_command_buffer_set_color_rendertarget_command_info set_rt_command_info;

                set_rt_command_info.blend_enabled         = false;
                set_rt_command_info.channel_writes.color0 = true;
                set_rt_command_info.channel_writes.color1 = true;
                set_rt_command_info.channel_writes.color2 = true;
                set_rt_command_info.channel_writes.color3 = true;
                set_rt_command_info.rendertarget_index    = 0;
                set_rt_command_info.texture_view          = blur_ptr->ping_pong_rt_view_l1;

                ral_command_buffer_record_set_color_rendertargets(cmd_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &set_rt_command_info);
            }

            /* Do a full-screen pass.
             *
             * NOTE!! : The call below currently TDRs Intel driver :-( */
            {
                ral_command_buffer_draw_call_regular_command_info draw_call_command_info;

                draw_call_command_info.base_instance = 0;
                draw_call_command_info.base_vertex   = 0; /* 0 here causes FS to read from layer 0 */
                draw_call_command_info.n_instances   = 1;
                draw_call_command_info.n_vertices    = 4;

                ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                            1, /* n_draw_calls */
                                                           &draw_call_command_info);
            }

            /* Vertical pass: */

            /* Update rendertarget */
            {
                ral_command_buffer_set_color_rendertarget_command_info set_rt_command_info;

                set_rt_command_info.blend_enabled         = false;
                set_rt_command_info.channel_writes.color0 = true;
                set_rt_command_info.channel_writes.color1 = true;
                set_rt_command_info.channel_writes.color2 = true;
                set_rt_command_info.channel_writes.color3 = true;
                set_rt_command_info.rendertarget_index    = 0;
                set_rt_command_info.texture_view          = blur_ptr->ping_pong_rt_view_l0;

                ral_command_buffer_record_set_color_rendertargets(cmd_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &set_rt_command_info);
            }

            /* Do a full-screen pass again */
            {
                ral_command_buffer_draw_call_regular_command_info draw_call_command_info;

                draw_call_command_info.base_instance = 0;
                draw_call_command_info.base_vertex   = 4; /* 4 here causes FS to read from layer 1 */
                draw_call_command_info.n_instances   = 1;
                draw_call_command_info.n_vertices    = 4;

                ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                            1, /* n_draw_calls */
                                                           &draw_call_command_info);
            }
        }

        /* Step 2b) Run the extra iteration if frac(n_iterations) != 0 */
        if (n_iterations_frac > 1e-5f)
        {
            /* Draw the src texture, blurred (n_iterations + 1) times, into texture layer 1 */

            /* Horizontal pass */
            {
                ral_command_buffer_set_color_rendertarget_command_info set_rt_command_info;

                set_rt_command_info.blend_enabled         = false;
                set_rt_command_info.channel_writes.color0 = true;
                set_rt_command_info.channel_writes.color1 = true;
                set_rt_command_info.channel_writes.color2 = true;
                set_rt_command_info.channel_writes.color3 = true;
                set_rt_command_info.rendertarget_index    = 0;
                set_rt_command_info.texture_view          = blur_ptr->ping_pong_rt_view_l2;

                ral_command_buffer_record_set_color_rendertargets(cmd_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &set_rt_command_info);
            }

            {
                ral_command_buffer_draw_call_regular_command_info draw_call_command_info;

                draw_call_command_info.base_instance = 0;
                draw_call_command_info.base_vertex   = 0; /* 0 here causes FS to read from layer 0 */
                draw_call_command_info.n_instances   = 1;
                draw_call_command_info.n_vertices    = 4;

                ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                            1, /* n_draw_calls */
                                                           &draw_call_command_info);
            }

            /* Vertical pass */
            {
                ral_command_buffer_set_color_rendertarget_command_info set_rt_command_info;

                set_rt_command_info.blend_enabled         = false;
                set_rt_command_info.channel_writes.color0 = true;
                set_rt_command_info.channel_writes.color1 = true;
                set_rt_command_info.channel_writes.color2 = true;
                set_rt_command_info.channel_writes.color3 = true;
                set_rt_command_info.rendertarget_index    = 0;
                set_rt_command_info.texture_view          = blur_ptr->ping_pong_rt_view_l1;

                ral_command_buffer_record_set_color_rendertargets(cmd_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &set_rt_command_info);
            }

            {
                ral_command_buffer_draw_call_regular_command_info draw_call_command_info;

                draw_call_command_info.base_instance = 0;
                draw_call_command_info.base_vertex   = 8; /* 8 here causes FS to read from layer 1 */
                draw_call_command_info.n_instances   = 1;
                draw_call_command_info.n_vertices    = 4;

                ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                            1, /* n_draw_calls */
                                                           &draw_call_command_info);
            }

            /* Step 3): Blend the (n_iterations + 1) texture layer over the (n_iterations) texture layer */
            {
                ral_command_buffer_set_color_rendertarget_command_info set_rt_command_info;

                set_rt_command_info.blend_constant.data_type = RAL_COLOR_DATA_TYPE_FLOAT;
                set_rt_command_info.blend_constant.f32[0]    = 0.0f;
                set_rt_command_info.blend_constant.f32[1]    = 0.0f;
                set_rt_command_info.blend_constant.f32[2]    = 0.0f;
                set_rt_command_info.blend_constant.f32[3]    = n_iterations_frac;
                set_rt_command_info.blend_enabled            = true;
                set_rt_command_info.blend_op_alpha           = RAL_BLEND_OP_ADD;
                set_rt_command_info.blend_op_color           = RAL_BLEND_OP_ADD;
                set_rt_command_info.channel_writes.color0    = true;
                set_rt_command_info.channel_writes.color1    = true;
                set_rt_command_info.channel_writes.color2    = true;
                set_rt_command_info.channel_writes.color3    = true;
                set_rt_command_info.dst_alpha_blend_factor   = RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
                set_rt_command_info.dst_color_blend_factor   = RAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
                set_rt_command_info.rendertarget_index       = 0;
                set_rt_command_info.src_alpha_blend_factor   = RAL_BLEND_FACTOR_CONSTANT_ALPHA;
                set_rt_command_info.src_color_blend_factor   = RAL_BLEND_FACTOR_CONSTANT_ALPHA;
                set_rt_command_info.texture_view             = blur_ptr->ping_pong_rt_view_l1;

                ral_command_buffer_record_set_color_rendertargets(cmd_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &set_rt_command_info);
            }

            {
                ral_command_buffer_set_binding_command_info set_coeffs_data_binding_command_info;

                set_coeffs_data_binding_command_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
                set_coeffs_data_binding_command_info.name                          = system_hashed_ansi_string_create("coeffs_data");
                set_coeffs_data_binding_command_info.uniform_buffer_binding.buffer = blur_ptr->coeff_bo;
                set_coeffs_data_binding_command_info.uniform_buffer_binding.offset = blur_ptr->coeff_buffer_offset_for_value_1;
                set_coeffs_data_binding_command_info.uniform_buffer_binding.size   = blur_ptr->n_max_data_coeffs * 4 /* padding */ * sizeof(float);

                ral_command_buffer_record_set_bindings(cmd_buffer,
                                                       1, /* n_bindings */
                                                      &set_coeffs_data_binding_command_info);
            }

            /* Set the number of taps to 1 */
            {
                copy_op_frac_command_info.dst_buffer = blur_ptr->other_data_bo;
                copy_op_frac_command_info.src_buffer = blur_ptr->other_data_bo;

                ral_command_buffer_record_copy_buffer_to_buffer(cmd_buffer,
                                                                1, /* n_copy_ops */
                                                               &copy_op_frac_command_info);
            }

            /* Draw data from texture layer 1 */
            {
                ral_command_buffer_draw_call_regular_command_info draw_call_command_info;

                draw_call_command_info.base_instance = 0;
                draw_call_command_info.base_vertex   = 4;
                draw_call_command_info.n_instances   = 1;
                draw_call_command_info.n_vertices    = 4;

                ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                            1, /* n_draw_calls */
                                                           &draw_call_command_info);
            }
        }

        /* Step 4): Store the result in the user-specified texture */
        {
            ral_command_buffer_copy_texture_to_texture_command_info copy_texture_command_info;

            copy_texture_command_info.aspect               = RAL_TEXTURE_ASPECT_COLOR_BIT;
            copy_texture_command_info.dst_size[0]          = target_height;
            copy_texture_command_info.dst_size[1]          = target_width;
            copy_texture_command_info.dst_size[2]          = 1;
            copy_texture_command_info.dst_start_xyz[0]     = 0;
            copy_texture_command_info.dst_start_xyz[1]     = 0;
            copy_texture_command_info.dst_start_xyz[2]     = 0;
            copy_texture_command_info.dst_texture          = dst_src_texture;
            copy_texture_command_info.n_dst_texture_layer  = 0;
            copy_texture_command_info.n_dst_texture_mipmap = 0;
            copy_texture_command_info.n_src_texture_layer  = (dst_src_texture_view_type == RAL_TEXTURE_TYPE_2D) ? 0 : n_layer;
            copy_texture_command_info.n_src_texture_mipmap = 0;
            copy_texture_command_info.scaling_filter       = (target_sampler == blur_ptr->sampler_blur_nearest) ? RAL_TEXTURE_FILTER_NEAREST
                                                                                                                : RAL_TEXTURE_FILTER_LINEAR;
            copy_texture_command_info.src_size[0]          = dst_src_texture_view_width;
            copy_texture_command_info.src_size[1]          = dst_src_texture_view_height;
            copy_texture_command_info.src_size[2]          = 1;
            copy_texture_command_info.src_start_xyz[0]     = 0;
            copy_texture_command_info.src_start_xyz[1]     = 0;
            copy_texture_command_info.src_start_xyz[2]     = 0;
            copy_texture_command_info.src_texture          = blur_ptr->ping_pong_rt;

            ral_command_buffer_record_copy_texture_to_texture(cmd_buffer,
                                                              1, /* n_copy_ops */
                                                             &copy_texture_command_info);
        }
    }

    ral_command_buffer_record_invalidate_texture(cmd_buffer,
                                                 blur_ptr->ping_pong_rt,
                                                 0, /* n_start_mip */
                                                 1);
    ral_command_buffer_stop_recording           (cmd_buffer);


    /* Recording finished. Create a new presentation  task. */
    ral_present_task_io              dst_src_texture_io;
    ral_present_task_gpu_create_info present_task_create_info;

    dst_src_texture_io.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    dst_src_texture_io.texture_view = dst_src_texture_view;

    present_task_create_info.command_buffer   = cmd_buffer;
    present_task_create_info.n_unique_inputs  = 1;
    present_task_create_info.n_unique_outputs = 1;
    present_task_create_info.unique_inputs    = &dst_src_texture_io;
    present_task_create_info.unique_outputs   = &dst_src_texture_io;

    result = ral_present_task_create_gpu(blur_ptr->name,
                                        &present_task_create_info);

    ASSERT_DEBUG_SYNC(result != nullptr,
                      "Could not create a present task.");

    if (result != nullptr)
    {
        blur_ptr->cached_present_task                      = result;
        blur_ptr->cached_present_task_blur_resolution      = blur_resolution;
        blur_ptr->cached_present_task_dst_src_texture_view = dst_src_texture_view;
        blur_ptr->cached_present_task_n_iterations         = n_iterations;
        blur_ptr->cached_present_task_n_taps               = n_taps;

        ral_present_task_retain(result);
    }

end:
    return result;
}
