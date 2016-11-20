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
    ral_context                context;
    ral_gfx_state_create_info* create_info_ptr;

    ~_ral_gfx_state()
    {
        if (create_info_ptr != nullptr)
        {
            create_info_ptr->release(create_info_ptr);

            create_info_ptr = nullptr;
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
    ASSERT_DEBUG_SYNC(create_info_ptr->static_scissor_boxes_enabled && (create_info_ptr->static_n_scissor_boxes_and_viewports != 0        &&
                                                                        create_info_ptr->static_scissor_boxes                 != nullptr) ||
                     !create_info_ptr->static_scissor_boxes_enabled,
                      "Static scissor box configuration is invalid");
    ASSERT_DEBUG_SYNC(create_info_ptr->static_viewports_enabled && (create_info_ptr->static_n_scissor_boxes_and_viewports != 0        &&
                                                                    create_info_ptr->static_viewports                     != nullptr) ||
                     !create_info_ptr->static_viewports_enabled,
                      "Static viewport state configuration is invalid");

    /* Cache the user-specified info from @param create_info_ptr */
    if (gfx_state_ptr != nullptr)
    {
        gfx_state_ptr->context         = context;
        gfx_state_ptr->create_info_ptr = ral_gfx_state_create_info::clone(create_info_ptr,
                                                                          create_info_ptr->static_scissor_boxes_enabled,
                                                                          create_info_ptr->static_viewports_enabled,
                                                                          true); /* should_include_va_data */
    }

    return (ral_gfx_state) gfx_state_ptr;
}

/** Please see header for specification */
PUBLIC void ral_gfx_state_get_property(ral_gfx_state          gfx_state,
                                       ral_gfx_state_property property,
                                       void*                  out_result_ptr)
{
    _ral_gfx_state* gfx_state_ptr = reinterpret_cast<_ral_gfx_state*>(gfx_state);

    ASSERT_DEBUG_SYNC(gfx_state_ptr != nullptr,
                      "Input gfx_state instance is null");

    switch (property)
    {
        case RAL_GFX_STATE_PROPERTY_ALPHA_TO_COVERAGE_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->alpha_to_coverage;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_ALPHA_TO_ONE_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->alpha_to_one;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = gfx_state_ptr->context;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_CULLING_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->culling;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_CULL_MODE:
        {
            *reinterpret_cast<ral_cull_mode*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->cull_mode;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_CONSTANT_FACTOR:
        {
            *reinterpret_cast<float*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_bias_constant_factor;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_bias;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_SLOPE_FACTOR:
        {
            *reinterpret_cast<float*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_bias_slope_factor;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MAX:
        {
            *reinterpret_cast<float*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->max_depth_bounds;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MIN:
        {
            *reinterpret_cast<float*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->min_depth_bounds;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_TEST_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_bounds_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_CLAMP_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_clamp;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_COMPARE_FUNC:
        {
            *reinterpret_cast<ral_compare_op*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_test_compare_op;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_TEST_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_DEPTH_WRITES_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->depth_writes;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_FRONT_FACE:
        {
            *reinterpret_cast<ral_front_face*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->front_face;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_LINE_WIDTH:
        {
            *reinterpret_cast<float*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->line_width;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_LOGIC_OP:
        {
            *reinterpret_cast<ral_logic_op*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->logic_op;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_LOGIC_OP_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->logic_op_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_N_PATCH_CONTROL_POINTS:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->n_patch_control_points;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_N_STATIC_SCISSOR_BOXES_AND_VIEWPORTS:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->static_n_scissor_boxes_and_viewports;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_N_VERTEX_ATTRIBUTES:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->n_vertex_attributes;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_POLYGON_MODE_BACK:
        {
            *reinterpret_cast<ral_polygon_mode*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->polygon_mode_back;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_POLYGON_MODE_FRONT:
        {
            *reinterpret_cast<ral_polygon_mode*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->polygon_mode_front;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_PRIMITIVE_RESTART_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->primitive_restart;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE:
        {
            *reinterpret_cast<ral_primitive_type*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->primitive_type;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_RASTERIZER_DISCARD_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->rasterizer_discard;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->sample_shading;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_MIN:
        {
            *reinterpret_cast<float*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->sample_shading_min_sample_shading;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_SCISSOR_TEST_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->scissor_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES:
        {
            ASSERT_DEBUG_SYNC(gfx_state_ptr->create_info_ptr->static_scissor_boxes_enabled,
                              "Invalid RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES query");

            *reinterpret_cast<ral_command_buffer_set_scissor_box_command_info**>(out_result_ptr) = gfx_state_ptr->create_info_ptr->static_scissor_boxes;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STATIC_SCISSOR_BOXES_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->static_scissor_boxes_enabled;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS:
        {
            ASSERT_DEBUG_SYNC(gfx_state_ptr->create_info_ptr->static_viewports_enabled,
                              "Invalid RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS query");

            *reinterpret_cast<ral_command_buffer_set_viewport_command_info**>(out_result_ptr) = gfx_state_ptr->create_info_ptr->static_viewports;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->static_viewports_enabled;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STENCIL_TEST_BACK:
        {
            *reinterpret_cast<ral_stencil_op_state*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->stencil_test_back_face;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STENCIL_TEST_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->stencil_test;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_STENCIL_TEST_FRONT:
        {
            *reinterpret_cast<ral_stencil_op_state*>(out_result_ptr) = gfx_state_ptr->create_info_ptr->stencil_test_front_face;

            break;
        }

        case RAL_GFX_STATE_PROPERTY_VERTEX_ATTRIBUTES:
        {
            *reinterpret_cast<ral_gfx_state_vertex_attribute**>(out_result_ptr) = gfx_state_ptr->create_info_ptr->vertex_attribute_ptrs;

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
    delete reinterpret_cast<_ral_gfx_state*>(state);
}