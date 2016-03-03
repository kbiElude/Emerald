/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef UI_TYPES_H
#define UI_TYPES_H

#include "system/system_types.h"

DECLARE_HANDLE(ui);

typedef enum
{
    UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
    UI_SCROLLBAR_TEXT_LOCATION_LEFT_TO_SLIDER
} ui_scrollbar_text_location;

typedef enum
{
    UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_ALPHA,
    UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_DEPTH,
    UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RED,
    UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGB,
    UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGBA,

    UI_TEXTURE_PREVIEW_TYPE_SAMPLER2DARRAY_DEPTH,

} ui_texture_preview_type;

/* UI bag handle */
DECLARE_HANDLE(ui_bag);

/* UI control handle */
DECLARE_HANDLE(ui_control);


/** Type definitions */
typedef void (*PFNUIEVENTCALLBACKPROCPTR)  (ui_control     control,
                                            int            callback_id,
                                            void*          callback_subscriber_data,
                                            void*          callback_data);
typedef void (*PFNUIFIREPROCPTR)           (void*          fire_proc_user_arg,
                                            void*          event_user_arg);
typedef void (*PFNUIGETCURRENTVALUEPROCPTR)(void*          user_arg,
                                            system_variant result);
typedef void (*PFNUISETCURRENTVALUEPROCPTR)(void*          user_arg,
                                            system_variant new_value);

typedef enum
{
                        /* general */

    /* not settable, float. Range: <0.0, 1.0> */
    UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED,

    /* not settable, unsigned int. Tells the index of the control in the control stack */
    UI_CONTROL_PROPERTY_GENERAL_INDEX,
    UI_CONTROL_PROPERTY_GENERAL_TYPE,

    /* settable, bool. */
    UI_CONTROL_PROPERTY_GENERAL_VISIBLE,

    /* not settable, float. Range: <0.0, 1.0> */
    UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED,

    /* settable, float[2]. */
    UI_CONTROL_PROPERTY_GENERAL_X1Y1,


                        /* button-specific */

    /* not settable, float */
    UI_CONTROL_PROPERTY_BUTTON_HEIGHT_SS,

    /* not settable, float */
    UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS,

    /* settable, float[2] */
    UI_CONTROL_PROPERTY_BUTTON_X1Y1,

    /* settable, float[4] */
    UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2,


                        /* checkbox-specific */

    /* not settable, bool */
    UI_CONTROL_PROPERTY_CHECKBOX_CHECK_STATUS,

    /* settable, float[2] */
    UI_CONTROL_PROPERTY_CHECKBOX_X1Y1,

                        /* dropdown-specific */

    /* not settable, bool */
    UI_CONTROL_PROPERTY_DROPDOWN_IS_DROPAREA_VISIBLE,

    /* not settable, float[4] */
    UI_CONTROL_PROPERTY_DROPDOWN_LABEL_BG_X1Y1X2Y2,

    /* not settable, float[2] */
    UI_CONTROL_PROPERTY_DROPDOWN_LABEL_X1Y1,

    /* settable, bool */
    UI_CONTROL_PROPERTY_DROPDOWN_VISIBLE,

    /* not settable, float[4] */
    UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,

    /* settable, float[2] */
    UI_CONTROL_PROPERTY_DROPDOWN_X1Y1,

                        /* frame-specific */

    /* settable, float[4] */
    UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2,

                        /* label-specific */

    /* settable, system_hashed_ansi_string */
    UI_CONTROL_PROPERTY_LABEL_TEXT,

    /* not settable, float */
    UI_CONTROL_PROPERTY_LABEL_TEXT_HEIGHT_SS,

    /* settable, float[2] */
    UI_CONTROL_PROPERTY_LABEL_X1Y1,

                        /* scrollbar-specific */

    /* settable, bool */
    UI_CONTROL_PROPERTY_SCROLLBAR_VISIBLE,

                        /* texture_preview-specific */

    /* settable, GLfloat[4] */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_COLOR,

    /* settable, GLenum */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_ALPHA,

    /* settable, GLenum */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_RGB,

    /* settable, GLenum */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_ALPHA,

    /* settable, GLenum */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_RGB,

    /* settable, GLenum */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_ALPHA,

    /* settable, GLenum */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_RGB,

    /* settable, bool */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_IS_BLENDING_ENABLED,

    /* settable, GLuint. Only useful for 2d array texture previews */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_LAYER_SHOWN,

    /* settable, bool */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_SHOW_TEXTURE_NAME,

    /* settable, ral_texture */
    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE_RAL,
} ui_control_property;

typedef enum _ui_control_type
{
    UI_CONTROL_TYPE_BUTTON,
    UI_CONTROL_TYPE_CHECKBOX,
    UI_CONTROL_TYPE_DROPDOWN,
    UI_CONTROL_TYPE_FRAME,
    UI_CONTROL_TYPE_LABEL,
    UI_CONTROL_TYPE_SCROLLBAR,
    UI_CONTROL_TYPE_TEXTURE_PREVIEW,

    UI_CONTROL_TYPE_UNKNOWN
} ui_control_type;

#endif /* UI_TYPES_H */