/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_SCROLLBAR_H
#define OGL_UI_SCROLLBAR_H


/* Call-backs */
enum
{
    OGL_UI_SCROLLBAR_CALLBACK_ID_VISIBILITY_TOGGLE, /* fired when scrollbar visibility is toggled on or off / user_arg: not used */
};

/** TODO */
PUBLIC void ogl_ui_scrollbar_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_scrollbar_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_scrollbar_get_property(__in  __notnull const void*              scrollbar,
                                          __in            _ogl_ui_control_property property,
                                          __out __notnull void*                    out_result);

/** TODO */
PUBLIC void ogl_ui_scrollbar_hover(void*        internal_instance,
                                   const float* xy_screen_norm);

/** TODO */
PUBLIC void* ogl_ui_scrollbar_init(__in           __notnull   ogl_ui                         instance,
                                   __in           __notnull   ogl_text                       text_renderer,
                                   __in                       ogl_ui_scrollbar_text_location text_location,
                                   __in           __notnull   system_hashed_ansi_string      name,
                                   __in           __notnull   system_variant                 min_value,
                                   __in           __notnull   system_variant                 max_value,
                                   __in_ecount(2) __notnull   const float*                   x1y1,
                                   __in_ecount(2) __notnull   const float*                   x2y2,
                                   __in           __notnull   PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                   __in           __maybenull void*                          get_current_value_ptr_user_arg,
                                   __in           __notnull   PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                   __in           __maybenull void*                          set_current_value_ptr_user_arg);

/** TODO */
PUBLIC bool ogl_ui_scrollbar_is_over(void*        internal_instance,
                                     const float* xy);

/** TODO */
PUBLIC void ogl_ui_scrollbar_on_lbm_down(void*        internal_instance,
                                         const float* xy);

/** TODO */
PUBLIC void ogl_ui_scrollbar_on_lbm_up(void*        internal_instance,
                                       const float* xy);

/** TODO */
PUBLIC void ogl_ui_scrollbar_on_mouse_move(void*        internal_instance,
                                           const float* xy);

/** TODO */
PUBLIC void ogl_ui_scrollbar_set_property(__in __notnull void*                    scrollbar,
                                          __in __notnull _ogl_ui_control_property property,
                                          __in __notnull const void*              data);

#endif /* OGL_UI_SCROLLBAR_H */
