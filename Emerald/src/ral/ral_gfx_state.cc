/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_gfx_state.h"


typedef struct _ral_gfx_state
{
    ral_context               context;
    ral_gfx_state_create_info create_info;

    ~_ral_gfx_state()
    {
        if (create_info.static_scissor_boxes != nullptr)
        {
            delete [] create_info.static_scissor_boxes;

            create_info.static_scissor_boxes = nullptr;
        }

        if (create_info.static_viewports != nullptr)
        {
            delete [] create_info.static_viewports;

            create_info.static_viewports = nullptr;
        }
    }
} _ral_gfx_state;


/** Please see header for specification */
PUBLIC ral_gfx_state ral_gfx_state_create(ral_context                      context,
                                          const ral_gfx_state_create_info* create_info_ptr)
{
    _ral_gfx_state* gfx_state_ptr = new _ral_gfx_state();

    ASSERT_ALWAYS_SYNC(gfx_state_ptr != nullptr,
                       "Out of memory");

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(!create_info_ptr->static_scissor_boxes_and_viewports && (create_info_ptr->static_n_scissor_boxes_and_viewports == 0        &&
                                                                               create_info_ptr->static_scissor_boxes                 == nullptr  &&
                                                                               create_info_ptr->static_viewports                     == nullptr) ||
                       create_info_ptr->static_scissor_boxes_and_viewports && (create_info_ptr->static_n_scissor_boxes_and_viewports == 0        ||
                                                                               create_info_ptr->static_n_scissor_boxes_and_viewports != 0        &&
                                                                               create_info_ptr->static_scissor_boxes                 != nullptr  &&
                                                                               create_info_ptr->static_viewports                     != nullptr),
                      "Static scissor box+viewport state configuration is invalid");

    /* Cache the user-specified info from @param create_info_ptr */
    if (gfx_state_ptr != nullptr)
    {
        gfx_state_ptr->context     = context;
        gfx_state_ptr->create_info = *create_info_ptr;

        /* Static scissor box + viewport data needs extra care */
        if (create_info_ptr->static_n_scissor_boxes_and_viewports != 0)
        {
            gfx_state_ptr->create_info.static_scissor_boxes = new (std::nothrow) ral_command_buffer_set_scissor_box_command_info[create_info_ptr->static_n_scissor_boxes_and_viewports];
            gfx_state_ptr->create_info.static_viewports     = new (std::nothrow) ral_command_buffer_set_viewport_command_info   [create_info_ptr->static_n_scissor_boxes_and_viewports];

            ASSERT_ALWAYS_SYNC(gfx_state_ptr->create_info.static_scissor_boxes != nullptr &&
                               gfx_state_ptr->create_info.static_viewports     != nullptr,
                               "Out of memory");

            memcpy(gfx_state_ptr->create_info.static_scissor_boxes,
                   create_info_ptr->static_scissor_boxes,
                   sizeof(ral_command_buffer_set_scissor_box_command_info) * create_info_ptr->static_n_scissor_boxes_and_viewports);
            memcpy(gfx_state_ptr->create_info.static_viewports,
                   create_info_ptr->static_viewports,
                   sizeof(ral_command_buffer_set_viewport_command_info) * create_info_ptr->static_n_scissor_boxes_and_viewports);
        }
        else
        {
            gfx_state_ptr->create_info.static_scissor_boxes = nullptr;
            gfx_state_ptr->create_info.static_viewports     = nullptr;
        }
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

        case RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES:
        {
            ASSERT_DEBUG_SYNC(gfx_state_ptr->create_info.static_scissor_boxes_and_viewports,
                              "Invalid RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES query");

            *(ral_command_buffer_set_scissor_box_command_info**) out_result_ptr = gfx_state_ptr->create_info.static_scissor_boxes;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES_AND_VIEWPORTS_ENABLED:
        {
            *(bool*) out_result_ptr = gfx_state_ptr->create_info.static_scissor_boxes_and_viewports;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS:
        {
            ASSERT_DEBUG_SYNC(gfx_state_ptr->create_info.static_scissor_boxes_and_viewports,
                              "Invalid RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS query");

            *(ral_command_buffer_set_viewport_command_info**) out_result_ptr = gfx_state_ptr->create_info.static_viewports;

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