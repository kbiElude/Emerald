/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_UI_H
#define OGL_UI_H

#include "ogl/ogl_types.h"
#include "system/system_variant.h"


REFCOUNT_INSERT_DECLARATIONS(ogl_ui,
                             ogl_ui);

/** Type definitions */
typedef void (*PFNOGLUIEVENTCALLBACKPROCPTR)  (ogl_ui_control control,
                                               int            callback_id,
                                               void*          callback_subscriber_data,
                                               void*          callback_data);
typedef void (*PFNOGLUIFIREPROCPTR)           (void*          fire_proc_user_arg,
                                               void*          event_user_arg);
typedef void (*PFNOGLUIGETCURRENTVALUEPROCPTR)(void*          user_arg,
                                               system_variant result);
typedef void (*PFNOGLUISETCURRENTVALUEPROCPTR)(void*          user_arg,
                                               system_variant new_value);

typedef enum
{
    /* general */

    /* ogl_ui_button-specific */
    OGL_UI_CONTROL_PROPERTY_BUTTON_HEIGHT_SS, /* not settable, float */
    OGL_UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS,  /* not settable, float */
    OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1,      /* settable, float[2] */
    OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2,  /* settable, float[4] */

    /* ogl_ui_checkbox-specific */
    OGL_UI_CONTROL_PROPERTY_CHECKBOX_CHECK_STATUS, /* not settable, bool */
    OGL_UI_CONTROL_PROPERTY_CHECKBOX_X1Y1,         /* settable, float[2] */

    /* ogl_ui_dropdown-specific */
    OGL_UI_CONTROL_PROPERTY_DROPDOWN_IS_DROPAREA_VISIBLE, /* not settable, bool */
    OGL_UI_CONTROL_PROPERTY_DROPDOWN_LABEL_BG_X1Y1X2Y2,   /* not settable, float[4] */
    OGL_UI_CONTROL_PROPERTY_DROPDOWN_LABEL_X1Y1,          /* not settable, float[2] */
    OGL_UI_CONTROL_PROPERTY_DROPDOWN_VISIBLE,             /*     settable, bool */
    OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,            /* not settable, float[4] */
    OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1,                /*     settable, float[2] */

    /* ogl_ui_frame-specific */
    OGL_UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2, /* settable, float[4] */

    /* ogl_ui_label-specific */
    OGL_UI_CONTROL_PROPERTY_LABEL_TEXT,           /* settable,     system_hashed_ansi_string */
    OGL_UI_CONTROL_PROPERTY_LABEL_TEXT_HEIGHT_SS, /* not settable, float */
    OGL_UI_CONTROL_PROPERTY_LABEL_X1Y1,           /* settable,     float[2] */

    /* ogl_ui_scrollbar-specific */
    OGL_UI_CONTROL_PROPERTY_SCROLLBAR_VISIBLE, /* settable, bool */

    /* ogl_ui_texture_preview-specific */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_COLOR,              /* settable, GLfloat[4]                                        */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_ALPHA,     /* settable, GLenum                                            */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_RGB,       /* settable, GLenum                                            */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_ALPHA, /* settable, GLenum                                            */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_RGB,   /* settable, GLenum                                            */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_ALPHA, /* settable, GLenum                                            */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_RGB,   /* settable, GLenum                                            */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_IS_BLENDING_ENABLED,      /* settable, bool                                              */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_LAYER_SHOWN,              /* settable, GLuint. Only useful for 2d array texture previews */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_SHOW_TEXTURE_NAME,        /* settable, bool                                              */
    OGL_UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE,                  /* settable, ogl_texture                                       */
} _ogl_ui_control_property;

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_button(__in           __notnull   ogl_ui                    ui_instance,
                                                    __in           __notnull   system_hashed_ansi_string name,
                                                    __in_ecount(2) __notnull   const float*              x1y1,
                                                    __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_ptr,
                                                    __in           __maybenull void*                     fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_checkbox(__in           __notnull   ogl_ui                    ui_instance,
                                                      __in           __notnull   system_hashed_ansi_string name,
                                                      __in_ecount(2) __notnull   const float*              x1y1,
                                                      __in                       bool                      default_status,
                                                      __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_ptr,
                                                      __in           __maybenull void*                     fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_dropdown(__in                   __notnull   ogl_ui                     ui_instance,
                                                      __in                               uint32_t                   n_entries,
                                                      __in_ecount(n_entries) __notnull   system_hashed_ansi_string* strings,
                                                      __in_ecount(n_entries) __notnull   void**                     user_args,
                                                      __in                               uint32_t                   n_selected_entry,
                                                      __in                   __notnull   system_hashed_ansi_string  name,
                                                      __in_ecount(2)         __notnull   const float*               x1y1,
                                                      __in                   __notnull   PFNOGLUIFIREPROCPTR        pfn_fire_ptr,
                                                      __in                   __maybenull void*                      fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_frame(__in           __notnull ogl_ui       ui_instance,
                                                   __in_ecount(4) __notnull const float* x1y1x2y2);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_label(__in           __notnull ogl_ui                    ui_instance,
                                                   __in           __notnull system_hashed_ansi_string name,
                                                   __in_ecount(2) __notnull const float*              x1y1);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_scrollbar(__in           __notnull   ogl_ui                         ui_instance,
                                                       __in           __notnull   system_hashed_ansi_string      name,
                                                       __in           __notnull   system_variant                 min_value,
                                                       __in           __notnull   system_variant                 max_value,
                                                       __in_ecount(2) __notnull   const float*                   x1y1,
                                                       __in           __notnull   PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                                       __in           __maybenull void*                          get_current_value_ptr_user_arg,
                                                       __in           __notnull   PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                                       __in           __maybenull void*                          set_current_value_ptr_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_texture_preview(__in __notnull           ogl_ui                      ui_instance,
                                                             __in __notnull           system_hashed_ansi_string   name,
                                                             __in_ecount(2) __notnull const float*                x1y1,
                                                             __in_ecount(2) __notnull const float*                max_size,
                                                             __in_opt                 ogl_texture                 texture,
                                                             __in                     ogl_ui_texture_preview_type preview_type);
/** TODO */
PUBLIC EMERALD_API ogl_ui ogl_ui_create(__in __notnull ogl_text                  text_renderer,
                                        __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_ui_draw(__in __notnull ogl_ui);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_get_property(__in  __notnull ogl_ui_control           control,
                                            __in            _ogl_ui_control_property property,
                                            __out __notnull void*                    out_result);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC ogl_context ogl_ui_get_context(__in __notnull ogl_ui);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC ogl_program ogl_ui_get_registered_program(__in __notnull ogl_ui,
                                                 __in __notnull system_hashed_ansi_string);

/** INTERNAL USAGE ONLY.
 *
 *  Call-back entry-point to be used by UI control implementations. Will dispatch call-backs
 *  to subscribers by using engine thread pool.
 **/
PUBLIC void ogl_ui_receive_control_callback(__in     __notnull ogl_ui         ui,
                                            __in     __notnull ogl_ui_control control,
                                            __in     __notnull int            callback_id,
                                            __in_opt           void*          callback_user_arg);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_register_control_callback(__in __notnull ogl_ui                       ui,
                                                         __in __notnull ogl_ui_control               control,
                                                         __in __notnull int                          callback_id,
                                                         __in __notnull PFNOGLUIEVENTCALLBACKPROCPTR callback_proc_ptr,
                                                         __in __notnull void*                        callback_proc_user_arg);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC bool ogl_ui_register_program(__in __notnull ogl_ui,
                                    __in __notnull system_hashed_ansi_string,
                                    __in __notnull ogl_program);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_set_property(__in __notnull ogl_ui_control           control,
                                            __in           _ogl_ui_control_property property,
                                            __in __notnull const void*              data);

#endif /* OGL_UI_H */
