#ifndef RAL_GFX_STATE_H
#define RAL_GFX_STATE_H

#include "ral/ral_types.h"


typedef struct ral_gfx_state_vertex_attribute
{
    uint32_t                    binding;
    ral_vertex_attribute_format format;
    ral_vertex_input_rate       input_rate;
    uint32_t                    offset;
    uint32_t                    stride;
} ral_gfx_state_vertex_attribute;


typedef struct ral_gfx_state_create_info
{
    bool alpha_to_coverage;
    bool alpha_to_one;
    bool depth_bias;
    bool depth_bounds_test;
    bool depth_clamp;
    bool depth_test;
    bool depth_writes;
    bool logic_op_test;
    bool primitive_restart;
    bool rasterizer_discard;
    bool sample_shading;
    bool stencil_test;

    /* Depth bias */
    float depth_bias_constant_factor;
    float depth_bias_slope_factor;

    /* Depth clamp */
    float depth_clamp_value;

    /* Depth bounds test */
    float max_depth_bounds;
    float min_depth_bounds;

    /* Depth test */
    ral_compare_op depth_test_compare_op;

    /* Logic op */
    ral_logic_op logic_op;

    /* Sample shading */
    float sample_shading_min_sample_shading;

    /* Stencil test */
    ral_stencil_op_state stencil_test_back_face;
    ral_stencil_op_state stencil_test_front_face;

    /* Other */
    ral_cull_mode      cull_mode;
    ral_front_face     front_face;
    float              line_width;
    uint32_t           n_patch_control_points;
    ral_polygon_mode   polygon_mode;
    ral_primitive_type primitive_type;

    uint32_t                              n_vertex_attributes;
    const ral_gfx_state_vertex_attribute* vertex_attribute_ptrs;

    ral_gfx_state_create_info()
    {
        alpha_to_coverage  = false;
        alpha_to_one       = false;
        depth_bias         = false;
        depth_bounds_test  = false;
        depth_clamp        = false;
        depth_test         = false;
        depth_writes       = false;
        logic_op_test      = false;
        primitive_restart  = false;
        rasterizer_discard = false;
        sample_shading     = false;
        stencil_test       = false;

        cull_mode                         = RAL_CULL_MODE_BACK;
        depth_bias_constant_factor        = 0.0f;
        depth_bias_slope_factor           = 1.0f;
        depth_clamp_value                 = 0.0f;
        depth_test_compare_op             = RAL_COMPARE_OP_ALWAYS;
        front_face                        = RAL_FRONT_FACE_CCW;
        line_width                        = 1.0f;
        logic_op                          = RAL_LOGIC_OP_NOOP;
        max_depth_bounds                  = 1.0f;
        min_depth_bounds                  = 0.0f;
        n_vertex_attributes               = 0;
        n_patch_control_points            = 0;
        polygon_mode                      = RAL_POLYGON_MODE_FILL;
        primitive_type                    = RAL_PRIMITIVE_TYPE_TRIANGLES;
        sample_shading_min_sample_shading = 1.0f;
        vertex_attribute_ptrs             = nullptr;
    }
} ral_gfx_state_create_info;

typedef enum
{
    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_ALPHA_TO_COVERAGE_ENABLED,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_ALPHA_TO_ONE_ENABLED,

    /* not settable; ral_cull_mode */
    RAL_GFX_STATE_PROPERTY_CULL_MODE,

    /* not settable; float */
    RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_CONSTANT_FACTOR,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_ENABLED,

    /* not settable; float */
    RAL_GFX_STATE_PROPERTY_DEPTH_BIAS_SLOPE_FACTOR,

    /* not settable; float */
    RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MAX,

    /* not settable; float */
    RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_MIN,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_DEPTH_BOUNDS_TEST_ENABLED,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_DEPTH_CLAMP_ENABLED,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_DEPTH_TEST_ENABLED,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_DEPTH_WRITES_ENABLED,

    /* not settable; ral_front_face */
    RAL_GFX_STATE_PROPERTY_FRONT_FACE,

    /* not settable; float */
    RAL_GFX_STATE_PROPERTY_LINE_WIDTH,

    /* not settable; ral_logic_op */
    RAL_GFX_STATE_PROPERTY_LOGIC_OP,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_LOGIC_OP_ENABLED,

    /* not settable; uint32_t */
    RAL_GFX_STATE_PROPERTY_N_PATCH_CONTROL_POINTS,

    /* not settable; uint32_t */
    RAL_GFX_STATE_PROPERTY_N_VERTEX_ATTRIBUTES,

    /* not settable; ral_polygon_mode */
    RAL_GFX_STATE_PROPERTY_POLYGON_MODE,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_PRIMITIVE_RESTART_ENABLED,

    /* not settable; ral_primitive_type */
    RAL_GFX_STATE_PROPERTY_PRIMITIVE_TYPE,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_RASTERIZER_DISCARD_ENABLED,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_ENABLED,

    /* not settable; float */
    RAL_GFX_STATE_PROPERTY_SAMPLE_SHADING_MIN,

    /* not settable; ral_stencil_op_state */
    RAL_GFX_STATE_PROPERTY_STENCIL_TEST_BACK,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_STENCIL_TEST_ENABLED,

    /* not settable; ral_stencil_op_state */
    RAL_GFX_STATE_PROPERTY_STENCIL_TEST_FRONT,

    /* not settable; ral_gfx_state_vertex_attribute* (array of RAL_GFX_STATE_PROPERTY_N_VERTEX_ATTRIBUTES items) */
    RAL_GFX_STATE_PROPERTY_VERTEX_ATTRIBUTES
} ral_gfx_state_property;


/** TODO */
PUBLIC ral_gfx_state ral_gfx_state_create(const ral_gfx_state_create_info* create_info_ptr);

/** TODO */
PUBLIC void ral_gfx_state_get_property(ral_gfx_state          gfx_state,
                                       ral_gfx_state_property property,
                                       void*                  out_result_ptr);

/** TODO */
PUBLIC void ral_gfx_state_release(ral_gfx_state state);


#endif /* RAL_GFX_STATE_H */