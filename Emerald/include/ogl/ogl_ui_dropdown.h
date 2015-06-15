/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_DROPDOWN_H
#define OGL_UI_DROPDOWN_H

/* Call-backs */
enum
{
    OGL_UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,   /* fired when droparea is toggled on or off            / user_arg: not used */
    OGL_UI_DROPDOWN_CALLBACK_ID_VISIBILITY_TOGGLE, /* fired when droparea visibility is toggled on or off / user_arg: not used */
};


/** TODO */
PUBLIC void ogl_ui_dropdown_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_dropdown_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_dropdown_get_property(__in  __notnull const void*              dropdown,
                                         __in            _ogl_ui_control_property property,
                                         __out __notnull void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_dropdown_init(__in                   __notnull   ogl_ui                     instance,
                                  __in                   __notnull   ogl_text                   text_renderer,
                                  __in                   __notnull   system_hashed_ansi_string  label_text,
                                  __in                               uint32_t                   n_entries,
                                  __in_ecount(n_entries) __notnull   system_hashed_ansi_string* entries,
                                  __in_ecount(n_entries) __notnull   void**                     user_args,
                                  __in                               uint32_t                   n_selected_entry,
                                  __in                   __notnull   system_hashed_ansi_string  name,
                                  __in_ecount(2)         __notnull   const float*               x1y1,
                                  __in                   __notnull   PFNOGLUIFIREPROCPTR        pfn_fire_proc_ptr,
                                  __in                   __maybenull void*                      fire_proc_user_arg,
                                  __in                   __notnull   ogl_ui_control             owner_control);

/** TODO */
PUBLIC bool ogl_ui_dropdown_is_over(      void*  internal_instance,
                                    const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_lbm_down(      void*  internal_instance,
                                        const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_lbm_up(      void*  internal_instance,
                                      const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_mouse_move(      void*  internal_instance,
                                          const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_mouse_wheel(void* internal_instance,
                                           float wheel_delta);

/** TODO */
PUBLIC void ogl_ui_dropdown_set_property(__in __notnull void*                    dropdown,
                                         __in __notnull _ogl_ui_control_property property,
                                         __in __notnull const void*              data);

#endif /* OGL_UI_DROPDOWN_H */
