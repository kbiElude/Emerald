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
    OGL_UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED, /* not settable, float. Range: <0.0, 1.0> */
    OGL_UI_CONTROL_PROPERTY_GENERAL_INDEX,             /* not settable, unsigned int. Tells the index of the control in the control stack */
    OGL_UI_CONTROL_PROPERTY_GENERAL_TYPE,
    OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE,           /* settable, bool. */
    OGL_UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED,  /* not settable, float. Range: <0.0, 1.0> */
    OGL_UI_CONTROL_PROPERTY_GENERAL_X1Y1,              /* settable, float[2]. */

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

typedef enum _ogl_ui_control_type
{
    OGL_UI_CONTROL_TYPE_BUTTON,
    OGL_UI_CONTROL_TYPE_CHECKBOX,
    OGL_UI_CONTROL_TYPE_DROPDOWN,
    OGL_UI_CONTROL_TYPE_FRAME,
    OGL_UI_CONTROL_TYPE_LABEL,
    OGL_UI_CONTROL_TYPE_SCROLLBAR,
    OGL_UI_CONTROL_TYPE_TEXTURE_PREVIEW,

    OGL_UI_CONTROL_TYPE_UNKNOWN
} _ogl_ui_control_type;


/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_button(ogl_ui                    ui_instance,
                                                    system_hashed_ansi_string name,
                                                    const float*              x1y1,
                                                    PFNOGLUIFIREPROCPTR       pfn_fire_ptr,
                                                    void*                     fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_checkbox(ogl_ui                    ui_instance,
                                                      system_hashed_ansi_string name,
                                                      const float*              x1y1,
                                                      bool                      default_status,
                                                      PFNOGLUIFIREPROCPTR       pfn_fire_ptr,
                                                      void*                     fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_dropdown(ogl_ui                     ui_instance,
                                                      uint32_t                   n_entries,
                                                      system_hashed_ansi_string* strings,
                                                      void**                     user_args,
                                                      uint32_t                   n_selected_entry,
                                                      system_hashed_ansi_string  name,
                                                      const float*               x1y1,
                                                      PFNOGLUIFIREPROCPTR        pfn_fire_ptr,
                                                      void*                      fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_frame(ogl_ui       ui_instance,
                                                   const float* x1y1x2y2);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_label(ogl_ui                    ui_instance,
                                                   system_hashed_ansi_string name,
                                                   const float*              x1y1);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_scrollbar(ogl_ui                         ui_instance,
                                                       system_hashed_ansi_string      name,
                                                       ogl_ui_scrollbar_text_location text_location,
                                                       system_variant                 min_value,
                                                       system_variant                 max_value,
                                                       const float*                   x1y1,
                                                       PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                                       void*                          get_current_value_ptr_user_arg,
                                                       PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                                       void*                          set_current_value_ptr_user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_texture_preview(ogl_ui                      ui_instance,
                                                             system_hashed_ansi_string   name,
                                                             const float*                x1y1,
                                                             const float*                max_size,
                                                             ogl_texture                 texture,
                                                             ogl_ui_texture_preview_type preview_type);
/** TODO */
PUBLIC EMERALD_API ogl_ui ogl_ui_create(ogl_text                  text_renderer,
                                        system_hashed_ansi_string name);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_ui_draw(ogl_ui);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_get_control_property(ogl_ui_control           control,
                                                    _ogl_ui_control_property property,
                                                    void*                    out_result);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC ogl_context ogl_ui_get_context(ogl_ui);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC ogl_program ogl_ui_get_registered_program(ogl_ui,
                                                 system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_lock(ogl_ui                              ui,
                                    system_read_write_mutex_access_type access_type);

/** INTERNAL USAGE ONLY.
 *
 *  Call-back entry-point to be used by UI control implementations. Will dispatch call-backs
 *  to subscribers by using engine thread pool.
 **/
PUBLIC void ogl_ui_receive_control_callback(ogl_ui         ui,
                                            ogl_ui_control control,
                                            int            callback_id,
                                            void*          callback_user_arg);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_register_control_callback(ogl_ui                       ui,
                                                         ogl_ui_control               control,
                                                         int                          callback_id,
                                                         PFNOGLUIEVENTCALLBACKPROCPTR callback_proc_ptr,
                                                         void*                        callback_proc_user_arg);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC bool ogl_ui_register_program(ogl_ui,
                                    system_hashed_ansi_string,
                                    ogl_program);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_reposition_control(ogl_ui_control control,
                                                  unsigned int   new_control_index);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_set_control_property(ogl_ui_control           control,
                                                    _ogl_ui_control_property property,
                                                    const void*              data);

/** TODO */
PUBLIC EMERALD_API void ogl_ui_unlock(ogl_ui                              ui,
                                      system_read_write_mutex_access_type access_type);

#endif /* OGL_UI_H */
