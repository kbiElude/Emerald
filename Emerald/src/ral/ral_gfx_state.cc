/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_gfx_state.h"


typedef struct
{
    ral_context               context;
    ral_gfx_state_create_info create_info;
} _ral_gfx_state;


/** Please see header for specification */
PUBLIC ral_gfx_state ral_gfx_state_create(ral_context                      context,
                                          const ral_gfx_state_create_info* create_info_ptr)
{
    _ral_gfx_state* gfx_state_ptr = new _ral_gfx_state();

    ASSERT_ALWAYS_SYNC(gfx_state_ptr != nullptr,
                       "Out of memory");

    if (gfx_state_ptr != nullptr)
    {
        gfx_state_ptr->context     = context;
        gfx_state_ptr->create_info = *create_info_ptr;
    }

    return (ral_gfx_state) gfx_state_ptr;
}

/** Please see header for specification */
PUBLIC void ral_gfx_state_get_property(ral_gfx_state          gfx_state,
                                       ral_gfx_state_property property,
                                       void*                  out_result_ptr)
{
    _ral_gfx_state* gfx_state_ptr = (_ral_gfx_state*) gfx_state;

    switch (property)
    {
        case RAL_GFX_STATE_PROPERTY_ALPHA_TO_COVERAGE_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.alpha_to_coverage;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_ALPHA_TO_ONE_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.alpha_to_one;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_CONTEXT:
        {
            *(ral_context*) out_result_ptr = gfx_state_ptr->context;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_CULLING_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.culling;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_CULL_MODE:
        {
            *(ral_cull_mode*) out_result_ptr = gfx_state_ptr->create_info.cull_mode;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_CONSTANT_FACTOR:
        {
            *(float*) out_result_ptr = gfx_state_ptr->create_info.depth_bias_constant_factor;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.depth_bias;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_SLOPE_FACTOR:
        {
            *(float*) out_result_ptr = gfx_state_ptr->create_info.depth_bias_slope_factor;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MAX:
        {
            *(float*) out_result_ptr = gfx_state_ptr->create_info.max_depth_bounds;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MIN:
        {
            *(float*) out_result_ptr = gfx_state_ptr->create_info.min_depth_bounds;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_TEST_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.depth_bounds_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_CLAMP_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.depth_clamp;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_COMPARE_FUNC:
        {
            *(ral_compare_op*) out_result_ptr = gfx_state_ptr->create_info.depth_test_compare_op;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_TEST_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.depth_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_WRITES_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.depth_writes;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_FRONT_FACE:
        {
            *(ral_front_face*) out_result_ptr = gfx_state_ptr->create_info.front_face;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_LINE_WIDTH:
        {
            *(float*) out_result_ptr = gfx_state_ptr->create_info.line_width;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_LOGIC_OP:
        {
            *(ral_logic_op*) out_result_ptr = gfx_state_ptr->create_info.logic_op;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_LOGIC_OP_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.logic_op_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_N_PATCH_CONTROL_POINTS:
        {
            *(uint32_t*) out_result_ptr = gfx_state_ptr->create_info.n_patch_control_points;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_POLYGON_MODE_BACK:
        {
            *(ral_polygon_mode*) out_result_ptr = gfx_state_ptr->create_info.polygon_mode_back;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_POLYGON_MODE_FRONT:
        {
            *(ral_polygon_mode*) out_result_ptr = gfx_state_ptr->create_info.polygon_mode_front;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_PRIMITIVE_RESTART_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.primitive_restart;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE:
        {
            *(ral_primitive_type*) out_result_ptr = gfx_state_ptr->create_info.primitive_type;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_RASTERIZER_DISCARD_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.rasterizer_discard;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.sample_shading;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_MIN:
        {
            *(float*) out_result_ptr = gfx_state_ptr->create_info.sample_shading_min_sample_shading;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_SCISSOR_TEST_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.scissor_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STENCIL_TEST_BACK:
        {
            *(ral_stencil_op_state*) out_result_ptr = gfx_state_ptr->create_info.stencil_test_back_face;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STENCIL_TEST_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.stencil_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STENCIL_TEST_FRONT:
        {
            *(ral_stencil_op_state*) out_result_ptr = gfx_state_ptr->create_info.stencil_test_front_face;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_gfx_state_property value.");
        }
    }
}

/** Please see header for specification */
PUBLIC void ral_gfx_state_release(ral_gfx_state state)
{
    delete (_ral_gfx_state*) state;
}