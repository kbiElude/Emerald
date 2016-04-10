#ifndef RAL_GFX_STATE_H
#define RAL_GFX_STATE_H

#include "ral/ral_types.h"


typedef enum
{
    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_ALPHA_TO_COVERAGE_ENABLED,

    /* not settable; bool */
    RAL_GFX_STATE_PROPERTY_ALPHA_TO_ONE_ENABLED,

    /* not settable; ral_context */
    RAL_GFX_STATE_PROPERTY_CONTEXT,

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
PUBLIC ral_gfx_state ral_gfx_state_create(ral_context                      context,
                                          const ral_gfx_state_create_info* create_info_ptr);

/** TODO */
PUBLIC void ral_gfx_state_get_property(ral_gfx_state          gfx_state,
                                       ral_gfx_state_property property,
                                       void*                  out_result_ptr);

/** TODO */
PUBLIC void ral_gfx_state_release(ral_gfx_state state);


#endif /* RAL_GFX_STATE_H */